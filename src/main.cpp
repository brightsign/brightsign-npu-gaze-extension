#include <chrono>
#include <memory>
#include <sys/time.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <string>
#include <thread>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include "attention.h"
#include "image_utils.h"
#include "inference.h"
#include "publisher.h"
#include "queue.h"
#include "retinaface.h"
#include "utils.h"


std::atomic<bool> running{true};
ThreadSafeQueue<InferenceResult> resultQueue(1);

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";

    // Cleanup and shutdown
    running = false;
    resultQueue.signalShutdown();
}

int main(int argc, char **argv) {
    freopen("/storage/sd/console.log", "a", stdout);
    std::cout<<"DEBUG: OpenCV build information:\n";
    std::cout << cv::getBuildInformation() << std::endl; 
    char *model_name = NULL;
    if (argc != 3) {
        printf("Usage: %s <rknn model> <input_source> \n", argv[0]);
        printf("\nInput source examples:\n");
        printf("  USB Camera (device):     /dev/video0\n");
        printf("  USB Camera (GStreamer):  \"v4l2src device=/dev/video1 ! videoconvert ! video/x-raw,format=BGR ! appsink\"\n");
        printf("  RTSP Stream:             rtsp://192.168.1.100:554/stream\n");
        printf("  RTSP with GStreamer:     \"rtspsrc location=rtsp://192.168.1.100:554/stream ! decodebin ! videoconvert ! video/x-raw,format=BGR ! appsink\"\n");
        printf("  Video File:              /path/to/video.mp4\n");
        printf("  Test Pattern:            \"videotestsrc ! videoconvert ! video/x-raw,format=BGR ! appsink\"\n");
        return -1;
    }

    // The path where the model is located
    model_name = (char *)argv[1];
    char *input_source = argv[2];
    printf("input_source=%s\n", input_source);
    MLInferenceThread mlThread(
        model_name,
        input_source,
        resultQueue, 
        running,
        30);

    auto json_formatter = std::make_shared<JsonMessageFormatter>();
    UDPPublisher json_publisher(
        "127.0.0.1",
        5002,
        resultQueue, 
        running,
        json_formatter,
        10);

    auto bsvar_formatter = std::make_shared<BSVariableMessageFormatter>();
    UDPPublisher bsvar_publisher(
        "127.0.0.1",
        5000,
        resultQueue, 
        running,
        bsvar_formatter,
        10);        

    std::thread inferenceThread(std::ref(mlThread));
    std::thread json_publisherThread(std::ref(json_publisher));
    std::thread bsvar_publisherThread(std::ref(bsvar_publisher));

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup and shutdown
    running = false;
    resultQueue.signalShutdown();

    inferenceThread.join();
    json_publisherThread.join();
    bsvar_publisherThread.join();

    return 0;
}