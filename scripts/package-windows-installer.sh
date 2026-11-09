#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="${1:-$ROOT/releases/TK-App-Windows-x64}"
OUTPUT="${2:-$ROOT/releases}"
if command -v iscc >/dev/null 2>&1; then
    COMPILER="$(command -v iscc)"
elif command -v ISCC.exe >/dev/null 2>&1; then
    COMPILER="$(command -v ISCC.exe)"
elif [[ -x "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" ]]; then
    COMPILER="/c/Program Files (x86)/Inno Setup 6/ISCC.exe"
elif [[ -x "/c/Program Files/Inno Setup 6/ISCC.exe" ]]; then
    COMPILER="/c/Program Files/Inno Setup 6/ISCC.exe"
else
    echo "Nie znaleziono Inno Setup Compiler (ISCC)." >&2
    exit 1
}
[[ -d "$STAGE" ]] || { echo "Nie znaleziono katalogu aplikacji: $STAGE" >&2; exit 1; }
VERSION="$(python3 - "$ROOT/CMakeLists.txt" <<'PY'
import pathlib,re,sys
m=re.search(r'project\s*\(\s*TKApp\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\b',pathlib.Path(sys.argv[1]).read_text(),re.I)
if not m: sys.exit('Cannot read version from CMakeLists.txt.')
print(m.group(1))
PY
)"
mkdir -p "$OUTPUT"
STAGE_WIN="$(cygpath -w "$(cd "$STAGE" && pwd)")"
ROOT_WIN="$(cygpath -w "$ROOT")"
OUTPUT_WIN="$(cygpath -w "$(cd "$OUTPUT" && pwd)")"
"$COMPILER" "/DAppVersion=$VERSION" "/DStageDir=$STAGE_WIN" \
    "/O$OUTPUT_WIN" "$ROOT_WIN/packaging/windows/TK-App.iss"
INSTALLER="$OUTPUT/TK-App-$VERSION-Windows-x64-Setup.exe"
[[ -s "$INSTALLER" ]] || { echo "Inno Setup nie utworzył instalatora: $INSTALLER" >&2; exit 1; }
echo "Gotowy instalator: $INSTALLER"
