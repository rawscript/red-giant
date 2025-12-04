# examples/c/Makefile — UDP Edition only
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -O3 -I../../include
SRC    = ../../src/core/rgtp_core.c

all: udp_expose_file udp_pull_file

udp_expose_file: udp_expose_file.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

udp_pull_file: udp_pull_file.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f udp_expose_file udp_pull_file

demo:
	@echo "=== RED GIANT UDP REVOLUTION ==="
	./udp_expose_file ../../docs/advantages.md &
	@sleep 2
	@echo "Now run in another terminal:"
	@echo "./udp_pull_file 127.0.0.1 advantages.md"