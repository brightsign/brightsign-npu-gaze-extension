# GStreamer environment setup for RTSP streaming
# Extension plugins have priority, system plugins as fallback
export GST_PLUGIN_PATH=/var/volatile/bsext/ext_npu_gaze/RK3588/lib/gstreamer-1.0:/usr/lib/gstreamer-1.0
export LD_LIBRARY_PATH=/var/volatile/bsext/ext_npu_gaze/RK3588/lib:/usr/lib:$LD_LIBRARY_PATH
export GST_REGISTRY="/tmp/gst-registry.bin"
export GST_REGISTRY_REUSE_PLUGIN_SCANNER=1

# Note: If gstreamer-1.0 directory is empty, GStreamer will fall back to system plugins in /usr/lib/gstreamer-1.0
