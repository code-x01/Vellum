#!/bin/bash

# Vellum Run Script
# This script builds and runs the Vellum hypervisor on Linux (WSL2 recommended for Windows users)

set -e

echo "=== Vellum Hypervisor Setup ==="

# Check if we're on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Error: This script requires Linux. For Windows users, run this in WSL2."
    exit 1
fi

# Check for required tools
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake is required but not installed."; exit 1; }
command -v make >/dev/null 2>&1 || { echo "Error: make is required but not installed."; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "Error: g++ is required but not installed."; exit 1; }
command -v npm >/dev/null 2>&1 || { echo "Error: npm is required but not installed."; exit 1; }

# Check for KVM
if [[ ! -c /dev/kvm ]]; then
    echo "Warning: /dev/kvm not found. KVM may not be available."
    echo "On WSL2, ensure virtualization is enabled in Windows and WSL2 is configured."
fi

# Build backend
echo "Building backend..."
mkdir -p vellum/build
cd vellum/build
cmake ..
make -j$(nproc)

# Build frontend
echo "Building frontend..."
cd ../frontend
npm install
npm run build

# Run backend
echo "Starting Vellum backend..."
cd ../build
echo ""
echo "=== Vellum is running ==="
echo "Backend: http://localhost:8080 (API/WebSocket)"
echo "Frontend: http://localhost:3000 (React UI)"
echo ""
echo "To access the UI, open http://localhost:3000 in your browser"
echo "To stop, press Ctrl+C"
echo ""

./vellum