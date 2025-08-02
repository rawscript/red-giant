// Red Giant Protocol - Go Orchestration Layer
package main

/*
#cgo CFLAGS: -std=c99
#include "red_giant.h"
#include <stdlib.h>
*/
import "C"
import (
	"crypto/sha256"
	"fmt"
	"log"
	"sync"
	"time"
	"unsafe"
)

// RedGiantOrchestrator - High-level Go coordination with traffic awareness
type RedGiantOrchestrator struct {
	surface       *C.rg_exposure_surface_t
	fileData      []byte
	chunkSize     int
	totalSize     int64
	manifest      *RedGiantManifest
	mu            sync.RWMutex
	subscribers   []chan ChunkNotification
	trafficMonitor *TrafficMonitor
	adaptiveMode  bool
	currentParams AdaptiveParams
}

type RedGiantManifest struct {
	FileID           string
	TotalSize        int64
	ChunkSize        int
	EncodingType     uint16
	ExposureCadence  time.Duration
	TotalChunks      int
	Hash             [32]byte
}

type ChunkNotification struct {
	ChunkID   int
	IsReady   bool
	Timestamp time.Time
}

// NewRedGiantOrchestrator creates the orchestration layer with optional traffic monitoring
func NewRedGiantOrchestrator(fileData []byte, chunkSize int, adaptiveMode bool) *RedGiantOrchestrator {
	totalSize := int64(len(fileData))
	totalChunks := int((totalSize + int64(chunkSize) - 1) / int64(chunkSize))
	
	// Create manifest
	manifest := &RedGiantManifest{
		FileID:          fmt.Sprintf("rg_%d", time.Now().Unix()),
		TotalSize:       totalSize,
		ChunkSize:       chunkSize,
		EncodingType:    0x01, // Binary
		ExposureCadence: 100 * time.Millisecond,
		TotalChunks:     totalChunks,
		Hash:            sha256.Sum256(fileData),
	}
	
	// Create C manifest
	cManifest := C.rg_manifest_t{
		total_size:         C.uint64_t(totalSize),
		chunk_size:         C.uint32_t(chunkSize),
		encoding_type:      C.uint16_t(manifest.EncodingType),
		exposure_cadence_ms: C.uint32_t(manifest.ExposureCadence.Milliseconds()),
		total_chunks:       C.uint32_t(totalChunks),
	}
	
	// Copy file ID and hash
	copy((*[64]C.char)(unsafe.Pointer(&cManifest.file_id[0]))[:], manifest.FileID)
	copy((*[32]C.uint8_t)(unsafe.Pointer(&cManifest.hash[0]))[:], manifest.Hash[:])
	
	// Create exposure surface
	surface := C.rg_create_surface(&cManifest)
	if surface == nil {
		log.Fatal("Failed to create exposure surface")
	}
	
	orchestrator := &RedGiantOrchestrator{
		surface:      surface,
		fileData:     fileData,
		chunkSize:    chunkSize,
		totalSize:    totalSize,
		manifest:     manifest,
		subscribers:  make([]chan ChunkNotification, 0),
		adaptiveMode: adaptiveMode,
		currentParams: AdaptiveParams{
			ChunkSize:       chunkSize,
			ExposureCadence: manifest.ExposureCadence,
			WorkerCount:     4,
			BufferSize:      chunkSize * 8,
			Reason:          "Initial",
		},
	}
	
	// Initialize traffic monitor if adaptive mode is enabled
	if adaptiveMode {
		orchestrator.trafficMonitor = NewTrafficMonitor()
		go orchestrator.handleAdaptiveUpdates()
	}
	
	return orchestrator
}

// Subscribe to chunk exposure notifications (pub-sub model)
func (rg *RedGiantOrchestrator) Subscribe() <-chan ChunkNotification {
	rg.mu.Lock()
	defer rg.mu.Unlock()
	
	ch := make(chan ChunkNotification, 100)
	rg.subscribers = append(rg.subscribers, ch)
	return ch
}

