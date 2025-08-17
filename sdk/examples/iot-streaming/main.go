// Red Giant Protocol - IoT Data Streaming Example
// Demonstrates high-throughput sensor data collection and distribution
package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math"
	"math/rand"
	"os"
	"strconv"
	"time"

	"redgiant-sdk" // Import Red Giant SDK
)

type SensorReading struct {
	DeviceID   string    `json:"device_id"`
	SensorType string    `json:"sensor_type"`
	Value      float64   `json:"value"`
	Unit       string    `json:"unit"`
	Timestamp  time.Time `json:"timestamp"`
	Location   Location  `json:"location"`
	Metadata   Metadata  `json:"metadata"`
}

type Location struct {
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
	Altitude  float64 `json:"altitude"`
}

type Metadata struct {
	BatteryLevel   float64 `json:"battery_level"`
	SignalStrength int     `json:"signal_strength"`
	Firmware       string  `json:"firmware"`
}

type SensorBatch struct {
	BatchID   string          `json:"batch_id"`
	DeviceID  string          `json:"device_id"`
	Readings  []SensorReading `json:"readings"`
	Timestamp time.Time       `json:"timestamp"`
	Count     int             `json:"count"`
}

type IoTDevice struct {
	client    *redgiant.Client
	deviceID  string
	location  Location
	sensors   []string
	batchSize int
	interval  time.Duration
}

func main() {
	if len(os.Args) < 2 {
		showUsage()
		return
	}

	command := os.Args[1]

	// Connect to Red Giant server
	serverURL := os.Getenv("RED_GIANT_SERVER")
	if serverURL == "" {
		serverURL = "http://localhost:8080"
	}

	client := redgiant.NewClient(serverURL)

	fmt.Printf("🌐 Red Giant IoT Data Streaming\n")
	fmt.Printf("🔗 Server: %s\n", serverURL)

	switch command {
	case "device":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go device <device-id> [batch-size] [interval-ms]")
			return
		}
		runDevice(client, os.Args[2:])

	case "collector":
		runCollector(client)

	case "monitor":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go monitor <device-id>")
			return
		}
		monitorDevice(client, os.Args[2])

	case "analytics":
		runAnalytics(client)

	case "simulate":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go simulate <num-devices>")
			return
		}
		numDevices, _ := strconv.Atoi(os.Args[2])
		simulateNetwork(client, numDevices)

	default:
		fmt.Printf("Unknown command: %s\n", command)
		showUsage()
	}
}

func showUsage() {
	fmt.Println("🚀 Red Giant IoT Data Streaming")
	fmt.Println("Usage: go run main.go <command> [args...]")
	fmt.Println("")
	fmt.Println("Commands:")
	fmt.Println("  device <id> [batch] [interval]  Run IoT device simulator")
	fmt.Println("  collector                       Run data collector")
	fmt.Println("  monitor <device-id>             Monitor specific device")
	fmt.Println("  analytics                       Run real-time analytics")
	fmt.Println("  simulate <num-devices>          Simulate IoT network")
	fmt.Println("")
	fmt.Println("Examples:")
	fmt.Println("  go run main.go device sensor001 10 1000")
	fmt.Println("  go run main.go collector")
	fmt.Println("  go run main.go monitor sensor001")
	fmt.Println("  go run main.go simulate 50")
	fmt.Println("")
	fmt.Println("Environment:")
	fmt.Println("  RED_GIANT_SERVER  Server URL (default: http://localhost:8080)")
}

