// Red Giant Protocol - Client Library
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

type RedGiantClient struct {
	BaseURL    string
	HTTPClient *http.Client
}

type ProcessResponse struct {
	Status           string  `json:"status"`
	BytesProcessed   int     `json:"bytes_processed"`
	ChunksProcessed  int     `json:"chunks_processed"`
	ProcessingTimeMs int64   `json:"processing_time_ms"`
	ThroughputMbps   float64 `json:"throughput_mbps"`
	Timestamp        int64   `json:"timestamp"`
}

type HealthResponse struct {
	Status    string  `json:"status"`
	Version   string  `json:"version"`
	Timestamp int64   `json:"timestamp"`
	Uptime    float64 `json:"uptime"`
}

type MetricsResponse struct {
	TotalRequests    int64   `json:"total_requests"`
	TotalBytes       int64   `json:"total_bytes"`
	TotalChunks      int64   `json:"total_chunks"`
	AverageLatencyMs int64   `json:"average_latency_ms"`
	ErrorCount       int64   `json:"error_count"`
	UptimeSeconds    int64   `json:"uptime_seconds"`
	ThroughputMbps   float64 `json:"throughput_mbps"`
	Timestamp        int64   `json:"timestamp"`
}

func NewRedGiantClient(baseURL string) *RedGiantClient {
	return &RedGiantClient{
		BaseURL: baseURL,
		HTTPClient: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

func (c *RedGiantClient) ProcessData(data []byte) (*ProcessResponse, error) {
	url := c.BaseURL + "/process"
	
	resp, err := c.HTTPClient.Post(url, "application/octet-stream", bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}
	
	var result ProcessResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode failed: %w", err)
	}
	
	return &result, nil
}

func (c *RedGiantClient) ProcessFile(filename string) (*ProcessResponse, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("read file failed: %w", err)
	}
	
	return c.ProcessData(data)
}

func (c *RedGiantClient) Health() (*HealthResponse, error) {
	url := c.BaseURL + "/health"
	
	resp, err := c.HTTPClient.Get(url)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()
	
	var result HealthResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode failed: %w", err)
	}
	
	return &result, nil
}

func (c *RedGiantClient) Metrics() (*MetricsResponse, error) {
	url := c.BaseURL + "/metrics"
	
	resp, err := c.HTTPClient.Get(url)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()
	
	var result MetricsResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("decode failed: %w", err)
	}
	
	return &result, nil
}

// Example usage and testing
func main() {
	if len(os.Args) < 2 {
		fmt.Println("Red Giant Protocol Client")
		fmt.Println("Usage:")
		fmt.Println("  go run client.go test          - Run basic test")
		fmt.Println("  go run client.go file <path>   - Process file")
		fmt.Println("  go run client.go health        - Check server health")
		fmt.Println("  go run client.go metrics       - Get server metrics")
		fmt.Println("  go run client.go benchmark     - Run performance benchmark")
		return
	}

	client := NewRedGiantClient("http://localhost:8080")
	
	switch os.Args[1] {
	case "test":
		runBasicTest(client)
	case "file":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run client.go file <filepath>")
			return
		}
		processFile(client, os.Args[2])
	case "health":
		checkHealth(client)
	case "metrics":
		getMetrics(client)
	case "benchmark":
		runBenchmark(client)
	default:
		fmt.Printf("Unknown command: %s\n", os.Args[1])
	}
}

func runBasicTest(client *RedGiantClient) {
	fmt.Println("🧪 Running basic Red Giant Protocol test...")
	
	testData := []byte("Red Giant Protocol Test Data - High Performance Transmission!")
	
	result, err := client.ProcessData(testData)
	if err != nil {
		fmt.Printf("❌ Test failed: %v\n", err)
		return
	}
	
	fmt.Printf("✅ Test successful!\n")
	fmt.Printf("   📊 Processed: %d bytes in %d chunks\n", result.BytesProcessed, result.ChunksProcessed)
	fmt.Printf("   ⚡ Speed: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("   ⏱️  Time: %d ms\n", result.ProcessingTimeMs)
}

func processFile(client *RedGiantClient, filename string) {
	fmt.Printf("📁 Processing file: %s\n", filename)
	
	result, err := client.ProcessFile(filename)
	if err != nil {
		fmt.Printf("❌ File processing failed: %v\n", err)
		return
	}
	
	fmt.Printf("✅ File processed successfully!\n")
	fmt.Printf("   📊 Processed: %d bytes in %d chunks\n", result.BytesProcessed, result.ChunksProcessed)
	fmt.Printf("   ⚡ Speed: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("   ⏱️  Time: %d ms\n", result.ProcessingTimeMs)
}

func checkHealth(client *RedGiantClient) {
	fmt.Println("🏥 Checking server health...")
	
	health, err := client.Health()
	if err != nil {
		fmt.Printf("❌ Health check failed: %v\n", err)
		return
	}
	
	fmt.Printf("✅ Server is %s\n", health.Status)
	fmt.Printf("   📋 Version: %s\n", health.Version)
	fmt.Printf("   ⏰ Uptime: %.0f seconds\n", health.Uptime)
}

func getMetrics(client *RedGiantClient) {
	fmt.Println("📊 Getting server metrics...")
	
	metrics, err := client.Metrics()
	if err != nil {
		fmt.Printf("❌ Metrics request failed: %v\n", err)
		return
	}
	
	fmt.Printf("📈 Server Metrics:\n")
	fmt.Printf("   🔢 Total Requests: %d\n", metrics.TotalRequests)
	fmt.Printf("   📦 Total Bytes: %d (%.2f MB)\n", metrics.TotalBytes, float64(metrics.TotalBytes)/(1024*1024))
	fmt.Printf("   🧩 Total Chunks: %d\n", metrics.TotalChunks)
	fmt.Printf("   ⚡ Throughput: %.2f MB/s\n", metrics.ThroughputMbps)
	fmt.Printf("   ⏱️  Avg Latency: %d ms\n", metrics.AverageLatencyMs)
	fmt.Printf("   ❌ Errors: %d\n", metrics.ErrorCount)
	fmt.Printf("   ⏰ Uptime: %d seconds\n", metrics.UptimeSeconds)
}

func runBenchmark(client *RedGiantClient) {
	fmt.Println("🏁 Running performance benchmark...")
	
	// Test different data sizes
	sizes := []int{
		1024,           // 1KB
		1024 * 1024,    // 1MB
		10 * 1024 * 1024, // 10MB
	}
	
	for _, size := range sizes {
		fmt.Printf("\n📊 Testing %d bytes (%.2f MB)...\n", size, float64(size)/(1024*1024))
		
		// Generate test data
		testData := make([]byte, size)
		for i := range testData {
			testData[i] = byte(i % 256)
		}
		
		// Run test
		start := time.Now()
		result, err := client.ProcessData(testData)
		totalTime := time.Since(start)
		
		if err != nil {
			fmt.Printf("❌ Benchmark failed: %v\n", err)
			continue
		}
		
		fmt.Printf("   ✅ Success: %.2f MB/s (server: %.2f MB/s)\n", 
			float64(size)/totalTime.Seconds()/(1024*1024), result.ThroughputMbps)
		fmt.Printf("   ⏱️  Total time: %v (processing: %d ms)\n", totalTime, result.ProcessingTimeMs)
		fmt.Printf("   🧩 Chunks: %d\n", result.ChunksProcessed)
	}
}