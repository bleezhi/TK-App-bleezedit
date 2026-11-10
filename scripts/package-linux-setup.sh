#!/usr/bin/env bash
# Build a portable Linux setup ZIP from a tested native x86_64 build.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="${1:-$ROOT/build/tk-app}"
OUTPUT="${2:-${TK_OUTPUT_DIR:-$ROOT/releases}}"
WORK="${TK_LINUX_SETUP_WORK:-$ROOT/.cache/linux-setup}"
for command in python3 ldd patchelf zip; do
    command -v "$command" >/dev/null || { echo "Missing required command: $command" >&2; exit 1; }
done
[[ -f "$BINARY" ]] || { echo "Linux executable not found: $BINARY" >&2; exit 1; }
mkdir -p "$OUTPUT" "$WORK"
OUTPUT="$(cd "$OUTPUT" && pwd)"
WORK="$(cd "$WORK" && pwd)"
BINARY="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "$BINARY")"
VERSION="$(python3 - "$ROOT/CMakeLists.txt" <<'PY'
import pathlib,re,sys
text=pathlib.Path(sys.argv[1]).read_text()
match=re.search(r'project\s*\(\s*TKApp\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\b',text,re.I)
if not match: sys.exit('Cannot read TKApp VERSION from CMakeLists.txt')
print(match.group(1))
PY
)"
APPDIR="$WORK/TK-App.AppDir"
STAGE="$WORK/TK-App-Linux-Setup"
ARCHIVE="$OUTPUT/TK-App-Linux-Setup-x86_64.zip"
rm -rf -- "$APPDIR" "$STAGE"
python3 "$ROOT/scripts/bundle-linux.py" "$BINARY" "$APPDIR"
mkdir -p "$STAGE/payload"
cp -a "$APPDIR/." "$STAGE/payload/"
install -m 0755 "$ROOT/packaging/linux-setup/setup.sh" "$STAGE/setup.sh"
install -m 0755 "$ROOT/packaging/linux-setup/uninstall.sh" "$STAGE/uninstall.sh"
install -m 0644 "$ROOT/packaging/linux-setup/README.txt" "$STAGE/README.txt"
install -m 0644 "$ROOT/LICENSE" "$STAGE/LICENSE"
cat > "$STAGE/VERSION.txt" <<VERSION
TK App $VERSION
Linux x86_64 portable setup bundle
VERSION
rm -f -- "$ARCHIVE" "$ARCHIVE.sha256"
python3 - "$STAGE" "$ARCHIVE" <<'PY'
import pathlib,sys,zipfile
root=pathlib.Path(sys.argv[1]); archive=pathlib.Path(sys.argv[2])
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for path in sorted(root.rglob('*')):
        if path.is_file():
            z.write(path, pathlib.Path(root.name)/path.relative_to(root))
with zipfile.ZipFile(archive) as z:
    bad=z.testzip()
    if bad: raise SystemExit('ZIP integrity check failed at '+bad)
    if not any(n.endswith('/payload/AppRun') for n in z.namelist()):
        raise SystemExit('ZIP is missing payload/AppRun')
print('ZIP integrity OK:', archive, 'files:', len(z.namelist()))
PY
sha256sum "$ARCHIVE" > "$ARCHIVE.sha256"
echo "Linux setup archive ready: $ARCHIVE"
