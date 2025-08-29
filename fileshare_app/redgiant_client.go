package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

// RedGiantUploadResult represents the result of a Red Giant upload
type RedGiantUploadResult struct {
	Status          string  `json:"status"`
	FileID          string  `json:"file_id"`
	BytesProcessed  int     `json:"bytes_processed"`
	ChunksProcessed int     `json:"chunks_processed"`
	ThroughputMbps  float64 `json:"throughput_mbps"`
	ProcessingTime  int64   `json:"processing_time_ms"`
	Message         string  `json:"message"`
}

// UploadFile uploads a file to the Red Giant network
func (rg *RedGiantClient) UploadFile(filePath string) (*RedGiantUploadResult, error) {
	// Open file
	file, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer file.Close()

	// Get file info (optional, validates accessibility)
	_, err = file.Stat()
	if err != nil {
		return nil, fmt.Errorf("failed to get file info: %w", err)
	}

	// Create multipart form
	var buf bytes.Buffer
	writer := multipart.NewWriter(&buf)

	// Add file field
	fileWriter, err := writer.CreateFormFile("file", filepath.Base(filePath))
	if err != nil {
		return nil, fmt.Errorf("failed to create form file: %w", err)
	}

	// Copy file data
	_, err = io.Copy(fileWriter, file)
	if err != nil {
		return nil, fmt.Errorf("failed to copy file data: %w", err)
	}

	// Close writer
	err = writer.Close()
	if err != nil {
		return nil, fmt.Errorf("failed to close multipart writer: %w", err)
	}

	// Create request
	req, err := http.NewRequest("POST", rg.BaseURL+"/upload", &buf)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	// Set headers
	req.Header.Set("Content-Type", writer.FormDataContentType())
	req.Header.Set("X-Peer-ID", fmt.Sprintf("fileshare_app_%d", time.Now().Unix()))
	req.Header.Set("X-File-Name", filepath.Base(filePath))
	req.Header.Set("User-Agent", "RedGiant-FileShare-App/1.0")

	// Create HTTP client with timeout
	client := &http.Client{
		Timeout: 300 * time.Second, // 5 minutes for large files
	}

	// Send request
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()

	// Check response status
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("server error %d: %s", resp.StatusCode, string(body))
	}

	// Parse response
	var result RedGiantUploadResult
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return &result, nil
}

// DownloadFile downloads a file from the Red Giant network
func (rg *RedGiantClient) DownloadFile(fileID, outputPath string) error {
	// Create request
	req, err := http.NewRequest("GET", rg.BaseURL+"/download/"+fileID, nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	// Set headers
	req.Header.Set("X-Peer-ID", fmt.Sprintf("fileshare_app_%d", time.Now().Unix()))
	req.Header.Set("User-Agent", "RedGiant-FileShare-App/1.0")

	// Create HTTP client with timeout
	client := &http.Client{
		Timeout: 300 * time.Second, // 5 minutes for large files
	}

	// Send request
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("download failed: %w", err)
	}
	defer resp.Body.Close()

	// Check response status
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
