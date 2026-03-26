#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2019–2026 Andy Curtis <contactandyc@gmail.com>
# SPDX-FileCopyrightText: 2024–2025 Knode.ai
# SPDX-License-Identifier: Apache-2.0
#
# Maintainer: Andy Curtis <contactandyc@gmail.com>

set -euo pipefail

# --- Discover and source .scaffoldrc ---
_cur="$PWD"
while [ "$_cur" != "/" ]; do
  if [ -f "$_cur/.scaffoldrc" ]; then
    source "$_cur/.scaffoldrc"
    break
  fi
  _cur="$(dirname "$_cur")"
done
[ -z "${WORKSPACE_DIR:-}" ] && [ -f "$HOME/.scaffoldrc" ] && source "$HOME/.scaffoldrc"

: "${BUILD_DIR:=build}"

# --- Knobs ---
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
BUILD_VARIANT="${BUILD_VARIANT:-debug}"
PREFIX="${PREFIX:-/usr/local}"

rm -rf "${BUILD_DIR}"

cmake -S . -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_PREFIX_PATH="$PREFIX" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DA_BUILD_VARIANT="$BUILD_VARIANT" \
  "$@"

cmake --build "${BUILD_DIR}" -j"$( (command -v nproc >/dev/null && nproc) || (sysctl -n hw.ncpu) || echo 4 )"

echo
echo "✅ Built app binaries in '${BUILD_DIR}':"
find "${BUILD_DIR}" -maxdepth 1 -type f -perm +111 -print 2>/dev/null || true
