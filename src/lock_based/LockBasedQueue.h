#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template <typename T>
class LockBasedQueue {
public:
    LockBasedQueue() = default;
    LockBasedQueue(const LockBasedQueue&) = delete;
    LockBasedQueue& operator=(const LockBasedQueue&) = delete;

    void push(T value);
    T pop();
    std::optional<T> try_pop();
    bool empty() const;
    size_t size() const;
    void shutdown();

private:
    std::queue<T>           queue_;
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    bool                    shutdown_ = false;
};

#include "LockBasedQueue.cpp"
