// Red Giant Protocol - Universal Client for All Formats
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"mime"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

type UniversalClient struct {
	ServerURL string
	PeerID    string
	Client    *http.Client
}

type UploadResult struct {
	Status          string  `json:"status"`
	FileID          string  `json:"file_id"`
	BytesProcessed  int     `json:"bytes_processed"`
	OriginalSize    int     `json:"original_size"`
	ChunksProcessed int     `json:"chunks_processed"`
	ThroughputMbps  float64 `json:"throughput_mbps"`
	ContentType     string  `json:"content_type"`
	ProcessMode     int     `json:"process_mode"`
	IsCompressed    bool    `json:"is_compressed"`
	OptimalChunk    int     `json:"optimal_chunk_size"`
	Message         string  `json:"message"`
}

func NewUniversalClient(serverURL, peerID string) *UniversalClient {
	return &UniversalClient{
		ServerURL: serverURL,
		PeerID:    peerID,
		Client: &http.Client{
			Timeout: 60 * time.Second,
		},
	}
}

// Smart upload that auto-detects format and optimizes
func (uc *UniversalClient) SmartUpload(filePath string) (*UploadResult, error) {
	// Read file
	data, err := os.ReadFile(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to read file: %w", err)
	}

	// Auto-detect content type
	contentType := mime.TypeByExtension(filepath.Ext(filePath))
	if contentType == "" {
		contentType = http.DetectContentType(data)
	}

	fileName := filepath.Base(filePath)
	fmt.Printf("📤 Smart uploading: %s\n", fileName)
	fmt.Printf("   📋 Size: %d bytes (%.2f MB)\n", len(data), float64(len(data))/(1024*1024))
	fmt.Printf("   🎯 Detected type: %s\n", contentType)

	return uc.uploadWithType(data, fileName, contentType)
}

// Upload JSON data with optimization
func (uc *UniversalClient) UploadJSON(data interface{}, fileName string) (*UploadResult, error) {
	jsonData, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		return nil, fmt.Errorf("failed to marshal JSON: %w", err)
	}

	fmt.Printf("📤 JSON upload: %s (%d bytes)\n", fileName, len(jsonData))
	return uc.uploadWithType(jsonData, fileName, "application/json")
}

// Upload text with encoding optimization
func (uc *UniversalClient) UploadText(text, fileName string) (*UploadResult, error) {
	data := []byte(text)
	fmt.Printf("📤 Text upload: %s (%d bytes)\n", fileName, len(data))
	return uc.uploadWithType(data, fileName, "text/plain; charset=utf-8")
}

// Upload binary data with optimal chunking
func (uc *UniversalClient) UploadBinary(data []byte, fileName string) (*UploadResult, error) {
	fmt.Printf("📤 Binary upload: %s (%d bytes)\n", fileName, len(data))
	return uc.uploadWithType(data, fileName, "application/octet-stream")
}

