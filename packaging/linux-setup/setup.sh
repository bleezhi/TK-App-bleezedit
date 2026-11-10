#!/usr/bin/env bash
# Per-user TK App installer. No root access is needed unless system dependencies are missing.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${TK_APP_PREFIX:-$HOME/.local}"
APP_HOME="$PREFIX/opt/tk-app"
BIN_DIR="$PREFIX/bin"
APP_DIR="$PREFIX/share/applications"
ICON_DIR="$PREFIX/share/icons/hicolor/512x512/apps"

[[ -x "$ROOT/payload/AppRun" && -x "$ROOT/payload/usr/bin/tk-app" ]] || {
    echo "The setup archive is incomplete. Extract the entire ZIP before running setup.sh." >&2
    exit 1
}
[[ "$(uname -m)" == x86_64 ]] || {
    echo "This package is for x86_64 systems; detected: $(uname -m)" >&2
    exit 1
}
command -v python3 >/dev/null 2>&1 || {
    echo "Python 3 is required. Install python3 with your distribution's package manager." >&2
    exit 1
}
command -v ldd >/dev/null 2>&1 || {
    echo "ldd is required to verify system runtime dependencies." >&2
    exit 1
}

DISTRO_ID=unknown
DISTRO_NAME=unknown
if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    DISTRO_ID="${ID:-unknown}"
    DISTRO_NAME="${PRETTY_NAME:-$DISTRO_ID}"
fi
case "$DISTRO_ID" in
    debian|ubuntu|linuxmint|pop) PKG_MANAGER=apt; PACKAGES=(libgl1 libegl1 libxkbcommon-x11-0 libxcb-cursor0 libpulse0 libasound2) ;;
    fedora|rhel|centos|rocky|almalinux) PKG_MANAGER=dnf; PACKAGES=(mesa-libGL mesa-libEGL libxkbcommon-x11 xcb-util-cursor pulseaudio-libs alsa-lib) ;;
    arch|manjaro|endeavouros) PKG_MANAGER=pacman; PACKAGES=(libglvnd libxkbcommon-x11 xcb-util-cursor libpulse alsa-lib) ;;
    opensuse*|opensuse-tumbleweed|opensuse-leap|suse) PKG_MANAGER=zypper; PACKAGES=(Mesa-libGL1 Mesa-libEGL1 libxkbcommon-x11-0 libxcb-cursor0 libpulse0 alsa) ;;
    *)
        PKG_MANAGER=unknown
        PACKAGES=()
        echo "Detected $DISTRO_NAME. This distribution is not in the package-name database; dependency checks will still run."
        ;;
esac
echo "Detected Linux distribution: $DISTRO_NAME"

find_missing_libraries() {
    {
        ldd "$ROOT/payload/usr/bin/tk-app" 2>/dev/null || true
        while IFS= read -r -d '' plugin; do ldd "$plugin" 2>/dev/null || true; done < <(find "$ROOT/payload/usr/plugins" -type f -name '*.so' -print0 2>/dev/null)
    } | awk '/not found/ {print $1}' | sort -u
}
MISSING="$(find_missing_libraries)"
if [[ -n "$MISSING" ]]; then
    echo "Missing system runtime libraries:"
    printf '  %s\n' "$MISSING"
    if [[ "$PKG_MANAGER" == unknown ]]; then
        echo "Install the packages providing these libraries, then run setup.sh again." >&2
        exit 1
    fi
    echo "Suggested runtime packages for $DISTRO_NAME: ${PACKAGES[*]}"
    read -r -p "Install these packages now? [y/N] " answer
    case "$answer" in
        y|Y|yes|YES)
            if [[ "$(id -u)" -eq 0 ]]; then SUDO=()
            elif command -v sudo >/dev/null 2>&1; then SUDO=(sudo)
            else echo "sudo is required to install system dependencies." >&2; exit 1
            fi
            case "$PKG_MANAGER" in
                apt) "${SUDO[@]}" apt-get update; "${SUDO[@]}" apt-get install -y "${PACKAGES[@]}" ;;
                dnf) "${SUDO[@]}" dnf install -y "${PACKAGES[@]}" ;;
                pacman) "${SUDO[@]}" pacman -S --needed --noconfirm "${PACKAGES[@]}" ;;
                zypper) "${SUDO[@]}" zypper --non-interactive install "${PACKAGES[@]}" ;;
            esac
            MISSING="$(find_missing_libraries)"
            if [[ -n "$MISSING" ]]; then
                echo "Some libraries are still missing after installing the suggested packages:" >&2
                printf '  %s\n' "$MISSING" >&2
                exit 1
            fi
            ;;
        *) echo "Install the missing libraries and run setup.sh again."; exit 1 ;;
    esac
fi

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR" "$(dirname "$APP_HOME")"
if [[ -e "$APP_HOME" ]]; then
    BACKUP="$APP_HOME.backup.$(date +%Y%m%d%H%M%S)"
    echo "Existing installation found; moving it to $BACKUP"
    mv "$APP_HOME" "$BACKUP"
fi
mkdir -p "$APP_HOME"
cp -a "$ROOT/payload/." "$APP_HOME/"
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
