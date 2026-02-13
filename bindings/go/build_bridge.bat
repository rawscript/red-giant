#!/bin/bash

# Build script for Go RGTP bridge library
# This creates a shared library that Node.js can interface with

echo "Building Go RGTP bridge library..."

# Build the shared library
GOOS=windows GOARCH=amd64 CGO_ENABLED=1 \
	go build -buildmode=c-shared -o rgtp_bridge.dll bridge.go

if [ $? -eq 0 ]; then
    echo "Successfully built rgtp_bridge.dll"
    echo "Library exports:"
    dumpbin /exports rgtp_bridge.dll | findstr "Rgtp"
else
    echo "Build failed!"
    exit 1
fi