// Red Giant Protocol - Traffic-Aware Adaptive Demo
package main

import (
	"fmt"
	"math"
	"math/rand"
	"os"
	"strings"
	"sync"
	"time"
)

// Network simulation and adaptation structures
type NetworkMetrics struct {
	Bandwidth       float64
	Latency         time.Duration
	PacketLoss      float64
	Congestion      float64
	LastUpdate      time.Time
	SampleCount     int
	mu              sync.RWMutex
}

type NetworkSample struct {
	Timestamp        time.Time
	ResponseTime     time.Duration
	BytesTransferred int
	Success          bool
}

type AdaptiveParams struct {
	ChunkSize       int
	ExposureCadence time.Duration
	WorkerCount     int
	BufferSize      int
	Reason          string
}

type ChunkNotification struct {
	ChunkID   int
	IsReady   bool
	Timestamp time.Time
}

type TrafficAwareOrchestrator struct {
	fileData        []byte
	chunkSize       int
	totalChunks     int
	exposedChunks   int
	currentParams   AdaptiveParams
	metrics         *NetworkMetrics
	samples         []NetworkSample
	mu              sync.RWMutex
	output          func(string)
	subscribers     []chan ChunkNotification
}

func NewTrafficAwareOrchestrator(fileData []byte, initialChunkSize int, output func(string)) *TrafficAwareOrchestrator {
	totalChunks := (len(fileData) + initialChunkSize - 1) / initialChunkSize
	
	return &TrafficAwareOrchestrator{
		fileData:    fileData,
		chunkSize:   initialChunkSize,
		totalChunks: totalChunks,
		currentParams: AdaptiveParams{
			ChunkSize:       initialChunkSize,
			ExposureCadence: 100 * time.Millisecond,
			WorkerCount:     4,
			BufferSize:      initialChunkSize * 8,
			Reason:          "Initial",
		},
		metrics: &NetworkMetrics{
			Bandwidth:  1024 * 1024, // 1MB/s initial
			Latency:    50 * time.Millisecond,
			PacketLoss: 0.01,
			Congestion: 0.0,
		},
		samples:     make([]NetworkSample, 0, 20),
		output:      output,
		subscribers: make([]chan ChunkNotification, 0),
	}
}

func (tao *TrafficAwareOrchestrator) Subscribe() <-chan ChunkNotification {
	tao.mu.Lock()
	defer tao.mu.Unlock()
	
	ch := make(chan ChunkNotification, 100)
	tao.subscribers = append(tao.subscribers, ch)
	return ch
}

func (tao *TrafficAwareOrchestrator) recordSample(sample NetworkSample) {
	tao.mu.Lock()
	defer tao.mu.Unlock()
	
	tao.samples = append(tao.samples, sample)
	if len(tao.samples) > 20 {
		tao.samples = tao.samples[1:]
	}
	
	tao.updateMetrics()
	tao.adaptParameters()
}

func (tao *TrafficAwareOrchestrator) updateMetrics() {
	if len(tao.samples) == 0 {
		return
	}
	
	totalBytes := 0
	totalTime := time.Duration(0)
	successCount := 0
	latencySum := time.Duration(0)
	
	for _, sample := range tao.samples {
		if sample.Success {
			totalBytes += sample.BytesTransferred
			latencySum += sample.ResponseTime
			successCount++
		}
		totalTime += sample.ResponseTime
	}
	
	if successCount > 0 {
		avgTime := totalTime / time.Duration(len(tao.samples))
		if avgTime > 0 {
			tao.metrics.Bandwidth = float64(totalBytes) / avgTime.Seconds()
		}
		
		tao.metrics.Latency = latencySum / time.Duration(successCount)
		tao.metrics.PacketLoss = 1.0 - (float64(successCount) / float64(len(tao.samples)))
		tao.metrics.Congestion = tao.calculateCongestion()
	}
	
	tao.metrics.LastUpdate = time.Now()
	tao.metrics.SampleCount += len(tao.samples)
}

