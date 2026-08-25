# Gaze Detection Development Guide

## Overview

This guide covers development workflows, testing strategies, and debugging techniques specific to the BrightSign Gaze Detection extension.

## Table of Contents

- [Development Environment Setup](#development-environment-setup)
- [Development Workflow](#development-workflow)
- [Testing Strategies](#testing-strategies)
- [Debugging Techniques](#debugging-techniques)
- [Performance Optimization](#performance-optimization)
- [Best Practices](#best-practices)

## Development Environment Setup

### Prerequisites

Before starting development, ensure you have:

- x86_64 Linux host (Intel/AMD architecture)
- Docker installed and running
- Git for version control
- 25GB+ free disk space for SDK builds
- USB webcam for testing

### Initial Setup

```bash
# Clone the repository
git clone git@github.com:brightsign/brightsign-npu-gaze-extension.git
cd brightsign-npu-gaze-extension

# Setup development environment
./setup

# Compile RetinaFace models for all platforms
./compile-models

# Build OpenEmbedded SDK (one-time process)
./build --extract-sdk

# Install SDK for cross-compilation
./brightsign-x86_64-cobra-toolchain-*.sh -d ./sdk -y
```

## Development Workflow

### Rapid Development Cycle

For fastest iteration during development:

1. **Code Changes**: Modify C++ source files in `src/`
2. **Build**: `./build-apps` to compile for all platforms
3. **Package**: `./package --dev-only` for quick development package
4. **Test**: Deploy and test on target hardware

### Platform-Specific Development

```bash
# Build for specific platform only (faster)
./build-apps XT5     # For XT-5/RK3588 only
./build-apps LS5     # For LS-5/RK3568 only

# Package for specific platform
./package --soc RK3588 --dev-only
```

### Clean Builds

When you need to start fresh:

```bash
# Clean application builds
./build-apps --clean

# Clean model compilation 
./compile-models --clean

# Full clean (including SDK)
./build --clean
```

## Testing Strategies

### Local Development Testing

#### 1. Orange Pi Development

For rapid prototyping and debugging, use Orange Pi boards:

```bash
# See OrangePI_Development.md for complete setup
# Benefits: native compilation, full debugging tools, same hardware
```

#### 2. Cross-Platform Testing

Test on all supported platforms:

- **XT-5 (RK3588)**: Primary target, highest performance
- **LS-5 (RK3568)**: Lower power, different camera device  
- **Firebird (RK3576)**: Development platform

### Integration Testing

#### Camera Input Testing

```bash
# Test different cameras
registry write extension bsext-gaze-video-device /dev/video0
registry write extension bsext-gaze-video-device /dev/video1

# Verify camera detection
v4l2-ctl --list-devices
```

#### Output Validation

Test all output formats:

```bash
# Monitor UDP outputs
socat -u UDP-LISTEN:5000 -  # BrightScript format
socat -u UDP-LISTEN:5002 -  # JSON format

# Check decorated image output
ls -la /tmp/output.jpg
```

### Performance Testing

#### Frame Rate Monitoring

```bash
# Monitor processing performance
watch -n 1 "ps aux | grep attention_demo"

# Check NPU utilization (if available)
cat /sys/class/devfreq/*/load
```

#### Memory Usage

```bash
# Monitor memory consumption
watch -n 1 "free -h && ps aux --sort=-%mem | head -10"

# Check extension memory usage
ps -o pid,ppid,cmd,%mem,%cpu -p $(pgrep attention_demo)
```

## Debugging Techniques

### Enable Debug Mode

For development packages, debug symbols are included:

```bash
# Run extension in foreground for debugging
./bsext_init run

# Use GDB for debugging (on development builds)
gdb ./attention_demo
(gdb) set args /path/to/model /dev/video0
(gdb) run
```

### Log Analysis

#### Extension Logs

```bash
# Check systemd logs
journalctl -u bsext-gaze -f

# Check syslog for extension messages
tail -f /var/log/messages | grep gaze
```

#### Debug Output

Add debug output to source code:

```cpp
// In C++ source files
#ifdef DEBUG
    std::cout << "Debug: " << variable_value << std::endl;
#endif
```

### Common Issues and Solutions

#### Camera Not Found

```bash
# List available cameras
v4l2-ctl --list-devices
ls /dev/video*

# Test camera functionality
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

#### Model Loading Errors

```bash
# Verify model files exist
ls -la install/*/model/RetinaFace.rknn

# Check model compatibility
file install/*/model/RetinaFace.rknn
```

#### NPU Initialization Issues

```bash
# Check NPU device availability
ls -la /dev/dri/
lsmod | grep rknpu
```

## Performance Optimization

### Model Optimization

1. **Model Selection**: RetinaFace mobile320 provides good balance of speed/accuracy
2. **Quantization**: int8 quantization is used for NPU efficiency
3. **Input Resolution**: 320x320 input size for optimal performance

### Threading Optimization

The extension uses a producer-consumer pattern:
- **Producer Thread**: ML inference and frame processing
- **Consumer Threads**: UDP publishers for different output formats

### Memory Management

```cpp
// Best practices in C++ code:
// 1. Use RAII for resource management
// 2. Prefer move semantics over copying
// 3. Use smart pointers for dynamic allocation
// 4. Avoid memory leaks in loops
```

## Best Practices

### Code Organization

1. **Separation of Concerns**: Keep inference, I/O, and output formatting separate
2. **Error Handling**: Always check return values and handle errors gracefully
3. **Configuration**: Use registry keys for runtime configuration
4. **Logging**: Add appropriate logging for troubleshooting

### Resource Management

1. **Camera Resources**: Properly release camera handles
2. **NPU Resources**: Clean up RKNN contexts on exit
3. **Memory**: Avoid memory leaks in continuous operation
4. **File Handles**: Close file descriptors properly

### Configuration Best Practices

```bash
# Use registry for runtime configuration
registry write extension bsext-gaze-video-device /dev/video0

# Test configuration changes
./bsext_init restart
```

### Documentation

1. **Code Comments**: Document complex algorithms and hardware interfaces
2. **README Updates**: Keep installation and usage instructions current
3. **Version Notes**: Document changes between versions
4. **Integration Examples**: Provide clear usage examples

## Advanced Development

### Custom Model Integration

To integrate custom RetinaFace models:

1. **Export Model**: Export your trained model to ONNX format
2. **Compile**: Use `./compile-models` to convert to RKNN format
3. **Update Code**: Modify inference code if output format differs
4. **Test**: Validate on all target platforms

### Output Format Extensions

To add new output formats:

1. **Create Transport**: Implement new transport class (e.g., MQTT)
2. **Create Formatter**: Implement new message formatter
3. **Update Publisher**: Add new publisher instance
4. **Configure**: Add registry keys for configuration

### Platform Support

Adding support for new Rockchip SoCs:

1. **Model Compilation**: Compile models for new SOC
2. **Platform Detection**: Update SOC detection in `bsext_init`
3. **Configuration**: Add platform-specific settings
4. **Testing**: Validate on new hardware

This development guide provides the foundation for effective development, testing, and debugging of the BrightSign Gaze Detection extension. Follow these practices to maintain code quality and ensure reliable deployment across all supported platforms.