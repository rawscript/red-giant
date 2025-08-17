// Red Giant Protocol - SDK Test Suite
package main

import (
	"fmt"
	"log"
)

func main() {
	fmt.Println("🧪 Red Giant SDK Test Suite")
	fmt.Println("============================")

	// Test 1: Client Creation
	fmt.Println("\n1. Testing client creation...")
	client := redgiant.NewClient("http://localhost:5000")
	if client == nil {
		log.Fatal("❌ Failed to create client")
	}
	fmt.Printf("✅ Client created with peer ID: %s\n", client.PeerID)

	// Test 2: Health Check
	fmt.Println("\n2. Testing health check...")
	err := client.HealthCheck()
	if err != nil {
		fmt.Printf("⚠️  Health check failed (server may not be running): %v\n", err)
	} else {
		fmt.Println("✅ Health check passed")
	}

	// Test 3: Upload Test Data
	fmt.Println("\n3. Testing data upload...")
	testData := []byte("Hello Red Giant SDK! This is a test message from the Go SDK.")
	result, err := client.UploadData(testData, "sdk_test.txt")
	if err != nil {
		fmt.Printf("⚠️  Upload failed (server may not be running): %v\n", err)
	} else {
		fmt.Printf("✅ Upload successful! File ID: %s\n", result.FileID)
		fmt.Printf("   Throughput: %.2f MB/s\n", result.ThroughputMbps)
		fmt.Printf("   Processing time: %d ms\n", result.ProcessingTime)
	}

	// Test 4: Network Stats
	fmt.Println("\n4. Testing network stats...")
	stats, err := client.GetNetworkStats()
	if err != nil {
		fmt.Printf("⚠️  Stats failed (server may not be running): %v\n", err)
	} else {
		fmt.Printf("✅ Stats retrieved:\n")
		fmt.Printf("   Total requests: %d\n", stats.TotalRequests)
		fmt.Printf("   Total bytes: %d\n", stats.TotalBytes)
		fmt.Printf("   Uptime: %d seconds\n", stats.UptimeSeconds)
	}

	fmt.Println("\n🎉 SDK test completed!")
	fmt.Println("\nTo run full tests:")
	fmt.Println("1. Start server: go run red_giant_test_server.go")
	fmt.Println("2. Run this test again")
}
