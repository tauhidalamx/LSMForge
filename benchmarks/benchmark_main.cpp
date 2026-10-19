#include "lsmforge/db.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace lsmforge;

namespace {

struct BenchmarkStats {
    std::string name;
    int num_ops{0};
    double duration_sec{0.0};
    double ops_per_sec{0.0};
    double mb_per_sec{0.0};
    double p50_us{0.0};
    double p95_us{0.0};
    double p99_us{0.0};

    void Print() const {
        std::cout << std::left << std::setw(20) << name
                  << std::right << std::setw(10) << num_ops << " ops  "
                  << std::fixed << std::setprecision(3) << std::setw(8) << duration_sec << " s  "
                  << std::fixed << std::setprecision(1) << std::setw(10) << ops_per_sec << " ops/sec  "
                  << std::fixed << std::setprecision(2) << std::setw(8) << mb_per_sec << " MB/s  "
                  << "[p50: " << std::fixed << std::setprecision(1) << p50_us << "us, "
                  << "p95: " << p95_us << "us, "
                  << "p99: " << p99_us << "us]\n";
    }
};

BenchmarkStats ComputeStats(const std::string& name, int n,
                            const std::vector<uint64_t>& latencies,
                            double elapsed_sec, uint64_t total_bytes) {
    BenchmarkStats s;
    s.name = name;
    s.num_ops = n;
    s.duration_sec = elapsed_sec;
    s.ops_per_sec = (elapsed_sec > 0) ? (n / elapsed_sec) : 0;
    s.mb_per_sec = (elapsed_sec > 0) ? ((total_bytes / (1024.0 * 1024.0)) / elapsed_sec) : 0;

    std::vector<uint64_t> sorted = latencies;
    std::sort(sorted.begin(), sorted.end());
    if (!sorted.empty()) {
        s.p50_us = sorted[sorted.size() * 50 / 100];
        s.p95_us = sorted[sorted.size() * 95 / 100];
        s.p99_us = sorted[sorted.size() * 99 / 100];
    }
    return s;
}

std::string FormatKey(uint64_t num) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "user_key_%016llu", static_cast<unsigned long long>(num));
    return buf;
}

std::string GenerateValue(size_t size, std::mt19937& rng) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string val(size, 'x');
    for (size_t i = 0; i < size; ++i) {
        val[i] = charset[rng() % (sizeof(charset) - 1)];
    }
    return val;
}

} // namespace

