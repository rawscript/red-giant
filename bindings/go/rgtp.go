// Package rgtp provides Go bindings for the Red Giant Transport Protocol (RGTP)
//
// RGTP is a Layer 4 transport protocol that implements exposure-based data transmission,
// offering significant advantages over TCP for many use cases including multicast,
// resume capability, and adaptive flow control.
//
// Example usage:
//
//	// Expose data
//	session, err := rgtp.NewSession(&rgtp.Config{
//		Port:         9999,
//		AdaptiveMode: true,
//	})
//	if err != nil {
//		log.Fatal(err)
//	}
//	defer session.Close()
//
//	err = session.ExposeFile("large_file.bin")
//	if err != nil {
//		log.Fatal(err)
//	}
//
//	// Pull data
//	client, err := rgtp.NewClient(nil)
//	if err != nil {
//		log.Fatal(err)
//	}
//	defer client.Close()
//
//	err = client.PullToFile("192.168.1.100", 9999, "downloaded.bin")
//	if err != nil {
//		log.Fatal(err)
//	}
package rgtp

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../lib -lrgtp -lpthread -lm

#include <stdlib.h>
#include "rgtp/rgtp_sdk.h"

// Callback function wrappers for Go
extern void go_progress_callback(size_t bytes_transferred, size_t total_bytes, void* user_data);
extern void go_error_callback(int error_code, char* error_message, void* user_data);

// Helper function to set callbacks
static void set_progress_callback(rgtp_config_t* config, void* user_data) {
    config->progress_cb = (rgtp_progress_callback_t)go_progress_callback;
    config->user_data = user_data;
}

static void set_error_callback(rgtp_config_t* config, void* user_data) {
    config->error_cb = (rgtp_error_callback_t)go_error_callback;
    config->user_data = user_data;
}
*/
import "C"
import (
	"errors"
	"fmt"
	"runtime"
	"sync"
	"time"
	"unsafe"
)

// Initialize RGTP library once
var initOnce sync.Once
var initError error

func init() {
	initOnce.Do(func() {
		if C.rgtp_init() != 0 {
			initError = errors.New("failed to initialize RGTP library")
		}
	})
}

// Config represents RGTP configuration options
type Config struct {
	ChunkSize        uint32        // Chunk size in bytes (0 = auto)
	ExposureRate     uint32        // Initial exposure rate (chunks/sec)
	AdaptiveMode     bool          // Enable adaptive rate control
	EnableCompression bool         // Enable data compression
	EnableEncryption bool          // Enable encryption
	Port             uint16        // Port number (0 = auto)
	Timeout          time.Duration // Operation timeout
	ProgressCallback func(bytesTransferred, totalBytes uint64) // Progress callback
	ErrorCallback    func(errorCode int, errorMessage string) // Error callback
}

// Stats represents transfer statistics
type Stats struct {
	BytesTransferred     uint64        // Bytes successfully transferred
	TotalBytes          uint64        // Total bytes in transfer
	ThroughputMbps      float64       // Current throughput in MB/s
	AvgThroughputMbps   float64       // Average throughput in MB/s
	ChunksTransferred   uint32        // Number of chunks transferred
	TotalChunks         uint32        // Total number of chunks
	Retransmissions     uint32        // Number of retransmissions
	CompletionPercent   float64       // Completion percentage (0-100)
	Elapsed             time.Duration // Elapsed time
	EstimatedRemaining  time.Duration // Estimated remaining time
}

// EfficiencyPercent calculates transfer efficiency
func (s *Stats) EfficiencyPercent() float64 {
	if s.ChunksTransferred == 0 {
		return 100.0
	}
	totalAttempts := s.ChunksTransferred + s.Retransmissions
	return float64(s.ChunksTransferred) / float64(totalAttempts) * 100.0
}

// Session represents an RGTP session for exposing data
type Session struct {
	handle   unsafe.Pointer
	config   *Config
	closed   bool
	mutex    sync.Mutex
	callbackData *callbackData
}

// Client represents an RGTP client for pulling data
type Client struct {
	handle   unsafe.Pointer
	config   *Config
	closed   bool
	mutex    sync.Mutex
	callbackData *callbackData
}

// callbackData holds Go callback functions and prevents garbage collection
type callbackData struct {
	progressCallback func(uint64, uint64)
	errorCallback    func(int, string)
}

