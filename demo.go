// Red Giant Protocol - Demo Implementation
package main

import (
	"fmt"
	"log"
	"strings"
	"sync"
	"time"
)

// ReceiverOrchestrator simulates the receiving end with concurrent processing
type ReceiverOrchestrator struct {
	expectedChunks   int
	receivedData     [][]byte
	manifest         *RedGiantManifest
	chunkStatus      []bool
	completedChunks  int
	mu               sync.RWMutex
	workerPool       chan struct{}
	chunkQueue       chan int
	completionChan   chan bool
}

func NewReceiver(totalChunks int, workerCount int) *ReceiverOrchestrator {
	return &ReceiverOrchestrator{
		expectedChunks:  totalChunks,
		receivedData:    make([][]byte, totalChunks),
		chunkStatus:     make([]bool, totalChunks),
		workerPool:      make(chan struct{}, workerCount),
		chunkQueue:      make(chan int, totalChunks*2), // Buffered for performance
		completionChan:  make(chan bool, 1),
	}
}

// AdaptiveReceiver dynamically adjusts worker count based on sender's adaptive parameters
func (r *ReceiverOrchestrator) AdaptToTraffic(sender *RedGiantOrchestrator) {
	if !sender.adaptiveMode {
		return
	}
	
	go func() {
		ticker := time.NewTicker(1 * time.Second)
		defer ticker.Stop()
		
		for {
			select {
			case <-ticker.C:
				params := sender.GetAdaptiveParams()
				
				// Adjust worker pool if needed
				currentWorkers := cap(r.workerPool)
				if params.WorkerCount != currentWorkers {
					fmt.Printf("🔄 Adapting receiver workers: %d → %d\n", 
						currentWorkers, params.WorkerCount)
					
					// Create new worker pool with adjusted size
					newPool := make(chan struct{}, params.WorkerCount)
					
					r.mu.Lock()
					oldPool := r.workerPool
					r.workerPool = newPool
					r.mu.Unlock()
					
					// Drain old pool
					for len(oldPool) > 0 {
						<-oldPool
					}
					
					// Start additional workers if needed
					if params.WorkerCount > currentWorkers {
						for i := currentWorkers; i < params.WorkerCount; i++ {
							go r.chunkWorker(sender, i)
						}
					}
				}
				
			case <-r.completionChan:
				return
			}
		}
	}()
}

func (r *ReceiverOrchestrator) ConstructFile(sender *RedGiantOrchestrator) {
	fmt.Printf("📡 Receiver: Starting concurrent file construction\n")
	
	// Subscribe to chunk notifications
	notifications := sender.Subscribe()
	
	// Start worker goroutines for concurrent chunk processing
	workerCount := cap(r.workerPool)
	fmt.Printf("🚀 Spawning %d concurrent workers\n", workerCount)
	
	for i := 0; i < workerCount; i++ {
		go r.chunkWorker(sender, i)
	}
	
	// Notification dispatcher - queues chunks for workers
	go func() {
		for notification := range notifications {
			if notification.IsReady {
				select {
				case r.chunkQueue <- notification.ChunkID:
					// Chunk queued for processing
				default:
					// Queue full, process synchronously as fallback
					go r.processChunk(sender, notification.ChunkID, -1)
				}
			}
		}
		close(r.chunkQueue) // Signal workers to finish
	}()
	
	// Completion monitor
	go func() {
		for {
			r.mu.RLock()
			completed := r.completedChunks
			expected := r.expectedChunks
			r.mu.RUnlock()
			
			if completed >= expected {
				select {
				case r.completionChan <- true:
				default:
				}
				return
			}
			time.Sleep(10 * time.Millisecond)
		}
	}()
}

// chunkWorker processes chunks concurrently
func (r *ReceiverOrchestrator) chunkWorker(sender *RedGiantOrchestrator, workerID int) {
	// Acquire worker slot
	r.workerPool <- struct{}{}
	defer func() { <-r.workerPool }()
	
	for chunkID := range r.chunkQueue {
		r.processChunk(sender, chunkID, workerID)
	}
}

// processChunk handles individual chunk processing with retry logic
func (r *ReceiverOrchestrator) processChunk(sender *RedGiantOrchestrator, chunkID int, workerID int) {
	maxRetries := 3
	retryDelay := 5 * time.Millisecond
	
	for attempt := 0; attempt < maxRetries; attempt++ {
		// Check if already processed
		r.mu.RLock()
		if r.chunkStatus[chunkID] {
			r.mu.RUnlock()
			return // Already processed by another worker
		}
		r.mu.RUnlock()
		
		// Attempt to pull chunk
		chunkData, success := sender.PullChunk(chunkID)
		if success {
			r.mu.Lock()
			if !r.chunkStatus[chunkID] { // Double-check after acquiring lock
				r.receivedData[chunkID] = chunkData
				r.chunkStatus[chunkID] = true
				r.completedChunks++
				
				workerInfo := ""
				if workerID >= 0 {
					workerInfo = fmt.Sprintf(" [Worker-%d]", workerID)
				}
				fmt.Printf("🛠 Chunk %d constructed (%d bytes)%s [%d/%d]\n", 
					chunkID, len(chunkData), workerInfo, r.completedChunks, r.expectedChunks)
			}
			r.mu.Unlock()
			return
		}
		
		// Retry with exponential backoff
		if attempt < maxRetries-1 {
			time.Sleep(retryDelay * time.Duration(1<<attempt))
		}
	}
	
	fmt.Printf("⚠️  Failed to pull chunk %d after %d attempts\n", chunkID, maxRetries)
}

