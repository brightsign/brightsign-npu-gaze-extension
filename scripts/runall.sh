#!/bin/bash

# BrightSign NPU Gaze Extension - Complete Build Script
# This script automates all the steps from the README.md

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Global variables
AUTO_MODE=false
SKIP_ARCH_CHECK=false
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -auto|--auto)
            AUTO_MODE=true
            shift
            ;;
        --skip-arch-check)
            SKIP_ARCH_CHECK=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-auto|--auto] [--skip-arch-check]"
            echo "  -auto: Run all steps without prompting for confirmation"
            echo "  --skip-arch-check: Skip x86_64 architecture check (for testing)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h for help"
            exit 1
            ;;
    esac
done

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}\n"
}

# Function to prompt user for continuation
prompt_continue() {
    if [ "$AUTO_MODE" = true ]; then
        print_status "Auto mode: Continuing automatically..."
        return 0
    fi

    local message="$1"
    echo -e "\n${YELLOW}NEXT STEPS:${NC}"
    echo "$message"
    echo
    read -p "Do you want to continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status "Exiting..."
        exit 0
    fi
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if docker is running
check_docker_running() {
    if ! docker info >/dev/null 2>&1; then
        print_error "Docker is not running. Please start Docker and try again."
        exit 1
    fi
}

# STEP 0: Setup
step0_setup() {
    print_header "STEP 0: Setup"
    
    prompt_continue "This will:
- Check Docker installation
- Clone Rockchip repositories (rknn-toolkit2, rknn_model_zoo)
- Download and build BrightSign OS SDK
- Provide instructions for unsecuring the player"

    # Check Docker
    if ! command_exists docker; then
        print_error "Docker is not installed. Please install Docker first:"
        print_error "https://docs.docker.com/engine/install/"
        exit 1
    fi
    check_docker_running
    print_status "Docker is installed and running"

    # Check other required tools
    if ! command_exists git; then
        print_error "Git is not installed. Please install git first."
        exit 1
    fi
    
    if ! command_exists cmake; then
        print_error "CMake is not installed. Please install cmake first."
        exit 1
    fi
    
    if ! command_exists wget; then
        print_error "wget is not installed. Please install wget first."
        exit 1
    fi

    print_status "All required tools are installed"

    # Set project root environment variable
    export project_root="$PROJECT_ROOT"
    print_status "Project root set to: $project_root"

    # Clone supporting repositories
    print_status "Cloning supporting Rockchip repositories..."
    cd "$project_root"
    mkdir -p toolkit && cd toolkit

    if [ ! -d "rknn-toolkit2" ]; then
        git clone https://github.com/airockchip/rknn-toolkit2.git --depth 1 --branch v2.3.0
    else
        print_status "rknn-toolkit2 already exists"
    fi

    if [ ! -d "rknn_model_zoo" ]; then
        git clone https://github.com/airockchip/rknn_model_zoo.git --depth 1 --branch v2.3.0
    else
        print_status "rknn_model_zoo already exists"
    fi

    cd "$project_root"

    # Install BSOS SDK
    print_status "Setting up BrightSign OS SDK..."
    
    # Set OS version variables
    export BRIGHTSIGN_OS_MAJOR_VERSION=9.0
    export BRIGHTSIGN_OS_MINOR_VERSION=189
    export BRIGHTSIGN_OS_VERSION=${BRIGHTSIGN_OS_MAJOR_VERSION}.${BRIGHTSIGN_OS_MINOR_VERSION}
    
    # Download BrightSign OS source if not already downloaded
    if [ ! -f "brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz" ]; then
        print_status "Downloading BrightSign OS source..."
        wget "https://brightsignbiz.s3.amazonaws.com/firmware/opensource/${BRIGHTSIGN_OS_MAJOR_VERSION}/${BRIGHTSIGN_OS_VERSION}/brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz"
        wget "https://brightsignbiz.s3.amazonaws.com/firmware/opensource/${BRIGHTSIGN_OS_MAJOR_VERSION}/${BRIGHTSIGN_OS_VERSION}/brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz"
    else
        print_status "BrightSign OS source already downloaded"
    fi

    # Extract if not already extracted
    if [ ! -d "brightsign-oe" ]; then
        print_status "Extracting BrightSign OS source..."
        tar -xzf "brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz"
        tar -xzf "brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz"
        
        # Apply custom recipes
        rsync -av bsoe-recipes/ brightsign-oe/
        
        # Clean up
        rm "brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz"
        rm "brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz"
    else
        print_status "BrightSign OS source already extracted"
    fi

    # Build SDK in Docker
    if [ ! -f "Dockerfile" ]; then
        print_status "Downloading Dockerfile..."
        wget https://raw.githubusercontent.com/brightsign/extension-template/refs/heads/main/Dockerfile
    fi

    if ! docker images | grep -q "bsoe-build"; then
        print_status "Building BSOS Docker image..."
        docker build --rm --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) --ulimit memlock=-1:-1 -t bsoe-build .
    else
        print_status "BSOS Docker image already exists"
    fi

    mkdir -p srv

    # Check if SDK already exists
    if [ ! -f "brightsign-x86_64-cobra-toolchain-${BRIGHTSIGN_OS_VERSION}.sh" ]; then
        print_status "Building BrightSign SDK (this may take several hours)..."
        docker run -it --rm \
            -v $(pwd)/brightsign-oe:/home/builder/bsoe \
            -v $(pwd)/srv:/srv \
            bsoe-build \
            bash -c "cd /home/builder/bsoe/build && MACHINE=cobra ./bsbb brightsign-sdk"
        
        # Copy the SDK
        cp brightsign-oe/build/tmp-glibc/deploy/sdk/brightsign-x86_64-cobra-toolchain-${BRIGHTSIGN_OS_VERSION}.sh ./
    else
        print_status "SDK already exists"
    fi

    # Install SDK
    if [ ! -d "sdk" ]; then
        print_status "Installing SDK..."
        ./brightsign-x86_64-cobra-toolchain-${BRIGHTSIGN_OS_VERSION}.sh -d ./sdk -y
        
        # Patch SDK with Rockchip libraries
        cd sdk/sysroots/aarch64-oe-linux/usr/lib
        if [ ! -f "librknnrt.so" ]; then
            wget https://github.com/airockchip/rknn-toolkit2/raw/v2.3.2/rknpu2/runtime/Linux/librknn_api/aarch64/librknnrt.so
        fi
        cd "$project_root"
    else
        print_status "SDK already installed"
    fi

    print_status "Step 0 completed successfully!"
    
    print_warning "MANUAL STEP REQUIRED: You need to unsecure your BrightSign player"
    print_warning "Follow the instructions in the README.md under 'Unsecure the Player'"
    print_warning "This involves connecting serial cable and using boot commands"
}

