
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

type TestResult struct {
	Name        string  `json:"name"`
	Status      string  `json:"status"`
	Duration    int64   `json:"duration_ms"`
	Throughput  float64 `json:"throughput_mbps"`
	Error       string  `json:"error,omitempty"`
}

type TestSuite struct {
	Results []TestResult `json:"results"`
	Summary struct {
		Total   int     `json:"total"`
		Passed  int     `json:"passed"`
		Failed  int     `json:"failed"`
		Runtime int64   `json:"runtime_ms"`
	} `json:"summary"`
}

func main() {
	fmt.Println("🧪 Red Giant Protocol - Comprehensive Test Suite")
	fmt.Println("═══════════════════════════════════════════════════")
	
	suite := &TestSuite{}
	startTime := time.Now()
	
	// Wait for server to be ready
	fmt.Println("⏳ Waiting for server to be ready...")
	if !waitForServer("http://localhost:8080", 30) {
		fmt.Println("❌ Server not responding, please start it first")
		fmt.Println("💡 Run: go run red_giant_test_server.go")
		return
	}
	fmt.Println("✅ Server is ready")
	
	// Test 1: Health Check
	result := testHealthCheck()
	suite.Results = append(suite.Results, result)
	
	// Test 2: Small File Upload
	result = testSmallUpload()
	suite.Results = append(suite.Results, result)
	
	// Test 3: Large File Upload
	result = testLargeUpload()
	suite.Results = append(suite.Results, result)
	
	// Test 4: JSON Processing
	result = testJSONProcessing()
	suite.Results = append(suite.Results, result)
	
	// Test 5: Binary Data
	result = testBinaryData()
	suite.Results = append(suite.Results, result)
	
	// Test 6: Concurrent Uploads
	result = testConcurrentUploads()
	suite.Results = append(suite.Results, result)
	
	// Test 7: Metrics Endpoint
	result = testMetrics()
	suite.Results = append(suite.Results, result)
	
	// Test 8: Performance Validation
	result = testPerformance()
	suite.Results = append(suite.Results, result)
	
	// Calculate summary
	suite.Summary.Total = len(suite.Results)
	suite.Summary.Runtime = time.Since(startTime).Milliseconds()
	for _, r := range suite.Results {
		if r.Status == "PASS" {
			suite.Summary.Passed++
		} else {
			suite.Summary.Failed++
		}
	}
	
	// Display results
	displayResults(suite)
	
	// Save results to file
	saveResults(suite)
}

func waitForServer(url string, timeoutSec int) bool {
	for i := 0; i < timeoutSec; i++ {
		resp, err := http.Get(url + "/health")
		if err == nil && resp.StatusCode == 200 {
			resp.Body.Close()
			return true
		}
		if resp != nil {
			resp.Body.Close()
		}
		time.Sleep(1 * time.Second)
	}
	return false
}

