#export LD_LIBRARY_PATH="/var/volatile/bsext/ext_npu_gaze/RK3588/lib:${LD_LIBRARY_PATH}"
#export GST_PLUGIN_PATH="/usr/lib/gstreamer-1.0:/var/volatile/bsext/ext_npu_gaze/RK3588/lib/gstreamer-1.0"
export GST_PLUGIN_PATH=/usr/lib/gstreamer-1.0:/var/volatile/bsext/ext_npu_gaze/RK3588/lib/gstreamer-1.0
export LD_LIBRARY_PATH=/var/volatile/bsext/ext_npu_gaze/RK3588/lib:/usr/lib:$LD_LIBRARY_PATH
export GST_REGISTRY="/tmp/gst-registry.bin"
export GST_REGISTRY_REUSE_PLUGIN_SCANNER=1
