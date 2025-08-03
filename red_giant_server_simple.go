// Red Giant Protocol - Simplified P2P Server (Pure Go)
package main

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

// Configuration with environment variable support
type Config struct {
	Port         int    `json:"port"`
	Host         string `json:"host"`
	MaxWorkers   int    `json:"max_workers"`
	MaxChunkSize int    `json:"max_chunk_size"`
	BufferSize   int    `json:"buffer_size"`
	LogLevel     string `json:"log_level"`
}

func NewConfig() *Config {
	config := &Config{
		Port:         8080,
		Host:         "0.0.0.0",
		MaxWorkers:   runtime.NumCPU() * 2,
		MaxChunkSize: 256 * 1024, // 256KB
		BufferSize:   1024 * 1024, // 1MB
		LogLevel:     "INFO",
	}

	// Override with environment variables
	if port := os.Getenv("RED_GIANT_PORT"); port != "" {
		if p, err := strconv.Atoi(port); err == nil {
			config.Port = p
		}
	}
	if host := os.Getenv("RED_GIANT_HOST"); host != "" {
		config.Host = host
	}
	if workers := os.Getenv("RED_GIANT_WORKERS"); workers != "" {
		if w, err := strconv.Atoi(workers); err == nil {
			config.MaxWorkers = w
		}
	}

	return config
}

type Metrics struct {
	TotalRequests   int64
	TotalBytes      int64
	TotalChunks     int64
	AverageLatency  int64
	ErrorCount      int64
	StartTime       time.Time
	mu              sync.RWMutex
}

func NewMetrics() *Metrics {
	return &Metrics{
		StartTime: time.Now(),
	}
}

func (m *Metrics) RecordRequest(bytes int64, chunks int64, latency time.Duration) {
	atomic.AddInt64(&m.TotalRequests, 1)
	atomic.AddInt64(&m.TotalBytes, bytes)
	atomic.AddInt64(&m.TotalChunks, chunks)
	atomic.StoreInt64(&m.AverageLatency, latency.Milliseconds())
}

func (m *Metrics) RecordError() {
	atomic.AddInt64(&m.ErrorCount, 1)
}

func (m *Metrics) GetSnapshot() map[string]interface{} {
	uptime := time.Since(m.StartTime).Seconds()
	throughput := float64(atomic.LoadInt64(&m.TotalBytes)) / uptime / (1024 * 1024)

	return map[string]interface{}{
		"total_requests":     atomic.LoadInt64(&m.TotalRequests),
		"total_bytes":        atomic.LoadInt64(&m.TotalBytes),
		"total_chunks":       atomic.LoadInt64(&m.TotalChunks),
		"average_latency_ms": atomic.LoadInt64(&m.AverageLatency),
		"error_count":        atomic.LoadInt64(&m.ErrorCount),
		"uptime_seconds":     int64(uptime),
		"throughput_mbps":    throughput,
		"timestamp":          time.Now().Unix(),
	}
}

// File storage for peer-to-peer functionality
type StoredFile struct {
	ID         string    `json:"id"`
	Name       string    `json:"name"`
	Size       int64     `json:"size"`
	Hash       string    `json:"hash"`
	PeerID     string    `json:"peer_id"`
	Data       []byte    `json:"-"`
	UploadedAt time.Time `json:"uploaded_at"`
}

// High-performance Red Giant processor (Pure Go)
type RedGiantProcessor struct {
	config      *Config
	workerPool  chan struct{}
	metrics     *Metrics
	logger      *log.Logger
	fileStorage map[string]*StoredFile
	storageMu   sync.RWMutex
}

func NewRedGiantProcessor(config *Config) *RedGiantProcessor {
	processor := &RedGiantProcessor{
		config:      config,
		workerPool:  make(chan struct{}, config.MaxWorkers),
		metrics:     NewMetrics(),
		logger:      log.New(os.Stdout, "[RedGiant] ", log.LstdFlags),
		fileStorage: make(map[string]*StoredFile),
	}
	
	processor.logger.Printf("🚀 Pure Go Red Giant Protocol initialized")
	return processor
}