func testHealthCheck() TestResult {
	start := time.Now()
	
	resp, err := http.Get("http://localhost:8080/health")
	if err != nil {
		return TestResult{
			Name:     "Health Check",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != 200 {
		return TestResult{
			Name:     "Health Check",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    fmt.Sprintf("Status code: %d", resp.StatusCode),
		}
	}
	
	return TestResult{
		Name:     "Health Check",
		Status:   "PASS",
		Duration: time.Since(start).Milliseconds(),
	}
}

func testSmallUpload() TestResult {
	start := time.Now()
	data := "Red Giant Protocol Test Data - Small File"
	
	resp, err := uploadData(data, "small_test.txt")
	if err != nil {
		return TestResult{
			Name:     "Small File Upload",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	
	throughput := float64(len(data)) / time.Since(start).Seconds() / (1024 * 1024)
	
	return TestResult{
		Name:       "Small File Upload",
		Status:     "PASS",
		Duration:   time.Since(start).Milliseconds(),
		Throughput: throughput,
	}
}

func testLargeUpload() TestResult {
	start := time.Now()
	data := strings.Repeat("Red Giant Protocol Large Test Data. ", 10000) // ~370KB
	
	resp, err := uploadData(data, "large_test.txt")
	if err != nil {
		return TestResult{
			Name:     "Large File Upload",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	
	throughput := float64(len(data)) / time.Since(start).Seconds() / (1024 * 1024)
	
	return TestResult{
		Name:       "Large File Upload",
		Status:     "PASS",
		Duration:   time.Since(start).Milliseconds(),
		Throughput: throughput,
	}
}

func testJSONProcessing() TestResult {
	start := time.Now()
	jsonData := `{
		"protocol": "Red Giant",
		"version": "2.0.0",
		"performance": {
			"throughput": 500,
			"latency": 1.2,
			"adaptive": true
		},
		"features": ["exposure-based", "c-core", "multi-format", "adaptive"]
	}`
	
	resp, err := uploadDataWithContentType(jsonData, "test.json", "application/json")
	if err != nil {
		return TestResult{
			Name:     "JSON Processing",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	
	throughput := float64(len(jsonData)) / time.Since(start).Seconds() / (1024 * 1024)
	
	return TestResult{
		Name:       "JSON Processing",
		Status:     "PASS",
		Duration:   time.Since(start).Milliseconds(),
		Throughput: throughput,
	}
}

func testBinaryData() TestResult {
	start := time.Now()
	
	// Create binary data
	binaryData := make([]byte, 50000) // 50KB
	for i := range binaryData {
		binaryData[i] = byte(i % 256)
	}
	
	resp, err := uploadDataWithContentType(string(binaryData), "binary_test.bin", "application/octet-stream")
	if err != nil {
		return TestResult{
			Name:     "Binary Data Processing",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	
	throughput := float64(len(binaryData)) / time.Since(start).Seconds() / (1024 * 1024)
	
	return TestResult{
		Name:       "Binary Data Processing",
		Status:     "PASS",
		Duration:   time.Since(start).Milliseconds(),
		Throughput: throughput,
	}
}

func testConcurrentUploads() TestResult {
	start := time.Now()
	
	// Test 5 concurrent uploads
	concurrency := 5
	results := make(chan error, concurrency)
	
	for i := 0; i < concurrency; i++ {
		go func(id int) {
			data := fmt.Sprintf("Concurrent test data from goroutine %d. %s", id, strings.Repeat("X", 1000))
			_, err := uploadData(data, fmt.Sprintf("concurrent_%d.txt", id))
			results <- err
		}(i)
	}
	
	// Wait for all to complete
	var failed int
	for i := 0; i < concurrency; i++ {
		if err := <-results; err != nil {
			failed++
		}
	}
	
	status := "PASS"
	errorMsg := ""
	if failed > 0 {
		status = "FAIL"
		errorMsg = fmt.Sprintf("%d out of %d uploads failed", failed, concurrency)
	}
	
	return TestResult{
		Name:     "Concurrent Uploads",
		Status:   status,
		Duration: time.Since(start).Milliseconds(),
		Error:    errorMsg,
	}
}

func testMetrics() TestResult {
	start := time.Now()
	
	resp, err := http.Get("http://localhost:8080/metrics")
	if err != nil {
		return TestResult{
			Name:     "Metrics Endpoint",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	defer resp.Body.Close()
	
	var metrics map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&metrics); err != nil {
		return TestResult{
			Name:     "Metrics Endpoint",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    "Failed to decode metrics JSON",
		}
	}
	
	// Check if key metrics exist
	requiredMetrics := []string{"total_requests", "total_bytes", "total_chunks", "uptime_seconds"}
	for _, metric := range requiredMetrics {
		if _, exists := metrics[metric]; !exists {
			return TestResult{
				Name:     "Metrics Endpoint",
				Status:   "FAIL",
				Duration: time.Since(start).Milliseconds(),
				Error:    fmt.Sprintf("Missing metric: %s", metric),
			}
		}
	}
	
	return TestResult{
		Name:     "Metrics Endpoint",
		Status:   "PASS",
		Duration: time.Since(start).Milliseconds(),
	}
}

func testPerformance() TestResult {
	start := time.Now()
	
	// Upload a 1MB file and measure throughput
	data := strings.Repeat("Performance test data for Red Giant Protocol. ", 20000) // ~1MB
	
	uploadStart := time.Now()
	resp, err := uploadData(data, "performance_test.txt")
	uploadTime := time.Since(uploadStart)
	
	if err != nil {
		return TestResult{
			Name:     "Performance Validation",
			Status:   "FAIL",
			Duration: time.Since(start).Milliseconds(),
			Error:    err.Error(),
		}
	}
	
	throughput := float64(len(data)) / uploadTime.Seconds() / (1024 * 1024)
	
	// Validate performance criteria
	status := "PASS"
	errorMsg := ""
	
	if throughput < 50.0 { // Expect at least 50 MB/s for this test
		status = "FAIL"
		errorMsg = fmt.Sprintf("Throughput %.2f MB/s below threshold", throughput)
	}
	
	if uploadTime.Milliseconds() > 1000 { // Should complete within 1 second
		status = "FAIL"
		errorMsg = fmt.Sprintf("Upload took %d ms, too slow", uploadTime.Milliseconds())
	}
	
	return TestResult{
		Name:       "Performance Validation",
		Status:     status,
		Duration:   time.Since(start).Milliseconds(),
		Throughput: throughput,
		Error:      errorMsg,
	}
}

func uploadData(data, filename string) (map[string]interface{}, error) {
	return uploadDataWithContentType(data, filename, "text/plain")
}

func uploadDataWithContentType(data, filename, contentType string) (map[string]interface{}, error) {
	req, err := http.NewRequest("POST", "http://localhost:8080/upload", strings.NewReader(data))
	if err != nil {
		return nil, err
	}
	
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-File-Name", filename)
	
	client := &http.Client{Timeout: 30 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(body))
	}
	
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, err
	}
	
	return result, nil
}

func displayResults(suite *TestSuite) {
	fmt.Println("\n📊 Test Results Summary")
	fmt.Println("═══════════════════════════════════════")
	fmt.Printf("Total Tests: %d | Passed: %d | Failed: %d | Runtime: %d ms\n",
		suite.Summary.Total, suite.Summary.Passed, suite.Summary.Failed, suite.Summary.Runtime)
	fmt.Println("═══════════════════════════════════════")
	
	for _, result := range suite.Results {
		status := "✅"
		if result.Status == "FAIL" {
			status = "❌"
		}
		
		throughputStr := ""
		if result.Throughput > 0 {
			throughputStr = fmt.Sprintf(" | %.2f MB/s", result.Throughput)
		}
		
		errorStr := ""
		if result.Error != "" {
			errorStr = fmt.Sprintf(" | Error: %s", result.Error)
		}
		
		fmt.Printf("%s %-25s | %4d ms%s%s\n",
			status, result.Name, result.Duration, throughputStr, errorStr)
	}
	
	fmt.Println("═══════════════════════════════════════")
	
	if suite.Summary.Failed == 0 {
		fmt.Println("🎉 All tests passed! Red Giant Protocol is ready for testing.")
	} else {
		fmt.Printf("⚠️  %d test(s) failed. Please review and fix issues.\n", suite.Summary.Failed)
	}
}

func saveResults(suite *TestSuite) {
	data, _ := json.MarshalIndent(suite, "", "  ")
	os.WriteFile("test_results.json", data, 0644)
	fmt.Println("\n💾 Results saved to test_results.json")
}
