
// Red Giant Protocol - Comprehensive Test Server
package main

/*
#cgo CFLAGS: -std=c99 -O3 -march=native -DNDEBUG
#include "red_giant.h"
#include "red_giant.c"
#include <stdlib.h>
*/
import "C"
import (
	"bytes"
	"context"
	"crypto/md5"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/signal"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
	"unsafe"
)

// Test configuration
type TestConfig struct {
	Port         int    `json:"port"`
	Host         string `json:"host"`
	MaxWorkers   int    `json:"max_workers"`
	MaxChunkSize int    `json:"max_chunk_size"`
	BufferSize   int    `json:"buffer_size"`
	EnableDebug  bool   `json:"enable_debug"`
	TestMode     bool   `json:"test_mode"`
}

// Comprehensive metrics
type TestMetrics struct {
	TotalRequests     int64     `json:"total_requests"`
	TotalBytes        int64     `json:"total_bytes"`
	TotalChunks       int64     `json:"total_chunks"`
	AverageLatency    int64     `json:"average_latency_ms"`
	ErrorCount        int64     `json:"error_count"`
	StartTime         time.Time `json:"start_time"`
	LastRequestTime   time.Time `json:"last_request_time"`
	MaxThroughput     float64   `json:"max_throughput_mbps"`
	MinLatency        int64     `json:"min_latency_ms"`
	MaxLatency        int64     `json:"max_latency_ms"`
	SuccessRate       float64   `json:"success_rate"`
	ActiveConnections int64     `json:"active_connections"`
	
	// Edge case tracking
	EmptyRequests     int64 `json:"empty_requests"`
	OversizedRequests int64 `json:"oversized_requests"`
	MalformedRequests int64 `json:"malformed_requests"`
	TimeoutRequests   int64 `json:"timeout_requests"`
	
	mu sync.RWMutex
}

// File storage with metadata
type TestFile struct {
	ID          string            `json:"id"`
	Name        string            `json:"name"`
	Size        int64             `json:"size"`
	Hash        string            `json:"hash"`
	MD5Hash     string            `json:"md5_hash"`
	Data        []byte            `json:"-"`
	UploadedAt  time.Time         `json:"uploaded_at"`
	ContentType string            `json:"content_type"`
	Metadata    map[string]string `json:"metadata"`
	ChunkCount  int               `json:"chunk_count"`
}

// Test processor with comprehensive error handling
type TestProcessor struct {
	config      *TestConfig
	metrics     *TestMetrics
	logger      *log.Logger
	surface     *C.rg_exposure_surface_t
	fileStorage map[string]*TestFile
	storageMu   sync.RWMutex
	workerPool  chan struct{}
}

func NewTestConfig() *TestConfig {
	return &TestConfig{
		Port:         5000, // Use Replit's recommended port
		Host:         "0.0.0.0",
		MaxWorkers:   runtime.NumCPU() * 2,
		MaxChunkSize: 256 * 1024, // 256KB
		BufferSize:   10 * 1024 * 1024, // 10MB
		EnableDebug:  true,
		TestMode:     true,
	}
}

func NewTestMetrics() *TestMetrics {
	return &TestMetrics{
		StartTime:   time.Now(),
		MinLatency:  9999999, // Initialize to high value
		MaxLatency:  0,
		SuccessRate: 100.0,
	}
}

func NewTestProcessor(config *TestConfig) *TestProcessor {
	// Create C manifest with validation
	totalChunks := uint32((config.BufferSize + config.MaxChunkSize - 1) / config.MaxChunkSize)
	if totalChunks > C.RG_MAX_CONCURRENT_CHUNKS {
		totalChunks = C.RG_MAX_CONCURRENT_CHUNKS
	}
	
	cManifest := C.rg_manifest_t{
		total_size:           C.uint64_t(config.BufferSize),
		chunk_size:           C.uint32_t(config.MaxChunkSize),
		encoding_type:        C.uint16_t(0x01),
		exposure_cadence_ms:  C.uint32_t(1),
		total_chunks:         C.uint32_t(totalChunks),
		version:              C.uint32_t(1),
	}
	
	// Set file ID
	fileID := fmt.Sprintf("test_surface_%d", time.Now().Unix())
	copy(cManifest.file_id[:], fileID)
	
	// Create surface with error checking
	surface := C.rg_create_surface(&cManifest)
	if surface == nil {
		log.Fatal("Failed to create Red Giant test surface")
	}
	
	processor := &TestProcessor{
		config:      config,
		metrics:     NewTestMetrics(),
		logger:      log.New(os.Stdout, "[RedGiant-Test] ", log.LstdFlags|log.Lshortfile),
		surface:     surface,
		fileStorage: make(map[string]*TestFile),
		workerPool:  make(chan struct{}, config.MaxWorkers),
	}
	
	processor.logger.Printf("🧪 Test processor initialized with C-core")
	return processor
}

