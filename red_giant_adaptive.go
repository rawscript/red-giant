//go:build adaptive
// +build adaptive

// Red Giant Protocol - Adaptive Multi-Format High-Performance Server
package main

/*
#cgo CFLAGS: -std=gnu99 -O3 -march=native -D_GNU_SOURCE
#include "red_giant.h"
#include "red_giant_wrapper.h"
#cgo LDFLAGS: -lm
*/
import "C"
import (
	"bytes"
	"compress/gzip"
	"context"
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"io"
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
	"unsafe"
)

// Adaptive processing modes based on content type and size
type ProcessingMode int

const (
	ModeRaw ProcessingMode = iota
	ModeJSON
	ModeXML
	ModeText
	ModeBinary
	ModeImage
	ModeVideo
	ModeAudio
	ModeCompressed
	ModeStreaming
)

// Content type detection and optimization
type ContentAnalyzer struct {
	ContentType  string
	Size         int64
	IsCompressed bool
	IsStreamable bool
	OptimalChunk int
	ProcessMode  ProcessingMode
	Encoding     string
}

// Adaptive configuration based on request type
type AdaptiveConfig struct {
	Port         int    `json:"port"`
	Host         string `json:"host"`
	MaxWorkers   int    `json:"max_workers"`
	MaxChunkSize int    `json:"max_chunk_size"`
	BufferSize   int    `json:"buffer_size"`
	LogLevel     string `json:"log_level"`

	// Adaptive settings
	EnableCompression    bool `json:"enable_compression"`
	EnableStreaming      bool `json:"enable_streaming"`
	AutoOptimize         bool `json:"auto_optimize"`
	MaxConcurrentStreams int  `json:"max_concurrent_streams"`
}

func NewAdaptiveConfig() *AdaptiveConfig {
	return &AdaptiveConfig{
		Port:                 8080,
		Host:                 "0.0.0.0",
		MaxWorkers:           runtime.NumCPU() * 2,
		MaxChunkSize:         256 * 1024,  // 256KB default
		BufferSize:           1024 * 1024, // 1MB
		LogLevel:             "INFO",
		EnableCompression:    true,
		EnableStreaming:      true,
		AutoOptimize:         true,
		MaxConcurrentStreams: 100,
	}
}

// Enhanced metrics with format-specific tracking
type AdaptiveMetrics struct {
	TotalRequests  int64
	TotalBytes     int64
	TotalChunks    int64
	AverageLatency int64
	ErrorCount     int64
	StartTime      time.Time

	// Format-specific metrics
	JSONRequests     int64
	BinaryRequests   int64
	StreamRequests   int64
	CompressedBytes  int64
	OptimizationHits int64

	mu sync.RWMutex
}

func NewAdaptiveMetrics() *AdaptiveMetrics {
	return &AdaptiveMetrics{
		StartTime: time.Now(),
	}
}

func (m *AdaptiveMetrics) RecordRequest(bytes int64, chunks int64, latency time.Duration, mode ProcessingMode) {
	atomic.AddInt64(&m.TotalRequests, 1)
	atomic.AddInt64(&m.TotalBytes, bytes)
	atomic.AddInt64(&m.TotalChunks, chunks)
	atomic.StoreInt64(&m.AverageLatency, latency.Milliseconds())

	// Track format-specific metrics
	switch mode {
	case ModeJSON:
		atomic.AddInt64(&m.JSONRequests, 1)
	case ModeBinary, ModeImage, ModeVideo, ModeAudio:
		atomic.AddInt64(&m.BinaryRequests, 1)
	case ModeStreaming:
		atomic.AddInt64(&m.StreamRequests, 1)
	}
}

func (m *AdaptiveMetrics) RecordCompression(originalSize, compressedSize int64) {
	atomic.AddInt64(&m.CompressedBytes, compressedSize)
	if compressedSize < originalSize {
		atomic.AddInt64(&m.OptimizationHits, 1)
	}
}

