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

static std::atomic<bool> app_running{true};           // app lifetime only
static ThreadSafeQueue<InferenceResult> resultQueue(1);

void signalHandler(int signum) {
    printf("Signal (%d) received. Shutting down...\n", signum);
    app_running = false;
    resultQueue.signalShutdown();
}

// Helper to read registry values
class RegistryHelper {
private:
    static const std::string DAEMON_NAME;
    
public:
    static std::string readExtensionValue(const std::string& key) {
        std::string command = "registry extension " + key;
        return executeCommand(command);
    }
    
    static std::string getUdpPublishRate() {
        std::string key = DAEMON_NAME + "-udp-publish-rate";
        std::string publishRate = readExtensionValue(key);
        printf("DEBUG: Registry key: '%s'\n", key.c_str());
        printf("DEBUG: Raw registry result: '%s' (length: %zu)\n", publishRate.c_str(), publishRate.length());

        if (publishRate.empty()) {
            printf("DEBUG: Registry value is empty, using default '10'\n");
            publishRate = "10";
        } else {
            printf("DEBUG: Using registry value: '%s'\n", publishRate.c_str());
        }
        return publishRate;
    }
    
private:
    static std::string executeCommand(const std::string& command) {
        
        std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen(command.c_str(), "r"), pclose);
        
        if (!pipe) {
            printf("DEBUG: Failed to open pipe\n");
            return "";
        }
        
        char buffer[256];
        std::string result;
        
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result += buffer;
        }
        
        // Trim whitespace and newlines
        result.erase(std::remove_if(result.begin(), result.end(), 
                    [](unsigned char c) { return std::isspace(c); }), 
                    result.end());
        
        return result;
    }
};

// Define the DAEMON_NAME constant (same as in bsext_init script)
const std::string RegistryHelper::DAEMON_NAME = "bsext-gaze";


int main(int argc, char **argv) {
#ifdef DEBUG
    if (FILE* log_file = fopen("/storage/sd/console.log", "a")) {
        setvbuf(log_file, NULL, _IOLBF, 0);
        fclose(log_file);
        if (freopen("/storage/sd/console.log", "a", stdout) == NULL) {
            printf("WARNING: Failed to redirect stdout\n");
        } else {
            setvbuf(stdout, NULL, _IOLBF, 0);
        }
    }
#endif

    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    printf("=== BrightSign NPU Gaze Extension Starting ===\n");
    printf("Build timestamp: %s %s\n", __DATE__, __TIME__);

    if (argc != 3) {
        printf("Usage: %s <rknn model> <input_source>\n", argv[0]);
        return -1;
    }
    const char* model_path   = argv[1];
    const char* input_source = argv[2];

    // Start publishers ONCE; they stay up while the app is up.
    printf("Initializing publishers...\n");
    auto json_fmt = std::make_shared<JsonMessageFormatter>();
    auto bs_fmt   = std::make_shared<BSVariableMessageFormatter>();

    std::string udpPublishRate = RegistryHelper::getUdpPublishRate();
    std::cout << "Udp Publish rate from registry: " << udpPublishRate << std::endl;
    
    // Convert string to integer for messages_per_second parameter
    int messages_per_second = std::stoi(udpPublishRate);
    
    // Publishers consume resultQueue until app_running==false or queue shutdown.
    std::atomic<bool> pubs_running{true};
    UDPPublisher json_pub("127.0.0.1", 5002, resultQueue, pubs_running, json_fmt, messages_per_second);
    UDPPublisher bsvar_pub("127.0.0.1", 5000, resultQueue, pubs_running, bs_fmt, messages_per_second);

    std::thread json_publisherThread(std::ref(json_pub));
    std::thread bsvar_publisherThread(std::ref(bsvar_pub));

    // ---- Inference worker supervisor loop ----
    while (app_running) {
        // Per-worker running flag (separate from app_running)
        std::atomic<bool> worker_running{true};
        std::atomic<bool> worker_dead{false};

        // Wrap the worker so we can mark worker_dead when it exits
        std::thread worker_thread([&](){
            try {
                MLInferenceThread worker(
                    model_path,
                    input_source,
                    resultQueue,
                    worker_running,   // this controls ONE worker lifetime
                    20                // target fps (if you use it internally)
                );
                // Run the worker loop
                worker();
            } catch (const std::exception& e) {
                printf("Worker exception: %s\n", e.what());
            } catch (...) {
                printf("Worker exception: unknown\n");
            }
            worker_dead = true;
        });

        // Monitor the worker; respawn if it finishes while app is still running
        while (app_running && !worker_dead.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // If app is stopping, ask the worker to stop too.
        if (!app_running) {
            worker_running = false;
        }

        // Join the worker (will return immediately if already finished)
        if (worker_thread.joinable()) worker_thread.join();

        if (!app_running) break;

        // Worker finished but app is still running → respawn
        printf("Inference worker exited. Restarting in 500 ms...\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // App shutdown: stop publishers and wait for them
    printf("Shutting down gaze tracking system...\n");
    app_running = false;
    resultQueue.signalShutdown();
    // stop publisher loops
    pubs_running = false;

    if (json_publisherThread.joinable())  json_publisherThread.join();
    if (bsvar_publisherThread.joinable()) bsvar_publisherThread.join();

    printf("All threads completed successfully\n");
    printf("=== BrightSign NPU Gaze Extension Shutdown Complete ===\n");
    return 0;
}
