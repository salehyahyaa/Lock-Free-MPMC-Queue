#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <mutex>
#include <cassert>
#include "../src/lock_based/LockBasedQueue.h"

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
    std::cout << "\n[Test 1] Basic single-threaded push/pop\n";
    LockBasedQueue<int> q;

    q.push(1); q.push(2); q.push(3);

    CHECK(q.size() == 3,     "size() == 3 after 3 pushes");
    CHECK(q.pop() == 1,      "pop() returns 1 (FIFO)");
    CHECK(q.pop() == 2,      "pop() returns 2");
    CHECK(q.pop() == 3,      "pop() returns 3");
    CHECK(q.empty() == true, "queue empty after all pops");
}

void test_try_pop_empty() {
    std::cout << "\n[Test 2] try_pop on empty queue\n";
    LockBasedQueue<int> q;
    CHECK(!q.try_pop().has_value(), "try_pop returns nullopt on empty queue");
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
    CHECK((int)results.size() == TOTAL,        "All items consumed");
    CHECK((int)unique_results.size() == TOTAL, "No duplicates");
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

    CHECK(consumer_exited.load(), "Consumer exited after shutdown()");
}

int main() {
    std::cout << "=== LockBasedQueue Tests ===\n";

    test_basic_push_pop();
    test_try_pop_empty();
    test_mpmc_correctness();
    test_shutdown();

    std::cout << "\n--- Results: "
              << tests_passed << " passed, "
              << tests_failed << " failed ---\n";

    return tests_failed == 0 ? 0 : 1;
}
