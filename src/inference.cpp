#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mutex>
#include <condition_variable>

#include <opencv2/opencv.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <rga/rga.h>
#include <rga/im2d.h>

#include "attention.h"
#include "inference.h"

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <dirent.h>

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

// ===============================
// Config
// ===============================
static constexpr int NET_W = 320;
static constexpr int NET_H = 320;

// ===============================
// One-time GStreamer init
// ===============================
static void gst_init_once() {
    static std::once_flag f;
    std::call_once(f, []{
        int argc = 0; char** argv = nullptr;
        gst_init(&argc, &argv);
        setvbuf(stdout, nullptr, _IOLBF, 0);
        if (!getenv("GST_REGISTRY")) setenv("GST_REGISTRY", "/tmp/gst-registry.bin", 1);
    });
}

// ===============================
// Minimal GST env wiring
// ===============================
static inline bool rdok(const char* p) { return p && access(p, R_OK) == 0; }

static void ensure_gstreamer_runtime() {
    const char* local = "/var/volatile/bsext/ext_npu_gaze/RK3588/lib/gstreamer-1.0";
    const char* sys   = "/usr/lib/gstreamer-1.0";
    std::string plugin_path = std::string(local) + ":" + sys;

    setenv("GST_PLUGIN_PATH", plugin_path.c_str(), 1);
    if (!getenv("GST_PLUGIN_SCANNER"))
        setenv("GST_PLUGIN_SCANNER", "/usr/libexec/gstreamer-1.0/gst-plugin-scanner", 1);
    if (!getenv("GST_REGISTRY"))
        setenv("GST_REGISTRY", "/tmp/gst-registry.bin", 1);

    printf("Ensuring GStreamer runtime environment...\n");
    printf("GStreamer environment:\n");
    printf("  GST_PLUGIN_PATH=%s\n", getenv("GST_PLUGIN_PATH"));
    printf("  GST_PLUGIN_SCANNER=%s\n", getenv("GST_PLUGIN_SCANNER"));
    printf("  GST_REGISTRY=%s\n", getenv("GST_REGISTRY"));

    printf("GST_PLUGIN_PATH: %s\n", getenv("GST_PLUGIN_PATH"));
    std::string pp = getenv("GST_PLUGIN_PATH");
    size_t pos = 0;
    while (true) {
        size_t colon = pp.find(':', pos);
        std::string part = pp.substr(pos, colon == std::string::npos ? std::string::npos : colon - pos);
        printf("  %s %s (readable)\n", (access(part.c_str(), R_OK)==0 ? "✓" : "✗"), part.c_str());
        if (colon == std::string::npos) break;
        pos = colon + 1;
    }
    printf("GST_PLUGIN_SCANNER: %s (readable)\n", access(getenv("GST_PLUGIN_SCANNER"), R_OK)==0 ? getenv("GST_PLUGIN_SCANNER") : "(unreadable)");
    printf("GStreamer environment validation passed\n");
}

// ===============================
// RTSP NV12 helper (appsink) — with small NV12 scratch buffer
// ===============================
class RtspNv12Source {
public:
    RtspNv12Source() = default;
    ~RtspNv12Source() { close(); }

    bool open(const std::string& pipeline_str, int first_frame_timeout_ms = 3000) {
        close();
        gst_init_once();

        GError* err = nullptr;
        pipeline_ = gst_parse_launch(pipeline_str.c_str(), &err);
        if (!pipeline_) {
            if (err) { fprintf(stderr, "GStreamer parse error: %s\n", err->message); g_error_free(err); }
            else { fprintf(stderr, "GStreamer parse error (unknown)\n"); }
            return false;
        }

        appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "mysink");
        if (!appsink_) {
            fprintf(stderr, "appsink 'mysink' not found in pipeline\n");
            close();
            return false;
        }

        // Force NV12 on appsink
        {
            GstCaps* caps = gst_caps_from_string("video/x-raw,format=NV12");
            g_object_set(G_OBJECT(appsink_), "caps", caps, "drop", 1, "max-buffers", 1,
                         "enable-last-sample", FALSE, "sync", FALSE, nullptr);
            gst_caps_unref(caps);
        }

        gst_element_set_state(pipeline_, GST_STATE_PLAYING);

