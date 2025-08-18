// Red Giant Protocol - Peer-to-Peer Client
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

type RedGiantPeer struct {
	ServerURL string
	PeerID    string
	Client    *http.Client
}

type FileInfo struct {
	Name       string    `json:"name"`
	Size       int64     `json:"size"`
	Hash       string    `json:"hash"`
	PeerID     string    `json:"peer_id"`
	UploadedAt time.Time `json:"uploaded_at"`
}

type FileListResponse struct {
	Files []FileInfo `json:"files"`
	Count int        `json:"count"`
}

type UploadResponse struct {
	Status          string  `json:"status"`
	FileID          string  `json:"file_id"`
	BytesProcessed  int     `json:"bytes_processed"`
	ChunksProcessed int     `json:"chunks_processed"`
	ThroughputMbps  float64 `json:"throughput_mbps"`
	Message         string  `json:"message"`
}

type DownloadResponse struct {
	Status         string  `json:"status"`
	BytesReceived  int     `json:"bytes_received"`
	ThroughputMbps float64 `json:"throughput_mbps"`
	Message        string  `json:"message"`
}

func NewRedGiantPeer(serverURL, peerID string) *RedGiantPeer {
	return &RedGiantPeer{
		ServerURL: serverURL,
		PeerID:    peerID,
		Client: &http.Client{
			Timeout: 60 * time.Second,
		},
	}
}

// Upload a file to the Red Giant network
func (p *RedGiantPeer) UploadFile(filename string) (*UploadResponse, error) {
	// Read file
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("failed to read file: %w", err)
	}

	fileInfo, _ := os.Stat(filename)
	fmt.Printf("📤 Uploading: %s (%d bytes)\n", filepath.Base(filename), len(data))

	// Create multipart request with metadata
	url := fmt.Sprintf("%s/upload", p.ServerURL)
	req, err := http.NewRequest("POST", url, bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	// Add headers
	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("X-Peer-ID", p.PeerID)
	req.Header.Set("X-File-Name", filepath.Base(filename))
	req.Header.Set("X-File-Size", fmt.Sprintf("%d", fileInfo.Size()))

	// Send request
	start := time.Now()
	resp, err := p.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()

	uploadTime := time.Since(start)

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result UploadResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	fmt.Printf("✅ Upload successful!\n")
	fmt.Printf("   • File ID: %s\n", result.FileID)
	fmt.Printf("   • Upload time: %v\n", uploadTime)
	fmt.Printf("   • Server throughput: %.2f MB/s\n", result.ThroughputMbps)

	return &result, nil
}

