Project Overview
This project aims to design and implement two concurrent queue systems: a traditional lock-based queue and a lock-free queue using atomic operations. The purpose is to analyze how different synchronization techniques impact system performance in a multi-threaded environment.

By comparing these two approaches, the project will provide insight into key operating systems concepts such as thread synchronization, contention, and non-blocking algorithms, while also reflecting real-world systems used in high-performance computing and low-latency applications.



Project Goals
Implement a thread-safe lock-based queue using mutexes
Develop a lock-free queue using atomic operations and compare-and-swap (CAS)
Benchmark both implementations under varying levels of concurrency
Evaluate performance based on throughput, latency, and scalability
Analyze tradeoffs between blocking and non-blocking synchronization
Project Plan & Timeline
Week 1 – Lock-Based Queue (Foundation)

Focus on building a reliable baseline using traditional synchronization.

Implement a queue using std::queue and std::mutex
Create thread-safe push() and pop() functions
Support multiple producer and consumer threads
Integrate std::condition_variable to avoid busy-waiting when the queue is empty, allowing consumer threads to sleep until new data is available
Validate correctness through concurrency testing
Deliverable: Fully functional lock-based queue supporting multi-threaded access with efficient waiting using condition variables



Week 2 – Lock-Free Queue (Core Implementation)

Introduce non-blocking synchronization techniques.

Design a node-based queue structure
Use std::atomic and compare-and-swap (CAS) operations
Implement lock-free push() and pop() methods
Ensure correctness without using locks
Address the ABA problem by incorporating techniques such as tagged pointers (pointer + version counter) or safe memory reclamation strategies
Deliverable: Functional lock-free queue using atomic operations with ABA mitigation



Week 3 – Benchmarking and Performance Testing

Measure and compare performance under realistic workloads.

Build benchmarking tools using std::thread and std::chrono
Simulate multiple producers and consumers
Measure:
Throughput (operations per second)
Latency (time per operation)
Scalability (performance across 1, 2, 4, 8 threads)
Run experiments on both implementations
Deliverable: Benchmark results and performance data



Week 4 – Analysis and Final Report

Interpret results and connect findings to real-world systems.

Compare performance under different contention levels
Identify strengths and limitations of each approach
Optionally optimize implementations
Document design decisions, results, and conclusions
Deliverable: Final report, documented codebase, and performance summary

Expected Outcomes
This project will demonstrate how lock-free data structures can improve performance in high-concurrency environments compared to traditional lock-based approaches. It will also provide hands-on experience with key operating systems concepts such as threading, synchronization, and performance optimization.

Additionally, the project reflects real-world systems used in areas like distributed systems, backend infrastructure, and low-latency trading platforms, making it both academically and professionally relevant.



Conclusion
This project combines theoretical operating systems concepts with practical implementation and performance analysis. By building and benchmarking both lock-based and lock-free queues, we aim to gain a deeper understanding of concurrency design decisions and their impact on system performance.

