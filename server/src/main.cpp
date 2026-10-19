#include "http_server.h"
#include "lsmforge/db.h"
#include <csignal>
#include <iostream>
#include <memory>

static std::unique_ptr<lsmforge::LSMForgeServer> g_server;
static lsmforge::DB* g_db = nullptr;

void SignalHandler(int signum) {
    std::cout << "\n[LSMForgeServer] Caught signal " << signum << ", shutting down gracefully...\n";
    if (g_server) {
        g_server->Stop();
    }
}

int main(int argc, char** argv) {
    std::string dbpath = "./lsmforge_db";
    std::string host = "127.0.0.1";
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--db" && i + 1 < argc) {
            dbpath = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: lsmforge_server [--db PATH] [--host HOST] [--port PORT]\n"
                      << "  --db PATH     Path to database directory (default: ./lsmforge_db)\n"
                      << "  --host HOST   Binding host address (default: 127.0.0.1)\n"
                      << "  --port PORT   HTTP port (default: 8080)\n";
            return 0;
        } else if (arg[0] != '-') {
            dbpath = arg;
        }
    }

    std::cout << "===============================================================\n"
              << "       LSMForge — Full-Stack Storage Engine Backend API        \n"
              << "===============================================================\n"
              << "Database path: " << dbpath << "\n"
              << "Binding:       http://" << host << ":" << port << "\n"
              << "API Base:      http://" << host << ":" << port << "/api/v1\n\n";

    lsmforge::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 4 * 1024 * 1024; // 4 MB
    options.bloom_bits_per_key = 10;

    lsmforge::Status s = lsmforge::DB::Open(options, dbpath, &g_db);
    if (!s.ok()) {
        std::cerr << "CRITICAL: Failed to open database at " << dbpath << ": " << s.ToString() << "\n";
        return 1;
    }

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    g_server = std::make_unique<lsmforge::LSMForgeServer>(g_db, host, port);
    bool ok = g_server->Start();

    std::cout << "[LSMForgeServer] Closing database...\n";
    delete g_db;
    g_db = nullptr;
    std::cout << "[LSMForgeServer] Shutdown complete.\n";

    return ok ? 0 : 1;
}
