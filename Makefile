CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -O2 -fPIC
GO=go

.PHONY: all clean demo adaptive pure concurrent traffic test

all: pure

pure: pure_go_demo.go
	$(GO) run pure_go_demo.go

concurrent: concurrent_demo.go
	$(GO) run concurrent_demo.go

traffic: traffic_demo.go
	$(GO) run traffic_demo.go

demo: red_giant.c red_giant.h main.go demo.go
	$(GO) run demo.go main.go

adaptive: red_giant.c red_giant.h main.go adaptive_demo.go traffic_monitor.go network_simulator.go
	$(GO) run adaptive_demo.go main.go traffic_monitor.go network_simulator.go

test: red_giant.c red_giant.h
	$(CC) $(CFLAGS) -DTEST_MODE red_giant.c -o test_red_giant
	./test_red_giant

clean:
	rm -f test_red_giant
	$(GO) clean

install-deps:
	$(GO) mod tidy

.PHONY: help
help:
	@echo "Red Giant Protocol - Build Commands"
	@echo "=================================="
	@echo "make pure       - Run the pure Go demonstration (recommended)"
	@echo "make concurrent - Run the concurrent processing demonstration"
	@echo "make traffic    - Run the traffic-aware adaptive demonstration"
	@echo "make demo       - Run the basic protocol demonstration (requires GCC)"
	@echo "make adaptive   - Run the full adaptive demonstration (requires GCC)"
	@echo "make test       - Run C unit tests"
	@echo "make clean      - Clean build artifacts"
	@echo "make help       - Show this help message"