//go:build mobileserver
// +build mobileserver

// Red Giant Protocol - Mobile-Optimized Server
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

// Mobile server configuration
type MobileServerConfig struct {
	Port            int     `json:"port"`
	EnableGSM       bool    `json:"enable_gsm"`
	Enable2G        bool    `json:"enable_2g"`
	Enable3G        bool    `json:"enable_3g"`
	Enable4G        bool    `json:"enable_4g"`
	Enable5G        bool    `json:"enable_5g"`
	PowerOptimized  bool    `json:"power_optimized"`
	LowBandwidth    bool    `json:"low_bandwidth_mode"`
	MaxChunkSize    int     `json:"max_chunk_size"`
	CompressionRate float64 `json:"compression_rate"`
}

// Mobile session tracking
type MobileSession struct {
	DeviceID       string    `json:"device_id"`
	NetworkType    string    `json:"network_type"`
	StartTime      time.Time `json:"start_time"`
	LastActivity   time.Time `json:"last_activity"`
	BytesReceived  int64     `json:"bytes_received"`
	ChunksReceived int       `json:"chunks_received"`
	Active         bool      `json:"active"`
}

// Mobile Red Giant server
type MobileRedGiantServer struct {
	config   *MobileServerConfig
	sessions map[string]*MobileSession
	listener net.Listener
	mu       sync.RWMutex
	logger   *log.Logger
	ctx      context.Context
	cancel   context.CancelFunc
}

func NewMobileRedGiantServer() *MobileRedGiantServer {
	ctx, cancel := context.WithCancel(context.Background())

	config := &MobileServerConfig{
		Port:            9090,
		EnableGSM:       true,
		Enable2G:        true,
		Enable3G:        true,
		Enable4G:        true,
		Enable5G:        true,
		PowerOptimized:  true,
		LowBandwidth:    true,
		MaxChunkSize:    4096, // Small chunks for mobile
		CompressionRate: 0.7,
	}

	server := &MobileRedGiantServer{
		config:   config,
		sessions: make(map[string]*MobileSession),
		logger:   log.New(os.Stdout, "[RedGiant-Mobile-Server] ", log.LstdFlags),
		ctx:      ctx,
		cancel:   cancel,
	}

	return server
}

// Start mobile server
func (mrs *MobileRedGiantServer) Start() error {
	// Start TCP listener for mobile connections
	listener, err := net.Listen("tcp", fmt.Sprintf("0.0.0.0:%d", mrs.config.Port))
	if err != nil {
		return fmt.Errorf("failed to start mobile server: %w", err)
	}
	mrs.listener = listener

	mrs.logger.Printf("📱 Red Giant Mobile Server started on port %d", mrs.config.Port)
	mrs.logger.Printf("🌐 Supported networks: GSM, 2G, 3G, 4G, 5G")
	mrs.logger.Printf("⚡ Power optimization: %v", mrs.config.PowerOptimized)

	// Start HTTP status server
	go mrs.startHTTPServer()

	// Accept mobile connections
	go mrs.acceptConnections()

	// Start session monitor
	go mrs.monitorSessions()

	return nil
}

// Accept mobile connections
func (mrs *MobileRedGiantServer) acceptConnections() {
	for {
		select {
		case <-mrs.ctx.Done():
			return
		default:
			conn, err := mrs.listener.Accept()
			if err != nil {
				if mrs.ctx.Err() != nil {
					return
				}
				mrs.logger.Printf("❌ Accept error: %v", err)
				continue
			}

			go mrs.handleMobileConnection(conn)
		}
	}
}

// Handle mobile device connection
func (mrs *MobileRedGiantServer) handleMobileConnection(conn net.Conn) {
	defer conn.Close()

	mrs.logger.Printf("📱 New mobile connection from %s", conn.RemoteAddr())

	// Read connection data
	buffer := make([]byte, 4096)
	n, err := conn.Read(buffer)
	if err != nil {
		mrs.logger.Printf("❌ Read error: %v", err)
		return
	}

	data := string(buffer[:n])

	// Parse mobile header
	if strings.HasPrefix(data, "RG-MOBILE-CHUNK:") {
		mrs.handleMobileChunk(conn, data)
	} else {
		mrs.handleMobileData(conn, buffer[:n])
	}
}

// Handle mobile chunk transmission
func (mrs *MobileRedGiantServer) handleMobileChunk(conn net.Conn, headerData string) {
	lines := strings.Split(headerData, "\n")
	if len(lines) < 2 {
		return
	}

	header := lines[0]
	chunkData := []byte(strings.Join(lines[1:], "\n"))

	// Parse header: RG-MOBILE-CHUNK:chunkIndex:totalChunks:deviceID
	parts := strings.Split(header, ":")
	if len(parts) != 4 {
		return
	}

	chunkIndex, _ := strconv.Atoi(parts[1])
	totalChunks, _ := strconv.Atoi(parts[2])
	deviceID := parts[3]

	mrs.logger.Printf("📦 Received chunk %d/%d from device %s (%d bytes)",
		chunkIndex+1, totalChunks, deviceID, len(chunkData))

	// Update session
	mrs.updateSession(deviceID, len(chunkData))

	// Send acknowledgment
	response := fmt.Sprintf("RG-MOBILE-ACK:%d:%s\n", chunkIndex, deviceID)
	conn.Write([]byte(response))
}

