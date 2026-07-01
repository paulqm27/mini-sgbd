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
