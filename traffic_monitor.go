// Red Giant Protocol - Traffic-Aware Adaptive System
package main

import (
	"fmt"
	"math"
	"sync"
	"time"
)

// NetworkMetrics tracks real-time network performance
type NetworkMetrics struct {
	Bandwidth       float64 // bytes per second
	Latency         time.Duration
	PacketLoss      float64 // percentage (0.0 - 1.0)
	Congestion      float64 // congestion level (0.0 - 1.0)
	LastUpdate      time.Time
	SampleCount     int
	mu              sync.RWMutex
}

// TrafficMonitor continuously monitors network conditions
type TrafficMonitor struct {
	metrics         *NetworkMetrics
	measurementChan chan NetworkSample
	stopChan        chan bool
	adaptiveChan    chan AdaptiveParams
}

// NetworkSample represents a single network measurement
type NetworkSample struct {
	Timestamp    time.Time
	ResponseTime time.Duration
	BytesTransferred int
	Success      bool
}

// AdaptiveParams contains dynamically adjusted parameters
type AdaptiveParams struct {
	ChunkSize       int
	ExposureCadence time.Duration
	WorkerCount     int
	BufferSize      int
	Reason          string
}

func NewTrafficMonitor() *TrafficMonitor {
	return &TrafficMonitor{
		metrics: &NetworkMetrics{
			Bandwidth:   1024 * 1024, // Start with 1MB/s assumption
			Latency:     50 * time.Millisecond,
			PacketLoss:  0.0,
			Congestion:  0.0,
			LastUpdate:  time.Now(),
		},
		measurementChan: make(chan NetworkSample, 100),
		stopChan:        make(chan bool, 1),
		adaptiveChan:    make(chan AdaptiveParams, 10),
	}
}

// StartMonitoring begins continuous network monitoring
func (tm *TrafficMonitor) StartMonitoring() <-chan AdaptiveParams {
	go tm.monitoringLoop()
	return tm.adaptiveChan
}

// RecordSample adds a network measurement sample
func (tm *TrafficMonitor) RecordSample(sample NetworkSample) {
	select {
	case tm.measurementChan <- sample:
	default:
		// Channel full, drop oldest sample (non-blocking)
	}
}

// monitoringLoop processes network samples and calculates adaptive parameters
func (tm *TrafficMonitor) monitoringLoop() {
	ticker := time.NewTicker(500 * time.Millisecond) // Adapt every 500ms
	defer ticker.Stop()
	
	samples := make([]NetworkSample, 0, 20)
	
	for {
		select {
		case sample := <-tm.measurementChan:
			samples = append(samples, sample)
			if len(samples) > 20 {
				samples = samples[1:] // Keep only last 20 samples
			}
			
		case <-ticker.C:
			if len(samples) > 0 {
				tm.updateMetrics(samples)
				params := tm.calculateAdaptiveParams()
				
				select {
				case tm.adaptiveChan <- params:
				default:
				}
			}
			
		case <-tm.stopChan:
			return
		}
	}
}

// updateMetrics calculates current network metrics from samples
func (tm *TrafficMonitor) updateMetrics(samples []NetworkSample) {
	tm.metrics.mu.Lock()
	defer tm.metrics.mu.Unlock()
	
	if len(samples) == 0 {
		return
	}
	
	// Calculate bandwidth (bytes/second)
	totalBytes := 0
	totalTime := time.Duration(0)
	successCount := 0
	latencySum := time.Duration(0)
	
	for _, sample := range samples {
		if sample.Success {
			totalBytes += sample.BytesTransferred
			latencySum += sample.ResponseTime
			successCount++
		}
		totalTime += sample.ResponseTime
	}
	
	if successCount > 0 {
		// Average bandwidth over successful transfers
		avgTime := totalTime / time.Duration(len(samples))
		if avgTime > 0 {
			tm.metrics.Bandwidth = float64(totalBytes) / avgTime.Seconds()
		}
		
		// Average latency
		tm.metrics.Latency = latencySum / time.Duration(successCount)
		
		// Packet loss rate
		tm.metrics.PacketLoss = 1.0 - (float64(successCount) / float64(len(samples)))
		
		// Congestion estimation (based on latency variance)
		tm.metrics.Congestion = tm.calculateCongestion(samples)
	}
	
	tm.metrics.LastUpdate = time.Now()
	tm.metrics.SampleCount += len(samples)
}

