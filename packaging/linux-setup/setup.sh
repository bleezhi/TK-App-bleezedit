#!/usr/bin/env bash
# Per-user TK App installer. No root access is needed.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${TK_APP_PREFIX:-$HOME/.local}"
APP_HOME="$PREFIX/opt/tk-app"
BIN_DIR="$PREFIX/bin"
APP_DIR="$PREFIX/share/applications"
ICON_DIR="$PREFIX/share/icons/hicolor/512x512/apps"
mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR"
[[ -x "$ROOT/payload/AppRun" && -x "$ROOT/payload/usr/bin/tk-app" ]] || {
    echo "The setup archive is incomplete. Extract the entire ZIP before running setup.sh." >&2
    exit 1
}
[[ "$(uname -m)" == x86_64 ]] || {
    echo "This package is for x86_64 systems; detected: $(uname -m)" >&2
    exit 1
}
if ! command -v python3 >/dev/null 2>&1; then
    echo "Python 3 is required to create a correctly escaped desktop entry." >&2
    exit 1
fi
if [[ -e "$APP_HOME" ]]; then
    BACKUP="$APP_HOME.backup.$(date +%Y%m%d%H%M%S)"
    echo "Existing installation found; moving it to $BACKUP"
    mv "$APP_HOME" "$BACKUP"
fi
mkdir -p "$(dirname "$APP_HOME")"
cp -a "$ROOT/payload" "$APP_HOME"
chmod 0755 "$APP_HOME/AppRun" "$APP_HOME/usr/bin/tk-app"
cat > "$BIN_DIR/tk-app" <<LAUNCHER
#!/bin/sh
exec "$APP_HOME/AppRun" "\$@"
LAUNCHER
chmod 0755 "$BIN_DIR/tk-app"
install -m 0644 "$APP_HOME/tk-app.png" "$ICON_DIR/tk-app.png"
python3 - "$APP_DIR/tk-app.desktop" "$BIN_DIR/tk-app" "$ICON_DIR/tk-app.png" <<'PY'
import pathlib,sys
desktop,launcher,icon=map(pathlib.Path,sys.argv[1:])
def escape(value):
    return str(value).replace('\\','\\\\').replace(' ','\\ ').replace('&','\\&')
desktop.write_text(
    '[Desktop Entry]\nType=Application\nName=TK App\n'
    'Comment=Klient SIP Telekomunikacji Kaliskiej\n'
    f'Exec={escape(launcher)}\nIcon={icon}\nTerminal=false\n'
    'Categories=Network;Telephony;\nStartupWMClass=tk-app\n',
    encoding='utf-8')
PY
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi
echo
echo "TK App installed for the current user."
echo "Launch it from your application menu or run: $BIN_DIR/tk-app"
case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) echo "Note: $BIN_DIR is not in PATH. Add this line to your shell profile:"; echo "  export PATH=\"$BIN_DIR:\$PATH\"" ;;
esac
echo "To remove it, run: $ROOT/uninstall.sh"
