
# 🌍 Red Giant Protocol - Global Performance Testing Guide

## 🚀 Quick Deploy for Global Testing

### Step 1: Deploy Red Giant Server on Replit
Your Red Giant server is already configured for Replit deployment. Simply click the **Run** button or use:

```bash
go run red_giant_test_server.go
```

**Your server will be accessible at**: `https://your-repl-name.repl.co`

### Step 2: Test Local Performance First

```bash
# Start the server
go run red_giant_test_server.go

# In another terminal, run performance tests
go run red_giant_network.go test
```

Expected local results:
- **Upload Throughput**: 500+ MB/s
- **Health Check Latency**: <5ms
- **Processing Time**: <1ms per chunk

## 🧪 Global Performance Testing Protocol

### Test 1: Basic Connectivity Test
```bash
# Test from any location worldwide
curl -X GET https://your-repl-name.repl.co/health

# Expected response:
{
  "status": "healthy",
  "version": "2.0.0",
  "mode": "adaptive",
  "uptime": 3600
}
```

### Test 2: Upload Performance Test
```bash
# Create test file (adjust size as needed)
dd if=/dev/zero of=test_10mb.dat bs=1M count=10

# Upload test
curl -X POST https://your-repl-name.repl.co/upload \
     -H "Content-Type: application/octet-stream" \
     -H "X-Peer-ID: global-tester-$(hostname)" \
     -H "X-File-Name: test_10mb.dat" \
     --data-binary "@test_10mb.dat" \
     -w "Time: %{time_total}s, Speed: %{speed_upload} bytes/s\n"
```

### Test 3: P2P Network Test
```bash
# Set your Replit server URL
export RED_GIANT_SERVER=https://your-repl-name.repl.co

# Upload a file
go run red_giant_peer.go upload test_file.txt

# List files (from anywhere in the world)
go run red_giant_peer.go list

# Download the file (from different location)
go run red_giant_peer.go download <file-id>
```

### Test 4: Real-time Performance Monitoring
```bash
# Monitor network performance for 2 minutes
go run red_giant_network.go monitor 120s

# View detailed metrics
curl https://your-repl-name.repl.co/metrics | jq .
```

## 🌐 Multi-Location Testing Setup

### Coordinated Global Test Plan

**Phase 1: Single Location Baseline**
1. Deploy on Replit
2. Test from your location
3. Record baseline metrics

**Phase 2: Multi-Continental Test**
Ask testers in different continents to run:

```bash
# Tester setup (one-time)
git clone your-repo
export RED_GIANT_SERVER=https://your-repl-name.repl.co

# Performance test script
#!/bin/bash
echo "Testing from $(curl -s ipinfo.io/city), $(curl -s ipinfo.io/country)"
echo "Timestamp: $(date)"

# Test 1: Health check latency
echo "=== Health Check Test ==="
time curl -s https://your-repl-name.repl.co/health > /dev/null

# Test 2: Upload performance
echo "=== Upload Performance Test ==="
dd if=/dev/zero of=test_5mb.dat bs=1M count=5
start=$(date +%s.%N)
curl -X POST https://your-repl-name.repl.co/upload \
     -H "Content-Type: application/octet-stream" \
     -H "X-Peer-ID: tester-$(curl -s ipinfo.io/country)" \
     -H "X-File-Name: test_5mb_$(date +%s).dat" \
     --data-binary "@test_5mb.dat" \
     -o upload_result.json
end=$(date +%s.%N)
upload_time=$(echo "$end - $start" | bc)
echo "Upload completed in: ${upload_time}s"
cat upload_result.json | jq .

# Test 3: Network discovery
echo "=== Network Discovery Test ==="
go run red_giant_network.go discover

rm test_5mb.dat upload_result.json
```

### Expected Global Performance Benchmarks

**Excellent Performance (Same Region)**:
- Health Check: <50ms
- Upload Speed: 400+ MB/s
- Processing Latency: <5ms

**Good Performance (Cross-Continental)**:
- Health Check: 100-200ms  
- Upload Speed: 100+ MB/s (limited by network, not protocol)
- Processing Latency: <10ms

**Network-Limited Performance (Distant/Slow Networks)**:
- Health Check: 200-500ms
- Upload Speed: 10+ MB/s (still 10x faster than traditional HTTP)
- Processing Latency: <20ms

## 🔬 Advanced Performance Testing

