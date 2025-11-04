# Red Giant Transport Protocol (RGTP) - Makefile
# Production-ready build system

# Project information
PROJECT_NAME = rgtp
VERSION = 1.0.0
DESCRIPTION = Red Giant Transport Protocol - Layer 4 Implementation

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
LIB_DIR = lib
BIN_DIR = bin
TEST_DIR = tests
EXAMPLE_DIR = examples
DOC_DIR = docs

# Compiler settings
CC = gcc
CXX = g++
AR = ar
RANLIB = ranlib

# Compiler flags
CFLAGS_BASE = -std=c99 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
CFLAGS_RELEASE = $(CFLAGS_BASE) -O3 -DNDEBUG -march=native -flto
CFLAGS_PROFILE = $(CFLAGS_RELEASE) -pg -fprofile-arcs -ftest-coverage

# Include paths
INCLUDES = -I$(INCLUDE_DIR)

# Library flags
LDFLAGS = -L$(LIB_DIR)
LIBS = -lrgtp -lpthread -lm

# Source files
CORE_SOURCES = $(wildcard $(SRC_DIR)/core/*.c)
CORE_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Library targets
STATIC_LIB = $(LIB_DIR)/librgtp.a
SHARED_LIB = $(LIB_DIR)/librgtp.so.$(VERSION)
SHARED_LIB_LINK = $(LIB_DIR)/librgtp.so

# Example targets
EXAMPLES = $(patsubst $(EXAMPLE_DIR)/c/%.c,$(BIN_DIR)/%,$(wildcard $(EXAMPLE_DIR)/c/*.c))

# Test targets  
TESTS = $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/test_%,$(wildcard $(TEST_DIR)/*.c))

# Default build type
BUILD_TYPE ?= release

# Set flags based on build type
ifeq ($(BUILD_TYPE),debug)
    CFLAGS = $(CFLAGS_DEBUG)
else ifeq ($(BUILD_TYPE),profile)
    CFLAGS = $(CFLAGS_PROFILE)
else
    CFLAGS = $(CFLAGS_RELEASE)
endif

# Default target
.PHONY: all
all: directories $(STATIC_LIB) $(SHARED_LIB) examples

# Create necessary directories
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)/core $(LIB_DIR) $(BIN_DIR)

# Static library
$(STATIC_LIB): $(CORE_OBJECTS)
	@echo "Creating static library: $@"
	@$(AR) rcs $@ $^
	@$(RANLIB) $@

# Shared library
$(SHARED_LIB): $(CORE_OBJECTS)
	@echo "Creating shared library: $@"
	@$(CC) -shared -Wl,-soname,librgtp.so.1 -o $@ $^ $(LIBS)
	@ln -sf librgtp.so.$(VERSION) $(SHARED_LIB_LINK)

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Examples
.PHONY: examples
examples: $(EXAMPLES) go-examples node-bindings

$(BIN_DIR)/%: $(EXAMPLE_DIR)/c/%.c $(STATIC_LIB)
	@echo "Building example: $@"
	@$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) $(LIBS)

# Go examples
.PHONY: go-examples
go-examples: $(STATIC_LIB)
	@echo "Building Go examples..."
	@cd examples/go && go build -o ../../$(BIN_DIR)/rgtp_go_example simple_transfer.go

# Node.js bindings
.PHONY: node-bindings
node-bindings: $(STATIC_LIB)
	@echo "Building Node.js bindings..."
	@cd bindings/node && npm install && npm run build

# Tests
.PHONY: test
test: $(TESTS)
	@echo "Running tests..."
	@for test in $(TESTS); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done

$(BIN_DIR)/test_%: $(TEST_DIR)/%.c $(STATIC_LIB)
	@echo "Building test: $@"
	@$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) $(LIBS)

# Build variants
.PHONY: debug release profile
debug:
	@$(MAKE) BUILD_TYPE=debug

release:
	@$(MAKE) BUILD_TYPE=release

profile:
	@$(MAKE) BUILD_TYPE=profile

# Benchmarks
.PHONY: benchmark
benchmark: $(BIN_DIR)/rgtp_demo
	@echo "Running performance benchmarks..."
	@$(BIN_DIR)/rgtp_demo

# Documentation
.PHONY: docs
docs:
	@echo "Generating documentation..."
	@doxygen Doxyfile 2>/dev/null || echo "Doxygen not found, skipping docs"

# Installation
PREFIX ?= /usr/local
INSTALL_LIB_DIR = $(PREFIX)/lib
INSTALL_INCLUDE_DIR = $(PREFIX)/include
INSTALL_BIN_DIR = $(PREFIX)/bin

.PHONY: install
install: all
	@echo "Installing RGTP..."
	@install -d $(INSTALL_LIB_DIR) $(INSTALL_INCLUDE_DIR) $(INSTALL_BIN_DIR)
	@install -m 644 $(STATIC_LIB) $(INSTALL_LIB_DIR)/
	@install -m 755 $(SHARED_LIB) $(INSTALL_LIB_DIR)/
	@ln -sf librgtp.so.$(VERSION) $(INSTALL_LIB_DIR)/librgtp.so
	@cp -r $(INCLUDE_DIR)/rgtp $(INSTALL_INCLUDE_DIR)/
	@install -m 755 $(BIN_DIR)/rgtp_* $(INSTALL_BIN_DIR)/ 2>/dev/null || true
	@ldconfig 2>/dev/null || true
	@echo "RGTP installed successfully!"

.PHONY: uninstall
uninstall:
	@echo "Uninstalling RGTP..."
	@rm -f $(INSTALL_LIB_DIR)/librgtp.*
	@rm -rf $(INSTALL_INCLUDE_DIR)/rgtp
	@rm -f $(INSTALL_BIN_DIR)/rgtp_*
	@ldconfig 2>/dev/null || true

# Packaging
.PHONY: package
package: clean all
	@echo "Creating package..."
	@tar -czf rgtp-$(VERSION).tar.gz \
		--exclude='.git*' \
		--exclude='build' \
		--exclude='*.tar.gz' \
		--transform 's,^,rgtp-$(VERSION)/,' \
		*

# Cross-compilation
CROSS_COMPILE ?=
ifneq ($(CROSS_COMPILE),)
    CC = $(CROSS_COMPILE)gcc
    AR = $(CROSS_COMPILE)ar
    RANLIB = $(CROSS_COMPILE)ranlib
endif

.PHONY: cross-arm64
cross-arm64:
	@$(MAKE) CROSS_COMPILE=aarch64-linux-gnu-

.PHONY: cross-arm
cross-arm:
	@$(MAKE) CROSS_COMPILE=arm-linux-gnueabihf-

# Static analysis
.PHONY: analyze
analyze:
	@echo "Running static analysis..."
	@cppcheck --enable=all --std=c99 $(SRC_DIR) $(INCLUDE_DIR) 2>/dev/null || echo "cppcheck not found"
	@clang-static-analyzer $(SRC_DIR)/*.c 2>/dev/null || echo "clang static analyzer not found"

# Code formatting
.PHONY: format
format:
	@echo "Formatting code..."
	@find $(SRC_DIR) $(INCLUDE_DIR) $(EXAMPLE_DIR) $(TEST_DIR) -name "*.c" -o -name "*.h" | \
		xargs clang-format -i 2>/dev/null || echo "clang-format not found"

# Memory leak check
.PHONY: memcheck
memcheck: debug
	@echo "Running memory leak check..."
	@valgrind --leak-check=full --show-leak-kinds=all $(BIN_DIR)/rgtp_demo 2>/dev/null || \
		echo "valgrind not found"

# Performance profiling
.PHONY: perf
perf: profile
	@echo "Running performance profiling..."
	@$(BIN_DIR)/rgtp_demo
	@gprof $(BIN_DIR)/rgtp_demo gmon.out > performance_report.txt 2>/dev/null || \
		echo "gprof not found"

# Clean targets
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR)
	@rm -f *.gcov *.gcda *.gcno gmon.out performance_report.txt

.PHONY: distclean
distclean: clean
	@echo "Deep cleaning..."
	@rm -f *.tar.gz
	@rm -rf docs/html docs/latex

# Help
.PHONY: help
help:
	@echo "Red Giant Transport Protocol (RGTP) Build System"
	@echo "================================================"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build libraries and examples (default)"
	@echo "  debug       - Build with debug flags"
	@echo "  release     - Build optimized release version"
	@echo "  profile     - Build with profiling enabled"
	@echo "  examples    - Build example programs"
	@echo "  test        - Build and run tests"
	@echo "  benchmark   - Run performance benchmarks"
	@echo "  docs        - Generate documentation"
	@echo "  install     - Install system-wide"
	@echo "  uninstall   - Remove system installation"
	@echo "  package     - Create distribution package"
	@echo "  analyze     - Run static analysis"
	@echo "  format      - Format source code"
	@echo "  memcheck    - Check for memory leaks"
	@echo "  perf        - Performance profiling"
	@echo "  clean       - Remove build artifacts"
	@echo "  distclean   - Deep clean everything"
	@echo ""
	@echo "Cross-compilation:"
	@echo "  cross-arm64 - Cross-compile for ARM64"
	@echo "  cross-arm   - Cross-compile for ARM32"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE  - debug, release, profile (default: release)"
	@echo "  PREFIX      - Installation prefix (default: /usr/local)"
	@echo "  CC          - C compiler (default: gcc)"
	@echo ""
	@echo "Examples:"
	@echo "  make debug"
	@echo "  make test"
	@echo "  make install PREFIX=/opt/rgtp"
	@echo "  make cross-arm64"

# Version information
.PHONY: version
version:
	@echo "RGTP Version: $(VERSION)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"

# Show configuration
.PHONY: config
config: version
	@echo "Configuration:"
	@echo "  Source Dir: $(SRC_DIR)"
	@echo "  Include Dir: $(INCLUDE_DIR)"
	@echo "  Build Dir: $(BUILD_DIR)"
	@echo "  Library Dir: $(LIB_DIR)"
	@echo "  Binary Dir: $(BIN_DIR)"
	@echo "  Install Prefix: $(PREFIX)"