// Global callback registry to prevent garbage collection
var callbackRegistry = make(map[unsafe.Pointer]*callbackData)
var callbackMutex sync.RWMutex

// NewSession creates a new RGTP session with optional configuration
func NewSession(config *Config) (*Session, error) {
	if initError != nil {
		return nil, initError
	}

	session := &Session{
		config: config,
	}

	if config == nil {
		// Use default configuration
		session.handle = C.rgtp_session_create()
	} else {
		// Convert Go config to C config
		cConfig := session.configToC(config)
		session.handle = C.rgtp_session_create_with_config(&cConfig)
	}

	if session.handle == nil {
		return nil, errors.New("failed to create RGTP session")
	}

	// Set up callbacks if provided
	if config != nil && (config.ProgressCallback != nil || config.ErrorCallback != nil) {
		session.callbackData = &callbackData{
			progressCallback: config.ProgressCallback,
			errorCallback:    config.ErrorCallback,
		}
		
		callbackMutex.Lock()
		callbackRegistry[session.handle] = session.callbackData
		callbackMutex.Unlock()
	}

	// Set finalizer to ensure cleanup
	runtime.SetFinalizer(session, (*Session).Close)

	return session, nil
}

// NewClient creates a new RGTP client with optional configuration
func NewClient(config *Config) (*Client, error) {
	if initError != nil {
		return nil, initError
	}

	client := &Client{
		config: config,
	}

	if config == nil {
		// Use default configuration
		client.handle = C.rgtp_client_create()
	} else {
		// Convert Go config to C config
		cConfig := client.configToC(config)
		client.handle = C.rgtp_client_create_with_config(&cConfig)
	}

	if client.handle == nil {
		return nil, errors.New("failed to create RGTP client")
	}

	// Set up callbacks if provided
	if config != nil && (config.ProgressCallback != nil || config.ErrorCallback != nil) {
		client.callbackData = &callbackData{
			progressCallback: config.ProgressCallback,
			errorCallback:    config.ErrorCallback,
		}
		
		callbackMutex.Lock()
		callbackRegistry[client.handle] = client.callbackData
		callbackMutex.Unlock()
	}

	// Set finalizer to ensure cleanup
	runtime.SetFinalizer(client, (*Client).Close)

	return client, nil
}

// ExposeFile exposes a file for pulling
func (s *Session) ExposeFile(filename string) error {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if s.closed {
		return errors.New("session is closed")
	}

	cFilename := C.CString(filename)
	defer C.free(unsafe.Pointer(cFilename))

	result := C.rgtp_session_expose_file(s.handle, cFilename)
	if result != 0 {
		return fmt.Errorf("failed to expose file: %s", filename)
	}

	return nil
}

// WaitComplete waits for exposure to complete
func (s *Session) WaitComplete() error {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if s.closed {
		return errors.New("session is closed")
	}

	result := C.rgtp_session_wait_complete(s.handle)
	if result != 0 {
		return errors.New("exposure failed or timed out")
	}

	return nil
}

// GetStats returns current transfer statistics
func (s *Session) GetStats() (*Stats, error) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if s.closed {
		return nil, errors.New("session is closed")
	}

	var cStats C.rgtp_stats_t
	result := C.rgtp_session_get_stats(s.handle, &cStats)
	if result != 0 {
		return nil, errors.New("failed to get statistics")
	}

	return s.statsFromC(&cStats), nil
}

// Cancel cancels the ongoing exposure
func (s *Session) Cancel() error {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if s.closed {
		return errors.New("session is closed")
	}

	result := C.rgtp_session_cancel(s.handle)
	if result != 0 {
		return errors.New("failed to cancel session")
	}

	return nil
}

// Close closes the session and frees resources
func (s *Session) Close() error {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	if s.closed {
		return nil
	}

	if s.handle != nil {
		// Remove from callback registry
		callbackMutex.Lock()
		delete(callbackRegistry, s.handle)
		callbackMutex.Unlock()

		C.rgtp_session_destroy(s.handle)
		s.handle = nil
	}

	s.closed = true
	runtime.SetFinalizer(s, nil)

	return nil
}

