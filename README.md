# Lock-Free MPMC Queue

Implementation of lock-based and lock-free multi-producer multi-consumer queues with CAS-based synchronization and performance benchmarking.



This repository contains two concurrent queue implementations:

- A lock-based MPMC queue using a mutex and condition variable for blocking behavior
- A lock-free MPMC queue using atomic operations and compare-and-swap (CAS)

It also includes correctness tests and a benchmarking harness that measures throughput and average per-operation latency across multiple thread counts.

## Theory

### Multi Producer Multi Consumer queues

An MPMC queue is a shared FIFO structure where multiple producer threads push items and multiple consumer threads pop items concurrently. Correctness requires that operations are thread safe and that values are not lost or duplicated under contention.

### Lock-based synchronization

The lock-based queue protects a standard `std::queue` with:

- `std::mutex` to serialize access to the internal queue
- `std::condition_variable` to allow consumers to sleep while waiting for data instead of busy waiting
- A shutdown flag that can unblock waiting consumers

This design is straightforward and typically performs well at low to moderate contention, but can suffer when many threads contend for the same lock.

### Lock-free synchronization

The lock-free queue is a non-blocking algorithm using atomic pointers and CAS. The implementation uses:

- A sentinel dummy head node so `head_` and `tail_` never start as null
- `std::atomic<Node*>` for `head_`, `tail_`, and `Node::next`
- CAS loops to link new nodes on push and advance head on pop

The algorithm resembles the Michael and Scott linked queue approach in structure:

- `push` links a new node at `tail->next` using CAS, then attempts to advance `tail_`
- `try_pop` reads `head_`, `tail_`, and `head->next`; if empty returns `nullopt`, otherwise CAS advances `head_` and deletes the old head node

This design avoids mutex blocking, but it introduces additional complexity and overhead from retry loops, atomic memory ordering, and memory management.

## Design

### Lock-based queue techniques

- Mutual exclusion with `std::mutex`
- Blocking and wakeup using `std::condition_variable`
- Shutdown signaling by setting a flag and notifying all waiters
- Non-blocking pop option via `try_pop` returning `std::optional<T>`

### Lock-free queue techniques

- Sentinel dummy node initialization
- CAS based synchronization with `compare_exchange_weak`
- Atomic memory ordering with acquire, release, and relaxed operations
- Retry loops to handle contention
- Manual node allocation and deletion

### Testing techniques

Two standalone test executables validate correctness:

- Single-thread FIFO behavior
- `empty()` behavior
- MPMC correctness with multiple producers and consumers, checking that all items are consumed and there are no duplicates
- High contention scenarios
- Lock-based shutdown behavior to ensure blocked consumers can exit

### Benchmarking techniques

The benchmark program runs both implementations with thread counts:

- 1, 2, 4, 8 producers and the same number of consumers

Measured metrics:

- Total duration in milliseconds
- Throughput in operations per second
- Average per-operation latency in microseconds

Results are printed to stdout and appended to a CSV file.

## Findings

The repository includes benchmark output in:

- `results/benchmark_results.csv`

From the provided CSV currently in the repo:

- For the tested runs, the lock-based queue achieves higher throughput than the lock-free queue at 1 and 2 thread pairs in the recorded entries.
- The lock-free queue shows higher average latency and lower throughput in those recorded entries.

Important note about results:

- The CSV contains multiple entries and appears to include repeated runs for some configurations.
- The benchmark tool appends results to the same CSV file, so the file can include multiple sessions over time.

## Conclusion

This project demonstrates both blocking and non-blocking approaches to building MPMC queues and provides a practical comparison via tests and benchmarking. The lock-based queue offers simpler correctness and efficient blocking behavior with condition variables, while the lock-free queue demonstrates CAS-driven concurrency with retry based progress. The included benchmark results show that lock-free is not automatically faster in practice and that performance depends on contention level, atomic overhead, and implementation details.

## Project Structure

```
.
├── CMakeLists.txt
├── LICENSE
├── README.md
├── documentation/
│   ├── PRD.md
│   └── Paper.pdf
├── results/
│   └── benchmark_results.csv
├── src/
│   ├── benchmark/
│   │   ├── Benchmark.cpp
│   │   └── Benchmark.h
│   ├── lock_based/
│   │   ├── LockBasedQueue.cpp
│   │   └── LockBasedQueue.h
│   ├── lock_free/
│   │   ├── LockFreeQueue.cpp
│   │   └── LockFreeQueue.h
│   ├── main.cpp
│   └── utils/
│       ├── ThreadPool.h
│       └── Timer.h
└── tests/
    ├── test_lock_based.cpp
    └── test_lock_free.cpp
```

## How to Install

### Prerequisites

- CMake 3.15 or newer
- A C++17 compatible compiler
- A platform providing pthreads (CMake uses `find_package(Threads REQUIRED)`)

### Build

From the repository root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

This builds three executables:

- `benchmark`
- `test_lock_based`
- `test_lock_free`

### Run tests

From the `build` directory:

```bash
./test_lock_based
./test_lock_free
```

### Run benchmark

From the `build` directory:

```bash
./benchmark
```

The benchmark writes results to:

- `../results/benchmark_results.csv`

So it should be executed from `build` as shown above to match the relative path used by the program.

## Acknowledgements

- Saleh Yahya
- Christopher Fawaz