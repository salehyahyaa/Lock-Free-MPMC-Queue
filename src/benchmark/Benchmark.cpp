#include "Benchmark.h"
#include "../lock_based/LockBasedQueue.h"
#include "../lock_free/LockFreeQueue.h"
#include "../utils/Timer.h"

#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <chrono>

using hrc = std::chrono::high_resolution_clock;

BenchmarkResult run_lock_based_benchmark(int num_producers, int num_consumers,
                                          int ops_per_producer, std::vector<OpRecord>& records) {
    LockBasedQueue<int> queue;
    const int item_total = num_producers * ops_per_producer;
    const int reported_total_ops = 2 * item_total;
    std::atomic<int> consumed{0};
    std::atomic<int> op_counter{0};
    std::mutex rec_mutex;

    Timer timer;
    timer.start();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ops_per_producer; ++i) {
                auto t0 = hrc::now();
                queue.push(p * ops_per_producer + i);
                auto t1 = hrc::now();
                double lat = std::chrono::duration<double, std::micro>(t1 - t0).count();
                std::lock_guard<std::mutex> lk(rec_mutex);
                records.push_back({"LockBased", p + 1, "push", op_counter.fetch_add(1) + 1, lat});
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&, c]() {
            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= item_total) break;
                auto t0 = hrc::now();
                auto item = queue.try_pop();
                auto t1 = hrc::now();
                if (item.has_value()) {
                    double lat = std::chrono::duration<double, std::micro>(t1 - t0).count();
                    std::lock_guard<std::mutex> lk(rec_mutex);
                    records.push_back({"LockBased", c + 1, "pop", op_counter.fetch_add(1) + 1, lat});
                    int prev = consumed.fetch_add(1, std::memory_order_relaxed);
                    if (prev + 1 >= item_total) break;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    double duration_ms = timer.elapsed_ms();
    return {"LockBased", num_producers, num_consumers, reported_total_ops, duration_ms,
            (reported_total_ops / duration_ms) * 1000.0, (duration_ms * 1000.0) / reported_total_ops};
}

BenchmarkResult run_lock_free_benchmark(int num_producers, int num_consumers,
                                         int ops_per_producer, std::vector<OpRecord>& records) {
    LockFreeQueue<int> queue;
    const int item_total = num_producers * ops_per_producer;
    const int reported_total_ops = 2 * item_total;
    std::atomic<int> consumed{0};
    std::atomic<int> op_counter{0};
    std::mutex rec_mutex;

    Timer timer;
    timer.start();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ops_per_producer; ++i) {
                auto t0 = hrc::now();
                queue.push(p * ops_per_producer + i);
                auto t1 = hrc::now();
                double lat = std::chrono::duration<double, std::micro>(t1 - t0).count();
                std::lock_guard<std::mutex> lk(rec_mutex);
                records.push_back({"LockFree", p + 1, "push", op_counter.fetch_add(1) + 1, lat});
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&, c]() {
            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= item_total) break;
                auto t0 = hrc::now();
                auto item = queue.try_pop();
                auto t1 = hrc::now();
                if (item.has_value()) {
                    double lat = std::chrono::duration<double, std::micro>(t1 - t0).count();
                    std::lock_guard<std::mutex> lk(rec_mutex);
                    records.push_back({"LockFree", c + 1, "pop", op_counter.fetch_add(1) + 1, lat});
                    int prev = consumed.fetch_add(1, std::memory_order_relaxed);
                    if (prev + 1 >= item_total) break;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    double duration_ms = timer.elapsed_ms();
    return {"LockFree", num_producers, num_consumers, reported_total_ops, duration_ms,
            (reported_total_ops / duration_ms) * 1000.0, (duration_ms * 1000.0) / reported_total_ops};
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
    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ifstream check(filepath);
    bool write_header = !check.good() || check.peek() == std::ifstream::traits_type::eof();
    check.close();

    std::ofstream file(filepath, std::ios::app);
    if (write_header)
        file << "queue_type,num_producers,num_consumers,total_ops,duration_ms,throughput_ops_per_sec,avg_latency_us\n";

    file << std::fixed << std::setprecision(4)
         << r.queue_type << "," << r.num_producers << "," << r.num_consumers << ","
         << r.total_ops << "," << r.duration_ms << "," << r.throughput_ops_per_sec << ","
         << r.avg_latency_us << "\n";
}

void save_raw_csv(const std::vector<OpRecord>& records, const std::string& filepath) {
    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream file(filepath);
    file << "queue_type,thread_id,op_type,op_id,latency_us\n";
    for (const auto& r : records) {
        file << std::fixed << std::setprecision(4)
             << r.queue_type << "," << r.thread_id << ","
             << r.op_type << "," << r.op_id << "," << r.latency_us << "\n";
    }
    std::cout << "Raw data saved to: " << filepath << " (" << records.size() << " operations)\n";
}