// Process data with comprehensive error handling
func (tp *TestProcessor) ProcessDataSafe(data []byte, fileName string) (map[string]interface{}, error) {
	start := time.Now()
	atomic.AddInt64(&tp.metrics.ActiveConnections, 1)
	defer atomic.AddInt64(&tp.metrics.ActiveConnections, -1)
	
	// Validate input
	if len(data) == 0 {
		atomic.AddInt64(&tp.metrics.EmptyRequests, 1)
		return nil, fmt.Errorf("empty data provided")
	}
	
	if len(data) > tp.config.BufferSize {
		atomic.AddInt64(&tp.metrics.OversizedRequests, 1)
		return nil, fmt.Errorf("data too large: %d bytes (max: %d)", len(data), tp.config.BufferSize)
	}
	
	// Calculate optimal chunking
	chunkSize := tp.config.MaxChunkSize
	totalChunks := (len(data) + chunkSize - 1) / chunkSize
	
	if totalChunks > int(C.RG_MAX_CONCURRENT_CHUNKS) {
		return nil, fmt.Errorf("too many chunks: %d (max: %d)", totalChunks, C.RG_MAX_CONCURRENT_CHUNKS)
	}
	
	// Process chunks with worker pool
	numWorkers := tp.config.MaxWorkers
	if totalChunks < numWorkers {
		numWorkers = totalChunks
	}
	
	var wg sync.WaitGroup
	var processedChunks int64
	var processingErrors int64
	
	for worker := 0; worker < numWorkers; worker++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			
			// Acquire worker slot
			tp.workerPool <- struct{}{}
			defer func() { <-tp.workerPool }()
			
			start := workerID * (totalChunks / numWorkers)
			end := start + (totalChunks / numWorkers)
			if workerID == numWorkers-1 {
				end = totalChunks
			}
			
			for i := start; i < end; i++ {
				chunkStart := i * chunkSize
				chunkEnd := chunkStart + chunkSize
				if chunkEnd > len(data) {
					chunkEnd = len(data)
				}
				
				chunkData := data[chunkStart:chunkEnd]
				
				// Expose chunk with error checking
				success := C.rg_expose_chunk_fast(
					tp.surface,
					C.uint32_t(i),
					unsafe.Pointer(&chunkData[0]),
					C.uint32_t(len(chunkData)),
				)
				
				if bool(success) {
					atomic.AddInt64(&processedChunks, 1)
				} else {
					atomic.AddInt64(&processingErrors, 1)
					if tp.config.EnableDebug {
						tp.logger.Printf("Failed to expose chunk %d", i)
					}
				}
			}
		}(worker)
	}
	
	wg.Wait()
	
	if processingErrors > 0 {
		return nil, fmt.Errorf("failed to process %d chunks", processingErrors)
	}
	
	// Raise red flag
	C.rg_raise_red_flag(tp.surface)
	
	// Calculate performance metrics
	duration := time.Since(start)
	throughput := float64(len(data)) / duration.Seconds() / (1024 * 1024)
	
	// Update metrics
	tp.updateMetrics(int64(len(data)), processedChunks, duration, throughput)
	
	// Generate file info
	fileID := tp.generateFileID(data, fileName)
	
	// Store file
	testFile := &TestFile{
		ID:          fileID,
		Name:        fileName,
		Size:        int64(len(data)),
		Hash:        fileID,
		MD5Hash:     fmt.Sprintf("%x", md5.Sum(data)),
		Data:        make([]byte, len(data)),
		UploadedAt:  time.Now(),
		ContentType: http.DetectContentType(data),
		ChunkCount:  totalChunks,
		Metadata: map[string]string{
			"processing_time_ms": fmt.Sprintf("%d", duration.Milliseconds()),
			"throughput_mbps":    fmt.Sprintf("%.2f", throughput),
			"worker_count":       fmt.Sprintf("%d", numWorkers),
		},
	}
	copy(testFile.Data, data)
	
	tp.storageMu.Lock()
	tp.fileStorage[fileID] = testFile
	tp.storageMu.Unlock()
	
	return map[string]interface{}{
		"status":             "success",
		"file_id":            fileID,
		"file_name":          fileName,
		"bytes_processed":    len(data),
		"chunks_processed":   processedChunks,
		"processing_time_ms": duration.Milliseconds(),
		"throughput_mbps":    throughput,
		"content_type":       testFile.ContentType,
		"md5_hash":           testFile.MD5Hash,
		"worker_count":       numWorkers,
		"surface_complete":   bool(C.rg_is_complete(tp.surface)),
	}, nil
}

