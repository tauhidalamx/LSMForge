#include "http_server.h"
#include "db/filename.h"
#include "wal/wal_reader.h"
#include "util/coding.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

using json = nlohmann::json;

namespace lsmforge {

namespace {

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string GenerateRequestId() {
    static std::atomic<uint64_t> req_counter{1};
    return "req-" + std::to_string(NowMs()) + "-" + std::to_string(req_counter.fetch_add(1));
}

void SendJson(httplib::Response& res, const json& data, int status_code = 200) {
    json response;
    response["status"] = "ok";
    response["data"] = data;
    response["request_id"] = GenerateRequestId();
    response["timestamp"] = NowMs();
    res.status = status_code;
    res.set_content(response.dump(2), "application/json");
}

void SendError(httplib::Response& res, int http_status, const std::string& code, const std::string& message) {
    json response;
    response["status"] = "error";
    response["error"] = {
        {"code", code},
        {"message", message}
    };
    response["request_id"] = GenerateRequestId();
    response["timestamp"] = NowMs();
    res.status = http_status;
    res.set_content(response.dump(2), "application/json");
}

} // namespace

LSMForgeServer::LSMForgeServer(DB* db, std::string host, int port)
    : db_(db), host_(std::move(host)), port_(port), start_time_(std::chrono::steady_clock::now()) {
    svr_ = std::make_unique<httplib::Server>();
    SetupCors();
    SetupRoutes();
    RecordEvent("server_init", "LSMForge HTTP API server initialized on " + host_ + ":" + std::to_string(port_));
}

LSMForgeServer::~LSMForgeServer() {
    Stop();
}

bool LSMForgeServer::Start() {
    std::cout << "[LSMForgeServer] Listening on http://" << host_ << ":" << port_ << "\n";
    return svr_->listen(host_.c_str(), port_);
}

void LSMForgeServer::Stop() {
    if (svr_) {
        svr_->stop();
    }
}

void LSMForgeServer::RecordEvent(std::string type, std::string message,
                                 std::string component, std::string severity) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    EngineEvent ev;
    ev.id = "ev-" + std::to_string(NowMs()) + "-" + std::to_string(events_.size() + 1);
    ev.type = std::move(type);
    ev.message = std::move(message);
    ev.component = std::move(component);
    ev.severity = std::move(severity);
    ev.timestamp_ms = NowMs();

    events_.push_back(std::move(ev));
    if (events_.size() > kMaxEvents) {
        events_.pop_front();
    }
}

void LSMForgeServer::SetupCors() {
    svr_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, HEAD"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, Accept, X-Requested-With"}
    });

    // Handle preflight OPTIONS requests across all routes
    svr_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
}

