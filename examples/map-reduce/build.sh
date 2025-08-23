#!/usr/bin/env bash
set -euo pipefail

: "${BUILD_DIR:=build}"

rm -rf "${BUILD_DIR}"
cmake -S . -B "${BUILD_DIR}" "$@"
cmake --build "${BUILD_DIR}" -j"$( (command -v nproc >/dev/null && nproc) || (sysctl -n hw.ncpu) || echo 4 )"

echo
echo "✅ Built app binaries in '${BUILD_DIR}':"
find "${BUILD_DIR}" -maxdepth 1 -type f -perm +111 -print 2>/dev/null || true
