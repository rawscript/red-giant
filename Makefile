# Red Giant Protocol - Makefile
# Builds the C core and wrapper with proper optimization

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O3 -march=native -mtune=native
CFLAGS += -ffast-math -funroll-loops -finline-functions
CFLAGS += -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L

# Platform-specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -pthread
    LDFLAGS += -lm -lrt
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -pthread
    LDFLAGS += -lm
endif

# Debug build
DEBUG_CFLAGS = -std=c99 -Wall -Wextra -g -O0 -DDEBUG
DEBUG_CFLAGS += -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L

# Source files
CORE_SOURCES = red_giant.c red_giant_reliable.c
WRAPPER_SOURCES = red_giant_wrapper.c
TEST_SOURCES = test_wrapper.c
EXAMPLE_SOURCES = example_usage.c
INTEGRATION_SOURCES = integration_test.c

# Object files
CORE_OBJECTS = $(CORE_SOURCES:.c=.o)
WRAPPER_OBJECTS = $(WRAPPER_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
EXAMPLE_OBJECTS = $(EXAMPLE_SOURCES:.c=.o)
INTEGRATION_OBJECTS = $(INTEGRATION_SOURCES:.c=.o)

# Targets
.PHONY: all clean test debug install examples

all: test_wrapper example_usage integration_test

# Build optimized test program
test_wrapper: $(TEST_SOURCES) $(WRAPPER_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build example program
example_usage: $(EXAMPLE_SOURCES) $(WRAPPER_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build integration test
integration_test: $(INTEGRATION_SOURCES) $(WRAPPER_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build debug version
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: test_wrapper_debug

test_wrapper_debug: $(TEST_SOURCES) $(WRAPPER_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build static library
libred_giant.a: $(CORE_OBJECTS) $(WRAPPER_OBJECTS)
	ar rcs $@ $^

# Build shared library
libred_giant.so: $(CORE_SOURCES) $(WRAPPER_SOURCES)
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^ $(LDFLAGS)

# Individual object files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Run tests
test: test_wrapper
	@echo "Running Red Giant Protocol tests..."
	./test_wrapper

# Run examples
examples: example_usage
	@echo "Running Red Giant Protocol examples..."
	./example_usage

# Run integration tests
integration: integration_test
	@echo "Running Red Giant Protocol integration tests..."
	./integration_test

# Run with performance monitoring
perf-test: test_wrapper
	@echo "Running performance tests..."
	time ./test_wrapper

# Memory leak check (requires valgrind)
memcheck: test_wrapper_debug
	valgrind --leak-check=full --show-leak-kinds=all ./test_wrapper_debug

# Install headers and library
install: libred_giant.a libred_giant.so
	@echo "Installing Red Giant Protocol..."
	mkdir -p /usr/local/include/red_giant
	mkdir -p /usr/local/lib
	cp red_giant.h red_giant_wrapper.h /usr/local/include/red_giant/
	cp libred_giant.a libred_giant.so /usr/local/lib/
	ldconfig

# Clean build artifacts
clean:
	rm -f *.o test_wrapper test_wrapper_debug example_usage integration_test
	rm -f libred_giant.a libred_giant.so
	rm -f test_*.dat example_*.dat batch_file_*.dat performance_test.dat
	rm -f core vgcore.*

# Show compiler and system info
info:
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "System: $(UNAME_S)"
	@echo "Architecture: $(shell uname -m)"
	@$(CC) --version

# Benchmark build (maximum optimization)
benchmark: CFLAGS += -DNDEBUG -flto -fomit-frame-pointer
benchmark: test_wrapper

# Help
help:
	@echo "Red Giant Protocol - Build System"
	@echo "================================="
	@echo "Targets:"
	@echo "  all         - Build test program and examples (default)"
	@echo "  test_wrapper - Build optimized test program"
	@echo "  example_usage - Build example program"
	@echo "  debug       - Build debug version"
	@echo "  test        - Build and run tests"
	@echo "  examples    - Build and run examples"
	@echo "  perf-test   - Run performance tests"
	@echo "  memcheck    - Run memory leak check (requires valgrind)"
	@echo "  benchmark   - Build with maximum optimization"
	@echo "  libred_giant.a  - Build static library"
	@echo "  libred_giant.so - Build shared library"
	@echo "  install     - Install headers and libraries"
	@echo "  clean       - Clean build artifacts"
	@echo "  info        - Show compiler and system info"
	@echo "  help        - Show this help"