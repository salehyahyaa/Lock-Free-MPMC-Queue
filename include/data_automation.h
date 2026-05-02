#pragma once

#include <string>
#include <vector>

namespace pqxx {
class connection;
}

struct BenchmarkResult;
struct OpRecord;

class DataAutomation {
public:
    explicit DataAutomation(pqxx::connection& conn);

    /// Inserts one benchmark session: benchmark_run, benchmark_result, and operation_times.
    void storeRun(const BenchmarkResult& summary, const std::vector<OpRecord>& records);

private:
    pqxx::connection& conn_;
};
