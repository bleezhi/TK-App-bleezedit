#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${TK_DEPS_DIR:-$ROOT/.cache/deps}"
PREFIX="${TK_PJ_PREFIX:-$DEPS/pj-install}"
VERSION=2.17
mkdir -p "$DEPS"
ARCHIVE="$DEPS/pjproject-$VERSION.tar.gz"
if [[ ! -f "$ARCHIVE" ]]; then
    if [[ -f "$ROOT/vendor/pjproject-$VERSION.tar.gz" ]]; then
        cp "$ROOT/vendor/pjproject-$VERSION.tar.gz" "$ARCHIVE"
    else
        curl -fL --retry 3 "https://github.com/pjsip/pjproject/archive/refs/tags/$VERSION.tar.gz" -o "$ARCHIVE"
    fi
fi
echo "065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce  $ARCHIVE" | sha256sum -c -
[[ -d "$DEPS/pjproject-$VERSION" ]] || tar xzf "$ARCHIVE" -C "$DEPS"
cd "$DEPS/pjproject-$VERSION"
cat > pjlib/include/pj/config_site.h <<'CFG'
#define PJMEDIA_HAS_VIDEO 0
#define PJMEDIA_HAS_OPUS_CODEC 1
#define PJ_LOG_MAX_LEVEL 3
CFG
EXTRA=()
CONFIGURE_PREFIX="$PREFIX"
case "$(uname -s)" in
  MINGW*|MSYS*)
    cat >> pjlib/include/pj/config_site.h <<'CFG'
#define PJMEDIA_AUDIO_DEV_HAS_WMME 1
#define PJMEDIA_AUDIO_DEV_HAS_WASAPI 0
CFG
    EXTRA=(--disable-sound)
    CONFIGURE_PREFIX="$(cygpath -m "$PREFIX")"
    ;;
esac
./configure --prefix="$CONFIGURE_PREFIX" --disable-ssl --disable-video --disable-libyuv --disable-ffmpeg \
    --disable-v4l2 --disable-sdl --disable-openh264 --disable-libwebrtc --disable-speex-aec "${EXTRA[@]}"
make dep
make -j"${TK_JOBS:-2}"
make install
printf '\nPJSIP installed: %s\n' "$PREFIX"