int main(int argc, char** argv) {
    int num_ops = 50000;
    size_t val_size = 128;
    bool sync = false;
    std::string dbpath = "./lsmforge_benchmark_db";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--num=", 0) == 0) {
            num_ops = std::stoi(arg.substr(6));
        } else if (arg.rfind("--val_size=", 0) == 0) {
            val_size = std::stoul(arg.substr(11));
        } else if (arg.rfind("--sync=", 0) == 0) {
            sync = (std::stoi(arg.substr(7)) != 0);
        } else if (arg.rfind("--db=", 0) == 0) {
            dbpath = arg.substr(5);
        }
    }

    std::cout << "=========================================================================\n"
              << "       LSMForge — High Performance Storage Engine Benchmark Suite       \n"
              << "=========================================================================\n"
              << "Parameters: num_ops=" << num_ops << ", val_size=" << val_size
              << " B, sync=" << (sync ? "true" : "false") << ", db=" << dbpath << "\n\n";

    Options options;
    options.create_if_missing = true;
    options.error_if_exists = false;
    options.write_buffer_size = 4 * 1024 * 1024; // 4MB
    options.block_size = 4096;                   // 4KB
    options.bloom_bits_per_key = 10;             // 10 bits (~1% FP)
    options.wal_sync_policy = sync ? WalSyncPolicy::kSyncAlways : WalSyncPolicy::kNoSyncOnWrite;

    // Clean previous bench db
    DestroyDB(dbpath, options);

    DB* db = nullptr;
    Status s = DB::Open(options, dbpath, &db);
    if (!s.ok()) {
        std::cerr << "Failed to open benchmark DB: " << s.ToString() << "\n";
        return 1;
    }

    WriteOptions wopt;
    wopt.sync = sync;
    ReadOptions ropt;

    std::mt19937 rng(42); // Reproducible seed
    std::string value_sample = GenerateValue(val_size, rng);

    std::vector<uint64_t> latencies;
    latencies.reserve(num_ops);

    // 1. Sequential Write Benchmark
    {
        latencies.clear();
        uint64_t total_bytes = 0;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < num_ops; ++i) {
            std::string k = FormatKey(i);
            total_bytes += k.size() + value_sample.size();
            auto t1 = std::chrono::steady_clock::now();
            db->Put(wopt, k, value_sample);
            auto t2 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
        }
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        ComputeStats("Sequential Write", num_ops, latencies, elapsed, total_bytes).Print();
    }

    // 2. Random Read (Cache & MemTable hits)
    {
        latencies.clear();
        std::string val;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < num_ops; ++i) {
            uint64_t target = rng() % num_ops;
            std::string k = FormatKey(target);
            auto t1 = std::chrono::steady_clock::now();
            db->Get(ropt, k, &val);
            auto t2 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
        }
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        ComputeStats("Random Read", num_ops, latencies, elapsed, num_ops * (32 + val_size)).Print();
    }

    // 3. Random Updates
    {
        latencies.clear();
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < num_ops; ++i) {
            uint64_t target = rng() % num_ops;
            std::string k = FormatKey(target);
            auto t1 = std::chrono::steady_clock::now();
            db->Put(wopt, k, value_sample);
            auto t2 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
        }
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        ComputeStats("Random Updates", num_ops, latencies, elapsed, num_ops * (32 + val_size)).Print();
    }

    // 4. Mixed Workload (80% Read / 20% Write)
    {
        latencies.clear();
        std::string val;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < num_ops; ++i) {
            uint64_t target = rng() % (num_ops * 2);
            std::string k = FormatKey(target);
            auto t1 = std::chrono::steady_clock::now();
            if ((rng() % 100) < 80) {
                db->Get(ropt, k, &val);
            } else {
                db->Put(wopt, k, value_sample);
            }
            auto t2 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
        }
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        ComputeStats("Mixed 80R/20W", num_ops, latencies, elapsed, num_ops * (32 + val_size)).Print();
    }

    // 5. Sorted Range Scan
    {
        int num_scans = std::max(100, num_ops / 100);
        int scan_len = 50;
        latencies.clear();
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < num_scans; ++i) {
            uint64_t start_key = rng() % (num_ops > 100 ? num_ops - 100 : 1);
            std::string k = FormatKey(start_key);
            auto t1 = std::chrono::steady_clock::now();
            std::unique_ptr<Iterator> iter(db->NewIterator(ropt));
            iter->Seek(k);
            int count = 0;
            while (iter->Valid() && count < scan_len) {
                count++;
                iter->Next();
            }
            auto t2 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
        }
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        ComputeStats("Range Scan (50 items)", num_scans, latencies, elapsed, num_scans * scan_len * (32 + val_size)).Print();
    }

    // 6. Deletes
    {
        int num_deletes = num_ops / 5;
        latencies.clear();
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < num_deletes; ++i) {
            uint64_t target = rng() % num_ops;
            std::string k = FormatKey(target);
            auto t1 = std::chrono::steady_clock::now();
            db->Delete(wopt, k);
            auto t2 = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
        }
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        ComputeStats("Random Deletes", num_deletes, latencies, elapsed, num_deletes * 32).Print();
    }

    // 7. Flush and Leveled Compaction
    {
        std::cout << "\nTriggering Flush and Full Leveled Compaction...\n";
        auto start = std::chrono::steady_clock::now();
        db->Flush(FlushOptions());
        db->Compact();
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        std::cout << "Compaction completed in " << std::fixed << std::setprecision(3) << elapsed << " s.\n";
    }

    std::cout << "\n" << db->GetStats().ToString() << "\n";

    delete db;
    std::cout << "\nBenchmark completed successfully.\n";
    return 0;
}
