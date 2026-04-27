#pragma once

#include <string>

struct BenchmarkResult {
    std::string queue_type;
    int         num_producers;
    int         num_consumers;
    int         total_ops;
    double      duration_ms;
    double      throughput_ops_per_sec;
    double      avg_latency_us;
};

BenchmarkResult run_lock_based_benchmark(int num_producers, int num_consumers, int ops_per_producer);
BenchmarkResult run_lock_free_benchmark(int num_producers, int num_consumers, int ops_per_producer);
void print_result(const BenchmarkResult& r);
void save_result_csv(const BenchmarkResult& r, const std::string& filepath);
