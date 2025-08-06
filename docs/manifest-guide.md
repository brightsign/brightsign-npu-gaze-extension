# BrightSign Gaze Extension Manifest Guide

## Overview

The BrightSign Gaze Extension uses a manifest system to provide:

- Version information and compatibility declarations
- Hardware requirements and system capabilities
- Configuration options and registry keys
- Build metadata and deployment information

This guide explains how to use the manifest system in the gaze detection extension.

## Quick Start

### 1. Create Your Configuration

Copy the template and customize it for your extension:

```bash
cp manifest-config.template.json manifest-config.json
# Edit manifest-config.json with your extension details
```

### 2. Key Fields for Gaze Extension

**Extension Information:**
```json
"extension": {
  "version": "1.0.0",
  "description": "NPU-accelerated gaze detection using RetinaFace models", 
  "author": {
    "name": "BrightSign LLC",
    "email": "support@brightsign.biz",
    "url": "https://www.brightsign.biz"
  },
  "license": "Apache-2.0",
  "homepage": "https://github.com/brightsign/brightsign-npu-gaze-extension"
}
```

**Compatibility Requirements:**
```json
"compatibility": {
  "osVersion": {
    "min": "9.0.0",     // Minimum BrightSign OS version
    "target": "9.1.0",  // Version you developed/tested against
    "max": "10.0.0"     // Maximum version (optional)
  }
}
```

### 3. Generate the Manifest

The `package` script automatically generates `manifest.json` from your configuration:

```bash
./package
# manifest.json is created in the extension package
```

## Configuration Reference

### Extension Metadata

| Field | Required | Description |
|-------|----------|-------------|
| `version` | Yes | Semantic version (MAJOR.MINOR.PATCH) |
| `description` | Yes | Brief description of gaze detection functionality |
| `author` | Yes | Author name, email, and URL |
| `license` | Yes | Software license (e.g., "Apache-2.0", "MIT") |
| `homepage` | No | Project homepage URL |
| `category` | Yes | Should be "ai-vision" for gaze detection |

### Compatibility

| Field | Required | Description |
|-------|----------|-------------|
| `osVersion.min` | Yes | Minimum BrightSign OS version |
| `osVersion.target` | No | OS version tested against |
| `osVersion.max` | No | Maximum OS version |

### Requirements

| Field | Required | Description |
|-------|----------|-------------|
| `capabilities` | No | Required hardware features: ["camera.usb", "npu.rockchip"] |
| `memory.minimum` | No | Minimum RAM required (e.g., "512MB") |
| `memory.recommended` | No | Recommended RAM (e.g., "1GB") |
| `storage.installation` | No | Disk space for installation (e.g., "150MB") |
| `storage.runtime` | No | Disk space for runtime data (e.g., "50MB") |

### Runtime Configuration

| Field | Default | Description |
|-------|---------|-------------|
| `autoStart` | true | Start automatically on boot |
| `startupDelay` | 5 | Seconds to wait before starting |
| `restartPolicy` | "always" | always, on-failure, or never |
| `priority` | "normal" | Process priority: low, normal, high |

### Registry Configuration

The manifest declares available registry configuration keys:

```json
"registry": {
  "configurable": [
    {
      "key": "video-device",
      "type": "string", 
      "default": "/dev/video0",
      "description": "USB camera device path"
    },
    {
      "key": "disable-auto-start",
      "type": "boolean",
      "default": false,
      "description": "Disable automatic startup of the extension"
    }
  ]
}
```

## Gaze Extension Specific Settings

### Hardware Capabilities

For gaze detection extensions, declare these required capabilities:

```json
"requirements": {
  "capabilities": [
    "camera.usb",      // USB camera support required
    "npu.rockchip",    // Rockchip NPU acceleration required
    "storage.persistent"  // Persistent storage for models
  ]
}
```

### Memory Requirements

Gaze detection has specific memory requirements:

```json
"requirements": {
  "memory": {
    "minimum": "512MB",     // Minimum for basic functionality
    "recommended": "1GB"    // Recommended for optimal performance
  },
  "storage": {
    "installation": "150MB", // Space for models and binaries
    "runtime": "50MB"        // Space for output files and logs
  }
}
```

### Gaze-Specific Registry Keys

```json
"registry": {
  "configurable": [
    {
      "key": "video-device",
      "type": "string",
      "default": "/dev/video0",
      "description": "USB camera device path (auto-detected by SOC type)"
    },
    {
      "key": "disable-auto-start", 
      "type": "boolean",
      "default": false,
      "description": "Disable automatic startup of the gaze extension"
    }
  ]
}
```

## Update Management

The manifest supports update configuration:

```json
"update": {
  "policy": "manual",           // Update policy: automatic, manual, blocked
  "backupPrevious": true,       // Create backup before updating
  "preserveConfig": true,       // Preserve registry configuration
  "rollbackSupported": true,    // Support rollback to previous version
  "minVersionForUpdate": "1.0.0", // Minimum version that can be updated
  "maxVersionGap": "2.0.0",     // Maximum version difference allowed
  "requiresReboot": false       // Whether update requires reboot
}
```

## Platform Detection

The gaze extension automatically detects the platform at runtime:

- **RK3588** (XT-5): Uses `/dev/video1`, loads `RK3588/model/RetinaFace.rknn`
- **RK3568** (LS-5): Uses `/dev/video0`, loads `RK3568/model/RetinaFace.rknn`
- **RK3576** (Firebird): Uses `/dev/video0`, loads `RK3576/model/RetinaFace.rknn`

## Example Complete Manifest Configuration

```json
{
  "extension": {
    "version": "1.0.0",
    "description": "NPU-accelerated gaze detection using RetinaFace models",
    "author": {
      "name": "BrightSign LLC",
      "email": "support@brightsign.biz", 
      "url": "https://www.brightsign.biz"
    },
    "license": "Apache-2.0",
    "homepage": "https://github.com/brightsign/brightsign-npu-gaze-extension",
    "category": "ai-vision"
  },
  
  "compatibility": {
    "osVersion": {
      "min": "9.0.0",
      "target": "9.1.0",
      "max": "10.0.0"
    }
  },
  
  "requirements": {
    "capabilities": [
      "camera.usb",
      "npu.rockchip",
      "storage.persistent"
    ],
    "memory": {
      "minimum": "512MB",
      "recommended": "1GB"
    },
    "storage": {
      "installation": "150MB",
      "runtime": "50MB"
    }
  },
  
  "runtime": {
    "autoStart": true,
    "startupDelay": 5,
    "restartPolicy": "always",
    "priority": "normal"
  },
  
  "registry": {
    "configurable": [
      {
        "key": "video-device",
        "type": "string",
        "default": "/dev/video0",
        "description": "USB camera device path"
      },
      {
        "key": "disable-auto-start",
        "type": "boolean", 
        "default": false,
        "description": "Disable automatic startup of the extension"
      }
    ]
  },
  
  "update": {
    "policy": "manual",
    "backupPrevious": true,
    "preserveConfig": true,
    "rollbackSupported": true,
    "requiresReboot": false
  }
}
```

## Best Practices

### Version Management

- Use semantic versioning (MAJOR.MINOR.PATCH)
- Increment MAJOR for breaking changes
- Increment MINOR for new features
- Increment PATCH for bug fixes

### Compatibility

- Test thoroughly on minimum supported OS version
- Set conservative minimum version requirements
- Document any OS version-specific behavior

### Registry Configuration

- Provide sensible defaults for all configurable options
- Include clear descriptions for user-facing settings
- Use appropriate data types (string, boolean, number)

### Resource Requirements

- Set realistic minimum memory requirements
- Account for model size in storage requirements  
- Consider runtime data accumulation

This manifest system ensures proper deployment, configuration, and lifecycle management of the gaze detection extension across different BrightSign player platforms.