package main

import (
	"fmt"
	"log"

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

	stats, err := rgtp.SendFile("test_file.txt", sendConfig)
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

	stats, err = rgtp.ReceiveFile("localhost", 9999, "output.txt", receiveConfig)
	if err != nil {
		log.Printf("Receive failed (expected without server): %v", err)
	} else {
		fmt.Printf("Receive completed successfully!\n")
		printStats(stats)
	}

	fmt.Println("\nGo bindings structure is ready!")
	fmt.Println("Next steps: Build the C library and enable full CGO integration")
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
