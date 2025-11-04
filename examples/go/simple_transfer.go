// Simple RGTP file transfer example in Go
//
// This example demonstrates basic file transfer using RGTP Go bindings.
// It shows both exposing (serving) and pulling (downloading) files.

package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"path/filepath"
	"strconv"
	"syscall"
	"time"

	"github.com/red-giant-protocol/rgtp-go"
)

func main() {
	fmt.Println("RGTP Go Example")
	fmt.Println("===============")
	fmt.Printf("RGTP Version: %s\n\n", rgtp.Version())

	if len(os.Args) < 2 {
		printUsage()
		os.Exit(1)
	}

	command := os.Args[1]

	switch command {
	case "expose":
		if len(os.Args) != 3 {
			fmt.Println("Usage: expose <filename>")
			os.Exit(1)
		}
		filename := os.Args[2]
		exposeFile(filename)

	case "pull":
		if len(os.Args) < 4 || len(os.Args) > 5 {
			fmt.Println("Usage: pull <host> <port> [output_file]")
			os.Exit(1)
		}
		host := os.Args[2]
		port, err := strconv.ParseUint(os.Args[3], 10, 16)
		if err != nil {
			log.Fatalf("Invalid port: %v", err)
		}
		outputFile := "rgtp_download.bin"
		if len(os.Args) == 5 {
			outputFile = os.Args[4]
		}
		pullFile(host, uint16(port), outputFile)

	default:
		fmt.Printf("Unknown command: %s\n", command)
		printUsage()
		os.Exit(1)
	}
}

func printUsage() {
	fmt.Println("Usage:")
	fmt.Printf("  %s expose <filename>\n", os.Args[0])
	fmt.Printf("  %s pull <host> <port> [output_file]\n", os.Args[0])
	fmt.Println()
	fmt.Println("Examples:")
	fmt.Printf("  %s expose document.pdf\n", os.Args[0])
	fmt.Printf("  %s pull localhost 9999\n", os.Args[0])
	fmt.Printf("  %s pull 192.168.1.100 9999 downloaded.pdf\n", os.Args[0])
}

func exposeFile(filename string) {
	fmt.Printf("📤 Exposing file: %s\n", filename)

	// Check if file exists
	if _, err := os.Stat(filename); os.IsNotExist(err) {
		log.Fatalf("File not found: %s", filename)
	}

	// Create configuration optimized for LAN
	config := rgtp.LANConfig()
	config.Port = 9999
	config.ProgressCallback = func(bytesTransferred, totalBytes uint64) {
		if totalBytes > 0 {
			percent := float64(bytesTransferred) / float64(totalBytes) * 100.0
			fmt.Printf("\rProgress: %.1f%% (%s/%s)", 
				percent, 
				formatBytes(bytesTransferred), 
				formatBytes(totalBytes))
		}
	}
	config.ErrorCallback = func(errorCode int, errorMessage string) {
		fmt.Printf("\nError %d: %s\n", errorCode, errorMessage)
	}

	fmt.Printf("Configuration:\n")
	fmt.Printf("  • Port: %d\n", config.Port)
	fmt.Printf("  • Chunk size: %s\n", formatBytes(uint64(config.ChunkSize)))
	fmt.Printf("  • Adaptive mode: %v\n", config.AdaptiveMode)
	fmt.Printf("  • Exposure rate: %d chunks/sec\n", config.ExposureRate)

	// Create session
	session, err := rgtp.NewSession(config)
	if err != nil {
		log.Fatalf("Failed to create session: %v", err)
	}
	defer session.Close()

	// Set up signal handling for graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Start exposure in a goroutine
	done := make(chan error, 1)
	go func() {
		err := session.ExposeFile(filename)
		if err != nil {
			done <- err
			return
		}

		fmt.Printf("\n✅ File exposed successfully on port %d\n", config.Port)
		fmt.Println("Clients can now pull from: <this_host>:9999")
		fmt.Println("Press Ctrl+C to stop...\n")

		err = session.WaitComplete()
		done <- err
	}()

	// Monitor progress and handle shutdown
	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			stats, err := session.GetStats()
			if err == nil && stats.ChunksTransferred > 0 {
				fmt.Printf("\rStats: %s transferred, %.1f MB/s, %.1f%% complete",
					formatBytes(stats.BytesTransferred),
					stats.ThroughputMbps,
					stats.CompletionPercent)
			}

		case <-sigChan:
			fmt.Println("\n🛑 Shutting down...")
			session.Cancel()
			return

		case err := <-done:
			if err != nil {
				fmt.Printf("\n❌ Exposure failed: %v\n", err)
				os.Exit(1)
			}
			fmt.Println("\n✅ Exposure completed successfully!")
			
			// Print final statistics
			if stats, err := session.GetStats(); err == nil {
				printFinalStats(stats, "Exposure")
			}
			return
		}
	}
}

