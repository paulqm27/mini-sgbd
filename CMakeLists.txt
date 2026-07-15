cmake_minimum_required(VERSION 3.14)
project(mini-sgbd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure data directory exists for tests and main
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data)

# Source files — módulos base (sin cambios)
set(SOURCES
    src/storage/page.cpp
    src/storage/storage.cpp
    src/buffer/buffer.cpp
    src/index/bplus_tree.cpp
)

# Source files — módulo del Procesador de Consultas (Semanas 14 y 15)
set(QUERY_SOURCES
    src/query/scan_operator.cpp
    src/query/select_operator.cpp
    src/query/project_operator.cpp
    src/query/nested_loop_join_operator.cpp
    src/query/index_scan_operator.cpp
    src/query/query_executor.cpp
)

# Main executable
add_executable(mini-sgbd src/main.cpp ${SOURCES})
target_include_directories(mini-sgbd PRIVATE src)

# ---- Tests existentes (sin cambios) ----
add_executable(test_bplus_insert tests/test_bplus_insert.cpp ${SOURCES})
target_include_directories(test_bplus_insert PRIVATE src)

add_executable(test_bplus_delete tests/test_bplus_delete.cpp ${SOURCES})
target_include_directories(test_bplus_delete PRIVATE src)

add_executable(test_bplus_search tests/test_bplus_search.cpp ${SOURCES})
target_include_directories(test_bplus_search PRIVATE src)

add_executable(test_buffer_manager tests/test_buffer_manager.cpp ${SOURCES})
target_include_directories(test_buffer_manager PRIVATE src)

add_executable(test_persistence tests/test_persistence.cpp ${SOURCES})
target_include_directories(test_persistence PRIVATE src)

# ---- Tests nuevos: Procesador de Consultas (Semanas 14 y 15) ----
add_executable(test_query_operators
    tests/test_query_operators.cpp
    ${SOURCES}
    ${QUERY_SOURCES}
)
target_include_directories(test_query_operators PRIVATE src)

add_executable(test_performance
    tests/test_performance.cpp
    ${SOURCES}
    ${QUERY_SOURCES}
)
target_include_directories(test_performance PRIVATE src)