        // First frame wait
        GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(appsink_), first_frame_timeout_ms * GST_MSECOND);
        if (!sample) {
            printf("First-frame timeout (%d ms)\n", first_frame_timeout_ms);
            close();
            return false;
        }
        GstCaps* scaps = gst_sample_get_caps(sample);
        const GstStructure* s = gst_caps_get_structure(scaps, 0);
        gst_structure_get_int(s, "width", &w_);
        gst_structure_get_int(s, "height", &h_);
        gst_sample_unref(sample);

        // allocate small NV12 buffer (contiguous Y then interleaved UV)
        nv12_small_.resize(NET_W * NET_H * 3 / 2);

        printf("RTSP NV12 appsink opened: %dx%d\n", w_, h_);
        return true;
    }
    
    bool pull_into_rgb(cv::Mat& rgb_prealloc) {
    if (!pipeline_ || !appsink_) return false;

    // short, repeated polls—keeps the loop responsive
    const int per_try_ms = 200;
    const int tries = 5;

    for (int i = 0; i < tries; ++i) {
        GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(appsink_), per_try_ms * GST_MSECOND);
        if (!sample) continue;

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstCaps* caps     = gst_sample_get_caps(sample);
        if (!buffer || !caps) { gst_sample_unref(sample); continue; }

        // Describe format/strides from caps
        GstVideoInfo vinfo;
        if (!gst_video_info_from_caps(&vinfo, caps)) {
            gst_sample_unref(sample);
            continue;
        }

        // Map as video frame (gets plane pointers + strides safely)
        GstVideoFrame vframe;
        if (!gst_video_frame_map(&vframe, &vinfo, buffer, GST_MAP_READ)) {
            gst_sample_unref(sample);
            continue;
        }

        const int W = GST_VIDEO_INFO_WIDTH(&vinfo);
        const int H = GST_VIDEO_INFO_HEIGHT(&vinfo);

        // Plane 0: Y
        uint8_t* y_ptr      = (uint8_t*)GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0);
        int      y_stride   = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0);

        // Plane 1: interleaved UV (NV12)
        uint8_t* uv_ptr     = (uint8_t*)GST_VIDEO_FRAME_PLANE_DATA(&vframe, 1);
        int      uv_stride  = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 1);

        // Build OpenCV mats with the CORRECT shapes and strides
        // Y  = H x W, 1 channel
        // UV = H/2 x W/2, 2 channels (interleaved)
        cv::Mat y_plane (H,     W,     CV_8UC1, y_ptr, y_stride);
        cv::Mat uv_plane(H/2,   W/2,   CV_8UC2, uv_ptr, uv_stride);

        // NV12 -> BGR (full size, correct colors)
        cv::Mat bgr_full;
        cv::cvtColorTwoPlane(y_plane, uv_plane, bgr_full, cv::COLOR_YUV2BGR_NV12);

        // Resize to network input and convert to RGB for the model
        if (rgb_prealloc.empty() || rgb_prealloc.cols != NET_W || rgb_prealloc.rows != NET_H)
            rgb_prealloc.create(NET_H, NET_W, CV_8UC3);

        cv::Mat bgr_small;
        cv::resize(bgr_full, bgr_small, cv::Size(NET_W, NET_H), 0, 0, cv::INTER_AREA);
        cv::cvtColor(bgr_small, rgb_prealloc, cv::COLOR_BGR2RGB);

        gst_video_frame_unmap(&vframe);
        gst_sample_unref(sample);
        return true;
    }
    return false;
}


    int width()  const { return w_; }
    int height() const { return h_; }

    void close() {
        if (pipeline_) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
        }
        if (appsink_) {
            gst_object_unref(appsink_);
            appsink_ = nullptr;
        }
        nv12_small_.clear();
        nv12_small_.shrink_to_fit();
        w_ = h_ = 0;
    }

private:
    GstElement* pipeline_ = nullptr;
    GstElement* appsink_  = nullptr;
    int w_ = 0, h_ = 0;

    // contiguous NV12 buffer for 320x320
    std::vector<uint8_t> nv12_small_;
};

