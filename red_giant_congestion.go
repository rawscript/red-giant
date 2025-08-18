// Red Giant Protocol - Intelligent Congestion Control
// Maintains exposure-based approach with adaptive flow control
package main

/*
#cgo CFLAGS: -std=gnu99 -O3 -march=native
#include "red_giant.h"
*/
import "C"
import (
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

type CongestionController struct {
	currentWindowSize int64
	maxWindowSize     int64
	minWindowSize     int64
	rttEstimate       int64 // nanoseconds
	packetLossRate    float64
	// exposureRate is calculated on the fly, not stored
	exposureRate      int64 // chunks per second
	adaptiveThreshold int64
	mu                sync.RWMutex
}

func NewCongestionController(maxWindow int64) *CongestionController {
	return &CongestionController{
		currentWindowSize: maxWindow / 4, // Start conservatively
		maxWindowSize:     maxWindow,
		minWindowSize:     1,
		rttEstimate:       int64(10 * time.Millisecond), // 10ms default RTT
		adaptiveThreshold: 80,                           // 80% utilization threshold
	}
}

// Adaptive exposure rate based on network conditions
func (cc *CongestionController) GetOptimalExposureRate() int64 {
	cc.mu.RLock()
	defer cc.mu.RUnlock()

	rtt := cc.rttEstimate
	if rtt == 0 {
		rtt = int64(time.Second) // Avoid division by zero, assume high latency
	}

	// Calculate optimal exposure rate based on current conditions
	baseRate := cc.currentWindowSize * 1000000000 / rtt

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

	// Smoothed RTT estimation (like TCP's Jacobson/Karels algorithm)
	sampleRTT := rtt.Nanoseconds()
	if sampleRTT > 0 {
		if cc.rttEstimate == 0 {
			cc.rttEstimate = sampleRTT
		} else {
			// alpha = 0.125
			cc.rttEstimate = (cc.rttEstimate*7 + sampleRTT) / 8
		}
	}

	if successful {
		// Additive increase - exposure-aware growth
		if cc.currentWindowSize < cc.maxWindowSize {
			increment := int64(1)
			if cc.currentWindowSize < cc.maxWindowSize/2 {
				increment = 2 // Faster growth (slow start phase)
			}
			cc.currentWindowSize += increment
		}

		// Decrease loss rate estimate
		cc.packetLossRate *= 0.95
	} else {
		// Multiplicative decrease - preserve some exposure capacity
		cc.currentWindowSize = cc.currentWindowSize * 3 / 4 // 25% reduction
		if cc.currentWindowSize < cc.minWindowSize {
			cc.currentWindowSize = cc.minWindowSize
		}

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
	failedChunks    int64
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
			optimalRate := es.controller.GetOptimalExposureRate()
			if optimalRate == 0 {
				optimalRate = 1 // Ensure we can always process at least one chunk
			}
			// Convert chunks/sec to a reasonable worker count
			maxWorkers := optimalRate / 1000
			if maxWorkers == 0 {
				maxWorkers = 1
			}

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
			// This ticker is now just for ensuring the loop doesn't block forever
			// if the channel is empty. The actual congestion control logic is
			// driven by chunk success/failure feedback.
		}
	}
}

// Expose individual chunk with performance monitoring
func (es *ExposureScheduler) exposeChunk(surface *C.rg_exposure_surface_t, task ChunkExposureTask) {
	atomic.AddInt64(&es.activeWorkers, 1)
	defer atomic.AddInt64(&es.activeWorkers, -1)

	start := time.Now()

	// Handle empty data slice to prevent panic with CGO
	var dataPtr unsafe.Pointer
	if len(task.Data) > 0 {
		dataPtr = unsafe.Pointer(&task.Data[0])
	}

	success := C.rg_expose_chunk_fast(
		surface,
		C.uint32_t(task.ChunkID),
		dataPtr,
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
			// Re-schedule with exponential backoff
			time.AfterFunc(time.Millisecond*time.Duration(1<<task.Retries), func() {
				es.ScheduleExposure(task.ChunkID, task.Data)
			})
		} else {
			atomic.AddInt64(&es.failedChunks, 1)
			es.controller.UpdateWindow(false, duration)
		}
	}
}
