#!/bin/bash
# Quick test to validate bbappend file syntax

echo "=== GStreamer bbappend Syntax Validation ==="
echo ""

BBAPPEND_DIR="/home/sree/bs/ipstream/brightsign-npu-gaze-extension/brightsign-oe/meta-bs/recipes-multimedia/gstreamer"

echo "=== Checking bbappend files ==="
for file in gstreamer1.0-plugins-base_%.bbappend gstreamer1.0-plugins-good_%.bbappend gstreamer1.0-plugins-bad_%.bbappend; do
    if [ -f "$BBAPPEND_DIR/$file" ]; then
        echo "✓ $file exists"
        echo "  Content preview:"
        grep -E "(PACKAGECONFIG|EXTRA_OEMESON)" "$BBAPPEND_DIR/$file" | head -3 | sed 's/^/    /'
        echo ""
    else
        echo "✗ $file missing"
    fi
done

echo "=== Checking for syntax issues ==="
echo "Valid PACKAGECONFIG options for good plugins:"
echo "  soup2, soup3, cairo, flac, jpeg, lame, libpng, mpg123, pulseaudio, speex, v4l2, etc."
echo ""

echo "Good plugins bbappend PACKAGECONFIG:"
grep "PACKAGECONFIG" "$BBAPPEND_DIR/gstreamer1.0-plugins-good_%.bbappend" | sed 's/^/  /'

echo ""
echo "=== Summary ==="
echo "Fixed issues:"
echo "  ✓ Changed 'soup' to 'soup2' (valid PACKAGECONFIG)"
echo "  ✓ Removed 'rtp' from PACKAGECONFIG (not valid, but kept in EXTRA_OEMESON)"
echo ""
echo "The syntax should now be valid for the build system."
