#pragma once

#include <atomic>
#include <optional>

template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue();
    ~LockFreeQueue();

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    void push(T value);
    std::optional<T> try_pop();
    bool empty() const;

private:
    struct Node {
        T data;
        std::atomic<Node*> next;
        Node() : next(nullptr) {}
        Node(T val) : data(std::move(val)), next(nullptr) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};

#include "LockFreeQueue.cpp"