// PullToFile pulls data from remote host and saves to file
func (c *Client) PullToFile(host string, port uint16, filename string) error {
	c.mutex.Lock()
	defer c.mutex.Unlock()

	if c.closed {
		return errors.New("client is closed")
	}

	cHost := C.CString(host)
	defer C.free(unsafe.Pointer(cHost))

	cFilename := C.CString(filename)
	defer C.free(unsafe.Pointer(cFilename))

	result := C.rgtp_client_pull_to_file(c.handle, cHost, C.uint16_t(port), cFilename)
	if result != 0 {
		return fmt.Errorf("failed to pull data from %s:%d", host, port)
	}

	return nil
}

// GetStats returns current transfer statistics
func (c *Client) GetStats() (*Stats, error) {
	c.mutex.Lock()
	defer c.mutex.Unlock()

	if c.closed {
		return nil, errors.New("client is closed")
	}

	var cStats C.rgtp_stats_t
	result := C.rgtp_client_get_stats(c.handle, &cStats)
	if result != 0 {
		return nil, errors.New("failed to get statistics")
	}

	return c.statsFromC(&cStats), nil
}

// Cancel cancels the ongoing pull operation
func (c *Client) Cancel() error {
	c.mutex.Lock()
	defer c.mutex.Unlock()

	if c.closed {
		return errors.New("client is closed")
	}

	result := C.rgtp_client_cancel(c.handle)
	if result != 0 {
		return errors.New("failed to cancel client")
	}

	return nil
}

// Close closes the client and frees resources
func (c *Client) Close() error {
	c.mutex.Lock()
	defer c.mutex.Unlock()

	if c.closed {
		return nil
	}

	if c.handle != nil {
		// Remove from callback registry
		callbackMutex.Lock()
		delete(callbackRegistry, c.handle)
		callbackMutex.Unlock()

		C.rgtp_client_destroy(c.handle)
		c.handle = nil
	}

	c.closed = true
	runtime.SetFinalizer(c, nil)

	return nil
}

// Helper methods for config conversion
func (s *Session) configToC(config *Config) C.rgtp_config_t {
	var cConfig C.rgtp_config_t

	C.rgtp_config_default(&cConfig)

	if config != nil {
		if config.ChunkSize > 0 {
			cConfig.chunk_size = C.uint32_t(config.ChunkSize)
		}
		if config.ExposureRate > 0 {
			cConfig.exposure_rate = C.uint32_t(config.ExposureRate)
		}
		cConfig.adaptive_mode = C.bool(config.AdaptiveMode)
		cConfig.enable_compression = C.bool(config.EnableCompression)
		cConfig.enable_encryption = C.bool(config.EnableEncryption)
		if config.Port > 0 {
			cConfig.port = C.uint16_t(config.Port)
		}
		if config.Timeout > 0 {
			cConfig.timeout_ms = C.int(config.Timeout.Milliseconds())
		}

		// Set up callbacks
		if config.ProgressCallback != nil {
			C.set_progress_callback(&cConfig, s.handle)
		}
		if config.ErrorCallback != nil {
			C.set_error_callback(&cConfig, s.handle)
		}
	}

	return cConfig
}

func (c *Client) configToC(config *Config) C.rgtp_config_t {
	var cConfig C.rgtp_config_t

	C.rgtp_config_default(&cConfig)

	if config != nil {
		if config.ChunkSize > 0 {
			cConfig.chunk_size = C.uint32_t(config.ChunkSize)
		}
		if config.ExposureRate > 0 {
			cConfig.exposure_rate = C.uint32_t(config.ExposureRate)
		}
		cConfig.adaptive_mode = C.bool(config.AdaptiveMode)
		cConfig.enable_compression = C.bool(config.EnableCompression)
		cConfig.enable_encryption = C.bool(config.EnableEncryption)
		if config.Port > 0 {
			cConfig.port = C.uint16_t(config.Port)
		}
		if config.Timeout > 0 {
			cConfig.timeout_ms = C.int(config.Timeout.Milliseconds())
		}

		// Set up callbacks
		if config.ProgressCallback != nil {
			C.set_progress_callback(&cConfig, c.handle)
		}
		if config.ErrorCallback != nil {
			C.set_error_callback(&cConfig, c.handle)
		}
	}

	return cConfig
}

