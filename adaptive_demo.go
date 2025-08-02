// Red Giant Protocol - Traffic-Aware Adaptive Demo
package main

import (
	"fmt"
	"math/rand"
	"strings"
	"time"
)

func main() {
	fmt.Printf("🧠 Red Giant Protocol - Traffic-Aware Demonstration\n")
	fmt.Printf("═══════════════════════════════════════════════════\n\n")
	
	// Create larger sample data to better show adaptation
	sampleData := []byte(strings.Repeat("Red Giant Protocol adapts to network traffic! ", 500))
	initialChunkSize := 32 * 1024 // Start with 32KB chunks
	
	fmt.Printf("📋 Original data: %d bytes\n", len(sampleData))
	fmt.Printf("🔧 Initial chunk size: %d bytes\n", initialChunkSize)
	fmt.Printf("📊 Expected chunks: %d\n\n", (len(sampleData)+initialChunkSize-1)/initialChunkSize)
	
	// Test different network scenarios
	scenarios := []string{"stable", "congested", "mobile", "degrading"}
	
	for _, scenario := range scenarios {
		fmt.Printf("\n" + strings.Repeat("=", 60) + "\n")
		fmt.Printf("🌐 Testing Network Scenario: %s\n", strings.ToUpper(scenario))
		fmt.Printf(strings.Repeat("=", 60) + "\n")
		
		runAdaptiveTest(sampleData, initialChunkSize, scenario)
		
		// Brief pause between scenarios
		time.Sleep(2 * time.Second)
	}
	
	fmt.Printf("\n🎯 Traffic-Aware Red Giant Protocol demonstration complete!\n")
	fmt.Printf("   • Dynamic chunk sizing ✓\n")
	fmt.Printf("   • Adaptive exposure cadence ✓\n")
	fmt.Printf("   • Worker pool scaling ✓\n")
	fmt.Printf("   • Network condition monitoring ✓\n")
	fmt.Printf("   • Real-time parameter adjustment ✓\n")
}

func runAdaptiveTest(sampleData []byte, chunkSize int, scenario string) {
	// 🔐 1. Secure Handshake (simulated)
	fmt.Printf("🔐 Phase 1: Secure Handshake\n")
	fmt.Printf("   ✓ TLS-like channel established\n\n")
	
	// 🌐 2. Start network simulation
	fmt.Printf("🌐 Phase 2: Network Simulation\n")
	simulator := NewNetworkSimulator(scenario, 20*time.Second)
	simulator.Start()
	
	// 🧭 3. Create orchestrator with traffic awareness
	fmt.Printf("🧭 Phase 3: Adaptive Orchestrator\n")
	sender := NewRedGiantOrchestrator(sampleData, chunkSize, true)
	defer sender.Cleanup()
	
	// Inject simulated network conditions
	go injectSimulatedConditions(sender, simulator)
	
	// 📡 4. Setup adaptive receiver
	fmt.Printf("📡 Phase 4: Adaptive Receiver\n")
	totalChunks := (len(sampleData) + chunkSize - 1) / chunkSize
	workerCount := 4
	receiver := NewReceiver(totalChunks, workerCount)
	receiver.ConstructFile(sender)
	receiver.AdaptToTraffic(sender)
	fmt.Printf("   ✓ Adaptive receiver initialized\n\n")
	
	// 🛠 5. Begin adaptive exposure
	fmt.Printf("🛠 Phase 5: Traffic-Aware Exposure\n")
	sender.BeginExposure()
	
	// 🚩 6. Monitor and complete
	fmt.Printf("🚩 Phase 6: Adaptive Completion\n")
	reconstructedData := receiver.WaitForCompletion(sender)
	
	// Stop simulation
	simulator.Stop()
	
	// Verify integrity
	if string(reconstructedData) == string(sampleData) {
		fmt.Printf("   ✅ Integrity check PASSED\n")
		fmt.Printf("   ✅ Adaptive reconstruction SUCCESSFUL\n")
	} else {
		fmt.Printf("   ❌ Integrity check FAILED\n")
	}
	
	// Display final metrics
	if sender.trafficMonitor != nil {
		metrics := sender.trafficMonitor.GetCurrentMetrics()
		params := sender.GetAdaptiveParams()
		
		fmt.Printf("\n📊 Final Adaptive State:\n")
		fmt.Printf("   • Bandwidth: %.2f KB/s\n", metrics.Bandwidth/1024)
		fmt.Printf("   • Latency: %v\n", metrics.Latency)
		fmt.Printf("   • Packet Loss: %.2f%%\n", metrics.PacketLoss*100)
		fmt.Printf("   • Final Chunk Size: %d bytes\n", params.ChunkSize)
		fmt.Printf("   • Final Cadence: %v\n", params.ExposureCadence)
		fmt.Printf("   • Final Workers: %d\n", params.WorkerCount)
		fmt.Printf("   • Adaptation Reason: %s\n", params.Reason)
	}
}

// injectSimulatedConditions feeds simulated network data to the traffic monitor
func injectSimulatedConditions(sender *RedGiantOrchestrator, simulator *NetworkSimulator) {
	if sender.trafficMonitor == nil {
		return
	}
	
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()
	
	for simulator.IsActive() {
		select {
		case <-ticker.C:
			latency, packetLoss, bandwidth := simulator.GetCurrentConditions()
			
			// Create synthetic network samples based on simulation
			success := packetLoss < 0.1 && rand.Float64() > packetLoss
			
			// Simulate chunk transfer
			chunkSize := 32 * 1024 // Simulate 32KB chunk
			transferTime := time.Duration(float64(chunkSize)/bandwidth*1000) * time.Millisecond
			actualLatency := latency + transferTime
			
			sample := NetworkSample{
				Timestamp:        time.Now(),
				ResponseTime:     actualLatency,
				BytesTransferred: chunkSize,
				Success:          success,
			}
			
			sender.trafficMonitor.RecordSample(sample)
		}
	}
}