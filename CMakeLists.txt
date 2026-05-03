#compiling of the code basically build instructions
cmake_minimum_required(VERSION 3.15)
project(LockFreeMPMCQueue CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Core Dependencies -------------------------------------------------------
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
# Homebrew installs libpq keg-only; libpqxx.pc requires libpq.pc on PKG_CONFIG_PATH.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  foreach(_brew_prefix IN ITEMS /opt/homebrew /usr/local)
    if(EXISTS "${_brew_prefix}/opt/libpq/lib/pkgconfig/libpq.pc")
      set(ENV{PKG_CONFIG_PATH} "${_brew_prefix}/opt/libpq/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
      break()
    endif()
  endforeach()
endif()

pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
s
# ---- Benchmark ---------------------------------------------------------------
add_executable(benchmark
    src/main.cpp
    src/benchmark/Benchmark.cpp
    src/database/connection.cpp
    src/database/data_automation.cpp
    src/database/numb_data_automation.cpp
)

target_include_directories(benchmark PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(benchmark
    PRIVATE
    Threads::Threads
    PkgConfig::PQXX
)


# ---- Tests -------------------------------------------------------------------
add_executable(test_lock_based
    tests/test_lock_based.cpp
    tests/UnitTestRecorder.cpp
)

target_include_directories(test_lock_based PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/tests"
)
target_link_libraries(test_lock_based PRIVATE Threads::Threads)

add_executable(test_lock_free
    tests/test_lock_free.cpp
    tests/UnitTestRecorder.cpp
)
target_include_directories(test_lock_free PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/tests"
)
target_link_libraries(test_lock_free PRIVATE Threads::Threads)

# Refresh root compile_commands.json on each default build (for clangd / C++ extension).
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(
    copy_compile_commands_json ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Updating compile_commands.json at project root for IDE"
    VERBATIM
  )
endif()