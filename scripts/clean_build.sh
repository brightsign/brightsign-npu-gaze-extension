#!/bin/bash
# Script to clear bitbake cache and rebuild with fresh layer configuration

echo "=== Clearing BitBake Cache and Rebuilding ==="
echo ""

if [ ! -d "brightsign-oe/build" ]; then
    echo "Error: Run this from the project root directory"
    exit 1
fi

cd brightsign-oe/build

echo "=== Clearing BitBake cache ==="
# Clear bitbake's internal cache
rm -rf cache/
rm -rf bitbake-cookerdaemon.log*

echo "=== Verifying layer configuration ==="
echo "bs-layer collection name:"
grep "BBFILE_COLLECTIONS" ../meta-bs/conf/layer.conf

echo "bs-husk-layer dependencies:"
grep "LAYERDEPENDS" ../meta-bs-husk/conf/layer.conf

echo ""
echo "=== Attempting build with fresh cache ==="
MACHINE=cobra ./bsbb brightsign-sdk

echo ""
echo "Build completed. Check status above."
