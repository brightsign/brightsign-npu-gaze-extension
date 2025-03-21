#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> shutdown{false};
    size_t max_depth;

public:
    ThreadSafeQueue(size_t max_depth) : max_depth(max_depth) {}
    void push(T value);
    bool pop(T& value);
    void signalShutdown();
};

#include "queue.tpp"

#endif // THREAD_SAFE_QUEUE_H