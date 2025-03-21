#ifndef INFERENCE_H
#define INFERENCE_H

#include <atomic>
#include <chrono>
#include <string>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include "queue.h"
#include "retinaface.h"

// Struct to hold ML inference results
struct InferenceResult {
    // float confidence;
    // std::string label;
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
    int frames{0};
    
    // Simulated ML model inference
    InferenceResult runInference(cv::Mat& img);

public:
    MLInferenceThread(
        const char* model_path,
        const char* source_name,
        ThreadSafeQueue<InferenceResult>& queue, 
        std::atomic<bool>& isRunning,
        int target_fps);
    ~MLInferenceThread(); // Destructor declaration
    void operator()();
};

#endif // INFERENCE_H