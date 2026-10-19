#pragma once

#include "lsmforge/db.h"
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace httplib {
class Server;
struct Request;
struct Response;
class DataSink;
}

namespace lsmforge {

struct EngineEvent {
    std::string id;
    std::string type;
    std::string message;
    std::string component;
    std::string severity; // info, warn, error
    uint64_t timestamp_ms{0};
};

struct BenchmarkJob {
    std::string id;
    std::string workload;
    int num_operations{0};
    int value_size{128};
    double read_ratio{0.8};
    bool sync{false};

    std::string state; // queued, running, completed, failed
    int progress_percent{0};
    double duration_sec{0.0};
    double ops_per_sec{0.0};
    double mb_per_sec{0.0};
    double p50_us{0.0};
    double p90_us{0.0};
    double p95_us{0.0};
    double p99_us{0.0};
    std::vector<double> latency_samples_us;
    std::string error_message;
    uint64_t started_at_ms{0};
    uint64_t completed_at_ms{0};
};

class LSMForgeServer {
public:
    LSMForgeServer(DB* db, std::string host = "127.0.0.1", int port = 8080);
    ~LSMForgeServer();

    // Start server (blocks until stopped or runs in thread)
    bool Start();
    void Stop();

    void RecordEvent(std::string type, std::string message,
                     std::string component = "engine", std::string severity = "info");

private:
    void SetupRoutes();
    void SetupCors();

    DB* db_;
    std::string host_;
    int port_;
    std::unique_ptr<httplib::Server> svr_;
    std::chrono::steady_clock::time_point start_time_;

    // Benchmark jobs
    std::mutex bench_mutex_;
    std::map<std::string, BenchmarkJob> benchmarks_;
    int next_bench_id_{1};

    // Events ring buffer
    std::mutex events_mutex_;
    std::deque<EngineEvent> events_;
    static constexpr size_t kMaxEvents = 500;
};

} // namespace lsmforge