func runDevice(client *redgiant.Client, args []string) {
	deviceID := args[0]
	batchSize := 10
	interval := 1000 * time.Millisecond

	if len(args) > 1 {
		if b, err := strconv.Atoi(args[1]); err == nil {
			batchSize = b
		}
	}

	if len(args) > 2 {
		if i, err := strconv.Atoi(args[2]); err == nil {
			interval = time.Duration(i) * time.Millisecond
		}
	}

	client.SetPeerID(fmt.Sprintf("iot_device_%s", deviceID))

	device := &IoTDevice{
		client:   client,
		deviceID: deviceID,
		location: Location{
			Latitude:  40.7128 + rand.Float64()*0.1, // NYC area
			Longitude: -74.0060 + rand.Float64()*0.1,
			Altitude:  rand.Float64() * 100,
		},
		sensors:   []string{"temperature", "humidity", "pressure", "light", "motion"},
		batchSize: batchSize,
		interval:  interval,
	}

	fmt.Printf("🔌 Starting IoT Device: %s\n", deviceID)
	fmt.Printf("📍 Location: %.4f, %.4f\n", device.location.Latitude, device.location.Longitude)
	fmt.Printf("📊 Batch size: %d readings\n", batchSize)
	fmt.Printf("⏱️  Interval: %v\n", interval)
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	device.startStreaming()
}

func (device *IoTDevice) startStreaming() {
	ticker := time.NewTicker(device.interval)
	defer ticker.Stop()

	batchCount := 0
	totalReadings := 0

	for range ticker.C {
		batch := device.generateBatch()

		// Serialize batch to JSON
		data, err := json.Marshal(batch)
		if err != nil {
			log.Printf("❌ Failed to serialize batch: %v", err)
			continue
		}

		// Upload using Red Giant's high-performance C core
		filename := fmt.Sprintf("iot_batch_%s_%d.json", device.deviceID, time.Now().UnixNano())

		start := time.Now()
		result, err := device.client.UploadData(data, filename)
		if err != nil {
			log.Printf("❌ Failed to upload batch: %v", err)
			continue
		}

		uploadTime := time.Since(start)
		batchCount++
		totalReadings += len(batch.Readings)

		fmt.Printf("📤 Batch %d uploaded: %d readings, %d bytes in %v (%.2f MB/s)\n",
			batchCount, len(batch.Readings), len(data), uploadTime, result.ThroughputMbps)

		// Show summary every 10 batches
		if batchCount%10 == 0 {
			fmt.Printf("📊 Summary: %d batches, %d total readings\n", batchCount, totalReadings)
		}
	}
}

func (device *IoTDevice) generateBatch() SensorBatch {
	readings := make([]SensorReading, device.batchSize)

	for i := 0; i < device.batchSize; i++ {
		sensorType := device.sensors[rand.Intn(len(device.sensors))]

		reading := SensorReading{
			DeviceID:   device.deviceID,
			SensorType: sensorType,
			Value:      device.generateSensorValue(sensorType),
			Unit:       device.getSensorUnit(sensorType),
			Timestamp:  time.Now().Add(time.Duration(i) * time.Millisecond),
			Location:   device.location,
			Metadata: Metadata{
				BatteryLevel:   80 + rand.Float64()*20,
				SignalStrength: -60 + rand.Intn(40),
				Firmware:       "v1.2.3",
			},
		}

		readings[i] = reading
	}

	return SensorBatch{
		BatchID:   fmt.Sprintf("batch_%s_%d", device.deviceID, time.Now().UnixNano()),
		DeviceID:  device.deviceID,
		Readings:  readings,
		Timestamp: time.Now(),
		Count:     len(readings),
	}
}

func (device *IoTDevice) generateSensorValue(sensorType string) float64 {
	switch sensorType {
	case "temperature":
		return 20 + rand.Float64()*15 // 20-35°C
	case "humidity":
		return 30 + rand.Float64()*40 // 30-70%
	case "pressure":
		return 1000 + rand.Float64()*50 // 1000-1050 hPa
	case "light":
		return rand.Float64() * 1000 // 0-1000 lux
	case "motion":
		return math.Round(rand.Float64()) // 0 or 1
	default:
		return rand.Float64() * 100
	}
}

func (device *IoTDevice) getSensorUnit(sensorType string) string {
	switch sensorType {
	case "temperature":
		return "°C"
	case "humidity":
		return "%"
	case "pressure":
		return "hPa"
	case "light":
		return "lux"
	case "motion":
		return "bool"
	default:
		return "unit"
	}
}

