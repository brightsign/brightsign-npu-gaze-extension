# Minimal, valid Meson flags (enough for RTSP pipelines; h264parse is built by default)
EXTRA_OEMESON = " \
    -Dintrospection=disabled \
    -Dexamples=disabled \
    -Ddoc=disabled \
    -Dtests=disabled \
    -Dorc=enabled \
    -Dgir=disabled \
    -Dmpegtsdemux=disabled \
    -Dmpegts=disabled \
    -Dgst_codecs=disabled \
    -Dcodecparsers=disabled \
"

# Strip invalid/bogus options that break configure (defensive)
EXTRA_OEMESON:remove = "-Dvideoparsersbad=enabled"
EXTRA_OEMESON:remove = "-Dvideoparsersbad=disabled"

# Disable introspection completely
GI_DATA_ENABLED = "False"

# Remove encoding profiles that aren't needed for RTSP streaming
do_install:append() {
    rm -rf ${D}${datadir}/gstreamer-1.0/encoding-profiles
    # Remove the parent directory if it's empty
    rmdir ${D}${datadir}/gstreamer-1.0 2>/dev/null || true
}

# Keep PACKAGECONFIG minimal but don't override completely
PACKAGECONFIG:remove = " \
  gl x11 wayland qt5 vulkan nvcodec kms va \
  webrtc webrtcdsp sctp \
  openal opencv openjpeg openh264 msdk aom libde265 modplug assrender faac faad fluidsynth \
  dash hls smoothstreaming curl curl-ssh2 rtmp srt srtp \
  bz2 webp rsvg sndfile sbc uvch264 zbar x265 dtls ttml avtp directfb dc1394 neon \
  introspection mpegts codecs codecparsers \
"