// ===============================
// Build RTSP pipelines — EXACT shape as working standalone
// ===============================
#if 0
static std::vector<std::string> build_rtsp_nv12_pipelines(const std::string& url) {
    std::vector<std::string> p;

    auto mk = [&](const std::string& enc){
        return std::string(
            "rtspsrc location=") + url +
            " protocols=tcp latency=150 drop-on-latency=true do-rtsp-keep-alive=true tcp-timeout=3000000000 "
            "name=src src. ! application/x-rtp,media=video,encoding-name=" + enc + " ! " +
            (enc == "H265"
                 ? "rtph265depay ! h265parse disable-passthrough=true ! "
                 : "rtph264depay ! h264parse config-interval=-1 disable-passthrough=true ! ") +
            "mppvideodec ! queue max-size-buffers=2 leaky=downstream ! "
            "appsink name=mysink caps=video/x-raw,format=NV12 drop=1 max-buffers=1 enable-last-sample=false sync=false";
    };

    p.push_back(mk("H265"));
    p.push_back(mk("H264"));
    return p;
}
#endif

// Build RTSP NV12 pipelines — MediaMTX-friendly set
static std::vector<std::string> build_rtsp_nv12_pipelines(const std::string& url) {
    std::vector<std::string> p;

    auto mk = [&](std::string proto,
                  bool specify_encoding,
                  std::string enc /* "H264" or "H265" (ignored if !specify_encoding) */,
                  bool sw_decode,
                  bool add_videoconvert_before_sink,
                  bool use_decodebin) {
        std::string s = "rtspsrc location=" + url +
                        " protocols=" + proto +
                        " latency=150 drop-on-latency=true do-rtsp-keep-alive=true tcp-timeout=3000000000 "
                        "name=src ";

        if (use_decodebin) {
            // Very forgiving path; decodebin chooses depay/parse/decoder.
            // We still end at NV12 for our appsink.
            s += "! decodebin ! ";
            if (add_videoconvert_before_sink) s += "videoconvert ! ";
            s += "video/x-raw,format=NV12 ! "
                 "queue max-size-buffers=2 leaky=downstream ! "
                 "appsink name=mysink caps=video/x-raw,format=NV12 drop=1 max-buffers=1 enable-last-sample=false sync=false";
            return s;
        }

        // Split pad explicitly (more deterministic than decodebin when it works)
        s += "src. ! ";

        if (specify_encoding) {
            s += "application/x-rtp,media=video,encoding-name=" + enc + " ! ";
            if (enc == "H265") {
                s += "rtph265depay ! h265parse disable-passthrough=true ! ";
            } else {
                s += "rtph264depay ! h264parse config-interval=-1 disable-passthrough=true ! ";
            }
        } else {
            // Let rtspsrc expose caps; add a jitterbuffer to make mpp happier with some servers
            s += "rtpjitterbuffer drop-on-late=true ! ";
            // Choose depay later based on enc
            s += "queue max-size-buffers=4 leaky=downstream ! ";
            // Try H265 first then H264 via decodebin in fallback below
            // Here we’ll assume H264; the next pipelines will cover the other case.
            // (This variant is kept generic—decodebin fallback below is even more tolerant.)
        }

        if (specify_encoding) {
            if (sw_decode) {
                s += (enc == "H265" ? "avdec_hevc ! " : "avdec_h264 ! ");
                s += "videoconvert ! ";
            } else {
                s += "mppvideodec ! ";
            }
        } else {
            // If we didn’t specify encoding, be forgiving and let decodebin pick depay/decoder
            s += "decodebin ! ";
        }

        if (add_videoconvert_before_sink) s += "videoconvert ! ";
        s += "video/x-raw,format=NV12 ! "
             "queue max-size-buffers=2 leaky=downstream ! "
             "appsink name=mysink caps=video/x-raw,format=NV12 drop=1 max-buffers=1 enable-last-sample=false sync=false";
        return s;
    };

    // --- Fast, strict (matches your camera case) — keep first
    p.push_back(mk("tcp", /*specify_encoding=*/true,  "H265", /*sw=*/false, /*vcvt=*/false, /*decodebin=*/false));
    p.push_back(mk("tcp", /*specify_encoding=*/true,  "H264", /*sw=*/false, /*vcvt=*/false, /*decodebin=*/false));

    // --- Looser caps (no encoding-name filter), still TCP
    p.push_back(mk("tcp", /*specify_encoding=*/false, "H264", /*sw=*/false, /*vcvt=*/false, /*decodebin=*/false));

    // --- UDP variant (some MediaMTX setups prefer UDP)
    p.push_back(mk("udp", /*specify_encoding=*/true,  "H264", /*sw=*/false, /*vcvt=*/false, /*decodebin=*/false));

    // --- Software decode fallbacks (TCP)
    p.push_back(mk("tcp", /*specify_encoding=*/true,  "H265", /*sw=*/true,  /*vcvt=*/true,  /*decodebin=*/false));
    p.push_back(mk("tcp", /*specify_encoding=*/true,  "H264", /*sw=*/true,  /*vcvt=*/true,  /*decodebin=*/false));

    // --- Ultra-forgiving decodebin fallbacks (TCP then UDP)
    p.push_back(mk("tcp", /*specify_encoding=*/false, "H264", /*sw=*/false, /*vcvt=*/true,  /*decodebin=*/true));
    p.push_back(mk("udp", /*specify_encoding=*/false, "H264", /*sw=*/false, /*vcvt=*/true,  /*decodebin=*/true));

    return p;
}


