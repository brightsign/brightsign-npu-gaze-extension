# Project Discrepancies and Improvements

## Model Licensing Documentation
- The project uses the RetinaFace model from the Rockchip Model Zoo
- According to `model-licenses.md`, RetinaFace is MIT licensed which is correctly documented and appropriate for commercial use
- The licensing documentation was last updated on May 20, 2025 (which matches the current date context)

## Hardware Support
- The project officially supports two platforms:
  - XT-5 (XT1145, XT2145) with RK3588 SOC - requires OS version 9.0.189
  - LS-5 (LS445) with RK3568 SOC - requires BETA-9.1.49

## Build Requirements
- Correctly emphasizes that model compilation and final build MUST be done on x86_64 architecture
- Orange Pi development is marked as optional
- Required tools are properly documented: Docker, git, cmake

## Model Compilation Process
- Separate compilation paths for RK3588 and RK3568
- Uses Docker to ensure consistent build environment
- Output paths in the README show a discrepancy:
  ```sh
  # Old path commented out
  # mkdir -p ../../install/model/RK3588
  # cp examples/RetinaFace/model/RK3588/RetinaFace.rknn ../../install/model/RK3588/
  
  # New path being used
  mkdir -p ../../install/RK3588/model
  cp examples/RetinaFace/model/RK3588/RetinaFace.rknn ../../install/RK3588/model/
  ```
  The commented-out paths should be removed to avoid confusion.

## Camera Support
- The project states "any camera supported by Linux should work"
- However, the implementation in `bsext_init` shows different default video devices for different SOCs:
  - RK3588: `/dev/video1`
  - Others (including RK3568): `/dev/video0`
  This detail should be mentioned in the camera support section.

## Build Process Documentation
- The SDK patching step requires downloading a binary from `v2.3.2` of rknn-toolkit2, but the project clones `v2.3.0`
- This version mismatch should be addressed or explained

## Production Deployment Section
- The "for production" section is marked as "under development" 
- This is a significant gap for a project labeled as ALPHA RELEASE

## Extension Cleanup
- The uninstall instructions mention both `/dev/mapper/bsext_npu_gaze` and `/dev/mapper/bsos-ext_npu_gaze`
- The dual paths could cause confusion; the correct path should be clarified

## Directory Structure
- The `toolkit/` directory mentioned in setup is not shown in the initial workspace structure
- The installation steps create and use this directory, but it should be documented in the project structure

## Key Recommendations
1. Standardize the model output paths in documentation
2. Update camera device documentation to reflect SOC differences
3. Align rknn-toolkit2 versions (either update clone to 2.3.2 or update SDK patch to 2.3.0)
4. Complete the production deployment section
5. Clarify the correct extension paths for uninstallation
6. Document the full directory structure including dynamically created directories
7. Remove commented-out obsolete paths to reduce confusion
8. Add more detail about video device differences in the camera support section

The project is generally well-documented and structured, but these discrepancies should be addressed to improve clarity and reliability.