# STEP 1: Compile ONNX Models
step1_compile_models() {
    print_header "STEP 1: Compile ONNX Models for Rockchip NPU"
    
    prompt_continue "This will:
- Build Docker container for model compilation
- Download RetinaFace model
- Compile model for RK3588 (XT-5 players)
- Compile model for RK3568 (LS-5 players)"

    cd "$project_root/toolkit/rknn-toolkit2/rknn-toolkit2/docker/docker_file/ubuntu_20_04_cp38"
    
    # Build Docker image for model compilation
    if ! docker images | grep -q "rknn_tk2"; then
        print_status "Building RKNN Toolkit Docker image..."
        docker build --rm -t rknn_tk2 -f Dockerfile_ubuntu_20_04_for_cp38 .
    else
        print_status "RKNN Toolkit Docker image already exists"
    fi

    # Download model
    cd "$project_root/toolkit/rknn_model_zoo/"
    mkdir -p examples/RetinaFace/model/RK3588
    mkdir -p examples/RetinaFace/model/RK3568
    
    pushd examples/RetinaFace/model
    if [ ! -f "RetinaFace_mobile320.onnx" ]; then
        print_status "Downloading RetinaFace model..."
        chmod +x ./download_model.sh && ./download_model.sh
    else
        print_status "RetinaFace model already downloaded"
    fi
    popd

    # Compile model for RK3588 (XT-5 players)
    if [ ! -f "examples/RetinaFace/model/RK3588/RetinaFace.rknn" ]; then
        print_status "Compiling model for RK3588 (XT-5 players)..."
        docker run -it --rm -v $(pwd):/zoo rknn_tk2 /bin/bash \
            -c "cd /zoo/examples/RetinaFace/python && python convert.py ../model/RetinaFace_mobile320.onnx rk3588 i8 ../model/RK3588/RetinaFace.rknn"
    else
        print_status "RK3588 model already compiled"
    fi

    # Compile model for RK3568 (LS-5 players)
    if [ ! -f "examples/RetinaFace/model/RK3568/RetinaFace.rknn" ]; then
        print_status "Compiling model for RK3568 (LS-5 players)..."
        docker run -it --rm -v $(pwd):/zoo rknn_tk2 /bin/bash \
            -c "cd /zoo/examples/RetinaFace/python && python convert.py ../model/RetinaFace_mobile320.onnx rk3568 i8 ../model/RK3568/RetinaFace.rknn"
    else
        print_status "RK3568 model already compiled"
    fi

    # Copy models to install directory
    mkdir -p "$project_root/install/RK3588/model"
    mkdir -p "$project_root/install/RK3568/model"
    
    cp examples/RetinaFace/model/RK3588/RetinaFace.rknn "$project_root/install/RK3588/model/"
    cp examples/RetinaFace/model/RK3568/RetinaFace.rknn "$project_root/install/RK3568/model/"

    print_status "Step 1 completed successfully!"
}

