#!/bin/bash

# RGTP HTTPS Demo Script
# This script demonstrates the HTTPS over RGTP implementation

echo "========================================"
echo "   RGTP HTTPS Demo Setup Script"
echo "========================================"
echo

# Check if required tools are available
echo "Checking for required tools..."
if ! command -v gcc &> /dev/null; then
    echo "Error: GCC compiler not found"
    exit 1
fi

if ! command -v openssl &> /dev/null; then
    echo "Error: OpenSSL not found"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "Error: Make not found"
    exit 1
fi

echo "All required tools found!"
echo

# Build the examples
echo "Building HTTPS over RGTP examples..."
cd ../..
make -C examples/c clean
make -C examples/c all

if [ $? -ne 0 ]; then
    echo "Error: Failed to build examples"
    exit 1
fi

echo "Examples built successfully!"
echo

# Create certificates
echo "Generating self-signed certificate..."
make -C examples/c cert

if [ $? -ne 0 ]; then
    echo "Error: Failed to generate certificate"
    exit 1
fi

echo "Certificate generated successfully!"
echo

# Create test directory
echo "Creating test directory structure..."
make -C examples/c test-dir

if [ $? -ne 0 ]; then
    echo "Error: Failed to create test directory"
    exit 1
fi

echo "Test directory created successfully!"
echo

# Show what we've created
echo "========================================"
echo "Demo setup complete! Here's what we created:"
echo
echo "Executables:"
ls -la examples/c/*_rgtp_*
echo
echo "Certificates:"
ls -la server.*
echo
echo "Test files:"
ls -la www/
echo
echo "========================================"
echo
echo "To run the demo:"
echo "1. Start the server in one terminal:"
echo "   make -C examples/c run-server"
echo
echo "2. In another terminal, run the client:"
echo "   make -C examples/c run-client"
echo
echo "Or run them manually:"
echo "   ./examples/c/https_rgtp_server ./www 8443"
echo "   ./examples/c/https_rgtp_client https://localhost:8443/index.html downloaded.html"
echo