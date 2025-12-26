# Orange Pi 5 Development Workflow Summary

Here's the step-by-step process for developing on an Orange Pi 5:

## Phase 1: On x86_64 Host Machine (required first)

**Step 1-2** - Model compilation must be done on x86_64 - Orange Pi can't do this:
```bash
./setup
./compile-models
```
This creates models in `install/RK3588/model/` (for Orange Pi 5) or `install/RK3568/model/` (for Orange Pi 3/4).

**Step 3** - Transfer compiled models to Orange Pi:
```bash
scp -r install/ user@orangepi-ip:/path/to/project/
```

## Phase 2: On Orange Pi

**Step 4** - Install dependencies:
```bash
sudo apt update && sudo apt install -y cmake gdb git libboost-all-dev \
    libturbojpeg-dev libjpeg-turbo8-dev libjpeg-turbo-progs libopencv-dev build-essential
```

**Step 5** - Clone project and setup RKNN runtime:
```bash
git clone <repo-url>
cd brightsign-npu-gaze-extension
mkdir -p lib
# Download librknnrt.so and librga.so from Rockchip repos
```

**Step 6** - Native build:
```bash
mkdir -p build && cd build
cmake .. -DTARGET_SOC="rk3588"
make -j$(nproc)
make install
```

**Step 7** - Test:
```bash
cd ../install/RK3588
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
./attention_demo ./model/RetinaFace.rknn /dev/video0
```

## Phase 3: Deployment back to BrightSign

Once development is done, return to x86_64 host:
```bash
./build-apps
./package
```

**Key Point**: Models compile on x86_64 → transfer to Orange Pi → develop/test natively on Orange Pi → final cross-compile back on x86_64 for BrightSign deployment.
