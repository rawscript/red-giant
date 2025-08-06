
// Red Giant Protocol - Intelligent Congestion Control
// Maintains exposure-based approach with adaptive flow control
package main

import (
    "sync"
    "time"
    "sync/atomic"
)

type CongestionController struct {
    currentWindowSize    int64
    maxWindowSize       int64
    minWindowSize       int64
    rttEstimate         int64  // nanoseconds
    packetLossRate      float64
    exposureRate        int64  // chunks per second
    adaptiveThreshold   int64
    mu                  sync.RWMutex
}

func NewCongestionController(maxWindow int64) *CongestionController {
    return &CongestionController{
        currentWindowSize: maxWindow / 4, // Start conservatively
        maxWindowSize:     maxWindow,
        minWindowSize:     1,
        rttEstimate:       1000000, // 1ms default
        adaptiveThreshold: 80,      // 80% utilization threshold
    }
}

// Adaptive exposure rate based on network conditions
func (cc *CongestionController) GetOptimalExposureRate() int64 {
    cc.mu.RLock()
    defer cc.mu.RUnlock()
    
    // Calculate optimal exposure rate based on current conditions
    baseRate := atomic.LoadInt64(&cc.currentWindowSize) * 1000000000 / cc.rttEstimate
    
    // Adjust for packet loss
    if cc.packetLossRate > 0.01 { // More than 1% loss
        baseRate = int64(float64(baseRate) * (1.0 - cc.packetLossRate))
    }
    
    return baseRate
}

// Update congestion window based on feedback
func (cc *CongestionController) UpdateWindow(successful bool, rtt time.Duration) {
    cc.mu.Lock()
    defer cc.mu.Unlock()
    
    cc.rttEstimate = rtt.Nanoseconds()
    
    if successful {
        // Additive increase - exposure-aware growth
        if cc.currentWindowSize < cc.maxWindowSize {
            increment := int64(1)
            if cc.currentWindowSize < cc.maxWindowSize/2 {
                increment = 2 // Faster growth when well below limit
            }
            atomic.AddInt64(&cc.currentWindowSize, increment)
        }
        
        // Decrease loss rate estimate
        cc.packetLossRate *= 0.95
    } else {
        // Multiplicative decrease - preserve some exposure capacity
        newSize := atomic.LoadInt64(&cc.currentWindowSize) * 3 / 4 // 25% reduction
        if newSize < cc.minWindowSize {
            newSize = cc.minWindowSize
        }
        atomic.StoreInt64(&cc.currentWindowSize, newSize)
        
        // Increase loss rate estimate
        cc.packetLossRate = (cc.packetLossRate + 0.1) / 2.0
        if cc.packetLossRate > 0.5 {
            cc.packetLossRate = 0.5 // Cap at 50%
        }
    }
}

// Smart exposure scheduler that respects congestion control
type ExposureScheduler struct {
    controller      *CongestionController
    pendingChunks   chan ChunkExposureTask
    activeWorkers   int64
    completedChunks int64
    failedChunks   int64
}

type ChunkExposureTask struct {
    ChunkID   uint32
    Data      []byte
    Timestamp time.Time
    Retries   int
}

func NewExposureScheduler(congestionController *CongestionController, bufferSize int) *ExposureScheduler {
    return &ExposureScheduler{
        controller:    congestionController,
        pendingChunks: make(chan ChunkExposureTask, bufferSize),
    }
}

// Schedule chunk exposure with congestion awareness
func (es *ExposureScheduler) ScheduleExposure(chunkID uint32, data []byte) bool {
    task := ChunkExposureTask{
        ChunkID:   chunkID,
        Data:      data,
        Timestamp: time.Now(),
        Retries:   0,
    }
    
    select {
    case es.pendingChunks <- task:
        return true
    default:
        // Channel full - backpressure signal
        es.controller.UpdateWindow(false, time.Since(task.Timestamp))
        return false
    }
}

// Process exposure tasks with adaptive rate limiting
func (es *ExposureScheduler) ProcessExposures(surface *C.rg_exposure_surface_t) {
    ticker := time.NewTicker(time.Microsecond * 100) // 10kHz base rate
    defer ticker.Stop()
    
    for {
        select {
        case task := <-es.pendingChunks:
            // Check if we should throttle based on congestion control
            currentWorkers := atomic.LoadInt64(&es.activeWorkers)
            maxWorkers := es.controller.GetOptimalExposureRate() / 1000 // Convert to reasonable worker count
            
            if currentWorkers < maxWorkers {
                go es.exposeChunk(surface, task)
            } else {
                // Re-queue task for later processing
                select {
                case es.pendingChunks <- task:
                default:
                    // If we can't re-queue, it's a loss - update congestion control
                    es.controller.UpdateWindow(false, time.Since(task.Timestamp))
                    atomic.AddInt64(&es.failedChunks, 1)
                }
            }
            
        case <-ticker.C:
            // Periodic congestion control adjustment
            totalTasks := atomic.LoadInt64(&es.completedChunks) + atomic.LoadInt64(&es.failedChunks)
            if totalTasks > 0 {
                successRate := float64(atomic.LoadInt64(&es.completedChunks)) / float64(totalTasks)
                es.controller.UpdateWindow(successRate > 0.95, time.Millisecond) // Assume 1ms RTT
            }
        }
    }
}

// Expose individual chunk with performance monitoring
func (es *ExposureScheduler) exposeChunk(surface *C.rg_exposure_surface_t, task ChunkExposureTask) {
    atomic.AddInt64(&es.activeWorkers, 1)
    defer atomic.AddInt64(&es.activeWorkers, -1)
    
    start := time.Now()
    
    success := C.rg_expose_chunk_fast(
        surface,
        C.uint32_t(task.ChunkID),
        unsafe.Pointer(&task.Data[0]),
        C.uint32_t(len(task.Data)),
    )
    
    duration := time.Since(start)
    
    if bool(success) {
        atomic.AddInt64(&es.completedChunks, 1)
        es.controller.UpdateWindow(true, duration)
    } else {
        // Retry logic for failed exposures
        if task.Retries < 3 {
            task.Retries++
            time.AfterFunc(time.Millisecond*time.Duration(1<<task.Retries), func() {
                es.ScheduleExposure(task.ChunkID, task.Data)
            })
        } else {
            atomic.AddInt64(&es.failedChunks, 1)
        }
        es.controller.UpdateWindow(false, duration)
    }
}