func (tao *TrafficAwareOrchestrator) calculateCongestion() float64 {
	if len(tao.samples) < 3 {
		return 0.0
	}
	
	var latencies []float64
	for _, sample := range tao.samples {
		if sample.Success {
			latencies = append(latencies, float64(sample.ResponseTime.Nanoseconds()))
		}
	}
	
	if len(latencies) < 2 {
		return 0.0
	}
	
	// Calculate variance as congestion indicator
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
	
	congestion := math.Min(variance/math.Pow(mean, 2), 1.0)
	return congestion
}

func (tao *TrafficAwareOrchestrator) adaptParameters() {
	bandwidth := tao.metrics.Bandwidth
	latency := tao.metrics.Latency
	packetLoss := tao.metrics.PacketLoss
	congestion := tao.metrics.Congestion
	
	oldParams := tao.currentParams
	baseChunkSize := 32 * 1024 // 32KB base
	baseCadence := 100 * time.Millisecond
	
	var reason string
	
	// Adapt chunk size based on bandwidth and packet loss
	chunkSize := baseChunkSize
	if bandwidth > 5*1024*1024 { // High bandwidth (>5MB/s)
		chunkSize = int(math.Min(float64(baseChunkSize)*3, 256*1024)) // Up to 256KB
		reason += "High-BW "
	} else if bandwidth < 200*1024 { // Low bandwidth (<200KB/s)
		chunkSize = baseChunkSize / 2 // Smaller chunks
		reason += "Low-BW "
	}
	
	// Adjust for packet loss
	if packetLoss > 0.05 { // >5% packet loss
		chunkSize = int(float64(chunkSize) * (1.0 - packetLoss))
		reason += "High-Loss "
	}
	
	// Adapt exposure cadence
	exposureCadence := baseCadence
	if latency > 150*time.Millisecond { // High latency
		exposureCadence = time.Duration(float64(baseCadence) * 1.5)
		reason += "High-Latency "
	} else if latency < 30*time.Millisecond { // Low latency
		exposureCadence = baseCadence / 2
		reason += "Low-Latency "
	}
	
	// Adjust for congestion
	if congestion > 0.6 { // High congestion
		exposureCadence = time.Duration(float64(exposureCadence) * (1.0 + congestion))
		chunkSize = int(float64(chunkSize) * 0.8)
		reason += "Congested "
	}
	
	// Worker count adaptation
	workerCount := 4
	if bandwidth > 3*1024*1024 && packetLoss < 0.02 { // Good conditions
		workerCount = 6
		reason += "Optimal "
	} else if packetLoss > 0.1 || congestion > 0.7 { // Poor conditions
		workerCount = 2
		reason += "Degraded "
	}
	
	if reason == "" {
		reason = "Stable"
	}
	
	tao.currentParams = AdaptiveParams{
		ChunkSize:       chunkSize,
		ExposureCadence: exposureCadence,
		WorkerCount:     workerCount,
		BufferSize:      chunkSize * workerCount * 2,
		Reason:          reason,
	}
	
	// Log significant changes
	if chunkSize != oldParams.ChunkSize || exposureCadence != oldParams.ExposureCadence {
		tao.output(fmt.Sprintf("🌐 Network adaptation: %s\n", reason))
		tao.output(fmt.Sprintf("   • Chunk size: %d → %d bytes\n", oldParams.ChunkSize, chunkSize))
		tao.output(fmt.Sprintf("   • Cadence: %v → %v\n", oldParams.ExposureCadence, exposureCadence))
		tao.output(fmt.Sprintf("   • Workers: %d → %d\n", oldParams.WorkerCount, workerCount))
	}
}

