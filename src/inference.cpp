#include <cstdlib>
#include <iostream>
#include <thread>
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>

#include "attention.h"
#include "inference.h"
#include <unistd.h>

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <dirent.h>

#include <rga/rga.h>
#include <rga/im2d.h>

// Test RTSP connectivity with timeout and better error handling
bool test_rtsp_connectivity(const std::string& rtsp_url, int timeout_seconds = 10) {
    printf("Testing RTSP connectivity to: %s\n", rtsp_url.c_str());
    
    // Extract host and port from RTSP URL for basic connectivity test
    std::string host;
    int port = 554; // Default RTSP port
    
    size_t start = rtsp_url.find("://");
    if (start != std::string::npos) {
        start += 3;
        size_t end = rtsp_url.find(":", start);
        if (end != std::string::npos) {
            host = rtsp_url.substr(start, end - start);
            size_t port_end = rtsp_url.find("/", end);
            if (port_end != std::string::npos) {
                try {
                    port = std::stoi(rtsp_url.substr(end + 1, port_end - end - 1));
                } catch (...) {
                    port = 554; // Fallback to default
                }
            }
        } else {
            size_t path_start = rtsp_url.find("/", start);
            if (path_start != std::string::npos) {
                host = rtsp_url.substr(start, path_start - start);
            } else {
                host = rtsp_url.substr(start);
            }
        }
    }
    
    if (host.empty()) {
        printf("ERROR: Could not extract host from RTSP URL\n");
        return false;
    }
    
    printf("Testing connectivity to host: %s, port: %d\n", host.c_str(), port);
    
    // Use netcat or ping for basic connectivity test with timeout
    std::string test_cmd = "timeout " + std::to_string(timeout_seconds) + 
                          " nc -z " + host + " " + std::to_string(port) + " 2>/dev/null";
    int result = system(test_cmd.c_str());
    
    if (result == 0) {
        printf("RTSP server is reachable at %s:%d\n", host.c_str(), port);
        return true;
    } else {
        printf("WARNING: RTSP server connectivity test failed for %s:%d\n", host.c_str(), port);
        return false;
    }
}


void cv_to_image_buffer(cv::Mat& img, image_buffer_t* image) {
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

    image->width = img.cols;
    image->height = img.rows;
    image->width_stride = img.cols;
    image->height_stride = img.rows;
    image->format = IMAGE_FORMAT_RGB888;
    image->virt_addr = img.data;
    image->size = img.cols * img.rows * 3;
    image->fd = -1;
}

// Simulated ML model inference
InferenceResult MLInferenceThread::runInference(cv::Mat& cap) {
    image_buffer_t image;
    memset(&image, 0, sizeof(image));
    cv_to_image_buffer(cap, &image);

    InferenceResult final_result{-1, -1, std::chrono::system_clock::now()};

    retinaface_result result;
    int ret = inference_retinaface_model(&rknn_app_ctx, 
        &image, &result);
    if (ret != 0) {
        printf("inference_retinaface_model fail! ret=%d\n", ret);
        return final_result;
    }
    final_result.count_all_faces_in_frame = result.count;
    final_result.num_faces_attending = 0;
    
    // Draw boxes on the image
    for (auto i{0}; i < result.count; i++) {
        auto color = cv::Scalar(255, 0, 0);     // red
        if (face_is_looking_at_us(result.object[i])) {
            // std::cout << "Face is looking at us" << std::endl;
            final_result.num_faces_attending += 1;
            color = cv::Scalar(0, 255, 0);     // green

            // draw eyes
            auto left_eye = result.object[i].ponit[0];
            auto right_eye = result.object[i].ponit[1];
            cv::circle(cap, cv::Point(left_eye.x, left_eye.y), 2, cv::Scalar(0, 128, 128), 2);

            // draw the other points
            for (auto j{2}; j < 5; j++) {
                auto point = result.object[i].ponit[j];
                cv::circle(cap, cv::Point(point.x, point.y), 2, cv::Scalar(128, 128, 0), 2);
            }
        }

        auto box = result.object[i].box;
        cv::rectangle(cap, cv::Point(box.left, box.top), cv::Point(box.right, box.bottom), color, 2);     
    }

    // Optionally write processed image to file for debugging
    cv::cvtColor(cap, cap, cv::COLOR_RGB2BGR);

    frames++;

    return final_result;
}

