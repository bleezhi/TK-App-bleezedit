#!/usr/bin/env bash
# Package an ALREADY BUILT native Linux x86_64 executable. Fedora/Debian/Ubuntu.
set -Eeuo pipefail
if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'HELP'
Usage: bash scripts/package-appimage.sh [Linux executable] [output directory]
Defaults: build/tk-app and releases, relative to the project root.
Fedora prerequisites:
  sudo dnf install squashfs-tools patchelf python3 curl qt5-qtbase-devel alsa-plugins-pulseaudio
For a Qt6 executable use qt6-qtbase-devel instead of qt5-qtbase-devel.
Optional: TK_QMAKE=/path/to/native/qmake, TK_PACKAGE_WORK=/path/to/workdir,
          TK_APPIMAGE_RUNTIME=/path/to/runtime-x86_64, TK_JOBS=2
Output filename reads VERSION from CMakeLists.txt automatically.
This only packages the program. It does NOT compile or test it.
No Markdown files are required or included; upstream Markdown notices become .txt.
An AppImage built on a new Fedora is NOT guaranteed to run on older glibc.
HELP
    exit 0
fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for command in python3 ldd patchelf mksquashfs; do
    command -v "$command" >/dev/null || { echo "Missing command: $command" >&2; exit 1; }
done
BINARY="${1:-$ROOT/build/tk-app}"
[[ -f "$BINARY" ]] || { echo "Linux executable not found: $BINARY" >&2; echo 'Compile the Linux version first (not tk-app.exe).' >&2; exit 1; }
OUTPUT="${2:-$ROOT/releases}"
WORK="${TK_PACKAGE_WORK:-$ROOT/.cache/package}"
mkdir -p "$OUTPUT" "$WORK"
OUTPUT="$(cd "$OUTPUT" && pwd)"
WORK="$(cd "$WORK" && pwd)"
BINARY="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "$BINARY")"
VERSION="$(python3 - "$ROOT/CMakeLists.txt" <<'PY'
import pathlib,re,sys
text=pathlib.Path(sys.argv[1]).read_text()
match=re.search(r'project\s*\(\s*TKApp\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\b',text,re.I)
if not match:sys.exit('Cannot read TKApp VERSION from CMakeLists.txt.')
print(match.group(1))
PY
)"
APPDIR="$WORK/TK-App.AppDir"
case "$BINARY/" in "$APPDIR/"*) echo 'Input executable cannot be inside the packaging work AppDir.' >&2; exit 1;; esac
if [[ "$APPDIR" == "$ROOT" || "$ROOT/" == "$APPDIR/"* ]]; then
    echo 'Unsafe packaging workdir: it contains the source tree.' >&2; exit 1
fi
rm -rf -- "$APPDIR"
python3 "$ROOT/scripts/bundle-linux.py" "$BINARY" "$APPDIR"
python3 - "$APPDIR" <<'PY'
import pathlib,sys
root=pathlib.Path(sys.argv[1])
bad=[p for p in root.rglob('*') if p.name.lower().endswith('.md') and not p.is_dir()]
if bad:sys.exit('Refusing to package Markdown files: '+str(bad[0]))
print('Pre-compression check: no Markdown files.')
PY
DEFAULT_RUNTIME="$WORK/runtime-x86_64"
[[ ! -f "$ROOT/vendor/appimage-runtime-x86_64" ]] || DEFAULT_RUNTIME="$ROOT/vendor/appimage-runtime-x86_64"
RUNTIME="${TK_APPIMAGE_RUNTIME:-$DEFAULT_RUNTIME}"
if [[ ! -s "$RUNTIME" ]]; then
    command -v curl >/dev/null || { echo 'Missing curl to download the AppImage runtime.' >&2; exit 1; }
    curl -fL --retry 3 https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 -o "$RUNTIME"
fi
IMAGE="$OUTPUT/TK-App-$VERSION-x86_64.AppImage"
rm -f -- "$WORK/app.squashfs"
mksquashfs "$APPDIR" "$WORK/app.squashfs" -root-owned -noappend -comp zstd -Xcompression-level 19 -b 1M -processors "${TK_JOBS:-2}"
TEMP="$(mktemp "$OUTPUT/.TK-App-AppImage-XXXXXX")"
trap 'rm -f -- "${TEMP:-}"' EXIT
cat "$RUNTIME" "$WORK/app.squashfs" > "$TEMP"
chmod 0755 "$TEMP"
mv -f -- "$TEMP" "$IMAGE"
python3 - "$IMAGE" <<'PY'
import hashlib,pathlib,sys
path=pathlib.Path(sys.argv[1]);digest=hashlib.sha256()
with path.open('rb') as f:
    for chunk in iter(lambda:f.read(1024*1024),b''):digest.update(chunk)
path.with_suffix(path.suffix+'.sha256').write_text(digest.hexdigest()+'  '+path.name+'\n')
PY
printf '\nReady: %s\n' "$IMAGE"
echo 'Test this new AppImage before sharing. SIP/audio tests were not run by this script.'