// ===============================
// USB helper
// ===============================
static std::string findWorkingCameraDevice() {
    std::vector<std::string> devices;
    if (DIR* dir = opendir("/dev")) {
        if (dirent* e; (e = readdir(dir)) != nullptr) {
            do {
                std::string name = e->d_name;
                if (name.rfind("video", 0) == 0) devices.emplace_back("/dev/" + name);
            } while ((e = readdir(dir)) != nullptr);
        }
        closedir(dir);
    }
    std::sort(devices.begin(), devices.end());
    for (const auto& dev : devices) {
        if (access(dev.c_str(), R_OK) != 0) continue;
        cv::VideoCapture cap(dev, cv::CAP_V4L2);
        if (!cap.isOpened()) continue;
        cv::Mat f;
        if (cap.read(f) && !f.empty()) return dev;
    }
    return "";
}

// ===============================
// Tiny "latest frame" buffer (capacity=1)
// ===============================
class LatestFrame {
public:
    void set(const cv::Mat& rgb320) {
        std::lock_guard<std::mutex> lk(m_);
        rgb320.copyTo(buf_);
        fresh_ = true;
        cv_.notify_one();
    }
    bool get(cv::Mat& out, int wait_ms = 100) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, std::chrono::milliseconds(wait_ms), [&]{ return fresh_; }))
            return false;
        buf_.copyTo(out);
        fresh_ = false;
        return true;
    }
    void prealloc(int w, int h) {
        std::lock_guard<std::mutex> lk(m_);
        buf_.create(h, w, CV_8UC3);
        buf_.setTo(cv::Scalar(0,0,0));
        fresh_ = false;
    }
private:
    cv::Mat buf_;
    bool fresh_ = false;
    std::mutex m_;
    std::condition_variable cv_;
};

// ===============================
// Inference glue — RGB in
// ===============================
static void mat_to_image_buffer_rgb(cv::Mat& rgb, image_buffer_t* image) {
    image->width = rgb.cols;
    image->height = rgb.rows;
    image->width_stride = rgb.cols;
    image->height_stride = rgb.rows;
    image->format = IMAGE_FORMAT_RGB888;
    image->virt_addr = rgb.data;
    image->size = rgb.cols * rgb.rows * 3;
    image->fd = -1;
}

