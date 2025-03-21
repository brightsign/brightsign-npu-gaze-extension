#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include "queue.h"
#include "inference.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class UDPPublisher {
private:
    ThreadSafeQueue<InferenceResult>& resultQueue;
    std::atomic<bool>& running;
    int sockfd;
    struct sockaddr_in servaddr;
    int target_mps;            // messages per second

    void setupSocket(const std::string& ip, const int port);
    json formatMessage(const InferenceResult& result);

public:
    UDPPublisher(
            const std::string& dest_ip,
            const int dest_port,
            ThreadSafeQueue<InferenceResult>& queue, 
            std::atomic<bool>& isRunning,
            int updates_per_second);
    ~UDPPublisher();
    void operator()();
};

#endif // PUBLISHER_H