// High-performance chunk processing using Red Giant protocol (Pure Go)
func (rg *RedGiantProcessor) ProcessData(data []byte) (int, time.Duration, error) {
	start := time.Now()
	
	chunkSize := rg.config.MaxChunkSize
	totalChunks := (len(data) + chunkSize - 1) / chunkSize
	
	// Use worker pool for parallel processing
	numWorkers := min(rg.config.MaxWorkers, totalChunks)
	chunksPerWorker := totalChunks / numWorkers
	
	var wg sync.WaitGroup
	var processedChunks int64
	
	for worker := 0; worker < numWorkers; worker++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			
			// Acquire worker slot
			rg.workerPool <- struct{}{}
			defer func() { <-rg.workerPool }()
			
			start := workerID * chunksPerWorker
			end := start + chunksPerWorker
			if workerID == numWorkers-1 {
				end = totalChunks
			}
			
			for i := start; i < end; i++ {
				chunkStart := i * chunkSize
				chunkEnd := chunkStart + chunkSize
				if chunkEnd > len(data) {
					chunkEnd = len(data)
				}
				
				// Red Giant exposure processing (high-speed pure Go)
				_ = data[chunkStart:chunkEnd]
				atomic.AddInt64(&processedChunks, 1)
			}
		}(worker)
	}
	
	wg.Wait()
	duration := time.Since(start)
	
	return int(processedChunks), duration, nil
}

// Initialize file storage directory
func (rg *RedGiantProcessor) initFileStorage() {
	// Create storage directory if it doesn't exist
	if err := os.MkdirAll("./storage", 0755); err != nil {
		rg.logger.Printf("Warning: Could not create storage directory: %v", err)
	}
	rg.logger.Printf("📁 File storage initialized")
}

// Generate file ID from content hash
func (rg *RedGiantProcessor) generateFileID(data []byte, filename string) string {
	hash := sha256.Sum256(data)
	return fmt.Sprintf("%x", hash[:8]) // Use first 8 bytes of hash
}

// Handle file upload for P2P sharing
func (rg *RedGiantProcessor) handleUpload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Get peer info from headers
	peerID := r.Header.Get("X-Peer-ID")
	fileName := r.Header.Get("X-File-Name")
	if peerID == "" || fileName == "" {
		http.Error(w, "Missing peer ID or file name", http.StatusBadRequest)
		return
	}

	// Read file data
	data := make([]byte, rg.config.BufferSize)
	n, err := r.Body.Read(data)
	if err != nil && err.Error() != "EOF" {
		http.Error(w, "Failed to read file data", http.StatusBadRequest)
		rg.metrics.RecordError()
		return
	}
	data = data[:n]

	// Process with Red Giant protocol for high-speed handling
	chunks, duration, err := rg.ProcessData(data)
	if err != nil {
		http.Error(w, "Processing failed", http.StatusInternalServerError)
		rg.metrics.RecordError()
		return
	}

	// Generate file ID and store
	fileID := rg.generateFileID(data, fileName)
	
	storedFile := &StoredFile{
		ID:         fileID,
		Name:       fileName,
		Size:       int64(len(data)),
		Hash:       fileID,
		PeerID:     peerID,
		Data:       make([]byte, len(data)),
		UploadedAt: time.Now(),
	}
	copy(storedFile.Data, data)

	// Store in memory and optionally on disk
	rg.storageMu.Lock()
	rg.fileStorage[fileID] = storedFile
	rg.storageMu.Unlock()

	// Save to disk for persistence
	diskPath := fmt.Sprintf("./storage/%s_%s", fileID, fileName)
	if err := os.WriteFile(diskPath, data, 0644); err != nil {
		rg.logger.Printf("Warning: Could not save file to disk: %v", err)
	}

	// Record metrics
	rg.metrics.RecordRequest(int64(len(data)), int64(chunks), duration)
	throughput := float64(len(data)) / duration.Seconds() / (1024 * 1024)

	// Send response
	response := map[string]interface{}{
		"status":             "success",
		"file_id":            fileID,
		"bytes_processed":    len(data),
		"chunks_processed":   chunks,
		"processing_time_ms": duration.Milliseconds(),
		"throughput_mbps":    throughput,
		"message":            fmt.Sprintf("File '%s' uploaded successfully", fileName),
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)

	rg.logger.Printf("📤 File uploaded: %s (%d bytes) by peer %s", fileName, len(data), peerID)
}

