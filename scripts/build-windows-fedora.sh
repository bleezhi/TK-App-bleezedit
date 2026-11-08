#!/usr/bin/env bash
# Build the CURRENT TK App sources on Fedora for Windows x86_64.
# No MSYS2, Windows installation or Wine is required for compilation.
set -Eeuo pipefail
if [[ "${1:-}" == "--help" ]]; then
    cat <<'HELP'
Save this file as TK-App/scripts/build-windows-fedora.sh.
Run from the project: bash scripts/build-windows-fedora.sh
Preflight only:       bash scripts/build-windows-fedora.sh --check
Required Fedora packages:
  gcc gcc-c++ make cmake ninja-build pkgconf-pkg-config python3 curl tar
  mingw64-gcc mingw64-gcc-c++ mingw64-binutils
  mingw64-qt5-qtbase mingw64-qt5-qmake mingw64-opus mingw64-libogg
Output:
  releases/windows-fedora/TK-App-Windows-x64/tk-app.exe
  releases/TK-App-<project version>-Windows-x64.zip
Windows tests are NOT run automatically. Test on Windows or with Wine.
HELP
    exit 0
fi
[[ $# -eq 0 || ( $# -eq 1 && "$1" == "--check" ) ]] || { echo 'Use --help for usage.' >&2; exit 2; }
trap 'echo "ERROR: Windows cross-build stopped at line $LINENO. Read the preceding error; no Windows tests have been run." >&2' ERR
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[[ -f "$ROOT/CMakeLists.txt" && -f "$ROOT/scripts/bundle-windows.py" ]] || {
    echo 'Place the script inside the scripts/ directory of your existing TK-App project.' >&2; exit 1;
}
[[ "$ROOT" != *' '* && "$ROOT" != *';'* ]] || {
    echo 'Use a project directory without spaces or semicolons (required by the PJPROJECT makefiles).' >&2; exit 1;
}
TARGET=x86_64-w64-mingw32
SYSROOT="${TK_WINDOWS_SYSROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}"
DEPS="$ROOT/.cache/tk-windows-fedora"
PJ_SRC="$DEPS/pjproject-2.17"
PJ_PREFIX="$DEPS/pj-install"
BUILD="$DEPS/build"
JOBS="${TK_JOBS:-2}"
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || { echo 'TK_JOBS must be a positive integer.' >&2; exit 1; }
for cmd in gcc make cmake pkg-config python3 tar sha256sum rpm "$TARGET-gcc" "$TARGET-g++" "$TARGET-ar" "$TARGET-ranlib" "$TARGET-windres" "$TARGET-objdump"; do
    command -v "$cmd" >/dev/null || { echo "Missing command: $cmd. Install the packages listed by --help." >&2; exit 1; }
done
NINJA="$(command -v ninja || command -v ninja-build || true)"
[[ -n "$NINJA" ]] || { echo 'Missing Ninja. Install ninja-build.' >&2; exit 1; }
[[ -f "$SYSROOT/lib/cmake/Qt5/Qt5Config.cmake" ]] || {
    echo "Missing Windows Qt 5 at $SYSROOT. Install mingw64-qt5-qtbase." >&2; exit 1;
}
find_host_tool() {
    local name="$1" candidate
    for candidate in "$TARGET-$name-qt5" "$name-qt5" "/usr/lib64/qt5/bin/$name" "/usr/lib/qt5/bin/$name"; do
        if command -v "$candidate" >/dev/null 2>&1; then command -v "$candidate"; return; fi
    done
    echo "Missing Linux-host $name for Qt 5. Install mingw64-qt5-qmake." >&2; return 1
}
MOC="$(find_host_tool moc)"
RCC="$(find_host_tool rcc)"
"$MOC" -v
"$RCC" -v
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
export PKG_CONFIG_PATH=""
export PKG_CONFIG_SYSROOT_DIR=""
export PKG_CONFIG_LIBDIR="$PJ_PREFIX/lib/pkgconfig:$SYSROOT/lib/pkgconfig:$SYSROOT/share/pkgconfig"
pkg-config --exists opus ogg Qt5Core || {
    echo 'Missing Windows pkg-config metadata. Install mingw64-opus, mingw64-libogg and mingw64-qt5-qtbase.' >&2; exit 1;
}
PLUGIN="$(find "$SYSROOT/lib" -type f -iname qwindows.dll -print -quit)"
[[ -n "$PLUGIN" ]] || { echo 'qwindows.dll was not found in the Windows Qt package.' >&2; exit 1; }
echo "Project: $ROOT"
echo "Windows libraries: $SYSROOT"
echo "Windows Qt: $(pkg-config --modversion Qt5Core)"
"$TARGET-g++" --version | sed -n '1p'
[[ "${1:-}" != "--check" ]] || { echo 'Preflight passed. No compilation or Windows tests were run.'; exit 0; }
mkdir -p "$DEPS" "$BUILD"

# Separate Windows PJSIP cache: never reuse the Linux .cache/deps/pj-install.
if [[ ! -f "$PJ_PREFIX/lib/pkgconfig/libpjproject.pc" || "${TK_REBUILD_PJSIP:-0}" == 1 ]]; then
    ARCHIVE="$ROOT/vendor/pjproject-2.17.tar.gz"
    if [[ ! -f "$ARCHIVE" ]]; then
        ARCHIVE="$DEPS/pjproject-2.17.tar.gz"
        [[ -f "$ARCHIVE" ]] || curl -fL --retry 3 'https://github.com/pjsip/pjproject/archive/refs/tags/2.17.tar.gz' -o "$ARCHIVE"
    fi
    echo "065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce  $ARCHIVE" | sha256sum -c -
    mkdir -p "$PJ_SRC"
    if [[ ! -f "$PJ_SRC/configure" ]]; then tar xzf "$ARCHIVE" -C "$PJ_SRC" --strip-components=1; fi
    cat > "$PJ_SRC/pjlib/include/pj/config_site.h" <<'CFG'
#define PJMEDIA_HAS_VIDEO 0
#define PJMEDIA_HAS_OPUS_CODEC 1
#define PJMEDIA_AUDIO_DEV_HAS_WMME 1
#define PJMEDIA_AUDIO_DEV_HAS_WASAPI 0
#define PJ_LOG_MAX_LEVEL 3
CFG
    (
        cd "$PJ_SRC"
        export CC="$TARGET-gcc" CXX="$TARGET-g++" AR="$TARGET-ar" RANLIB="$TARGET-ranlib" LD="$TARGET-g++" STRIP="$TARGET-strip" WINDRES="$TARGET-windres"
        export CFLAGS="-O2 -I$SYSROOT/include" CXXFLAGS="-O2 -I$SYSROOT/include" LDFLAGS="-L$SYSROOT/lib"
        ./configure --host="$TARGET" --prefix="$PJ_PREFIX" \
          --disable-video --disable-libyuv --disable-ffmpeg --disable-v4l2 \
          --disable-sdl --disable-openh264 --disable-libwebrtc \
          --disable-speex-aec --disable-ssl --disable-sound --with-opus="$SYSROOT"
        make dep
        make -j"$JOBS"
        make install
    )
fi

cat > "$DEPS/toolchain.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $TARGET-gcc)
set(CMAKE_CXX_COMPILER $TARGET-g++)
set(CMAKE_RC_COMPILER $TARGET-windres)
set(CMAKE_FIND_ROOT_PATH "$SYSROOT" "$PJ_PREFIX" "/usr/$TARGET")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

# Host-native moc/rcc generate code; Windows Qt DLL/import libraries link the app.
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DCMAKE_TOOLCHAIN_FILE="$DEPS/toolchain.cmake" \
    -DQT_DIR="$SYSROOT/lib/cmake/Qt5" \
    -DTK_HOST_MOC="$MOC" -DTK_HOST_RCC="$RCC" \
    -DBUILD_TESTING=OFF -DTK_BUILD_TESTS=OFF
cmake --build "$BUILD" --target tk-app -j"$JOBS"

STAGE="$ROOT/releases/windows-fedora/TK-App-Windows-x64"
# Preserve an earlier generated folder instead of mixing old and new DLLs.
if [[ -e "$STAGE" ]]; then
    BACKUP="$DEPS/previous-portable-$(date +%Y%m%d-%H%M%S)-$$"
    mv "$STAGE" "$BACKUP"
    echo "Previous generated folder preserved at: $BACKUP"
fi
mkdir -p "$STAGE/platforms"
cp "$BUILD/tk-app.exe" "$STAGE/tk-app.exe"
cp "$PLUGIN" "$STAGE/platforms/qwindows.dll"
printf '[Paths]\nPlugins=.\n' > "$STAGE/qt.conf"
# GCC runtime DLLs can live outside sys-root/bin on some Fedora releases.
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libssp-0.dll; do
    file="$("$TARGET-g++" -print-file-name="$dll")"
    if [[ -f "$file" ]]; then cp "$file" "$STAGE/$dll"; fi
done
python3 "$ROOT/scripts/bundle-windows.py" "$STAGE" "$SYSROOT" --objdump "$TARGET-objdump"

export TK_CROSS_ROOT="$ROOT" TK_CROSS_STAGE="$STAGE" TK_CROSS_PJSRC="$PJ_SRC"
export TK_CROSS_COMPILER_INFO="$("$TARGET-g++" --version | sed -n '1p')"
export TK_CROSS_QT_VERSION="$(pkg-config --modversion Qt5Core)"
python3 - <<'PY'
import os, pathlib, re, shutil, subprocess, zipfile, hashlib
root=pathlib.Path(os.environ['TK_CROSS_ROOT'])
stage=pathlib.Path(os.environ['TK_CROSS_STAGE'])
pjsrc=pathlib.Path(os.environ['TK_CROSS_PJSRC'])
for name in ['README.md','BUILDING.md','THIRD_PARTY.md','LICENSE']:
    if (root/name).is_file():shutil.copy2(root/name,stage/name)
licenses=stage/'licenses';licenses.mkdir(exist_ok=True)
if (root/'packaging/licenses').is_dir():shutil.copytree(root/'packaging/licenses',licenses,dirs_exist_ok=True)
brand=root/'packaging/branding/README.md'
if brand.is_file():shutil.copy2(brand,licenses/'TELEKOMUNIKACJA-KALISKA-LOGO.md')
# PJPROJECT includes third-party license notices not all covered by its GPL text.
if pjsrc.is_dir():
    for f in pjsrc.rglob('*'):
        name=f.name.lower()
        if f.is_file() and (name.startswith(('license','licence','copying','copyright')) or name=='readme.resample'):
            dst=licenses/'PJPROJECT'/f.relative_to(pjsrc);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dst)
# Fedora keeps RPM license files outside the Windows sysroot.
packages=sorted(p for p in subprocess.check_output(['rpm','-qa','--qf','%{NAME}\n'],text=True).splitlines() if p.startswith('mingw64-'))
for package in packages:
    files=subprocess.check_output(['rpm','-ql',package],text=True).splitlines()
    for path in files:
        if '/share/licenses/' not in path:continue
        f=pathlib.Path(path)
        if not f.is_file():continue
        relative=path.split('/share/licenses/',1)[1]
        dst=licenses/'fedora'/relative;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dst)
