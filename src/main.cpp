#include <iostream>
#include "benchmark/Benchmark.h"
#include "../include/connection.h"
#include "../include/data_automation.h"
#include "../include/numb_data_automation.h"

// Personal toggle — flip this one line for demos after data is already in PostgreSQL:
//   0 = Automates executed data to DB
//   1 = Ignores executed data to be sent to DB         // done by ignoring the data_automation.cpp file
#define PERSONAL_BENCHMARK_NUMB_DB 1

static const int THREAD_COUNTS[]  = {1, 2, 4, 8};
static const int OPS_PER_PRODUCER = 100000;
static const std::string CSV_PATH     = "../results/benchmark_results.csv";
static const std::string RAW_LB_PATH = "../results/raw_data_operations/lock_based_raw.csv";
static const std::string RAW_LF_PATH = "../results/raw_data_operations/lock_free_raw.csv";

int main() {
    std::cout << "=== mpmc-queue-benchmarking ===\n\n";

#if PERSONAL_BENCHMARK_NUMB_DB
    NumbDataAutomation store;
    std::cout << "[NumbDataAutomation] No database persistence this run.\n\n";
#else
    Connection db_conn;
    DataAutomation store(db_conn.getConnection());
#endif

    std::vector<OpRecord> lb_records;
    std::vector<OpRecord> lf_records;

    std::cout << "-- Lock-Based Queue --\n";
    for (int threads : THREAD_COUNTS) {
        std::vector<OpRecord> batch;
        BenchmarkResult r = run_lock_based_benchmark(threads, threads, OPS_PER_PRODUCER, batch);
        print_result(r);
        save_result_csv(r, CSV_PATH);
        store.storeRun(r, batch);
        lb_records.insert(lb_records.end(), batch.begin(), batch.end());
    }

    std::cout << "\n-- Lock-Free Queue --\n";
    for (int threads : THREAD_COUNTS) {
        std::vector<OpRecord> batch;
        BenchmarkResult r = run_lock_free_benchmark(threads, threads, OPS_PER_PRODUCER, batch);
        print_result(r);
        save_result_csv(r, CSV_PATH);
        store.storeRun(r, batch);
        lf_records.insert(lf_records.end(), batch.begin(), batch.end());
    }

    std::cout << "\nResults saved to: " << CSV_PATH << '\n';
    save_raw_csv(lb_records, RAW_LB_PATH);
    save_raw_csv(lf_records, RAW_LF_PATH);

    return 0;
}
