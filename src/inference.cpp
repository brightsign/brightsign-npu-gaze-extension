#include <cstdlib>
#include <iostream>
#include <thread>
#include <sys/stat.h>
#include <errno.h>

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

#include <rga/rga.h>
#include <rga/im2d.h>


// Simple test function to check if GStreamer can access RTSP
bool test_gstreamer_rtsp_direct(const char* rtsp_url) {
    printf("DEBUG: Testing direct GStreamer RTSP access (simulated)\n");
    
    // Since we can't use GStreamer command-line tools or direct API,
    // we'll use gst-transcoder-1.0 to test connectivity
    std::string test_cmd = "gst-transcoder-1.0 \"" + std::string(rtsp_url) + 
                          "\" /tmp/rtsp_test_output.null \"video/x-raw\" >/dev/null 2>&1 &";
    printf("DEBUG: Testing with command simulation (would use GStreamer API in production)\n");
    
    // Since the direct API isn't available, we'll return false for now
    // This indicates that we need OpenCV with proper GStreamer support
    return false;
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

    printf("calling inference_retinaface_model\n");
    retinaface_result result;
    int ret = inference_retinaface_model(&rknn_app_ctx, 
        &image, &result);
    if (ret != 0) {
        printf("inference_retinaface_model fail! ret=%d\n", ret);
        return final_result;
    }
    final_result.count_all_faces_in_frame = result.count;
    final_result.num_faces_attending = 0;
    printf("inference_retinaface_model success! count=%d\n", result.count);

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

    printf("Processed frame %d\n", frames);
    return final_result;
}

static inline bool rdok(const char* p) { return p && access(p, R_OK) == 0; }

static void ensure_gstreamer_runtime() {
    // Tell GStreamer where the plugins + scanner live (what you found via SSH)
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
    if (!getenv("GST_DEBUG")) setenv("GST_DEBUG", "3", 0);  // Increased to 3 for more details

    fprintf(stderr, "GST_PLUGIN_PATH=%s\n", getenv("GST_PLUGIN_PATH"));
    fprintf(stderr, "GST_PLUGIN_SCANNER=%s\n", getenv("GST_PLUGIN_SCANNER"));
    fprintf(stderr, "GST_REGISTRY=%s\n", getenv("GST_REGISTRY"));

    if (!rdok(getenv("GST_PLUGIN_PATH")))
        fprintf(stderr, "WARNING: GST_PLUGIN_PATH not readable\n");
    if (!rdok(getenv("GST_PLUGIN_SCANNER")))
        fprintf(stderr, "WARNING: GST_PLUGIN_SCANNER not readable\n");
}

