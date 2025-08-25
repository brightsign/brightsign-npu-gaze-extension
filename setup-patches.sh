#!/bin/bash

# BrightSign OE Patch Application Script for Container
# Applies patches from mounted patch directory to the source

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date +'%T')] $1${NC}"
}

warn() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

error() {
    echo -e "${RED}❌ $1${NC}"
    exit 1
}

success() {
    echo -e "${GREEN}✅ $1${NC}"
}

# Check if source exists
if [[ ! -d "/home/builder/bsoe/brightsign-oe" ]]; then
    error "BrightSign OE source not found. Run setup-source.sh first."
fi

# Check if patches are mounted
if [[ ! -d "/home/builder/patches/meta-bs" ]]; then
    error "Patch directory not mounted at /home/builder/patches"
fi

log "Applying patches to BrightSign OE source..."

# Apply recipe patches using rsync with --delete to ensure clean state
rsync -av "/home/builder/patches/meta-bs/" "/home/builder/bsoe/brightsign-oe/meta-bs/"

# Apply local.conf patches if script exists
if [[ -f "/home/builder/host-scripts/patch-local-conf.sh" ]]; then
    log "Applying local.conf patches..."
    cd /home/builder/bsoe
    /home/builder/host-scripts/patch-local-conf.sh -y
fi

success "All patches applied successfully"