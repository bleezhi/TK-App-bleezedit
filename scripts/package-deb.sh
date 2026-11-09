#!/usr/bin/env bash
# Build a Debian/Ubuntu package from the same tested binary bundle as the AppImage.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="${1:-$ROOT/build/tk-app}"
OUTPUT="${2:-$ROOT/releases}"
WORK="${TK_DEB_WORK:-$ROOT/.cache/deb-package}"
for command in python3 ldd patchelf dpkg-deb; do
    command -v "$command" >/dev/null || { echo "Brak wymaganego polecenia: $command" >&2; exit 1; }
done
[[ -f "$BINARY" ]] || { echo "Nie znaleziono pliku wykonywalnego: $BINARY" >&2; exit 1; }
VERSION="$(python3 - "$ROOT/CMakeLists.txt" <<'PY'
import pathlib,re,sys
text=pathlib.Path(sys.argv[1]).read_text()
match=re.search(r'project\s*\(\s*TKApp\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\b',text,re.I)
if not match: sys.exit('Nie można odczytać wersji z CMakeLists.txt.')
print(match.group(1))
PY
)"
mkdir -p "$OUTPUT" "$WORK"
OUTPUT="$(cd "$OUTPUT" && pwd)"
WORK="$(cd "$WORK" && pwd)"
BINARY="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve())' "$BINARY")"
APPDIR="$WORK/TK-App.AppDir"
PKGROOT="$WORK/package-root"
rm -rf -- "$APPDIR" "$PKGROOT"
python3 "$ROOT/scripts/bundle-linux.py" "$BINARY" "$APPDIR"
install -d "$PKGROOT/opt/tk-app" "$PKGROOT/usr/bin" \
    "$PKGROOT/usr/share/applications" "$PKGROOT/usr/share/icons/hicolor/scalable/apps" \
    "$PKGROOT/usr/share/icons/hicolor/512x512/apps" "$PKGROOT/usr/share/doc/tk-app"
cp -a "$APPDIR/usr/." "$PKGROOT/opt/tk-app/usr/"
install -m 0755 "$APPDIR/AppRun" "$PKGROOT/opt/tk-app/AppRun"
cat > "$PKGROOT/usr/bin/tk-app" <<'LAUNCHER'
#!/bin/sh
exec /opt/tk-app/AppRun "$@"
LAUNCHER
chmod 0755 "$PKGROOT/usr/bin/tk-app"
install -m 0644 "$APPDIR/tk-app.desktop" "$PKGROOT/usr/share/applications/tk-app.desktop"
install -m 0644 "$ROOT/packaging/tk-app.svg" "$PKGROOT/usr/share/icons/hicolor/scalable/apps/tk-app.svg"
install -m 0644 "$ROOT/packaging/tk-app.png" "$PKGROOT/usr/share/icons/hicolor/512x512/apps/tk-app.png"
install -m 0644 "$ROOT/LICENSE" "$PKGROOT/usr/share/doc/tk-app/copyright"
cat > "$PKGROOT/DEBIAN/control" <<CONTROL
Package: tk-app
Version: $VERSION
Section: net
Priority: optional
Architecture: amd64
Maintainer: TK App Project
Depends: libc6 (>= 2.35), libstdc++6, libgcc-s1
Description: Klient SIP Telekomunikacji Kaliskiej
 Natywna aplikacja telefoniczna Qt z obsługą kont i połączeń SIP.
CONTROL
install -d "$PKGROOT/DEBIAN"
cat > "$PKGROOT/DEBIAN/postinst" <<'POSTINST'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
exit 0
POSTINST
chmod 0755 "$PKGROOT/DEBIAN/postinst"
DEB="$OUTPUT/TK-App-$VERSION-amd64.deb"
rm -f "$DEB"
dpkg-deb --build --root-owner-group "$PKGROOT" "$DEB"
sha256sum "$DEB" > "$DEB.sha256"
echo "Gotowy pakiet: $DEB"
