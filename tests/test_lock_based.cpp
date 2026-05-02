#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <mutex>
#include <cassert>
#include <chrono>
#include <ctime>
#include <sstream>
#include "../src/lock_based/LockBasedQueue.h"

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
    // Check if file already exists to decide whether to write header
    std::ifstream check(path);
    bool write_header = !check.good() || check.peek() == std::ifstream::traits_type::eof();
    check.close();

    std::ofstream f(path, std::ios::app);
    if (write_header)
        f << "id,description,expected_result,pass_fail,what_went_wrong,trial_date,owner\n";

    for (const auto& r : g_records) {
        // Wrap fields in quotes to handle commas safely
        f << "\"" << r.id            << "\","
          << "\"" << r.description   << "\","
          << "\"" << r.expected      << "\","
          << "\"" << r.result        << "\","
          << "\"" << r.what_went_wrong << "\","
          << "\"" << r.trial_date    << "\","
          << "\"" << r.owner         << "\"\n";
    }
    std::cout << "\nTest results saved to: " << path
              << " (" << g_records.size() << " records)\n";
}

// ── CHECK macro — records to CSV and prints to terminal ───────────────────────
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
    std::cout << "\n[Test 1] Basic single-threaded push/pop\n";
    LockBasedQueue<int> q;
    q.push(1); q.push(2); q.push(3);

    CHECK(q.size() == 3,     "LB-1.1", "size() == 3 after 3 pushes",    "size returns 3",              "Saleh Yahya / Christopher Fawaz");
    CHECK(q.pop()  == 1,     "LB-1.2", "pop() returns 1 (FIFO order)",  "first pushed value returned", "Saleh Yahya / Christopher Fawaz");
    CHECK(q.pop()  == 2,     "LB-1.3", "pop() returns 2",               "second pushed value returned","Saleh Yahya / Christopher Fawaz");
    CHECK(q.pop()  == 3,     "LB-1.4", "pop() returns 3",               "third pushed value returned", "Saleh Yahya / Christopher Fawaz");
    CHECK(q.empty() == true, "LB-1.5", "queue empty after all pops",    "empty() returns true",        "Saleh Yahya / Christopher Fawaz");
}

void test_try_pop_empty() {
    std::cout << "\n[Test 2] try_pop on empty queue\n";
    LockBasedQueue<int> q;
    CHECK(!q.try_pop().has_value(), "LB-2.1", "try_pop returns nullopt on empty queue",
          "std::nullopt returned", "Saleh Yahya / Christopher Fawaz");
}

void test_mpmc_correctness() {
    std::cout << "\n[Test 3] MPMC correctness (4 producers, 4 consumers)\n";

    const int NUM_PRODUCERS  = 4;
    const int NUM_CONSUMERS  = 4;
    const int OPS_PER_THREAD = 1000;
    const int TOTAL          = NUM_PRODUCERS * OPS_PER_THREAD;

    LockBasedQueue<int> q;
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
    CHECK((int)results.size()        == TOTAL, "LB-3.1", "All 4000 items consumed (4 producers x 1000 ops)",
          "consumed count == 4000",   "Saleh Yahya / Christopher Fawaz");
    CHECK((int)unique_results.size() == TOTAL, "LB-3.2", "No duplicate items across all consumer threads",
          "unique count == 4000",     "Saleh Yahya / Christopher Fawaz");
}

void test_shutdown() {
    std::cout << "\n[Test 4] shutdown() unblocks blocked consumers\n";

    LockBasedQueue<int> q;
    std::atomic<bool> consumer_exited{false};

    std::thread consumer([&]() {
        q.pop();
        consumer_exited = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.shutdown();
    consumer.join();

    CHECK(consumer_exited.load(), "LB-4.1", "Consumer exits cleanly after shutdown() is called",
          "consumer_exited == true", "Saleh Yahya / Christopher Fawaz");
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "=== LockBasedQueue Tests ===\n";

    test_basic_push_pop();
    test_try_pop_empty();
    test_mpmc_correctness();
    test_shutdown();

    std::cout << "\n--- Results: "
              << tests_passed << " passed, "
              << tests_failed << " failed ---\n";

    save_csv("../results/test_cases.csv");

    return tests_failed == 0 ? 0 : 1;
}