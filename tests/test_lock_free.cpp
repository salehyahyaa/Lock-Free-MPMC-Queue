#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <mutex>
#include "../src/lock_free/LockFreeQueue.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, name)                                          \
    do {                                                           \
        if (cond) {                                                \
            std::cout << "  [PASS] " << name << "\n";             \
            ++tests_passed;                                        \
        } else {                                                   \
            std::cout << "  [FAIL] " << name << "\n";             \
            ++tests_failed;                                        \
        }                                                          \
    } while (0)

void test_basic_push_pop() {
    std::cout << "\n[Test 1] Basic single-threaded push/try_pop\n";
    LockFreeQueue<int> q;

    q.push(10);
    q.push(20);
    q.push(30);

    CHECK(q.try_pop() == 10, "try_pop returns 10 (FIFO)");
    CHECK(q.try_pop() == 20, "try_pop returns 20");
    CHECK(q.try_pop() == 30, "try_pop returns 30");
    CHECK(!q.try_pop().has_value(), "try_pop returns nullopt when empty");
}

void test_empty() {
    std::cout << "\n[Test 2] empty() on new queue\n";
    LockFreeQueue<int> q;
    CHECK(q.empty(), "new queue is empty");

    q.push(1);
    CHECK(!q.empty(), "queue not empty after push");

    q.try_pop();
    CHECK(q.empty(), "queue empty after pop");
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
    CHECK((int)results.size() == TOTAL,        "All items consumed");
    CHECK((int)unique_results.size() == TOTAL, "No duplicates");
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

    CHECK(consumed.load() == TOTAL, "All items consumed under high contention");
}

int main() {
    std::cout << "=== LockFreeQueue Tests ===\n";

    test_basic_push_pop();
    test_empty();
    test_mpmc_correctness();
    test_high_contention();

    std::cout << "\n--- Results: "
              << tests_passed << " passed, "
              << tests_failed << " failed ---\n";

    return tests_failed == 0 ? 0 : 1;
}
