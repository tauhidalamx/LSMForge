#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace lsmforge {

struct DBStats {
    std::atomic<uint64_t> num_puts{0};
    std::atomic<uint64_t> num_gets{0};
    std::atomic<uint64_t> num_deletes{0};
    std::atomic<uint64_t> num_scans{0};

    std::atomic<uint64_t> wal_bytes_written{0};
    std::atomic<uint64_t> sst_count{0};
    std::atomic<uint64_t> sst_total_size{0};
    std::atomic<uint64_t> memtable_bytes{0};

    std::atomic<uint64_t> bloom_filter_useful{0}; // Bloom filter avoided disk read (true negative)
    std::atomic<uint64_t> bloom_filter_positive{0}; // Bloom filter reported key might exist
    std::atomic<uint64_t> data_block_reads{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};

    std::atomic<uint64_t> flush_count{0};
    std::atomic<uint64_t> flush_duration_micros{0};
    std::atomic<uint64_t> compaction_count{0};
    std::atomic<uint64_t> compaction_duration_micros{0};
    std::atomic<uint64_t> compaction_bytes_read{0};
    std::atomic<uint64_t> compaction_bytes_written{0};

    std::atomic<uint64_t> recovery_duration_micros{0};
    std::atomic<uint64_t> write_errors{0};
    std::atomic<uint64_t> read_errors{0};

    DBStats() = default;

    DBStats(const DBStats& o) {
        num_puts.store(o.num_puts.load(std::memory_order_relaxed), std::memory_order_relaxed);
        num_gets.store(o.num_gets.load(std::memory_order_relaxed), std::memory_order_relaxed);
        num_deletes.store(o.num_deletes.load(std::memory_order_relaxed), std::memory_order_relaxed);
        num_scans.store(o.num_scans.load(std::memory_order_relaxed), std::memory_order_relaxed);
        wal_bytes_written.store(o.wal_bytes_written.load(std::memory_order_relaxed), std::memory_order_relaxed);
        sst_count.store(o.sst_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
        sst_total_size.store(o.sst_total_size.load(std::memory_order_relaxed), std::memory_order_relaxed);
        memtable_bytes.store(o.memtable_bytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
        bloom_filter_useful.store(o.bloom_filter_useful.load(std::memory_order_relaxed), std::memory_order_relaxed);
        bloom_filter_positive.store(o.bloom_filter_positive.load(std::memory_order_relaxed), std::memory_order_relaxed);
        data_block_reads.store(o.data_block_reads.load(std::memory_order_relaxed), std::memory_order_relaxed);
        cache_hits.store(o.cache_hits.load(std::memory_order_relaxed), std::memory_order_relaxed);
        cache_misses.store(o.cache_misses.load(std::memory_order_relaxed), std::memory_order_relaxed);
        flush_count.store(o.flush_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
        flush_duration_micros.store(o.flush_duration_micros.load(std::memory_order_relaxed), std::memory_order_relaxed);
        compaction_count.store(o.compaction_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
        compaction_duration_micros.store(o.compaction_duration_micros.load(std::memory_order_relaxed), std::memory_order_relaxed);
        compaction_bytes_read.store(o.compaction_bytes_read.load(std::memory_order_relaxed), std::memory_order_relaxed);
        compaction_bytes_written.store(o.compaction_bytes_written.load(std::memory_order_relaxed), std::memory_order_relaxed);
        recovery_duration_micros.store(o.recovery_duration_micros.load(std::memory_order_relaxed), std::memory_order_relaxed);
        write_errors.store(o.write_errors.load(std::memory_order_relaxed), std::memory_order_relaxed);
        read_errors.store(o.read_errors.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    DBStats& operator=(const DBStats& o) {
        if (this != &o) {
            num_puts.store(o.num_puts.load(std::memory_order_relaxed), std::memory_order_relaxed);
            num_gets.store(o.num_gets.load(std::memory_order_relaxed), std::memory_order_relaxed);
            num_deletes.store(o.num_deletes.load(std::memory_order_relaxed), std::memory_order_relaxed);
            num_scans.store(o.num_scans.load(std::memory_order_relaxed), std::memory_order_relaxed);
            wal_bytes_written.store(o.wal_bytes_written.load(std::memory_order_relaxed), std::memory_order_relaxed);
            sst_count.store(o.sst_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            sst_total_size.store(o.sst_total_size.load(std::memory_order_relaxed), std::memory_order_relaxed);
            memtable_bytes.store(o.memtable_bytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
            bloom_filter_useful.store(o.bloom_filter_useful.load(std::memory_order_relaxed), std::memory_order_relaxed);
            bloom_filter_positive.store(o.bloom_filter_positive.load(std::memory_order_relaxed), std::memory_order_relaxed);
            data_block_reads.store(o.data_block_reads.load(std::memory_order_relaxed), std::memory_order_relaxed);
            cache_hits.store(o.cache_hits.load(std::memory_order_relaxed), std::memory_order_relaxed);
            cache_misses.store(o.cache_misses.load(std::memory_order_relaxed), std::memory_order_relaxed);
            flush_count.store(o.flush_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            flush_duration_micros.store(o.flush_duration_micros.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compaction_count.store(o.compaction_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compaction_duration_micros.store(o.compaction_duration_micros.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compaction_bytes_read.store(o.compaction_bytes_read.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compaction_bytes_written.store(o.compaction_bytes_written.load(std::memory_order_relaxed), std::memory_order_relaxed);
            recovery_duration_micros.store(o.recovery_duration_micros.load(std::memory_order_relaxed), std::memory_order_relaxed);
            write_errors.store(o.write_errors.load(std::memory_order_relaxed), std::memory_order_relaxed);
            read_errors.store(o.read_errors.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    // Reset all metrics
    void Reset() {
        num_puts.store(0, std::memory_order_relaxed);
        num_gets.store(0, std::memory_order_relaxed);
        num_deletes.store(0, std::memory_order_relaxed);
        num_scans.store(0, std::memory_order_relaxed);
        wal_bytes_written.store(0, std::memory_order_relaxed);
        sst_count.store(0, std::memory_order_relaxed);
        sst_total_size.store(0, std::memory_order_relaxed);
        memtable_bytes.store(0, std::memory_order_relaxed);
        bloom_filter_useful.store(0, std::memory_order_relaxed);
        bloom_filter_positive.store(0, std::memory_order_relaxed);
        data_block_reads.store(0, std::memory_order_relaxed);
        cache_hits.store(0, std::memory_order_relaxed);
        cache_misses.store(0, std::memory_order_relaxed);
        flush_count.store(0, std::memory_order_relaxed);
        flush_duration_micros.store(0, std::memory_order_relaxed);
        compaction_count.store(0, std::memory_order_relaxed);
        compaction_duration_micros.store(0, std::memory_order_relaxed);
        compaction_bytes_read.store(0, std::memory_order_relaxed);
        compaction_bytes_written.store(0, std::memory_order_relaxed);
        recovery_duration_micros.store(0, std::memory_order_relaxed);
        write_errors.store(0, std::memory_order_relaxed);
        read_errors.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] std::string ToString() const;
};

} // namespace lsmforge
