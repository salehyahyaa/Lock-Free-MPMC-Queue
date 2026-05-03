# mpmc queue benchmarking

Comparative research and implementation of multi-producer multi-consumer (MPMC) queues under contention, to benchmark performance between both data structures. The study measures throughput and average per-operation latency across multiple thread counts, together with downstream visualization of the results.

This repository contains two concurrent queue implementations:
- A lock-based MPMC queue using a mutex and condition variable for blocking behavior
- A lock-free MPMC queue using atomic operations and compare-and-swap (CAS)

---
## Theory

### MPMC queues

We study a shared FIFO queue used by many producers and many consumers at once (MPMC). Each producer enqueues items and each consumer dequeues them, so many threads can touch the same structure in parallel. The work compares one design that uses a lock with one that uses lock-free atomics, then measures how they behave under contention. The next two subsections state the main idea behind each design.

### Lock based approach

The lock based variant protects shared state with a **mutex** and uses a **condition variable** so consumers can block efficiently when the queue is empty, with a controlled shutdown path. Serialization simplifies reasoning about correctness but can limit scalability when many threads contend on the same lock.

### Lock free approach

The lock free variant uses **atomic pointers** and **compare and swap (CAS)** loops to link nodes and advance the logical head/tail without holding a global mutex. Progress is non blocking in the usual sense for such structures, at the cost of retry loops, careful memory ordering, and more complex lifecycle management for nodes.

Together, these models illustrate the trade off between **blocking simplicity** and **lock free contention tolerance** as studied in the concurrency literature (e.g. Michael Scott style linked queues as a conceptual reference for CAS based list queues).

---
## Features 

- **MPMC Algorithm Implementation** — Between 2 data strcutres, a **lock-based** queue (mutex + condition variable, with a controlled shutdown path) and a **lock-free** queue (**atomic pointers + CAS** to link nodes and move head/tail).
- **Design** — queues, benchmark harness, PostgreSQL persistence, standalone tests, and the Python dashboard live in **separate directories and build targets**, instead of one monolithic program.
- **testing** — dedicated executables exercise **FIFO behavior**, **emptiness**, **multi-producer / multi-consumer** stress, and (for the lock-based design) **shutdown** behavior.
- **Benchmarking Performance** — symmetric producer–consumer counts (**1, 2, 4, 8**), **100,000 enqueues per producer** per trial, with reported **throughput**, **duration**, **average per-operation latency**, and **total operations** (each **push** and **pop** counted in the total).
- **Data recording** — **CSV** summary (`results/benchmark_results.csv`) plus **per-operation latency traces** under `results/raw_data_operations/` when raw export is enabled for the run.
- **Database** — benchmark **runs**, **summary metrics**, and **per-operation latency rows** are stored in a **relational schema** (see `documentation/schema.sql`) using **PostgreSQL**, so results can be **queried and reused** beyond the flat **CSV** files.
- **Dashboard visualization** — `dashboard/visualization.py` builds an **interactive HTML** report (e.g. `dashboard/out/benchmark_dashboard.html`) from the CSVs for **throughput**, **average latency**, and **latency histograms** from raw traces when present.

--- 
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
- Benchmarks run both lock-based and lock-free MPMC queue implementations at thread counts of **1, 2, 4, and 8 producer(s) plus an equal number of consumer(s)**.
- In each benchmark session, **every producer performs 100,000 operations**. Thus, at each thread level (with equal number of consumers), the total number of operations per run is:
    - **1 producer / 1 consumer:** 100,000 operations
    - **2 producers / 2 consumers:** 200,000 operations
    - **4 producers / 4 consumers:** 400,000 operations
    - **8 producers / 8 consumers:** 800,000 operations
- These levels systematically scale contention from low (2 threads) to high (16 threads).
- **Metrics measured:**
    - **Total duration** (milliseconds)
    - **Throughput** (operations per second)
    - **Average latency** (microseconds per operation)
- Results are printed to the console, saved to `results/benchmark_results.csv`, and optionally inserted into a PostgreSQL database for further research and visualization.
- Detailed, per-operation timings are available under `results/raw_data_operations/` for both queue types.