func pullFile(host string, port uint16, outputFile string) {
	fmt.Printf("📥 Pulling from %s:%d -> %s\n", host, port, outputFile)

	// Create configuration optimized for WAN
	config := rgtp.WANConfig()
	config.ProgressCallback = func(bytesTransferred, totalBytes uint64) {
		if totalBytes > 0 {
			percent := float64(bytesTransferred) / float64(totalBytes) * 100.0
			
			// Simple progress bar
			barWidth := 50
			filled := int(percent / 100.0 * float64(barWidth))
			
			fmt.Printf("\r[")
			for i := 0; i < barWidth; i++ {
				if i < filled {
					fmt.Print("=")
				} else if i == filled {
					fmt.Print(">")
				} else {
					fmt.Print(" ")
				}
			}
			fmt.Printf("] %.1f%% (%s/%s)", 
				percent, 
				formatBytes(bytesTransferred), 
				formatBytes(totalBytes))
		}
	}
	config.ErrorCallback = func(errorCode int, errorMessage string) {
		fmt.Printf("\nError %d: %s\n", errorCode, errorMessage)
	}

	fmt.Printf("Configuration:\n")
	fmt.Printf("  • Timeout: %v\n", config.Timeout)
	fmt.Printf("  • Chunk size: %s\n", formatBytes(uint64(config.ChunkSize)))
	fmt.Printf("  • Adaptive mode: %v\n", config.AdaptiveMode)

	// Create client
	client, err := rgtp.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	fmt.Println("Starting pull operation...")

	// Record start time
	startTime := time.Now()

	// Pull the file
	err = client.PullToFile(host, port, outputFile)
	
	// Calculate elapsed time
	elapsed := time.Since(startTime)

	if err != nil {
		fmt.Printf("\n❌ Pull failed: %v\n", err)
		
		// Try to get partial statistics
		if stats, err := client.GetStats(); err == nil && stats.BytesTransferred > 0 {
			fmt.Printf("   • Partial data received: %s (%.1f%%)\n",
				formatBytes(stats.BytesTransferred),
				stats.CompletionPercent)
		}
		os.Exit(1)
	}

	fmt.Printf("\n✅ Pull completed successfully!\n")
	fmt.Printf("   • File saved as: %s\n", outputFile)
	fmt.Printf("   • Total time: %v\n", elapsed)

	// Get final statistics
	if stats, err := client.GetStats(); err == nil {
		printFinalStats(stats, "Pull")
	}
}

func printFinalStats(stats *rgtp.Stats, operation string) {
	fmt.Printf("\n📊 %s Statistics:\n", operation)
	fmt.Printf("   • File size: %s\n", formatBytes(stats.TotalBytes))
	fmt.Printf("   • Duration: %v\n", stats.Elapsed)
	fmt.Printf("   • Average throughput: %.2f MB/s\n", stats.AvgThroughputMbps)
	fmt.Printf("   • Peak throughput: %.2f MB/s\n", stats.ThroughputMbps)
	fmt.Printf("   • Chunks transferred: %d/%d\n", stats.ChunksTransferred, stats.TotalChunks)
	fmt.Printf("   • Retransmissions: %d\n", stats.Retransmissions)
	fmt.Printf("   • Efficiency: %.1f%%\n", stats.EfficiencyPercent())
	
	if stats.TotalBytes > 0 && stats.Elapsed > 0 {
		effectiveSpeed := float64(stats.TotalBytes) / (1024 * 1024) / stats.Elapsed.Seconds()
		fmt.Printf("   • Effective speed: %.2f MB/s\n", effectiveSpeed)
	}
}

func formatBytes(bytes uint64) string {
	const unit = 1024
	if bytes < unit {
		return fmt.Sprintf("%d B", bytes)
	}
	div, exp := uint64(unit), 0
	for n := bytes / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.1f %cB", float64(bytes)/float64(div), "KMGTPE"[exp])
}