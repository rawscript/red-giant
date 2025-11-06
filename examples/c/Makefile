# Makefile for RGTP Layer 4 Protocol Examples
# 
# These examples demonstrate RGTP as a pure Layer 4 transport protocol
# that completely replaces TCP/UDP, operating directly over IP.

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -I../../include
LDFLAGS = -L../../lib -lrgtp -pthread

# Note: Some examples require root privileges for raw sockets
EXAMPLES = rgtp_demo simple_exposer simple_puller \
           http_over_rgtp_server http_over_rgtp_client \
           rgtp_layer4_demo industrial_iot_rgtp

.PHONY: all clean install help

all: $(EXAMPLES)

# Basic RGTP examples
rgtp_demo: rgtp_demo.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

simple_exposer: simple_exposer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

simple_puller: simple_puller.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# HTTP over RGTP examples (Layer 4 replacement)
http_over_rgtp_server: http_over_rgtp_server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Note: HTTP server uses RGTP instead of TCP at Layer 4"

http_over_rgtp_client: http_over_rgtp_client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Note: HTTP client uses RGTP instead of TCP at Layer 4"

# Pure Layer 4 RGTP demonstration (requires root)
rgtp_layer4_demo: rgtp_layer4_demo.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Note: This example requires root privileges for raw sockets"
	@echo "Usage: sudo ./rgtp_layer4_demo exposer"
	@echo "       sudo ./rgtp_layer4_demo puller [target_ip]"

# Industrial IoT example
industrial_iot_rgtp: industrial_iot_rgtp.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Note: Industrial IoT demo showing RGTP replacing TCP/UDP"
	@echo "Usage: ./industrial_iot_rgtp sensor"
	@echo "       ./industrial_iot_rgtp scada [sensor_ip]"
	@echo "       ./industrial_iot_rgtp analytics [sensor_ip]"
	@echo "       ./industrial_iot_rgtp safety [sensor_ip]"

clean:
	rm -f $(EXAMPLES) *.o

install: all
	mkdir -p /usr/local/bin/rgtp-examples
	cp $(EXAMPLES) /usr/local/bin/rgtp-examples/
	@echo "Examples installed to /usr/local/bin/rgtp-examples/"

help:
	@echo "RGTP Layer 4 Protocol Examples"
	@echo "=============================="
	@echo ""
	@echo "Available examples:"
	@echo "  rgtp_demo              - Basic RGTP functionality"
	@echo "  simple_exposer         - Simple data exposer"
	@echo "  simple_puller          - Simple data puller"
	@echo "  http_over_rgtp_server  - HTTP server using RGTP transport"
	@echo "  http_over_rgtp_client  - HTTP client using RGTP transport"
	@echo "  rgtp_layer4_demo       - Pure Layer 4 RGTP demo (needs root)"
	@echo "  industrial_iot_rgtp    - Industrial IoT scenario"
	@echo ""
	@echo "Key Concepts:"
	@echo "  - RGTP operates at Layer 4, replacing TCP/UDP entirely"
	@echo "  - No connection state management required"
	@echo "  - Natural multicast: one exposer serves multiple pullers"
	@echo "  - Instant resume capability for interrupted transfers"
	@echo "  - Pull-based flow control prevents congestion"
	@echo ""
	@echo "Build commands:"
	@echo "  make all      - Build all examples"
	@echo "  make clean    - Remove built files"
	@echo "  make install  - Install examples system-wide"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Example Usage:"
	@echo "  # Basic file transfer"
	@echo "  ./simple_exposer large_file.bin &"
	@echo "  ./simple_puller 127.0.0.1 9999 downloaded_file.bin"
	@echo ""
	@echo "  # HTTP over RGTP"
	@echo "  ./http_over_rgtp_server ./www 8080 &"
	@echo "  ./http_over_rgtp_client http://localhost:8080/file.zip"
	@echo ""
	@echo "  # Industrial IoT (multiple consumers, one sensor)"
	@echo "  ./industrial_iot_rgtp sensor &"
	@echo "  ./industrial_iot_rgtp scada &"
	@echo "  ./industrial_iot_rgtp analytics &"
	@echo "  ./industrial_iot_rgtp safety"

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: all

profile: CFLAGS += -pg
profile: all

# Test targets
test-basic: simple_exposer simple_puller
	@echo "Testing basic RGTP functionality..."
	@echo "This will start an exposer and puller to test data transfer"
	./simple_exposer /etc/passwd &
	sleep 1
	./simple_puller 127.0.0.1 9999 test_output.txt
	@echo "Test completed. Check test_output.txt"

test-http: http_over_rgtp_server http_over_rgtp_client
	@echo "Testing HTTP over RGTP..."
	@mkdir -p test_www
	@echo "<html><body><h1>RGTP Test</h1></body></html>" > test_www/index.html
	./http_over_rgtp_server test_www 8080 &
	sleep 1
	./http_over_rgtp_client http://localhost:8080/index.html test_download.html
	@echo "HTTP test completed. Check test_download.html"

.PHONY: debug profile test-basic test-http