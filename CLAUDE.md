# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the BrightSign NPU Gaze Extension - an AI-powered gaze detection system that runs on BrightSign digital signage players using their Neural Processing Unit (NPU). It detects faces in camera feeds, determines if they're looking at the screen, and broadcasts attention metrics via UDP.

## Architecture

### Core Technologies
- **C++20** application with multi-threaded processing
- **OpenCV** for camera capture
- **RKNN** runtime for NPU inference  
- **RetinaFace** model (MIT licensed) for face detection
- **CMake** cross-compilation build system
- **BrightSign OS Extension** packaging

### Supported Hardware
- **XT-5** (XT1145, XT2145): RK3588 SOC - Requires OS 9.0.189+
- **Firebird**: RK3576 SOC - Requires BETA-9.1.49+
- **LS-5** (LS445): RK3568 SOC - Requires BETA-9.1.49+

## Development Commands

### Build Setup
```bash
# Clone project and supporting repos
git clone git@github.com:brightsign/brightsign-npu-gaze-extension.git
cd brightsign-npu-gaze-extension
export project_root=$(pwd)

# Clone Rockchip dependencies
mkdir -p toolkit && cd toolkit
git clone https://github.com/airockchip/rknn-toolkit2.git --depth 1 --branch v2.3.0
git clone https://github.com/airockchip/rknn_model_zoo.git --depth 1 --branch v2.3.0
cd ..
```

### Model Compilation (x86_64 host required)
```bash
# Build Docker container for model compilation
cd toolkit/rknn-toolkit2/rknn-toolkit2/docker/docker_file/ubuntu_20_04_cp38
docker build --rm -t rknn_tk2 -f Dockerfile_ubuntu_20_04_for_cp38 .

# Download and compile model for each SOC
cd "${project_root}/toolkit/rknn_model_zoo/"
mkdir -p examples/RetinaFace/model
cd examples/RetinaFace/model
chmod +x ./download_model.sh && ./download_model.sh
cd ../../../

# Compile for XT-5 (RK3588)
docker run -it --rm -v $(pwd):/zoo rknn_tk2 /bin/bash \
    -c "cd /zoo/examples/RetinaFace/python && python convert.py ../model/RetinaFace_mobile320.onnx rk3588 i8 ../model/RK3588/RetinaFace.rknn"

# Compile for Firebird (RK3576)
docker run -it --rm -v $(pwd):/zoo rknn_tk2 /bin/bash \
    -c "cd /zoo/examples/RetinaFace/python && python convert.py ../model/RetinaFace_mobile320.onnx rk3576 i8 ../model/RK3576/RetinaFace.rknn"

# Compile for LS-5 (RK3568)
docker run -it --rm -v $(pwd):/zoo rknn_tk2 /bin/bash \
    -c "cd /zoo/examples/RetinaFace/python && python convert.py ../model/RetinaFace_mobile320.onnx rk3568 i8 ../model/RK3568/RetinaFace.rknn"
```

### SDK Installation
```bash
# Build SDK from source (recommended)
docker build --rm --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) --ulimit memlock=-1:-1 -t bsoe-build .
mkdir -p srv
docker run -it --rm -u $(id -u):$(id -g) -v $(pwd)/brightsign-oe:/home/builder/bsoe -v $(pwd)/srv:/srv bsoe-build

# Inside Docker:
cd /home/builder/bsoe/build
MACHINE=cobra ./bsbb brightsign-sdk
# Exit Docker with Ctrl-D

# Install SDK
cd "${project_root}"
cp brightsign-oe/build/tmp-glibc/deploy/sdk/brightsign-x86_64-cobra-toolchain-*.sh ./
./brightsign-x86_64-cobra-toolchain-*.sh -d ./sdk -y
```

### Application Build
```bash
# Build for XT-5
cd "${project_root}"
source ./sdk/environment-setup-aarch64-oe-linux
mkdir -p build_xt5 && cd build_xt5
cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3588"
make && make install

# Build for Firebird  
cd "${project_root}"
source ./sdk/environment-setup-aarch64-oe-linux
mkdir -p build_firebird && cd build_firebird
cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3576"
make && make install

# Build for LS-5
cd "${project_root}"
source ./sdk/environment-setup-aarch64-oe-linux
mkdir -p build_ls5 && cd build_ls5
cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3568"
make && make install
```

