#pragma once

#include <vector>
#include <thread>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(int num_threads) {
        threads_.reserve(num_threads);
    }

    void submit(std::function<void()> task) {
        threads_.emplace_back(std::move(task));
    }

    void wait_all() {
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
        threads_.clear();
    }

    ~ThreadPool() {
        wait_all();
    }

private:
    std::vector<std::thread> threads_;
};
