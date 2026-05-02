#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include "../src/lock_free/LockFreeQueue.h"

static int tests_passed = 0;
static int tests_failed = 0;

// ── CSV output ────────────────────────────────────────────────────────────────
struct TestRecord {
    std::string id;
    std::string description;
    std::string expected;
    std::string result;
    std::string what_went_wrong;
    std::string trial_date;
    std::string owner;
};

static std::vector<TestRecord> g_records;

static std::string current_timestamp() {
    auto now  = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

static void save_csv(const std::string& path) {
    // Always append — lock_based runs first and writes the header
    std::ofstream f(path, std::ios::app);
    for (const auto& r : g_records) {
        f << "\"" << r.id              << "\","
          << "\"" << r.description     << "\","
          << "\"" << r.expected        << "\","
          << "\"" << r.result          << "\","
          << "\"" << r.what_went_wrong << "\","
          << "\"" << r.trial_date      << "\","
          << "\"" << r.owner           << "\"\n";
    }
    std::cout << "Test results appended to: " << path
              << " (" << g_records.size() << " records)\n";
}

// ── CHECK macro ───────────────────────────────────────────────────────────────
#define CHECK(cond, id, description, expected, owner)                          \
    do {                                                                        \
        bool _ok = (cond);                                                      \
        std::cout << (_ok ? "  [PASS] " : "  [FAIL] ") << (description) << "\n"; \
        if (_ok) ++tests_passed; else ++tests_failed;                          \
        g_records.push_back({                                                   \
            (id),                                                               \
            (description),                                                      \
            (expected),                                                         \
            _ok ? "PASS" : "FAIL",                                             \
            _ok ? "" : "Condition not met: " #cond,                            \
            current_timestamp(),                                                \
            (owner)                                                             \
        });                                                                     \
    } while (0)

// ── Tests ─────────────────────────────────────────────────────────────────────

void test_basic_push_pop() {
    std::cout << "\n[Test 1] Basic single-threaded push/try_pop\n";
    LockFreeQueue<int> q;
    q.push(10); q.push(20); q.push(30);

    CHECK(q.try_pop() == 10,         "LF-1.1", "try_pop returns 10 (FIFO order)",   "first value = 10",    "Saleh Yahya / Christopher Fawaz");
    CHECK(q.try_pop() == 20,         "LF-1.2", "try_pop returns 20",                "second value = 20",   "Saleh Yahya / Christopher Fawaz");
    CHECK(q.try_pop() == 30,         "LF-1.3", "try_pop returns 30",                "third value = 30",    "Saleh Yahya / Christopher Fawaz");
    CHECK(!q.try_pop().has_value(),  "LF-1.4", "try_pop returns nullopt when empty","std::nullopt returned","Saleh Yahya / Christopher Fawaz");
}

void test_empty() {
    std::cout << "\n[Test 2] empty() on new queue\n";
    LockFreeQueue<int> q;
    CHECK(q.empty(),   "LF-2.1", "new queue is empty",          "empty() == true",  "Saleh Yahya / Christopher Fawaz");
    q.push(1);
    CHECK(!q.empty(),  "LF-2.2", "queue not empty after push",  "empty() == false", "Saleh Yahya / Christopher Fawaz");
    q.try_pop();
    CHECK(q.empty(),   "LF-2.3", "queue empty after pop",       "empty() == true",  "Saleh Yahya / Christopher Fawaz");
}

void test_mpmc_correctness() {
    std::cout << "\n[Test 3] MPMC correctness (4 producers, 4 consumers)\n";

    const int NUM_PRODUCERS  = 4;
    const int NUM_CONSUMERS  = 4;
    const int OPS_PER_THREAD = 1000;
    const int TOTAL          = NUM_PRODUCERS * OPS_PER_THREAD;

    LockFreeQueue<int> q;
    std::atomic<int> consumed{0};
    std::vector<int> results;
    std::mutex results_mutex;

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i)
                q.push(p * OPS_PER_THREAD + i);
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                if (consumed.load(std::memory_order_relaxed) >= TOTAL) break;
                auto item = q.try_pop();
                if (item.has_value()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results.push_back(*item);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    std::set<int> unique_results(results.begin(), results.end());
    CHECK((int)results.size()        == TOTAL, "LF-3.1", "All 4000 items consumed (4 producers x 1000 ops)",
          "consumed count == 4000",  "Saleh Yahya / Christopher Fawaz");
    CHECK((int)unique_results.size() == TOTAL, "LF-3.2", "No duplicate items across all consumer threads",
          "unique count == 4000",    "Saleh Yahya / Christopher Fawaz");
}

void test_high_contention() {
    std::cout << "\n[Test 4] High contention (8 producers, 8 consumers)\n";

    const int NUM_THREADS    = 8;
    const int OPS_PER_THREAD = 500;
    const int TOTAL          = NUM_THREADS * OPS_PER_THREAD;

    LockFreeQueue<int> q;
    std::atomic<int> consumed{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_THREADS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i)
                q.push(p * OPS_PER_THREAD + i);
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < NUM_THREADS; ++c) {
        consumers.emplace_back([&]() {
            while (consumed.load(std::memory_order_relaxed) < TOTAL) {
                auto item = q.try_pop();
                if (item.has_value())
                    consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    CHECK(consumed.load() == TOTAL, "LF-4.1",
          "All 4000 items consumed under high contention (8P/8C)",
          "consumed count == 4000", "Saleh Yahya / Christopher Fawaz");
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "=== LockFreeQueue Tests ===\n";

    test_basic_push_pop();
    test_empty();
    test_mpmc_correctness();
    test_high_contention();

    std::cout << "\n--- Results: "
              << tests_passed << " passed, "
              << tests_failed << " failed ---\n";

    save_csv("../results/test_cases.csv");

    return tests_failed == 0 ? 0 : 1;
}