// Handle general mobile data
func (mrs *MobileRedGiantServer) handleMobileData(conn net.Conn, data []byte) {
	deviceID := fmt.Sprintf("mobile_%d", time.Now().Unix())

	mrs.logger.Printf("📱 Received data from mobile device (%d bytes)", len(data))

	// Update session
	mrs.updateSession(deviceID, len(data))

	// Send response
	response := map[string]interface{}{
		"status":         "success",
		"device_id":      deviceID,
		"bytes_received": len(data),
		"timestamp":      time.Now().Unix(),
	}

	jsonResponse, _ := json.Marshal(response)
	conn.Write(jsonResponse)
}

// Update session information
func (mrs *MobileRedGiantServer) updateSession(deviceID string, bytesReceived int) {
	mrs.mu.Lock()
	defer mrs.mu.Unlock()

	session, exists := mrs.sessions[deviceID]
	if !exists {
		session = &MobileSession{
			DeviceID:    deviceID,
			NetworkType: "mobile", // Would be detected from device
			StartTime:   time.Now(),
			Active:      true,
		}
		mrs.sessions[deviceID] = session
	}

	session.LastActivity = time.Now()
	session.BytesReceived += int64(bytesReceived)
	session.ChunksReceived++
}

// Monitor sessions for cleanup
func (mrs *MobileRedGiantServer) monitorSessions() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			mrs.cleanupInactiveSessions()
		case <-mrs.ctx.Done():
			return
		}
	}
}

// Cleanup inactive sessions
func (mrs *MobileRedGiantServer) cleanupInactiveSessions() {
	mrs.mu.Lock()
	defer mrs.mu.Unlock()

	now := time.Now()
	for deviceID, session := range mrs.sessions {
		if now.Sub(session.LastActivity) > 5*time.Minute {
			session.Active = false
			mrs.logger.Printf("📱 Session cleanup: %s (inactive for %v)",
				deviceID, now.Sub(session.LastActivity))
		}
	}
}

// Start HTTP status server
func (mrs *MobileRedGiantServer) startHTTPServer() {
	httpPort := mrs.config.Port + 1000 // HTTP on port 10090

	mux := http.NewServeMux()

	// Status endpoint
	mux.HandleFunc("/mobile/status", func(w http.ResponseWriter, r *http.Request) {
		mrs.mu.RLock()
		activeSessions := 0
		for _, session := range mrs.sessions {
			if session.Active {
				activeSessions++
			}
		}
		mrs.mu.RUnlock()

		status := map[string]interface{}{
			"status":             "running",
			"mobile_port":        mrs.config.Port,
			"active_sessions":    activeSessions,
			"total_sessions":     len(mrs.sessions),
			"networks_supported": []string{"GSM", "2G", "3G", "4G", "5G"},
			"power_optimized":    mrs.config.PowerOptimized,
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(status)
	})

	// Sessions endpoint
	mux.HandleFunc("/mobile/sessions", func(w http.ResponseWriter, r *http.Request) {
		mrs.mu.RLock()
		sessions := make([]*MobileSession, 0, len(mrs.sessions))
		for _, session := range mrs.sessions {
			sessions = append(sessions, session)
		}
		mrs.mu.RUnlock()

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"sessions": sessions,
			"count":    len(sessions),
		})
	})

	server := &http.Server{
		Addr:    fmt.Sprintf("0.0.0.0:%d", httpPort),
		Handler: mux,
	}

	mrs.logger.Printf("🌐 Mobile HTTP status server: http://0.0.0.0:%d", httpPort)
	server.ListenAndServe()
}

// Get mobile server statistics
func (mrs *MobileRedGiantServer) GetStats() map[string]interface{} {
	mrs.mu.RLock()
	defer mrs.mu.RUnlock()

	var totalBytes int64
	var totalChunks int
	activeSessions := 0

	for _, session := range mrs.sessions {
		totalBytes += session.BytesReceived
		totalChunks += session.ChunksReceived
		if session.Active {
			activeSessions++
		}
	}

	return map[string]interface{}{
		"total_sessions":  len(mrs.sessions),
		"active_sessions": activeSessions,
		"total_bytes":     totalBytes,
		"total_chunks":    totalChunks,
		"networks_enabled": map[string]bool{
			"gsm": mrs.config.EnableGSM,
			"2g":  mrs.config.Enable2G,
			"3g":  mrs.config.Enable3G,
			"4g":  mrs.config.Enable4G,
			"5g":  mrs.config.Enable5G,
		},
		"power_optimized": mrs.config.PowerOptimized,
		"timestamp":       time.Now().Unix(),
	}
}

// Shutdown server
func (mrs *MobileRedGiantServer) Shutdown() {
	mrs.logger.Printf("📱 Shutting down Mobile Red Giant Server...")
	mrs.cancel()
	if mrs.listener != nil {
		mrs.listener.Close()
	}
}

func main() {
	server := NewMobileRedGiantServer()

	// Handle graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Start server
	if err := server.Start(); err != nil {
		log.Fatalf("Failed to start mobile server: %v", err)
	}

	// Wait for shutdown signal
	<-sigChan
	server.Shutdown()
}
