#include <chrono>
#include <memory>
#include <sys/time.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <string>
#include <thread>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
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
    char *model_name = NULL;
    if (argc != 3) {
        printf("Usage: %s <rknn model> <source> \n", argv[0]);
        return -1;
    }

    // The path where the model is located
    model_name = (char *)argv[1];
    char *source_name = argv[2];

    MLInferenceThread mlThread(
        model_name,
        source_name,
        resultQueue, 
        running,
        30);
    const std::string dest_ip = "127.0.0.1";
    UDPPublisher publisher(
        dest_ip,
        5002,
        resultQueue, 
        running,
        1);

    std::thread inferenceThread(std::ref(mlThread));
    std::thread publisherThread(std::ref(publisher));

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup and shutdown
    running = false;
    resultQueue.signalShutdown();

    inferenceThread.join();
    publisherThread.join();

    return 0;
}