--- 
## Findings
- **Memory Safety Issues (Lock-Free):**  
  Under high contention, a thread may deallocate (free) a queue node while another thread is still reading it, causing segmentation faults (use-after-free bugs).  
  Without hazard pointers or epoch-based memory reclamation, lock-free designs expose threads to undefined behavior—a core challenge absent in lock-based queues.
- **Atomic Contention Degrades Performance:**  
  Lock-free queues rely on atomic compare-and-swap (CAS) operations. With more threads, CAS failure rate rises, triggering more retries, which reduces throughput and increases observed latency.
- **CPU Cache Coherence Overhead:**  
  Shared variables (e.g., head/tail pointers) are cached by CPU cores. Each update triggers cache invalidations and remote memory access, forcing all threads to constantly synchronize with main memory. The result: noticeable overhead and performance degradation under heavy concurrency.

---
## How To Install & Run 

### 1. Requirements

- **C++20**
- **pthreads**
- **pkg-config**
- **libpqxx** (for DB connection)
- **CMake ≥ 3.15**

### 2. Build C++ Components

```bash
git clone https://github.com/salehyahyaa/Lock-Free-MPMC-Queue.git
cd Lock-Free-MPMC-Queue
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 3. Setup PostgreSQL (for DB-logged benchmarks)

- Ensure PostgreSQL server is running and create the `queue_benchmark` database.
- Load schema:
    ```bash
    psql -d queue_benchmark -f documentation/schema.sql
    ```
- Adjust connection details in `src/database/connection.cpp` if needed.

### 4. Run Benchmarks

From `build/`:
```bash
./benchmark
```
- Summary: `results/benchmark_results.csv`
- Raw data: `results/raw_data_operations/`
- Database integration is optional but recommended for full research replication.

### 5. Run Correctness Tests

From `build/`:
```bash
./test_lock_based
./test_lock_free
```
- Output appended to `results/test_cases.csv`.

### 6. Data Visualization

From the repository root:

```bash
pip install -r dashboard/requirements.txt
python3 dashboard/visualization.py```
```

---

## Technologies Used

- **C++20:** Queue implementations, benchmarking, testing
- **libpqxx**: PostgreSQL C++ driver for DB integration
- **Python (pandas, matplotlib):** Data analysis & visualization (see `data_viz/`)
- **PostgreSQL:** For structured storage of benchmark results

---
## Dependencies

**C++ 20:**  
- CMake ≥ 3.15  
- libpqxx (and dependencies: libpq/PostgreSQL client libs)  
- pthreads

**Python:**  
See `data_viz/requirements.txt` for all required packages  
(pandas, matplotlib, and related libraries)

--- 

## Project Structure
```
mpmc-queue-benchmarking/
├── dashboard/
│   ├── requirements.txt
│   └── visualization.py
├── documentation/
│   ├── PRD.md
│   ├── ResearchPaper.pdf
│   ├── clear_benchmark_data.sql
│   └── schema.sql
├── include/
│   ├── connection.h
│   ├── data_automation.h
│   └── numb_data_automation.h
├── results/
│   ├── benchmark_results.csv
│   ├── pre-test_results.csv
│   └── raw_data_operations/
├── src/
│   ├── benchmark/
│   ├── database/
│   ├── lock_based/
│   ├── lock_free/
│   ├── main.cpp
│   └── utils/
├── tests/
│   ├── QueueMpmcScenarios.h
│   ├── TestMacros.h
│   ├── UnitTestRecorder.cpp
│   ├── UnitTestRecorder.h
│   ├── test_lock_based.cpp
│   └── test_lock_free.cpp
├── .gitattributes
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── README.md
```

---

## 📄 License
Apache License

---

## ✍️ Acknowledgements
- Saleh Yahya
- Christopher Fawaz

---
For full methodology and results, visit either of the following locations:
- **ResearchGate:** [Concurrent Queue Performance Benchmarking](https://www.researchgate.net/publication/404301162_Concurrent_Queue_Performance_Benchmarking)
- See also: [documentation/ResearchPaper.pdf](documentation/ResearchPaper.pdf)