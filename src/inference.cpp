#include <cstdlib>
#include <iostream>
#include <thread>

#include "attention.h"
#include "inference.h"


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

MLInferenceThread::MLInferenceThread(
        const char* model_path,
        const char* source_name,
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

    // open the capture
    printf("Opening capture from source: %s\n", source_name);
    try {
        if (!capture.open(source_name)) {
            printf("Failed to open camera at %s\n", source_name);
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
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 320);

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