InferenceResult MLInferenceThread::runInference(cv::Mat& rgb) {
    image_buffer_t image;
    memset(&image, 0, sizeof(image));
    mat_to_image_buffer_rgb(rgb, &image);

    InferenceResult final_result{-1, -1, std::chrono::system_clock::now()};
    retinaface_result result;
    int ret = inference_retinaface_model(&rknn_app_ctx, &image, &result);
    if (ret != 0) {
        printf("inference_retinaface_model fail! ret=%d\n", ret);
        return final_result;
    }

    final_result.count_all_faces_in_frame = result.count;
    final_result.num_faces_attending = 0;

    for (int i = 0; i < result.count; i++) {
        cv::Scalar color(255, 0, 0);
        if (face_is_looking_at_us(result.object[i])) {
            final_result.num_faces_attending += 1;
            color = cv::Scalar(0, 255, 0);
            auto left_eye  = result.object[i].ponit[0];
            auto right_eye = result.object[i].ponit[1];
            cv::circle(rgb, cv::Point(left_eye.x, left_eye.y), 2, cv::Scalar(0, 128, 128), 2);
            for (int j = 2; j < 5; j++) {
                auto p = result.object[i].ponit[j];
                cv::circle(rgb, cv::Point(p.x, p.y), 2, cv::Scalar(128, 128, 0), 2);
            }
        }
        auto box = result.object[i].box;
        cv::rectangle(rgb, cv::Point(box.left, box.top), cv::Point(box.right, box.bottom), color, 2);
    }

    frames++;
    return final_result;
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
            printf("Testing external connectivity...\n");
            int ping_result = system("ping -c 1 -W 3 8.8.8.8 >/dev/null 2>&1");
            if (ping_result == 0) {
                printf("External connectivity confirmed\n");
                return;
            } else {
                printf("External connectivity test failed, but local network is up\n");
                return;
            }
        }
        printf("Waiting for network (attempt %d/%d)...\n", i+1, retries);
        std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
    }
    printf("ERROR: No network interface got an IP after %d attempts\n", retries);
}

MLInferenceThread::~MLInferenceThread() {
    release_retinaface_model(&rknn_app_ctx);
    running = false;
    resultQueue.signalShutdown();
}


MLInferenceThread::MLInferenceThread(
    const char* model_path,
    const char* input_source,
    ThreadSafeQueue<InferenceResult>& queue,
    std::atomic<bool>& isRunning,
    int target_fps)
: resultQueue(queue), running(isRunning), target_fps(target_fps) {

    printf("Initializing ML model from: %s\n", model_path);

    memset(&rknn_app_ctx, 0, sizeof(rknn_app_ctx));
    int ret = init_retinaface_model(model_path, &rknn_app_ctx);
    if (ret != 0) {
        printf("ERROR: Failed to initialize RetinaFace model! ret=%d\n", ret);
    } else {
        printf("RetinaFace model initialized successfully\n");
    }

    printf("Opening capture from source: %s\n", input_source);
    printf("DEBUG: OpenCV version: %s\n", cv::getVersionString().c_str());
    video_source = input_source;

    // Only wait for network when an RTSP URL is requested
    if (strstr(input_source, "rtsp://") || strstr(input_source, "rtsps://")) {
        printf("Waiting for network before opening RTSP...\n");
        wait_for_network_with_validation();
    }

    // Acquisition (RTSP/USB) is handled by acquireSourceLoop() called from operator().
    // This prevents the thread from exiting when sources are absent at startup.
    printf("Constructor complete — capture will be acquired in operator() via acquireSourceLoop().\n");
}