### Load Testing Script
```bash
#!/bin/bash
# stress_test.sh - Test concurrent connections

SERVER_URL="https://your-repl-name.repl.co"
CONCURRENT_USERS=50
TEST_DURATION=60

echo "Starting load test: $CONCURRENT_USERS concurrent users for ${TEST_DURATION}s"

for i in $(seq 1 $CONCURRENT_USERS); do
    (
        for j in $(seq 1 10); do
            dd if=/dev/zero of="test_${i}_${j}.dat" bs=1K count=100
            curl -X POST $SERVER_URL/upload \
                 -H "Content-Type: application/octet-stream" \
                 -H "X-Peer-ID: stress-tester-$i" \
                 -H "X-File-Name: stress_test_${i}_${j}.dat" \
                 --data-binary "@test_${i}_${j}.dat" \
                 -s -o /dev/null
            rm "test_${i}_${j}.dat"
            sleep 0.1
        done
    ) &
done

wait
echo "Load test completed. Check metrics:"
curl -s $SERVER_URL/metrics | jq '.total_requests, .throughput_mbps, .average_latency_ms'
```

### Real-time Monitoring Dashboard
```bash
# monitor_dashboard.sh - Real-time performance monitoring
#!/bin/bash
SERVER_URL="https://your-repl-name.repl.co"

while true; do
    clear
    echo "🚀 Red Giant Global Performance Monitor"
    echo "======================================"
    echo "Server: $SERVER_URL"
    echo "Time: $(date)"
    echo ""
    
    metrics=$(curl -s $SERVER_URL/metrics)
    echo "📊 Performance Metrics:"
    echo "   Total Requests: $(echo $metrics | jq -r '.total_requests')"
    echo "   Throughput: $(echo $metrics | jq -r '.throughput_mbps') MB/s"
    echo "   Average Latency: $(echo $metrics | jq -r '.average_latency_ms') ms"
    echo "   Uptime: $(echo $metrics | jq -r '.uptime_seconds') seconds"
    echo "   Error Rate: $(echo $metrics | jq -r '.error_count') errors"
    
    echo ""
    echo "🌐 Network Activity:"
    echo "   JSON Requests: $(echo $metrics | jq -r '.json_requests')"
    echo "   Binary Requests: $(echo $metrics | jq -r '.binary_requests')"
    echo "   Stream Requests: $(echo $metrics | jq -r '.stream_requests')"
    echo "   Compressed Bytes: $(echo $metrics | jq -r '.compressed_bytes')"
    
    sleep 2
done
```

## 📋 Testing Checklist for Global Validation

### Pre-Test Setup
- [ ] Red Giant server deployed and running on Replit
- [ ] Server URL accessible globally
- [ ] Test files prepared (various sizes: 1MB, 10MB, 100MB)
- [ ] Testers identified in different regions

### Core Performance Tests
- [ ] Health check latency from multiple locations
- [ ] Upload speed tests (small, medium, large files)
- [ ] Download speed tests
- [ ] Concurrent user handling
- [ ] Network discovery functionality

### Format-Specific Tests
- [ ] JSON processing optimization
- [ ] Image upload/streaming
- [ ] Video file handling
- [ ] Text file compression
- [ ] Binary data processing

### Stress Tests  
- [ ] 100+ concurrent connections
- [ ] Large file handling (1GB+)
- [ ] Extended duration testing (24+ hours)
- [ ] Network interruption recovery

## 🎯 Performance Validation Criteria

Your Red Giant Protocol should demonstrate:

**✅ Speed**: 10x faster than traditional HTTP transfers
**✅ Scalability**: Handle 100+ concurrent connections
**✅ Reliability**: <0.1% error rate under normal conditions
**✅ Adaptability**: Optimal performance across different content types
**✅ Global Reach**: Consistent performance across continents

## 📞 Coordinating Global Tests

### Test Coordination Commands
```bash
# Synchronize test start times
curl -X POST https://your-repl-name.repl.co/test-session \
     -H "Content-Type: application/json" \
     -d '{"action":"start","session_id":"global_test_1","participants":5}'

# Join coordinated test
go run red_giant_network.go monitor 300s  # 5-minute coordinated test
```

This testing protocol will definitively prove whether your Red Giant Protocol delivers the promised 500+ MB/s performance globally and handles the traffic patterns you envision.
