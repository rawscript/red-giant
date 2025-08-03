// Red Giant Protocol - P2P File Sharing Example
// Demonstrates how to build a decentralized file sharing network
package main

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"
	"time"

	"../go" // Import Red Giant SDK
)

func main() {
	if len(os.Args) < 2 {
		showUsage()
		return
	}

	// Connect to Red Giant server (deployed anywhere!)
	serverURL := os.Getenv("RED_GIANT_SERVER")
	if serverURL == "" {
		serverURL = "http://localhost:8080"
	}

	client := redgiant.NewClient(serverURL)
	client.SetPeerID(fmt.Sprintf("p2p_peer_%d", time.Now().Unix()))

	fmt.Printf("🌐 Connected to Red Giant P2P Network: %s\n", serverURL)
	fmt.Printf("🆔 Peer ID: %s\n\n", client.PeerID)

	command := os.Args[1]

	switch command {
	case "upload":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go upload <file-path>")
			return
		}
		uploadFile(client, os.Args[2])

	case "download":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run main.go download <file-id> <output-path>")
			return
		}
		downloadFile(client, os.Args[2], os.Args[3])

	case "list":
		listFiles(client)

	case "search":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go search <pattern>")
			return
		}
		searchFiles(client, os.Args[2])

	case "share":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go share <directory>")
			return
		}
		shareDirectory(client, os.Args[2])

	case "sync":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go sync <local-directory>")
			return
		}
		syncDirectory(client, os.Args[2])

	case "stats":
		showNetworkStats(client)

	default:
		fmt.Printf("Unknown command: %s\n", command)
		showUsage()
	}
}

func showUsage() {
	fmt.Println("🚀 Red Giant P2P File Sharing")
	fmt.Println("Usage: go run main.go <command> [args...]")
	fmt.Println("")
	fmt.Println("Commands:")
	fmt.Println("  upload <file>           Upload file to P2P network")
	fmt.Println("  download <id> <output>  Download file from network")
	fmt.Println("  list                    List all files in network")
	fmt.Println("  search <pattern>        Search files by name")
	fmt.Println("  share <directory>       Share entire directory")
	fmt.Println("  sync <directory>        Sync local directory with network")
	fmt.Println("  stats                   Show network statistics")
	fmt.Println("")
	fmt.Println("Environment:")
	fmt.Println("  RED_GIANT_SERVER        Server URL (default: http://localhost:8080)")
	fmt.Println("")
	fmt.Println("Examples:")
	fmt.Println("  go run main.go upload document.pdf")
	fmt.Println("  go run main.go download abc123 downloaded.pdf")
	fmt.Println("  go run main.go search '*.pdf'")
	fmt.Println("  go run main.go share ./my_documents")
}

func uploadFile(client *redgiant.Client, filePath string) {
	fmt.Printf("📤 Uploading file: %s\n", filePath)

	start := time.Now()
	result, err := client.UploadFile(filePath)
	if err != nil {
		log.Fatalf("❌ Upload failed: %v", err)
	}

	uploadTime := time.Since(start)

	fmt.Printf("✅ Upload successful!\n")
	fmt.Printf("   • File ID: %s\n", result.FileID)
	fmt.Printf("   • Size: %d bytes (%.2f MB)\n", result.BytesProcessed, float64(result.BytesProcessed)/(1024*1024))
	fmt.Printf("   • Chunks: %d\n", result.ChunksProcessed)
	fmt.Printf("   • Upload time: %v\n", uploadTime)
	fmt.Printf("   • Server throughput: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("   • Processing time: %d ms\n", result.ProcessingTime)
	fmt.Printf("\n💡 Share this File ID with others: %s\n", result.FileID)
}

func downloadFile(client *redgiant.Client, fileID, outputPath string) {
	fmt.Printf("📥 Downloading file: %s\n", fileID)

	start := time.Now()
	err := client.DownloadFile(fileID, outputPath)
	if err != nil {
		log.Fatalf("❌ Download failed: %v", err)
	}

	downloadTime := time.Since(start)

	// Get file info
	info, err := os.Stat(outputPath)
	if err != nil {
		log.Printf("Warning: Could not get file info: %v", err)
		return
	}

	throughput := float64(info.Size()) / downloadTime.Seconds() / (1024 * 1024)

	fmt.Printf("✅ Download successful!\n")
	fmt.Printf("   • Output: %s\n", outputPath)
	fmt.Printf("   • Size: %d bytes (%.2f MB)\n", info.Size(), float64(info.Size())/(1024*1024))
	fmt.Printf("   • Download time: %v\n", downloadTime)
	fmt.Printf("   • Throughput: %.2f MB/s\n", throughput)
}

func listFiles(client *redgiant.Client) {
	fmt.Printf("📋 Listing files in P2P network...\n\n")

	files, err := client.ListFiles()
	if err != nil {
		log.Fatalf("❌ Failed to list files: %v", err)
	}

	if len(files) == 0 {
		fmt.Println("No files found in the network.")
		return
	}

	fmt.Printf("Found %d files:\n\n", len(files))

	for i, file := range files {
		fmt.Printf("%d. %s\n", i+1, file.Name)
		fmt.Printf("   • ID: %s\n", file.ID)
		fmt.Printf("   • Size: %d bytes (%.2f MB)\n", file.Size, float64(file.Size)/(1024*1024))
		fmt.Printf("   • Peer: %s\n", file.PeerID)
		fmt.Printf("   • Uploaded: %s\n", file.UploadedAt.Format("2006-01-02 15:04:05"))
		fmt.Println()
	}
}

func searchFiles(client *redgiant.Client, pattern string) {
	fmt.Printf("🔍 Searching for files matching: %s\n\n", pattern)

	files, err := client.SearchFiles(pattern)
	if err != nil {
		log.Fatalf("❌ Search failed: %v", err)
	}

	if len(files) == 0 {
		fmt.Printf("No files found matching pattern: %s\n", pattern)
		return
	}

	fmt.Printf("Found %d matching files:\n\n", len(files))

	for i, file := range files {
		fmt.Printf("%d. %s\n", i+1, file.Name)
		fmt.Printf("   • ID: %s\n", file.ID)
		fmt.Printf("   • Size: %.2f MB\n", float64(file.Size)/(1024*1024))
		fmt.Printf("   • Peer: %s\n", file.PeerID)
		fmt.Println()
	}
}

func shareDirectory(client *redgiant.Client, dirPath string) {
	fmt.Printf("📁 Sharing directory: %s\n\n", dirPath)

	var uploadedFiles []string
	var totalSize int64

	err := filepath.Walk(dirPath, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		// Skip directories
		if info.IsDir() {
			return nil
		}

		// Skip hidden files
		if strings.HasPrefix(info.Name(), ".") {
			return nil
		}

		fmt.Printf("📤 Uploading: %s\n", path)

		result, err := client.UploadFile(path)
		if err != nil {
			fmt.Printf("❌ Failed to upload %s: %v\n", path, err)
			return nil // Continue with other files
		}

		uploadedFiles = append(uploadedFiles, result.FileID)
		totalSize += int64(result.BytesProcessed)

		fmt.Printf("   ✅ File ID: %s (%.2f MB/s)\n", result.FileID, result.ThroughputMbps)

		return nil
	})

	if err != nil {
		log.Fatalf("❌ Directory walk failed: %v", err)
	}

	fmt.Printf("\n🎉 Directory sharing completed!\n")
	fmt.Printf("   • Files uploaded: %d\n", len(uploadedFiles))
	fmt.Printf("   • Total size: %.2f MB\n", float64(totalSize)/(1024*1024))
	fmt.Printf("   • File IDs:\n")
	for _, fileID := range uploadedFiles {
		fmt.Printf("     - %s\n", fileID)
	}
}

