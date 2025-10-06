#ifndef INFERENCE_H
#define INFERENCE_H

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include "queue.h"
#include "retinaface.h"

class RtspNv12Source;
class LatestFrame;  // forward decl is OK if we hold a pointer

// Struct to hold ML inference results
struct InferenceResult {
    int count_all_faces_in_frame;
    int num_faces_attending;
    std::chrono::system_clock::time_point timestamp;
};

class MLInferenceThread {
private:
    ThreadSafeQueue<InferenceResult>& resultQueue;
    std::atomic<bool>& running;
    int target_fps;
    rknn_app_context_t rknn_app_ctx;
    cv::VideoCapture capture;
    std::string video_source;
    int frames{0};
    std::unique_ptr<RtspNv12Source> rtsp;
    bool using_rtsp = false;

    // Use a pointer for the incomplete type
    std::unique_ptr<LatestFrame> latest;
    cv::Mat prod_rgb;
    std::atomic<long long> capture_convert_ns{0};
    std::atomic<long long> infer_ns{0};
    // Used to signal producer/consumer to quit when source is gone
    std::atomic<bool> source_broken{false};
    // Store the opened USB device path so we can detect unplug
    std::string usb_dev_path;
    void producerLoop();

    InferenceResult runInference(cv::Mat& img);
    bool acquireSourceLoop(int backoff_ms = 500, int backoff_max_ms = 5000);

public:
    MLInferenceThread(
        const char* model_path,
        const char* input_source,
        ThreadSafeQueue<InferenceResult>& queue, 
        std::atomic<bool>& isRunning,
        int target_fps);
    ~MLInferenceThread();
    void operator()();
};

#endif // INFERENCE_H
