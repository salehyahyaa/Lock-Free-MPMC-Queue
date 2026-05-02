#pragma once

#include <string>
#include <vector>

struct BenchmarkResult {
    std::string queue_type;
    int         num_producers;
    int         num_consumers;
    /// Push + pop count (= 2 × items through the queue for balanced runs).
    int         total_ops;
    double      duration_ms;
    double      throughput_ops_per_sec;
    double      avg_latency_us;
};

struct OpRecord {
    std::string queue_type;
    int         thread_id;
    std::string op_type;
    int         op_id;
    double      latency_us;
};

BenchmarkResult run_lock_based_benchmark(int num_producers, int num_consumers, int ops_per_producer, std::vector<OpRecord>& records);
BenchmarkResult run_lock_free_benchmark(int num_producers, int num_consumers, int ops_per_producer, std::vector<OpRecord>& records);
void print_result(const BenchmarkResult& r);
void save_result_csv(const BenchmarkResult& r, const std::string& filepath);
void save_raw_csv(const std::vector<OpRecord>& records, const std::string& filepath);