#pragma once

#include "UnitTestRecorder.h"

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace queue_tests {

/// Four producers × four consumers × 1000 pushes each; validates count and uniqueness via try_pop.
template <typename Queue>
void run_four_by_four_try_pop_mpmc(UnitTestRecorder& recorder,
                                   Queue& queue,
                                   const std::string& case_id_prefix,
                                   const std::string& owner) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kOpsPerThread = 1000;
    const int total = kProducers * kOpsPerThread;

    std::atomic<int> consumed{0};
    std::vector<int> results;
    std::mutex results_mutex;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < kOpsPerThread; ++i)
                queue.push(p * kOpsPerThread + i);
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumers);
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= total)
                    break;
                auto item = queue.try_pop();
                if (item.has_value()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results.push_back(*item);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : producers)
        t.join();
    for (auto& t : consumers)
        t.join();

    const std::set<int> unique_results(results.begin(), results.end());

    recorder.record(static_cast<int>(results.size()) == total,
                   case_id_prefix + ".1",
                   "All 4000 items consumed (4 producers x 1000 ops)",
                   "consumed count == 4000",
                   owner,
                   "results.size() == TOTAL");
    recorder.record(static_cast<int>(unique_results.size()) == total,
                   case_id_prefix + ".2",
                   "No duplicate items across all consumer threads",
                   "unique count == 4000",
                   owner,
                   "unique_results.size() == TOTAL");
}

} // namespace queue_tests
