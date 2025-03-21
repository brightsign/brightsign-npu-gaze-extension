#include "queue.h"

template<typename T>
void ThreadSafeQueue<T>::push(T value) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.size() >= max_depth) {
        queue.pop();
    }
    queue.push(std::move(value));
    cond.notify_one();
}

template<typename T>
bool ThreadSafeQueue<T>::pop(T& value) {
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [this] { 
        return !queue.empty() || shutdown; 
    });
    
    if (shutdown && queue.empty()) {
        return false;
    }

    value = std::move(queue.front());
    queue.pop();
    return true;
}

template<typename T>
void ThreadSafeQueue<T>::signalShutdown() {
    shutdown = true;
    cond.notify_all();
}