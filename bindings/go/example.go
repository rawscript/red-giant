package main

import (
	"fmt"
	"log"
	"os"
	"time"

	"github.com/redgiant/rgtp-go"
)

func main() {
	// Initialize RGTP
	if err := rgtp.Initialize(); err != nil {
		log.Fatalf("Failed to initialize RGTP: %v", err)
	}
	defer rgtp.Cleanup()

	fmt.Printf("RGTP Version: %s\n", rgtp.Version())

	// Example 1: Send a file
	fmt.Println("\n=== Sending File Example ===")
	sendConfig := rgtp.CreateConfig()
	sendConfig.ChunkSize = 512 * 1024 // 512KB chunks
	sendConfig.ExposureRate = 2000    // 2000 chunks/sec
	
	// Create a test file
	testFilename := "test_file.txt"
	createTestFile(testFilename, 1024*1024) // 1MB test file
	
	stats, err := rgtp.SendFile(testFilename, sendConfig)
	if err != nil {
		log.Printf("Send failed: %v", err)
	} else {
		fmt.Printf("Send completed successfully!\n")
		printStats(stats)
	}

	// Example 2: Receive a file
	fmt.Println("\n=== Receiving File Example ===")
	receiveConfig := rgtp.CreateConfig()
	receiveConfig.ChunkSize = 512 * 1024
	receiveConfig.TimeoutMs = 60000 // 60 seconds
	
	// In a real scenario, you'd connect to a server
	// For demonstration, this will likely fail without a server
	outputFilename := "received_file.txt"
	stats, err = rgtp.ReceiveFile("localhost", 9999, outputFilename, receiveConfig)
	if err != nil {
		log.Printf("Receive failed (expected without server): %v", err)
	} else {
		fmt.Printf("Receive completed successfully!\n")
		printStats(stats)
	}

	// Example 3: Manual session management
	fmt.Println("\n=== Manual Session Management Example ===")
	session, err := rgtp.CreateSession(sendConfig)
	if err != nil {
		log.Fatalf("Failed to create session: %v", err)
	}
	defer session.Destroy()

	fmt.Println("Session created successfully")
	
	// Get initial stats
	stats, err = session.GetStats()
	if err != nil {
		log.Printf("Failed to get stats: %v", err)
	} else {
		fmt.Println("Initial session stats:")
		printStats(stats)
	}

	fmt.Println("Manual session management example completed")

	// Clean up test file
	os.Remove(testFilename)
}

func createTestFile(filename string, size int) {
	file, err := os.Create(filename)
	if err != nil {
		log.Fatalf("Failed to create test file: %v", err)
	}
	defer file.Close()

	// Write pattern data
	data := make([]byte, 1024)
	for i := range data {
		data[i] = byte(i % 256)
	}

	for i := 0; i < size/len(data); i++ {
		file.Write(data)
	}

	fmt.Printf("Created test file: %s (%d bytes)\n", filename, size)
}

func printStats(stats *rgtp.Stats) {
	fmt.Printf("  Bytes Sent: %d\n", stats.BytesSent)
	fmt.Printf("  Bytes Received: %d\n", stats.BytesReceived)
	fmt.Printf("  Chunks Sent: %d\n", stats.ChunksSent)
	fmt.Printf("  Chunks Received: %d\n", stats.ChunksReceived)
	fmt.Printf("  Packet Loss Rate: %.4f\n", stats.PacketLossRate)
	fmt.Printf("  RTT: %d ms\n", stats.RttMs)
	fmt.Printf("  Retransmissions: %d\n", stats.Retransmissions)
	fmt.Printf("  Average Throughput: %.2f Mbps\n", stats.AvgThroughputMbps)
	fmt.Printf("  Completion: %.2f%%\n", stats.CompletionPercent)
	fmt.Printf("  Active Connections: %d\n", stats.ActiveConnections)
}