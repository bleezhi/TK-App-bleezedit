#!/usr/bin/env bash
set -Eeuo pipefail
PREFIX="${TK_APP_PREFIX:-$HOME/.local}"
APP_HOME="$PREFIX/opt/tk-app"
BIN="$PREFIX/bin/tk-app"
DESKTOP="$PREFIX/share/applications/tk-app.desktop"
ICON="$PREFIX/share/icons/hicolor/512x512/apps/tk-app.png"
removed=0
for path in "$APP_HOME" "$BIN" "$DESKTOP" "$ICON"; do
    if [[ -e "$path" || -L "$path" ]]; then
        rm -rf -- "$path"
        removed=1
        echo "Removed: $path"
    fi
done
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi
(( removed )) || echo "No TK App installation found for this user."
