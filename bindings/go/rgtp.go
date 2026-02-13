package rgtp

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../src/core -lrgtp

#include "rgtp/rgtp.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// Config represents RGTP configuration
type Config struct {
	ChunkSize         uint32
	ExposureRate      uint32
	AdaptiveMode      bool
	EnableCompression bool
	EnableEncryption  bool
	Port              uint16
	TimeoutMs         int
	UserData          unsafe.Pointer
}

// Stats represents RGTP statistics
type Stats struct {
	BytesSent         uint64
	BytesReceived     uint64
	ChunksSent        uint32
	ChunksReceived    uint32
	PacketLossRate    float32
	RttMs             int
	PacketsLost       uint32
	Retransmissions   uint32
	AvgThroughputMbps float32
	CompletionPercent float32
	ActiveConnections uint32
}

// Session represents an RGTP session
type Session struct {
	cSession *C.rgtp_session_t
	config   *Config
}

// Client represents an RGTP client
type Client struct {
	cClient *C.rgtp_client_t
	config  *Config
}

// Surface represents an RGTP surface
type Surface struct {
	cSurface *C.rgtp_surface_t
}

// Initialize initializes the RGTP library
func Initialize() error {
	result := C.rgtp_init()
	if result != 0 {
		return fmt.Errorf("failed to initialize RGTP: %d", int(result))
	}
	return nil
}

// Cleanup cleans up the RGTP library
func Cleanup() {
	C.rgtp_cleanup()
}

// Version returns the RGTP version
func Version() string {
	version := C.rgtp_version()
	return C.GoString(version)
}

// CreateConfig creates a new RGTP configuration
func CreateConfig() *Config {
	return &Config{
		ChunkSize:         1024 * 1024, // 1MB default
		ExposureRate:      1000,        // 1000 chunks/sec default
		AdaptiveMode:      true,
		EnableCompression: false,
		EnableEncryption:  false,
		Port:              0,     // Auto-select
		TimeoutMs:         30000, // 30 seconds default
		UserData:          nil,
	}
}

// CreateSession creates a new RGTP session
func CreateSession(config *Config) (*Session, error) {
	if config == nil {
		return nil, fmt.Errorf("config cannot be nil")
	}

	cConfig := C.rgtp_config_t{
		chunk_size:         C.uint32_t(config.ChunkSize),
		exposure_rate:      C.uint32_t(config.ExposureRate),
		adaptive_mode:      C.bool(config.AdaptiveMode),
		enable_compression: C.bool(config.EnableCompression),
		enable_encryption:  C.bool(config.EnableEncryption),
		port:               C.uint16_t(config.Port),
		timeout_ms:         C.int(config.TimeoutMs),
		user_data:          config.UserData,
	}

	cSession := C.rgtp_session_create(&cConfig)
	if cSession == nil {
		return nil, fmt.Errorf("failed to create session")
	}

	session := &Session{
		cSession: cSession,
		config:   config,
	}

	return session, nil
}

// ExposeFile exposes a file through the session
func (s *Session) ExposeFile(filename string) error {
	if s.cSession == nil {
		return fmt.Errorf("invalid session")
	}

	cFilename := C.CString(filename)
	defer C.free(unsafe.Pointer(cFilename))

	result := C.rgtp_session_expose_file(s.cSession, cFilename)
	if result != 0 {
		return fmt.Errorf("failed to expose file: %d", int(result))
	}

	return nil
}

// WaitComplete waits for the session to complete
func (s *Session) WaitComplete() error {
	if s.cSession == nil {
		return fmt.Errorf("invalid session")
	}

	result := C.rgtp_session_wait_complete(s.cSession)
	if result != 0 {
		return fmt.Errorf("session wait failed: %d", int(result))
	}

	return nil
}

// GetStats gets session statistics
func (s *Session) GetStats() (*Stats, error) {
	if s.cSession == nil {
		return nil, fmt.Errorf("invalid session")
	}

	var cStats C.rgtp_stats_t
	result := C.rgtp_session_get_stats(s.cSession, &cStats)
	if result != 0 {
		return nil, fmt.Errorf("failed to get stats: %d", int(result))
	}

	stats := &Stats{
		BytesSent:         uint64(cStats.bytes_sent),
		BytesReceived:     uint64(cStats.bytes_received),
		ChunksSent:        uint32(cStats.chunks_sent),
		ChunksReceived:    uint32(cStats.chunks_received),
		PacketLossRate:    float32(cStats.packet_loss_rate),
		RttMs:             int(cStats.rtt_ms),
		PacketsLost:       uint32(cStats.packets_lost),
		Retransmissions:   uint32(cStats.retransmissions),
		AvgThroughputMbps: float32(cStats.avg_throughput_mbps),
		CompletionPercent: float32(cStats.completion_percent),
		ActiveConnections: uint32(cStats.active_connections),
	}

	return stats, nil
}

