# Lock-Free MPMC-Queue






## How it works



## Project Strcutre 
```
lockfree-queue-project/
│
├── src/
│   ├── lock_based/
│   │   ├── LockBasedQueue.h
│   │   ├── LockBasedQueue.cpp
│   │
│   ├── lock_free/
│   │   ├── LockFreeQueue.h
│   │   ├── LockFreeQueue.cpp
│   │
│   ├── benchmark/
│   │   ├── Benchmark.h
│   │   ├── Benchmark.cpp
│   │
│   ├── utils/
│   │   ├── Timer.h
│   │   ├── ThreadPool.h   (optional but strong)
│   │
│   └── main.cpp
│
├── tests/
│   ├── test_lock_based.cpp
│   ├── test_lock_free.cpp
│
├── results/
│   ├── raw_data/
│   ├── benchmark_results.csv
│
├── documentation/
│   ├── challenges.pdf
│   ├── overview.txt
│
├── CMakeLists.txt
├── README.md
└── .gitignore
```