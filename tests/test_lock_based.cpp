#include "UnitTestRecorder.h"
#include "TestMacros.h"
#include "QueueMpmcScenarios.h"
#include "lock_based/LockBasedQueue.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace {

constexpr const char* kDefaultOwner = "Saleh Yahya / Christopher Fawaz";

/// Lock-based queue acceptance tests; results go through `UnitTestRecorder`.
class LockBasedQueueTestSuite {
public:
    explicit LockBasedQueueTestSuite(UnitTestRecorder& recorder)
        : recorder_(recorder) {}

    void run_all() {
        basic_push_pop();
        try_pop_empty();
        mpmc_correctness();
        shutdown_unblocks_consumer();
    }

private:
    void basic_push_pop() {
        std::cout << "\n[Test 1] Basic single-threaded push/pop\n";
        LockBasedQueue<int> queue;
        queue.push(1);
        queue.push(2);
        queue.push(3);

        QUEUE_TEST_RECORD(recorder_, queue.size() == 3, "LB-1.1", "size() == 3 after 3 pushes", "size returns 3", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, queue.pop() == 1, "LB-1.2", "pop() returns 1 (FIFO order)", "first pushed value returned", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, queue.pop() == 2, "LB-1.3", "pop() returns 2", "second pushed value returned", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, queue.pop() == 3, "LB-1.4", "pop() returns 3", "third pushed value returned", kDefaultOwner);
        QUEUE_TEST_RECORD(recorder_, queue.empty() == true, "LB-1.5", "queue empty after all pops", "empty() returns true", kDefaultOwner);
    }

    void try_pop_empty() {
        std::cout << "\n[Test 2] try_pop on empty queue\n";
        LockBasedQueue<int> queue;
        QUEUE_TEST_RECORD(recorder_, !queue.try_pop().has_value(), "LB-2.1", "try_pop returns nullopt on empty queue",
                          "std::nullopt returned", kDefaultOwner);
    }

    void mpmc_correctness() {
        std::cout << "\n[Test 3] MPMC correctness (4 producers, 4 consumers)\n";
        LockBasedQueue<int> queue;
        queue_tests::run_four_by_four_try_pop_mpmc(recorder_, queue, "LB-3", kDefaultOwner);
    }

    void shutdown_unblocks_consumer() {
        std::cout << "\n[Test 4] shutdown() unblocks blocked consumers\n";
        LockBasedQueue<int> queue;
        std::atomic<bool> consumer_exited{false};

        std::thread consumer([&]() {
            queue.pop();
            consumer_exited = true;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        queue.shutdown();
        consumer.join();

        QUEUE_TEST_RECORD(recorder_, consumer_exited.load(), "LB-4.1", "Consumer exits cleanly after shutdown() is called",
                          "consumer_exited == true", kDefaultOwner);
    }

    UnitTestRecorder& recorder_;
};

} // namespace

int main() {
    std::cout << "=== LockBasedQueue Tests ===\n";

    UnitTestRecorder recorder;
    LockBasedQueueTestSuite suite(recorder);
    suite.run_all();

    recorder.print_summary();
    recorder.save_csv("../results/test_cases.csv");

    return recorder.failed() == 0 ? 0 : 1;
}
