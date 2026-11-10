TK APP — LINUX SETUP (x86_64)
=================================

INSTALL
1. Extract the complete TK-App-Linux-Setup-x86_64.zip archive.
2. Open a terminal in the extracted TK-App-Linux-Setup directory.
3. Run:

     ./setup.sh

This installs TK App for the current user only. Root/sudo is not required.
The application appears in the desktop application menu. You can also run
"tk-app" after adding ~/.local/bin to PATH.

UNINSTALL
Run ./uninstall.sh from this extracted directory. It removes the per-user
installation under ~/.local. It does not remove your account settings stored
in ~/.config or other user data.

SUPPORTED SYSTEMS
This bundle targets 64-bit x86 Linux distributions, including Debian,
Ubuntu, Fedora, Arch Linux, openSUSE and their derivatives. It bundles the
application and most Qt/PJSIP/audio runtime libraries. It still uses the
host system's glibc, graphics drivers, display server and audio server.
A sufficiently recent glibc and a working X11/XWayland environment are
required. Wayland desktops normally provide XWayland; native Wayland Qt
support is not included in this bundle.

DEPENDENCIES
The setup does not silently install operating-system packages or invoke
sudo. If the application does not start because a host graphics/audio
dependency is missing, install the corresponding runtime package using
your distribution's package manager. Common package names include:

 Debian / Ubuntu:
   libgl1 libegl1 libxkbcommon-x11-0 libxcb-cursor0 libpulse0 libasound2

 Fedora:
   mesa-libGL mesa-libEGL libxkbcommon-x11 xcb-util-cursor pulseaudio-libs alsa-lib

 Arch:
   libglvnd libxkbcommon-x11 xcb-util-cursor libpulse alsa-lib

 openSUSE:
   Mesa-libGL1 Mesa-libEGL1 libxkbcommon-x11-0 libxcb-cursor0
   libpulse0 alsa

TROUBLESHOOTING
- Run from a terminal to see diagnostic output: ~/.local/bin/tk-app
- Check that your desktop session has XWayland/X11 and audio configured.
- This is a native x86_64 build; it is not an ARM package.
- The setup ZIP installs files; it does not build the source code.
- SIP account details are not included in this archive.

LICENSES
The project is GPL-3.0-or-later. Third-party notices for bundled components
are included under ~/.local/opt/tk-app/usr/share/tk-app/licenses.

Project: https://github.com/bleezhi/TK-App-bleezedit