static inline bool rdok(const char* p) { return p && access(p, R_OK) == 0; }

static bool validate_gstreamer_environment() {
    bool valid = true;
    
    // Check critical GStreamer paths
    if (!rdok(getenv("GST_PLUGIN_PATH"))) {
        fprintf(stderr, "ERROR: GST_PLUGIN_PATH not readable: %s\n", 
                getenv("GST_PLUGIN_PATH") ? getenv("GST_PLUGIN_PATH") : "(null)");
        valid = false;
    }
    
    if (!rdok(getenv("GST_PLUGIN_SCANNER"))) {
        fprintf(stderr, "ERROR: GST_PLUGIN_SCANNER not readable: %s\n", 
                getenv("GST_PLUGIN_SCANNER") ? getenv("GST_PLUGIN_SCANNER") : "(null)");
        valid = false;
    }
    
    // Check registry directory is writable
    const char* registry_path = getenv("GST_REGISTRY");
    if (registry_path) {
        std::string registry_dir = std::string(registry_path);
        size_t last_slash = registry_dir.find_last_of('/');
        if (last_slash != std::string::npos) {
            registry_dir = registry_dir.substr(0, last_slash);
            if (access(registry_dir.c_str(), W_OK) != 0) {
                fprintf(stderr, "WARNING: Registry directory not writable: %s\n", registry_dir.c_str());
            }
        }
    }
    
    return valid;
}