func (m *AdaptiveMetrics) GetSnapshot() map[string]interface{} {
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
		"json_requests":      atomic.LoadInt64(&m.JSONRequests),
		"binary_requests":    atomic.LoadInt64(&m.BinaryRequests),
		"stream_requests":    atomic.LoadInt64(&m.StreamRequests),
		"compressed_bytes":   atomic.LoadInt64(&m.CompressedBytes),
		"optimization_hits":  atomic.LoadInt64(&m.OptimizationHits),
		"timestamp":          time.Now().Unix(),
	}
}

// File storage with format-aware metadata
type AdaptiveFile struct {
	ID           string            `json:"id"`
	Name         string            `json:"name"`
	Size         int64             `json:"size"`
	Hash         string            `json:"hash"`
	PeerID       string            `json:"peer_id"`
	Data         []byte            `json:"-"`
	UploadedAt   time.Time         `json:"uploaded_at"`
	ContentType  string            `json:"content_type"`
	ProcessMode  ProcessingMode    `json:"process_mode"`
	IsCompressed bool              `json:"is_compressed"`
	Metadata     map[string]string `json:"metadata"`
}

// Adaptive Red Giant processor with format intelligence
type AdaptiveProcessor struct {
	config      *AdaptiveConfig
	workerPool  chan struct{}
	metrics     *AdaptiveMetrics
	logger      *log.Logger
	surface     *C.rg_exposure_surface_t
	fileStorage map[string]*AdaptiveFile
	storageMu   sync.RWMutex

	// Streaming support
	activeStreams map[string]*StreamSession
	streamsMu     sync.RWMutex
}

type StreamSession struct {
	ID        string
	StartTime time.Time
	BytesSent int64
	Active    bool
}

func NewAdaptiveProcessor(config *AdaptiveConfig) *AdaptiveProcessor {
	// Create C manifest for high-performance surface
	cManifest := C.rg_manifest_t{
		total_size:          C.uint64_t(config.BufferSize),
		chunk_size:          C.uint32_t(config.MaxChunkSize),
		encoding_type:       C.uint16_t(0x01),
		exposure_cadence_ms: C.uint32_t(1), // Ultra-fast
		total_chunks:        C.uint32_t((config.BufferSize + config.MaxChunkSize - 1) / config.MaxChunkSize),
	}

	// Initialize file ID
	fileID := fmt.Sprintf("rg_adaptive_%d", time.Now().Unix())
	fileIDBytes := []byte(fileID)
	for i := 0; i < len(fileIDBytes) && i < 64; i++ {
		cManifest.file_id[i] = C.char(fileIDBytes[i])
	}

	// Create high-performance C surface
	surface := C.rg_create_surface(&cManifest)
	if surface == nil {
		log.Fatal("Failed to create Red Giant C surface")
	}

	processor := &AdaptiveProcessor{
		config:        config,
		workerPool:    make(chan struct{}, config.MaxWorkers),
		metrics:       NewAdaptiveMetrics(),
		logger:        log.New(os.Stdout, "[RedGiant-Adaptive] ", log.LstdFlags),
		surface:       surface,
		fileStorage:   make(map[string]*AdaptiveFile),
		activeStreams: make(map[string]*StreamSession),
	}

	processor.logger.Printf("🚀 Adaptive Red Giant Protocol with C-Core initialized")
	return processor
}