// BeginExposure starts the orchestrated exposure process with traffic adaptation
func (rg *RedGiantOrchestrator) BeginExposure() {
	fmt.Printf("🧭 Red Giant Protocol - Beginning Exposure\n")
	fmt.Printf("📋 Manifest: %s (%d bytes, %d chunks)\n", 
		rg.manifest.FileID, rg.manifest.TotalSize, rg.manifest.TotalChunks)
	
	if rg.adaptiveMode {
		fmt.Printf("🌐 Traffic-aware adaptive mode: ENABLED\n")
		rg.trafficMonitor.StartMonitoring()
	}
	
	go func() {
		for i := 0; i < rg.manifest.TotalChunks; i++ {
			startTime := time.Now()
			
			// Get current adaptive parameters
			rg.mu.RLock()
			currentCadence := rg.currentParams.ExposureCadence
			rg.mu.RUnlock()
			
			// Calculate chunk boundaries (may be dynamically sized in future)
			start := i * rg.chunkSize
			end := start + rg.chunkSize
			if end > len(rg.fileData) {
				end = len(rg.fileData)
			}
			
			chunkData := rg.fileData[start:end]
			
			// Expose chunk via C layer
			success := C.rg_expose_chunk(
				rg.surface,
				C.uint32_t(i),
				unsafe.Pointer(&chunkData[0]),
			)
			
			if success {
				// Record network sample for traffic monitoring
				if rg.adaptiveMode {
					sample := NetworkSample{
						Timestamp:        time.Now(),
						ResponseTime:     time.Since(startTime),
						BytesTransferred: len(chunkData),
						Success:          true,
					}
					rg.trafficMonitor.RecordSample(sample)
				}
				
				// Notify subscribers
				notification := ChunkNotification{
					ChunkID:   i,
					IsReady:   true,
					Timestamp: time.Now(),
				}
				
				rg.mu.RLock()
				for _, subscriber := range rg.subscribers {
					select {
					case subscriber <- notification:
					default: // Non-blocking
					}
				}
				rg.mu.RUnlock()
			} else if rg.adaptiveMode {
				// Record failed sample
				sample := NetworkSample{
					Timestamp:        time.Now(),
					ResponseTime:     time.Since(startTime),
					BytesTransferred: 0,
					Success:          false,
				}
				rg.trafficMonitor.RecordSample(sample)
			}
			
			// Respect adaptive exposure cadence
			time.Sleep(currentCadence)
		}
		
		// Raise the red flag
		C.rg_raise_red_flag(rg.surface)
		fmt.Printf("🚩 RED FLAG RAISED - Exposure complete!\n")
		
		if rg.adaptiveMode {
			rg.trafficMonitor.Stop()
		}
	}()
}

// handleAdaptiveUpdates processes traffic-aware parameter updates
func (rg *RedGiantOrchestrator) handleAdaptiveUpdates() {
	if !rg.adaptiveMode || rg.trafficMonitor == nil {
		return
	}
	
	adaptiveChan := rg.trafficMonitor.StartMonitoring()
	
	for params := range adaptiveChan {
		rg.mu.Lock()
		oldParams := rg.currentParams
		rg.currentParams = params
		rg.mu.Unlock()
		
		// Log significant changes
		if params.ChunkSize != oldParams.ChunkSize || 
		   params.ExposureCadence != oldParams.ExposureCadence {
			fmt.Printf("🌐 Network adaptation: %s\n", params.Reason)
			fmt.Printf("   • Chunk size: %d → %d bytes\n", oldParams.ChunkSize, params.ChunkSize)
			fmt.Printf("   • Cadence: %v → %v\n", oldParams.ExposureCadence, params.ExposureCadence)
			fmt.Printf("   • Workers: %d → %d\n", oldParams.WorkerCount, params.WorkerCount)
		}
	}
}

// GetAdaptiveParams returns current adaptive parameters (thread-safe)
func (rg *RedGiantOrchestrator) GetAdaptiveParams() AdaptiveParams {
	rg.mu.RLock()
	defer rg.mu.RUnlock()
	return rg.currentParams
}

// PullChunk allows receiver to actively pull exposed chunks
func (rg *RedGiantOrchestrator) PullChunk(chunkID int) ([]byte, bool) {
	if chunkID >= rg.manifest.TotalChunks {
		return nil, false
	}
	
	// Check if chunk is exposed
	chunk := C.rg_peek_chunk(rg.surface, C.uint32_t(chunkID))
	if chunk == nil {
		return nil, false
	}
	
	// Pull the chunk data
	chunkSize := int(chunk.data_size)
	buffer := make([]byte, chunkSize)
	
	success := C.rg_pull_chunk(
		rg.surface,
		C.uint32_t(chunkID),
		unsafe.Pointer(&buffer[0]),
	)
	
	return buffer, bool(success)
}

// IsComplete checks if transmission is complete
func (rg *RedGiantOrchestrator) IsComplete() bool {
	return bool(C.rg_is_complete(rg.surface))
}

// Cleanup destroys the exposure surface
func (rg *RedGiantOrchestrator) Cleanup() {
	if rg.surface != nil {
		C.rg_destroy_surface(rg.surface)
		rg.surface = nil
	}
}