// Download a file from the Red Giant network
func (p *RedGiantPeer) DownloadFile(fileID, outputPath string) (*DownloadResponse, error) {
	fmt.Printf("📥 Downloading file ID: %s\n", fileID)

	url := fmt.Sprintf("%s/download/%s", p.ServerURL, fileID)
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("X-Peer-ID", p.PeerID)

	start := time.Now()
	resp, err := p.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("download failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	// Create output file
	outFile, err := os.Create(outputPath)
	if err != nil {
		return nil, fmt.Errorf("failed to create output file: %w", err)
	}
	defer outFile.Close()

	// Copy data
	bytesReceived, err := io.Copy(outFile, resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to write file: %w", err)
	}

	downloadTime := time.Since(start)
	throughput := float64(bytesReceived) / downloadTime.Seconds() / (1024 * 1024)

	fmt.Printf("✅ Download successful!\n")
	fmt.Printf("   • Bytes received: %d\n", bytesReceived)
	fmt.Printf("   • Download time: %v\n", downloadTime)
	fmt.Printf("   • Throughput: %.2f MB/s\n", throughput)
	fmt.Printf("   • Saved to: %s\n", outputPath)

	return &DownloadResponse{
		Status:         "success",
		BytesReceived:  int(bytesReceived),
		ThroughputMbps: throughput,
		Message:        "File downloaded successfully",
	}, nil
}

// List available files in the network
func (p *RedGiantPeer) ListFiles() (*FileListResponse, error) {
	url := fmt.Sprintf("%s/files", p.ServerURL)
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("X-Peer-ID", p.PeerID)

	resp, err := p.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result FileListResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &result, nil
}

// Search for files by name pattern
func (p *RedGiantPeer) SearchFiles(pattern string) (*FileListResponse, error) {
	url := fmt.Sprintf("%s/search?q=%s", p.ServerURL, pattern)
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("X-Peer-ID", p.PeerID)

	resp, err := p.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	var result FileListResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &result, nil
}

// Main function to handle command line arguments and execute actions
func Main() {
	if len(os.Args) < 2 {
		fmt.Println("🚀 Red Giant Protocol - Peer-to-Peer Client")
		fmt.Println("Usage:")
		fmt.Println("  go run red_giant_peer.go upload <file>")
		fmt.Println("  go run red_giant_peer.go download <file-id> [output-path]")
		fmt.Println("  go run red_giant_peer.go list")
		fmt.Println("  go run red_giant_peer.go search <pattern>")
		fmt.Println("  go run red_giant_peer.go share <folder>")
		fmt.Println("")
		fmt.Println("Examples:")
		fmt.Println("  go run red_giant_peer.go upload document.pdf")
		fmt.Println("  go run red_giant_peer.go download abc123 downloaded_file.pdf")
		fmt.Println("  go run red_giant_peer.go list")
		fmt.Println("  go run red_giant_peer.go search *.pdf")
		fmt.Println("  go run red_giant_peer.go share ./my_files")
		return
	}

	// Create peer with unique ID
	peerID := fmt.Sprintf("peer_%d", time.Now().Unix())
	serverURL := "http://localhost:8080"
	if env := os.Getenv("RED_GIANT_SERVER"); env != "" {
		serverURL = env
	}

	peer := NewRedGiantPeer(serverURL, peerID)
	fmt.Printf("🌐 Connected to Red Giant network: %s\n", serverURL)
	fmt.Printf("🆔 Peer ID: %s\n\n", peerID)

	command := os.Args[1]

	switch command {
	case "upload":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_peer.go upload <file>")
			return
		}
		filename := os.Args[2]
		if _, err := peer.UploadFile(filename); err != nil {
			fmt.Printf("❌ Upload failed: %v\n", err)
		}

	case "download":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_peer.go download <file-id> [output-path]")
			return
		}
		fileID := os.Args[2]
		outputPath := fmt.Sprintf("downloaded_%s", fileID)
		if len(os.Args) > 3 {
			outputPath = os.Args[3]
		}
		if _, err := peer.DownloadFile(fileID, outputPath); err != nil {
			fmt.Printf("❌ Download failed: %v\n", err)
		}

	case "list":
		files, err := peer.ListFiles()
		if err != nil {
			fmt.Printf("❌ List failed: %v\n", err)
			return
		}
		fmt.Printf("📁 Available files (%d):\n", files.Count)
		for i, file := range files.Files {
			hashDisplay := file.Hash
			if len(hashDisplay) > 8 {
				hashDisplay = hashDisplay[:8] + "..."
			}
			fmt.Printf("  [%d] %s\n", i+1, file.Name)
			fmt.Printf("      ID: %s\n", hashDisplay)
			fmt.Printf("      Size: %d bytes (%.2f MB)\n", file.Size, float64(file.Size)/(1024*1024))
			fmt.Printf("      Peer: %s\n", file.PeerID)
			fmt.Printf("      Uploaded: %v\n\n", file.UploadedAt.Format("2006-01-02 15:04:05"))
		}

	case "search":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_peer.go search <pattern>")
			return
		}
		pattern := os.Args[2]
		files, err := peer.SearchFiles(pattern)
		if err != nil {
			fmt.Printf("❌ Search failed: %v\n", err)
			return
		}
		fmt.Printf("🔍 Search results for '%s' (%d files):\n", pattern, files.Count)
		for i, file := range files.Files {
			hashDisplay := file.Hash
			if len(hashDisplay) > 8 {
				hashDisplay = hashDisplay[:8] + "..."
			}
			fmt.Printf("  [%d] %s (ID: %s)\n", i+1, file.Name, hashDisplay)
		}

	case "share":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_peer.go share <folder>")
			return
		}
		folder := os.Args[2]
		fmt.Printf("📂 Sharing folder: %s\n", folder)

		err := filepath.Walk(folder, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if !info.IsDir() {
				fmt.Printf("📤 Uploading: %s\n", path)
				if _, err := peer.UploadFile(path); err != nil {
					fmt.Printf("❌ Failed to upload %s: %v\n", path, err)
				}
			}
			return nil
		})

		if err != nil {
			fmt.Printf("❌ Share failed: %v\n", err)
		} else {
			fmt.Printf("✅ Folder shared successfully!\n")
		}

	default:
		fmt.Printf("❌ Unknown command: %s\n", command)
		fmt.Println("Use 'upload', 'download', 'list', 'search', or 'share'")
	}
}