// calculateCongestion estimates network congestion from latency patterns
func (tm *TrafficMonitor) calculateCongestion(samples []NetworkSample) float64 {
	if len(samples) < 3 {
		return 0.0
	}
	
	// Calculate latency variance as congestion indicator
	var latencies []float64
	for _, sample := range samples {
		if sample.Success {
			latencies = append(latencies, float64(sample.ResponseTime.Nanoseconds()))
		}
	}
	
	if len(latencies) < 2 {
		return 0.0
	}
	
	// Calculate variance
	mean := 0.0
	for _, lat := range latencies {
		mean += lat
	}
	mean /= float64(len(latencies))
	
	variance := 0.0
	for _, lat := range latencies {
		variance += math.Pow(lat-mean, 2)
	}
	variance /= float64(len(latencies))
	
	// Normalize congestion to 0.0-1.0 range
	// Higher variance indicates more congestion
	congestion := math.Min(variance/math.Pow(mean, 2), 1.0)
	return congestion
}

// calculateAdaptiveParams determines optimal parameters based on current network conditions
func (tm *TrafficMonitor) calculateAdaptiveParams() AdaptiveParams {
	tm.metrics.mu.RLock()
	defer tm.metrics.mu.RUnlock()
	
	bandwidth := tm.metrics.Bandwidth
	latency := tm.metrics.Latency
	packetLoss := tm.metrics.PacketLoss
	congestion := tm.metrics.Congestion
	
	// Base parameters
	baseChunkSize := 64 * 1024 // 64KB
	baseCadence := 100 * time.Millisecond
	baseWorkers := 4
	
	var reason string
	
	// Adapt chunk size based on bandwidth and packet loss
	chunkSize := baseChunkSize
	if bandwidth > 10*1024*1024 { // High bandwidth (>10MB/s)
		chunkSize = int(math.Min(float64(baseChunkSize)*4, 1024*1024)) // Up to 1MB chunks
		reason += "High-BW "
	} else if bandwidth < 100*1024 { // Low bandwidth (<100KB/s)
		chunkSize = baseChunkSize / 4 // Smaller chunks for low bandwidth
		reason += "Low-BW "
	}
	
	// Adjust for packet loss
	if packetLoss > 0.05 { // >5% packet loss
		chunkSize = int(float64(chunkSize) * (1.0 - packetLoss)) // Smaller chunks
		reason += "High-Loss "
	}
	
	// Adapt exposure cadence based on latency and congestion
	exposureCadence := baseCadence
	if latency > 200*time.Millisecond { // High latency
		exposureCadence = time.Duration(float64(baseCadence) * 2) // Slower exposure
		reason += "High-Latency "
	} else if latency < 20*time.Millisecond { // Low latency
		exposureCadence = baseCadence / 2 // Faster exposure
		reason += "Low-Latency "
	}
	
	// Adjust for congestion
	if congestion > 0.7 { // High congestion
		exposureCadence = time.Duration(float64(exposureCadence) * (1.0 + congestion))
		chunkSize = int(float64(chunkSize) * 0.7) // Smaller chunks, slower cadence
		reason += "Congested "
	}
	
	// Adapt worker count based on overall conditions
	workerCount := baseWorkers
	if bandwidth > 5*1024*1024 && packetLoss < 0.01 { // Good conditions
		workerCount = 8 // More workers
		reason += "Optimal "
	} else if packetLoss > 0.1 || congestion > 0.8 { // Poor conditions
		workerCount = 2 // Fewer workers to reduce contention
		reason += "Degraded "
	}
	
	// Buffer size scales with chunk size and worker count
	bufferSize := chunkSize * workerCount * 2
	
	if reason == "" {
		reason = "Stable"
	}
	
	return AdaptiveParams{
		ChunkSize:       chunkSize,
		ExposureCadence: exposureCadence,
		WorkerCount:     workerCount,
		BufferSize:      bufferSize,
		Reason:          reason,
	}
}

// GetCurrentMetrics returns current network metrics (thread-safe)
func (tm *TrafficMonitor) GetCurrentMetrics() NetworkMetrics {
	tm.metrics.mu.RLock()
	defer tm.metrics.mu.RUnlock()
	return *tm.metrics
}

// Stop halts the traffic monitoring
func (tm *TrafficMonitor) Stop() {
	select {
	case tm.stopChan <- true:
	default:
	}
}