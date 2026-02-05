    # RGTP Performance Monitoring and Analytics System

This directory contains the performance monitoring and analytics system for RGTP (Red Giant Transport Protocol). The system provides comprehensive monitoring, event tracking, and analytics capabilities for RGTP transfers.

## Overview

The performance monitoring system provides:

- **Real-time Performance Tracking**: Monitors RGTP transfers in real-time
- **Comprehensive Event Logging**: Tracks all significant RGTP events
- **Advanced Analytics**: Calculates performance metrics and trends
- **Reporting Capabilities**: Generates detailed performance reports in multiple formats
- **Resource Utilization Monitoring**: Tracks memory, CPU, and bandwidth usage

## Components

### Core Files
- `perf_monitor.h` - Header file defining the performance monitoring API
- `perf_monitor.c` - Implementation of the performance monitoring system
- `test_perf_monitor.c` - Test suite for the monitoring system

### Key Features
- Real-time performance tracking
- Event-based monitoring with timestamps
- Advanced metrics calculation (throughput, RTT, packet loss, etc.)
- Multiple report formats (text, JSON, CSV)
- Thread-safe operation
- Configurable logging and sampling intervals

## Architecture

The monitoring system uses an event-driven architecture:

```
RGTP Operations --> Event Recorder --> Metrics Calculator --> Report Generator
```

### Event Types Tracked
- Transfer start/end events
- Packet sent/received events
- Retransmission events
- Congestion control adjustments
- RTT measurements
- Error events

### Metrics Collected
- Transfer metrics (bytes sent/received, packets, errors)
- Performance metrics (throughput, RTT, packet loss rate)
- Congestion control metrics (rate adjustments, increases/decreases)
- Resource utilization (memory, CPU)

## Usage

The API provides functions for:

1. **Monitor Management**:
   - Creating and configuring monitors
   - Starting and stopping monitoring
   - Managing monitor lifecycle

2. **Event Recording**:
   - Recording transfer events
   - Logging packet transmission/reception
   - Tracking congestion adjustments
   - Recording RTT measurements
   - Logging errors

3. **Metrics Access**:
   - Retrieving current performance metrics
   - Getting transfer-specific metrics
   - Accessing session-level metrics

4. **Reporting**:
   - Generating comprehensive performance reports
   - Exporting data in JSON format
   - Exporting data in CSV format
   - Exporting raw event logs

## Integration

The performance monitoring system integrates with RGTP operations by:

- Providing callback mechanisms for automatic event recording
- Offering lightweight metrics collection during transfers
- Supporting both synchronous and asynchronous monitoring modes
- Enabling detailed analysis of RGTP performance characteristics

## Configuration

The system is highly configurable:

- Enable/disable real-time monitoring
- Configure logging levels and destinations
- Set sampling intervals
- Define log file rotation policies
- Customize report generation

## Testing

Run the test suite with:
```bash
gcc -o test_perf_monitor test_perf_monitor.c perf_monitor.c -lpthread
./test_perf_monitor
```

## Output Formats

The system supports multiple output formats:

- **Text Reports**: Human-readable performance summaries
- **JSON**: Structured data for programmatic consumption
- **CSV**: Tabular data for spreadsheet analysis
- **Event Logs**: Raw event data for detailed analysis

## Performance Impact

The monitoring system is designed to have minimal impact on RGTP performance:
- Lightweight event recording
- Efficient data structures
- Asynchronous processing where possible
- Configurable sampling rates

## Status

This is a comprehensive implementation of the performance monitoring and analytics system for RGTP. It provides the foundation for detailed performance analysis and optimization of RGTP transfers.