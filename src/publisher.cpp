#include "publisher.h"

#include <iostream>
#include <thread>


UDPPublisher::UDPPublisher(
        const std::string& ip,
        const int port,
        ThreadSafeQueue<InferenceResult>& queue, 
        std::atomic<bool>& isRunning,
        int messages_per_second=1)
    : resultQueue(queue), running(isRunning), target_mps(messages_per_second) {

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

json UDPPublisher::formatMessage(const InferenceResult& result) {
    json j;

    j["faces_in_frame_total"] = result.count_all_faces_in_frame;
    j["faces_attending"] = result.num_faces_attending;
    j["timestamp"] = std::chrono::system_clock::to_time_t(result.timestamp);
    return j;
}

void UDPPublisher::operator()() {
    InferenceResult result;
    while (resultQueue.pop(result)) {
        json message = formatMessage(result);
        std::string msg_str = message.dump();
        // std::cout << msg_str << std::endl;
        
        sendto(sockfd, msg_str.c_str(), msg_str.length(), 0,
               (struct sockaddr*)&servaddr, sizeof(servaddr));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / target_mps));
    }
}