/**
 * Industrial IoT using RGTP as Layer 4 Protocol
 * 
 * This example demonstrates how RGTP replaces TCP/UDP in industrial
 * automation scenarios. Multiple systems can pull sensor data
 * simultaneously without the overhead of TCP connections.
 * 
 * Scenario:
 * - Factory sensor exposes temperature readings via RGTP
 * - SCADA system pulls real-time data for monitoring
 * - Analytics system pulls batch data for ML processing
 * - Safety system pulls critical alerts with high priority
 * 
 * RGTP Benefits:
 * - One sensor exposure serves all consumers simultaneously
 * - No connection state management
 * - Natural load balancing through pull pressure
 * - Instant resume if any system goes offline
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rgtp/rgtp.h"

#define SENSOR_PORT 5000
#define MAX_READINGS 1000
#define READING_INTERVAL_MS 100  // 10 Hz sensor

// Sensor reading structure
typedef struct {
    uint32_t timestamp;
    uint32_t sensor_id;
    float temperature;
    float pressure;
    float humidity;
    uint8_t status;
    uint8_t alert_level;  // 0=normal, 1=warning, 2=critical
} sensor_reading_t;

// Sensor exposer
typedef struct {
    int rgtp_socket;
    struct sockaddr_in addr;
    sensor_reading_t readings[MAX_READINGS];
    uint32_t reading_count;
    uint32_t session_id;
    pthread_t thread;
    int running;
    uint32_t sensor_id;
} sensor_exposer_t;

// Consumer types
typedef enum {
    CONSUMER_SCADA,      // Real-time monitoring
    CONSUMER_ANALYTICS,  // Batch processing
    CONSUMER_SAFETY      // Critical alerts only
} consumer_type_t;

// Data consumer
typedef struct {
    int rgtp_socket;
    struct sockaddr_in sensor_addr;
    consumer_type_t type;
    uint32_t session_id;
    uint32_t last_reading_id;
    pthread_t thread;
    int running;
} data_consumer_t;

// Generate realistic sensor reading
sensor_reading_t generate_reading(uint32_t sensor_id, uint32_t reading_id) {
    sensor_reading_t reading;
    
    reading.timestamp = time(NULL);
    reading.sensor_id = sensor_id;
    
    // Simulate temperature variations (20-30°C with noise)
    reading.temperature = 25.0f + (rand() % 100 - 50) / 10.0f;
    
    // Simulate pressure (1000-1100 hPa)
    reading.pressure = 1050.0f + (rand() % 100 - 50) / 2.0f;
    
    // Simulate humidity (40-60%)
    reading.humidity = 50.0f + (rand() % 200 - 100) / 10.0f;
    
    reading.status = 1; // Active
    
    // Determine alert level
    if (reading.temperature > 28.0f || reading.pressure > 1080.0f) {
        reading.alert_level = 2; // Critical
    } else if (reading.temperature > 26.0f || reading.pressure > 1070.0f) {
        reading.alert_level = 1; // Warning
    } else {
        reading.alert_level = 0; // Normal
    }
    
    return reading;
}

// Sensor exposer thread
void* sensor_exposer_thread(void* arg) {
    sensor_exposer_t* sensor = (sensor_exposer_t*)arg;
    uint32_t reading_id = 0;
    
    printf("Sensor %u started, exposing data on RGTP port %d\n", 
           sensor->sensor_id, ntohs(sensor->addr.sin_port));
    
    while (sensor->running) {
        // Generate new sensor reading
        sensor_reading_t reading = generate_reading(sensor->sensor_id, reading_id);
        
        // Store in circular buffer
        sensor->readings[reading_id % MAX_READINGS] = reading;
        sensor->reading_count = reading_id + 1;
        
        printf("Sensor %u: T=%.1f°C, P=%.1f hPa, H=%.1f%%, Alert=%d\n",
               sensor->sensor_id, reading.temperature, reading.pressure, 
               reading.humidity, reading.alert_level);
        
        // Expose reading via RGTP (all consumers can pull simultaneously)
        rgtp_expose_data_chunk(sensor->rgtp_socket, sensor->session_id, 
                              reading_id, &reading, sizeof(reading));
        
        reading_id++;
        usleep(READING_INTERVAL_MS * 1000);
    }
    
    return NULL;
}

// Consumer thread
void* consumer_thread(void* arg) {
    data_consumer_t* consumer = (data_consumer_t*)arg;
    const char* consumer_names[] = {"SCADA", "Analytics", "Safety"};
    
    printf("%s consumer started, pulling from sensor\n", consumer_names[consumer->type]);
    
    while (consumer->running) {
        sensor_reading_t reading;
        uint32_t chunk_id;
        
        // Pull strategy depends on consumer type
        switch (consumer->type) {
            case CONSUMER_SCADA:
                // SCADA needs real-time data - pull latest readings
                if (rgtp_pull_latest_chunk(consumer->rgtp_socket, consumer->session_id,
                                         &chunk_id, &reading, sizeof(reading)) == 0) {
                    
                    printf("[SCADA] Latest reading %u: T=%.1f°C, Alert=%d\n",
                           chunk_id, reading.temperature, reading.alert_level);
                    consumer->last_reading_id = chunk_id;
                }
                usleep(500000); // 0.5 second interval
                break;
                
            case CONSUMER_ANALYTICS:
                // Analytics can batch process - pull multiple readings
                sensor_reading_t batch[10];
                uint32_t batch_size = 10;
                
                if (rgtp_pull_chunk_range(consumer->rgtp_socket, consumer->session_id,
                                        consumer->last_reading_id + 1, batch_size,
                                        batch, sizeof(batch)) > 0) {
                    
                    // Process batch for analytics
                    float avg_temp = 0;
                    for (int i = 0; i < batch_size; i++) {
                        avg_temp += batch[i].temperature;
                    }
                    avg_temp /= batch_size;
                    
                    printf("[Analytics] Batch processed: avg_temp=%.1f°C\n", avg_temp);
                    consumer->last_reading_id += batch_size;
                }
                sleep(5); // 5 second interval for batch processing
                break;
                
            case CONSUMER_SAFETY:
                // Safety system only needs critical alerts
                if (rgtp_pull_filtered_chunk(consumer->rgtp_socket, consumer->session_id,
                                           RGTP_FILTER_ALERT_LEVEL, 2, // Critical only
                                           &chunk_id, &reading, sizeof(reading)) == 0) {
                    
                    printf("[SAFETY] CRITICAL ALERT %u: T=%.1f°C, P=%.1f hPa\n",
                           chunk_id, reading.temperature, reading.pressure);
                    
                    // Safety system takes immediate action
                    if (reading.temperature > 29.0f) {
                        printf("[SAFETY] EMERGENCY SHUTDOWN TRIGGERED!\n");
                    }
                }
                usleep(100000); // 0.1 second interval for safety
                break;
        }
    }
    
    return NULL;
}

// Create sensor exposer
sensor_exposer_t* create_sensor(uint32_t sensor_id, int port) {
    sensor_exposer_t* sensor = malloc(sizeof(sensor_exposer_t));
    if (!sensor) return NULL;
    
    // Create RGTP socket (Layer 4, replaces TCP/UDP)
    sensor->rgtp_socket = rgtp_socket(AF_INET, RGTP_EXPOSER, 0);
    if (sensor->rgtp_socket < 0) {
        free(sensor);
        return NULL;
    }
    
    // Bind to address
    sensor->addr.sin_family = AF_INET;
    sensor->addr.sin_addr.s_addr = INADDR_ANY;
    sensor->addr.sin_port = htons(port);
    
    if (rgtp_bind(sensor->rgtp_socket, (struct sockaddr*)&sensor->addr, 
                  sizeof(sensor->addr)) != 0) {
        rgtp_close(sensor->rgtp_socket);
        free(sensor);
        return NULL;
    }
    
    // Configure RGTP for industrial IoT
    rgtp_config_t config = {
        .chunk_size = sizeof(sensor_reading_t),
        .exposure_rate = 10,            // 10 Hz sensor rate
        .adaptive_mode = 1,
        .multicast_enabled = 1,         // Multiple consumers
        .priority_enabled = 1,          // Support priority pulling
        .retention_time = 3600          // Keep 1 hour of data
    };
    
    rgtp_setsockopt(sensor->rgtp_socket, RGTP_SOL_RGTP, RGTP_CONFIG, 
                   &config, sizeof(config));
    
    sensor->sensor_id = sensor_id;
    sensor->session_id = rand();
    sensor->reading_count = 0;
    sensor->running = 1;
    
    // Start sensor thread
    if (pthread_create(&sensor->thread, NULL, sensor_exposer_thread, sensor) != 0) {
        fprintf(stderr, "Failed to create sensor thread\n");
        rgtp_close(sensor->rgtp_socket);
        free(sensor);
        return NULL;
    }
    
    return sensor;
}

// Create data consumer
data_consumer_t* create_consumer(consumer_type_t type, const char* sensor_ip, int sensor_port) {
    data_consumer_t* consumer = malloc(sizeof(data_consumer_t));
    if (!consumer) return NULL;
    
    // Create RGTP socket (Layer 4, replaces TCP/UDP)
    consumer->rgtp_socket = rgtp_socket(AF_INET, RGTP_PULLER, 0);
    if (consumer->rgtp_socket < 0) {
        free(consumer);
        return NULL;
    }
    
    // Set target sensor address
    consumer->sensor_addr.sin_family = AF_INET;
    consumer->sensor_addr.sin_port = htons(sensor_port);
    inet_pton(AF_INET, sensor_ip, &consumer->sensor_addr.sin_addr);
    
    // Configure RGTP for consumer type
    rgtp_config_t config = {0};
    
    switch (type) {
        case CONSUMER_SCADA:
            config.priority = RGTP_PRIORITY_REALTIME;
            config.pull_strategy = RGTP_PULL_LATEST;
            break;
        case CONSUMER_ANALYTICS:
            config.priority = RGTP_PRIORITY_BATCH;
            config.pull_strategy = RGTP_PULL_RANGE;
            break;
        case CONSUMER_SAFETY:
            config.priority = RGTP_PRIORITY_CRITICAL;
            config.pull_strategy = RGTP_PULL_FILTERED;
            break;
    }
    
    rgtp_setsockopt(consumer->rgtp_socket, RGTP_SOL_RGTP, RGTP_CONFIG,
                   &config, sizeof(config));
    
    consumer->type = type;
    consumer->session_id = 0; // Will be discovered
    consumer->last_reading_id = 0;
    consumer->running = 1;
    
    // Start consumer thread
    if (pthread_create(&consumer->thread, NULL, consumer_thread, consumer) != 0) {
        fprintf(stderr, "Failed to create consumer thread\n");
        rgtp_close(consumer->rgtp_socket);
        free(consumer);
        return NULL;
    }
    
    return consumer;
}

int main(int argc, char* argv[]) {
    // Initialize random number generator for realistic sensor variations
    srand(time(NULL));
    
    if (argc < 2) {
        printf("Usage: %s <sensor|scada|analytics|safety> [sensor_ip]\n", argv[0]);
        printf("\nIndustrial IoT Demo using RGTP Layer 4 Protocol\n");
        printf("===============================================\n");
        printf("sensor     - Start temperature sensor (exposer)\n");
        printf("scada      - Start SCADA monitoring system (puller)\n");
        printf("analytics  - Start analytics system (puller)\n");
        printf("safety     - Start safety monitoring (puller)\n");
        return 1;
    }
    
    printf("Industrial IoT RGTP Demo\n");
    printf("========================\n");
    printf("RGTP replaces TCP/UDP at Layer 4\n");
    printf("Multiple consumers can pull from one sensor simultaneously\n\n");
    
    if (strcmp(argv[1], "sensor") == 0) {
        printf("Starting Industrial Sensor (RGTP Exposer)...\n");
        
        sensor_exposer_t* sensor = create_sensor(1001, SENSOR_PORT);
        if (!sensor) {
            fprintf(stderr, "Failed to create sensor\n");
            return 1;
        }
        
        printf("Sensor exposing data on RGTP port %d\n", SENSOR_PORT);
        printf("Consumers can connect and pull data simultaneously\n");
        printf("Press Ctrl+C to stop\n\n");
        
        // Keep running
        while (1) {
            sleep(1);
        }
        
    } else {
        const char* sensor_ip = argc > 2 ? argv[2] : "127.0.0.1";
        consumer_type_t type;
        
        if (strcmp(argv[1], "scada") == 0) {
            type = CONSUMER_SCADA;
        } else if (strcmp(argv[1], "analytics") == 0) {
            type = CONSUMER_ANALYTICS;
        } else if (strcmp(argv[1], "safety") == 0) {
            type = CONSUMER_SAFETY;
        } else {
            printf("Invalid consumer type\n");
            return 1;
        }
        
        printf("Starting %s Consumer (RGTP Puller)...\n", argv[1]);
        printf("Connecting to sensor at %s:%d\n\n", sensor_ip, SENSOR_PORT);
        
        data_consumer_t* consumer = create_consumer(type, sensor_ip, SENSOR_PORT);
        if (!consumer) {
            fprintf(stderr, "Failed to create consumer\n");
            return 1;
        }
        
        printf("Consumer started. Press Ctrl+C to stop\n\n");
        
        // Keep running
        while (1) {
            sleep(1);
        }
    }
    
    return 0;
}