func runCollector(client *redgiant.Client) {
	client.SetPeerID(fmt.Sprintf("iot_collector_%d", time.Now().Unix()))

	fmt.Printf("📡 Starting IoT Data Collector\n")
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	lastCheck := time.Now().Add(-1 * time.Minute)
	totalBatches := 0
	totalReadings := 0

	for range ticker.C {
		// Search for new IoT batches
		files, err := client.SearchFiles("iot_batch_")
		if err != nil {
			log.Printf("❌ Failed to search for batches: %v", err)
			continue
		}

		newBatches := 0
		newReadings := 0

		for _, file := range files {
			if file.UploadedAt.After(lastCheck) {
				batch, err := downloadAndParseBatch(client, file.ID)
				if err != nil {
					continue
				}

				newBatches++
				newReadings += batch.Count

				fmt.Printf("📥 Collected batch from %s: %d readings (%.2f MB)\n",
					batch.DeviceID, batch.Count, float64(file.Size)/(1024*1024))
			}
		}

		if newBatches > 0 {
			totalBatches += newBatches
			totalReadings += newReadings

			fmt.Printf("📊 Collection summary: %d new batches, %d new readings\n", newBatches, newReadings)
			fmt.Printf("📈 Total collected: %d batches, %d readings\n\n", totalBatches, totalReadings)
		}

		lastCheck = time.Now()
	}
}

func downloadAndParseBatch(client *redgiant.Client, fileID string) (*SensorBatch, error) {
	data, err := client.DownloadData(fileID)
	if err != nil {
		return nil, err
	}

	var batch SensorBatch
	if err := json.Unmarshal(data, &batch); err != nil {
		return nil, err
	}

	return &batch, nil
}

func monitorDevice(client *redgiant.Client, deviceID string) {
	client.SetPeerID(fmt.Sprintf("iot_monitor_%s_%d", deviceID, time.Now().Unix()))

	fmt.Printf("📊 Monitoring Device: %s\n", deviceID)
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	lastCheck := time.Now()
	readings := make(map[string][]float64)

	for range ticker.C {
		// Search for batches from this device
		pattern := fmt.Sprintf("iot_batch_%s_", deviceID)
		files, err := client.SearchFiles(pattern)
		if err != nil {
			continue
		}

		for _, file := range files {
			if file.UploadedAt.After(lastCheck) {
				batch, err := downloadAndParseBatch(client, file.ID)
				if err != nil {
					continue
				}

				// Process readings
				for _, reading := range batch.Readings {
					if _, exists := readings[reading.SensorType]; !exists {
						readings[reading.SensorType] = make([]float64, 0)
					}
					readings[reading.SensorType] = append(readings[reading.SensorType], reading.Value)

					// Keep only last 100 readings per sensor
					if len(readings[reading.SensorType]) > 100 {
						readings[reading.SensorType] = readings[reading.SensorType][1:]
					}
				}

				fmt.Printf("📥 New batch: %d readings from %s\n", len(batch.Readings), deviceID)
			}
		}

		// Show current sensor values
		if len(readings) > 0 {
			fmt.Printf("📊 Current sensor values for %s:\n", deviceID)
			for sensorType, values := range readings {
				if len(values) > 0 {
					latest := values[len(values)-1]
					avg := calculateAverage(values)
					fmt.Printf("   • %s: %.2f (avg: %.2f)\n", sensorType, latest, avg)
				}
			}
			fmt.Println()
		}

		lastCheck = time.Now()
	}
}

