//go:build mobile
// +build mobile

// Red Giant Protocol - Mobile GSM/Cellular Network Adapter
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"
	"strings"
	"sync"
	"time"
)

// Mobile network types
type NetworkType int

const (
	NetworkGSM NetworkType = iota
	Network2G
	Network3G
	Network4G
	Network5G
	NetworkWiFi
	NetworkBluetooth
	NetworkNFC
)

// Mobile optimization profiles
type MobileProfile struct {
	NetworkType     NetworkType `json:"network_type"`
	MaxBandwidth    int64       `json:"max_bandwidth_bps"`
	Latency         int         `json:"latency_ms"`
	ChunkSize       int         `json:"chunk_size"`
	CompressionRate float64     `json:"compression_rate"`
	RetryAttempts   int         `json:"retry_attempts"`
	PowerOptimized  bool        `json:"power_optimized"`
}

// Mobile device characteristics
type MobileDevice struct {
	ID             string        `json:"id"`
	Type           string        `json:"type"` // "smartphone", "tablet", "iot", "embedded"
	NetworkType    NetworkType   `json:"network_type"`
	BatteryLevel   int           `json:"battery_level"`
	SignalStrength int           `json:"signal_strength"`
	Profile        MobileProfile `json:"profile"`
	LastSeen       time.Time     `json:"last_seen"`
}

// GSM/Cellular specific configuration
type CellularConfig struct {
	APN             string `json:"apn"`
	NetworkOperator string `json:"network_operator"`
	CountryCode     string `json:"country_code"`
	NetworkCode     string `json:"network_code"`
	CellID          string `json:"cell_id"`
	LAC             string `json:"lac"` // Location Area Code
	EnableRoaming   bool   `json:"enable_roaming"`
	PowerSaveMode   bool   `json:"power_save_mode"`
	DataCompression bool   `json:"data_compression"`
}

// Mobile Red Giant adapter
type MobileRedGiantAdapter struct {
	serverAddr string
	localPort  int
	devices    map[string]*MobileDevice
	cellular   *CellularConfig
	profiles   map[NetworkType]*MobileProfile
	devicesMu  sync.RWMutex
	logger     *log.Logger
	ctx        context.Context
	cancel     context.CancelFunc
}

func NewMobileRedGiantAdapter(serverAddr string, localPort int) *MobileRedGiantAdapter {
	ctx, cancel := context.WithCancel(context.Background())

	adapter := &MobileRedGiantAdapter{
		serverAddr: serverAddr,
		localPort:  localPort,
		devices:    make(map[string]*MobileDevice),
		logger:     log.New(os.Stdout, "[RedGiant-Mobile] ", log.LstdFlags),
		ctx:        ctx,
		cancel:     cancel,
		profiles:   createMobileProfiles(),
		cellular:   createDefaultCellularConfig(),
	}

	adapter.logger.Printf("📱 Mobile Red Giant Adapter initialized")
	return adapter
}

// Create optimized profiles for different network types - REALISTIC PERFORMANCE
func createMobileProfiles() map[NetworkType]*MobileProfile {
	return map[NetworkType]*MobileProfile{
		NetworkGSM: {
			NetworkType:     NetworkGSM,
			MaxBandwidth:    9600, // 9.6 Kbps - Legacy only
			Latency:         500,  // 500ms
			ChunkSize:       1024, // 1KB chunks
			CompressionRate: 0.8,  // Aggressive compression
			RetryAttempts:   5,
			PowerOptimized:  true,
		},
		Network2G: {
			NetworkType:     Network2G,
			MaxBandwidth:    384000, // 384 Kbps (EDGE max)
			Latency:         200,
			ChunkSize:       8192, // 8KB chunks
			CompressionRate: 0.7,
			RetryAttempts:   3,
			PowerOptimized:  true,
		},
		Network3G: {
			NetworkType:     Network3G,
			MaxBandwidth:    42000000, // 42 Mbps (HSPA+ real-world)
			Latency:         80,
			ChunkSize:       131072, // 128KB chunks
			CompressionRate: 0.5,
			RetryAttempts:   2,
			PowerOptimized:  false,
		},
		Network4G: {
			NetworkType:     Network4G,
			MaxBandwidth:    300000000, // 300 Mbps (LTE-A real-world)
			Latency:         30,        // Modern LTE latency
			ChunkSize:       1048576,   // 1MB chunks - SAME AS DESKTOP
			CompressionRate: 0.3,
			RetryAttempts:   2,
			PowerOptimized:  false,
		},
		Network5G: {
			NetworkType:     Network5G,
			MaxBandwidth:    10000000000, // 10 Gbps (5G real peak)
			Latency:         5,           // 5ms ultra-low latency
			ChunkSize:       4194304,     // 4MB chunks - FASTER THAN DESKTOP
			CompressionRate: 0.1,         // Minimal compression needed
			RetryAttempts:   1,
			PowerOptimized:  false,
		},
		NetworkWiFi: {
			NetworkType:     NetworkWiFi,
			MaxBandwidth:    1000000000, // 1 Gbps (WiFi 6 on mobile)
			Latency:         10,
			ChunkSize:       2097152, // 2MB chunks
			CompressionRate: 0.2,
			RetryAttempts:   1,
			PowerOptimized:  false,
		},
	}
}

