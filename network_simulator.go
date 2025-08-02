// Red Giant Protocol - Network Condition Simulator
package main

import (
	"fmt"
	"math"
	"math/rand"
	"time"
)

// NetworkSimulator simulates various network conditions for testing
type NetworkSimulator struct {
	scenario    string
	duration    time.Duration
	startTime   time.Time
	isActive    bool
}

// NetworkScenario defines different network condition patterns
type NetworkScenario struct {
	Name        string
	Description string
	Simulate    func(elapsed time.Duration) (latency time.Duration, packetLoss float64, bandwidth float64)
}

var NetworkScenarios = []NetworkScenario{
	{
		Name:        "stable",
		Description: "Stable high-speed connection",
		Simulate: func(elapsed time.Duration) (time.Duration, float64, float64) {
			return 20*time.Millisecond + time.Duration(rand.Intn(10))*time.Millisecond,
				0.001, // 0.1% packet loss
				10*1024*1024 + float64(rand.Intn(2*1024*1024)) // 10-12 MB/s
		},
	},
	{
		Name:        "congested",
		Description: "Network congestion with variable performance",
		Simulate: func(elapsed time.Duration) (time.Duration, float64, float64) {
			// Simulate congestion waves
			congestionFactor := 1.0 + 0.5*math.Sin(float64(elapsed.Seconds())/10.0)
			latency := time.Duration(50*congestionFactor) * time.Millisecond
			packetLoss := 0.02 * congestionFactor // 2-5% packet loss
			bandwidth := (2*1024*1024) / congestionFactor // 1-2 MB/s
			return latency, packetLoss, bandwidth
		},
	},
	{
		Name:        "mobile",
		Description: "Mobile network with intermittent connectivity",
		Simulate: func(elapsed time.Duration) (time.Duration, float64, float64) {
			// Simulate mobile tower handoffs and signal strength
			cycle := elapsed.Seconds() / 15.0 // 15-second cycles
			signalStrength := 0.5 + 0.5*math.Cos(cycle*2*math.Pi)
			
			latency := time.Duration(100+50*(1-signalStrength)) * time.Millisecond
			packetLoss := 0.05 * (1 - signalStrength) // 0-5% based on signal
			bandwidth := (500*1024 + 1500*1024*signalStrength) // 500KB-2MB/s
			return latency, packetLoss, bandwidth
		},
	},
	{
		Name:        "degrading",
		Description: "Gradually degrading connection",
		Simulate: func(elapsed time.Duration) (time.Duration, float64, float64) {
			// Linear degradation over time
			degradation := math.Min(elapsed.Seconds()/30.0, 1.0) // Degrade over 30 seconds
			
			latency := time.Duration(20+200*degradation) * time.Millisecond
			packetLoss := 0.001 + 0.1*degradation // 0.1% to 10%
			bandwidth := (5*1024*1024) * (1 - 0.8*degradation) // 5MB/s to 1MB/s
			return latency, packetLoss, bandwidth
		},
	},
	{
		Name:        "recovering",
		Description: "Network recovering from poor conditions",
		Simulate: func(elapsed time.Duration) (time.Duration, float64, float64) {
			// Exponential recovery
			recovery := 1.0 - math.Exp(-elapsed.Seconds()/10.0) // Recover over ~30 seconds
			
			latency := time.Duration(200-180*recovery) * time.Millisecond
			packetLoss := 0.1 * (1 - recovery) // 10% to 0%
			bandwidth := (500*1024) + (9.5*1024*1024)*recovery // 500KB/s to 10MB/s
			return latency, packetLoss, bandwidth
		},
	},
}

func NewNetworkSimulator(scenarioName string, duration time.Duration) *NetworkSimulator {
	return &NetworkSimulator{
		scenario: scenarioName,
		duration: duration,
		isActive: false,
	}
}

// Start begins the network simulation
func (ns *NetworkSimulator) Start() {
	ns.startTime = time.Now()
	ns.isActive = true
	
	// Find the scenario
	var selectedScenario *NetworkScenario
	for _, scenario := range NetworkScenarios {
		if scenario.Name == ns.scenario {
			selectedScenario = &scenario
			break
		}
	}
	
	if selectedScenario == nil {
		fmt.Printf("⚠️  Unknown network scenario: %s\n", ns.scenario)
		return
	}
	
	fmt.Printf("🌐 Network Simulation: %s\n", selectedScenario.Description)
	fmt.Printf("   Duration: %v\n", ns.duration)
}

// GetCurrentConditions returns simulated network conditions
func (ns *NetworkSimulator) GetCurrentConditions() (latency time.Duration, packetLoss float64, bandwidth float64) {
	if !ns.isActive {
		return 50 * time.Millisecond, 0.01, 1024 * 1024 // Default conditions
	}
	
	elapsed := time.Since(ns.startTime)
	if elapsed > ns.duration {
		ns.isActive = false
		return 50 * time.Millisecond, 0.01, 1024 * 1024 // Return to default
	}
	
	// Find and execute the scenario
	for _, scenario := range NetworkScenarios {
		if scenario.Name == ns.scenario {
			return scenario.Simulate(elapsed)
		}
	}
	
	return 50 * time.Millisecond, 0.01, 1024 * 1024 // Fallback
}

// IsActive returns whether the simulation is currently running
func (ns *NetworkSimulator) IsActive() bool {
	if !ns.isActive {
		return false
	}
	
	elapsed := time.Since(ns.startTime)
	if elapsed > ns.duration {
		ns.isActive = false
		return false
	}
	
	return true
}

// Stop ends the simulation
func (ns *NetworkSimulator) Stop() {
	ns.isActive = false
}