
# Minimal, valid Meson options (headless-friendly)
EXTRA_OEMESON = " \
    -Dintrospection=disabled \
    -Dexamples=disabled \
    -Dnls=disabled \
    -Ddoc=disabled \
    -Dtests=disabled \
    -Dorc=enabled \
"

# Strip any accidental -Dvideoconvert=… injected by another layer
EXTRA_OEMESON:remove = "-Dvideoconvert=enabled"
EXTRA_OEMESON:remove = "-Dvideoconvert=disabled"

# Make sure introspection is off (your headless case) and not re-enabled elsewhere
EXTRA_OEMESON:remove = "-Dintrospection=enabled"
EXTRA_OEMESON:append = " -Dintrospection=disabled"

# (Optional: keep headless-friendly defaults here too)
PACKAGECONFIG:remove = " gl x11 wayland egl gles2 pango introspection "
PACKAGECONFIG:append = " orc "

# Strip any accidental, invalid Meson options that some layer injects
EXTRA_OEMESON:remove = "-Dvideoparsersbad=enabled"
EXTRA_OEMESON:remove = "-Dvideoparsersbad=disabled"

EXTRA_OEMESON:remove = "-Dvideo=enabled"
EXTRA_OEMESON:remove = "-Dvideo=disabled"
EXTRA_OEMESON:remove = "-Dvideo"

# Keep headless-friendly defaults; only valid keys here
PACKAGECONFIG:remove = " wayland x11 qt5 vulkan nvcodec sctp webrtc introspection "

# meta-bs/recipes-multimedia/gstreamer/gstreamer1.0-plugins-base_%.bbappend
EXTRA_OEMESON:append = " -Dapp=enabled "
EXTRA_OEMESON:append = " -Dvideoconvertscale=enabled "
EXTRA_OEMESON:append = " -Dvideoconvert=enabled "
