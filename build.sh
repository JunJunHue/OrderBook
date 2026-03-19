#!/bin/bash

# Build script for OrderBook project

set -e

echo "Building OrderBook project..."

# Create build directory
mkdir -p build
cd build

# Configure CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build complete! Executable: ./build/orderbook"
echo "Run with: ./build/orderbook"