// Create default cellular configuration
func createDefaultCellularConfig() *CellularConfig {
	return &CellularConfig{
		APN:             "internet",
		NetworkOperator: "auto",
		EnableRoaming:   false,
		PowerSaveMode:   true,
		DataCompression: true,
	}
}

// Detect network type from system or manual configuration
func (mra *MobileRedGiantAdapter) detectNetworkType() NetworkType {
	// In a real implementation, this would query system APIs
	// For demo, we'll simulate based on environment variables

	if networkType := os.Getenv("MOBILE_NETWORK_TYPE"); networkType != "" {
		switch strings.ToUpper(networkType) {
		case "GSM":
			return NetworkGSM
		case "2G":
			return Network2G
		case "3G":
			return Network3G
		case "4G", "LTE":
			return Network4G
		case "5G":
			return Network5G
		case "WIFI":
			return NetworkWiFi
		}
	}

	// Default to 4G for demonstration
	return Network4G
}

// Register mobile device with adaptive profile
func (mra *MobileRedGiantAdapter) RegisterDevice(deviceID, deviceType string) error {
	networkType := mra.detectNetworkType()
	profile := mra.profiles[networkType]

	device := &MobileDevice{
		ID:             deviceID,
		Type:           deviceType,
		NetworkType:    networkType,
		BatteryLevel:   100, // Would be read from system
		SignalStrength: 4,   // Would be read from system (1-5 bars)
		Profile:        *profile,
		LastSeen:       time.Now(),
	}

	mra.devicesMu.Lock()
	mra.devices[deviceID] = device
	mra.devicesMu.Unlock()

	mra.logger.Printf("📱 Device registered: %s (%s) on %s network",
		deviceID, deviceType, mra.networkTypeString(networkType))

	return nil
}

// Send data with mobile optimizations
func (mra *MobileRedGiantAdapter) SendMobileData(deviceID string, data []byte, destination string) error {
	mra.devicesMu.RLock()
	device, exists := mra.devices[deviceID]
	mra.devicesMu.RUnlock()

	if !exists {
		return fmt.Errorf("device %s not registered", deviceID)
	}

	// Apply mobile optimizations
	optimizedData, err := mra.optimizeForMobile(data, &device.Profile)
	if err != nil {
		return fmt.Errorf("failed to optimize data: %w", err)
	}

	// Send data in optimized chunks
	return mra.sendChunkedData(device, optimizedData, destination)
}

// Optimize data for mobile transmission
func (mra *MobileRedGiantAdapter) optimizeForMobile(data []byte, profile *MobileProfile) ([]byte, error) {
	// Apply compression if beneficial
	if profile.CompressionRate > 0 {
		// Simple compression simulation (in real implementation, use gzip/lz4)
		compressionSavings := int(float64(len(data)) * profile.CompressionRate)
		if compressionSavings > 100 { // Only compress if we save significant bytes
			compressedData := make([]byte, len(data)-compressionSavings)
			copy(compressedData, data[:len(compressedData)])

			mra.logger.Printf("📦 Data compressed: %d -> %d bytes (%.1f%% savings)",
				len(data), len(compressedData), profile.CompressionRate*100)
			return compressedData, nil
		}
	}

	return data, nil
}

