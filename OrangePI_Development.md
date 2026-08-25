# Orange Pi Development Guide - Gaze Detection

This guide covers development and testing using Orange Pi boards (OPi) as an alternative development environment for the BrightSign Gaze Detection project.

## Overview

While not required for BrightSign deployment, Orange Pi boards can facilitate a more responsive build and debug process due to their full Linux distribution and native compiler. This approach allows for rapid prototyping and testing before final cross-compilation for BrightSign OS.

**Important**: Orange Pi cannot be used for model compilation (requires x86_64 architecture) but is excellent for application development and testing with pre-compiled models.

## Requirements

### Hardware
- Orange Pi 5 Plus (or similar ARM-based Orange Pi board)
- USB webcam (same as BrightSign requirements - Logitech C270 recommended)
- Cables, monitors, etc.

### Software
Orange Pi boards typically run Debian (Armbian) images. Use the eMMC image for best performance.

```bash
sudo apt update 
sudo apt install -y \
    cmake \
    gdb \
    git \
    libboost-all-dev \
    libturbojpeg-dev \
    libjpeg-turbo8-dev \
    libjpeg-turbo-progs \
    libopencv-dev \
    build-essential
```

## Development Workflow

### 1. Model Preparation

**IMPORTANT**: Models must be compiled on an x86_64 host machine first, as Orange Pi (ARM architecture) cannot run the RKNN toolkit compilation process.

#### On your x86_64 development machine:
```bash
# Follow the main README instructions to compile models
./setup
./compile-models
```

This creates compiled RKNN models in the following locations:
```
install/
├── RK3588/model/          # For Orange Pi 5/5B and BrightSign XT-5
│   └── RetinaFace.rknn   # RetinaFace gaze detection model
└── RK3568/model/          # For Orange Pi 3/4 and BrightSign LS-5  
    └── RetinaFace.rknn   # RetinaFace gaze detection model
```

#### Transfer models to Orange Pi:
```bash
# Copy models from development machine
scp -r install/ user@orangepi-ip:/path/to/project/
```

### 2. Orange Pi Setup

#### Clone the project on Orange Pi:
```bash
git clone git@github.com:brightsign/brightsign-npu-gaze-extension.git
cd brightsign-npu-gaze-extension
```

#### Install RKNN Runtime Libraries

Download the appropriate RKNN runtime for your Orange Pi:

```bash
# For Orange Pi 5 (RK3588)
wget https://github.com/airockchip/rknn-toolkit2/blob/v2.3.0/rknpu2/runtime/Linux/librknn_api/aarch64/librknnrt.so
wget https://github.com/airockchip/librga/raw/main/libs/Linux/gcc-aarch64/librga.so

# Copy to system location or use local directory
mkdir -p lib
cp librknnrt.so lib/
cp librga.so lib/
```

### 3. Native Build on Orange Pi

```bash
mkdir -p build && cd build

# Configure for native ARM build
cmake .. -DTARGET_SOC="rk3588"  # or rk3568 for older Orange Pi

# Build the application
make -j$(nproc)

# Install locally
make install
```

### 4. Testing on Orange Pi

```bash
# Navigate to install directory
cd ../install/RK3588  # or RK3568

# Set library path
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH

# Test with USB camera
./attention_demo ./model/RetinaFace.rknn /dev/video0

# Monitor output
# Terminal 1: Watch UDP output
socat -u UDP-LISTEN:5002 -

# Terminal 2: Monitor decorated images
watch -n 1 "ls -la /tmp/output.jpg"
```

## Development Benefits

### Faster Build Cycle
- **Native Compilation**: No cross-compilation overhead
- **Full GCC/Clang**: Complete debugging symbols and optimization
- **Package Managers**: Easy installation of development tools

### Better Debugging
```bash
# Full GDB support with symbols
gdb ./attention_demo
(gdb) set args ./model/RetinaFace.rknn /dev/video0
(gdb) break main
(gdb) run

# Valgrind for memory debugging
valgrind --leak-check=full ./attention_demo ./model/RetinaFace.rknn /dev/video0

# Profiling with perf
perf record -g ./attention_demo ./model/RetinaFace.rknn /dev/video0
perf report
```

### System Monitoring
```bash
# Real-time system monitoring
htop

# NPU utilization (if available)
cat /sys/class/devfreq/*/load

# Camera device monitoring
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

## Platform-Specific Considerations

### Orange Pi 5/5B (RK3588)
- Same SOC as BrightSign XT-5
- Use `/dev/video1` (matches XT-5 behavior)
- Full NPU acceleration available
- 8GB RAM for comfortable development

### Orange Pi 3/4 (RK3568)  
- Same SOC as BrightSign LS-5
- Use `/dev/video0`
- NPU acceleration available
- Monitor memory usage more carefully

### Camera Configuration
```bash
# List available cameras
v4l2-ctl --list-devices

# Test camera functionality
v4l2-ctl --device=/dev/video0 --list-formats-ext

# Capture test image
v4l2-ctl --device=/dev/video0 --set-fmt-video=width=640,height=480,pixelformat=YUYV
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-to=test.raw --stream-count=1
```

## Gaze Detection Specific Testing

### Face Detection Validation
```bash
# Test with known good images
./attention_demo ./model/RetinaFace.rknn test_face_image.jpg

# Validate face detection accuracy
# - Check bounding box coordinates
# - Verify eye landmark detection
# - Confirm gaze direction inference
```

### Output Format Testing
```bash
# Test BrightScript format output
socat -u UDP-LISTEN:5000 -
# Expected: faces_attending:1!!faces_in_frame_total:1!!timestamp:1746732408

# Test JSON format output  
socat -u UDP-LISTEN:5002 -
# Expected: {"faces_attending":1,"faces_in_frame_total":1,"timestamp":1746732408}
```

### Performance Benchmarking
```bash
# Measure frame processing rate
time ./attention_demo ./model/RetinaFace.rknn /dev/video0

# Monitor resource usage
top -p $(pgrep attention_demo)

# Test sustained performance
./attention_demo ./model/RetinaFace.rknn /dev/video0 &
sleep 300  # Run for 5 minutes
kill %1
```

## Deployment Testing

Before deploying to BrightSign, validate:

1. **Functionality**: All gaze detection features work correctly
2. **Performance**: Acceptable frame rates and resource usage  
3. **Stability**: No memory leaks during extended operation
4. **Output**: Correct UDP packet formats and timing

## Transferring to BrightSign

Once development is complete on Orange Pi:

1. **Cross-compile**: Use the main build system on x86_64 host
2. **Package**: Create BrightSign extension package
3. **Deploy**: Install on target BrightSign player
4. **Validate**: Confirm functionality matches Orange Pi testing

```bash
# On x86_64 development machine
./build-apps
./package
# Deploy resulting package to BrightSign
```

## Best Practices

### Code Organization
- Keep Orange Pi development code in sync with main repository
- Use version control for all changes
- Test on Orange Pi before committing changes

### Testing Strategy
- Validate basic functionality on Orange Pi first
- Use Orange Pi for algorithm development and tuning
- Cross-validate results on actual BrightSign hardware

### Performance Considerations  
- Orange Pi may have different performance characteristics
- Always validate performance on target BrightSign hardware
- Account for differences in thermal management and power constraints

This Orange Pi development workflow provides a rapid, flexible environment for developing and testing gaze detection functionality before final deployment to BrightSign players.