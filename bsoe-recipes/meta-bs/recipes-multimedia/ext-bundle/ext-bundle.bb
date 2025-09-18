SUMMARY = "SDK-only tarball of GStreamer runtime for the BrightSign extension"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

# Ensure these are built so their files exist in STAGING
DEPENDS = "gstreamer1.0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad"

S = "${WORKDIR}"

# Where we’ll install the opaque payload tarball in the SDK target sysroot
DATADIR_PAYLOAD = "${datadir}/ext-bundle"
PAYLOAD_TGZ = "ext-bundle-payload.tar.gz"

# Keep packaging simple: one binary package only
PACKAGES = "${PN}"

# We install a tarball, not actual .so files – but skip any QA noise anyway
INHIBIT_PACKAGE_SHLIBS:${PN} = "1"
INHIBIT_PACKAGE_DEBUG_SPLIT = "1"
INSANE_SKIP:${PN} += "libdir dev-so already-stripped ldflags rpaths"

do_install() {
    # Build payload into a scratch dir outside ${D}
    mkdir -p ${WORKDIR}/payload/lib ${WORKDIR}/payload/gstreamer-1.0

    # ---- Copy versioned runtime libs (don’t copy unversioned .so) ----
    for f in \
        ${STAGING_LIBDIR}/libgstreamer-1.0.so.* \
        ${STAGING_LIBDIR}/libgstbase-1.0.so.* \
        ${STAGING_LIBDIR}/libgstapp-1.0.so.* \
        ${STAGING_LIBDIR}/libgstvideo-1.0.so.* \
        ${STAGING_LIBDIR}/libgstrtsp-1.0.so.* \
        ${STAGING_LIBDIR}/liborc-0.4.so.* ; do
        [ -e "$f" ] && install -m0644 "$f" ${WORKDIR}/payload/lib/
    done

    # ---- GStreamer plugin modules (GStreamer >= 1.22 names) ----
    for p in libgstapp.so libgstvideoconvertscale.so libgstvideoconvert.so libgstrtsp.so libgstrtp.so libgstrtpmanager.so libgstvideoparsersbad.so ; do
        [ -f ${STAGING_LIBDIR}/gstreamer-1.0/$p ] && \
            install -m0644 ${STAGING_LIBDIR}/gstreamer-1.0/$p ${WORKDIR}/payload/gstreamer-1.0/
    done

    # ---- GStreamer tools ----
    for b in gst-launch-1.0 gst-inspect-1.0 ; do
        if [ -f ${STAGING_BINDIR}/$b ]; then
            install -m0755 ${STAGING_BINDIR}/$b ${WORKDIR}/payload/bin/
        fi
    done

    # Create the opaque tarball we’ll ship in the SDK
    install -d ${D}${DATADIR_PAYLOAD}
    tar -C ${WORKDIR}/payload -czf ${D}${DATADIR_PAYLOAD}/${PAYLOAD_TGZ} .
}
#RDEPENDS:${PN} += "gstreamer1.0 (= ${PV})"
RDEPENDS:${PN} += "gstreamer1.0"

# Only ship the tarball
FILES:${PN} = "${DATADIR_PAYLOAD}/${PAYLOAD_TGZ}"
