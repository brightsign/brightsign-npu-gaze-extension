# BrightSign Gaze Detection Extension

**Automated gaze detection extension for BrightSign Series 5 players using Rockchip NPU acceleration.**

This project provides a complete, automated build system to create BrightSign extensions that run RetinaFace-based gaze detection on the NPU at real-time performance, detecting faces and determining if people are looking at the screen.

## Release Status

This is an **ALPHA** quality release, intended mostly for educational purposes. This model is not tuned for optimum performance and has had only standard testing.  **NOT RECOMMENDED FOR PRODUCTION USE**.

## 🚀 Quick Start (Complete Automated Workflow)

**Total Time**: 60-90 minutes | **Prerequisites**: Docker, git, x86_64 Linux host


> ⏱️ **Time Breakdown**: Most time is spent in the OpenEmbedded SDK build (30-45 min). The process is fully automated but requires patience for the BitBake compilation.

```bash
# 1. Clone and setup environment (5-10 minutes)
git clone <repository-url>
cd brightsign-npu-gaze-extension
./setup

# 2. Compile ONNX models to RKNN format (3-5 minutes)
./compile-models

# 3. Build OpenEmbedded SDK (30-45 minutes)
./build --extract-sdk

# 4. Install the SDK (1 minute)
./brightsign-x86_64-cobra-toolchain-*.sh -d ./sdk -y

# 5. Build C++ applications for all platforms (3-8 minutes)
./build-apps

# 6. Package extension for deployment (1 minute)
./package
```

In a typical development workflow, steps 1-4 (setup, model compilation, build and install the SDK) will need to only be done once. Building the apps and packaging them will likely be repeated as the developer changes the app code.

**✅ Success**: You now have production-ready extension packages:

- `gaze-dev-<timestamp>.zip` (development/testing)
- `gaze-ext-<timestamp>.zip` (production deployment)

**🎯 Deploy to Player**:

1. Transfer extension package to BrightSign player via DWS
2. Install: `bash ./ext_npu_gaze_install-lvm.sh && reboot`
3. Extension auto-starts with USB camera detection

## 📋 Requirements & Prerequisites

### Hardware Requirements

| Component | Requirement |
|-----------|-------------|
| **Development Host** | x86_64 architecture (Intel/AMD) |
| **BrightSign Player** | Series 5 (XT-5, LS-5) or Firebird dev board |
| **Camera** | USB webcam (tested: Logitech C270, Thustar) |
| **Storage** | 25GB+ free space for builds |

### Supported Players

| Player | SOC | Platform Code | Status |
|--------|-----|---------------|---------|
| XT-5 (XT1145, XT2145) | RK3588 | XT5 | ✅ Production |
| LS-5 (LS445) | RK3568 | LS5 | ✅ Beta |
| Firebird | RK3576 | Firebird | 🧪 Development |

### Software Requirements

- **Docker** (for containerized builds)
- **Git** (for repository cloning)
- **25GB+ disk space** (for OpenEmbedded builds)

**Important**: Apple Silicon Macs are not supported. Use x86_64 Linux or Windows with WSL2.

## ⚙️ Configuration & Customization


The extension is highly configurable via BrightSign registry keys:

### Core Settings

```bash
# Auto-start control
registry write extension bsext-gaze-disable-auto-start true

# Camera device override
registry write extension bsext-gaze-video-device /dev/video1
```

### Extension Control

This extension allows two, optional registry keys to be set to:

* Disable the auto-start of the extension -- this can be useful in debugging or other problems
* Set the `v4l` device filename to override the auto-discovered device

**Registry keys are organized in the `extension` section**

| Registry Key | Values | Effect |
| --- | --- | --- |
| `bsext-gaze-disable-auto-start` | `true` or `false` | when truthy, disables the extension from autostart (`bsext_init start` will simply return). The extension can still be manually run with `bsext_init run` |
| `bsext-gaze-video-device` | a valid v4l device file name like `/dev/video0` or `/dev/video1` | normally not needed, but may be useful to override for some unusual or test condition |

### Extension Behavior

- **Visual Output**: `/tmp/output.jpg` (decorated image with face bounding boxes and eye detection)
- **Data Output**: UDP streaming with gaze detection results
- **Performance**: Real-time face detection and gaze estimation on NPU

## 📊 Using the Inference Data

The extension "watches" the camera field of view and finds all faces. It then looks to find the eyes in each face. If it can find both eyes it infers that the person was looking in the direction of the camera. This data is output in two UDP packets to localhost once per second.

### UDP Output Formats


**Port 5000** (BrightScript format for BrightAuthor:connected):
```ini
faces_attending:1!!faces_in_frame_total:1!!timestamp:1746732408
```

**Port 5002** (JSON format for node applications):
```json
{"faces_attending":1,"faces_in_frame_total":1,"timestamp":1746732408}
```

### Output Data Fields

| Property | Description |
| --- | --- |
| `faces_in_frame_total` | Total count of all faces detected in the current frame |
| `faces_attending` | Number of faces estimated to be paying attention to the screen |
| `timestamp` | Unix timestamp of the measurement |

### Integration Examples

