#!/bin/bash
# Script to rebuild just the GStreamer plugins to test our bbappend changes

echo "=== Rebuilding GStreamer Plugins with bbappend Changes ==="
echo ""

# Check if we're in the right directory
if [ ! -d "brightsign-oe/build" ]; then
    echo "Error: Run this from the project root directory"
    exit 1
fi

cd brightsign-oe/build

echo "=== Cleaning existing GStreamer builds ==="
MACHINE=cobra ./bsbb -c cleansstate gstreamer1.0-plugins-base
MACHINE=cobra ./bsbb -c cleansstate gstreamer1.0-plugins-good 
MACHINE=cobra ./bsbb -c cleansstate gstreamer1.0-plugins-bad

echo ""
echo "=== Rebuilding GStreamer plugins ==="
echo "Building base plugins..."
MACHINE=cobra ./bsbb gstreamer1.0-plugins-base

echo "Building good plugins..."
MACHINE=cobra ./bsbb gstreamer1.0-plugins-good

echo "Building bad plugins..."
MACHINE=cobra ./bsbb gstreamer1.0-plugins-bad

echo ""
echo "=== Rebuilding SDK to include new plugins ==="
MACHINE=cobra ./bsbb brightsign-sdk

echo ""
echo "=== Verification ==="
echo "Run this to check if plugins are now available:"
echo "  cd ../../ && ./scripts/verify_plugin_build.sh"
