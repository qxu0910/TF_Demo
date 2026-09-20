#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
args=()
if [[ -f "$root/tmp/runtime-deps/httplib.h" && -f "$root/tmp/runtime-deps/json.hpp" ]]; then
    args+=("-DRUNTIME_DEPS_DIR=$root/tmp/runtime-deps")
fi
cmake -S "$root/runtime-cpp" -B "$root/runtime-cpp/build" "${args[@]}"
cmake --build "$root/runtime-cpp/build" --parallel 2
exec "$root/runtime-cpp/build/runtime_server" "${1:-8082}" "${2:-2}" "${3:-8}" "${4:-0}"
