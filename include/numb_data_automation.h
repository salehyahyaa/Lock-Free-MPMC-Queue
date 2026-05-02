#pragma once

#include <vector>

struct BenchmarkResult;
struct OpRecord;

/// Personal no-op stand-in for `DataAutomation`: `storeRun` does nothing (no PostgreSQL).
/// In `main.cpp`, flip `PERSONAL_BENCHMARK_NUMB_DB` to 1 for demos after your first DB load.
class NumbDataAutomation {
public:
    void storeRun(const BenchmarkResult& summary, const std::vector<OpRecord>& records);
};
