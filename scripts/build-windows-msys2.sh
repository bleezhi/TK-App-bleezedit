#!/usr/bin/env bash
# Run in the MSYS2 MINGW64 shell, not MSYS / UCRT64 / PowerShell.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then echo 'Open the MSYS2 MINGW64 shell first.' >&2; exit 1; fi
DEPS="${TK_DEPS_DIR:-$ROOT/.cache/deps}"
export TK_DEPS_DIR="$DEPS"
if [[ ! -f "${TK_PJ_PREFIX:-$DEPS/pj-install}/lib/pkgconfig/libpjproject.pc" ]]; then
    "$ROOT/scripts/build-pjsip.sh"
fi
export PKG_CONFIG_PATH="$(cygpath -m "${TK_PJ_PREFIX:-$DEPS/pj-install}/lib/pkgconfig");$(cygpath -m /mingw64/lib/pkgconfig)${PKG_CONFIG_PATH:+;$PKG_CONFIG_PATH}"
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DTK_BUILD_TESTS=ON -DQT_DIR=/mingw64/lib/cmake/Qt5
cmake --build "$ROOT/build" -j"${TK_JOBS:-2}"
"$ROOT/build/tk-unit-tests.exe"
# Loopback SIP tests require Python; do not access the production server.
"$ROOT/build/tk-sip-tests.exe"
QT_QPA_PLATFORM=offscreen "$ROOT/build/tk-ui-tests.exe"
OUTPUT="${TK_OUTPUT_DIR:-$ROOT/releases}"
STAGE="$OUTPUT/TK-App-Windows-x64"
mkdir -p "$STAGE"
cp "$ROOT/build/tk-app.exe" "$STAGE/"
windeployqt --release --no-translations --no-opengl-sw "$STAGE/tk-app.exe"
python3 "$ROOT/scripts/bundle-windows.py" "$STAGE" /mingw64 --objdump objdump
cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/THIRD_PARTY.md" "$ROOT/BUILDING.md" "$ROOT/TEST-REPORT.md" "$STAGE/"
mkdir -p "$STAGE/licenses"
cp -r "$ROOT/packaging/licenses/." "$STAGE/licenses/"
cp "$ROOT/packaging/branding/README.md" "$STAGE/licenses/TELEKOMUNIKACJA-KALISKA-LOGO.md"
printf '[Paths]\nPlugins=.\n' > "$STAGE/qt.conf"
(cd "$OUTPUT" && cmake -E tar cf TK-App-2.0.0-Windows-x64.zip --format=zip TK-App-Windows-x64)
echo "Ready: $OUTPUT/TK-App-2.0.0-Windows-x64.zip"