// Stream large files with adaptive chunking
func (uc *UniversalClient) StreamUpload(filePath string) (*UploadResult, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer file.Close()

	// Get file info
	info, err := file.Stat()
	if err != nil {
		return nil, fmt.Errorf("failed to get file info: %w", err)
	}

	// Auto-detect content type
	contentType := mime.TypeByExtension(filepath.Ext(filePath))
	if contentType == "" {
		// Read first 512 bytes for detection
		buffer := make([]byte, 512)
		n, _ := file.Read(buffer)
		contentType = http.DetectContentType(buffer[:n])
		file.Seek(0, 0) // Reset file position
	}

	fileName := filepath.Base(filePath)
	fmt.Printf("📡 Streaming upload: %s\n", fileName)
	fmt.Printf("   📋 Size: %d bytes (%.2f MB)\n", info.Size(), float64(info.Size())/(1024*1024))
	fmt.Printf("   🎯 Content type: %s\n", contentType)

	// Create request
	req, err := http.NewRequest("POST", uc.ServerURL+"/upload", file)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-Peer-ID", uc.PeerID)
	req.Header.Set("X-File-Name", fileName)
	req.Header.Set("X-Stream-Mode", "true")
	req.ContentLength = info.Size()

	// Send request
	start := time.Now()
	resp, err := uc.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()

	uploadTime := time.Since(start)

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result UploadResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	fmt.Printf("✅ Stream upload successful!\n")
	fmt.Printf("   • File ID: %s\n", result.FileID)
	fmt.Printf("   • Upload time: %v\n", uploadTime)
	fmt.Printf("   • Server throughput: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("   • Process mode: %d\n", result.ProcessMode)
	fmt.Printf("   • Compressed: %v\n", result.IsCompressed)

	return &result, nil
}

// Internal upload method with content type
func (uc *UniversalClient) uploadWithType(data []byte, fileName, contentType string) (*UploadResult, error) {
	req, err := http.NewRequest("POST", uc.ServerURL+"/upload", bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-Peer-ID", uc.PeerID)
	req.Header.Set("X-File-Name", fileName)

	start := time.Now()
	resp, err := uc.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()

	uploadTime := time.Since(start)

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result UploadResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	fmt.Printf("✅ Upload successful!\n")
	fmt.Printf("   • File ID: %s\n", result.FileID)
	fmt.Printf("   • Upload time: %v\n", uploadTime)
	fmt.Printf("   • Server throughput: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("   • Optimal chunk: %d KB\n", result.OptimalChunk/1024)
	fmt.Printf("   • Compressed: %v\n", result.IsCompressed)
	fmt.Printf("   • Compression ratio: %.1f%%\n",
		(1.0-float64(result.BytesProcessed)/float64(result.OriginalSize))*100)

	return &result, nil
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("🚀 Red Giant Protocol - Universal Client")
		fmt.Println("Usage:")
		fmt.Println("  go run red_giant_universal.go smart <file>")
		fmt.Println("  go run red_giant_universal.go stream <file>")
		fmt.Println("  go run red_giant_universal.go json <data> <filename>")
		fmt.Println("  go run red_giant_universal.go text <text> <filename>")
		fmt.Println("  go run red_giant_universal.go binary <file>")
		fmt.Println("")
		fmt.Println("Examples:")
		fmt.Println("  go run red_giant_universal.go smart document.pdf")
		fmt.Println("  go run red_giant_universal.go stream video.mp4")
		fmt.Println("  go run red_giant_universal.go json '{\"test\":true}' data.json")
		fmt.Println("  go run red_giant_universal.go text 'Hello World' message.txt")
		return
	}

	serverURL := "http://localhost:8080"
	if env := os.Getenv("RED_GIANT_SERVER"); env != "" {
		serverURL = env
	}

	peerID := fmt.Sprintf("universal_%d", time.Now().Unix())
	client := NewUniversalClient(serverURL, peerID)

	fmt.Printf("🌐 Connected to Red Giant Adaptive Server: %s\n", serverURL)
	fmt.Printf("🆔 Peer ID: %s\n\n", peerID)

	command := os.Args[1]

	switch command {
	case "smart":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_universal.go smart <file>")
			return
		}
		filePath := os.Args[2]
		if _, err := client.SmartUpload(filePath); err != nil {
			fmt.Printf("❌ Smart upload failed: %v\n", err)
		}

	case "stream":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_universal.go stream <file>")
			return
		}
		filePath := os.Args[2]
		if _, err := client.StreamUpload(filePath); err != nil {
			fmt.Printf("❌ Stream upload failed: %v\n", err)
		}

	case "json":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run red_giant_universal.go json <json-data> <filename>")
			return
		}
		jsonStr := os.Args[2]
		fileName := os.Args[3]

		var data interface{}
		if err := json.Unmarshal([]byte(jsonStr), &data); err != nil {
			fmt.Printf("❌ Invalid JSON: %v\n", err)
			return
		}

		if _, err := client.UploadJSON(data, fileName); err != nil {
			fmt.Printf("❌ JSON upload failed: %v\n", err)
		}

	case "text":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run red_giant_universal.go text <text> <filename>")
			return
		}
		text := os.Args[2]
		fileName := os.Args[3]

		if _, err := client.UploadText(text, fileName); err != nil {
			fmt.Printf("❌ Text upload failed: %v\n", err)
		}

	case "binary":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run red_giant_universal.go binary <file>")
			return
		}
		filePath := os.Args[2]

		data, err := os.ReadFile(filePath)
		if err != nil {
			fmt.Printf("❌ Failed to read file: %v\n", err)
			return
		}

		fileName := filepath.Base(filePath)
		if _, err := client.UploadBinary(data, fileName); err != nil {
			fmt.Printf("❌ Binary upload failed: %v\n", err)
		}

	default:
		fmt.Printf("❌ Unknown command: %s\n", command)
		fmt.Println("Use 'smart', 'stream', 'json', 'text', or 'binary'")
	}
}
