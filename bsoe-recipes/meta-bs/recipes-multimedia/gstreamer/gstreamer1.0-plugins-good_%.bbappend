# Minimal configuration for RTSP support
PACKAGECONFIG:append = " soup2 "

# Remove invalid PACKAGECONFIG options that other layers might add
PACKAGECONFIG:remove = " soup rtp rtsp "
