# BrightSign NPU Gaze Extension

**_STATUS_**: In development.


Model compilation instructions validated and tested.

Orange Pi development guide pending

Build for XT5 instructions validated and tested

Extension Packaging Instructions in development

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

**This guide assumes you have the SDK `brightsign-x86_64-cobra-toolchain-9.1.22.2-BCN-17804-unreleased-pensando-gaze-demo-20250304.sh` in the project root**

```sh
cd "${project_root:-.}"

sh brightsign-x86_64-cobra-toolchain-9.1.22.2-BCN-17804-unreleased-pensando-gaze-demo-20250304.sh
# answer the questions, use `./sdk` for the installation directory
```

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
mkdir -p ../../install/model/RK3588
cp examples/RetinaFace/model/RK3588/RetinaFace.rknn ../../install/model/RK3588/
```

TODO:  DELETE -- this isn't needed anymore ?Copy run-time libs to the right place for later

```sh
# cd "${project_root:-.}"/toolkit/rknn_model_zoo/

# mkdir -p ../../install/lib
# cp 3rdparty/rknpu2/Linux/armhf/librknnrt.so ../../install/lib/
# cp 3rdparty/librga/Linux/armhf/librga.so ../../install/lib/
```

**The necessary binaries (model, libraries) are now in the `install` directory of the project**

## (Optional) Step 2 - Build and test on Orange Pi

While not strictly required, it can be handy to move the project to an OrangePi (OPi) as this facilitates a more responsive build and debug process due to a fully linux distribution and native compiler. Consult the [Orange Pi Wiki](http://www.orangepi.org/orangepiwiki/index.php/Orange_Pi_5_Plus) for more information.

Use of the Debian image from the eMMC is recommended. Common tools like `git`, `gcc` and `cmake` are also needed to build the project. In the interest of brevity, installation instructions for those are not included with this project.

**FIRST**: Copy this project tree to the OPi (can git clone and then copy the binaries built in Step 1 from the `install` directory or use network mounts).

**THEN**: Connect to the OPi using a local head, ssh, VSCode remote or other mechanism.

```sh
### TODO: add build instructions
```

## Step 3 - Build and Test on XT5

The BrightSign SDK for the specific BSOS version must be used on an x86 host to build the binary that can be deployed on to the XT5 player.

_Ensure you have installed the SDK in `${project_root}/sdk` as described in Step 0 - Setup._

The setup script `environment-setup-aarch64-oe-linux` will set appropriate paths for the toolchain and files. This script must be `source`d in every new shell.

```sh
cd "${project_root:-.}"

source ./sdk/environment-setup-aarch64-oe-linux

# this command can be used to clean old builds
#rm -rf build

mkdir -p build && cd $_

cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3588"
make
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
```

### for development

Transfer the files `install/ext_npu_gaze.squashfs` and `install/ext_npu_gaze_install-lvm.sh` to an unsecured player. From a linux command on the player (ssh, telnet, or serial / ctl-c, exit, exit to get to command prompt), copy both files to an executable location (e.g. `cp /storage/sd/ext_npu_* /usr/local/`) and run the install script `sh /usr/local/ext_npu_gaze_install-lvm.sh` to install the extension.  Ctrl-D or exit to resume the BrightScript interpretter.
