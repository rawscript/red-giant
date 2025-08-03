// Red Giant Protocol - System Test
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"time"
)

func testServer() error {
	fmt.Print("🏥 Testing server health... ")
	resp, err := http.Get("http://localhost:8080/health")
	if err != nil {
		return fmt.Errorf("health check failed: %w", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("server returned status %d", resp.StatusCode)
	}
	
	fmt.Println("✅ OK")
	return nil
}

func testUpload() (string, error) {
	fmt.Print("📤 Testing file upload... ")
	
	testData := []byte("Red Giant Protocol Test File - Hello World!")
	
	req, err := http.NewRequest("POST", "http://localhost:8080/upload", bytes.NewReader(testData))
	if err != nil {
		return "", fmt.Errorf("failed to create request: %w", err)
	}
	
	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("X-Peer-ID", "test_peer")
	req.Header.Set("X-File-Name", "test_file.txt")
	
	client := &http.Client{Timeout: 30 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("upload failed: %w", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("upload returned status %d", resp.StatusCode)
	}
	
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return "", fmt.Errorf("failed to parse response: %w", err)
	}
	
	fileID := result["file_id"].(string)
	fmt.Printf("✅ OK (ID: %s)\n", fileID)
	return fileID, nil
}

func testDownload(fileID string) error {
	fmt.Print("📥 Testing file download... ")
	
	req, err := http.NewRequest("GET", "http://localhost:8080/download/"+fileID, nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}
	
	req.Header.Set("X-Peer-ID", "test_peer")
	
	client := &http.Client{Timeout: 30 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("download failed: %w", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("download returned status %d", resp.StatusCode)
	}
	
	fmt.Println("✅ OK")
	return nil
}

func testFileList() error {
	fmt.Print("📁 Testing file list... ")
	
	resp, err := http.Get("http://localhost:8080/files")
	if err != nil {
		return fmt.Errorf("file list failed: %w", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("file list returned status %d", resp.StatusCode)
	}
	
	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}
	
	count := int(result["count"].(float64))
	fmt.Printf("✅ OK (%d files)\n", count)
	return nil
}

func main() {
	fmt.Println("🧪 Red Giant Protocol - System Test")
	fmt.Println("═══════════════════════════════════════")
	
	// Test 1: Server Health
	if err := testServer(); err != nil {
		fmt.Printf("❌ %v\n", err)
		fmt.Println("Make sure the server is running: go run red_giant_server.go")
		os.Exit(1)
	}
	
	// Test 2: File Upload
	fileID, err := testUpload()
	if err != nil {
		fmt.Printf("❌ %v\n", err)
		os.Exit(1)
	}
	
	// Test 3: File Download
	if err := testDownload(fileID); err != nil {
		fmt.Printf("❌ %v\n", err)
		os.Exit(1)
	}
	
	// Test 4: File List
	if err := testFileList(); err != nil {
		fmt.Printf("❌ %v\n", err)
		os.Exit(1)
	}
	
	fmt.Println()
	fmt.Println("🎉 All tests passed!")
	fmt.Println("✅ Red Giant P2P system is working correctly")
	fmt.Println()
	fmt.Println("Next steps:")
	fmt.Println("• Try file sharing: go run red_giant_peer.go upload README.md")
	fmt.Println("• Try chat system: go run red_giant_chat.go your_name")
	fmt.Println("• Visit web interface: http://localhost:8080")
}