func (tp *TestProcessor) updateMetrics(bytes, chunks int64, duration time.Duration, throughput float64) {
	atomic.AddInt64(&tp.metrics.TotalRequests, 1)
	atomic.AddInt64(&tp.metrics.TotalBytes, bytes)
	atomic.AddInt64(&tp.metrics.TotalChunks, chunks)
	
	latencyMs := duration.Milliseconds()
	atomic.StoreInt64(&tp.metrics.AverageLatency, latencyMs)
	tp.metrics.LastRequestTime = time.Now()
	
	// Update min/max latency
	for {
		current := atomic.LoadInt64(&tp.metrics.MinLatency)
		if latencyMs >= current || atomic.CompareAndSwapInt64(&tp.metrics.MinLatency, current, latencyMs) {
			break
		}
	}
	
	for {
		current := atomic.LoadInt64(&tp.metrics.MaxLatency)
		if latencyMs <= current || atomic.CompareAndSwapInt64(&tp.metrics.MaxLatency, current, latencyMs) {
			break
		}
	}
	
	// Update max throughput
	if throughput > tp.metrics.MaxThroughput {
		tp.metrics.MaxThroughput = throughput
	}
	
	// Calculate success rate
	totalReqs := atomic.LoadInt64(&tp.metrics.TotalRequests)
	errors := atomic.LoadInt64(&tp.metrics.ErrorCount)
	if totalReqs > 0 {
		tp.metrics.SuccessRate = float64(totalReqs-errors) / float64(totalReqs) * 100.0
	}
}

func (tp *TestProcessor) generateFileID(data []byte, filename string) string {
	hash := sha256.Sum256(data)
	return fmt.Sprintf("%x", hash[:8])
}

// HTTP Handlers
func (tp *TestProcessor) handleUpload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		atomic.AddInt64(&tp.metrics.MalformedRequests, 1)
		return
	}
	
	// Get metadata
	fileName := r.Header.Get("X-File-Name")
	if fileName == "" {
		fileName = fmt.Sprintf("upload_%d", time.Now().Unix())
	}
	
	// Read data with size limit
	data, err := io.ReadAll(io.LimitReader(r.Body, int64(tp.config.BufferSize)))
	if err != nil {
		http.Error(w, "Failed to read data", http.StatusBadRequest)
		atomic.AddInt64(&tp.metrics.ErrorCount, 1)
		return
	}
	
	// Process data
	result, err := tp.ProcessDataSafe(data, fileName)
	if err != nil {
		http.Error(w, fmt.Sprintf("Processing failed: %v", err), http.StatusInternalServerError)
		atomic.AddInt64(&tp.metrics.ErrorCount, 1)
		return
	}
	
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(result)
}

func (tp *TestProcessor) handleDownload(w http.ResponseWriter, r *http.Request) {
	fileID := strings.TrimPrefix(r.URL.Path, "/download/")
	if fileID == "" {
		http.Error(w, "File ID required", http.StatusBadRequest)
		return
	}
	
	tp.storageMu.RLock()
	file, exists := tp.fileStorage[fileID]
	tp.storageMu.RUnlock()
	
	if !exists {
		http.Error(w, "File not found", http.StatusNotFound)
		return
	}
	
	w.Header().Set("Content-Type", file.ContentType)
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=%s", file.Name))
	w.Header().Set("Content-Length", fmt.Sprintf("%d", file.Size))
	w.Write(file.Data)
}

