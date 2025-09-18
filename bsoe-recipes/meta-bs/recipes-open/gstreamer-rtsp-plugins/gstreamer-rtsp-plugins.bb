SUMMARY = "GStreamer plugin ensemble for RTSP streaming"
DESCRIPTION = "Custom package to ensure GStreamer RTSP plugins are properly included in SDK"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# This is a meta-package, no actual files
ALLOW_EMPTY:${PN} = "1"

# Runtime dependencies - these packages will be installed
RDEPENDS:${PN} = " \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    gstreamer1.0-rtsp-server \
"

# Development packages for SDK
RDEPENDS:${PN}-dev = " \
    gstreamer1.0-dev \
    gstreamer1.0-plugins-base-dev \
    gstreamer1.0-plugins-good-dev \
    gstreamer1.0-plugins-bad-dev \
"

# Build dependencies to ensure they're available
DEPENDS = " \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    gstreamer1.0-rtsp-server \
"
