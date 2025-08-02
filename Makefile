CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -O2 -fPIC
GO=go

.PHONY: all clean demo test

all: demo

demo: red_giant.c red_giant.h main.go demo.go
	$(GO) run demo.go main.go

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
	@echo "make demo     - Run the full protocol demonstration"
	@echo "make test     - Run C unit tests"
	@echo "make clean    - Clean build artifacts"
	@echo "make help     - Show this help message"