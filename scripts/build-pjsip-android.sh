#!/usr/bin/env bash
set -euo pipefail

PJSIP_VERSION="${PJSIP_VERSION:-2.15.1}"
PJSIP_DIR="${PJSIP_DIR:-$PWD/.pjsip-src}"
OUTPUT_DIR="${OUTPUT_DIR:-$PWD/android/app/build/pjsip}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT to an installed Android NDK}"
TARGET_ABI="${TARGET_ABI:-arm64-v8a}"

if [[ "$TARGET_ABI" != "arm64-v8a" ]]; then
  echo "This first integration build currently supports arm64-v8a only (requested: $TARGET_ABI)." >&2
  exit 2
fi

if [[ ! -d "$PJSIP_DIR/.git" ]]; then
  git clone --depth 1 --branch "$PJSIP_VERSION" https://github.com/pjsip/pjproject.git "$PJSIP_DIR"
fi

cd "$PJSIP_DIR"
mkdir -p pjlib/include/pj
cat > pjlib/include/pj/config_site.h <<'EOF'
/* PJSIP Android configuration for TK App. */
#define PJ_CONFIG_ANDROID 1
#include <pj/config_site_sample.h>
EOF

export TARGET_ABI
export ANDROID_NDK_ROOT
# Keep native ELF segments compatible with Android 15 devices using 16 KB pages.
export CFLAGS="${CFLAGS:-} -D__BIONIC_NO_PAGE_SIZE_MACRO"
export LDFLAGS="${LDFLAGS:-} -Wl,-z,max-page-size=16384"
export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"

./configure-android --use-ndk-cflags
make dep
make clean
make -j"$(nproc)"

# Generate the official SWIG Java/JNI binding. PJSIP documents this as the
# supported way to consume PJSUA2 from Android Java/Kotlin applications.
command -v swig >/dev/null
cd pjsip-apps/src/swig
# Build only the Java/PJSUA2 bindings. The default Android SWIG target also
# builds the Xamarin/C# wrapper, which copies optional camera helper sources
# that are absent in this audio-only build and causes CI to fail.
# The upstream Java Makefile generates SWIG sources as a prerequisite of the
# JNI shared library, then compiles the generated Java classes. Running this
# target in parallel races javac against SWIG and can report missing Error.java.
make java

GENERATED="java/android/pjsua2/src/main"
test -d "$GENERATED/jniLibs/$TARGET_ABI"
test -d "$GENERATED/java/org/pjsip/pjsua2"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/jniLibs" "$OUTPUT_DIR/java/org/pjsip"
cp -a "$GENERATED/jniLibs/." "$OUTPUT_DIR/jniLibs/"
cp -a "$GENERATED/java/org/pjsip/pjsua2" "$OUTPUT_DIR/java/org/pjsip/"

echo "PJSIP $PJSIP_VERSION Android bindings staged in $OUTPUT_DIR"
find "$OUTPUT_DIR/jniLibs" -maxdepth 2 -type f -name '*.so' -print
