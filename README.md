# mpmc-queue-benchmarking

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
│   └── ResearchPaper.pdf
├── include/
│   ├── connection.h
│   └── data_automation.h
├── results/
│   ├── benchmark_results.csv
│   └── raw_data/            (create before benchmark; per-op CSVs written here)
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
│   ├── database/
│   │   ├── connection.cpp
│   │   └── data_automation.cpp
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
- Pthreads (CMake uses `find_package(Threads REQUIRED)`)
- **Pkg-config** (`pkg-config`) and **libpqxx** (with **libpq** / PostgreSQL client libraries). CMake resolves libpqxx via `pkg_check_modules(PQXX REQUIRED libpqxx)`.
- **PostgreSQL** server (only required to run the **`benchmark`** executable; the **`test_lock_*`** programs do not use the database).

On macOS with Homebrew, libpq is often keg-only. CMake prepends Homebrew’s `libpq` pkgconfig path when configuring so libpqxx can be found. Typical installs:

```bash
brew install cmake pkg-config libpqxx
```

### PostgreSQL setup (benchmark only)

1. Start PostgreSQL and create a database (the default connection string in code uses `dbname=queue_benchmark`; change it in `src/database/connection.cpp` if you use another name or credentials).
2. Load the schema:

   ```bash
   psql -d queue_benchmark -f documentation/schema.sql
   ```

3. If connection fails, edit the constructor in `src/database/connection.cpp` to match your `dbname`, `user`, `host`, and `port`.
4. To wipe benchmark rows only (keeps `queue_type` seeds): `psql -d queue_benchmark -f documentation/clear_benchmark_data.sql`

### Result directories

Create the folder used for per-operation CSV exports (the benchmark does not create parent directories for you):

```bash
mkdir -p results/raw_data
```

Summary results append to `results/benchmark_results.csv` relative to the repo root.

### Build

From the repository root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

This produces three executables in `build/`:

- `benchmark` — runs benchmarks, writes CSVs, persists runs to PostgreSQL via libpqxx
- `test_lock_based` — lock-based queue correctness tests
- `test_lock_free` — lock-free queue correctness tests

After a successful configure, CMake can copy `compile_commands.json` into the repo root (for clangd / the C/C++ extension). If IntelliSense cannot find headers under `src/`, run a full build from your binary directory so include flags stay in sync.

### Run tests (no database)

From the `build` directory (paths match how the tests write under `results/`):

```bash
./test_lock_based
./test_lock_free
```

Each test run appends rows to `results/test_cases.csv` (created on first write).

### Run benchmark

Always run from the **`build`** directory so relative paths resolve:

```bash
cd build
./benchmark
```

To re-run the benchmark **without** opening PostgreSQL or inserting rows (e.g. after your first load is in the DB, for demos), set `PERSONAL_BENCHMARK_NUMB_DB` to `1` in `src/main.cpp` (see comment there). `0` uses normal `DataAutomation`.

**Outputs**

| Output | Path (from repo root) | Contents |
|--------|------------------------|----------|
| Summary CSV | `results/benchmark_results.csv` | One row per run; `total_ops` = push + pop primitives (2× items); columns: `queue_type`, `num_producers`, `num_consumers`, `total_ops`, `duration_ms`, `throughput_ops_per_sec`, `avg_latency_us` |
| Raw per-op CSV (lock-based) | `results/raw_data/lock_based_raw.csv` | `queue_type`, `thread_id`, `op_type`, `op_id`, `latency_us` |
| Raw per-op CSV (lock-free) | `results/raw_data/lock_free_raw.csv` | Same columns as lock-based raw file |

The summary file is opened in append mode: new benchmark sessions add rows. If the file is empty, the program writes the header row automatically. Optional comment lines at the top of a hand-edited CSV are not produced by the benchmark.

## Acknowledgements

- Saleh Yahya
- Christopher Fawaz