# STEP 3: Build and Test on XT5
step3_build_xt5() {
    print_header "STEP 3: Build and Test on XT5"
    
    prompt_continue "This will:
- Build application for XT5 (RK3588)
- Build application for LS5 (RK3568)
- Install binaries and libraries to install directory"

    cd "$project_root"
    
    # Source the SDK environment
    source ./sdk/environment-setup-aarch64-oe-linux

    # Build for XT5 (RK3588)
    print_status "Building for XT5 (RK3588)..."
    rm -rf build_xt5
    mkdir -p build_xt5 && cd build_xt5
    
    cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3588"
    make
    make install
    
    cd "$project_root"

    # Build for LS5 (RK3568)
    print_status "Building for LS5 (RK3568)..."
    rm -rf build_ls5
    mkdir -p build_ls5 && cd build_ls5
    
    cmake .. -DOECORE_TARGET_SYSROOT="${OECORE_TARGET_SYSROOT}" -DTARGET_SOC="rk3568"
    make
    make install

    cd "$project_root"
    
    print_status "Step 3 completed successfully!"
}

# STEP 4: Package the Extension
step4_package() {
    print_header "STEP 4: Package the Extension"
    
    prompt_continue "This will:
- Copy extension scripts to install directory
- Create development package
- Create production extension package"

    cd "$project_root"
    
    # Copy extension scripts
    cp bsext_init install/ && chmod +x install/bsext_init
    cp sh/uninstall.sh install/ && chmod +x install/uninstall.sh

    # Create development package
    cd "$project_root/install"
    rm -f ../gaze-dev-*.zip
    zip -r "../gaze-dev-$(date +%s).zip" ./
    
    # Create production extension
    ../sh/make-extension-lvm
    rm -f ../gaze-ext-*.zip
    zip "../gaze-ext-$(date +%s).zip" ext_npu_gaze*
    rm -rf ext_npu_gaze*

    cd "$project_root"
    
    print_status "Step 4 completed successfully!"
    print_status "Development package: gaze-dev-*.zip"
    print_status "Production extension: gaze-ext-*.zip"
}

# Main execution
main() {
    print_header "BrightSign NPU Gaze Extension - Complete Build"
    
    if [ "$AUTO_MODE" = true ]; then
        print_status "Running in automatic mode - no prompts"
    else
        print_status "Running in interactive mode - will prompt between steps"
    fi
    
    print_status "Project root: $PROJECT_ROOT"
    
    # Check architecture
    if [ "$(uname -m)" != "x86_64" ] && [ "$SKIP_ARCH_CHECK" != true ]; then
        print_error "This script requires x86_64 architecture"
        print_error "Current architecture: $(uname -m)"
        print_error "Use --skip-arch-check to bypass this check for testing"
        exit 1
    elif [ "$SKIP_ARCH_CHECK" = true ]; then
        print_warning "Skipping architecture check - this is for testing only"
    fi
    
    # Execute steps
    step0_setup
    step1_compile_models
    step3_build_xt5
    step4_package
    
    print_header "BUILD COMPLETE"
    print_status "All steps completed successfully!"
    print_status "Check the install directory for the built files"
    print_status "Development package: gaze-dev-*.zip"
    print_status "Production extension: gaze-ext-*.zip"
    
    print_warning "Don't forget to unsecure your BrightSign player as described in the README!"
}

# Run main function
main "$@"