(licenses/'FEDORA-PACKAGES.txt').write_text(subprocess.check_output(['rpm','-q',*packages],text=True) if packages else '',encoding='utf-8')
info=('TK App Windows x86_64: cross-compiled on Fedora.\n'
      'Compiler: '+os.environ['TK_CROSS_COMPILER_INFO']+'\n'
      'Windows Qt: '+os.environ['TK_CROSS_QT_VERSION']+'\n'
      'Windows unit/SIP/UI tests: NOT RUN by this script.\n'
      'No production SIP registration is performed by this script.\n'
      'Test the complete portable folder on Windows or with Wine before distribution.\n'
      'Verify source-availability and other dependency-license obligations before publication.\n')
(stage/'BUILD-INFO.txt').write_text(info,encoding='utf-8')
old_report=root/'TEST-REPORT.md'
if old_report.is_file():
    (stage/'TEST-REPORT.md').write_text('UWAGA: ponizszy raport pochodzi z bazowego wydania projektu.\nNie potwierdza testow tej nowej kompilacji ani lokalnych zmian.\nDane tego buildu: BUILD-INFO.txt.\n\n'+old_report.read_text(encoding='utf-8'),encoding='utf-8')
match=re.search(r'project\s*\(\s*TKApp\s+VERSION\s+([0-9.]+)',(root/'CMakeLists.txt').read_text(),re.I)
version=match.group(1) if match else 'custom'
output=root/'releases'/f'TK-App-{version}-Windows-x64.zip';temporary=output.with_suffix('.zip.tmp')
with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for f in sorted(stage.rglob('*')):
        if f.is_file():z.write(f,pathlib.Path(stage.name)/f.relative_to(stage))
with zipfile.ZipFile(temporary) as z:
    bad=z.testzip()
    if bad:raise RuntimeError('ZIP integrity check failed: '+bad)
os.replace(temporary,output)
checksum=hashlib.sha256(output.read_bytes()).hexdigest()
output.with_suffix('.zip.sha256').write_text(checksum+'  '+output.name+'\n')
print('\nWindows EXE:',stage/'tk-app.exe')
print('Portable ZIP:',output)
print('SHA256:',checksum)
print('Compilation, DLL import closure and ZIP integrity checked; Windows runtime tests NOT RUN.')
PY
