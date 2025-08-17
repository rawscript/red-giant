// Red Giant Protocol - Go SDK
// High-performance client library preserving CGO operations
package redgiant

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// Client represents a Red Giant Protocol client
type Client struct {
	BaseURL    string
	PeerID     string
	HTTPClient *http.Client
	UserAgent  string
}

// FileInfo represents file metadata in the Red Giant network
type FileInfo struct {
	ID         string    `json:"id"`
	Name       string    `json:"name"`
	Size       int64     `json:"size"`
	Hash       string    `json:"hash"`
	PeerID     string    `json:"peer_id"`
	UploadedAt time.Time `json:"uploaded_at"`
}

// UploadResult represents the result of a file upload
type UploadResult struct {
	Status          string  `json:"status"`
	FileID          string  `json:"file_id"`
	BytesProcessed  int     `json:"bytes_processed"`
	ChunksProcessed int     `json:"chunks_processed"`
	ThroughputMbps  float64 `json:"throughput_mbps"`
	ProcessingTime  int64   `json:"processing_time_ms"`
	Message         string  `json:"message"`
}

// ProcessResult represents the result of raw data processing
type ProcessResult struct {
	Status         string  `json:"status"`
	BytesProcessed int     `json:"bytes_processed"`
	ThroughputMbps float64 `json:"throughput_mbps"`
	Timestamp      int64   `json:"timestamp"`
}

// NetworkStats represents network statistics
type NetworkStats struct {
	TotalRequests  int64   `json:"total_requests"`
	TotalBytes     int64   `json:"total_bytes"`
	TotalChunks    int64   `json:"total_chunks"`
	AverageLatency int64   `json:"average_latency_ms"`
	ErrorCount     int64   `json:"error_count"`
	UptimeSeconds  int64   `json:"uptime_seconds"`
	ThroughputMbps float64 `json:"throughput_mbps"`
	Timestamp      int64   `json:"timestamp"`
}

// NewClient creates a new Red Giant client
func NewClient(baseURL string) *Client {
	return &Client{
		BaseURL: strings.TrimSuffix(baseURL, "/"),
		PeerID:  fmt.Sprintf("go_client_%d", time.Now().Unix()),
		HTTPClient: &http.Client{
			Timeout: 60 * time.Second,
		},
		UserAgent: "RedGiant-Go-SDK/1.0",
	}
}

// SetPeerID sets a custom peer ID
func (c *Client) SetPeerID(peerID string) {
	c.PeerID = peerID
}

// UploadFile uploads a file to the Red Giant network
func (c *Client) UploadFile(filePath string) (*UploadResult, error) {
	// Read file
	data, err := os.ReadFile(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to read file: %w", err)
	}

	fileName := filepath.Base(filePath)
	return c.UploadData(data, fileName)
}

// UploadData uploads raw data to the Red Giant network
func (c *Client) UploadData(data []byte, fileName string) (*UploadResult, error) {
	// Create request
	req, err := http.NewRequest("POST", c.BaseURL+"/upload", bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	// Set headers
	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("X-Peer-ID", c.PeerID)
	req.Header.Set("X-File-Name", fileName)
	req.Header.Set("User-Agent", c.UserAgent)

	// Send request
	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result UploadResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &result, nil
}

// DownloadFile downloads a file from the Red Giant network
func (c *Client) DownloadFile(fileID, outputPath string) error {
	// Create request
	req, err := http.NewRequest("GET", c.BaseURL+"/download/"+fileID, nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("X-Peer-ID", c.PeerID)
	req.Header.Set("User-Agent", c.UserAgent)

	// Send request
	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return fmt.Errorf("download failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	// Create output file
	outFile, err := os.Create(outputPath)
	if err != nil {
		return fmt.Errorf("failed to create output file: %w", err)
	}
	defer outFile.Close()

	// Copy data
	_, err = io.Copy(outFile, resp.Body)
	if err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}

	return nil
}

// DownloadData downloads file data directly into memory
func (c *Client) DownloadData(fileID string) ([]byte, error) {
	req, err := http.NewRequest("GET", c.BaseURL+"/download/"+fileID, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("X-Peer-ID", c.PeerID)
	req.Header.Set("User-Agent", c.UserAgent)

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("download failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	return io.ReadAll(resp.Body)
}

// ListFiles lists all files in the Red Giant network
func (c *Client) ListFiles() ([]FileInfo, error) {
	req, err := http.NewRequest("GET", c.BaseURL+"/files", nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("User-Agent", c.UserAgent)

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("server error %d", resp.StatusCode)
	}

	var result struct {
		Files []FileInfo `json:"files"`
		Count int        `json:"count"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return result.Files, nil
}

// SearchFiles searches for files by name pattern
func (c *Client) SearchFiles(pattern string) ([]FileInfo, error) {
	params := url.Values{}
	params.Add("q", pattern)

	req, err := http.NewRequest("GET", c.BaseURL+"/search?"+params.Encode(), nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("User-Agent", c.UserAgent)

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("server error %d", resp.StatusCode)
	}

	var result struct {
		Files []FileInfo `json:"files"`
		Count int        `json:"count"`
		Query string     `json:"query"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return result.Files, nil
}

// ProcessData processes raw data using Red Giant's high-performance C core
func (c *Client) ProcessData(data []byte) (*ProcessResult, error) {
	req, err := http.NewRequest("POST", c.BaseURL+"/process", bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("User-Agent", c.UserAgent)

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("processing failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result ProcessResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &result, nil
}

// GetNetworkStats retrieves network performance statistics
func (c *Client) GetNetworkStats() (*NetworkStats, error) {
	req, err := http.NewRequest("GET", c.BaseURL+"/metrics", nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("User-Agent", c.UserAgent)

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("server error %d", resp.StatusCode)
	}

	var stats NetworkStats
	if err := json.NewDecoder(resp.Body).Decode(&stats); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &stats, nil
}

// HealthCheck checks if the Red Giant server is healthy
func (c *Client) HealthCheck() error {
	req, err := http.NewRequest("GET", c.BaseURL+"/health", nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("User-Agent", c.UserAgent)

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return fmt.Errorf("health check failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("server unhealthy: status %d", resp.StatusCode)
	}

	return nil
}

// StreamingUpload uploads large files with streaming for better performance
func (c *Client) StreamingUpload(filePath string, chunkSize int) (*UploadResult, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer file.Close()

	fileName := filepath.Base(filePath)

	// Create multipart form
	var buf bytes.Buffer
	writer := multipart.NewWriter(&buf)

	// Add file field
	fileWriter, err := writer.CreateFormFile("file", fileName)
	if err != nil {
		return nil, fmt.Errorf("failed to create form file: %w", err)
	}

	// Copy file data
	_, err = io.Copy(fileWriter, file)
	if err != nil {
		return nil, fmt.Errorf("failed to copy file data: %w", err)
	}

	err = writer.Close()
	if err != nil {
		return nil, fmt.Errorf("failed to close multipart writer: %w", err)
	}

	// Create request
	req, err := http.NewRequest("POST", c.BaseURL+"/upload", &buf)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", writer.FormDataContentType())
	req.Header.Set("X-Peer-ID", c.PeerID)
	req.Header.Set("X-File-Name", fileName)
	req.Header.Set("X-Stream-Mode", "true")
	req.Header.Set("User-Agent", c.UserAgent)

	// Send request
	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	var result UploadResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &result, nil
}
