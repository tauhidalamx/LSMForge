#include "lsmforge/db.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace lsmforge;

namespace {

void PrintHelp() {
    std::cout << "LSMForge Interactive Database Shell\n"
              << "Usage: [command] [args...]\n\n"
              << "Available Commands:\n"
              << "  put <key> <value>      Store key-value pair\n"
              << "  get <key>              Retrieve value for key\n"
              << "  delete <key>           Delete key\n"
              << "  scan <start> <end>     Scan keys in range [start, end]\n"
              << "  batch <file>           Execute batch script from file\n"
              << "  flush                  Flush MemTable to L0 SSTable\n"
              << "  compact                Run full leveled compaction\n"
              << "  stats                  Display engine performance statistics\n"
              << "  check <key>            Verify if key exists\n"
              << "  benchmark <num_ops>    Quick micro-benchmark\n"
              << "  help                   Show this help menu\n"
              << "  exit / quit            Exit shell\n";
}

bool ExecuteCommand(DB* db, const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd)) return true;

    if (cmd == "exit" || cmd == "quit") {
        return false;
    } else if (cmd == "help") {
        PrintHelp();
    } else if (cmd == "put") {
        std::string key, value;
        if (iss >> key >> value) {
            WriteOptions opt;
            Status s = db->Put(opt, key, value);
            if (s.ok()) {
                std::cout << "OK\n";
            } else {
                std::cerr << "Error: " << s.ToString() << "\n";
            }
        } else {
            std::cerr << "Usage: put <key> <value>\n";
        }
    } else if (cmd == "get") {
        std::string key;
        if (iss >> key) {
            ReadOptions opt;
            std::string value;
            Status s = db->Get(opt, key, &value);
            if (s.ok()) {
                std::cout << value << "\n";
            } else if (s.IsNotFound()) {
                std::cout << "(nil)\n";
            } else {
                std::cerr << "Error: " << s.ToString() << "\n";
            }
        } else {
            std::cerr << "Usage: get <key>\n";
        }
    } else if (cmd == "delete") {
        std::string key;
        if (iss >> key) {
            WriteOptions opt;
            Status s = db->Delete(opt, key);
            if (s.ok()) {
                std::cout << "OK\n";
            } else {
                std::cerr << "Error: " << s.ToString() << "\n";
            }
        } else {
            std::cerr << "Usage: delete <key>\n";
        }
    } else if (cmd == "scan") {
        std::string start_key, end_key;
        if (iss >> start_key >> end_key) {
            ReadOptions opt;
            std::unique_ptr<Iterator> iter(db->NewIterator(opt));
            iter->Seek(start_key);
            int count = 0;
            while (iter->Valid() && iter->key() <= end_key) {
                std::cout << iter->key().ToString() << " => "
                          << iter->value().ToString() << "\n";
                count++;
                iter->Next();
            }
            std::cout << "Total: " << count << " entries.\n";
        } else {
            std::cerr << "Usage: scan <start_key> <end_key>\n";
        }
    } else if (cmd == "flush") {
        Status s = db->Flush(FlushOptions());
        if (s.ok()) {
            std::cout << "Flushed successfully.\n";
        } else {
            std::cerr << "Flush error: " << s.ToString() << "\n";
        }
    } else if (cmd == "compact") {
        std::cout << "Compacting database...\n";
        Status s = db->Compact();
        if (s.ok()) {
            std::cout << "Compaction complete.\n";
        } else {
            std::cerr << "Compaction error: " << s.ToString() << "\n";
        }
    } else if (cmd == "stats") {
        DBStats stats = db->GetStats();
        std::cout << stats.ToString() << "\n";
    } else if (cmd == "check") {
        std::string key;
        if (iss >> key) {
            ReadOptions opt;
            std::string val;
            Status s = db->Get(opt, key, &val);
            if (s.ok()) {
                std::cout << "EXISTS (size: " << val.size() << " bytes)\n";
            } else if (s.IsNotFound()) {
                std::cout << "DOES NOT EXIST\n";
            } else {
                std::cerr << "Error: " << s.ToString() << "\n";
            }
        } else {
            std::cerr << "Usage: check <key>\n";
        }
    } else if (cmd == "batch") {
        std::string filename;
        if (iss >> filename) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Cannot open batch file: " << filename << "\n";
            } else {
                std::string file_line;
                WriteBatch batch;
                while (std::getline(file, file_line)) {
                    std::istringstream f_iss(file_line);
                    std::string op;
                    if (f_iss >> op) {
                        if (op == "put") {
                            std::string k, v;
                            if (f_iss >> k >> v) batch.Put(k, v);
                        } else if (op == "delete") {
                            std::string k;
                            if (f_iss >> k) batch.Delete(k);
                        }
                    }
                }
                WriteOptions opt;
                Status s = db->Write(opt, &batch);
                if (s.ok()) {
                    std::cout << "Executed batch of " << batch.Count() << " operations.\n";
                } else {
                    std::cerr << "Batch execution failed: " << s.ToString() << "\n";
                }
            }
        } else {
            std::cerr << "Usage: batch <filename>\n";
        }
    } else if (cmd == "benchmark") {
        int num_ops = 10000;
        iss >> num_ops;
        if (num_ops <= 0) num_ops = 10000;
        std::cout << "Running microbenchmark with " << num_ops << " operations...\n";
        auto start = std::chrono::steady_clock::now();
        WriteOptions wopt;
        for (int i = 0; i < num_ops; ++i) {
            std::string k = "bench_key_" + std::to_string(i);
            std::string v = "bench_val_" + std::to_string(i);
            db->Put(wopt, k, v);
        }
        auto mid = std::chrono::steady_clock::now();
        ReadOptions ropt;
        std::string val;
        for (int i = 0; i < num_ops; ++i) {
            std::string k = "bench_key_" + std::to_string(i);
            db->Get(ropt, k, &val);
        }
        auto end = std::chrono::steady_clock::now();
        auto write_dur = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start).count();
        auto read_dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid).count();
        std::cout << "Writes: " << num_ops << " ops in " << write_dur << " ms ("
                  << (num_ops * 1000.0 / (write_dur ? write_dur : 1)) << " ops/sec)\n";
        std::cout << "Reads:  " << num_ops << " ops in " << read_dur << " ms ("
                  << (num_ops * 1000.0 / (read_dur ? read_dur : 1)) << " ops/sec)\n";
    } else {
        std::cerr << "Unknown command: " << cmd << ". Type 'help' for instructions.\n";
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "LSMForge Storage Engine CLI\n"
                  << "Usage:\n"
                  << "  lsmforge_cli [db_path]                  (Interactive Shell)\n"
                  << "  lsmforge_cli [db_path] <command> [args] (One-shot Command)\n\n";
        PrintHelp();
        return 0;
    }

    std::string dbpath = "./lsmforge_db";
    int cmd_start = 1;
    if (argc > 1 && argv[1][0] != '-') {
        dbpath = argv[1];
        cmd_start = 2;
    }

    Options options;
    options.create_if_missing = true;
    DB* db = nullptr;
    Status s = DB::Open(options, dbpath, &db);
    if (!s.ok()) {
        std::cerr << "Failed to open database at " << dbpath << ": " << s.ToString() << "\n";
        return 1;
    }

    if (argc > cmd_start) {
        // One-shot command execution
        std::string command_line;
        for (int i = cmd_start; i < argc; ++i) {
            if (i > cmd_start) command_line += " ";
            command_line += argv[i];
        }
        ExecuteCommand(db, command_line);
        delete db;
        return 0;
    }

    std::cout << "LSMForge Database Engine opened successfully at: " << dbpath << "\n";
    std::cout << "Type 'help' for commands, 'exit' to quit.\n";

    std::string line;
    std::cout << "lsmforge> " << std::flush;
    while (std::getline(std::cin, line)) {
        if (!ExecuteCommand(db, line)) {
            break;
        }
        std::cout << "lsmforge> " << std::flush;
    }

    delete db;
    std::cout << "Database closed cleanly.\n";
    return 0;
}