// Intelligent content analysis for optimal processing
func (ap *AdaptiveProcessor) analyzeContent(data []byte, contentType string) *ContentAnalyzer {
	analyzer := &ContentAnalyzer{
		ContentType:  contentType,
		Size:         int64(len(data)),
		OptimalChunk: ap.config.MaxChunkSize,
	}

	// Detect content type if not provided
	if contentType == "" {
		analyzer.ContentType = http.DetectContentType(data)
	}

	// Determine processing mode and optimizations
	switch {
	case strings.Contains(analyzer.ContentType, "application/json"):
		analyzer.ProcessMode = ModeJSON
		analyzer.OptimalChunk = 64 * 1024 // Smaller chunks for JSON
		analyzer.Encoding = "utf-8"

	case strings.Contains(analyzer.ContentType, "text/"):
		analyzer.ProcessMode = ModeText
		analyzer.OptimalChunk = 128 * 1024
		analyzer.Encoding = "utf-8"

	case strings.Contains(analyzer.ContentType, "image/"):
		analyzer.ProcessMode = ModeImage
		analyzer.OptimalChunk = 512 * 1024                // Larger chunks for images
		analyzer.IsStreamable = analyzer.Size > 1024*1024 // Stream if > 1MB

	case strings.Contains(analyzer.ContentType, "video/"):
		analyzer.ProcessMode = ModeVideo
		analyzer.OptimalChunk = 1024 * 1024 // 1MB chunks for video
		analyzer.IsStreamable = true

	case strings.Contains(analyzer.ContentType, "audio/"):
		analyzer.ProcessMode = ModeAudio
		analyzer.OptimalChunk = 256 * 1024
		analyzer.IsStreamable = analyzer.Size > 512*1024

	case strings.Contains(analyzer.ContentType, "application/gzip") ||
		strings.Contains(analyzer.ContentType, "application/zip"):
		analyzer.ProcessMode = ModeCompressed
		analyzer.IsCompressed = true
		analyzer.OptimalChunk = 1024 * 1024 // Large chunks for compressed data

	default:
		analyzer.ProcessMode = ModeBinary
		analyzer.OptimalChunk = ap.config.MaxChunkSize
	}

	// Auto-detect compression opportunity
	if !analyzer.IsCompressed && analyzer.Size > 1024 && ap.config.EnableCompression {
		// Test compression ratio on first 1KB
		testSize := 1024
		if len(data) < testSize {
			testSize = len(data)
		}

		var buf bytes.Buffer
		gz := gzip.NewWriter(&buf)
		gz.Write(data[:testSize])
		gz.Close()

		compressionRatio := float64(buf.Len()) / float64(testSize)
		if compressionRatio < 0.8 { // If compression saves 20%+
			analyzer.IsCompressed = true
		}
	}

	return analyzer
}

// Adaptive high-performance processing with format optimization
func (ap *AdaptiveProcessor) ProcessDataAdaptive(data []byte, analyzer *ContentAnalyzer) (int, time.Duration, error) {
	start := time.Now()

	// Use optimal chunk size based on content analysis
	chunkSize := analyzer.OptimalChunk
	totalChunks := (len(data) + chunkSize - 1) / chunkSize

	// Adaptive worker count based on content type and size
	numWorkers := ap.config.MaxWorkers
	if analyzer.ProcessMode == ModeStreaming || analyzer.IsStreamable {
		numWorkers = min(numWorkers, 4) // Limit workers for streaming
	} else if analyzer.Size < 64*1024 {
		numWorkers = min(numWorkers, 2) // Fewer workers for small files
	}

	numWorkers = min(numWorkers, totalChunks)
	chunksPerWorker := totalChunks / numWorkers

	var wg sync.WaitGroup
	var processedChunks int64

	for worker := 0; worker < numWorkers; worker++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()

			// Acquire worker slot
			ap.workerPool <- struct{}{}
			defer func() { <-ap.workerPool }()

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

				chunkData := data[chunkStart:chunkEnd]

				// Use C fast exposure function with adaptive chunk size
				success := C.rg_expose_chunk_fast(
					ap.surface,
					C.uint32_t(i),
					unsafe.Pointer(&chunkData[0]),
					C.uint32_t(len(chunkData)),
				)

				if bool(success) {
					atomic.AddInt64(&processedChunks, 1)
				}
			}
		}(worker)
	}

	wg.Wait()

	// Raise red flag for completion
	C.rg_raise_red_flag(ap.surface)

	duration := time.Since(start)
	return int(processedChunks), duration, nil
}