// Send data in mobile-optimized chunks
func (mra *MobileRedGiantAdapter) sendChunkedData(device *MobileDevice, data []byte, destination string) error {
	chunkSize := device.Profile.ChunkSize
	totalChunks := (len(data) + chunkSize - 1) / chunkSize

	mra.logger.Printf("📤 Sending %d bytes in %d chunks of %d bytes each",
		len(data), totalChunks, chunkSize)

	for i := 0; i < totalChunks; i++ {
		start := i * chunkSize
		end := start + chunkSize
		if end > len(data) {
			end = len(data)
		}

		chunk := data[start:end]

		// Send chunk with retries for mobile reliability
		err := mra.sendChunkWithRetry(device, chunk, i, totalChunks, destination)
		if err != nil {
			return fmt.Errorf("failed to send chunk %d: %w", i, err)
		}

		// Add delay for power optimization on slow networks
		if device.Profile.PowerOptimized {
			time.Sleep(time.Duration(device.Profile.Latency/10) * time.Millisecond)
		}
	}

	mra.logger.Printf("✅ Mobile transmission completed successfully")
	return nil
}

// Send individual chunk with mobile-specific retry logic
func (mra *MobileRedGiantAdapter) sendChunkWithRetry(device *MobileDevice, chunk []byte, chunkIndex, totalChunks int, destination string) error {
	for attempt := 0; attempt < device.Profile.RetryAttempts; attempt++ {
		// Connect to destination
		conn, err := net.DialTimeout("tcp", destination, 30*time.Second)
		if err != nil {
			if attempt < device.Profile.RetryAttempts-1 {
				mra.logger.Printf("⚠️ Connection attempt %d failed, retrying...", attempt+1)
				time.Sleep(time.Duration(attempt+1) * time.Second)
				continue
			}
			return fmt.Errorf("failed to connect after %d attempts: %w", device.Profile.RetryAttempts, err)
		}
		defer conn.Close()

		// Send chunk with mobile headers
		header := fmt.Sprintf("RG-MOBILE-CHUNK:%d:%d:%s\n", chunkIndex, totalChunks, device.ID)
		if _, err := conn.Write([]byte(header)); err != nil {
			continue
		}

		if _, err := conn.Write(chunk); err != nil {
			continue
		}

		// Success
		return nil
	}

	return fmt.Errorf("failed to send chunk after %d attempts", device.Profile.RetryAttempts)
}

// Monitor mobile network conditions
func (mra *MobileRedGiantAdapter) MonitorMobileNetwork(duration time.Duration) {
	mra.logger.Printf("📡 Starting mobile network monitoring for %v", duration)

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	startTime := time.Now()

	for {
		select {
		case <-ticker.C:
			elapsed := time.Since(startTime)
			if elapsed >= duration {
				mra.logger.Printf("✅ Mobile network monitoring completed")
				return
			}

			// Update device network conditions
			mra.updateNetworkConditions()

			// Log status
			mra.devicesMu.RLock()
			deviceCount := len(mra.devices)
			mra.devicesMu.RUnlock()

			mra.logger.Printf("📊 Network Status: %d devices, %v elapsed",
				deviceCount, elapsed.Round(time.Second))

		case <-mra.ctx.Done():
			return
		}
	}
}

// Update network conditions for all devices
func (mra *MobileRedGiantAdapter) updateNetworkConditions() {
	mra.devicesMu.Lock()
	defer mra.devicesMu.Unlock()

	for deviceID, device := range mra.devices {
		// Simulate network condition changes
		// In real implementation, this would query actual network APIs

		// Update signal strength (simulate)
		if device.SignalStrength > 1 && time.Since(device.LastSeen) > 30*time.Second {
			device.SignalStrength--
		}

		// Update battery level (simulate drain)
		if device.BatteryLevel > 0 {
			device.BatteryLevel--
		}

		device.LastSeen = time.Now()

		// Adapt profile based on conditions
		if device.SignalStrength <= 2 || device.BatteryLevel <= 20 {
			// Switch to power save mode
			device.Profile.PowerOptimized = true
			device.Profile.ChunkSize = min(device.Profile.ChunkSize, 4096)
			device.Profile.CompressionRate = 0.8
		}

		mra.logger.Printf("📱 %s: Signal=%d/5, Battery=%d%%, Network=%s",
			deviceID, device.SignalStrength, device.BatteryLevel,
			mra.networkTypeString(device.NetworkType))
	}
}

