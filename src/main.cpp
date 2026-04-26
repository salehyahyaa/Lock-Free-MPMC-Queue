#include <iostream>
#include "benchmark/Benchmark.h"

static const int THREAD_COUNTS[]  = {1, 2, 4, 8};
static const int OPS_PER_PRODUCER = 100000;
static const std::string CSV_PATH = "../results/benchmark_results.csv";

int main() {
    std::cout << "=== Lock-Free MPMC Queue Benchmark ===\n\n";

    std::cout << "-- Lock-Based Queue --\n";
    for (int threads : THREAD_COUNTS) {
        BenchmarkResult r = run_lock_based_benchmark(threads, threads, OPS_PER_PRODUCER);
        print_result(r);
        save_result_csv(r, CSV_PATH);
    }

    std::cout << "\n-- Lock-Free Queue --\n";
    for (int threads : THREAD_COUNTS) {
        BenchmarkResult r = run_lock_free_benchmark(threads, threads, OPS_PER_PRODUCER);
        print_result(r);
        save_result_csv(r, CSV_PATH);
    }

    std::cout << "\nResults saved to: " << CSV_PATH << "\n";
    return 0;
}