// Adaptive upload handler with format intelligence
func (ap *AdaptiveProcessor) handleAdaptiveUpload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Get request metadata
	peerID := r.Header.Get("X-Peer-ID")
	fileName := r.Header.Get("X-File-Name")
	contentType := r.Header.Get("Content-Type")

	if peerID == "" {
		peerID = "anonymous_" + fmt.Sprintf("%d", time.Now().Unix())
	}
	if fileName == "" {
		fileName = "upload_" + fmt.Sprintf("%d", time.Now().Unix())
	}

	// Read data with size limit
	data, err := io.ReadAll(io.LimitReader(r.Body, int64(ap.config.BufferSize)))
	if err != nil {
		ap.logger.Printf("Error reading data: %v", err) // Log the error
		http.Error(w, "Failed to read data", http.StatusBadRequest)
		ap.metrics.RecordRequest(0, 0, 0, ModeRaw)
		return
	}

	// Analyze content for optimal processing
	analyzer := ap.analyzeContent(data, contentType)

	// Apply compression if beneficial
	originalSize := len(data)
	if analyzer.IsCompressed && ap.config.EnableCompression {
		var buf bytes.Buffer
		gz := gzip.NewWriter(&buf)
		gz.Write(data)
		gz.Close()

		if buf.Len() < originalSize {
			data = buf.Bytes()
			ap.metrics.RecordCompression(int64(originalSize), int64(len(data)))
		}
	}

	// Process with adaptive Red Giant protocol
	chunks, duration, err := ap.ProcessDataAdaptive(data, analyzer)
	if err != nil {
		ap.logger.Printf("Processing failed: %v", err) // Log the error
		http.Error(w, "Processing failed", http.StatusInternalServerError)
		ap.metrics.RecordRequest(0, 0, 0, analyzer.ProcessMode)
		return
	}

	// Generate file ID and store
	fileID := ap.generateFileID(data, fileName)

	adaptiveFile := &AdaptiveFile{
		ID:           fileID,
		Name:         fileName,
		Size:         int64(len(data)),
		Hash:         fileID,
		PeerID:       peerID,
		Data:         make([]byte, len(data)),
		UploadedAt:   time.Now(),
		ContentType:  analyzer.ContentType,
		ProcessMode:  analyzer.ProcessMode,
		IsCompressed: analyzer.IsCompressed,
		Metadata: map[string]string{
			"original_size": fmt.Sprintf("%d", originalSize),
			"optimal_chunk": fmt.Sprintf("%d", analyzer.OptimalChunk),
			"encoding":      analyzer.Encoding,
		},
	}
	copy(adaptiveFile.Data, data)

	// Store in memory and disk
	ap.storageMu.Lock()
	ap.fileStorage[fileID] = adaptiveFile
	ap.storageMu.Unlock()

	// Save to disk with format-specific directory
	formatDir := strings.Split(analyzer.ContentType, "/")[0]
	diskPath := fmt.Sprintf("./storage/%s/%s_%s", formatDir, fileID, fileName)
	os.MkdirAll(filepath.Dir(diskPath), 0755)
	os.WriteFile(diskPath, data, 0644)

	// Record metrics
	ap.metrics.RecordRequest(int64(len(data)), int64(chunks), duration, analyzer.ProcessMode)
	throughput := float64(len(data)) / duration.Seconds() / (1024 * 1024)

	// Send adaptive response
	response := map[string]interface{}{
		"status":             "success",
		"file_id":            fileID,
		"bytes_processed":    len(data),
		"original_size":      originalSize,
		"chunks_processed":   chunks,
		"processing_time_ms": duration.Milliseconds(),
		"throughput_mbps":    throughput,
		"content_type":       analyzer.ContentType,
		"process_mode":       analyzer.ProcessMode,
		"is_compressed":      analyzer.IsCompressed,
		"optimal_chunk_size": analyzer.OptimalChunk,
		"message":            fmt.Sprintf("File '%s' processed with adaptive optimization", fileName),
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)

	ap.logger.Printf("📤 Adaptive upload: %s (%d bytes, %s, %.2f MB/s) by %s",
		fileName, len(data), analyzer.ContentType, throughput, peerID)
}

// Generate file ID
func (ap *AdaptiveProcessor) generateFileID(data []byte, filename string) string {
	hash := sha256.Sum256(data)
	return fmt.Sprintf("%x", hash[:8])
}

