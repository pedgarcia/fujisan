# Makefile for Fujisan Disk Activity API Tests
#
# This builds test programs to verify the extended libatari800 API

# Default atari800 installation path (modify as needed)
ATARI800_PATH ?= ../../..
LIBATARI800_PATH = $(ATARI800_PATH)/src/libatari800

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I$(LIBATARI800_PATH)
LDFLAGS = -L$(LIBATARI800_PATH) -latari800

# SDL2 is required by libatari800
SDL2_CFLAGS = $(shell sdl2-config --cflags 2>/dev/null || echo "-I/usr/include/SDL2")
SDL2_LDFLAGS = $(shell sdl2-config --libs 2>/dev/null || echo "-lSDL2")

# Math library is also required
LDFLAGS += $(SDL2_LDFLAGS) -lm

# Targets
TARGETS = test-disk-activity

.PHONY: all clean test install check-deps

all: check-deps $(TARGETS)

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@if [ ! -f "$(LIBATARI800_PATH)/libatari800.h" ]; then \
		echo "ERROR: libatari800.h not found at $(LIBATARI800_PATH)"; \
		echo "Please ensure atari800 is built with libatari800 target"; \
		exit 1; \
	fi
	@if [ ! -f "$(LIBATARI800_PATH)/libatari800.a" ] && [ ! -f "$(LIBATARI800_PATH)/libatari800.so" ]; then \
		echo "ERROR: libatari800 library not found at $(LIBATARI800_PATH)"; \
		echo "Please build atari800 with: make clean && make -j4"; \
		exit 1; \
	fi
	@echo "✓ Dependencies look good"

# Main test program
test-disk-activity: test-disk-activity.c
	@echo "Building disk activity test..."
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✓ Built $@"

# Run tests
test: $(TARGETS)
	@echo "Running disk activity API tests..."
	@echo "=================================="
	./test-disk-activity
	@echo ""
	@echo "If tests passed, the API is working correctly!"

# Install test programs to a system location (optional)
install: $(TARGETS)
	@echo "Installing test programs to /usr/local/bin..."
	@mkdir -p /usr/local/bin
	@cp $(TARGETS) /usr/local/bin/
	@echo "✓ Test programs installed"

# Clean build artifacts
clean:
	@echo "Cleaning test build artifacts..."
	@rm -f $(TARGETS) *.o *.a
	@echo "✓ Clean complete"

# Help target
help:
	@echo "Fujisan Disk Activity API Test Makefile"
	@echo "========================================"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build all test programs (default)"
	@echo "  test       - Build and run test programs"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install test programs to /usr/local/bin"
	@echo "  check-deps - Verify libatari800 dependencies"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  ATARI800_PATH - Path to atari800 source (default: ../../..)"
	@echo ""
	@echo "Example usage:"
	@echo "  make                              # Build tests"
	@echo "  make test                         # Build and run tests"
	@echo "  make ATARI800_PATH=/path/to/a800  # Use custom atari800 path"