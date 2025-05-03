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
    cmake \
    libstdc++ \
    libmicrohttpd \
    opencv \
    rockchip-rga \
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