// Build a few RTSP pipelines (Rockchip HW -> V4L2 HW -> software -> generic)
static std::vector<std::string> build_rtsp_pipelines(const std::string& url) {
    std::vector<std::string> p;
    #if 0
    p.push_back(
                "rtspsrc location=" + url + " protocols=tcp latency=150 ! "
                "rtph264depay ! h264parse ! mppvideodec ! videoconvert ! videoscale ! "
                "video/x-raw,width=320,height=320,format=BGR ! "
                "appsink drop=1 max-buffers=1 sync=false"
                );
    
    #endif
    p.push_back("rtspsrc location=" + url + " protocols=tcp latency=150 ! "
                "rtph264depay ! h264parse ! mppvideodec ! videoconvert ! "
                "video/x-raw,format=BGR ! appsink drop=1 max-buffers=1 sync=false");
    return p;
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

static void wait_for_network_generic(int retries = 30, int delay_sec = 2) {
    for (int i = 0; i < retries; i++) {
        std::string iface, ip;
        if (any_interface_has_ip(iface, ip)) {
            printf("Network is up: interface %s has IP %s\n", iface.c_str(), ip.c_str());
            return;
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

MLInferenceThread::MLInferenceThread(
        const char* model_path,
        const char* input_source,
        ThreadSafeQueue<InferenceResult>& queue, 
        std::atomic<bool>& isRunning,
        int target_fps=5)
    : resultQueue(queue), running(isRunning), target_fps(target_fps) {


    // Create and initialize the model
    // rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_ctx));
    auto ret = init_retinaface_model(model_path, &rknn_app_ctx);
       if (ret != 0) {
        printf("init_retinaface_model fail! ret=%d model_path=%s\n", ret, model_path);
        // return -1;
    }

    printf("Waiting for network before opening RTSP...\n");
    wait_for_network_generic();

    // open the capture with appropriate backend based on source type
    printf("Opening capture from source: %s\n", input_source);
    
    // Debug: Show OpenCV version
    printf("DEBUG: OpenCV version: %s\n", cv::getVersionString().c_str());
    
        
    // Check if the device exists before trying to open it
    if (strstr(input_source, "/dev/video") != nullptr) {
        // For device paths, check if the device file exists and is accessible
        FILE* device_check = fopen(input_source, "r");
        if (device_check == nullptr) {
            printf("ERROR: Video device %s does not exist or is not accessible (errno: %d)\n", input_source, errno);
            return;
        }
        fclose(device_check);
        printf("Video device %s exists and is accessible\n", input_source);
        
        // Check device permissions
        struct stat device_stat;
        if (stat(input_source, &device_stat) == 0) {
            printf("Device permissions: %o, owner: %d, group: %d\n", 
                   device_stat.st_mode & 0777, device_stat.st_uid, device_stat.st_gid);
        }
    }
    
    try {
        bool capture_opened = false;
        
        // If it's a simple device path like /dev/video0, try V4L2 first (more reliable for USB cameras)
        if (strstr(input_source, "/dev/video") != nullptr) {
            printf("Detected device path, trying V4L2 backend first...\n");
            if (capture.open(input_source, cv::CAP_V4L2)) {
                printf("V4L2 backend opened successfully\n");
                capture_opened = true;
            } else {
                printf("V4L2 backend failed, trying GStreamer backend...\n");
                if (capture.open(input_source, cv::CAP_GSTREAMER)) {
                    printf("GStreamer backend opened successfully\n");
                    capture_opened = true;
                } else {
                    printf("GStreamer backend failed, trying default backend...\n");
                    if (capture.open(input_source)) {
                        printf("Default backend opened successfully\n");
                        capture_opened = true;
                    } else {
                        printf("All backends failed for device: %s\n", input_source);
                    }
                }
            }
        }
        // If it's an RTSP URL, try multiple approaches
        else if (strstr(input_source, "rtsp://") || strstr(input_source, "rtsps://")) {
            ensure_gstreamer_runtime();

            // Quick network connectivity test
            printf("Testing basic network connectivity to RTSP server...\n");
            std::string host = std::string(input_source);
            size_t start = host.find("://") + 3;
            size_t end = host.find(":", start);
            if (end != std::string::npos) {
                std::string server_ip = host.substr(start, end - start);
                printf("Extracted server IP: %s\n", server_ip.c_str());
                
                std::string ping_cmd = "ping -c 1 " + server_ip + " >/dev/null 2>&1";
                int ping_result = system(ping_cmd.c_str());
                if (ping_result == 0) {
                    printf("Server %s is reachable\n", server_ip.c_str());
                } else {
                    printf("WARNING: Server %s ping failed (may not respond to ping)\n", server_ip.c_str());
                    printf("Network connectivity test failed - continuing anyway\n");
                }
            }

            printf("Detected RTSP URL, opening with GStreamer…\n");
            auto pipes = build_rtsp_pipelines(input_source);

            for (size_t i = 0; i < pipes.size(); ++i) {
                printf("Trying GST pipeline %zu: %s\n", i+1, pipes[i].c_str());
                if (capture.open(pipes[i], cv::CAP_GSTREAMER)) {
                    printf("Opened RTSP with pipeline %zu\n", i+1);
                    capture_opened = true;
                    break;
                } else {
                    printf("Pipeline %zu failed\n", i+1);
                    capture_opened = false;
                }
            }

            if (!capture_opened) {
                printf("All RTSP complex pipelines failed. Trying simple RTSP URL...\n");
                fflush(stdout);
                
                // Try the simple RTSP URL directly - all plugins are confirmed available
                printf("Testing simple RTSP URL: %s\n", input_source);
                if (capture.open(input_source, cv::CAP_GSTREAMER)) {
                    printf("Simple RTSP URL opened successfully with GStreamer!\n");
                    capture_opened = true;
                } else {
                    printf("Simple RTSP URL failed with GStreamer, trying default backend...\n");
                    if (capture.open(input_source)) {
                        printf("Simple RTSP URL opened with default backend!\n");
                        capture_opened = true;
                    } else {
                        printf("RTSP connection failed - all methods exhausted\n");
                        
                        // Check if this might be a network issue
                        printf("Debugging: Checking if OpenCV was built with GStreamer support...\n");
                        cv::VideoCapture test_cap;
                        cv::String version = cv::getBuildInformation();
                        if (version.find("GStreamer") != std::string::npos) {
                            printf("OpenCV was built WITH GStreamer support\n");
                        } else {
                            printf("OpenCV was built WITHOUT GStreamer support\n");
                        }
                    }
                }
            }
        }
        // For other cases, try GStreamer first, then default
        else {
            printf("Trying GStreamer backend first...\n");
            if (capture.open(input_source, cv::CAP_GSTREAMER)) {
                printf("GStreamer backend opened successfully\n");
                capture_opened = true;
            } else {
                printf("GStreamer backend failed, trying default backend...\n");
                if (capture.open(input_source)) {
                    printf("Default backend opened successfully\n");
                    capture_opened = true;
                } else {
                    printf("All backends failed for: %s\n", input_source);
                }
            }
        }
        
        if (!capture_opened) {
            printf("ERROR: Failed to open capture from any backend for source: %s\n", input_source);
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        printf("Failed to open capture due to exception: %s\n", e.what());
        return;
    } catch (...) {
        std::cerr << "Unknown exception caught!" << std::endl;
        printf("Failed to open capture due to unknown exception!\n");
        return;
    }

    if (!capture.isOpened()) {
        printf("Failed to open capture\n");
        return;
    }

    printf("Capture opened successfully\n");
    //capture.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    //capture.set(cv::CAP_PROP_FRAME_HEIGHT, 320);

    if (!capture.isOpened()) {
        printf("Failed to open capture\n");
        // return -1;
    }

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
    while (running) {
        if (!capture.isOpened()) {
            printf("Capture is not opened\n");
            break;
        }

        auto frame_start_time = std::chrono::steady_clock::now();
        
        printf("Reading frame from capture\n");
        cv::Mat captured_img;

        using clk = std::chrono::steady_clock;
        using usec = std::chrono::microseconds;

        auto t_resize_start = clk::now();
        auto t_resize_end   = t_resize_start;

        try {
            if (!capture.read(captured_img)) {
                printf("Failed to read frame from capture\n");
                break;
            }
            printf("Frame read from capture %d x %d\n", captured_img.size().width, captured_img.size().height);

            // Ensure the captured frame is not empty
            if (captured_img.empty()) {
                printf("Captured frame is empty\n");
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1000 / target_fps));
                continue;
            }
        } catch (const cv::Exception& e) {
            std::cerr << "OpenCV exception caught: " << e.what() << std::endl;
            printf("Failed to read frame due to OpenCV exception: %s\n", e.what());
            break;
        } catch (const std::exception& e) {
            std::cerr << "Standard exception caught: " << e.what() << std::endl;
            printf("Failed to read frame due to standard exception: %s\n", e.what());
            break;
        } catch (...) {
            std::cerr << "Unknown exception caught!" << std::endl;
            printf("Failed to read frame due to unknown exception!\n");
            break;
        }

        cv::Mat resized;
        if (!rga_resize(captured_img, resized, 320, 320)) {
            cv::resize(captured_img, resized, cv::Size(320, 320));
        }
        t_resize_end = clk::now();

        printf("Running inference on frame\n");
        InferenceResult result = runInference(captured_img);
        resultQueue.push(std::move(result));

        // release opencv image
        cv::imwrite("/tmp/out.jpg", captured_img);
        captured_img.release();
        // rename the file
        std::rename("/tmp/out.jpg", "/tmp/output.jpg");

        auto current_time = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>
            (current_time - frame_start_time);
        // FPS limiting
        auto frame_interval = std::chrono::milliseconds(1000 / target_fps);
        
        if (frame_interval > frame_duration) {
            auto sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>
                (frame_interval - frame_duration);
            std::this_thread::sleep_for(sleep_time);
        }
    }
}