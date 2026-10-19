#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DB_PATH="/tmp/lsmforge_dev_db"

echo "=========================================================="
echo " Starting LSMForge Full-Stack Platform in Development Mode"
echo "=========================================================="

# Build backend if binary not present
if [ ! -f "$ROOT_DIR/build_release/lsmforge_server" ]; then
    echo ">> Building C++20 storage engine & REST server..."
    "$SCRIPT_DIR/build.sh"
fi

# Detect free port for frontend (3000 or fallback to 3001)
FRONTEND_PORT=3000
if lsof -i :3000 >/dev/null 2>&1; then
    FRONTEND_PORT=3001
fi

# Trap to kill child processes cleanly
cleanup() {
    echo ""
    echo ">> Shutting down LSMForge services..."
    if [ -n "${SERVER_PID:-}" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
    fi
    if [ -n "${FRONTEND_PID:-}" ]; then
        kill "$FRONTEND_PID" 2>/dev/null || true
    fi
    exit 0
}
trap cleanup SIGINT SIGTERM EXIT

# Start C++ HTTP server
echo ">> Launching C++20 REST API Server on http://127.0.0.1:8080 (DB: $DB_PATH)..."
"$ROOT_DIR/build_release/lsmforge_server" --db "$DB_PATH" --host 127.0.0.1 --port 8080 &
SERVER_PID=$!

# Wait for server port to be active
sleep 1

# Start Next.js frontend
echo ">> Launching Next.js Observability Dashboard on http://localhost:$FRONTEND_PORT..."
cd "$ROOT_DIR/frontend"
PORT=$FRONTEND_PORT npm run dev &
FRONTEND_PID=$!

echo "=========================================================="
echo " LSMForge is Running!"
echo " - Dashboard:  http://localhost:$FRONTEND_PORT"
echo " - REST API:   http://127.0.0.1:8080"
echo " - Storage:    $DB_PATH"
echo " Press Ctrl+C to terminate all services."
echo "=========================================================="

wait