static void ensure_gstreamer_runtime() {
    printf("Setting up GStreamer runtime environment...\n");
    
    // Tell GStreamer where the plugins + scanner live
    if (!getenv("GST_PLUGIN_PATH"))
        setenv("GST_PLUGIN_PATH", "/usr/lib/gstreamer-1.0", 1);
    if (!getenv("GST_PLUGIN_SCANNER"))
        setenv("GST_PLUGIN_SCANNER", "/usr/libexec/gstreamer-1.0/gst-plugin-scanner", 1);

    // Writable registry (often required on read-only images)
    if (!getenv("GST_REGISTRY"))
        setenv("GST_REGISTRY", "/tmp/gst-registry.bin", 1);
    setenv("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "1", 1);

    // Helpful diagnostics
    setenv("OPENCV_VIDEOIO_DEBUG", "1", 1);
    if (!getenv("GST_DEBUG")) setenv("GST_DEBUG", "2", 0);  // Moderate logging

    printf("GStreamer environment:\n");
    printf("  GST_PLUGIN_PATH=%s\n", getenv("GST_PLUGIN_PATH"));
    printf("  GST_PLUGIN_SCANNER=%s\n", getenv("GST_PLUGIN_SCANNER"));
    printf("  GST_REGISTRY=%s\n", getenv("GST_REGISTRY"));

    // Validate the environment
    if (!validate_gstreamer_environment()) {
        fprintf(stderr, "WARNING: GStreamer environment validation failed\n");
    } else {
        printf("GStreamer environment validation passed\n");
    }
}

// Build robust RTSP pipelines with fallback options (H.264 + H.265)
static std::vector<std::string> build_rtsp_pipelines(const std::string& url) {
    std::vector<std::string> pipelines;

    //
    // Explicit H.265 (video only) – Hardware accelerated
    //
    pipelines.push_back(
        "rtspsrc location=" + url + " protocols=tcp latency=150 drop-on-latency=true name=src "
        "src. ! application/x-rtp,media=video,encoding-name=H265 ! "
        "rtph265depay ! h265parse ! mppvideodec ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false"
    );

    //
    // Explicit H.265 (video only) – Software fallback
    //
    pipelines.push_back(
        "rtspsrc location=" + url + " protocols=tcp latency=150 drop-on-latency=true name=src "
        "src. ! application/x-rtp,media=video,encoding-name=H265 ! "
        "rtph265depay ! h265parse ! avdec_hevc ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false"
    );

    //
    // Explicit H.264 (video only) – Hardware accelerated
    //
    pipelines.push_back(
        "rtspsrc location=" + url + " protocols=tcp latency=150 drop-on-latency=true name=src "
        "src. ! application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay ! h264parse ! mppvideodec ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false"
    );

    //
    // Explicit H.264 (video only) – Software fallback
    //
    pipelines.push_back(
        "rtspsrc location=" + url + " protocols=tcp latency=150 drop-on-latency=true name=src "
        "src. ! application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false"
    );

    //
    // UDP mode (H.264 fast path, for LAN cameras that don’t like TCP)
    //
    pipelines.push_back(
        "rtspsrc location=" + url + " protocols=udp latency=100 drop-on-latency=true name=src "
        "src. ! application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay ! h264parse ! mppvideodec ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false"
    );

    //
    // Last-resort fallback: decodebin (can hang on XT5 if audio is present, so keep at the end)
    //
    pipelines.push_back(
        "rtspsrc location=" + url + " latency=200 ! "
        "decodebin ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false"
    );

    return pipelines;
}

// Try to open a pipeline with a timeout
static cv::VideoCapture try_open_pipeline(const std::string& pipeline, int timeout_ms = 3000) {
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    printf("Opened pipeline, checking for frames: %s\n", pipeline.c_str());
    if (!cap.isOpened()) {
        printf("Failed to open pipeline immediately: %s\n", pipeline.c_str());
        return {};
    }

    // Try to grab one frame with timeout
    auto start = std::chrono::steady_clock::now();
    cv::Mat frame;
    while (true) {
        printf("Attempting to read frame from pipeline...\n");  
        if (cap.read(frame) && !frame.empty()) {
            printf("Pipeline succeeded: %s\n", pipeline.c_str());
            return cap;  // success
        }

        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed > timeout_ms) {
            printf("Timeout waiting for frames on pipeline: %s\n", pipeline.c_str());
            cap.release();
            return {};  // fail, try next pipeline
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Optional: turn on useful diagnostics from OpenCV’s GStreamer wrapper and GStreamer itself
static void enable_gst_debug() {
    setenv("OPENCV_VIDEOIO_DEBUG", "1", 1);   // OpenCV videoio logs
    setenv("GST_DEBUG", "4", 0);              // raise to 3–4 for more details
}

static bool any_interface_has_ip(std::string &iface_out, std::string &ip_out) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return false;

    bool found = false;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        // Only care about IPv4
        if (ifa->ifa_addr->sa_family == AF_INET) {
            // Skip loopback
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;

            struct sockaddr_in *sa = (struct sockaddr_in*)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) {
                if (strcmp(ip, "0.0.0.0") != 0) {
                    iface_out = ifa->ifa_name;
                    ip_out    = ip;
                    found = true;
                    break;
                }
            }
        }
    }
    freeifaddrs(ifaddr);
    return found;
}

static void wait_for_network_with_validation(int retries = 30, int delay_sec = 2) {
    printf("Waiting for network connectivity...\n");
    
    for (int i = 0; i < retries; i++) {
        std::string iface, ip;
        if (any_interface_has_ip(iface, ip)) {
            printf("Network interface %s has IP %s\n", iface.c_str(), ip.c_str());
            
            // Additional validation: test external connectivity
            printf("Testing external connectivity...\n");
            int ping_result = system("ping -c 1 -W 3 8.8.8.8 >/dev/null 2>&1");
            if (ping_result == 0) {
                printf("External connectivity confirmed\n");
                return;
            } else {
                printf("External connectivity test failed, but local network is up\n");
                // Still return as local network might be sufficient for RTSP
                return;
            }
        }
        printf("Waiting for network (attempt %d/%d)...\n", i+1, retries);
        std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
    }
    printf("ERROR: No network interface got an IP after %d attempts\n", retries);
}

