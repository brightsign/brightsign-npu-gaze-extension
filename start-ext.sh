#!/bin/bash

# figure out the SOC model -- default to RK3588
SOC_MODEL=RK3588
if [ -f /sys/firmware/devicetree/base/compatible ]; then
    SOC_MODEL=$(cat /sys/firmware/devicetree/base/compatible | cut -d',' -f5 | tr '[:lower:]' '[:upper:]')
else
    echo "WARNING: Unable to determine SOC model, EXITING"
    exit 1
fi
echo "SOC_MODEL: ${SOC_MODEL}"

VID_DEVID=/dev/video0
if [ "${SOC_MODEL}" = "RK3588" ]; then
    VID_DEVID=/dev/video1
fi

export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
# run it
./attention_demo model/${SOC_MODEL}/RetinaFace.rknn ${VID_DEVID}