void LSMForgeServer::SetupRoutes() {
    // -------------------------------------------------------------
    // 1. Health, System, Config & Stats
    // -------------------------------------------------------------
    auto handle_health = [this](const httplib::Request&, httplib::Response& res) {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        json j = {
            {"engine", "LSMForge"},
            {"version", "1.0.0"},
            {"uptime_seconds", uptime},
            {"database_open", db_ != nullptr},
            {"database_name", db_ ? db_->GetName() : ""}
        };
        SendJson(res, j);
    };

    svr_->Get("/health", handle_health);
    svr_->Get("/api/v1/health", handle_health);

    svr_->Get("/api/v1/version", [](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"engine", "LSMForge Storage Engine"},
            {"version", "1.0.0"},
            {"cxx_standard", "C++20"},
            {"compiler",
#if defined(__clang__)
             "Clang " __clang_version__
#elif defined(__GNUC__)
             "GCC " __VERSION__
#else
             "Unknown"
#endif
            },
            {"platform",
#if defined(__APPLE__)
             "macOS (Apple Silicon / Darwin)"
#elif defined(__linux__)
             "Linux"
#else
             "POSIX"
#endif
            },
            {"capabilities", {
                "wal_framing_crc32c",
                "lockfree_skiplist_memtable",
                "prefix_delta_sstable",
                "murmur3_bloom_filters",
                "leveled_compaction",
                "mvcc_snapshots",
                "atomic_write_batches",
                "crash_recovery"
            }}
        };
        SendJson(res, j);
    });

    svr_->Get("/api/v1/config", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        const auto& opt = db_->GetOptions();
        json j = {
            {"write_buffer_size", opt.write_buffer_size},
            {"max_open_files", opt.max_open_files},
            {"block_size", opt.block_size},
            {"block_restart_interval", opt.block_restart_interval},
            {"bloom_bits_per_key", opt.bloom_bits_per_key},
            {"paranoid_checks", opt.paranoid_checks},
            {"max_background_jobs", opt.max_background_jobs},
            {"level0_file_num_compaction_trigger", opt.level0_file_num_compaction_trigger},
            {"level0_slowdown_writes_trigger", opt.level0_slowdown_writes_trigger},
            {"level0_stop_writes_trigger", opt.level0_stop_writes_trigger},
            {"wal_sync_policy", opt.wal_sync_policy == WalSyncPolicy::kSyncAlways ? "sync_always" : "sync_periodic"}
        };
        SendJson(res, j);
    });

    svr_->Get("/api/v1/stats", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        DBStats s = db_->GetStats();
        auto mem_info = db_->GetMemTableInfo();

        json j = {
            {"operations", {
                {"puts", s.num_puts.load()},
                {"gets", s.num_gets.load()},
                {"deletes", s.num_deletes.load()},
                {"scans", s.num_scans.load()}
            }},
            {"storage", {
                {"wal_bytes_written", s.wal_bytes_written.load()},
                {"memtable_bytes", mem_info.active_bytes},
                {"memtable_threshold", mem_info.write_buffer_size},
                {"immutable_memtables", mem_info.immutable_count},
                {"active_sstables", s.sst_count.load()},
                {"total_sstable_bytes", s.sst_total_size.load()}
            }},
            {"bloom_filter", {
                {"useful_checks", s.bloom_filter_useful.load()},
                {"positive_checks", s.bloom_filter_positive.load()}
            }},
            {"compaction", {
                {"flushes", s.flush_count.load()},
                {"flush_duration_us", s.flush_duration_micros.load()},
                {"compactions", s.compaction_count.load()},
                {"compaction_duration_us", s.compaction_duration_micros.load()},
                {"bytes_read", s.compaction_bytes_read.load()},
                {"bytes_written", s.compaction_bytes_written.load()}
            }},
            {"errors", {
                {"write_errors", s.write_errors.load()},
                {"read_errors", s.read_errors.load()}
            }}
        };
        SendJson(res, j);
    });

    // -------------------------------------------------------------
    // 2. Key-Value CRUD Operations
    // -------------------------------------------------------------
    svr_->Get(R"(/api/v1/keys/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        std::string key = req.matches[1];
        std::string value;
        ReadOptions ropt;
        auto start = std::chrono::steady_clock::now();
        Status s = db_->Get(ropt, key, &value);
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (s.ok()) {
            json j = {
                {"key", key},
                {"value", value},
                {"found", true},
                {"latency_us", dur}
            };
            SendJson(res, j);
        } else if (s.IsNotFound()) {
            SendError(res, 404, "NOT_FOUND", "Key '" + key + "' was not found");
        } else {
            SendError(res, 500, "READ_ERROR", s.ToString());
        }
    });

    svr_->Put(R"(/api/v1/keys/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        std::string key = req.matches[1];
        std::string value;

        // Support JSON payload {"value": "..."} or raw text
        if (!req.body.empty() && req.body.front() == '{') {
            try {
                auto parsed = json::parse(req.body);
                if (parsed.contains("value") && parsed["value"].is_string()) {
                    value = parsed["value"].get<std::string>();
                } else {
                    value = req.body;
                }
            } catch (...) {
                value = req.body;
            }
        } else {
            value = req.body;
        }

        WriteOptions wopt;
        auto start = std::chrono::steady_clock::now();
        Status s = db_->Put(wopt, key, value);
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (s.ok()) {
            RecordEvent("put", "Put key: " + key + " (" + std::to_string(value.size()) + " bytes)");
            json j = {
                {"key", key},
                {"value_size", value.size()},
                {"written", true},
                {"latency_us", dur}
            };
            SendJson(res, j);
        } else {
            SendError(res, 500, "WRITE_ERROR", s.ToString());
        }
    });

    svr_->Delete(R"(/api/v1/keys/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        std::string key = req.matches[1];
        WriteOptions wopt;
        auto start = std::chrono::steady_clock::now();
        Status s = db_->Delete(wopt, key);
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (s.ok()) {
            RecordEvent("delete", "Deleted key (tombstone appended): " + key);
            json j = {
                {"key", key},
                {"deleted", true},
                {"latency_us", dur}
            };
            SendJson(res, j);
        } else {
            SendError(res, 500, "DELETE_ERROR", s.ToString());
        }
    });

    svr_->Get("/api/v1/scan", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        std::string start_key = req.has_param("start") ? req.get_param_value("start") : "";
        std::string end_key = req.has_param("end") ? req.get_param_value("end") : "";
        int limit = 50;
        if (req.has_param("limit")) {
            try {
                limit = std::clamp(std::stoi(req.get_param_value("limit")), 1, 1000);
            } catch (...) {}
        }

        auto start = std::chrono::steady_clock::now();
        ReadOptions ropt;
        std::unique_ptr<Iterator> it(db_->NewIterator(ropt));
        if (start_key.empty()) {
            it->SeekToFirst();
        } else {
            it->Seek(start_key);
        }

        json items = json::array();
        int count = 0;
        while (it->Valid() && count < limit) {
            std::string k = it->key().ToString();
            if (!end_key.empty() && k > end_key) {
                break;
            }
            items.push_back({
                {"key", k},
                {"value", it->value().ToString()}
            });
            count++;
            it->Next();
        }

        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();

        json j = {
            {"items", items},
            {"count", count},
            {"has_more", it->Valid() && (end_key.empty() || it->key().ToString() <= end_key)},
            {"latency_us", dur}
        };
        SendJson(res, j);
    });

    svr_->Post("/api/v1/batches", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        try {
            auto body = json::parse(req.body);
            if (!body.contains("operations") || !body["operations"].is_array()) {
                SendError(res, 400, "INVALID_ARGUMENT", "Expected 'operations' array in request body");
                return;
            }

            WriteBatch batch;
            int count = 0;
            for (const auto& op : body["operations"]) {
                std::string type = op.value("type", "put");
                std::string key = op.value("key", "");
                if (key.empty()) continue;

                if (type == "put") {
                    std::string value = op.value("value", "");
                    batch.Put(key, value);
                    count++;
                } else if (type == "delete") {
                    batch.Delete(key);
                    count++;
                }
            }

            WriteOptions wopt;
            if (body.contains("sync") && body["sync"].is_boolean()) {
                wopt.sync = body["sync"].get<bool>();
            }

            auto start = std::chrono::steady_clock::now();
            Status s = db_->Write(wopt, &batch);
            auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();

            if (s.ok()) {
                RecordEvent("write_batch", "Committed atomic batch with " + std::to_string(count) + " operations");
                json j = {
                    {"applied_count", count},
                    {"latency_us", dur}
                };
                SendJson(res, j);
            } else {
                SendError(res, 500, "BATCH_ERROR", s.ToString());
            }
        } catch (const std::exception& e) {
            SendError(res, 400, "MALFORMED_JSON", e.what());
        }
    });

    // -------------------------------------------------------------
    // 3. MemTable Endpoints
    // -------------------------------------------------------------
    auto handle_memtables = [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        auto info = db_->GetMemTableInfo();
        double usage_pct = (info.write_buffer_size > 0)
            ? (static_cast<double>(info.active_bytes) * 100.0 / info.write_buffer_size) : 0.0;
        json j = {
            {"active", {
                {"memory_bytes", info.active_bytes},
                {"threshold_bytes", info.write_buffer_size},
                {"usage_percent", std::clamp(usage_pct, 0.0, 100.0)},
                {"data_structure", "Lock-free SkipList (p=0.25, max_height=12)"}
            }},
            {"immutable", {
                {"count", info.immutable_count},
                {"flushing", info.immutable_count > 0}
            }}
        };
        SendJson(res, j);
    };

    svr_->Get("/api/v1/memtables", handle_memtables);
    svr_->Get("/api/v1/memtables/active", handle_memtables);
    svr_->Get("/api/v1/memtables/immutable", handle_memtables);

    svr_->Post("/api/v1/flush", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        auto start = std::chrono::steady_clock::now();
        Status s = db_->Flush(FlushOptions());
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (s.ok()) {
            RecordEvent("flush", "Manually triggered MemTable flush completed in " + std::to_string(dur) + " ms");
            SendJson(res, {{"flushed", true}, {"duration_ms", dur}});
        } else {
            SendError(res, 500, "FLUSH_ERROR", s.ToString());
        }
    });

    svr_->Get("/api/v1/flushes", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        DBStats s = db_->GetStats();
        json j = {
            {"total_flushes", s.flush_count.load()},
            {"total_duration_us", s.flush_duration_micros.load()},
            {"avg_duration_us", s.flush_count.load() ? (s.flush_duration_micros.load() / s.flush_count.load()) : 0}
        };
        SendJson(res, j);
    });

    // -------------------------------------------------------------
    // 4. WAL Endpoints
    // -------------------------------------------------------------
    svr_->Get("/api/v1/wal", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        auto logs = db_->GetWalFiles();
        json files = json::array();
        for (uint64_t num : logs) {
            std::string path = LogFileName(db_->GetName(), num);
            uint64_t size = 0;
            Env::Default()->GetFileSize(path, &size);
            files.push_back({
                {"file_number", num},
                {"filename", "wal/" + std::to_string(num) + ".log"},
                {"file_size", size},
                {"is_active", !logs.empty() && num == logs.back()}
            });
        }
        SendJson(res, {{"wal_files", files}, {"count", files.size()}});
    });

    svr_->Get(R"(/api/v1/wal/(\d+)/records)", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        uint64_t log_num = std::stoull(req.matches[1]);
        std::string fname = LogFileName(db_->GetName(), log_num);

        std::unique_ptr<SequentialFile> file;
        Status s = Env::Default()->NewSequentialFile(fname, &file);
        if (!s.ok()) {
            SendError(res, 404, "FILE_NOT_FOUND", "WAL file " + std::to_string(log_num) + " not found");
            return;
        }

        wal::WalReader reader(file.get(), nullptr, true /* verify_checksums */);
        Slice record;
        std::string scratch;
        json records = json::array();
        int limit = 100;
        int count = 0;

        while (reader.ReadRecord(&record, &scratch) && count < limit) {
            if (record.size() >= 12) {
                SequenceNumber seq = DecodeFixed64(record.data());
                uint32_t batch_count = DecodeFixed32(record.data() + 8);
                records.push_back({
                    {"record_index", count + 1},
                    {"sequence_start", seq},
                    {"batch_count", batch_count},
                    {"payload_size", record.size()},
                    {"checksum_valid", true}
                });
            } else {
                records.push_back({
                    {"record_index", count + 1},
                    {"payload_size", record.size()},
                    {"checksum_valid", true}
                });
            }
            count++;
        }

        SendJson(res, {
            {"file_number", log_num},
            {"records", records},
            {"count", count}
        });
    });

    // -------------------------------------------------------------
    // 5. SSTable Endpoints
    // -------------------------------------------------------------
    svr_->Get("/api/v1/sstables", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        auto tables = db_->GetTableList();
        json arr = json::array();
        for (const auto& t : tables) {
            arr.push_back({
                {"file_number", t.number},
                {"filename", "sst/" + std::to_string(t.number) + ".sst"},
                {"level", t.level},
                {"file_size", t.file_size},
                {"smallest_key", t.smallest},
                {"largest_key", t.largest}
            });
        }
        SendJson(res, {{"sstables", arr}, {"count", arr.size()}});
    });

    svr_->Get("/api/v1/levels", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        auto levels = db_->GetLevelSummary();
        json arr = json::array();
        uint64_t total_storage = 0;
        for (const auto& l : levels) {
            arr.push_back({
                {"level", l.level},
                {"num_files", l.num_files},
                {"total_bytes", l.total_bytes},
                {"is_l0", l.level == 0}
            });
            total_storage += l.total_bytes;
        }
        SendJson(res, {{"levels", arr}, {"total_storage_bytes", total_storage}});
    });

    // -------------------------------------------------------------
    // 6. Compaction Endpoints
    // -------------------------------------------------------------
    svr_->Post("/api/v1/compactions", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        RecordEvent("compaction_start", "Starting manual leveled compaction pass", "compaction");
        auto start = std::chrono::steady_clock::now();
        Status s = db_->Compact();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (s.ok()) {
            RecordEvent("compaction_finish", "Manual leveled compaction completed in " + std::to_string(dur) + " ms", "compaction");
            SendJson(res, {{"compacted", true}, {"duration_ms", dur}});
        } else {
            RecordEvent("compaction_error", s.ToString(), "compaction", "error");
            SendError(res, 500, "COMPACTION_ERROR", s.ToString());
        }
    });

    svr_->Get("/api/v1/compactions/stats", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        DBStats s = db_->GetStats();
        json j = {
            {"compactions_run", s.compaction_count.load()},
            {"total_duration_us", s.compaction_duration_micros.load()},
            {"bytes_read", s.compaction_bytes_read.load()},
            {"bytes_written", s.compaction_bytes_written.load()},
            {"write_amplification", s.compaction_bytes_read.load() > 0 ?
             (static_cast<double>(s.compaction_bytes_written.load()) / s.compaction_bytes_read.load()) : 1.0}
        };
        SendJson(res, j);
    });

    // -------------------------------------------------------------
    // 7. Recovery, Integrity & Events
    // -------------------------------------------------------------
    svr_->Get("/api/v1/integrity", [this](const httplib::Request&, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }
        std::string dbname = db_->GetName();
        bool current_exists = Env::Default()->FileExists(CurrentFileName(dbname));
        bool lock_exists = Env::Default()->FileExists(LockFileName(dbname));

        auto tables = db_->GetTableList();
        int valid_tables = 0;
        for (const auto& t : tables) {
            if (Env::Default()->FileExists(TableFileName(dbname, t.number))) {
                valid_tables++;
            }
        }

        json j = {
            {"status", "healthy"},
            {"current_file_present", current_exists},
            {"lock_file_active", lock_exists},
            {"registered_sstables", tables.size()},
            {"verified_sstables", valid_tables},
            {"integrity_pass", current_exists && valid_tables == static_cast<int>(tables.size())}
        };
        SendJson(res, j);
    });

    svr_->Get("/api/v1/events", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(events_mutex_);
        json arr = json::array();
        for (const auto& ev : events_) {
            arr.push_back({
                {"id", ev.id},
                {"type", ev.type},
                {"message", ev.message},
                {"component", ev.component},
                {"severity", ev.severity},
                {"timestamp", ev.timestamp_ms}
            });
        }
        SendJson(res, {{"events", arr}, {"count", arr.size()}});
    });

    // -------------------------------------------------------------
    // 8. Benchmarking Engine
    // -------------------------------------------------------------
    svr_->Post("/api/v1/benchmarks", [this](const httplib::Request& req, httplib::Response& res) {
        if (!db_) {
            SendError(res, 500, "DB_CLOSED", "Database is not open");
            return;
        }

        json body;
        try {
            body = req.body.empty() ? json::object() : json::parse(req.body);
        } catch (...) {
            SendError(res, 400, "MALFORMED_JSON", "Invalid JSON request body");
            return;
        }

        int num_ops = std::clamp(body.value("num_operations", 10000), 500, 100000);
        int val_size = std::clamp(body.value("value_size", 128), 16, 4096);
        std::string workload = body.value("workload", "mixed");
        double read_ratio = std::clamp(body.value("read_ratio", 0.8), 0.0, 1.0);
        bool sync_mode = body.value("sync", false);

        std::string job_id;
        {
            std::lock_guard<std::mutex> lock(bench_mutex_);
            job_id = "bench-" + std::to_string(next_bench_id_++);
            BenchmarkJob job;
            job.id = job_id;
            job.workload = workload;
            job.num_operations = num_ops;
            job.value_size = val_size;
            job.read_ratio = read_ratio;
            job.sync = sync_mode;
            job.state = "running";
            job.started_at_ms = NowMs();
            benchmarks_[job_id] = std::move(job);
        }

        RecordEvent("benchmark_start", "Dispatched benchmark job " + job_id + " (" + std::to_string(num_ops) + " ops, " + workload + ")", "benchmark");

        // Spawn asynchronous benchmark worker thread
        std::thread([this, job_id, num_ops, val_size, workload, read_ratio, sync_mode]() {
            std::string sample_value(val_size, 'x');
            std::mt19937_64 rng(1337);
            std::vector<uint64_t> latencies;
            latencies.reserve(num_ops);

            WriteOptions wopt;
            wopt.sync = sync_mode;
            ReadOptions ropt;

            auto t_start = std::chrono::steady_clock::now();

            for (int i = 0; i < num_ops; ++i) {
                uint64_t key_num = rng() % (num_ops * 2 + 1);
                std::string k = "bench_" + std::to_string(key_num);

                auto op_t1 = std::chrono::steady_clock::now();
                if (workload == "sequential_write") {
                    std::string seq_k = "bench_seq_" + std::to_string(i);
                    db_->Put(wopt, seq_k, sample_value);
                } else if (workload == "random_read") {
                    std::string val;
                    db_->Get(ropt, k, &val);
                } else if (workload == "random_update") {
                    db_->Put(wopt, k, sample_value);
                } else {
                    // Mixed
                    if ((rng() % 100) < static_cast<uint64_t>(read_ratio * 100)) {
                        std::string val;
                        db_->Get(ropt, k, &val);
                    } else {
                        db_->Put(wopt, k, sample_value);
                    }
                }
                auto op_t2 = std::chrono::steady_clock::now();
                latencies.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(op_t2 - op_t1).count());

                if (i % 2000 == 0 || i == num_ops - 1) {
                    std::lock_guard<std::mutex> lock(bench_mutex_);
                    benchmarks_[job_id].progress_percent = static_cast<int>((i + 1) * 100 / num_ops);
                }
            }

            auto t_end = std::chrono::steady_clock::now();
            double duration_sec = std::chrono::duration<double>(t_end - t_start).count();

            // Compute percentiles
            std::vector<uint64_t> sorted = latencies;
            std::sort(sorted.begin(), sorted.end());

            std::lock_guard<std::mutex> lock(bench_mutex_);
            auto& job = benchmarks_[job_id];
            job.state = "completed";
            job.progress_percent = 100;
            job.duration_sec = duration_sec;
            job.ops_per_sec = (duration_sec > 0) ? (num_ops / duration_sec) : 0;
            uint64_t total_bytes = static_cast<uint64_t>(num_ops) * (32 + val_size);
            job.mb_per_sec = (duration_sec > 0) ? ((total_bytes / (1024.0 * 1024.0)) / duration_sec) : 0;

            if (!sorted.empty()) {
                job.p50_us = sorted[sorted.size() * 50 / 100];
                job.p90_us = sorted[sorted.size() * 90 / 100];
                job.p95_us = sorted[sorted.size() * 95 / 100];
                job.p99_us = sorted[sorted.size() * 99 / 100];
            }
            job.completed_at_ms = NowMs();

            // Sample 200 evenly spaced latencies for graphing
            job.latency_samples_us.clear();
            int step = std::max(1, static_cast<int>(latencies.size() / 100));
            for (size_t s_idx = 0; s_idx < latencies.size(); s_idx += step) {
                job.latency_samples_us.push_back(latencies[s_idx]);
            }

            RecordEvent("benchmark_finish", "Job " + job_id + " finished: " +
                        std::to_string(static_cast<int>(job.ops_per_sec)) + " ops/sec, p95=" +
                        std::to_string(static_cast<int>(job.p95_us)) + " us", "benchmark");
        }).detach();

        SendJson(res, {{"job_id", job_id}, {"state", "running"}}, 202);
    });

    svr_->Get("/api/v1/benchmarks", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(bench_mutex_);
        json arr = json::array();
        for (const auto& [id, job] : benchmarks_) {
            arr.push_back({
                {"id", job.id},
                {"workload", job.workload},
                {"state", job.state},
                {"num_operations", job.num_operations},
                {"ops_per_sec", job.ops_per_sec},
                {"p50_us", job.p50_us},
                {"p95_us", job.p95_us},
                {"p99_us", job.p99_us},
                {"duration_sec", job.duration_sec},
                {"progress_percent", job.progress_percent}
            });
        }
        SendJson(res, {{"benchmarks", arr}, {"count", arr.size()}});
    });

    svr_->Get(R"(/api/v1/benchmarks/([a-zA-Z0-9\-_]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string job_id = req.matches[1];
        std::lock_guard<std::mutex> lock(bench_mutex_);
        auto it = benchmarks_.find(job_id);
        if (it == benchmarks_.end()) {
            SendError(res, 404, "JOB_NOT_FOUND", "Benchmark job " + job_id + " not found");
            return;
        }

        const auto& job = it->second;
        json j = {
            {"id", job.id},
            {"workload", job.workload},
            {"state", job.state},
            {"num_operations", job.num_operations},
            {"value_size", job.value_size},
            {"read_ratio", job.read_ratio},
            {"progress_percent", job.progress_percent},
            {"duration_sec", job.duration_sec},
            {"ops_per_sec", job.ops_per_sec},
            {"mb_per_sec", job.mb_per_sec},
            {"p50_us", job.p50_us},
            {"p90_us", job.p90_us},
            {"p95_us", job.p95_us},
            {"p99_us", job.p99_us},
            {"latency_samples_us", job.latency_samples_us},
            {"started_at", job.started_at_ms},
            {"completed_at", job.completed_at_ms}
        };
        SendJson(res, j);
    });

    // -------------------------------------------------------------
    // 9. Live Event Streaming (Server-Sent Events)
    // -------------------------------------------------------------
    svr_->Get("/api/v1/events/stream", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider("text/event-stream", [](size_t, httplib::DataSink& sink) {
            // Send initial connected event
            std::string msg = "event: connected\ndata: {\"status\":\"connected\",\"timestamp\":" +
                              std::to_string(NowMs()) + "}\n\n";
            sink.write(msg.data(), msg.size());

            // Loop and stream events or heartbeats
            for (int i = 0; i < 30; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                std::string hb = "event: heartbeat\ndata: {\"timestamp\":" + std::to_string(NowMs()) + "}\n\n";
                if (!sink.is_writable() || !sink.write(hb.data(), hb.size())) {
                    break;
                }
            }
            return false; // Close connection after stream window
        });
    });
}

} // namespace lsmforge
