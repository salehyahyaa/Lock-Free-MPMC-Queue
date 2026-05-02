#include "UnitTestRecorder.h"
#include "TestMacros.h"
#include "QueueMpmcScenarios.h"
#include "lock_free/LockFreeQueue.h"

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

namespace {

constexpr const char* kDefaultOwner = "Saleh Yahya / Christopher Fawaz";

class LockFreeQueueTestSuite {
public:
    explicit LockFreeQueueTestSuite(UnitTestRecorder& recorder)
        : recorder_(recorder) {}

    void run_all() {
        basic_push_try_pop();
        empty_after_operations();
        mpmc_correctness();
        high_contention();
    }

private:
    void basic_push_try_pop() {
        std::cout << "\n[Test 1] Basic single-threaded push/try_pop\n";
        LockFreeQueue<int> queue;
        queue.push(10);
        queue.push(20);
        queue.push(30);

        QUEUE_TEST_RECORD(recorder_, queue.try_pop() == 10, "LF-1.1", "try_pop returns 10 (FIFO order)", "first value = 10", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, queue.try_pop() == 20, "LF-1.2", "try_pop returns 20", "second value = 20", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, queue.try_pop() == 30, "LF-1.3", "try_pop returns 30", "third value = 30", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, !queue.try_pop().has_value(), "LF-1.4", "try_pop returns nullopt when empty",
                          "std::nullopt returned", kDefaultOwner);
    }

    void empty_after_operations() {
        std::cout << "\n[Test 2] empty() on new queue\n";
        LockFreeQueue<int> queue;
        QUEUE_TEST_RECORD(recorder_, queue.empty(), "LF-2.1", "new queue is empty", "empty() == true", kDefaultOwner);
        queue.push(1);
        QUEUE_TEST_RECORD(recorder_, !queue.empty(), "LF-2.2", "queue not empty after push", "empty() == false", kDefaultOwner);
        queue.try_pop();
        QUEUE_TEST_RECORD(recorder_, queue.empty(), "LF-2.3", "queue empty after pop", "empty() == true", kDefaultOwner);
    }

    void mpmc_correctness() {
        std::cout << "\n[Test 3] MPMC correctness (4 producers, 4 consumers)\n";
        LockFreeQueue<int> queue;
        queue_tests::run_four_by_four_try_pop_mpmc(recorder_, queue, "LF-3", kDefaultOwner);
    }

    void high_contention() {
        std::cout << "\n[Test 4] High contention (8 producers, 8 consumers)\n";

        constexpr int kThreads = 8;
        constexpr int kOpsPerThread = 500;
        const int total = kThreads * kOpsPerThread;

        LockFreeQueue<int> queue;
        std::atomic<int> consumed{0};

        std::vector<std::thread> producers;
        producers.reserve(kThreads);
        for (int p = 0; p < kThreads; ++p) {
            producers.emplace_back([&, p]() {
                for (int i = 0; i < kOpsPerThread; ++i)
                    queue.push(p * kOpsPerThread + i);
            });
        }

        std::vector<std::thread> consumers;
        consumers.reserve(kThreads);
        for (int c = 0; c < kThreads; ++c) {
            consumers.emplace_back([&]() {
                while (consumed.load(std::memory_order_relaxed) < total) {
                    auto item = queue.try_pop();
                    if (item.has_value())
                        consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : producers)
            t.join();
        for (auto& t : consumers)
            t.join();

        QUEUE_TEST_RECORD(recorder_, consumed.load() == total, "LF-4.1",
                          "All 4000 items consumed under high contention (8P/8C)", "consumed count == 4000", kDefaultOwner);
    }

    UnitTestRecorder& recorder_;
};

} // namespace

int main() {
    std::cout << "=== LockFreeQueue Tests ===\n";

    UnitTestRecorder recorder;
    LockFreeQueueTestSuite suite(recorder);
    suite.run_all();

    recorder.print_summary();
    recorder.save_csv("../results/test_cases.csv");

    return recorder.failed() == 0 ? 0 : 1;
}
