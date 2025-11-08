# Extension Naming and Versioning

This document explains how the BrightSign NPU Gaze Extension is named, the constraints on naming, and how to change the name or add version numbers.

## Current Extension Name

The extension is currently named **`npu_gaze`**, which generates:
- Squashfs image: `ext_npu_gaze.squashfs`
- Install script: `ext_npu_gaze_install-lvm.sh`
- Mount point: `/var/volatile/bsext/ext_npu_gaze`

## Primary Name Definition

**Location**: `sh/make-extension-lvm:4`

```bash
name=npu_gaze
```

This is the **single source of truth** for the extension name. All other names are derived from this variable.

## Name Constraints

**Location**: `sh/make-extension-lvm:11`

```bash
if ! echo "${name}" | egrep -q '^[a-z][a-z0-9_]{2,12}$'; then
    echo "Error: Invalid extension name specified" 1>&2
    exit 1
fi
```

### Rules

- Must start with a **lowercase letter** (`a-z`)
- Followed by **2-12 additional characters** (lowercase letters, digits, or underscores)
- **Total length: 3-13 characters**

### Valid Examples

- `npu_gaze` (8 characters) ✓
- `gaze` (4 characters) ✓
- `face_detect` (11 characters) ✓
- `ai_vision` (9 characters) ✓

### Invalid Examples

- `NPU_gaze` - Contains uppercase letters ✗
- `ga` - Only 2 characters (too short) ✗
- `very_long_name` - 14 characters (too long) ✗
- `2gaze` - Starts with a digit ✗
- `-gaze` - Starts with a hyphen ✗

## Derived Names

From `name=npu_gaze`, these names are automatically generated:

| Component | Pattern | Example |
|-----------|---------|---------|
| Squashfs image | `ext_${name}.squashfs` | `ext_npu_gaze.squashfs` |
| Install script | `ext_${name}_install-lvm.sh` | `ext_npu_gaze_install-lvm.sh` |
| Mapper volume | `bsos-ext_${name}` | `bsos-ext_npu_gaze` |
| Temp volume | `bsos-tmp_${name}` | `bsos-tmp_npu_gaze` |
| Device mapper | `/dev/mapper/bsos-ext_${name}` | `/dev/mapper/bsos-ext_npu_gaze` |
| Alt device mapper | `/dev/mapper/bsext_${name}` | `/dev/mapper/bsext_npu_gaze` |
| Mount point | `/var/volatile/bsext/ext_${name}` | `/var/volatile/bsext/ext_npu_gaze` |

## Naming Inconsistency Warning

There is currently a **naming mismatch** between different parts of the system:

1. **Extension/Filesystem name**: Uses `npu_gaze` → produces `ext_npu_gaze`
2. **Registry keys**: Use `bsext-gaze-*` (just `gaze`, not `npu_gaze`)
3. **Daemon name**: `bsext-gaze` (in `bsext_init:16`)
4. **Hardcoded paths**: `/var/volatile/bsext/ext_npu_gaze/...` (in C++ source)

This inconsistency works but can be confusing. Consider standardizing to either:
- Full name everywhere: `npu_gaze` / `bsext-npu-gaze`
- Short name everywhere: `gaze` / `bsext-gaze`

## How to Rename the Extension

To rename the extension from `npu_gaze` to a new name, you must update multiple files:

### 1. Primary Name Definition

**File**: `sh/make-extension-lvm`

**Line 4**:
```bash
name=your_new_name  # Must match regex: ^[a-z][a-z0-9_]{2,12}$
```

### 2. Registry Key Prefix

**File**: `bsext_init`

**Line 16**:
```bash
DAEMON_NAME="bsext-your_new_name"  # Currently "bsext-gaze"
```

**Lines 108, 191-193, 208**: Update registry key references
```bash
registry extension ${DAEMON_NAME}-video-device
registry extension ${DAEMON_NAME}-disable-auto-start
registry extension ${DAEMON_NAME}-rtsp-server
```