- **BrightAuthor:connected**: [Simple Gaze Detection Presentation](https://github.com/brightsign/simple-gaze-detection-presentation)
- **HTML/Node.js**: [Simple Gaze Detection HTML](https://github.com/brightsign/simple-gaze-detection-html)

## 🖼️ Decorated Camera Output

Every frame of video captured is processed through the model. Every detected face has a bounding box drawn around it. Faces with two eyes detected have a green box, otherwise the box is red. The decorated image is written to `/tmp/output.jpg` on a RAM disk so it will not impact storage life.

## 📦 Production Deployment

### Package Options

```bash
# Create both development and production packages
./package

# Development package only (volatile installation)
./package --dev-only

# Production extension only (permanent installation)
./package --ext-only

# Package specific platform
./package --soc RK3588
```

### Installation Methods

**Development Installation** (volatile, lost on reboot):

```bash
# On player
mkdir -p /usr/local/gaze && cd /usr/local/gaze
unzip /storage/sd/gaze-dev-*.zip
./bsext_init run  # Test in foreground
```

**Production Installation** (permanent):

```bash
# On player
cd /usr/local && unzip /storage/sd/gaze-ext-*.zip
bash ./ext_npu_gaze_install-lvm.sh
reboot  # Extension auto-starts after reboot

```

## 🛠️ Development & Testing

### Rapid Development Workflow

For faster iteration during development, consider using Orange Pi boards:

**📋 See [OrangePI_Development.md](OrangePI_Development.md) for complete development guide**

Benefits:
- **Faster builds**: Native ARM compilation vs cross-compilation
- **Better debugging**: Full GDB support and system monitoring
- **Same hardware**: Uses identical Rockchip SoCs as BrightSign players

### Build System Options

```bash
# Build specific platforms only
./build-apps XT5      # XT-5 players only
./build-apps LS5      # LS-5 players only

# Compile models only
./compile-models      # All models for all platforms
./compile-models --clean  # Clean rebuild all models

# SDK build options
./build --help                 # See all build options
./build --clean brightsign-sdk # Clean SDK rebuild
```
### Image Stream Server

The **BrightSign Image Stream Server** is a built-in networking feature that serves camera frames over HTTP. Image Stream Server will start along with voice detection extension as a standalone daemon running in the background.The bs-image-stream-server continuously monitors a local image file by gaze detection and serves it via HTTP at 30 FPS. It specifically watches /tmp/output.jpg since that is where the BSMP files write their output.

This is intended for development and testing purposes only.

Enable or disable the image stream server using the registry options:

**Configuration Options:**

| Port Value | Behavior |
|------------|----------|
| `0` | **Disabled** - Image stream server is turned off (recommended for this extension) |
| `20200` | **Default** - Serves camera feed at `http://player-ip:20200/image_stream.jpg` |

**Usage Examples:**
```bash
# Disable image stream server
registry write networking bs-image-stream-server-port 0

# Enable on default port 20200
registry write networking bs-image-stream-server-port 20200

```

> **Note**: Changes to the image stream server port require a player reboot to take effect.

### Troubleshooting

**Common Issues**:

- **Docker not running**: `systemctl start docker`
- **Permission denied**: Add user to docker group
- **Out of space**: Need 25GB+ for OpenEmbedded builds
- **Wrong architecture**: Must use x86_64 host (not ARM/Apple Silicon)

**Getting Help**:

```bash
# Core build system
./setup --help                    # Setup and environment options
./compile-models --help           # Model compilation options  
./build --help                    # SDK build options
./build-apps --help               # Application build options
./package --help                  # Packaging options

```

**Build Failures**:

```bash
# Clean and retry
./build-apps --clean
./compile-models --clean
./setup  # Re-run if Docker images corrupted
```

## 🎯 Advanced Usage

### Custom Models

Replace default RetinaFace model with your own ONNX models:

1. Place ONNX model in `toolkit/rknn_model_zoo/examples/RetinaFace/model/`
2. Run `./compile-models` to convert to RKNN format
3. Update model path in extension configuration

### Multi-Platform Development

The extension automatically detects platform at runtime:

- **RK3588** (XT-5): Uses `RK3588/` subdirectory, `/dev/video1`
- **RK3568** (LS-5): Uses `RK3568/` subdirectory, `/dev/video0`
- **RK3576** (Firebird): Uses `RK3576/` subdirectory, `/dev/video0`

## 📚 Technical Documentation

For in-depth technical information:

### 📄 [Manifest Guide](docs/manifest-guide.md)

- Extension versioning system
- Compatibility declarations
- User configuration options

### 🍊 [Orange Pi Development](OrangePI_Development.md)

- Rapid prototyping workflow
- Native development environment
- Testing and debugging guide

### 🛠️ [Development Guide](docs/development-guide.md)

- Development workflow and best practices
- Testing strategies
- Debugging techniques


## 🗑️ Removing the Extension

To remove the extension, you can perform a Factory Reset or remove the extension manually:

1. Connect to the player over SSH and drop to the Linux shell
2. STOP the extension: `/var/volatile/bsext/ext_npu_gaze/bsext_init stop`
3. VERIFY all the processes for your extension have stopped
4. Run the uninstall script: `/var/volatile/bsext/ext_npu_gaze/uninstall.sh`
5. Reboot to apply changes: `reboot`

---

**🎉 Ready to get started?** Run `./setup` and follow the Quick Start guide above!

For questions or issues, see the troubleshooting section or check the technical documentation.