// Cleanup C resources
func (ap *AdaptiveProcessor) Cleanup() {
	if ap.surface != nil {
		C.rg_destroy_surface(ap.surface)
		ap.surface = nil
	}
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func main() {
	// Load adaptive configuration
	config := NewAdaptiveConfig()

	// Override with environment variables
	if port := os.Getenv("RED_GIANT_PORT"); port != "" {
		if p, err := strconv.Atoi(port); err == nil {
			config.Port = p
		}
	}

	// Create adaptive processor with C-Core
	processor := NewAdaptiveProcessor(config)
	defer processor.Cleanup()

	// Initialize storage
	os.MkdirAll("./storage", 0755)

	// Setup HTTP routes
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		html := `<!DOCTYPE html>
<html>
<head>
    <title>Red Giant Protocol - Adaptive Multi-Format Server</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #d32f2f; text-align: center; }
        .feature { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #d32f2f; }
        .format { background: #e3f2fd; padding: 10px; margin: 5px 0; border-radius: 3px; }
        pre { background: #263238; color: #fff; padding: 15px; border-radius: 5px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 Red Giant Protocol - Adaptive Multi-Format Server</h1>
        <p><strong>Status:</strong> Running | <strong>Version:</strong> 2.0.0 | <strong>Mode:</strong> Adaptive C-Core</p>

        <h2>🎯 Adaptive Format Support</h2>
        <div class="format">📄 <strong>JSON/Text:</strong> Optimized 64KB chunks, UTF-8 encoding</div>
        <div class="format">🖼️ <strong>Images:</strong> 512KB chunks, streaming for large files</div>
        <div class="format">🎥 <strong>Video/Audio:</strong> 1MB chunks, automatic streaming</div>
        <div class="format">📦 <strong>Compressed:</strong> Large chunks, skip re-compression</div>
        <div class="format">⚡ <strong>Binary:</strong> Adaptive chunk sizing based on content</div>

        <h2>🚀 Intelligent Features</h2>
        <div class="feature">
            <strong>Auto-Optimization:</strong> Automatically detects content type and applies optimal processing
        </div>
        <div class="feature">
            <strong>Smart Compression:</strong> Compresses data only when beneficial
        </div>
        <div class="feature">
            <strong>Adaptive Chunking:</strong> Chunk size optimized per content type
        </div>
        <div class="feature">
            <strong>Streaming Support:</strong> Automatic streaming for large media files
        </div>

        <h2>🧪 Test Different Formats</h2>
        <pre># JSON data
curl -X POST http://localhost:8080/upload \
     -H "Content-Type: application/json" \
     -H "X-File-Name: data.json" \
     --data '{"message": "Red Giant Protocol"}'

# Image upload
go run red_giant_peer.go upload image.jpg

# Video streaming
go run red_giant_peer.go upload video.mp4

# Text processing
go run red_giant_peer.go upload document.txt</pre>
    </div>
</body>
</html>`
		w.Header().Set("Content-Type", "text/html")
		w.Write([]byte(html))
	})

	mux.HandleFunc("/upload", processor.handleAdaptiveUpload)
	mux.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
		metrics := processor.metrics.GetSnapshot()
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(metrics)
	})
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		health := map[string]interface{}{
			"status":    "healthy",
			"version":   "2.0.0",
			"mode":      "adaptive",
			"timestamp": time.Now().Unix(),
			"uptime":    time.Since(processor.metrics.StartTime).Seconds(),
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(health)
	})

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

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigChan
		processor.logger.Println("Shutting down adaptive server...")

		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer shutdownCancel()

		server.Shutdown(shutdownCtx)
		cancel()
	}()

	// Start server
	processor.logger.Printf("🚀 Red Giant Adaptive Protocol Server starting on %s:%d", config.Host, config.Port)
	processor.logger.Printf("📊 Configuration: Workers=%d, Adaptive=ON, Compression=%v",
		config.MaxWorkers, config.EnableCompression)
	processor.logger.Printf("🎯 Supports: JSON, XML, Images, Video, Audio, Binary, Streaming")
	processor.logger.Printf("🌐 Web interface: http://localhost:%d", config.Port)

	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		processor.logger.Fatalf("Server failed to start: %v", err)
	}

	<-ctx.Done()
	processor.logger.Println("Red Giant Adaptive Protocol Server stopped")
}
