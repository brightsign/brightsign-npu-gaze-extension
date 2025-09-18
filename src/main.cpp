#include <chrono>
#include <memory>
#include <sys/time.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <signal.h>
#include <atomic>

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
    // Set up logging and signal handling
    #ifdef DEBUG
    freopen("/storage/sd/console.log", "a", stdout);
    #endif
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    printf("=== BrightSign NPU Gaze Extension Starting ===\n");
    printf("Build timestamp: %s %s\n", __DATE__, __TIME__);
    
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
    printf("Starting with model: %s\n", model_name);
    printf("Input source: %s\n", input_source);
    // Initialize ML inference thread with error handling
    printf("Initializing ML inference thread...\n");
    try {
        MLInferenceThread mlThread(
            model_name,
            input_source,
            resultQueue, 
            running,
            20);

        printf("ML inference thread created successfully\n");

        // Initialize JSON UDP publisher
        printf("Initializing JSON UDP publisher on 127.0.0.1:5002...\n");
        auto json_formatter = std::make_shared<JsonMessageFormatter>();
        UDPPublisher json_publisher(
            "127.0.0.1",
            5002,
            resultQueue, 
            running,
            json_formatter,
            10);
        printf("JSON UDP publisher initialized\n");

        // Initialize BrightSign variable UDP publisher  
        printf("Initializing BrightSign variable UDP publisher on 127.0.0.1:5000...\n");
        auto bsvar_formatter = std::make_shared<BSVariableMessageFormatter>();
        UDPPublisher bsvar_publisher(
            "127.0.0.1",
            5000,
            resultQueue, 
            running,
            bsvar_formatter,
            10);
        printf("BrightSign variable UDP publisher initialized\n");

        // Start all threads
        printf("Starting inference and publisher threads...\n");
        std::thread inferenceThread(std::ref(mlThread));
        std::thread json_publisherThread(std::ref(json_publisher));
        std::thread bsvar_publisherThread(std::ref(bsvar_publisher));
        
        printf("All threads started successfully\n");
        printf("Gaze tracking system is now running...\n");

        // Main loop with graceful shutdown
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Cleanup and shutdown
        printf("Shutting down gaze tracking system...\n");
        running = false;
        resultQueue.signalShutdown();

        // Join threads with timeout protection
        printf("Waiting for threads to complete...\n");
        inferenceThread.join();
        json_publisherThread.join();
        bsvar_publisherThread.join();
        printf("✓ All threads completed successfully\n");
        printf("=== BrightSign NPU Gaze Extension Shutdown Complete ===\n");
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception during initialization: %s\n", e.what());
        running = false;
        resultQueue.signalShutdown();
        return -1;
    } catch (...) {
        printf("ERROR: Unknown exception during initialization\n");
        running = false;
        resultQueue.signalShutdown();
        return -1;
    }

    return 0;
}