// Convert network type to string
func (mra *MobileRedGiantAdapter) networkTypeString(networkType NetworkType) string {
	switch networkType {
	case NetworkGSM:
		return "GSM"
	case Network2G:
		return "2G"
	case Network3G:
		return "3G"
	case Network4G:
		return "4G/LTE"
	case Network5G:
		return "5G"
	case NetworkWiFi:
		return "WiFi"
	case NetworkBluetooth:
		return "Bluetooth"
	case NetworkNFC:
		return "NFC"
	default:
		return "Unknown"
	}
}

// Get mobile network statistics
func (mra *MobileRedGiantAdapter) GetMobileStats() map[string]interface{} {
	mra.devicesMu.RLock()
	defer mra.devicesMu.RUnlock()

	stats := map[string]interface{}{
		"total_devices":   len(mra.devices),
		"cellular_config": mra.cellular,
		"timestamp":       time.Now().Unix(),
	}

	// Count devices by network type
	networkCounts := make(map[string]int)
	for _, device := range mra.devices {
		networkType := mra.networkTypeString(device.NetworkType)
		networkCounts[networkType]++
	}
	stats["devices_by_network"] = networkCounts

	return stats
}

// Cleanup resources
func (mra *MobileRedGiantAdapter) Shutdown() {
	mra.logger.Printf("📱 Shutting down Mobile Red Giant Adapter...")
	mra.cancel()
}

func Min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("📱 Red Giant Protocol - Mobile GSM/Cellular Adapter")
		fmt.Println("Usage:")
		fmt.Println("  go run red_giant_mobile.go register <device-id> <device-type>")
		fmt.Println("  go run red_giant_mobile.go send <device-id> <data> <destination>")
		fmt.Println("  go run red_giant_mobile.go monitor [duration]")
		fmt.Println("  go run red_giant_mobile.go stats")
		fmt.Println("")
		fmt.Println("Environment Variables:")
		fmt.Println("  MOBILE_NETWORK_TYPE: GSM|2G|3G|4G|5G|WIFI")
		fmt.Println("  RED_GIANT_SERVER: server address (default: localhost:8080)")
		fmt.Println("")
		fmt.Println("Examples:")
		fmt.Println("  MOBILE_NETWORK_TYPE=3G go run red_giant_mobile.go register phone001 smartphone")
		fmt.Println("  go run red_giant_mobile.go send phone001 'Hello Mobile' localhost:8080")
		fmt.Println("  go run red_giant_mobile.go monitor 30s")
		return
	}

	serverAddr := "localhost:8080"
	if env := os.Getenv("RED_GIANT_SERVER"); env != "" {
		serverAddr = env
	}

	adapter := NewMobileRedGiantAdapter(serverAddr, 9090)
	defer adapter.Shutdown()

	command := os.Args[1]

	switch command {
	case "register":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run red_giant_mobile.go register <device-id> <device-type>")
			return
		}
		deviceID := os.Args[2]
		deviceType := os.Args[3]

		if err := adapter.RegisterDevice(deviceID, deviceType); err != nil {
			fmt.Printf("❌ Registration failed: %v\n", err)
		}

	case "send":
		if len(os.Args) < 5 {
			fmt.Println("Usage: go run red_giant_mobile.go send <device-id> <data> <destination>")
			return
		}
		deviceID := os.Args[2]
		data := []byte(os.Args[3])
		destination := os.Args[4]

		if err := adapter.SendMobileData(deviceID, data, destination); err != nil {
			fmt.Printf("❌ Send failed: %v\n", err)
		}

	case "monitor":
		duration := 30 * time.Second
		if len(os.Args) > 2 {
			if d, err := time.ParseDuration(os.Args[2]); err == nil {
				duration = d
			}
		}
		adapter.MonitorMobileNetwork(duration)

	case "stats":
		stats := adapter.GetMobileStats()
		jsonData, _ := json.MarshalIndent(stats, "", "  ")
		fmt.Println(string(jsonData))

	default:
		fmt.Printf("❌ Unknown command: %s\n", command)
	}
}