func (tp *TestProcessor) handleMetrics(w http.ResponseWriter, r *http.Request) {
	uptime := time.Since(tp.metrics.StartTime).Seconds()
	
	// Get C performance stats
	var cThroughput C.uint32_t
	cElapsed := C.rg_get_performance_stats(tp.surface, &cThroughput)
	
	metrics := map[string]interface{}{
		"total_requests":       atomic.LoadInt64(&tp.metrics.TotalRequests),
		"total_bytes":          atomic.LoadInt64(&tp.metrics.TotalBytes),
		"total_chunks":         atomic.LoadInt64(&tp.metrics.TotalChunks),
		"average_latency_ms":   atomic.LoadInt64(&tp.metrics.AverageLatency),
		"min_latency_ms":       atomic.LoadInt64(&tp.metrics.MinLatency),
		"max_latency_ms":       atomic.LoadInt64(&tp.metrics.MaxLatency),
		"error_count":          atomic.LoadInt64(&tp.metrics.ErrorCount),
		"success_rate":         tp.metrics.SuccessRate,
		"uptime_seconds":       int64(uptime),
		"max_throughput_mbps":  tp.metrics.MaxThroughput,
		"active_connections":   atomic.LoadInt64(&tp.metrics.ActiveConnections),
		"empty_requests":       atomic.LoadInt64(&tp.metrics.EmptyRequests),
		"oversized_requests":   atomic.LoadInt64(&tp.metrics.OversizedRequests),
		"malformed_requests":   atomic.LoadInt64(&tp.metrics.MalformedRequests),
		"timeout_requests":     atomic.LoadInt64(&tp.metrics.TimeoutRequests),
		"c_core_elapsed_ms":    int64(cElapsed),
		"c_core_throughput":    int64(cThroughput),
		"surface_complete":     bool(C.rg_is_complete(tp.surface)),
		"timestamp":            time.Now().Unix(),
	}
	
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(metrics)
}

func (tp *TestProcessor) handleFiles(w http.ResponseWriter, r *http.Request) {
	tp.storageMu.RLock()
	files := make([]*TestFile, 0, len(tp.fileStorage))
	for _, file := range tp.fileStorage {
		files = append(files, file)
	}
	tp.storageMu.RUnlock()
	
	response := map[string]interface{}{
		"files": files,
		"count": len(files),
	}
	
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)
}

func (tp *TestProcessor) handleHealth(w http.ResponseWriter, r *http.Request) {
	health := map[string]interface{}{
		"status":      "healthy",
		"version":     "1.0.0",
		"mode":        "test",
		"c_core":      tp.surface != nil,
		"uptime":      time.Since(tp.metrics.StartTime).Seconds(),
		"requests":    atomic.LoadInt64(&tp.metrics.TotalRequests),
		"errors":      atomic.LoadInt64(&tp.metrics.ErrorCount),
		"success_rate": tp.metrics.SuccessRate,
		"timestamp":   time.Now().Unix(),
	}
	
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(health)
}