func runAnalytics(client *redgiant.Client) {
	client.SetPeerID(fmt.Sprintf("iot_analytics_%d", time.Now().Unix()))

	fmt.Printf("📈 Starting IoT Analytics Engine\n")
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	deviceStats := make(map[string]map[string][]float64)
	lastCheck := time.Now().Add(-1 * time.Minute)

	for range ticker.C {
		// Collect all recent IoT data
		files, err := client.SearchFiles("iot_batch_")
		if err != nil {
			continue
		}

		newData := false

		for _, file := range files {
			if file.UploadedAt.After(lastCheck) {
				batch, err := downloadAndParseBatch(client, file.ID)
				if err != nil {
					continue
				}

				// Initialize device stats if needed
				if _, exists := deviceStats[batch.DeviceID]; !exists {
					deviceStats[batch.DeviceID] = make(map[string][]float64)
				}

				// Process readings
				for _, reading := range batch.Readings {
					if _, exists := deviceStats[batch.DeviceID][reading.SensorType]; !exists {
						deviceStats[batch.DeviceID][reading.SensorType] = make([]float64, 0)
					}

					deviceStats[batch.DeviceID][reading.SensorType] = append(
						deviceStats[batch.DeviceID][reading.SensorType], reading.Value)
				}

				newData = true
			}
		}

		if newData {
			// Generate analytics report
			fmt.Printf("📊 IoT Network Analytics Report\n")
			fmt.Printf("===============================\n")
			fmt.Printf("Active devices: %d\n", len(deviceStats))

			totalReadings := 0
			for deviceID, sensors := range deviceStats {
				deviceReadings := 0
				fmt.Printf("\n🔌 Device: %s\n", deviceID)

				for sensorType, values := range sensors {
					if len(values) > 0 {
						avg := calculateAverage(values)
						min := calculateMin(values)
						max := calculateMax(values)

						fmt.Printf("   • %s: avg=%.2f, min=%.2f, max=%.2f (%d readings)\n",
							sensorType, avg, min, max, len(values))

						deviceReadings += len(values)
					}
				}

				fmt.Printf("   Total readings: %d\n", deviceReadings)
				totalReadings += deviceReadings
			}

			fmt.Printf("\n📈 Network Summary:\n")
			fmt.Printf("   • Total readings: %d\n", totalReadings)
			fmt.Printf("   • Average per device: %.1f\n", float64(totalReadings)/float64(len(deviceStats)))

			// Network performance
			stats, err := client.GetNetworkStats()
			if err == nil {
				fmt.Printf("   • Network throughput: %.2f MB/s\n", stats.ThroughputMbps)
				fmt.Printf("   • Average latency: %d ms\n", stats.AverageLatency)
			}

			fmt.Printf("===============================\n\n")
		}

		lastCheck = time.Now()
	}
}

func simulateNetwork(client *redgiant.Client, numDevices int) {
	fmt.Printf("🌐 Simulating IoT Network with %d devices\n\n", numDevices)

	// Start multiple device simulators
	for i := 0; i < numDevices; i++ {
		deviceID := fmt.Sprintf("sim_device_%03d", i+1)

		go func(id string) {
			deviceClient := redgiant.NewClient(client.BaseURL)
			deviceClient.SetPeerID(fmt.Sprintf("iot_device_%s", id))

			device := &IoTDevice{
				client:   deviceClient,
				deviceID: id,
				location: Location{
					Latitude:  40.7128 + rand.Float64()*0.2,
					Longitude: -74.0060 + rand.Float64()*0.2,
					Altitude:  rand.Float64() * 200,
				},
				sensors:   []string{"temperature", "humidity", "pressure", "light"},
				batchSize: 5 + rand.Intn(10),                                      // 5-15 readings per batch
				interval:  time.Duration(2000+rand.Intn(3000)) * time.Millisecond, // 2-5 seconds
			}

			fmt.Printf("🔌 Started device: %s\n", id)
			device.startStreaming()
		}(deviceID)

		// Stagger device starts
		time.Sleep(100 * time.Millisecond)
	}

	// Keep main thread alive
	select {}
}

func calculateAverage(values []float64) float64 {
	if len(values) == 0 {
		return 0
	}

	sum := 0.0
	for _, v := range values {
		sum += v
	}
	return sum / float64(len(values))
}

func calculateMin(values []float64) float64 {
	if len(values) == 0 {
		return 0
	}

	min := values[0]
	for _, v := range values {
		if v < min {
			min = v
		}
	}
	return min
}

func calculateMax(values []float64) float64 {
	if len(values) == 0 {
		return 0
	}

	max := values[0]
	for _, v := range values {
		if v > max {
			max = v
		}
	}
	return max
}