### Extension Packaging
```bash
cd "${project_root}"
cp bsext_init install/ && chmod +x install/bsext_init
cp sh/uninstall.sh install/ && chmod +x install/uninstall.sh

# Create extension package
cd install
../sh/make-extension-lvm
zip ../gaze-ext-$(date +%s).zip ext_npu_gaze*
rm -rf ext_npu_gaze*
```

### Testing & Debugging
```bash
# Deploy to unsecured player via DWS or SSH
cd /storage/sd
unzip ext_npu_gaze-*.zip
bash ./ext_npu_gaze_install-lvm.sh
reboot

# Manual control
/var/volatile/bsext/ext_npu_gaze/bsext_init stop   # Stop service
/var/volatile/bsext/ext_npu_gaze/bsext_init start  # Start service
/var/volatile/bsext/ext_npu_gaze/bsext_init run    # Run in foreground

# Monitor UDP output
socat -u UDP-LISTEN:5000 -  # BrightScript format
socat -u UDP-LISTEN:5002 -  # JSON format

# Check decorated camera output
ls -la /tmp/output.jpg
```

### Extension Removal
```bash
# Via uninstall script
/var/volatile/bsext/ext_npu_gaze/uninstall.sh
reboot

# Manual removal
/var/volatile/bsext/ext_npu_gaze/bsext_init stop
umount /var/volatile/bsext/ext_npu_gaze
rm -rf /var/volatile/bsext/ext_npu_gaze
lvremove --yes /dev/mapper/bsext_npu_gaze
reboot
```

## Code Structure

### Main Components
- `src/main.cpp` - Application entry point, threading, signal handling
- `src/inference.cpp` - RKNN model loading and execution
- `src/retinaface.cc` - RetinaFace face detection implementation
- `src/attention.cpp` - Gaze direction analysis algorithms
- `src/publisher.cpp` - UDP data broadcasting (ports 5000, 5002)
- `src/image_utils.c` - Image processing utilities
- `bsext_init` - Service management script (start/stop/restart/run)

### Build Outputs
- `install/RK3588/` - XT-5 binaries and models
- `install/RK3576/` - Firebird binaries and models
- `install/RK3568/` - LS-5 binaries and models
- `build_xt5/`, `build_firebird/`, `build_ls5/` - Build directories

### Configuration
- Registry keys (under `extension` section):
  - `bsext-gaze-disable-auto-start`: Disable service auto-start
  - `bsext-gaze-video-device`: Override camera device path

### Camera Devices
- RK3588 (XT-5): Default `/dev/video1`
- RK3576/RK3568 (Firebird/LS-5): Default `/dev/video0`

## Output Formats

### UDP Port 5000 (BrightScript)
```
faces_attending:1!!faces_in_frame_total:1!!timestamp:1746732408
```

### UDP Port 5002 (JSON)
```json
{"faces_attending":1,"faces_in_frame_total":1,"timestamp":1746732408}
```

### Decorated Image
- Path: `/tmp/output.jpg`
- Green boxes: Faces looking at screen
- Red boxes: Faces not looking at screen

## Important Notes

1. **x86_64 Required**: Model compilation and cross-compilation MUST be done on x86_64 architecture
2. **Player Security**: Player must be unsecured for development (SECURE_CHECKS=0)
3. **Model License**: RetinaFace is MIT licensed - suitable for commercial use
4. **Library Versions**: Project uses RKNN toolkit v2.3.0, SDK patch references v2.3.2
5. **Extension Format**: Production builds require BrightSign signing for .bsfw format

## Common Issues

- **Video device not found**: Check camera is connected, verify correct `/dev/video*` path for SOC
- **Model load failure**: Ensure model is compiled for correct SOC architecture
- **UDP data not received**: Check firewall settings, verify service is running
- **Build errors**: Ensure SDK is properly sourced before building

## Testing Approach

The project doesn't have a dedicated test framework. Testing is done through:
- Manual testing on target hardware
- UDP output verification with `socat`
- Visual verification of `/tmp/output.jpg`
- Service status checks via `bsext_init`