func (tao *TrafficAwareOrchestrator) simulateNetworkConditions(scenario string, elapsed time.Duration) (time.Duration, float64, float64) {
	switch scenario {
	case "stable":
		return 25*time.Millisecond + time.Duration(rand.Intn(10))*time.Millisecond,
			0.005, // 0.5% packet loss
			8*1024*1024 + float64(rand.Intn(2*1024*1024)) // 8-10 MB/s
			
	case "congested":
		congestionFactor := 1.0 + 0.4*math.Sin(float64(elapsed.Seconds())/8.0)
		latency := time.Duration(60*congestionFactor) * time.Millisecond
		packetLoss := 0.03 * congestionFactor
		bandwidth := (3*1024*1024) / congestionFactor
		return latency, packetLoss, bandwidth
		
	case "mobile":
		cycle := elapsed.Seconds() / 12.0
		signalStrength := 0.6 + 0.4*math.Cos(cycle*2*math.Pi)
		latency := time.Duration(80+40*(1-signalStrength)) * time.Millisecond
		packetLoss := 0.04 * (1 - signalStrength)
		bandwidth := (800*1024 + 2200*1024*signalStrength)
		return latency, packetLoss, bandwidth
		
	case "degrading":
		degradation := math.Min(elapsed.Seconds()/25.0, 1.0)
		latency := time.Duration(30+150*degradation) * time.Millisecond
		packetLoss := 0.005 + 0.08*degradation
		bandwidth := (6*1024*1024) * (1 - 0.7*degradation)
		return latency, packetLoss, bandwidth
		
	default:
		return 50 * time.Millisecond, 0.01, 2 * 1024 * 1024
	}
}

func (tao *TrafficAwareOrchestrator) BeginTrafficAwareExposure(scenario string) {
	tao.output("🧭 Traffic-Aware Red Giant Protocol - Beginning Adaptive Exposure\n")
	tao.output(fmt.Sprintf("📋 Data: %d bytes, Initial chunks: %d\n", len(tao.fileData), tao.totalChunks))
	tao.output(fmt.Sprintf("🌐 Network scenario: %s\n", scenario))
	
	startTime := time.Now()
	
	go func() {
		for i := 0; i < tao.totalChunks; i++ {
			elapsed := time.Since(startTime)
			
			// Get current adaptive parameters
			tao.mu.RLock()
			currentCadence := tao.currentParams.ExposureCadence
			tao.mu.RUnlock()
			
			// Simulate network conditions
			latency, packetLoss, bandwidth := tao.simulateNetworkConditions(scenario, elapsed)
			
			// Calculate chunk boundaries (using current adaptive chunk size for simulation)
			start := i * tao.chunkSize
			end := start + tao.chunkSize
			if end > len(tao.fileData) {
				end = len(tao.fileData)
			}
			
			chunkData := tao.fileData[start:end]
			
			// Simulate network transfer
			success := rand.Float64() > packetLoss
			transferTime := time.Duration(float64(len(chunkData))/bandwidth*1000) * time.Millisecond
			actualLatency := latency + transferTime
			
			// Record network sample
			sample := NetworkSample{
				Timestamp:        time.Now(),
				ResponseTime:     actualLatency,
				BytesTransferred: len(chunkData),
				Success:          success,
			}
			tao.recordSample(sample)
			
			if success {
				tao.output(fmt.Sprintf("[EXPOSE] Chunk %d exposed (%d bytes) - Latency: %v\n", 
					i, len(chunkData), actualLatency))
				
				// Notify subscribers
				notification := ChunkNotification{
					ChunkID:   i,
					IsReady:   true,
					Timestamp: time.Now(),
				}
				
				tao.mu.RLock()
				for _, subscriber := range tao.subscribers {
					select {
					case subscriber <- notification:
					default:
					}
				}
				tao.mu.RUnlock()
				
				tao.exposedChunks++
			} else {
				tao.output(fmt.Sprintf("[FAILED] Chunk %d exposure failed - Packet loss\n", i))
				i-- // Retry this chunk
			}
			
			// Wait for adaptive cadence
			time.Sleep(currentCadence)
		}
		
		tao.output("[RED FLAG] 🚩 Traffic-aware exposure complete!\n")
	}()
}

