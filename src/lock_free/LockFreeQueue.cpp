#pragma once

#include "LockFreeQueue.h"

// Uses a sentinel/dummy head node so head and tail never point to null.

template <typename T>
LockFreeQueue<T>::LockFreeQueue() {
    Node* dummy = new Node();
    head_.store(dummy, std::memory_order_relaxed);
    tail_.store(dummy, std::memory_order_relaxed);
}

template <typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    while (Node* node = head_.load(std::memory_order_relaxed)) {
        head_.store(node->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
        delete node;
    }
}

template <typename T>
void LockFreeQueue<T>::push(T value) {
    Node* new_node = new Node(std::move(value));

    while (true) {
        Node* tail = tail_.load(std::memory_order_acquire);
        Node* next = tail->next.load(std::memory_order_acquire);

        if (tail == tail_.load(std::memory_order_acquire)) {
            if (next == nullptr) {
                // Tail is pointing to last node, try to link new node
                if (tail->next.compare_exchange_weak(
                        next, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    tail_.compare_exchange_weak(
                        tail, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                    return;
                }
            } else {
                tail_.compare_exchange_weak(
                    tail, next,
                    std::memory_order_release,
                    std::memory_order_relaxed);
            }
        }
    }
}

template <typename T>
std::optional<T> LockFreeQueue<T>::try_pop() {
    while (true) {
        Node* head = head_.load(std::memory_order_acquire);
        Node* tail = tail_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);

        if (head == head_.load(std::memory_order_acquire)) {
            if (head == tail) {
                if (next == nullptr) return std::nullopt; // queue is empty
                // Tail is behind — advance it
                tail_.compare_exchange_weak(
                    tail, next,
                    std::memory_order_release,
                    std::memory_order_relaxed);
            } else {
                T value = next->data;
                if (head_.compare_exchange_weak(
                        head, next,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    delete head;
                    return value;
                }
            }
        }
    }
}

template <typename T>
bool LockFreeQueue<T>::empty() const {
    Node* head = head_.load(std::memory_order_acquire);
    Node* next = head->next.load(std::memory_order_acquire);
    return next == nullptr;
}