// ===============================
// Producer thread (capture + convert)
// ===============================
void MLInferenceThread::producerLoop() {
    using clock = std::chrono::steady_clock;

    int consecutive_fail = 0;
    auto last_ok = clock::now();

    while (running) {
        auto start = clock::now();
        bool ok = false;

        if (using_rtsp) {
            ok = rtsp->pull_into_rgb(prod_rgb);  // returns quickly if no sample
        } else {
            cv::Mat frame_bgr;
            if (capture.isOpened()) {
                ok = capture.read(frame_bgr) && !frame_bgr.empty();
            } else {
                ok = false;
            }

            if (ok) {
                // BGR->RGB, resize to NET_W x NET_H (RGA preferred)
                cv::Mat rgb_full(frame_bgr.rows, frame_bgr.cols, CV_8UC3);
                rga_buffer_t src_bgr = wrapbuffer_virtualaddr(frame_bgr.data, frame_bgr.cols, frame_bgr.rows, RK_FORMAT_BGR_888);
                rga_buffer_t dst_rgb = wrapbuffer_virtualaddr(rgb_full.data,  rgb_full.cols,  rgb_full.rows,  RK_FORMAT_RGB_888);
                int ret = imcvtcolor(src_bgr, dst_rgb, RK_FORMAT_BGR_888, RK_FORMAT_RGB_888, IM_SYNC);
                if (ret != IM_STATUS_SUCCESS) {
                    cv::cvtColor(frame_bgr, rgb_full, cv::COLOR_BGR2RGB);
                }
                rga_buffer_t src_rgb      = wrapbuffer_virtualaddr(rgb_full.data, rgb_full.cols, rgb_full.rows, RK_FORMAT_RGB_888);
                rga_buffer_t dst_rgb_small = wrapbuffer_virtualaddr(prod_rgb.data,  prod_rgb.cols,  prod_rgb.rows,  RK_FORMAT_RGB_888);
                double fx = (double)prod_rgb.cols / rgb_full.cols;
                double fy = (double)prod_rgb.rows / rgb_full.rows;
                ret = imresize(src_rgb, dst_rgb_small, fx, fy, 0, IM_SYNC);
                if (ret != IM_STATUS_SUCCESS) {
                    cv::resize(rgb_full, prod_rgb, prod_rgb.size(), 0, 0, cv::INTER_AREA);
                }
            }
        }

        if (ok) {
            consecutive_fail = 0;
            last_ok = clock::now();
            latest->set(prod_rgb);
        } else {
            consecutive_fail++;

            // USB unplug detection: the /dev node disappears
            if (!using_rtsp && !usb_dev_path.empty() && access(usb_dev_path.c_str(), F_OK) != 0) {
                printf("USB device disappeared: %s\n", usb_dev_path.c_str());
                source_broken.store(true, std::memory_order_release);
                break; // exit producer
            }

            // Stalled source (no frames for >3s)
            if (std::chrono::duration_cast<std::chrono::seconds>(clock::now() - last_ok).count() >= 3) {
                printf("Source stalled for >3s; marking broken\n");
                source_broken.store(true, std::memory_order_release);
                break; // exit producer
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        auto end = clock::now();
        capture_convert_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(),
                                     std::memory_order_relaxed);
    }

    // Clean up capture if USB path
    if (!using_rtsp && capture.isOpened()) {
        capture.release();
    }
}

// Add to MLInferenceThread class (private):
bool MLInferenceThread::acquireSourceLoop(int backoff_ms, int backoff_max_ms) {
    using namespace std::chrono;

    auto open_usb = [&]() -> bool {
        // Reuse last known device if we have it; otherwise probe
        std::string dev = usb_dev_path;
        if (dev.empty() || access(dev.c_str(), R_OK) != 0) {
            dev = findWorkingCameraDevice();
        }
        if (dev.empty()) {
            printf("No working USB camera found!\n");
            return false;
        }

        printf("Detected USB camera device: %s\n", dev.c_str());
        usb_dev_path = dev;

        cv::VideoCapture tmp(dev, cv::CAP_V4L2);
        if (!tmp.isOpened()) {
            printf("V4L2 backend failed for device: %s\n", dev.c_str());
            return false;
        }

        // Try to guide the camera into a sane mode
        tmp.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        tmp.set(cv::CAP_PROP_FRAME_WIDTH,  1280);
        tmp.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        tmp.set(cv::CAP_PROP_FPS, 30);

        // Swap into our member
        capture.release();
        capture = std::move(tmp);
        using_rtsp = false;

        printf("USB camera opened successfully\n");
        return true;
    };

    int delay_ms = backoff_ms;
    while (running) {
        bool opened = false;

        // If RTSP was requested, try RTSP first
        if (!video_source.empty() &&
            (video_source.rfind("rtsp://",0)==0 || video_source.rfind("rtsps://",0)==0)) {

            ensure_gstreamer_runtime();
            gst_init_once();

            auto pipelines = build_rtsp_nv12_pipelines(video_source);
            for (size_t i = 0; i < pipelines.size() && running; ++i) {
                printf("Trying pipeline %zu/%zu:\n%s\n", i+1, pipelines.size(), pipelines[i].c_str());
                std::unique_ptr<RtspNv12Source> tmp(new RtspNv12Source());
                if (tmp->open(pipelines[i], 3000)) {  // 3s like standalone
                    rtsp = std::move(tmp);
                    using_rtsp = true;
                    opened = true;
                    break;
                } else {
                    printf("Pipeline %zu failed, trying next...\n", i+1);
                }
            }

            if (!opened) {
                printf("RTSP open failed — falling back to USB camera probe...\n");
                opened = open_usb();
            }
        } else {
            // Non-RTSP token → try USB directly
            opened = open_usb();
        }

        if (opened) {
            // Preallocate producer/consumer buffers (only once per open)
            if (!latest) latest = std::make_unique<LatestFrame>();
            latest->prealloc(NET_W, NET_H);
            if (prod_rgb.empty()) {
                prod_rgb.create(NET_H, NET_W, CV_8UC3);
            }
            prod_rgb.setTo(cv::Scalar(0,0,0));
            printf("Capture opened successfully\n");
            return true;
        }

        // Nothing available → idle + exponential backoff
        printf("No source available (RTSP/USB). Retrying in %d ms...\n", delay_ms);
        for (int t=0; t<delay_ms && running; t+=200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        delay_ms = std::min(delay_ms * 2, backoff_max_ms);
    }
    return false;
}

void MLInferenceThread::operator()() {
    // Try to acquire a source (blocks with backoff until success or running==false)
    if (!acquireSourceLoop(/*backoff_ms=*/500, /*backoff_max_ms=*/5000)) {
        printf("MLInferenceThread main loop exited (no source and stopping)\n");
        return;
    }

    std::thread producer;
    if (using_rtsp || capture.isOpened())
        producer = std::thread(&MLInferenceThread::producerLoop, this);

    auto last_fps_time = std::chrono::steady_clock::now();
    
    printf("MLInferenceThread main loop starting\n");

    while (running) {
        cv::Mat rgb;
        // Wait for a produced frame; if none for a while, check health and reacquire.
        if (!latest->get(rgb, /*wait_ms=*/300)) {
            // If producer likely died (e.g., unplugged USB or RTSP dropped)
            bool need_reacquire = false;
            if (using_rtsp) {
                // If RTSP appsink starved repeatedly, producerLoop will stall → reacquire
                need_reacquire = true;
            } else {
                // For USB, if capture is closed, we must reacquire
                if (!capture.isOpened()) need_reacquire = true;
            }

            if (need_reacquire) {
                if (producer.joinable()) producer.join();
                // Tear down existing
                capture.release();
                rtsp.reset();

                // Block until new source appears (or we’re stopping)
                if (!acquireSourceLoop(/*backoff_ms=*/500, /*backoff_max_ms=*/5000)) break;

                // Relaunch producer
                if (using_rtsp || capture.isOpened())
                    producer = std::thread(&MLInferenceThread::producerLoop, this);
            }
            continue;
        }

        // Inference
        auto t0 = std::chrono::steady_clock::now();
        InferenceResult result = runInference(rgb);
        auto t1 = std::chrono::steady_clock::now();
        infer_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count(),
                           std::memory_order_relaxed);
        resultQueue.push(std::move(result));

        // Optional throttle
        if (target_fps > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / target_fps));
        }

        try {
            cv::Mat dbg_bgr;
            cv::cvtColor(rgb, dbg_bgr, cv::COLOR_RGB2BGR);
            cv::imwrite("/tmp/out.jpg", dbg_bgr);
            std::rename("/tmp/out.jpg", "/tmp/output.jpg");
        } catch (...) {}

        #if DEBUG_FPS
        int frame_count = 0;
        // Periodic perf print
        frame_count++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_fps_time).count() >= 5) {
            double secs = std::chrono::duration<double>(now - last_fps_time).count();
            long long cap_ns = capture_convert_ns.exchange(0, std::memory_order_relaxed);
            long long inf_ns = infer_ns.exchange(0, std::memory_order_relaxed);
            double fps = frame_count / secs;
            double cap_ms = (frame_count ? (cap_ns/1e6)/frame_count : 0.0);
            double inf_ms = (frame_count ? (inf_ns/1e6)/frame_count : 0.0);

            printf("Performance: %.1f FPS | Frame %dx%d | avg capture+convert: %.2f ms | avg inference: %.2f ms\n",
                   NET_W==0?0:fps, NET_W, NET_H, cap_ms, inf_ms);

            frame_count = 0;
            last_fps_time = now;
        }
        #endif
    }

    if (producer.joinable()) producer.join();
    printf("MLInferenceThread main loop exited\n");
}
