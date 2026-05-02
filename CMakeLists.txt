#compiling of the code basically build instructions
cmake_minimum_required(VERSION 3.15)
project(LockFreeMPMCQueue CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(Threads REQUIRED)

add_executable(benchmark
    src/main.cpp
    src/benchmark/Benchmark.cpp
)
target_include_directories(benchmark PRIVATE src)
target_link_libraries(benchmark PRIVATE Threads::Threads)

add_executable(test_lock_based
    tests/test_lock_based.cpp
)
target_include_directories(test_lock_based PRIVATE .)
target_link_libraries(test_lock_based PRIVATE Threads::Threads)

add_executable(test_lock_free
    tests/test_lock_free.cpp
)
target_include_directories(test_lock_free PRIVATE .)
target_link_libraries(test_lock_free PRIVATE Threads::Threads)