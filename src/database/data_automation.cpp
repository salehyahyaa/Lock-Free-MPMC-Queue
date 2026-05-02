#include "data_automation.h"
#include "benchmark/Benchmark.h"

#include <pqxx/pqxx>

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

DataAutomation::DataAutomation(pqxx::connection& conn) : conn_(conn) {}

namespace {

std::string canonicalQueueName(const std::string& queue_type) {
    if (queue_type == "LockBased")
        return "lock-based";
    if (queue_type == "LockFree")
        return "lock-free";
    throw std::invalid_argument("unknown queue_type label: " + queue_type);
}

int resolveQueueTypeId(pqxx::work& txn, const std::string& queue_type) {
    const std::string name = canonicalQueueName(queue_type);
    auto r = txn.exec_params("SELECT id FROM queue_type WHERE name = $1", name);
    if (r.empty())
        throw std::runtime_error("queue_type not found in database: " + name);
    return r[0][0].as<int>();
}

} // namespace

void DataAutomation::storeRun(const BenchmarkResult& summary, const std::vector<OpRecord>& records) {
    pqxx::work txn(conn_);

    const int queue_type_id = resolveQueueTypeId(txn, summary.queue_type);
    const int thread_count = summary.num_producers + summary.num_consumers;
    std::ostringstream notes;
    notes << "producers=" << summary.num_producers << " consumers=" << summary.num_consumers
          << " total_ops=" << summary.total_ops;

    auto run_row = txn.exec_params(
        "INSERT INTO benchmark_run (queue_type_id, operation, thread_count, notes) "
        "VALUES ($1, $2, $3, $4) RETURNING id",
        queue_type_id, std::string("mixed"), thread_count, notes.str());

    const int run_id = run_row[0][0].as<int>();

    double min_lat = std::numeric_limits<double>::infinity();
    double max_lat = 0.0;
    for (const auto& op : records) {
        if (op.queue_type != summary.queue_type)
            continue;
        min_lat = std::min(min_lat, op.latency_us);
        max_lat = std::max(max_lat, op.latency_us);
    }

    const bool have_samples = min_lat != std::numeric_limits<double>::infinity();
    pqxx::result res_row;
    if (have_samples) {
        res_row = txn.exec_params(
            "INSERT INTO benchmark_result (run_id, operations_performed, throughput_ops_per_sec, "
            "avg_latency_usec, min_latency_usec, max_latency_usec, measurement_interval_sec) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
            run_id, static_cast<long long>(summary.total_ops), summary.throughput_ops_per_sec,
            summary.avg_latency_us, min_lat, max_lat, summary.duration_ms / 1000.0);
    } else {
        res_row = txn.exec_params(
            "INSERT INTO benchmark_result (run_id, operations_performed, throughput_ops_per_sec, "
            "avg_latency_usec, min_latency_usec, max_latency_usec, measurement_interval_sec) "
            "VALUES ($1, $2, $3, $4, NULL, NULL, $5) RETURNING id",
            run_id, static_cast<long long>(summary.total_ops), summary.throughput_ops_per_sec,
            summary.avg_latency_us, summary.duration_ms / 1000.0);
    }

    const int result_id = res_row[0][0].as<int>();

    constexpr size_t kChunk = 1000;
    std::string batch;
    size_t in_chunk = 0;

    auto flush_chunk = [&]() {
        if (batch.empty())
            return;
        batch.pop_back();
        txn.exec("INSERT INTO operation_times (result_id, operation_index, latency_usec) VALUES " + batch);
        batch.clear();
        in_chunk = 0;
    };

    for (const auto& op : records) {
        if (op.queue_type != summary.queue_type)
            continue;
        std::ostringstream lat;
        lat << std::fixed << std::setprecision(6) << op.latency_us;
        batch += "(" + std::to_string(result_id) + "," + std::to_string(static_cast<long long>(op.op_id)) +
                 "," + lat.str() + "),";
        if (++in_chunk >= kChunk)
            flush_chunk();
    }
    flush_chunk();

    txn.commit();
}
