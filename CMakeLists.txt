cmake_minimum_required(VERSION 3.20)
project(LSMForge
    VERSION 1.0.0
    DESCRIPTION "A High-Performance LSM-Tree Storage Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Options
option(LSMFORGE_BUILD_TESTS "Build LSMForge test suite" ON)
option(LSMFORGE_BUILD_BENCHMARKS "Build LSMForge benchmarks" ON)
option(LSMFORGE_BUILD_CLI "Build LSMForge CLI tool" ON)
option(LSMFORGE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LSMFORGE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LSMFORGE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Compiler Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wno-unused-parameter
    )
endif()

# Sanitizers
if(LSMFORGE_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(LSMFORGE_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(LSMFORGE_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# Main Library Sources
set(LSMFORGE_PUBLIC_HEADERS
    include/lsmforge/slice.h
    include/lsmforge/status.h
    include/lsmforge/types.h
    include/lsmforge/options.h
    include/lsmforge/comparator.h
    include/lsmforge/iterator.h
    include/lsmforge/write_batch.h
    include/lsmforge/snapshot.h
    include/lsmforge/stats.h
    include/lsmforge/db.h
)

set(LSMFORGE_SOURCES
    src/util/crc32.cpp
    src/util/coding.cpp
    src/util/env_posix.cpp
    src/util/status.cpp
    src/util/arena.cpp
    src/wal/wal_writer.cpp
    src/wal/wal_reader.cpp
    src/memtable/memtable.cpp
    src/table/bloom_filter.cpp
    src/table/block_builder.cpp
    src/table/block.cpp
    src/table/table_builder.cpp
    src/table/table_reader.cpp
    src/table/two_level_iterator.cpp
    src/table/merging_iterator.cpp
    src/db/version_edit.cpp
    src/db/version_set.cpp
    src/db/compaction.cpp
    src/db/table_cache.cpp
    src/db/db_iter.cpp
    src/db/db_impl.cpp
    src/db/write_batch.cpp
)

add_library(lsmforge STATIC
    ${LSMFORGE_PUBLIC_HEADERS}
    ${LSMFORGE_SOURCES}
)

target_include_directories(lsmforge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(lsmforge PUBLIC Threads::Threads)

# CLI Executable
if(LSMFORGE_BUILD_CLI)
    add_executable(lsmforge_cli src/tools/cli.cpp)
    target_link_libraries(lsmforge_cli PRIVATE lsmforge)
    target_include_directories(lsmforge_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# Benchmarks
if(LSMFORGE_BUILD_BENCHMARKS)
    add_executable(lsmforge_bench
        benchmarks/benchmark_main.cpp
    )
    target_link_libraries(lsmforge_bench PRIVATE lsmforge)
    target_include_directories(lsmforge_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# HTTP REST Server
option(LSMFORGE_BUILD_SERVER "Build LSMForge HTTP REST Server" ON)
if(LSMFORGE_BUILD_SERVER)
    add_executable(lsmforge_server
        server/src/main.cpp
        server/src/http_server.cpp
    )
    target_link_libraries(lsmforge_server PRIVATE lsmforge Threads::Threads)
    target_include_directories(lsmforge_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/server/include
        ${CMAKE_CURRENT_SOURCE_DIR}/server/third_party
    )
endif()

# Tests
if(LSMFORGE_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Helper macro to add test
    macro(lsmforge_add_test test_name source_file)
        add_executable(${test_name} ${source_file})
        target_link_libraries(${test_name} PRIVATE lsmforge GTest::gtest_main)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        add_test(NAME ${test_name} COMMAND ${test_name})
    endmacro()

    # Unit Tests
    lsmforge_add_test(status_test tests/unit/status_test.cpp)
    lsmforge_add_test(slice_test tests/unit/slice_test.cpp)
    lsmforge_add_test(coding_test tests/unit/coding_test.cpp)
    lsmforge_add_test(crc32_test tests/unit/crc32_test.cpp)
    lsmforge_add_test(bloom_test tests/unit/bloom_test.cpp)
    lsmforge_add_test(skiplist_test tests/unit/skiplist_test.cpp)
    lsmforge_add_test(memtable_test tests/unit/memtable_test.cpp)
    lsmforge_add_test(wal_test tests/unit/wal_test.cpp)
    lsmforge_add_test(block_test tests/unit/block_test.cpp)
    lsmforge_add_test(table_test tests/unit/table_test.cpp)
    lsmforge_add_test(version_edit_test tests/unit/version_edit_test.cpp)
    lsmforge_add_test(version_set_test tests/unit/version_set_test.cpp)

    # Integration & Feature Tests
    lsmforge_add_test(db_basic_test tests/integration/db_basic_test.cpp)
    lsmforge_add_test(db_batch_test tests/integration/db_batch_test.cpp)
    lsmforge_add_test(db_iterator_test tests/integration/db_iterator_test.cpp)
    lsmforge_add_test(db_compaction_test tests/integration/db_compaction_test.cpp)
    lsmforge_add_test(db_recovery_test tests/recovery/db_recovery_test.cpp)
    lsmforge_add_test(db_fault_test tests/fault_injection/db_fault_test.cpp)
    lsmforge_add_test(db_concurrency_test tests/concurrency/db_concurrency_test.cpp)
endif()
