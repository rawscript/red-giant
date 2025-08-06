
// Global Red Giant Protocol Test Client
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"time"
)

type GlobalTestResult struct {
	Location         string  `json:"location"`
	Country          string  `json:"country"`
	Timestamp        int64   `json:"timestamp"`
	HealthLatencyMs  int64   `json:"health_latency_ms"`
	UploadSpeedMbps  float64 `json:"upload_speed_mbps"`
	UploadLatencyMs  int64   `json:"upload_latency_ms"`
	ProcessingTimeMs int64   `json:"processing_time_ms"`
	ThroughputMbps   float64 `json:"throughput_mbps"`
	ErrorCount       int     `json:"error_count"`
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("🌍 Red Giant Global Performance Tester")
		fmt.Println("Usage: go run global_test_client.go <server-url>")
		fmt.Println("Example: go run global_test_client.go https://your-repl.repl.co")
		return
	}

	serverURL := os.Args[1]
	fmt.Printf("🚀 Starting global performance test against: %s\n", serverURL)
	
	// Get location info
	location, country := getLocationInfo()
	fmt.Printf("📍 Testing from: %s, %s\n\n", location, country)

	result := &GlobalTestResult{
		Location:  location,
		Country:   country,
		Timestamp: time.Now().Unix(),
	}

	// Test 1: Health Check Latency
	fmt.Print("1. Health Check Test... ")
	healthLatency := testHealthCheck(serverURL)
	result.HealthLatencyMs = healthLatency.Milliseconds()
	if healthLatency > 0 {
		fmt.Printf("✅ %v\n", healthLatency)
	} else {
		fmt.Printf("❌ FAILED\n")
		result.ErrorCount++
	}

	// Test 2: Upload Performance
	fmt.Print("2. Upload Performance Test... ")
	uploadSpeed, uploadLatency := testUploadPerformance(serverURL)
	result.UploadSpeedMbps = uploadSpeed
	result.UploadLatencyMs = uploadLatency.Milliseconds()
	if uploadSpeed > 0 {
		fmt.Printf("✅ %.2f MB/s (%v)\n", uploadSpeed, uploadLatency)
	} else {
		fmt.Printf("❌ FAILED\n")
		result.ErrorCount++
	}

	// Test 3: Get Server Metrics
	fmt.Print("3. Server Metrics Test... ")
	throughput, processingTime := getServerMetrics(serverURL)
	result.ThroughputMbps = throughput
	result.ProcessingTimeMs = processingTime
	if throughput > 0 {
		fmt.Printf("✅ %.2f MB/s (avg: %dms)\n", throughput, processingTime)
	} else {
		fmt.Printf("❌ FAILED\n")
		result.ErrorCount++
	}

	// Display final results
	fmt.Printf("\n📊 Global Test Results Summary:\n")
	fmt.Printf("════════════════════════════════\n")
	fmt.Printf("📍 Location: %s, %s\n", result.Location, result.Country)
	fmt.Printf("⏱️  Health Check Latency: %dms\n", result.HealthLatencyMs)
	fmt.Printf("🚀 Upload Speed: %.2f MB/s\n", result.UploadSpeedMbps)
	fmt.Printf("⚡ Server Throughput: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("🔧 Processing Time: %dms\n", result.ProcessingTimeMs)
	fmt.Printf("❌ Errors: %d\n", result.ErrorCount)

	// Performance grade
	grade := calculatePerformanceGrade(result)
	fmt.Printf("🏆 Performance Grade: %s\n", grade)

	// Save results to JSON
	resultJSON, _ := json.MarshalIndent(result, "", "  ")
	filename := fmt.Sprintf("global_test_result_%s_%d.json", result.Country, result.Timestamp)
	os.WriteFile(filename, resultJSON, 0644)
	fmt.Printf("💾 Results saved to: %s\n", filename)
}

