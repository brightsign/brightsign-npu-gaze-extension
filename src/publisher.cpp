#include "publisher.h"

#include <iostream>
#include <thread>

// Implementation of the JsonMessageFormatter
std::string JsonMessageFormatter::formatMessage(const InferenceResult& result) {
    json j;
    j["faces_in_frame_total"] = result.count_all_faces_in_frame;
    j["faces_attending"] = result.num_faces_attending;
    j["timestamp"] = std::chrono::system_clock::to_time_t(result.timestamp);
    return j.dump();
}

// Implementation of the BSVariableMessageFormatter
std::string BSVariableMessageFormatter::formatMessage(const InferenceResult& result) {
    // format the message as a string like faces_attending:0!!faces_in_frame_total:0!!timestamp:1746732409
    std::string message = 
        "faces_attending:" + std::to_string(result.num_faces_attending) + "!!" + 
        "faces_in_frame_total:" + std::to_string(result.count_all_faces_in_frame) + "!!" +
        "timestamp:" + std::to_string(std::chrono::system_clock::to_time_t(result.timestamp));
    return message;
}

UDPPublisher::UDPPublisher(
        const std::string& ip,
        const int port,
        ThreadSafeQueue<InferenceResult>& queue, 
        std::atomic<bool>& isRunning,
        std::shared_ptr<MessageFormatter> formatter,
        int messages_per_second)
    : resultQueue(queue), 
      running(isRunning), 
      target_mps(messages_per_second),
      formatter(formatter) {

    setupSocket(ip, port);
}

UDPPublisher::~UDPPublisher() {
    close(sockfd);
}

void UDPPublisher::setupSocket(const std::string& ip, const int port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);  
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str()); 
}

void UDPPublisher::operator()() {
    InferenceResult result;
    while (resultQueue.pop(result)) {
        std::string message = formatter->formatMessage(result);
        // std::string msg_str = message.dump();
        // std::cout << msg_str << std::endl;
        
        sendto(sockfd, message.c_str(), message.length(), 0,
               (struct sockaddr*)&servaddr, sizeof(servaddr));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / target_mps));
    }
}