#include <iostream>
#include "benchmark/Benchmark.h"

static const int THREAD_COUNTS[]  = {1, 2, 4, 8};
static const int OPS_PER_PRODUCER = 100000;
static const std::string CSV_PATH     = "../results/benchmark_results.csv";
static const std::string RAW_LB_PATH = "../results/raw_data/lock_based_raw.csv";
static const std::string RAW_LF_PATH = "../results/raw_data/lock_free_raw.csv";

int main() {
    std::cout << "=== Lock-Free MPMC Queue Benchmark ===\n\n";

    std::vector<OpRecord> lb_records;
    std::vector<OpRecord> lf_records;

    std::cout << "-- Lock-Based Queue --\n";
    for (int threads : THREAD_COUNTS) {
        BenchmarkResult r = run_lock_based_benchmark(threads, threads, OPS_PER_PRODUCER, lb_records);
        print_result(r);
        save_result_csv(r, CSV_PATH);
    }

    std::cout << "\n-- Lock-Free Queue --\n";
    for (int threads : THREAD_COUNTS) {
        BenchmarkResult r = run_lock_free_benchmark(threads, threads, OPS_PER_PRODUCER, lf_records);
        print_result(r);
        save_result_csv(r, CSV_PATH);
    }

    std::cout << "\nResults saved to: " << CSV_PATH << "\n";
    save_raw_csv(lb_records, RAW_LB_PATH);
    save_raw_csv(lf_records, RAW_LF_PATH);

    return 0;
}