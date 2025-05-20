#pragma once

#include <string>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

// #include "thread_safe_queue.h"
#include "inference.h"

using json = nlohmann::json;

// Abstract message formatter interface
class MessageFormatter {
public:
    virtual ~MessageFormatter() = default;
    virtual std::string formatMessage(const InferenceResult& result) = 0;
};

// Concrete implementation of MessageFormatter for JSON format
class JsonMessageFormatter : public MessageFormatter {
public:
    std::string formatMessage(const InferenceResult& result) override;
};

// Concrete implementation of MessageFormatter for BrightScript variable format
//  e.g. "faces_attending:0!!faces_in_frame_total:0!!timestamp:1746732409"
class BSVariableMessageFormatter : public MessageFormatter {
public:
    std::string formatMessage(const InferenceResult& result) override;
};


class UDPPublisher {
public:
    UDPPublisher(
        const std::string& ip,
        const int port,
        ThreadSafeQueue<InferenceResult>& queue,
        std::atomic<bool>& isRunning,
        std::shared_ptr<MessageFormatter> formatter,
        int messages_per_second = 1);
    
    ~UDPPublisher();
    
    void operator()();

private:
    void setupSocket(const std::string& ip, const int port);
    
    int sockfd;
    struct sockaddr_in servaddr;
    ThreadSafeQueue<InferenceResult>& resultQueue;
    std::atomic<bool>& running;
    int target_mps;
    std::shared_ptr<MessageFormatter> formatter;
};