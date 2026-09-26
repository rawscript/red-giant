#!/bin/bash
# dev_setup.sh — Setup development environment for RGTP Python bindings

set -e

echo "========================================"
echo "RGTP Python Development Setup"
echo "========================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Check Python
echo "Checking Python installation..."
if command -v python3 &> /dev/null; then
    PYTHON=python3
elif command -v python &> /dev/null; then
    PYTHON=python
else
    print_error "Python not found"
    exit 1
fi

PYTHON_VERSION=$($PYTHON --version 2>&1 | awk '{print $2}')
print_success "Python $PYTHON_VERSION found"

# Check pip
echo ""
echo "Checking pip..."
if ! $PYTHON -m pip --version &> /dev/null; then
    print_error "pip not found"
    exit 1
fi
print_success "pip found"

# Check CMake
echo ""
echo "Checking CMake..."
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
    print_success "CMake $CMAKE_VERSION found"
else
    print_warning "CMake not found (needed for building librgtp)"
    echo "  Install with: pip install cmake"
fi

# Check libsodium
echo ""
echo "Checking libsodium..."
if [ "$(uname)" = "Darwin" ]; then
    if brew list libsodium &> /dev/null; then
        print_success "libsodium found (Homebrew)"
    else
        print_warning "libsodium not found"
        echo "  Install with: brew install libsodium"
    fi
elif [ "$(uname)" = "Linux" ]; then
    if dpkg -l libsodium-dev &> /dev/null; then
        print_success "libsodium-dev found (dpkg)"
    elif rpm -qa | grep libsodium-devel &> /dev/null; then
        print_success "libsodium-devel found (rpm)"
    else
        print_warning "libsodium not found"
        echo "  Install with: sudo apt-get install libsodium-dev"
        echo "  Or: sudo yum install libsodium-devel"
    fi
else
    print_warning "libsodium check not implemented for this platform"
fi

# Install Python dependencies
echo ""
echo "Installing Python development dependencies..."
$PYTHON -m pip install --upgrade pip
$PYTHON -m pip install -e ".[dev]"

print_success "Development dependencies installed"

# Verify package
echo ""
echo "Verifying package..."
$PYTHON verify_package.py

if [ $? -eq 0 ]; then
    print_success "Package verification passed"
else
    print_error "Package verification failed"
    exit 1
fi

# Try to build the library
echo ""
echo "Building librgtp..."
if [ -x "$(command -v cmake)" ]; then
    $PYTHON build_library.py && print_success "Library built successfully" || print_warning "Library build failed (expected if dependencies missing)"
else
    print_warning "Skipping library build (CMake not found)"
fi

echo ""
echo "========================================"
echo "Development Setup Complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "  1. Build the C library: python build_library.py"
echo "  2. Run tests: pytest tests/"
echo "  3. Build package: python -m build"
echo ""
