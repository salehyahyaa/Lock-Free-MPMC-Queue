#include "Benchmark.h"
#include "../lock_based/LockBasedQueue.h"
#include "../lock_free/LockFreeQueue.h"
#include "../utils/Timer.h"

#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <fstream>
#include <iomanip>

BenchmarkResult run_lock_based_benchmark(int num_producers,
                                          int num_consumers,
                                          int ops_per_producer) {
    LockBasedQueue<int> queue;
    int total_ops = num_producers * ops_per_producer;
    std::atomic<int> consumed{0};

    Timer timer;
    timer.start();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ops_per_producer; ++i)
                queue.push(p * ops_per_producer + i);
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= total_ops) break;
                auto item = queue.try_pop();
                if (item.has_value()) {
                    int prev = consumed.fetch_add(1, std::memory_order_relaxed);
                    if (prev + 1 >= total_ops) break;
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    double duration_ms = timer.elapsed_ms();

    return {
        "LockBased",
        num_producers,
        num_consumers,
        total_ops,
        duration_ms,
        (total_ops / duration_ms) * 1000.0,
        (duration_ms * 1000.0) / total_ops
    };
}

void print_result(const BenchmarkResult& r) {
    std::cout << std::fixed << std::setprecision(2)
              << "[" << r.queue_type << "] "
              << "Producers: " << r.num_producers
              << "  Consumers: " << r.num_consumers
              << "  Total ops: " << r.total_ops
              << "  Time: " << r.duration_ms << " ms"
              << "  Throughput: " << r.throughput_ops_per_sec << " ops/s"
              << "  Avg latency: " << r.avg_latency_us << " us\n";
}

void save_result_csv(const BenchmarkResult& r, const std::string& filepath) {
    std::ifstream check(filepath);
    bool write_header = !check.good() || check.peek() == std::ifstream::traits_type::eof();
    check.close();

    std::ofstream file(filepath, std::ios::app);
    if (write_header)
        file << "queue_type,num_producers,num_consumers,total_ops,duration_ms,throughput_ops_per_sec,avg_latency_us\n";

    file << std::fixed << std::setprecision(4)
         << r.queue_type << ","
         << r.num_producers << ","
         << r.num_consumers << ","
         << r.total_ops << ","
         << r.duration_ms << ","
         << r.throughput_ops_per_sec << ","
         << r.avg_latency_us << "\n";
}

BenchmarkResult run_lock_free_benchmark(int num_producers,
                                         int num_consumers,
                                         int ops_per_producer) {
    LockFreeQueue<int> queue;
    int total_ops = num_producers * ops_per_producer;
    std::atomic<int> consumed{0};

    Timer timer;
    timer.start();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ops_per_producer; ++i)
                queue.push(p * ops_per_producer + i);
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= total_ops) break;
                auto item = queue.try_pop();
                if (item.has_value()) {
                    int prev = consumed.fetch_add(1, std::memory_order_relaxed);
                    if (prev + 1 >= total_ops) break;
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    double duration_ms = timer.elapsed_ms();

    return {
        "LockFree",
        num_producers,
        num_consumers,
        total_ops,
        duration_ms,
        (total_ops / duration_ms) * 1000.0,
        (duration_ms * 1000.0) / total_ops
    };
}