// Resize using RGA (src: cv::Mat, dst: cv::Mat)
bool rga_resize(const cv::Mat& src, cv::Mat& dst, int dst_w, int dst_h) {
    if (dst.empty() || dst.cols != dst_w || dst.rows != dst_h) {
        dst.create(dst_h, dst_w, CV_8UC3);
    }

    // Wrap the source/destination
    rga_buffer_t src_buf = wrapbuffer_virtualaddr(
        src.data, src.cols, src.rows, RK_FORMAT_BGR_888);
    rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
        dst.data, dst.cols, dst.rows, RK_FORMAT_BGR_888);

    // Compute scaling factors
    double fx = (double)dst.cols / src.cols;
    double fy = (double)dst.rows / src.rows;

    int ret = imresize(src_buf, dst_buf, fx, fy, 0, IM_SYNC);
    if (ret != IM_STATUS_SUCCESS) {
        fprintf(stderr, "RGA resize failed: %s\n", imStrError((IM_STATUS)ret));
        return false;
    }
    return true;
}

std::string findWorkingCameraDevice() {
    std::vector<std::string> devices;
    DIR *dir = opendir("/dev");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            std::string name(entry->d_name);
            if (name.find("video") == 0) {
                devices.push_back("/dev/" + name);
            }
        }
        closedir(dir);
    }

    // Sort devices to test in numerical order (video0, video1, etc.)
    std::sort(devices.begin(), devices.end());

    for (const auto& dev : devices) {
        std::cout << "Probing " << dev << " ..." << std::endl;
        
        // Check if device exists and is accessible
        if (access(dev.c_str(), R_OK) != 0) {
            std::cout << "  Device not accessible: " << dev << " (errno: " << errno << ")" << std::endl;
            continue;
        }
        
        cv::VideoCapture cap(dev, cv::CAP_V4L2);
        if (cap.isOpened()) {
            std::cout << "  Device opened successfully, testing frame capture..." << std::endl;
            
            // Try to actually capture a frame to verify the device works
            cv::Mat test_frame;
            bool can_capture = false;
            for (int attempt = 0; attempt < 3; attempt++) {
                if (cap.read(test_frame) && !test_frame.empty()) {
                    can_capture = true;
                    std::cout << "  Frame capture successful (size: " << test_frame.cols << "x" << test_frame.rows << ")" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            cap.release(); // Explicitly release the device
            
            if (can_capture) {
                std::cout << "Found working camera: " << dev << std::endl;
                return dev;
            } else {
                std::cout << "  Device opened but cannot capture frames: " << dev << std::endl;
            }
        } else {
            std::cout << "  Failed to open device: " << dev << std::endl;
        }
    }

    std::cout << "No working /dev/video* devices found!" << std::endl;
    return "";
}

MLInferenceThread::MLInferenceThread(
        const char* model_path,
        const char* input_source,
        ThreadSafeQueue<InferenceResult>& queue, 
        std::atomic<bool>& isRunning,
        int target_fps)
    : resultQueue(queue), running(isRunning), target_fps(target_fps) {

    printf("Initializing ML model from: %s\n", model_path);
    
    // Create and initialize the model with error checking
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_ctx));
    auto ret = init_retinaface_model(model_path, &rknn_app_ctx);
    if (ret != 0) {
        printf("ERROR: Failed to initialize RetinaFace model! ret=%d model_path=%s\n", ret, model_path);
        printf("This will prevent inference from running properly.\n");
        // Don't return here, allow video capture to work even if model fails
    } else {
        printf("RetinaFace model initialized successfully\n");
    }

    printf("Waiting for network before opening RTSP...\n");
    wait_for_network_with_validation();

    // open the capture with appropriate backend based on source type
    printf("Opening capture from source: %s\n", input_source);
    printf("DEBUG: OpenCV version: %s\n", cv::getVersionString().c_str());

    video_source = input_source;
    bool capture_opened = false;

    if (strcmp(input_source, "usb_camera") == 0) {
        video_source = findWorkingCameraDevice();
        if (!video_source.empty()) {
            printf("Detected USB camera device: %s\n", video_source.c_str());
            if (capture.open(video_source, cv::CAP_V4L2)) {
                printf("V4L2 backend opened successfully\n");
                capture_opened = true;
            } else {
                printf("V4L2 backend failed for device: %s\n", video_source.c_str());
            }
        } else {
            printf("No working USB camera found!\n");
        }
    } else if (strstr(input_source, "rtsp://") || strstr(input_source, "rtsps://")) {
        ensure_gstreamer_runtime();
        printf("Detected RTSP URL: %s\n", input_source);
        
        auto pipelines = build_rtsp_pipelines(input_source);
        for (size_t i = 0; i < pipelines.size(); ++i) {
            printf("Trying pipeline %zu/%zu:\n%s\n", i+1, pipelines.size(), pipelines[i].c_str());

            capture = try_open_pipeline(pipelines[i], 3000); // 3s timeout
            if (capture.isOpened()) {
                printf("RTSP stream opened successfully with pipeline %zu\n", i+1);
                capture_opened = true;
                break;
            } else {
                printf("Pipeline %zu failed, trying next...\n", i+1);
            }
        }
    } else {
        printf("Unknown input source type: %s\n", input_source);
    }
    if (!capture_opened) {
        printf("ERROR: Failed to open capture from any backend for source: %s\n", input_source);
        return;
    }
    printf("Capture opened successfully\n");
    printf("done initializing MLInferenceThread\n");
    
}