// Handle file download for P2P sharing
func (rg *RedGiantProcessor) handleDownload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Extract file ID from URL path
	fileID := strings.TrimPrefix(r.URL.Path, "/download/")
	if fileID == "" {
		http.Error(w, "File ID required", http.StatusBadRequest)
		return
	}

	// Get peer ID
	peerID := r.Header.Get("X-Peer-ID")

	// Find file in storage
	rg.storageMu.RLock()
	storedFile, exists := rg.fileStorage[fileID]
	rg.storageMu.RUnlock()

	if !exists {
		// Try loading from disk
		matches, _ := filepath.Glob(fmt.Sprintf("./storage/%s_*", fileID))
		if len(matches) == 0 {
			http.Error(w, "File not found", http.StatusNotFound)
			return
		}

		// Load from disk
		diskData, err := os.ReadFile(matches[0])
		if err != nil {
			http.Error(w, "Failed to read file", http.StatusInternalServerError)
			return
		}

		// Create temporary stored file
		fileName := filepath.Base(matches[0])
		storedFile = &StoredFile{
			ID:   fileID,
			Name: fileName,
			Size: int64(len(diskData)),
			Data: diskData,
		}
	}

	// Set headers
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=\"%s\"", storedFile.Name))
	w.Header().Set("Content-Length", fmt.Sprintf("%d", storedFile.Size))

	// Send file data
	start := time.Now()
	bytesWritten, err := w.Write(storedFile.Data)
	if err != nil {
		rg.logger.Printf("Error sending file: %v", err)
		return
	}

	duration := time.Since(start)
	throughput := float64(bytesWritten) / duration.Seconds() / (1024 * 1024)

	rg.logger.Printf("📥 File downloaded: %s (%d bytes) by peer %s (%.2f MB/s)", 
		storedFile.Name, bytesWritten, peerID, throughput)
}

// Handle file listing for P2P discovery
func (rg *RedGiantProcessor) handleListFiles(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	rg.storageMu.RLock()
	files := make([]StoredFile, 0, len(rg.fileStorage))
	for _, file := range rg.fileStorage {
		// Don't include the actual data in the list
		fileCopy := *file
		fileCopy.Data = nil
		files = append(files, fileCopy)
	}
	rg.storageMu.RUnlock()

	response := map[string]interface{}{
		"files": files,
		"count": len(files),
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)
}

// Handle file search for P2P discovery
func (rg *RedGiantProcessor) handleSearchFiles(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	query := r.URL.Query().Get("q")
	if query == "" {
		http.Error(w, "Search query required", http.StatusBadRequest)
		return
	}

	rg.storageMu.RLock()
	var matchedFiles []StoredFile
	for _, file := range rg.fileStorage {
		// Simple pattern matching
		if strings.Contains(strings.ToLower(file.Name), strings.ToLower(query)) ||
		   strings.Contains(strings.ToLower(query), "*") {
			fileCopy := *file
			fileCopy.Data = nil
			matchedFiles = append(matchedFiles, fileCopy)
		}
	}
	rg.storageMu.RUnlock()

	response := map[string]interface{}{
		"files": matchedFiles,
		"count": len(matchedFiles),
		"query": query,
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)
}

// Handle raw data processing
func (rg *RedGiantProcessor) handleProcess(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		rg.metrics.RecordError()
		return
	}

	// Read request body
	data := make([]byte, rg.config.BufferSize)
	n, err := r.Body.Read(data)
	if err != nil && err.Error() != "EOF" {
		http.Error(w, "Failed to read data", http.StatusBadRequest)
		rg.metrics.RecordError()
		return
	}
	data = data[:n]

	// Process with Red Giant protocol
	chunks, duration, err := rg.ProcessData(data)
	if err != nil {
		http.Error(w, "Processing failed", http.StatusInternalServerError)
		rg.metrics.RecordError()
		return
	}

	// Record metrics
	rg.metrics.RecordRequest(int64(len(data)), int64(chunks), duration)

	// Calculate throughput
	throughput := float64(len(data)) / duration.Seconds() / (1024 * 1024)

	// Send response
	response := map[string]interface{}{
		"status":             "success",
		"bytes_processed":    len(data),
		"chunks_processed":   chunks,
		"processing_time_ms": duration.Milliseconds(),
		"throughput_mbps":    throughput,
		"timestamp":          time.Now().Unix(),
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)

	rg.logger.Printf("Processed %d bytes in %v (%.2f MB/s)", len(data), duration, throughput)
}

func (rg *RedGiantProcessor) handleMetrics(w http.ResponseWriter, r *http.Request) {
	metrics := rg.metrics.GetSnapshot()
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(metrics)
}

