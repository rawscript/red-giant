# examples/c/Makefile
#
# These examples are built via CMake (recommended):
#
#   cmake -B ../../build -DRGTP_BUILD_EXAMPLES=ON -DRGTP_ENABLE_FEC=ON
#   cmake --build ../../build --parallel
#
# The targets below are a convenience fallback for quick local builds
# when librgtp is already installed on the system.

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c17 -O2 -I../../include
LDFLAGS = -lrgtp -lsodium -lpthread -lm

all: udp_expose_file udp_pull_file

udp_expose_file: udp_expose_file.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

udp_pull_file: udp_pull_file.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f udp_expose_file udp_pull_file

# Quick demo (requires librgtp installed and a test file)
demo: all
	@echo "=== RGTP Demo ==="
	@echo "Starting exposer in background..."
	./udp_expose_file /etc/hostname 9000 &
	@sleep 1
	@echo ""
	@echo "Pulling from localhost:9000..."
	./udp_pull_file 127.0.0.1:9000 $$(./udp_expose_file /etc/hostname 9000 2>&1 | grep 'Exposure ID' | awk '{print $$3}') received.bin || true
	@echo ""
	@echo "Kill the exposer with: kill %1"

.PHONY: all clean demo