### 3. Hardcoded Paths in C++ Code

**File**: `src/inference.cpp`

**Line 64**:
```cpp
const char* local = "/var/volatile/bsext/ext_your_new_name/RK3588/lib/gstreamer-1.0";
```

**Line 176** (in `setup_gstreamer_env` function):
```bash
export GST_PLUGIN_PATH=/var/volatile/bsext/ext_your_new_name/$SOC_NAME/lib/gstreamer-1.0:/usr/lib/gstreamer-1.0
export LD_LIBRARY_PATH=/var/volatile/bsext/ext_your_new_name/$SOC_NAME/lib:/usr/lib:$LD_LIBRARY_PATH
```

**Note**: Line 176 is actually in the shell script `bsext_init`, not in C++ code. The C++ code has the hardcoded path at line 64.

### 4. Uninstall Script

**File**: `sh/uninstall.sh`

Update all references to `ext_npu_gaze`:
```bash
/var/volatile/bsext/ext_your_new_name/bsext_init stop
umount /var/volatile/bsext/ext_your_new_name
rm -rf /var/volatile/bsext/ext_your_new_name
lvremove --yes /dev/mapper/bsext_your_new_name
lvremove --yes /dev/mapper/bsos-ext_your_new_name
rm -rf /dev/mapper/bsext_your_new_name
rm -rf /dev/mapper/bsos-ext_your_new_name
```

### 5. Documentation

Update all documentation files:
- `README.md`
- `CLAUDE.md`
- `docs/*.md`

Search for references to:
- `npu_gaze`
- `ext_npu_gaze`
- `bsext-gaze`

### 6. Package Script

**File**: `package`

**Lines 584, 596**: Update hardcoded extension name in zip operations
```bash
zip "../$EXTENSION_PACKAGE" ext_your_new_name* >/dev/null
rm -rf ext_your_new_name*
```

## Version Numbering

The extension **already supports versioning** through the `package` script and manifest system.

### Version Location

**File**: `package`

**Line 396**:
```bash
local version=$(echo "$config_content" | jq -r '.extension.version // "1.0.0"')
```

The version is read from `manifest-config.json` and embedded in the generated `manifest.json`.

### Setting the Version

Create or edit `manifest-config.json` in the project root:

```json
{
  "extension": {
    "version": "2.1.3",
    "description": "NPU-accelerated gaze detection using RetinaFace",
    "author": {
      "name": "BrightSign LLC",
      "email": "support@brightsign.biz"
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
    "camera": true,
    "npu": true,
    "minimumMemory": "2GB"
  },
  "runtime": {
    "autoStart": true,
    "restartOnFailure": true
  },
  "registry": {
    "namespace": "extension",
    "keys": [
      "bsext-gaze-video-device",
      "bsext-gaze-disable-auto-start",
      "bsext-gaze-rtsp-server",
      "bsext-gaze-udp-publish-rate"
    ]
  }
}
```

### Version in Generated Manifest

The `package` script generates `staging/manifest.json` with the version and additional build metadata:

```json
{
  "$schema": "https://brightsign.biz/schemas/extension-manifest/v1.json",
  "manifestVersion": 1,

  "extension": {
    "id": "com.brightsign.gaze-detection",
    "name": "Gaze Detection",
    "shortName": "Gaze",
    "version": "2.1.3",
    "description": "NPU-accelerated gaze detection using RetinaFace",
    "author": {
      "name": "BrightSign LLC"
    },
    "license": "Apache-2.0",
    "homepage": "https://github.com/brightsign/brightsign-npu-gaze-extension",
    "category": "ai-vision"
  },

  "build": {
    "timestamp": "2025-01-15T18:30:00Z",
    "sdk": "brightsign-sdk-9.1.0",
    "commit": "a1b2c3d"
  }
}
```

### Package Naming with Version

