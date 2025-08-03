// Red Giant Protocol - File Sender Utility
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

type SendResult struct {
	Status           string  `json:"status"`
	BytesProcessed   int     `json:"bytes_processed"`
	ChunksProcessed  int     `json:"chunks_processed"`
	ProcessingTimeMs int64   `json:"processing_time_ms"`
	ThroughputMbps   float64 `json:"throughput_mbps"`
	Timestamp        int64   `json:"timestamp"`
}

func sendFile(serverURL, filename string) error {
	// Read file
	data, err := os.ReadFile(filename)
	if err != nil {
		return fmt.Errorf("failed to read file: %w", err)
	}

	fileInfo, _ := os.Stat(filename)
	fmt.Printf("📁 Sending file: %s\n", filename)
	fmt.Printf("📊 File size: %d bytes (%.2f MB)\n", len(data), float64(len(data))/(1024*1024))

	// Send to Red Giant server
	start := time.Now()
	resp, err := http.Post(serverURL+"/process", "application/octet-stream", bytes.NewReader(data))
	if err != nil {
		return fmt.Errorf("failed to send file: %w", err)
	}
	defer resp.Body.Close()

	totalTime := time.Since(start)

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	// Parse response
	var result SendResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	// Display results
	fmt.Printf("✅ File sent successfully!\n")
	fmt.Printf("📊 Results:\n")
	fmt.Printf("   • Bytes processed: %d\n", result.BytesProcessed)
	fmt.Printf("   • Chunks processed: %d\n", result.ChunksProcessed)
	fmt.Printf("   • Server processing time: %d ms\n", result.ProcessingTimeMs)
	fmt.Printf("   • Total transfer time: %v\n", totalTime)
	fmt.Printf("   • Server throughput: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("   • Total throughput: %.2f MB/s\n", float64(len(data))/totalTime.Seconds()/(1024*1024))

	return nil
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("🚀 Red Giant Protocol - File Sender")
		fmt.Println("Usage:")
		fmt.Println("  go run send_file.go <filename>")
		fmt.Println("  go run send_file.go <filename> <server-url>")
		fmt.Println("")
		fmt.Println("Examples:")
		fmt.Println("  go run send_file.go document.pdf")
		fmt.Println("  go run send_file.go large_file.zip http://localhost:8080")
		fmt.Println("  go run send_file.go *.txt  # Send all txt files")
		return
	}

	serverURL := "http://localhost:8080"
	if len(os.Args) > 2 {
		serverURL = os.Args[2]
	}

	filename := os.Args[1]

	// Handle wildcards
	matches, err := filepath.Glob(filename)
	if err != nil {
		fmt.Printf("❌ Invalid pattern: %v\n", err)
		return
	}

	if len(matches) == 0 {
		fmt.Printf("❌ No files found matching: %s\n", filename)
		return
	}

	fmt.Printf("🚀 Red Giant Protocol File Sender\n")
	fmt.Printf("🌐 Server: %s\n", serverURL)
	fmt.Printf("📁 Files to send: %d\n\n", len(matches))

	totalStart := time.Now()
	var totalBytes int64
	successCount := 0

	for i, file := range matches {
		fmt.Printf("[%d/%d] ", i+1, len(matches))
		
		if err := sendFile(serverURL, file); err != nil {
			fmt.Printf("❌ Failed to send %s: %v\n", file, err)
			continue
		}

		// Get file size for total calculation
		if info, err := os.Stat(file); err == nil {
			totalBytes += info.Size()
		}
		successCount++
		fmt.Println()
	}

	totalTime := time.Since(totalStart)

	fmt.Printf("🎯 Summary:\n")
	fmt.Printf("   • Files sent: %d/%d\n", successCount, len(matches))
	fmt.Printf("   • Total bytes: %d (%.2f MB)\n", totalBytes, float64(totalBytes)/(1024*1024))
	fmt.Printf("   • Total time: %v\n", totalTime)
	if totalTime.Seconds() > 0 {
		fmt.Printf("   • Overall throughput: %.2f MB/s\n", float64(totalBytes)/totalTime.Seconds()/(1024*1024))
	}
}