cmake_minimum_required(VERSION 3.4.1)

project(attention_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")

# disable RGA for XT5 as it is throwning errors
add_definitions(-DDISABLE_RGA)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
# OrangePi or other for development
    find_package(Boost REQUIRED COMPONENTS filesystem system)
    find_package(OpenCV REQUIRED)

    set(MY_LIBS ${Boost_LIBRARIES} ${OpenCV_LIBS})
else()
# cross-compile from x86 for rk3588, XT5
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-poison-system-directories")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wpoison-system-directories")

    set(BOOST_ROOT ${CMAKE_SOURCE_DIR}/sdk/sysroots/aarch64-oe-linux/usr)

    include_directories(${OECORE_TARGET_SYSROOT}/usr/include)
    include_directories(${OECORE_TARGET_SYSROOT}/usr/include)
    include_directories(${OECORE_TARGET_SYSROOT}/usr/include/opencv4)
    include_directories(${OECORE_TARGET_SYSROOT}/usr/include/rga)

    link_directories(${OECORE_TARGET_SYSROOT}/usr/lib)
    set(MY_LIBS boost_system boost_filesystem
        opencv_core opencv_aruco opencv_bgsegm opencv_calib3d
        opencv_ccalib opencv_datasets opencv_features2d opencv_gapi
        opencv_hfs opencv_imgcodecs opencv_img_hash opencv_imgproc
        opencv_line_descriptor opencv_optflow opencv_photo opencv_quality
        opencv_rapid opencv_rgbd opencv_saliency opencv_shape
        opencv_stereo opencv_stitching opencv_superres opencv_surface_matching
        opencv_tracking opencv_ts opencv_videoio opencv_video
        opencv_videostab opencv_xfeatures2d opencv_ximgproc opencv_xphoto
    )
endif()

# Set the build type to Debug to include debug symbols
set(CMAKE_BUILD_TYPE Debug)

# Add the -g flag to the compiler options
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# install target and libraries
if(NOT DEFINED TARGET_SOC)
  set(TARGET_SOC rk3588)
endif()

if(NOT DEFINED LIB_ARCH)
  set(LIB_ARCH aarch64)
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME} cmake_system_name_lower)
string(TOLOWER ${LIB_ARCH} lib_arch_lower)
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/${TARGET_SOC}_${cmake_system_name_lower}_${lib_arch_lower})

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# rknn api
set(LIB_ARCH aarch64)
set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/include/librknnrt.so)

#rga -- the 3588 rga lib is okay 3576
# set(RGA_PATH ${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588)
# set(RGA_LIB ${RGA_PATH}/lib/Linux//${LIB_ARCH}/librga.so)
set(RGA_LIB ${OECORE_TARGET_SYSROOT}/usr/lib/librga.so)

# turbojpeg
# set(TURBOJPEG_PATH ${CMAKE_SOURCE_DIR}/include/3rdparty/turbojpeg/lib/Linux/aarch64)
# set(TURBOJPEG_LIB ${TURBOJPEG_PATH}/libturbojpeg.a)


# include_directories(${RKNN_API_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/3rdparty)
include_directories(${RGA_PATH}/include)

# opencv
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  find_package(OpenCV REQUIRED)
  find_package(TurboJPEG REQUIRED)
else()
  message(STATUS "OECORE_TARGET_SYSROOT: ${OECORE_TARGET_SYSROOT}")
  include_directories(${OECORE_TARGET_SYSROOT}/usr/include)
  include_directories(${OECORE_TARGET_SYSROOT}/usr/include/opencv4)
  include_directories(${OECORE_TARGET_SYSROOT}/usr/include/rga)
  include_directories(${OECORE_TARGET_SYSROOT}/usr/include/turbojpeg)
endif()

set(CMAKE_INSTALL_RPATH "lib")

if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  get_filename_component(OECORE_TARGET_SYSROOT_ABS "${OECORE_TARGET_SYSROOT}" ABSOLUTE)
  message(STATUS "OECORE_TARGET_SYSROOT_ABS: ${OECORE_TARGET_SYSROOT_ABS}")
  set(OpenCV_LIBS
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_core.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_aruco.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_bgsegm.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_calib3d.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_ccalib.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_datasets.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_features2d.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_gapi.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_hfs.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_highgui.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_imgcodecs.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_img_hash.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_imgproc.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_line_descriptor.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_optflow.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_photo.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_quality.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_rapid.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_rgbd.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_saliency.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_shape.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_stereo.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_stitching.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_superres.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_surface_matching.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_tracking.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_ts.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_videoio.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_video.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_videostab.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_xfeatures2d.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_ximgproc.so
    ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_xphoto.so
  )
  set(TURBOJPEG_LIB ${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libturbojpeg.so)
endif()

set(CMAKE_INSTALL_RPATH "lib")

# rknn_yolov5_demo
include_directories( ${CMAKE_SOURCE_DIR}/include)

add_executable(attention_demo
        src/main.cpp
        src/attention.cpp
        src/file_utils.c
        src/image_utils.c
        src/inference.cpp
        src/publisher.cpp
        src/retinaface.cc
        src/utils.cc
)

target_link_libraries(attention_demo
  ${RKNN_RT_LIB}
  ${OpenCV_LIBS}
  ${RGA_LIB}
  ${TURBOJPEG_LIB}
)

# Custom command to copy .so files to the build directory
add_custom_command(TARGET attention_demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${RKNN_RT_LIB} $<TARGET_FILE_DIR:attention_demo>
  COMMAND ${CMAKE_COMMAND} -E copy ${RGA_LIB} $<TARGET_FILE_DIR:attention_demo>
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/model $<TARGET_FILE_DIR:attention_demo>/model
)

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/)
install(TARGETS attention_demo DESTINATION ./)

# Use FILE(COPY) which preserves the actual files rather than symlinks
install(CODE "
    # Create lib directory if it doesn't exist
    file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/lib)
    
    # Copy RKNN runtime library
    execute_process(COMMAND cp -L ${RKNN_RT_LIB} \${CMAKE_INSTALL_PREFIX}/lib/)
    
    # Copy RGA library and its versioned counterparts
    execute_process(
        COMMAND bash -c \"find $(dirname ${RGA_LIB}) -name 'librga.so*' -type f -o -type l | xargs -I{} cp -L {} \${CMAKE_INSTALL_PREFIX}/lib/\"
    )
    
    # Copy TurboJPEG library and its versioned counterparts
    execute_process(
        COMMAND bash -c \"find $(dirname ${TURBOJPEG_LIB}) -name 'libturbojpeg.so*' -type f -o -type l | xargs -I{} cp -L {} \${CMAKE_INSTALL_PREFIX}/lib/\"
    )
    
    # Copy OpenCV libraries
    file(GLOB OPENCV_LIB_FILES \"${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libopencv_*.so*\")
    foreach(lib \${OPENCV_LIB_FILES})
        execute_process(COMMAND cp -L \${lib} \${CMAKE_INSTALL_PREFIX}/lib/)
    endforeach()

    # Copy tbb libraries
    file(GLOB TBB_LIB_FILES \"${OECORE_TARGET_SYSROOT_ABS}/usr/lib/libtbb*.so*\")
    foreach(lib \${TBB_LIB_FILES})
        execute_process(COMMAND cp -L \${lib} \${CMAKE_INSTALL_PREFIX}/lib/)
    endforeach()
")