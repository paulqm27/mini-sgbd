cmake_minimum_required(VERSION 3.14)
project(mini-sgbd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure data directory exists for tests and main
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data)

# Source files
set(SOURCES
    src/storage/page.cpp
    src/storage/storage.cpp
    src/buffer/buffer.cpp
    src/index/bplus_tree.cpp
    src/execution/scan.cpp
    src/execution/select.cpp
    src/execution/project.cpp
    src/execution/nested_loop_join.cpp
    src/execution/index_scan.cpp
    src/execution/planner.cpp
)

# Main executable
add_executable(mini-sgbd src/main.cpp ${SOURCES})
target_include_directories(mini-sgbd PRIVATE src)

# Tests executable
add_executable(storage_test tests/storage_test.cpp ${SOURCES})
target_include_directories(storage_test PRIVATE src)

# B+ Tree delete/merge test
add_executable(bplus_delete_test tests/bplus_delete_test.cpp ${SOURCES})
target_include_directories(bplus_delete_test PRIVATE src)

# B+ Tree demo with delete, buffer manager and persistence
add_executable(bplus_delete_demo tests/bplus_delete_demo.cpp ${SOURCES})
target_include_directories(bplus_delete_demo PRIVATE src)

# Iterators demo (Scan, Select, Project)
add_executable(iterators_demo src/execution/demo_iterators.cpp ${SOURCES})
target_include_directories(iterators_demo PRIVATE src)

# Join demo (Nested Loop Join + IndexScan)
add_executable(join_demo src/execution/demo_join.cpp ${SOURCES})
target_include_directories(join_demo PRIVATE src)

# Full flow demo (Semana 14, 15 y 16)
add_executable(full_flow_demo src/execution/demo_full_flow.cpp ${SOURCES})
target_include_directories(full_flow_demo PRIVATE src)

# Buffer benchmark for week 16
add_executable(buffer_benchmark tests/buffer_benchmark.cpp ${SOURCES})
target_include_directories(buffer_benchmark PRIVATE src)

# Simple integration test for iterators
add_executable(iterators_test tests/iterators_test.cpp ${SOURCES})
target_include_directories(iterators_test PRIVATE src)

# GoogleTest integration and unit tests
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(gtest_iterators tests/gtest_iterators.cpp ${SOURCES})
target_include_directories(gtest_iterators PRIVATE src)
target_link_libraries(gtest_iterators PRIVATE GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(gtest_iterators)
