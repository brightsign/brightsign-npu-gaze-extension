# This recipe hijacks oe-core's SDK-generation mechanism to build a
# BrightSign-specific SDK.
#
# Unfortunately it needs to be built and therefore installed
# separately for AArch64 and ARM.

PACKAGE_ARCH = "${MACHINE_ARCH}"

require conf/brightsign-version.conf
PV = "${BOS_VERSION}"

TOOLCHAIN_HOST_TASK = "\
    packagegroup-cross-canadian-${MACHINE} \
    nativesdk-python-modules \
"

TOOLCHAIN_TARGET_TASK += "\
    libstdc++ \
    libmicrohttpd \
    opencv \
    rockchip-rga \
"

# --- GStreamer: core + plugins needed for RTSP -> BGR via appsink ---
# Using broader package approach since specific sub-packages don't exist  
# Adding development packages to ensure complete plugin sets are built
TOOLCHAIN_TARGET_TASK:append = " \
    gstreamer1.0 \
    gstreamer1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-base-dev \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-good-dev \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-bad-dev \
    gstreamer1.0-libav \
"

# Try to force inclusion of all available plugins by adding complete sets
# Some plugins may be disabled by default in the build configuration  
TOOLCHAIN_TARGET_TASK:append = " \
    gstreamer1.0-plugins-base-meta \
    gstreamer1.0-plugins-good-meta \
    gstreamer1.0-plugins-bad-meta \
"

# The filename ends up with "toolchain" in it later (via
# TOOLCHAIN_OUTPUTNAME) so we don't really need to say "sdk".
SDK_NAME_PREFIX = "brightsign"
SDK_VERSION = "${PV}"

# Malibu and Cobra are the same TUNE_PKGARCH, but are actually
# different due to the former being multilib and the latter not. We'd
# better ensure that the SDK gets named after MACHINE instead.
SDK_NAME = "${SDK_NAME_PREFIX}-${SDK_ARCH}-${MACHINE_ARCH}"

SUMMARY = "BrightSign extensions SDK"
LICENSE = "MIT"

PR = "r1"

LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

inherit populate_sdk
