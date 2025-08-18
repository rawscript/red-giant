//go:build netcli
// +build netcli

// Red Giant Protocol - Network Discovery and Management
package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"
)

type NetworkPeer struct {
	ID          string    `json:"id"`
	Name        string    `json:"name"`
	Address     string    `json:"address"`
	LastSeen    time.Time `json:"last_seen"`
	FilesShared int       `json:"files_shared"`
	BytesShared int64     `json:"bytes_shared"`
	Status      string    `json:"status"` // "online", "offline", "busy"
}

type NetworkStats struct {
	TotalPeers        int     `json:"total_peers"`
	OnlinePeers       int     `json:"online_peers"`
	TotalFiles        int     `json:"total_files"`
	TotalBytes        int64   `json:"total_bytes"`
	NetworkUptime     int64   `json:"network_uptime"`
	AverageThroughput float64 `json:"average_throughput_mbps"`
}

type RedGiantNetwork struct {
	ServerURL string
	Client    *http.Client
}

func NewRedGiantNetwork(serverURL string) *RedGiantNetwork {
	return &RedGiantNetwork{
		ServerURL: serverURL,
		Client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// Get network statistics
func (n *RedGiantNetwork) GetNetworkStats() (*NetworkStats, error) {
	resp, err := n.Client.Get(n.ServerURL + "/metrics")
	if err != nil {
		return nil, fmt.Errorf("failed to get network stats: %w", err)
	}
	defer resp.Body.Close()

	var metrics map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&metrics); err != nil {
		return nil, fmt.Errorf("failed to parse metrics: %w", err)
	}

	// Get file count
	filesResp, err := n.Client.Get(n.ServerURL + "/files")
	if err != nil {
		return nil, fmt.Errorf("failed to get files: %w", err)
	}
	defer filesResp.Body.Close()

	var filesData struct {
		Count int `json:"count"`
		Files []struct {
			Size int64 `json:"size"`
		} `json:"files"`
	}
	json.NewDecoder(filesResp.Body).Decode(&filesData)

	var totalBytes int64
	for _, file := range filesData.Files {
		totalBytes += file.Size
	}

	stats := &NetworkStats{
		TotalPeers:        1, // At least this server
		OnlinePeers:       1,
		TotalFiles:        filesData.Count,
		TotalBytes:        totalBytes,
		NetworkUptime:     int64(metrics["uptime_seconds"].(float64)),
		AverageThroughput: metrics["throughput_mbps"].(float64),
	}

	return stats, nil
}

// Discover available files in the network
func (n *RedGiantNetwork) DiscoverFiles(pattern string) error {
	url := n.ServerURL + "/search?q=" + pattern
	if pattern == "" {
		url = n.ServerURL + "/files"
	}

	resp, err := n.Client.Get(url)
	if err != nil {
		return fmt.Errorf("failed to discover files: %w", err)
	}
	defer resp.Body.Close()

	var result struct {
		Files []struct {
			ID         string    `json:"id"`
			Name       string    `json:"name"`
			Size       int64     `json:"size"`
			PeerID     string    `json:"peer_id"`
			UploadedAt time.Time `json:"uploaded_at"`
		} `json:"files"`
		Count int `json:"count"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	fmt.Printf("🔍 Network File Discovery Results (%d files):\n", result.Count)
	fmt.Printf("═══════════════════════════════════════════════\n")

	if result.Count == 0 {
		fmt.Printf("No files found in the network.\n")
		return nil
	}

	for i, file := range result.Files {
		fmt.Printf("[%d] 📁 %s\n", i+1, file.Name)
		fmt.Printf("    ID: %s\n", file.ID)
		fmt.Printf("    Size: %d bytes (%.2f MB)\n", file.Size, float64(file.Size)/(1024*1024))
		fmt.Printf("    Shared by: %s\n", file.PeerID)
		fmt.Printf("    Uploaded: %s\n", file.UploadedAt.Format("2006-01-02 15:04:05"))
		fmt.Printf("    Download: go run red_giant_peer.go download %s\n", file.ID)
		fmt.Println()
	}

	return nil
}

// Test network connectivity and performance
func (n *RedGiantNetwork) TestNetwork() error {
	fmt.Printf("🧪 Red Giant Network Performance Test\n")
	fmt.Printf("═══════════════════════════════════════════\n")

	// Test 1: Health check
	fmt.Printf("1. Health Check... ")
	start := time.Now()
	resp, err := n.Client.Get(n.ServerURL + "/health")
	if err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
		return err
	}
	resp.Body.Close()
	healthTime := time.Since(start)
	fmt.Printf("✅ OK (%v)\n", healthTime)

	// Test 2: Upload test
	fmt.Printf("2. Upload Test... ")
	testData := strings.Repeat("Red Giant Network Test Data! ", 1000) // ~30KB
	start = time.Now()

	uploadResp, err := http.Post(n.ServerURL+"/process", "application/octet-stream", strings.NewReader(testData))
	if err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
		return err
	}
	uploadResp.Body.Close()
	uploadTime := time.Since(start)
	uploadThroughput := float64(len(testData)) / uploadTime.Seconds() / (1024 * 1024)
	fmt.Printf("✅ OK (%v, %.2f MB/s)\n", uploadTime, uploadThroughput)

	// Test 3: Network statistics
	fmt.Printf("3. Network Stats... ")
	stats, err := n.GetNetworkStats()
	if err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
		return err
	}
	fmt.Printf("✅ OK\n")

	// Display results
	fmt.Printf("\n📊 Network Performance Summary:\n")
	fmt.Printf("   • Health Check Latency: %v\n", healthTime)
	fmt.Printf("   • Upload Throughput: %.2f MB/s\n", uploadThroughput)
	fmt.Printf("   • Network Uptime: %d seconds\n", stats.NetworkUptime)
	fmt.Printf("   • Files in Network: %d\n", stats.TotalFiles)
	fmt.Printf("   • Total Data: %.2f MB\n", float64(stats.TotalBytes)/(1024*1024))
	fmt.Printf("   • Average Throughput: %.2f MB/s\n", stats.AverageThroughput)

	return nil
}

// Monitor network activity
func (n *RedGiantNetwork) MonitorNetwork(duration time.Duration) error {
	fmt.Printf("📡 Monitoring Red Giant Network for %v\n", duration)
	fmt.Printf("═══════════════════════════════════════════\n")

	startTime := time.Now()
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			stats, err := n.GetNetworkStats()
			if err != nil {
				fmt.Printf("❌ Failed to get stats: %v\n", err)
				continue
			}

			elapsed := time.Since(startTime)
			fmt.Printf("[%s] Files: %d, Data: %.2f MB, Throughput: %.2f MB/s\n",
				elapsed.Round(time.Second),
				stats.TotalFiles,
				float64(stats.TotalBytes)/(1024*1024),
				stats.AverageThroughput)

		case <-time.After(duration):
			fmt.Printf("\n✅ Network monitoring completed\n")
			return nil
		}
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("🚀 Red Giant Protocol - Network Management")
		fmt.Println("Usage:")
		fmt.Println("  go run red_giant_network.go discover [pattern]")
		fmt.Println("  go run red_giant_network.go stats")
		fmt.Println("  go run red_giant_network.go test")
		fmt.Println("  go run red_giant_network.go monitor [duration]")
		fmt.Println("")
		fmt.Println("Examples:")
		fmt.Println("  go run red_giant_network.go discover")
		fmt.Println("  go run red_giant_network.go discover '*.pdf'")
		fmt.Println("  go run red_giant_network.go stats")
		fmt.Println("  go run red_giant_network.go test")
		fmt.Println("  go run red_giant_network.go monitor 30s")
		return
	}

	serverURL := "http://localhost:8080"
	if env := os.Getenv("RED_GIANT_SERVER"); env != "" {
		serverURL = env
	}

	network := NewRedGiantNetwork(serverURL)
	command := os.Args[1]

	switch command {
	case "discover":
		pattern := ""
		if len(os.Args) > 2 {
			pattern = os.Args[2]
		}
		if err := network.DiscoverFiles(pattern); err != nil {
			fmt.Printf("❌ Discovery failed: %v\n", err)
		}

	case "stats":
		stats, err := network.GetNetworkStats()
		if err != nil {
			fmt.Printf("❌ Failed to get stats: %v\n", err)
			return
		}
		fmt.Printf("📊 Red Giant Network Statistics\n")
		fmt.Printf("═══════════════════════════════\n")
		fmt.Printf("• Online Peers: %d\n", stats.OnlinePeers)
		fmt.Printf("• Total Files: %d\n", stats.TotalFiles)
		fmt.Printf("• Total Data: %.2f MB\n", float64(stats.TotalBytes)/(1024*1024))
		fmt.Printf("• Network Uptime: %d seconds\n", stats.NetworkUptime)
		fmt.Printf("• Average Throughput: %.2f MB/s\n", stats.AverageThroughput)

	case "test":
		if err := network.TestNetwork(); err != nil {
			fmt.Printf("❌ Network test failed: %v\n", err)
		}

	case "monitor":
		duration := 30 * time.Second
		if len(os.Args) > 2 {
			if d, err := time.ParseDuration(os.Args[2]); err == nil {
				duration = d
			}
		}
		if err := network.MonitorNetwork(duration); err != nil {
			fmt.Printf("❌ Monitoring failed: %v\n", err)
		}

	default:
		fmt.Printf("❌ Unknown command: %s\n", command)
		fmt.Println("Use 'discover', 'stats', 'test', or 'monitor'")
	}
}
