# BrightSign NPU Gaze Extension

___STATUS___: In development.

| Step | Status |
| --- | --- |
| Model compilation instructions | validated and tested |
| Orange Pi development guide | reference build validated and tested |
| Build for XT5 instructions | validated and tested |
| Extension Packaging Instructions | validated for development |

A BrightSign OS (BSOS) Extension for the Gaze Detection demo of the NPU (AI/ML) feature preview.

This repository gives the steps and tools to

1. Compile ONNX formatted models for use on the Rockchip RK3588 SoC -- used in the OrangePi 5 and XT-5 Player.
2. Develop and test an AI Application to load and run the model on the RK3588.
3. Build the AI Application for BrightSign OS
4. Package the Application and model as a BrightSign Extension

For this exercise, the RetinaFace model from the [Rockchip Model Zoo](https://github.com/airockchip/rknn_model_zoo).  The application code in this repo was adapted from the example code from the Rockchip Model Zoo as well.

## Application Overview

This project will create an installable BrightSign Extension that

1. Loads the compiled model into the RK3588 NPU
2. Acquires images from an attached Video for Linux (v4l) device such as a USB webcam (using OpenCV)
3. Runs the model for each captured image to detect faces in the image
4. For each face found:

   - determine if the face is looking at the screen
   - count the total number of faces
   - count the number of faces looking at the screen
   - decorate the captured image with a bounding box for the face
   - decorate the captured image with dots for the facial features

5. Save the decorated image (overwriting any previous image) to `/tmp/output.jpg`
6. Publish a UDP message on port `:5002` in JSON format with the properties

| property | value |
| --- | --- |
| `faces_in_frame_total` | count of all faces in the current frame |
| `faces_attending` | the number of faces in the current frame that are estimated to be paying attention to the screen |
| `timestamp` | time of message |

The use of UDP for prediction output is for simplicity when integrating with BrightSign presentations that can easily read from this source.

## Project Overview & Requirements

This repository describes building the project in these major steps:

1. Compile the ONNX formatted model into _RKNN_ format for the Rockchip NPU
2. Building and testing the model and application code on an [Orange Pi 5 Plus](http://www.orangepi.org/html/hardWare/computerAndMicrocontrollers/service-and-support/Orange-Pi-5-plus.html). ___NB__-_ this is optional, but is included as a guide to developing other applications
3. Building and testing the model and application code on a [BrightSign XT-5 Player](https://www.brightsign.biz/brightsign-players/series-5/xt5/)
4. Packaging the application and model as a BrightSign Extension

__IMPORTANT: THE TOOLCHAIN REFERENCED BY THIS PROJECT REQUIRES A DEVELOPMENT HOST WITH x86_64 (aka AMD64) INSTRUCTION SET ARCHITECTURE.__ This means that many common dev hosts such as Macs with Apple Silicon or ARM-based Windows and Linux computers __WILL NOT WORK.__  That also includes the OrangePi5Plus (OPi) as it is ARM-based. The OPi ___can___ be used to develop the application with good effect, but the model compilation and final build for BrigthSign OS (BSOS) ___must___ be performed on an x86_64 host.

### Requirements

1. A series 5 player running an experimental, debug build of BrightSign OS -- signed and unsigned versions
2. A Development computer with x86_64 instruction architecture to compile the model and cross-compile the executables.
3. The cross compile toolchain ___matching the BSOS version___ of the player.
4. USB webcam

   - Tested to work with Logitech c270
   - also known to work with [Thustar document/web cam](https://www.amazon.com/gp/product/B09C255SW7/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
   - _should_ work with any UVC device

5. Cables, switches, monitors, etc to connect it all.

#### Software Requirements -- Development Host

* [Docker](https://docs.docker.com/engine/install/) - it should be possible to use podman or other, but these instructions assume Docker
* [git](https://git-scm.com/)

## Step 0 - Setup

1. Install [Docker](https://docs.docker.com/engine/install/)
2. Clone this repository -- later instructions will assume to start from this directory unless otherwise noted.

```bash
#cd path/to/your/directory
git clone git@github.com:brightsign/brightsign-npu-gaze-extension.git
cd brightsign-npu-gaze-extension

export project_root=$(pwd)
# this environment variable is used in the following scripts to refer to the root of the project
```

3. Clone the supporting Rockchip repositories (this can take a while)

```sh
cd "${project_root:-.}"
mkdir -p toolkit && cd $_

git clone https://github.com/airockchip/rknn-toolkit2.git --depth 1 --branch v2.3.0
git clone https://github.com/airockchip/rknn_model_zoo.git --depth 1 --branch v2.3.0

cd -
```

4. Install the BSOS SDK

(*Required** to build binaries that will execute properly on the target XT5 player. Contact BrightSign if you do not have an appropriate SDK.)

Building any executable requires access to correct versions of headers and libraries that are compatible with the target run-time environment. For Yocto-derived projects like BSOS, the build process for the OS also creates an SDK/toolchain package that can be used to cross-compile projects for the target player.

**_[DEPRECATED - Build from public source]_** Download the [SDK version that matches the pre-release OS with OpenCV Support](https://brightsigninfo-my.sharepoint.com/:u:/r/personal/gherlein_brightsign_biz/Documents/BrightSign-NPU-Share-Quividi/brightsign-x86_64-cobra-toolchain-9.1.22.2-unreleased-opencv-for-gaze-demo-20250324.sh?csf=1&web=1&e=Vbr9bx)

**Build a custom SDK from public source**

The platform SDK can be built from public sources. Browse OS releases from the [BrightSign Open Source](https://docs.brightsign.biz/space/DOC/2378039297/BrightSign+Open+Source+Resources) page.  Set the environment variable in the next code block to the desired os release version.

```sh
# Download BrightSign OS and extract
cd "${project_root:-.}"

export BRIGHTSIGN_OS_MAJOR_VERION=9.0
export BRIGHTSIGN_OS_MINOR_VERION=189
export BRIGHTSIGN_OS_VERSION=${BRIGHTSIGN_OS_MAJOR_VERION}.${BRIGHTSIGN_OS_MINOR_VERION}

wget https://brightsignbiz.s3.amazonaws.com/firmware/opensource/${BRIGHTSIGN_OS_MAJOR_VERION}/${BRIGHTSIGN_OS_VERSION}/brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz
wget https://brightsignbiz.s3.amazonaws.com/firmware/opensource/${BRIGHTSIGN_OS_MAJOR_VERION}/${BRIGHTSIGN_OS_VERSION}/brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz

tar -xzf brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz
tar -xzf brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz

# Patch BrightSign OS with some special recipes for the SDK
# Apply custom recipes to BrightSign OS source
rsync -av bsoe-recipes/ brightsign-oe/ 

# clean up disk space
rm brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz
rm brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz

```

```sh
# Build the SDK
cd "${project_root:-.}/brightsign-oe/build"

MACHINE=cobra ./bsbb brightsign-sdk

# move the SDK to the project root
mv tmp-glibc/deploy/sdk/*.sh ../../

# clean up disk space
cd ../..
rm -rf brightsign-oe
```

**INSTALL INTO `./sdk`**

You can access the SDK from BrightSign.  The SDK is a shell script that will install the toolchain and supporting files in a directory of your choice.  This [link](https://brightsigninfo-my.sharepoint.com/:f:/r/personal/gherlein_brightsign_biz/Documents/BrightSign-NPU-Share-Quividi?csf=1&web=1&e=bgt7F7) is limited only to those with permissions to access the SDK.

```sh
cd "${project_root:-.}"

sh "brightsign-x86_64-cobra-toolchain-*.sh"
# answer the questions, use `./sdk` for the installation directory
```

Patch the SDK to include the Rockchip binary libraries that are closed source

```sh
cd "${project_root:-.}"/sdk/sysroots/aarch64-oe-linux/usr/lib

wget https://github.com/airockchip/rknn-toolkit2/blob/v2.3.2/rknpu2/runtime/Linux/librknn_api/aarch64/librknnrt.so
```

### Unsecure the Player and update OS

* Enabling the Diagnostic Web Server (DWS) is recommended as it's a handy way to transfer files and check various things on the player.  This can be done in BrightAuthor:Connected when creating setup files for a new player.

0. Power off the player
1. __Enable serial control__ | Connect a serial cable from the player to your development host.  Configure your terminal program for 115200 bps, no parity, 8 data bits, 1 stop bit (n-8-1) and start the terminal program.  Hold the __`SVC`__ button while applying power. _Quick_, like a bunny, type Ctl-C in your serial terminal to get the boot menu -- you have 3 seconds to do this.  type

```bash
=> console on
=> reboot
```

2. __Reboot the player again__ using the __`RST`__ button or the _Reboot_ button from the __Control__ tab of DWS for the player.  Within the first 3 seconds after boot, again type Ctl-C in your serial terminal program to get the boot prompt and type:

```bash
=> setenv SECURE_CHECKS 0
=> envsave
=> printenv
```

Verify that `SECURE_CHECKS` is set to 0. And type `reboot`.

**The player is now unsecured.**

3. Download a [pre-released OS version with OpenCV support](https://brightsigninfo-my.sharepoint.com/:u:/r/personal/gherlein_brightsign_biz/Documents/BrightSign-NPU-Share-Quividi/cobra-9.1.22.2-unreleased-opencv-for-gaze-demo-20250324-debug_sfrancis-bsoe-update.bsfw?csf=1&web=1&e=QVFKbZ).
4. Use DWS __SD__ tab to _Browse_ and _Upload_ the OS update to the player. From the __Control__ tab, press the _Reboot_ button.  The player will automatically update the OS on reboot.

Verify the OS was updated on the __Info__ tab of DWS where the `BrightSign OS Version` should be listed as
`9.1.22.2-unreleased-opencv-for-gaze-demo-20250324-debug_sfrancis-bsoe`

## Step 1 - Compile ONNX Models for the Rockchip NPU

**This step needs only be peformed once or when the model itself changes**

To run common models on the Rockchip NPU, the models must converted, compiled, lowered on to the operational primitives supported by the NPU from the abstract operations of the model frameworkd (e.g TesnsorFlow or PyTorch). Rockchip supplies a model converter/compiler/quantizer, written in python with lots of package dependencies. To simplify and stabilize the process a Dockerfile is provided in the `rknn-toolkit2` project.

__REQUIRES AN x86_64 INSTRUCTION ARCHITECTURE MACHINE OR VIRTUAL MACHINE__

For portability and repeatability, a Docker container is used to compile the models.

This docker image needs only be built once and can be reused across models

```sh
cd "${project_root:-.}"/toolkit/rknn-toolkit2/rknn-toolkit2/docker/docker_file/ubuntu_20_04_cp38
docker build --rm -t rknn_tk2 -f Dockerfile_ubuntu_20_04_for_cp38 .
```

Download the model (also only necesary one time, it will be stored in the filesystem)

```sh
cd "${project_root:-.}"/toolkit/rknn_model_zoo/

mkdir -p examples/RetinaFace/model/RK3588
pushd examples/RetinaFace/model
chmod +x ./download_model.sh && ./download_model.sh
popd
```

Compile the model.  Note the opetion for various SoCs.

```sh
cd "${project_root:-.}"/toolkit/rknn_model_zoo/

docker run -it --rm -v $(pwd):/zoo rknn_tk2 /bin/bash \
    -c "cd /zoo/examples/RetinaFace/python && python convert.py ../model/RetinaFace_mobile320.onnx rk3588 i8 ../model/RK3588/RetinaFace.rknn"

# move the generated model to the right place
# mkdir -p ../../install/model/RK3588
# cp examples/RetinaFace/model/RK3588/RetinaFace.rknn ../../install/model/RK3588/

mkdir -p ../../model/RK3588
cp examples/RetinaFace/model/RK3588/RetinaFace.rknn ../../model/RK3588/

```

**The necessary binaries (model, libraries) are now in the `install` directory of the project**

## (Optional) Step 2 - Build and test on Orange Pi

_this section under development_

TODO: refine build to use libraries provided by SDK to cross-build

While not strictly required, it can be handy to move the project to an OrangePi (OPi) as this facilitates a more responsive build and debug process due to a fully linux distribution and native compiler. Consult the [Orange Pi Wiki](http://www.orangepi.org/orangepiwiki/index.php/Orange_Pi_5_Plus) for more information.

Use of the Debian image from the eMMC is recommended. Common tools like `git`, `gcc` and `cmake` are also needed to build the project. In the interest of brevity, installation instructions for those are not included with this project.

**FIRST**: Clone this project tree to the OPi

___Unless otherwise noted all commands in this section are executed on the OrangePi -- via ssh or other means___

```sh
#cd path/to/your/directory
git clone git@github.com:brightsign/brightsign-npu-gaze-extension.git
cd brightsign-npu-gaze-extension

export project_root=$(pwd)
# this environment variable is used in the following scripts to refer to the root of the project
```

**SECOND**: Copy the `install` directory, all sub directories and files to the OPi

```sh
# example using scp... 
cd "${project_root:-.}"

# customize as needed
export dev_user=dev_user
export dev_host=192.168.x.x
export project_path=/path/to/project/on/dev_host

# copy the files to the device
scp -r  $dev_user@$dev_host:$project_path/install ./
# may propmpt for password depending on your setup

# check the files are there -- sample output shown
tree -Dsh install/
# [4.0K Mar 21 07:48]  install/
# ├── [4.0K Mar 21 07:50]  lib
# │   ├── [167K Mar 21 07:50]  librga.so
# │   └── [4.4M Mar 21 07:49]  librknnrt.so
# └── [4.0K Mar 21 07:48]  model
#     └── [4.0K Mar 21 07:48]  RK3588
#         └── [ 18M Mar 21 07:48]  RetinaFace.rknn

# 3 directories, 3 files

```

**Build the project**

```sh
cd "${project_root:-.}"

# this command can be used to clean old builds
#rm -rf build

mkdir -p build && cd $_

cmake .. -DTARGET_SOC="rk3588"
make
make install
```

## Step 3 - Build and Test on XT5

The BrightSign SDK for the specific BSOS version must be used on an x86 host to build the binary that can be deployed on to the XT5 player.

_Ensure you have installed the SDK in `${project_root}/sdk` as described in Step 0 - Setup._

The setup script `environment-setup-aarch64-oe-linux` will set appropriate paths for the toolchain and files. This script must be `source`d in every new shell.

**_[DEPRECATED -- USE A CUSTOM SDK]_**  [CMake](https://cmake.org/) is needed to build OpenCV

### Build the app

```sh
cd "${project_root:-.}"

source ./sdk/environment-setup-aarch64-oe-linux

# this command can be used to clean old builds
#rm -rf build

mkdir -p build && cd $_

cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3588" 
  # -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make

#rm -rf ../install
make install
```

**The built binary and libraries are copied into the `install` directory alongside the model binary.**

You can now copy that directory to the player and run it.

**Suggested workflow**

* zip the install dir and upload to player sd card
* ssh to player and exit to linux shell
* expand the zip to `/usr/local/gaze` (which is mounted with exec)

_If you are unfamiliar with this workflow or have not un-secured your player, consult BrightSign._

## Step 4 - Package the Extension

Copy the extension scripts to the install dir

```sh
cd "${project_root:-.}"

cp start-ext.sh install/
cp bsext_init install/
```

Run the make extension script on the install dir

```sh
cd "${project_root:-.}"/install

../sh/make-example-extension-lvm
# zip for convenience to transfer to player
zip ../gaze-demo-$(date +%s).zip ext_npu_gaze*
# clean up
rm -rf ext_npu_gaze*
```

### for development

* Transfer the files `ext_npu_gaze-*.zip` to an unsecured player with the _Browse_ and _Upload_ buttons from the __SD__ tab of DWS or other means.
* Connect to the player via ssh, telnet, or serial.
* Type Ctl-C to drop into the BrightScript Debugger, then type `exit` to the BrightSign prompt and `exit` again to get to the linux command prompt.

At the command prompt, **install** the extension with:

```bash
cd /storage/sd
# if you have multiple builds on the card, you might want to delete old ones
# or modify the unzip command to ONLY unzip the version you want to install
unzip ext_npu_gaze-*.zip
# you may need to answer prompts to overwrite old files

# install the extension
bash ./ext_npu_gaze_install-lvm.sh

# the extension will be installed on reboot
reboot
```

The gaze demo application will start automatically on boot (see `bsext_init` and `start-ext.sh`).  Files will have been unpacked to `/var/volatile/bsext/ext_npu_gaze`.

### for production

_this section under development_

* Submit the extension to BrightSign for signing
* the signed extension will be packaged as a `.bsfw` file that can be applied to a player running a signed OS.
