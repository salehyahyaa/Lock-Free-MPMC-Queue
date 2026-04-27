#pragma once
#include <optional>
#include "LockBasedQueue.h"

template <typename T>
void LockBasedQueue<T>::push(T value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
    }
    cv_.notify_one();
}

template <typename T>
T LockBasedQueue<T>::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
        return !queue_.empty() || shutdown_;
    });

    if (queue_.empty()) return T{};

    T value = std::move(queue_.front());
    queue_.pop();
    return value;
}

template <typename T>
std::optional<T> LockBasedQueue<T>::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return std::nullopt;

    T value = std::move(queue_.front());
    queue_.pop();
    return value;
}

template <typename T>
bool LockBasedQueue<T>::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template <typename T>
size_t LockBasedQueue<T>::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

template <typename T>
void LockBasedQueue<T>::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();
}