MLInferenceThread::~MLInferenceThread() {
    auto ret = release_retinaface_model(&rknn_app_ctx);
    if (ret != 0) {
        printf("release_retinaface_model fail! ret=%d\n", ret);
    }  

    running = false;
    resultQueue.signalShutdown();
}

void MLInferenceThread::operator()() {
    int consecutive_failures = 0;
    const int max_consecutive_failures = 10;
    const int retry_delay_ms = 100;
    
    while (running) {
        if (!capture.isOpened()) {
            printf("Capture is not opened, exiting thread\n");
            break;
        }

        auto frame_start_time = std::chrono::steady_clock::now();
        
        cv::Mat captured_img;
        bool frame_read_success = false;
        
        if (capture.grab()) {
            capture.retrieve(captured_img);
            frame_read_success = true;
        } else {
            std::cout << "grab() failed" << std::endl;
        }
        
        if (!frame_read_success) {
            consecutive_failures++;
            // Only attempt RTSP recovery if the source is RTSP
            if ((strstr(video_source.c_str(), "rtsp://") || strstr(video_source.c_str(), "rtsps://")) && consecutive_failures > 5) {
                capture.release();
                auto pipelines = build_rtsp_pipelines(video_source);
                for (size_t i = 0; i < pipelines.size(); ++i) {
                    if (capture.open(pipelines[i], cv::CAP_GSTREAMER)) {
                        break;
                    } else {
                        printf("GStreamer backend failed for pipeline: %s\n", pipelines[i].c_str());
                    }
                }
                if (!capture.isOpened()) {
                    printf("ERROR: Failed to reopen capture after release\n");
                } else {
                    printf("Capture reopened successfully\n");
                }
                consecutive_failures = 0; // Reset after reopening
            }
            // Wait before next attempt
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / target_fps));
            continue;
        }

        // Process the successfully read frame
        cv::Mat resized;
        if (!rga_resize(captured_img, resized, 320, 320)) {
            cv::resize(captured_img, resized, cv::Size(320, 320));
        }

        InferenceResult result = runInference(captured_img);
        resultQueue.push(std::move(result));

        // Save debug image
        try {
            cv::imwrite("/tmp/out.jpg", captured_img);
            std::rename("/tmp/out.jpg", "/tmp/output.jpg");
        } catch (...) {
            printf("Warning: Failed to save debug image\n");
        }

        captured_img.release();

        // FPS limiting with better timing
        auto current_time = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(current_time - frame_start_time);
        auto frame_interval = std::chrono::milliseconds(1000 / target_fps);
        
        if (frame_interval > frame_duration) {
            auto sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_interval - frame_duration);
            std::this_thread::sleep_for(sleep_time);
        }
    }
    
    printf("MLInferenceThread main loop exited\n");
}