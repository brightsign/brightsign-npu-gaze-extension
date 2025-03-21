// Thread-safe queue implementation
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> shutdown{false};

public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(value));
        cond.notify_one();
    }

    bool pop(T& value) {
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

    void signalShutdown() {
        shutdown = true;
        cond.notify_all();
    }
};