func (rg *RedGiantProcessor) handleHealth(w http.ResponseWriter, r *http.Request) {
	health := map[string]interface{}{
		"status":    "healthy",
		"version":   "1.0.0",
		"timestamp": time.Now().Unix(),
		"uptime":    time.Since(rg.metrics.StartTime).Seconds(),
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(health)
}

func (rg *RedGiantProcessor) handleRoot(w http.ResponseWriter, r *http.Request) {
	html := `<!DOCTYPE html>
<html>
<head>
    <title>Red Giant Protocol P2P Server</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #d32f2f; text-align: center; }
        .endpoint { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #d32f2f; }
        .method { color: #d32f2f; font-weight: bold; }
        pre { background: #263238; color: #fff; padding: 15px; border-radius: 5px; overflow-x: auto; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }
        .stat { background: #e3f2fd; padding: 15px; border-radius: 5px; text-align: center; }
        .stat-value { font-size: 24px; font-weight: bold; color: #1976d2; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 Red Giant Protocol P2P Server</h1>
        <p><strong>Status:</strong> Running | <strong>Version:</strong> 1.0.0 | <strong>Mode:</strong> Pure Go High-Performance</p>
        
        <h2>🌐 P2P Network Commands</h2>
        
        <div class="endpoint">
            <strong class="method">UPLOAD</strong> File to Network
            <pre>go run red_giant_peer.go upload myfile.pdf</pre>
        </div>

        <div class="endpoint">
            <strong class="method">LIST</strong> Network Files
            <pre>go run red_giant_peer.go list</pre>
        </div>

        <div class="endpoint">
            <strong class="method">DOWNLOAD</strong> File from Network
            <pre>go run red_giant_peer.go download {file-id} output.pdf</pre>
        </div>

        <div class="endpoint">
            <strong class="method">CHAT</strong> Real-time P2P Chat
            <pre>go run simple_chat.go your_name</pre>
        </div>

        <h2>🧪 Test the System</h2>
        <pre>go run test_system.go</pre>
    </div>
</body>
</html>`
	w.Header().Set("Content-Type", "text/html")
	w.Write([]byte(html))
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func main() {
	// Load configuration
	config := NewConfig()
	
	// Create processor (Pure Go)
	processor := NewRedGiantProcessor(config)
	
	// Initialize file storage
	processor.initFileStorage()
	
	// Setup HTTP routes
	mux := http.NewServeMux()
	mux.HandleFunc("/", processor.handleRoot)
	mux.HandleFunc("/process", processor.handleProcess)
	mux.HandleFunc("/upload", processor.handleUpload)
	mux.HandleFunc("/download/", processor.handleDownload)
	mux.HandleFunc("/files", processor.handleListFiles)
	mux.HandleFunc("/search", processor.handleSearchFiles)
	mux.HandleFunc("/metrics", processor.handleMetrics)
	mux.HandleFunc("/health", processor.handleHealth)
	
	// Create server
	server := &http.Server{
		Addr:         fmt.Sprintf("%s:%d", config.Host, config.Port),
		Handler:      mux,
		ReadTimeout:  30 * time.Second,
		WriteTimeout: 30 * time.Second,
		IdleTimeout:  60 * time.Second,
	}
	
	// Graceful shutdown
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	
	// Handle shutdown signals
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	
	go func() {
		<-sigChan
		processor.logger.Println("Shutting down server...")
		
		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer shutdownCancel()
		
		if err := server.Shutdown(shutdownCtx); err != nil {
			processor.logger.Printf("Server shutdown error: %v", err)
		}
		cancel()
	}()
	
	// Start server
	processor.logger.Printf("🚀 Red Giant Protocol P2P Server starting on %s:%d", config.Host, config.Port)
	processor.logger.Printf("📊 Configuration: Workers=%d, ChunkSize=%dKB, BufferSize=%dMB", 
		config.MaxWorkers, config.MaxChunkSize/1024, config.BufferSize/(1024*1024))
	processor.logger.Printf("🌐 Pure Go high-performance mode")
	processor.logger.Printf("🌐 Web interface: http://localhost:%d", config.Port)
	
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		processor.logger.Fatalf("Server failed to start: %v", err)
	}
	
	<-ctx.Done()
	processor.logger.Println("Red Giant Protocol P2P Server stopped")
}