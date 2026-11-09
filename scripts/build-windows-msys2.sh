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

# Pass an absolute Windows path: QProcess launches the local SIP peer as a
# native Windows child process, so relying on the MSYS shell's python3 alias
# can fail even when Python is installed in the MSYS2 environment.
PYTHON_EXE="$(command -v python || command -v python3)"
if [[ -z "$PYTHON_EXE" ]]; then echo "Python interpreter not found in MSYS2 PATH." >&2; exit 1; fi
export TK_TEST_PYTHON="$(cygpath -w "$PYTHON_EXE")"
echo "Running Windows unit tests..."
"$ROOT/build/tk-unit-tests.exe" -v1
echo "Running Windows local SIP loopback tests with $TK_TEST_PYTHON..."
"$ROOT/build/tk-sip-tests.exe" -v2 -o -,txt
echo "Running Windows UI tests..."
QT_QPA_PLATFORM=offscreen "$ROOT/build/tk-ui-tests.exe" -v1
echo "All Windows tests passed."
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
VERSION="$(python3 - "$ROOT/CMakeLists.txt" <<'PY'
import pathlib,re,sys
m=re.search(r'project\s*\(\s*TKApp\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\b',pathlib.Path(sys.argv[1]).read_text(),re.I)
if not m: sys.exit('Cannot read version from CMakeLists.txt.')
print(m.group(1))
PY
)"
(cd "$OUTPUT" && cmake -E tar cf "TK-App-$VERSION-Windows-x64.zip" --format=zip TK-App-Windows-x64)
"$ROOT/scripts/package-windows-installer.sh" "$STAGE" "$OUTPUT"
echo "Ready: $OUTPUT/TK-App-$VERSION-Windows-x64.zip"
