#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="${1:-$ROOT/build/tk-app}"
OUTPUT="${2:-$ROOT/releases}"
WORK="${TK_PACKAGE_WORK:-$ROOT/.cache/package}"
mkdir -p "$OUTPUT" "$WORK"
APPDIR="$WORK/TK-App.AppDir"
rm -rf "$APPDIR"
python3 "$ROOT/scripts/bundle-linux.py" "$BINARY" "$APPDIR"
DEFAULT_RUNTIME="$WORK/runtime-x86_64"
[[ ! -f "$ROOT/vendor/appimage-runtime-x86_64" ]] || DEFAULT_RUNTIME="$ROOT/vendor/appimage-runtime-x86_64"
RUNTIME="${TK_APPIMAGE_RUNTIME:-$DEFAULT_RUNTIME}"
if [[ ! -f "$RUNTIME" ]]; then
    curl -fL --retry 3 https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 -o "$RUNTIME"
fi
IMAGE="$OUTPUT/TK-App-2.0.0-x86_64.AppImage"
rm -f "$WORK/app.squashfs"
mksquashfs "$APPDIR" "$WORK/app.squashfs" -root-owned -noappend -comp zstd -Xcompression-level 19 -b 1M -processors "${TK_JOBS:-2}"
cat "$RUNTIME" "$WORK/app.squashfs" > "$IMAGE"
chmod +x "$IMAGE"
echo "Ready: $IMAGE"
