#pragma once

#include "lsmforge/comparator.h"
#include <cstddef>
#include <cstdint>

namespace lsmforge {

class Snapshot;

enum class WalSyncPolicy {
    kSyncAlways,       // Call fsync/F_FULLFSYNC on every write/batch
    kSyncOnThreshold,  // Sync periodically or at batch threshold
    kNoSyncOnWrite     // Rely on OS page cache flushing (highest throughput)
};

struct Options {
    // Comparator used to order user keys.
    // Default: BytewiseComparator()
    const Comparator* comparator{nullptr};

    // If true, the database will be created if it is missing.
    bool create_if_missing{true};

    // If true, an error is raised if the database already exists.
    bool error_if_exists{false};

    // If true, verify block checksums and fail on corruption.
    bool paranoid_checks{true};

    // Amount of data to build up in memory (in bytes) before converting
    // to a sorted on-disk file. Default: 4MB (4 * 1024 * 1024).
    size_t write_buffer_size{4 * 1024 * 1024};

    // Approximate size of user data packed per block in SSTables.
    // Default: 4KB (4096).
    size_t block_size{4096};

    // Target bits per key for Bloom Filter (10 bits -> ~1% false positive).
    // Set to 0 to disable Bloom filter. Default: 10.
    int bloom_bits_per_key{10};

    // Number of keys between restart points for SSTable block delta encoding.
    // Default: 16.
    int block_restart_interval{16};

    // Maximum number of open files that can be used by the DB.
    int max_open_files{1000};

    // WAL synchronization policy.
    WalSyncPolicy wal_sync_policy{WalSyncPolicy::kNoSyncOnWrite};

    // Number of Level-0 files that triggers background compaction.
    int level0_file_num_compaction_trigger{4};

    // Soft limit on number of Level-0 files. Writes slow down here.
    int level0_slowdown_writes_trigger{8};

    // Maximum number of Level-0 files. Writes stall completely here.
    int level0_stop_writes_trigger{12};

    // Base target bytes for Level 1. Each level grows by a factor of 10.
    // Default: 10MB.
    uint64_t max_bytes_for_level_base{10 * 1024 * 1024};

    // Multiplier for level target sizes.
    int max_bytes_for_level_multiplier{10};

    // Maximum number of background flush and compaction worker threads.
    int max_background_jobs{2};

    // Block cache capacity in bytes (0 means disabled). Default: 8MB.
    size_t block_cache_capacity_bytes{8 * 1024 * 1024};

    // Format version of LSMForge engine.
    uint32_t format_version{1};

    Options();
};

struct WriteOptions {
    // If true, the write will be flushed from the OS buffer cache before
    // the write is considered complete.
    bool sync{false};

    WriteOptions() = default;
};

struct ReadOptions {
    // If true, all data read from underlying storage will be verified against checksums.
    bool verify_checksums{true};

    // Should the data read be inserted into the block cache?
    bool fill_cache{true};

    // If "snapshot" is non-null, read as of the supplied snapshot.
    // If "snapshot" is null, read as of the DB state at the start of Read operation.
    const Snapshot* snapshot{nullptr};

    ReadOptions() = default;
};

struct FlushOptions {
    // If true, the Flush operation will wait until the flush is completed.
    bool wait{true};

    FlushOptions() = default;
};

struct CompactionOptions {
    // Maximum bytes for an output SSTable file.
    uint64_t max_output_file_size{2 * 1024 * 1024};

    CompactionOptions() = default;
};

} // namespace lsmforge