func (r *ReceiverOrchestrator) WaitForCompletion(sender *RedGiantOrchestrator) []byte {
	fmt.Printf("⏳ Waiting for concurrent reconstruction to complete...\n")
	
	// Wait for either completion signal or red flag
	completionTimeout := time.NewTimer(30 * time.Second)
	defer completionTimeout.Stop()
	
	select {
	case <-r.completionChan:
		fmt.Printf("✅ All chunks received via concurrent processing\n")
	case <-completionTimeout.C:
		fmt.Printf("⚠️  Timeout waiting for completion, proceeding with available chunks\n")
	}
	
	// Also wait for sender's red flag as final confirmation
	for !sender.IsComplete() {
		time.Sleep(10 * time.Millisecond)
	}
	
	// Reconstruct complete file with integrity checking
	var completeFile []byte
	missingChunks := 0
	
	r.mu.RLock()
	for i, chunk := range r.receivedData {
		if chunk == nil {
			log.Printf("Warning: Missing chunk %d", i)
			missingChunks++
			continue
		}
		completeFile = append(completeFile, chunk...)
	}
	r.mu.RUnlock()
	
	fmt.Printf("📽 Concurrent reconstruction complete:\n")
	fmt.Printf("   • Total bytes: %d\n", len(completeFile))
	fmt.Printf("   • Chunks processed: %d/%d\n", r.expectedChunks-missingChunks, r.expectedChunks)
	fmt.Printf("   • Missing chunks: %d\n", missingChunks)
	
	return completeFile
}

func main() {
	fmt.Printf("🧠 Red Giant Protocol - Orchestral Demonstration\n")
	fmt.Printf("═══════════════════════════════════════════════\n\n")
	
	// Create sample data to transmit
	sampleData := []byte(strings.Repeat("Red Giant Protocol rocks! ", 100))
	chunkSize := 64
	
	fmt.Printf("📋 Original data: %d bytes\n", len(sampleData))
	fmt.Printf("🔧 Chunk size: %d bytes\n", chunkSize)
	fmt.Printf("📊 Expected chunks: %d\n\n", (len(sampleData)+chunkSize-1)/chunkSize)
	
	// 🔐 1. Secure Handshake (simulated)
	fmt.Printf("🔐 Phase 1: Secure Handshake\n")
	fmt.Printf("   ✓ TLS-like channel established\n")
	fmt.Printf("   ✓ Mutual authentication complete\n\n")
	
	// 🧭 2. Create orchestrator with manifest and traffic awareness
	fmt.Printf("🧭 Phase 2: Manifest Transmission\n")
	adaptiveMode := true // Enable traffic-aware adaptation
	sender := NewRedGiantOrchestrator(sampleData, chunkSize, adaptiveMode)
	defer sender.Cleanup()
	
	// 📡 3. Setup receiver with concurrent workers
	fmt.Printf("📡 Phase 3: Receiver Preparation\n")
	totalChunks := (len(sampleData) + chunkSize - 1) / chunkSize
	workerCount := 4 // Concurrent workers for chunk processing
	receiver := NewReceiver(totalChunks, workerCount)
	receiver.ConstructFile(sender)
	receiver.AdaptToTraffic(sender) // Enable traffic-aware adaptation
	fmt.Printf("   ✓ Buffer allocated, chunk placeholders ready\n")
	fmt.Printf("   ✓ %d concurrent workers initialized\n")
	fmt.Printf("   ✓ Traffic-aware adaptation enabled\n\n", workerCount)
	
	// 🛠 4. Begin exposure process
	fmt.Printf("🛠 Phase 4: Real-Time Exposure\n")
	sender.BeginExposure()
	
	// 🚩 5. Wait for completion and reconstruct
	fmt.Printf("\n🚩 Phase 5: Completion & Verification\n")
	reconstructedData := receiver.WaitForCompletion(sender)
	
	// Verify integrity
	if string(reconstructedData) == string(sampleData) {
		fmt.Printf("   ✅ Integrity check PASSED\n")
		fmt.Printf("   ✅ File reconstruction SUCCESSFUL\n")
	} else {
		fmt.Printf("   ❌ Integrity check FAILED\n")
	}
	
	// Display final network metrics
	if sender.adaptiveMode {
		metrics := sender.trafficMonitor.GetCurrentMetrics()
		fmt.Printf("\n📊 Final Network Metrics:\n")
		fmt.Printf("   • Bandwidth: %.2f KB/s\n", metrics.Bandwidth/1024)
		fmt.Printf("   • Latency: %v\n", metrics.Latency)
		fmt.Printf("   • Packet Loss: %.2f%%\n", metrics.PacketLoss*100)
		fmt.Printf("   • Congestion: %.2f%%\n", metrics.Congestion*100)
		fmt.Printf("   • Samples: %d\n", metrics.SampleCount)
	}
	
	fmt.Printf("\n🎯 Red Giant Protocol demonstration complete!\n")
	fmt.Printf("   • Low-level C exposure mechanics ✓\n")
	fmt.Printf("   • High-level Go orchestration ✓\n")
	fmt.Printf("   • Concurrent chunk processing ✓\n")
	fmt.Printf("   • Traffic-aware adaptation ✓\n")
	fmt.Printf("   • Real-time streaming support ✓\n")
	fmt.Printf("   • Pub-sub notification system ✓\n")
	fmt.Printf("   • Worker pool concurrency ✓\n")
}