func (tp *TestProcessor) handleRoot(w http.ResponseWriter, r *http.Request) {
	html := fmt.Sprintf(`<!DOCTYPE html>
<html>
<head>
    <title>Red Giant Protocol - Test Server</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #d32f2f; text-align: center; }
        .status { background: #e8f5e8; padding: 15px; border-radius: 5px; margin: 20px 0; }
        .endpoint { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #007bff; }
        .test { background: #fff3cd; padding: 10px; margin: 5px 0; border-radius: 3px; }
        pre { background: #263238; color: #fff; padding: 15px; border-radius: 5px; overflow-x: auto; }
        .metric { display: inline-block; margin: 10px; padding: 10px; background: #e3f2fd; border-radius: 5px; }
        .button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; margin: 5px; cursor: pointer; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🧪 Red Giant Protocol - Test Server</h1>
        
        <div class="status">
            <strong>Status:</strong> Running on port %d | 
            <strong>Workers:</strong> %d | 
            <strong>Max Chunk:</strong> %d KB | 
            <strong>Buffer Size:</strong> %d MB
        </div>
        
        <h2>📊 Quick Metrics</h2>
        <div class="metric">Requests: <strong id="requests">0</strong></div>
        <div class="metric">Bytes: <strong id="bytes">0</strong></div>
        <div class="metric">Errors: <strong id="errors">0</strong></div>
        <div class="metric">Success Rate: <strong id="success">100%%</strong></div>
        
        <h2>🔗 API Endpoints</h2>
        <div class="endpoint">
            <strong>POST /upload</strong><br>
            Upload data for exposure-based processing
        </div>
        <div class="endpoint">
            <strong>GET /download/{file_id}</strong><br>
            Download processed file by ID
        </div>
        <div class="endpoint">
            <strong>GET /metrics</strong><br>
            Get detailed performance metrics
        </div>
        <div class="endpoint">
            <strong>GET /files</strong><br>
            List all processed files
        </div>
        <div class="endpoint">
            <strong>GET /health</strong><br>
            Health check endpoint
        </div>
        
        <h2>🧪 Quick Tests</h2>
        <button class="button" onclick="testUpload()">Test Upload</button>
        <button class="button" onclick="testMetrics()">Get Metrics</button>
        <button class="button" onclick="testHealth()">Health Check</button>
        <button class="button" onclick="refreshMetrics()">Refresh</button>
        
        <div class="test">
            <h3>Postman/cURL Examples:</h3>
            <pre># Upload a file
curl -X POST http://localhost:%d/upload \\
  -H "X-File-Name: test.txt" \\
  --data "Red Giant Protocol Test Data"

# Get metrics
curl http://localhost:%d/metrics

# Health check
curl http://localhost:%d/health</pre>
        </div>
    </div>

    <script>
        async function testUpload() {
            try {
                const response = await fetch('/upload', {
                    method: 'POST',
                    headers: { 'X-File-Name': 'test.txt' },
                    body: 'Red Giant Protocol Test: ' + new Date().toISOString()
                });
                const result = await response.json();
                alert('Upload successful! File ID: ' + result.file_id);
                refreshMetrics();
            } catch (error) {
                alert('Upload failed: ' + error.message);
            }
        }
        
        async function testMetrics() {
            try {
                const response = await fetch('/metrics');
                const metrics = await response.json();
                alert(JSON.stringify(metrics, null, 2));
            } catch (error) {
                alert('Metrics failed: ' + error.message);
            }
        }
        
        async function testHealth() {
            try {
                const response = await fetch('/health');
                const health = await response.json();
                alert('Health: ' + health.status + '\\nUptime: ' + health.uptime + ' seconds');
            } catch (error) {
                alert('Health check failed: ' + error.message);
            }
        }
        
        async function refreshMetrics() {
            try {
                const response = await fetch('/metrics');
                const metrics = await response.json();
                document.getElementById('requests').textContent = metrics.total_requests;
                document.getElementById('bytes').textContent = (metrics.total_bytes / (1024*1024)).toFixed(2) + ' MB';
                document.getElementById('errors').textContent = metrics.error_count;
                document.getElementById('success').textContent = metrics.success_rate.toFixed(1) + '%%';
            } catch (error) {
                console.error('Failed to refresh metrics:', error);
            }
        }
        
        // Auto-refresh metrics every 5 seconds
        setInterval(refreshMetrics, 5000);
        refreshMetrics(); // Initial load
    </script>
</body>
</html>`, tp.config.Port, tp.config.MaxWorkers, tp.config.MaxChunkSize/1024, tp.config.BufferSize/(1024*1024), tp.config.Port, tp.config.Port, tp.config.Port)
	
	w.Header().Set("Content-Type", "text/html")
	w.Write([]byte(html))
}

func (tp *TestProcessor) Cleanup() {
	if tp.surface != nil {
		C.rg_destroy_surface(tp.surface)
		tp.surface = nil
	}
}

func main() {
	config := NewTestConfig()
	
	// Override with environment variables
	if port := os.Getenv("PORT"); port != "" {
		if p, err := strconv.Atoi(port); err == nil {
			config.Port = p
		}
	}
	
	processor := NewTestProcessor(config)
	defer processor.Cleanup()
	
	// Setup HTTP routes
	mux := http.NewServeMux()
	mux.HandleFunc("/", processor.handleRoot)
	mux.HandleFunc("/upload", processor.handleUpload)
	mux.HandleFunc("/download/", processor.handleDownload)
	mux.HandleFunc("/metrics", processor.handleMetrics)
	mux.HandleFunc("/files", processor.handleFiles)
	mux.HandleFunc("/health", processor.handleHealth)
	
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
	
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	
	go func() {
		<-sigChan
		processor.logger.Println("Shutting down test server...")
		
		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer shutdownCancel()
		
		server.Shutdown(shutdownCtx)
		cancel()
	}()
	
	processor.logger.Printf("🧪 Red Giant Test Server starting on %s:%d", config.Host, config.Port)
	processor.logger.Printf("🔗 Web interface: http://localhost:%d", config.Port)
	processor.logger.Printf("📊 Metrics: http://localhost:%d/metrics", config.Port)
	processor.logger.Printf("❤️  Health: http://localhost:%d/health", config.Port)
	
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		processor.logger.Fatalf("Server failed to start: %v", err)
	}
	
	<-ctx.Done()
	processor.logger.Println("Red Giant Test Server stopped")
}
