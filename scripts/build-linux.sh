#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${TK_DEPS_DIR:-$ROOT/.cache/deps}"
export TK_DEPS_DIR="$DEPS"
if [[ ! -f "${TK_PJ_PREFIX:-$DEPS/pj-install}/lib/pkgconfig/libpjproject.pc" ]]; then
    "$ROOT/scripts/build-pjsip.sh"
fi
export PKG_CONFIG_PATH="${TK_PJ_PREFIX:-$DEPS/pj-install}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DTK_BUILD_TESTS=ON
cmake --build "$ROOT/build" -j"${TK_JOBS:-2}"
ctest --test-dir "$ROOT/build" --output-on-failure
OUTPUT="${TK_OUTPUT_DIR:-$ROOT/releases}"
"$ROOT/scripts/package-appimage.sh" "$ROOT/build/tk-app" "$OUTPUT"
"$ROOT/scripts/package-deb.sh" "$ROOT/build/tk-app" "$OUTPUT"