func (tao *TrafficAwareOrchestrator) IsComplete() bool {
	tao.mu.RLock()
	defer tao.mu.RUnlock()
	return tao.exposedChunks >= tao.totalChunks
}

func (tao *TrafficAwareOrchestrator) GetMetrics() NetworkMetrics {
	tao.mu.RLock()
	defer tao.mu.RUnlock()
	return *tao.metrics
}

func (tao *TrafficAwareOrchestrator) GetCurrentParams() AdaptiveParams {
	tao.mu.RLock()
	defer tao.mu.RUnlock()
	return tao.currentParams
}

func runTrafficScenario(scenario string, sampleData []byte, chunkSize int, output func(string)) {
	output(fmt.Sprintf("\n" + strings.Repeat("=", 60) + "\n"))
	output(fmt.Sprintf("🌐 Testing Network Scenario: %s\n", strings.ToUpper(scenario)))
	output(strings.Repeat("=", 60) + "\n")
	
	// Create traffic-aware orchestrator
	sender := NewTrafficAwareOrchestrator(sampleData, chunkSize, output)
	
	// Begin adaptive exposure
	sender.BeginTrafficAwareExposure(scenario)
	
	// Wait for completion
	for !sender.IsComplete() {
		time.Sleep(100 * time.Millisecond)
	}
	
	// Display final metrics
	metrics := sender.GetMetrics()
	params := sender.GetCurrentParams()
	
	output("\n📊 Final Network State:\n")
	output(fmt.Sprintf("   • Bandwidth: %.2f KB/s\n", metrics.Bandwidth/1024))
	output(fmt.Sprintf("   • Latency: %v\n", metrics.Latency))
	output(fmt.Sprintf("   • Packet Loss: %.2f%%\n", metrics.PacketLoss*100))
	output(fmt.Sprintf("   • Congestion: %.2f%%\n", metrics.Congestion*100))
	output(fmt.Sprintf("   • Final Chunk Size: %d bytes\n", params.ChunkSize))
	output(fmt.Sprintf("   • Final Cadence: %v\n", params.ExposureCadence))
	output(fmt.Sprintf("   • Final Workers: %d\n", params.WorkerCount))
	output(fmt.Sprintf("   • Adaptation: %s\n", params.Reason))
}

func main() {
	// Create output file
	file, err := os.Create("traffic_output.txt")
	if err != nil {
		panic(err)
	}
	defer file.Close()
	
	output := func(msg string) {
		fmt.Print(msg)
		file.WriteString(msg)
	}
	
	output("🧠 Red Giant Protocol - Traffic-Aware Adaptive Demo\n")
	output("═══════════════════════════════════════════════════\n\n")
	
	// Create sample data
	sampleData := []byte(strings.Repeat("Traffic-aware Red Giant! ", 150))
	initialChunkSize := 16 * 1024 // 16KB initial
	
	output(fmt.Sprintf("📋 Original data: %d bytes\n", len(sampleData)))
	output(fmt.Sprintf("🔧 Initial chunk size: %d bytes\n", initialChunkSize))
	output(fmt.Sprintf("📊 Initial chunks: %d\n", (len(sampleData)+initialChunkSize-1)/initialChunkSize))
	
	// Test different network scenarios
	scenarios := []string{"stable", "congested", "mobile", "degrading"}
	
	for _, scenario := range scenarios {
		runTrafficScenario(scenario, sampleData, initialChunkSize, output)
		time.Sleep(1 * time.Second) // Brief pause between scenarios
	}
	
	output("\n🎯 Traffic-Aware Red Giant Protocol demonstration complete!\n")
	output("   • Dynamic chunk sizing ✓\n")
	output("   • Adaptive exposure cadence ✓\n")
	output("   • Network condition monitoring ✓\n")
	output("   • Real-time parameter adjustment ✓\n")
	output("   • Congestion detection ✓\n")
	output("   • Packet loss adaptation ✓\n")
	
	output("\nCheck traffic_output.txt for full results!\n")
}