func getLocationInfo() (string, string) {
	// Try to get location from ipinfo.io
	client := &http.Client{Timeout: 5 * time.Second}
	
	cityResp, err := client.Get("https://ipinfo.io/city")
	if err != nil {
		return "Unknown", "Unknown"
	}
	defer cityResp.Body.Close()
	
	countryResp, err := client.Get("https://ipinfo.io/country")
	if err != nil {
		return "Unknown", "Unknown"
	}
	defer countryResp.Body.Close()
	
	cityBytes, _ := io.ReadAll(cityResp.Body)
	countryBytes, _ := io.ReadAll(countryResp.Body)
	
	city := string(bytes.TrimSpace(cityBytes))
	country := string(bytes.TrimSpace(countryBytes))
	
	if city == "" {
		city = "Unknown"
	}
	if country == "" {
		country = "Unknown"
	}
	
	return city, country
}

func testHealthCheck(serverURL string) time.Duration {
	client := &http.Client{Timeout: 30 * time.Second}
	
	start := time.Now()
	resp, err := client.Get(serverURL + "/health")
	duration := time.Since(start)
	
	if err != nil {
		return 0
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != 200 {
		return 0
	}
	
	return duration
}

func testUploadPerformance(serverURL string) (float64, time.Duration) {
	// Create 5MB test data
	testData := bytes.Repeat([]byte("Red Giant Performance Test Data! "), 157286) // ~5MB
	
	client := &http.Client{Timeout: 60 * time.Second}
	
	start := time.Now()
	resp, err := client.Post(
		serverURL+"/upload",
		"application/octet-stream",
		bytes.NewReader(testData),
	)
	duration := time.Since(start)
	
	if err != nil {
		return 0, 0
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != 200 {
		return 0, 0
	}
	
	// Calculate throughput in MB/s
	sizeMB := float64(len(testData)) / (1024 * 1024)
	speedMbps := sizeMB / duration.Seconds()
	
	return speedMbps, duration
}

func getServerMetrics(serverURL string) (float64, int64) {
	client := &http.Client{Timeout: 10 * time.Second}
	
	resp, err := client.Get(serverURL + "/metrics")
	if err != nil {
		return 0, 0
	}
	defer resp.Body.Close()
	
	var metrics map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&metrics); err != nil {
		return 0, 0
	}
	
	throughput, ok1 := metrics["throughput_mbps"].(float64)
	avgLatency, ok2 := metrics["average_latency_ms"].(float64)
	
	if !ok1 {
		throughput = 0
	}
	if !ok2 {
		avgLatency = 0
	}
	
	return throughput, int64(avgLatency)
}

func calculatePerformanceGrade(result *GlobalTestResult) string {
	score := 0
	
	// Health check latency scoring
	if result.HealthLatencyMs < 50 {
		score += 25
	} else if result.HealthLatencyMs < 100 {
		score += 20
	} else if result.HealthLatencyMs < 200 {
		score += 15
	} else if result.HealthLatencyMs < 500 {
		score += 10
	}
	
	// Upload speed scoring
	if result.UploadSpeedMbps > 400 {
		score += 25
	} else if result.UploadSpeedMbps > 200 {
		score += 20
	} else if result.UploadSpeedMbps > 100 {
		score += 15
	} else if result.UploadSpeedMbps > 50 {
		score += 10
	} else if result.UploadSpeedMbps > 10 {
		score += 5
	}
	
	// Server throughput scoring
	if result.ThroughputMbps > 500 {
		score += 25
	} else if result.ThroughputMbps > 300 {
		score += 20
	} else if result.ThroughputMbps > 200 {
		score += 15
	} else if result.ThroughputMbps > 100 {
		score += 10
	} else if result.ThroughputMbps > 50 {
		score += 5
	}
	
	// Processing time scoring
	if result.ProcessingTimeMs < 5 {
		score += 25
	} else if result.ProcessingTimeMs < 10 {
		score += 20
	} else if result.ProcessingTimeMs < 20 {
		score += 15
	} else if result.ProcessingTimeMs < 50 {
		score += 10
	} else if result.ProcessingTimeMs < 100 {
		score += 5
	}
	
	// Error penalty
	score -= result.ErrorCount * 10
	
	// Grade assignment
	if score >= 90 {
		return "A+ (Excellent Global Performance)"
	} else if score >= 80 {
		return "A (Very Good Performance)"
	} else if score >= 70 {
		return "B+ (Good Performance)"
	} else if score >= 60 {
		return "B (Acceptable Performance)"
	} else if score >= 50 {
		return "C (Limited Performance)"
	} else {
		return "F (Poor Performance - Check Network)"
	}
}