Currently, the package script creates timestamped packages:

**File**: `package`

**Lines 48-50**:
```bash
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
DEVELOPMENT_PACKAGE="gaze-dev-${TIMESTAMP}.zip"
EXTENSION_PACKAGE="gaze-ext-${TIMESTAMP}.zip"
```

Example output:
- `gaze-dev-20250115-183045.zip`
- `gaze-ext-20250115-183045.zip`

### Adding Version to Package Names

To include the version number in package filenames, modify the `package` script:

**After line 396** (where version is read):
```bash
local version=$(echo "$config_content" | jq -r '.extension.version // "1.0.0"')
```

**Replace lines 48-50** with:
```bash
# Read version from manifest-config.json if it exists
VERSION="1.0.0"
if [[ -f "manifest-config.json" ]]; then
    VERSION=$(jq -r '.extension.version // "1.0.0"' manifest-config.json 2>/dev/null || echo "1.0.0")
fi

TIMESTAMP=$(date +%Y%m%d-%H%M%S)
DEVELOPMENT_PACKAGE="gaze-dev-v${VERSION}-${TIMESTAMP}.zip"
EXTENSION_PACKAGE="gaze-ext-v${VERSION}-${TIMESTAMP}.zip"
```

This would produce:
- `gaze-dev-v2.1.3-20250115-183045.zip`
- `gaze-ext-v2.1.3-20250115-183045.zip`

## Semantic Versioning

Follow [Semantic Versioning](https://semver.org/) (SemVer) for version numbers:

### Format: MAJOR.MINOR.PATCH

- **MAJOR**: Incompatible API changes, breaking changes
- **MINOR**: New features, backwards-compatible
- **PATCH**: Bug fixes, backwards-compatible

### Examples

- `1.0.0` - Initial release
- `1.1.0` - Added RTSP support (new feature)
- `1.1.1` - Fixed memory leak (bug fix)
- `2.0.0` - Changed registry key names (breaking change)

### Pre-release Versions

For beta/alpha releases:
- `1.0.0-alpha.1`
- `1.0.0-beta.2`
- `1.0.0-rc.1` (release candidate)

## Recommendations

### For Naming Consistency

1. **Decide on a consistent name**:
   - Option A: Use `gaze` everywhere (shorter, matches current registry keys)
   - Option B: Use `npu_gaze` everywhere (more descriptive)

2. **Update all references** to use the chosen name consistently

3. **Test thoroughly** after renaming:
   - Build for all SOCs
   - Test installation on target hardware
   - Verify registry keys work
   - Check runtime paths are correct

### For Versioning

1. **Create `manifest-config.json`** with initial version `1.0.0`
2. **Document version changes** in `CHANGELOG.md`
3. **Tag git commits** with version numbers: `git tag v1.0.0`
4. **Bump version** before each release
5. **Consider automatic versioning** from git tags in CI/CD

## Quick Rename Checklist

- [ ] Update `sh/make-extension-lvm` line 4 (`name=`)
- [ ] Update `bsext_init` line 16 (`DAEMON_NAME=`)
- [ ] Update `bsext_init` registry key references (lines 108, 191-193, 208)
- [ ] Update `src/inference.cpp` line 64 (hardcoded path)
- [ ] Update `bsext_init` line 176 (`setup_gstreamer_env` paths)
- [ ] Update `sh/uninstall.sh` (all `ext_npu_gaze` references)
- [ ] Update `package` lines 584, 596 (zip operations)
- [ ] Search and replace in all documentation files
- [ ] Create `manifest-config.json` with version
- [ ] Test build and installation
- [ ] Update `CHANGELOG.md`

## See Also

- [BrightSign Extension Development Guide](https://docs.brightsign.biz/display/DOC/Extension+Development)
- [Semantic Versioning Specification](https://semver.org/)
- [LVM Extension Format Documentation](https://docs.brightsign.biz/display/DOC/LVM+Extensions)