func (s *Session) statsFromC(cStats *C.rgtp_stats_t) *Stats {
	return &Stats{
		BytesTransferred:   uint64(cStats.bytes_transferred),
		TotalBytes:        uint64(cStats.total_bytes),
		ThroughputMbps:    float64(cStats.throughput_mbps),
		AvgThroughputMbps: float64(cStats.avg_throughput_mbps),
		ChunksTransferred: uint32(cStats.chunks_transferred),
		TotalChunks:       uint32(cStats.total_chunks),
		Retransmissions:   uint32(cStats.retransmissions),
		CompletionPercent: float64(cStats.completion_percent),
		Elapsed:           time.Duration(cStats.elapsed_ms) * time.Millisecond,
		EstimatedRemaining: time.Duration(cStats.estimated_remaining_ms) * time.Millisecond,
	}
}

func (c *Client) statsFromC(cStats *C.rgtp_stats_t) *Stats {
	return &Stats{
		BytesTransferred:   uint64(cStats.bytes_transferred),
		TotalBytes:        uint64(cStats.total_bytes),
		ThroughputMbps:    float64(cStats.throughput_mbps),
		AvgThroughputMbps: float64(cStats.avg_throughput_mbps),
		ChunksTransferred: uint32(cStats.chunks_transferred),
		TotalChunks:       uint32(cStats.total_chunks),
		Retransmissions:   uint32(cStats.retransmissions),
		CompletionPercent: float64(cStats.completion_percent),
		Elapsed:           time.Duration(cStats.elapsed_ms) * time.Millisecond,
		EstimatedRemaining: time.Duration(cStats.estimated_remaining_ms) * time.Millisecond,
	}
}

// Convenience functions

// SendFile sends a file using RGTP (convenience function)
func SendFile(filename, host string, port uint16, config *Config) (*Stats, error) {
	session, err := NewSession(config)
	if err != nil {
		return nil, err
	}
	defer session.Close()

	err = session.ExposeFile(filename)
	if err != nil {
		return nil, err
	}

	err = session.WaitComplete()
	if err != nil {
		return nil, err
	}

	return session.GetStats()
}

// ReceiveFile receives a file using RGTP (convenience function)
func ReceiveFile(host string, port uint16, filename string, config *Config) (*Stats, error) {
	client, err := NewClient(config)
	if err != nil {
		return nil, err
	}
	defer client.Close()

	err = client.PullToFile(host, port, filename)
	if err != nil {
		return nil, err
	}

	return client.GetStats()
}

// Configuration helpers

// DefaultConfig returns a default configuration
func DefaultConfig() *Config {
	return &Config{
		AdaptiveMode: true,
		Timeout:      30 * time.Second,
	}
}

// LANConfig returns configuration optimized for LAN networks
func LANConfig() *Config {
	config := DefaultConfig()
	config.ChunkSize = 1024 * 1024 // 1MB chunks
	config.ExposureRate = 10000    // High rate for LAN
	return config
}

// WANConfig returns configuration optimized for WAN networks
func WANConfig() *Config {
	config := DefaultConfig()
	config.ChunkSize = 64 * 1024 // 64KB chunks
	config.ExposureRate = 1000   // Conservative rate for WAN
	config.Timeout = 60 * time.Second
	return config
}

// MobileConfig returns configuration optimized for mobile networks
func MobileConfig() *Config {
	config := DefaultConfig()
	config.ChunkSize = 16 * 1024 // 16KB chunks
	config.ExposureRate = 100    // Very conservative for mobile
	config.Timeout = 120 * time.Second
	return config
}

// Version returns the RGTP library version
func Version() string {
	cVersion := C.rgtp_version()
	return C.GoString(cVersion)
}

// Callback bridge functions (called from C)

//export go_progress_callback
func go_progress_callback(bytesTransferred C.size_t, totalBytes C.size_t, userData unsafe.Pointer) {
	callbackMutex.RLock()
	data, exists := callbackRegistry[userData]
	callbackMutex.RUnlock()

	if exists && data.progressCallback != nil {
		data.progressCallback(uint64(bytesTransferred), uint64(totalBytes))
	}
}

//export go_error_callback
func go_error_callback(errorCode C.int, errorMessage *C.char, userData unsafe.Pointer) {
	callbackMutex.RLock()
	data, exists := callbackRegistry[userData]
	callbackMutex.RUnlock()

	if exists && data.errorCallback != nil {
		message := C.GoString(errorMessage)
		data.errorCallback(int(errorCode), message)
	}
}