// Destroy destroys the session
func (s *Session) Destroy() {
	if s.cSession != nil {
		C.rgtp_session_destroy(s.cSession)
		s.cSession = nil
	}
}

// CreateClient creates a new RGTP client
func CreateClient(config *Config) (*Client, error) {
	if config == nil {
		return nil, fmt.Errorf("config cannot be nil")
	}

	cConfig := C.rgtp_config_t{
		chunk_size:         C.uint32_t(config.ChunkSize),
		exposure_rate:      C.uint32_t(config.ExposureRate),
		adaptive_mode:      C.bool(config.AdaptiveMode),
		enable_compression: C.bool(config.EnableCompression),
		enable_encryption:  C.bool(config.EnableEncryption),
		port:               C.uint16_t(config.Port),
		timeout_ms:         C.int(config.TimeoutMs),
		user_data:          config.UserData,
	}

	cClient := C.rgtp_client_create(&cConfig)
	if cClient == nil {
		return nil, fmt.Errorf("failed to create client")
	}

	client := &Client{
		cClient: cClient,
		config:  config,
	}

	return client, nil
}

// PullToFile pulls data to a file
func (c *Client) PullToFile(host string, port uint16, filename string) error {
	if c.cClient == nil {
		return fmt.Errorf("invalid client")
	}

	cHost := C.CString(host)
	cFilename := C.CString(filename)
	defer C.free(unsafe.Pointer(cHost))
	defer C.free(unsafe.Pointer(cFilename))

	result := C.rgtp_client_pull_to_file(c.cClient, cHost, C.uint16_t(port), cFilename)
	if result != 0 {
		return fmt.Errorf("failed to pull file: %d", int(result))
	}

	return nil
}

// GetClientStats gets client statistics
func (c *Client) GetStats() (*Stats, error) {
	if c.cClient == nil {
		return nil, fmt.Errorf("invalid client")
	}

	var cStats C.rgtp_stats_t
	result := C.rgtp_client_get_stats(c.cClient, &cStats)
	if result != 0 {
		return nil, fmt.Errorf("failed to get client stats: %d", int(result))
	}

	stats := &Stats{
		BytesSent:         uint64(cStats.bytes_sent),
		BytesReceived:     uint64(cStats.bytes_received),
		ChunksSent:        uint32(cStats.chunks_sent),
		ChunksReceived:    uint32(cStats.chunks_received),
		PacketLossRate:    float32(cStats.packet_loss_rate),
		RttMs:             int(cStats.rtt_ms),
		PacketsLost:       uint32(cStats.packets_lost),
		Retransmissions:   uint32(cStats.retransmissions),
		AvgThroughputMbps: float32(cStats.avg_throughput_mbps),
		CompletionPercent: float32(cStats.completion_percent),
		ActiveConnections: uint32(cStats.active_connections),
	}

	return stats, nil
}

// Destroy destroys the client
func (c *Client) Destroy() {
	if c.cClient != nil {
		C.rgtp_client_destroy(c.cClient)
		c.cClient = nil
	}
}

// SendFile convenience function to send a file
func SendFile(filename string, config *Config) (*Stats, error) {
	session, err := CreateSession(config)
	if err != nil {
		return nil, fmt.Errorf("failed to create session: %w", err)
	}
	defer session.Destroy()

	if err := session.ExposeFile(filename); err != nil {
		return nil, fmt.Errorf("failed to expose file: %w", err)
	}

	if err := session.WaitComplete(); err != nil {
		return nil, fmt.Errorf("session failed: %w", err)
	}

	return session.GetStats()
}

// ReceiveFile convenience function to receive a file
func ReceiveFile(host string, port uint16, filename string, config *Config) (*Stats, error) {
	client, err := CreateClient(config)
	if err != nil {
		return nil, fmt.Errorf("failed to create client: %w", err)
	}
	defer client.Destroy()

	if err := client.PullToFile(host, port, filename); err != nil {
		return nil, fmt.Errorf("failed to pull file: %w", err)
	}

	return client.GetStats()
}