func syncDirectory(client *redgiant.Client, localDir string) {
	fmt.Printf("🔄 Syncing directory: %s\n\n", localDir)

	// Create directory if it doesn't exist
	if err := os.MkdirAll(localDir, 0755); err != nil {
		log.Fatalf("❌ Failed to create directory: %v", err)
	}

	// Get all files from network
	networkFiles, err := client.ListFiles()
	if err != nil {
		log.Fatalf("❌ Failed to list network files: %v", err)
	}

	// Get local files
	localFiles := make(map[string]bool)
	filepath.Walk(localDir, func(path string, info os.FileInfo, err error) error {
		if err == nil && !info.IsDir() {
			relPath, _ := filepath.Rel(localDir, path)
			localFiles[relPath] = true
		}
		return nil
	})

	var downloaded int
	var totalSize int64

	// Download missing files
	for _, file := range networkFiles {
		localPath := filepath.Join(localDir, file.Name)

		// Check if file already exists
		if _, exists := localFiles[file.Name]; exists {
			fmt.Printf("⏭️  Skipping existing file: %s\n", file.Name)
			continue
		}

		fmt.Printf("📥 Downloading: %s\n", file.Name)

		err := client.DownloadFile(file.ID, localPath)
		if err != nil {
			fmt.Printf("❌ Failed to download %s: %v\n", file.Name, err)
			continue
		}

		downloaded++
		totalSize += file.Size

		fmt.Printf("   ✅ Downloaded: %s\n", localPath)
	}

	fmt.Printf("\n🎉 Directory sync completed!\n")
	fmt.Printf("   • Files downloaded: %d\n", downloaded)
	fmt.Printf("   • Total size: %.2f MB\n", float64(totalSize)/(1024*1024))
}

func showNetworkStats(client *redgiant.Client) {
	fmt.Printf("📊 Red Giant Network Statistics\n\n")

	stats, err := client.GetNetworkStats()
	if err != nil {
		log.Fatalf("❌ Failed to get stats: %v", err)
	}

	fmt.Printf("Network Performance:\n")
	fmt.Printf("   • Total Requests: %d\n", stats.TotalRequests)
	fmt.Printf("   • Total Data: %.2f MB\n", float64(stats.TotalBytes)/(1024*1024))
	fmt.Printf("   • Total Chunks: %d\n", stats.TotalChunks)
	fmt.Printf("   • Average Latency: %d ms\n", stats.AverageLatency)
	fmt.Printf("   • Error Count: %d\n", stats.ErrorCount)
	fmt.Printf("   • Uptime: %d seconds\n", stats.UptimeSeconds)
	fmt.Printf("   • Current Throughput: %.2f MB/s\n", stats.ThroughputMbps)

	// Calculate success rate
	if stats.TotalRequests > 0 {
		successRate := float64(stats.TotalRequests-stats.ErrorCount) / float64(stats.TotalRequests) * 100
		fmt.Printf("   • Success Rate: %.1f%%\n", successRate)
	}

	fmt.Printf("\nServer Health: ")
	if err := client.HealthCheck(); err != nil {
		fmt.Printf("❌ Unhealthy (%v)\n", err)
	} else {
		fmt.Printf("✅ Healthy\n")
	}
}