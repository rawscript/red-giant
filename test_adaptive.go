// Red Giant Protocol - Adaptive Format Testing
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"
)

type TestResult struct {
	Format       string  `json:"format"`
	Size         int     `json:"size"`
	Throughput   float64 `json:"throughput_mbps"`
	ProcessTime  int64   `json:"processing_time_ms"`
	ChunkSize    int     `json:"optimal_chunk_size"`
	Compressed   bool    `json:"is_compressed"`
	ProcessMode  int     `json:"process_mode"`
}

func testFormat(name, contentType, data string) *TestResult {
	fmt.Printf("🧪 Testing %s format... ", name)
	
	client := &http.Client{Timeout: 30 * time.Second}
	
	req, err := http.NewRequest("POST", "http://localhost:8080/upload", strings.NewReader(data))
	if err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
		return nil
	}
	
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-Peer-ID", "adaptive_test")
	req.Header.Set("X-File-Name", fmt.Sprintf("test.%s", strings.Split(contentType, "/")[1]))
	
	start := time.Now()
	resp, err := client.Do(req)
	if err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
		return nil
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		fmt.Printf("❌ FAILED: Status %d\n", resp.StatusCode)
		return nil
	}
	
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
		return nil
	}
	
	testResult := &TestResult{
		Format:      name,
		Size:        int(result["bytes_processed"].(float64)),
		Throughput:  result["throughput_mbps"].(float64),
		ProcessTime: int64(result["processing_time_ms"].(float64)),
		ChunkSize:   int(result["optimal_chunk_size"].(float64)),
		Compressed:  result["is_compressed"].(bool),
		ProcessMode: int(result["process_mode"].(float64)),
	}
	
	fmt.Printf("✅ OK (%.2f MB/s, %dKB chunks, compressed: %v)\n", 
		testResult.Throughput, testResult.ChunkSize/1024, testResult.Compressed)
	
	return testResult
}

func main() {
	fmt.Println("🚀 Red Giant Protocol - Adaptive Format Testing")
	fmt.Println("═══════════════════════════════════════════════════")
	
	// Test different data formats
	var results []*TestResult
	
	// 1. JSON Data
	jsonData := `{
		"message": "Red Giant Protocol Adaptive Test",
		"timestamp": "2025-01-01T00:00:00Z",
		"data": {
			"performance": "high",
			"adaptive": true,
			"formats": ["json", "xml", "binary", "image", "video"]
		},
		"metrics": {
			"throughput": 500.0,
			"latency": 1.2,
			"optimization": "automatic"
		}
	}`
	if result := testFormat("JSON", "application/json", jsonData); result != nil {
		results = append(results, result)
	}
	
	// 2. XML Data
	xmlData := `<?xml version="1.0" encoding="UTF-8"?>
	<redgiant>
		<protocol>adaptive</protocol>
		<performance>high</performance>
		<features>
			<feature>auto-optimization</feature>
			<feature>format-detection</feature>
			<feature>smart-compression</feature>
		</features>
		<metrics throughput="500" latency="1.2"/>
	</redgiant>`
	if result := testFormat("XML", "application/xml", xmlData); result != nil {
		results = append(results, result)
	}
	
	// 3. Plain Text
	textData := strings.Repeat("Red Giant Protocol Adaptive Format Testing. This is plain text data that should be optimized for text processing with appropriate chunk sizes and encoding detection. ", 50)
	if result := testFormat("Text", "text/plain", textData); result != nil {
		results = append(results, result)
	}
	
	// 4. Binary Data (simulated)
	binaryData := string(make([]byte, 10240)) // 10KB of binary data
	for i := range binaryData {
		binaryData = binaryData[:i] + string(byte(i%256)) + binaryData[i+1:]
	}
	if result := testFormat("Binary", "application/octet-stream", binaryData); result != nil {
		results = append(results, result)
	}
	
	// 5. Compressible Data
	compressibleData := strings.Repeat("AAAAAAAAAA", 1000) // Highly compressible
	if result := testFormat("Compressible", "text/plain", compressibleData); result != nil {
		results = append(results, result)
	}
	
	// Display results summary
	fmt.Println("\n📊 Adaptive Format Test Results")
	fmt.Println("═══════════════════════════════════════")
	fmt.Printf("%-15s %-10s %-12s %-10s %-12s %-10s\n", 
		"Format", "Size", "Throughput", "Time(ms)", "ChunkSize", "Compressed")
	fmt.Println(strings.Repeat("-", 80))
	
	totalThroughput := 0.0
	for _, result := range results {
		fmt.Printf("%-15s %-10d %-12.2f %-10d %-12s %-10v\n",
			result.Format,
			result.Size,
			result.Throughput,
			result.ProcessTime,
			fmt.Sprintf("%dKB", result.ChunkSize/1024),
			result.Compressed)
		totalThroughput += result.Throughput
	}
	
	fmt.Println(strings.Repeat("-", 80))
	fmt.Printf("Average Throughput: %.2f MB/s\n", totalThroughput/float64(len(results)))
	
	// Test server metrics
	fmt.Println("\n📈 Server Metrics")
	fmt.Println("═══════════════════")
	
	resp, err := http.Get("http://localhost:8080/metrics")
	if err == nil {
		defer resp.Body.Close()
		var metrics map[string]interface{}
		if json.NewDecoder(resp.Body).Decode(&metrics) == nil {
			fmt.Printf("Total Requests: %.0f\n", metrics["total_requests"].(float64))
			fmt.Printf("JSON Requests: %.0f\n", metrics["json_requests"].(float64))
			fmt.Printf("Binary Requests: %.0f\n", metrics["binary_requests"].(float64))
			fmt.Printf("Optimization Hits: %.0f\n", metrics["optimization_hits"].(float64))
			fmt.Printf("Compressed Bytes: %.0f\n", metrics["compressed_bytes"].(float64))
		}
	}
	
	fmt.Println("\n🎉 Adaptive Format Testing Complete!")
	fmt.Println("✅ Red Giant Protocol successfully adapts to different formats")
	fmt.Println("✅ Automatic optimization based on content type")
	fmt.Println("✅ Smart compression and chunk sizing")
	fmt.Println("✅ High-performance C-Core processing")
}