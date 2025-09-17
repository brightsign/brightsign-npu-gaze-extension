# Minimal, valid Meson flags (enough for RTSP pipelines; h264parse is built by default)
EXTRA_OEMESON = " \
    -Dintrospection=disabled \
    -Dexamples=disabled \
    -Ddoc=disabled \
    -Dtests=disabled \
    -Dorc=enabled \
"

# Strip invalid/bogus options that break configure (defensive)
EXTRA_OEMESON:remove = "-Dvideoparsersbad=enabled"
EXTRA_OEMESON:remove = "-Dvideoparsersbad=disabled"

# Keep PACKAGECONFIG lean; remove GUI/GL/Wayland/WebRTC/etc if any layer added them
PACKAGECONFIG = "orc"
PACKAGECONFIG:remove = " \
  gl x11 wayland qt5 vulkan nvcodec kms va v4l2codecs \
  webrtc webrtcdsp sctp \
  openal opencv openjpeg openh264 msdk aom libde265 modplug assrender faac faad fluidsynth \
  dash hls smoothstreaming curl curl-ssh2 rtmp srt srtp \
  bz2 webp rsvg sndfile sbc uvch264 zbar x265 dtls ttml avtp directfb dc1394 neon \
  introspection \
"
