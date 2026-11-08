#!/usr/bin/env python3
"""Bundle a native Linux x86_64 TK App into an AppDir (Fedora or Debian/Ubuntu).

Run on the SAME distribution/SDK used to compile the input executable.
The system glibc and graphics drivers are intentionally NOT bundled.
"""
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys


def run(args, check=True):
    return subprocess.run(args, capture_output=True, text=True, check=check)


def is_x86_64_elf(path):
    with pathlib.Path(path).open('rb') as f:
        header = f.read(20)
    return (len(header) == 20 and header[:4] == b'\x7fELF' and
            header[4:6] == b'\x02\x01' and header[18:20] == b'\x3e\x00')


def qt_plugins(imports):
    match = re.search(r'libQt([56])Core\.so', imports)
    if not match:
        raise SystemExit('No shared native Qt5Core/Qt6Core found. Pass the Linux executable, not a Windows EXE.')
    major = match.group(1)
    override = os.environ.get('TK_QMAKE')
    candidates = [override] if override else [
        f'qmake-qt{major}', f'qmake{major}', 'qmake',
        f'/usr/lib64/qt{major}/bin/qmake',
        f'/usr/lib/qt{major}/bin/qmake',
        f'/usr/lib/x86_64-linux-gnu/qt{major}/bin/qmake',
    ]
    for candidate in candidates:
        executable = shutil.which(candidate)
        if not executable:
            continue
        version = run([executable, '-query', 'QT_VERSION'], check=False)
        directory = run([executable, '-query', 'QT_INSTALL_PLUGINS'], check=False)
        if version.returncode or directory.returncode or not version.stdout.strip().startswith(major + '.'):
            continue
        plugins = pathlib.Path(directory.stdout.strip())
        platform = plugins / 'platforms/libqxcb.so'
        if platform.is_file() and is_x86_64_elf(platform):
            return plugins, version.stdout.strip()
    raise SystemExit(
        f'Cannot find native Qt{major} qmake and libqxcb.so. '
        f'On Fedora install qt{major}-qtbase-devel (and the matching GUI package). '
        'You may set TK_QMAKE=/path/to/native/qmake. Do not use MinGW qmake.')


def package_backend():
    info = pathlib.Path('/etc/os-release').read_text() if pathlib.Path('/etc/os-release').is_file() else ''
    rpm_system = any(word in info.lower() for word in ('fedora', 'rhel', 'centos', 'suse', 'rocky', 'almalinux'))
    choices = ('rpm', 'dpkg-query') if rpm_system else ('dpkg-query', 'rpm')
    for command in choices:
        if shutil.which(command):
            return command
    raise SystemExit('No RPM/DPKG package database tools: cannot collect installed dependency license notices.')


def owners(paths, backend):
    # Include both symlink names and canonical paths (also handles /usr merge).
    files = sorted({str(p) for source in paths for p in (source, source.resolve())})
    packages = set()
    for offset in range(0, len(files), 100):
        chunk = files[offset:offset + 100]
        if backend == 'rpm':
            output = run(['rpm', '-qf', '--qf', '%{NAME}\\n', *chunk], check=False)
            # A batch may contain unowned local files; keep successful queries.
            packages.update(line.strip() for line in output.stdout.splitlines()
                            if re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9+_.-]*', line.strip()))
        else:
            output = run(['dpkg-query', '-S', *chunk], check=False)
            for line in output.stdout.splitlines():
                if ': ' in line:
                    packages.add(line.split(': ', 1)[0].split(':', 1)[0])
    return packages


def copy_package_licenses(packages, backend, destination):
    if backend == 'rpm':
        for package in sorted(packages):
            listing = run(['rpm', '-ql', package], check=False)
            for line in listing.stdout.splitlines():
                source = pathlib.Path(line)
                if not source.is_file():
                    continue
                in_licenses = str(source).startswith('/usr/share/licenses/')
                in_docs = (str(source).startswith('/usr/share/doc/') and
                           re.match(r'^(copying|license|copyright|notice)', source.name, re.I))
                if in_licenses or in_docs:
                    target = destination / 'fedora' / source.relative_to('/')
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, target)
    else:
        for package in sorted(packages):
            source = pathlib.Path('/usr/share/doc') / package / 'copyright'
            if source.is_file():
                shutil.copy2(source, destination / (package + '.txt'))
        common = pathlib.Path('/usr/share/common-licenses')
        if common.is_dir():
            shutil.copytree(common, destination / 'common-licenses', dirs_exist_ok=True)
    (destination / 'package-list.txt').write_text('\n'.join(sorted(packages)) + '\n')


BRANDING_NOTICE = (
    'Oznaczenia Telekomunikacji Kaliskiej\n\n'
    'Oryginalne logo dostarczyl uzytkownik. Wersja kadrowana usuwa jedynie\n'
    'przezroczyste zewnetrzne marginesy. Ikony aplikacji zawieraja ten sam\n'
    'oryginalny znak, bez zmiany jego kolorow ani generowania nowego logo.\n\n'
    'Logo, nazwa sieci i prawa do oznaczen pozostaja przy ich uprawnionym\n'
    'wlascicielu. Licencja GPL kodu aplikacji nie jest zapewnieniem praw\n'
    'do znaku ani zgoda na jego niezalezne wykorzystanie. Material dolaczono\n'
    'zgodnie z poleceniem uzytkownika do klienta SIP Telekomunikacji Kaliskiej.\n'
)


def normalize_markdown_files(appdir):
    """Keep all upstream notice bytes, but emit no Markdown filenames.

    Copies are completed before source names are removed, including symlinks.
    Existing .txt files are never overwritten, even on filename collisions.
    Only the freshly built AppDir is changed, never the project or host files.
    """
    sources = sorted(p for p in appdir.rglob('*')
                     if p.name.lower().endswith('.md') and not p.is_dir())
    renamed = []
    reserved = set()
    for source in sources:
        stem = source.name[:-3] or 'NOTICE'
        target = source.with_name(stem + '.txt')
        counter = 1
        while target.exists() or target.is_symlink() or target in reserved:
            target = source.with_name(stem + '-notice-' + str(counter) + '.txt')
            counter += 1
        shutil.copy2(source, target)
        reserved.add(target)
        renamed.append((source, target))
    for source, _ in renamed:
        source.unlink()
    remaining = [p for p in appdir.rglob('*')
                 if p.name.lower().endswith('.md') and not p.is_dir()]
    if remaining:
        raise SystemExit('Markdown filename still present in AppDir: ' + str(remaining[0]))
    print('Markdown check: 0 .md files; ' + str(len(renamed)) +
          ' upstream text files retained as .txt.', flush=True)
    return renamed


def validate_assets(root):
    required = (
        'LICENSE',
        'packaging/AppRun', 'packaging/tk-app.desktop',
        'packaging/tk-app.png', 'packaging/tk-app.svg',
        'packaging/LICENSE.AppImage-runtime.txt',
        'packaging/licenses/PJSIP/COPYING',
        'packaging/licenses/PJSIP/third_party/resample/COPYING',
    )
    missing = [name for name in required if not (root / name).is_file()]
    if missing:
        raise SystemExit('Missing packaging files (restore these before retrying):\n  ' +
                         '\n  '.join(missing) +
                         '\nDo not substitute empty files for license notices.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', help='Native Linux x86_64 executable, usually build/tk-app')
    parser.add_argument('appdir')
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    validate_assets(root)
    binary = pathlib.Path(args.binary).resolve(strict=True)
    appdir = pathlib.Path(args.appdir).resolve()
    if not is_x86_64_elf(binary):
        raise SystemExit('Input must be a Linux x86_64 ELF executable, not a Windows EXE.')
    for tool in ('ldd', 'patchelf'):
        if not shutil.which(tool):
            raise SystemExit('Missing command: ' + tool)
    imports = run(['ldd', str(binary)]).stdout
    if 'not found' in imports:
        raise SystemExit('Unresolved ELF import before packaging:\n' + imports)
    plugins, qt_version = qt_plugins(imports)
    backend = package_backend()
    print('Qt:', qt_version, '| Plugins:', plugins, '| Packages:', backend, flush=True)
    (appdir / 'usr/bin').mkdir(parents=True, exist_ok=True)
    libdir = appdir / 'usr/lib'
    libdir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binary, appdir / 'usr/bin/tk-app')
    for name in ('tk-app.desktop', 'tk-app.png', 'tk-app.svg'):
        shutil.copy2(root / 'packaging' / name, appdir / name)
    shutil.copy2(root / 'packaging/tk-app.png', appdir / '.DirIcon')
    shutil.copy2(root / 'packaging/AppRun', appdir / 'AppRun')
    os.chmod(appdir / 'AppRun', 0o755)
    seeds = [binary]
    package_sources = set()
    for category, names in {
        'platforms': ('libqxcb.so', 'libqoffscreen.so', 'libqminimal.so'),
        'xcbglintegrations': ('libqxcb-glx-integration.so', 'libqxcb-egl-integration.so'),
    }.items():
        out = appdir / 'usr/plugins' / category
        out.mkdir(parents=True, exist_ok=True)
        for name in names:
            source = plugins / category / name
            if source.is_file() and is_x86_64_elf(source):
                shutil.copy2(source, out / name)
                seeds.append(source)
    pulse_found = False
    for base in ('/usr/lib64/alsa-lib', '/usr/lib/x86_64-linux-gnu/alsa-lib', '/usr/lib/alsa-lib'):
        for name in ('libasound_module_pcm_pulse.so', 'libasound_module_ctl_pulse.so'):
            source = pathlib.Path(base) / name
            if source.is_file() and is_x86_64_elf(source):
                out = libdir / 'alsa-lib'
                out.mkdir(exist_ok=True)
                shutil.copy2(source, out / name)
                seeds.append(source)
                pulse_found = pulse_found or name == 'libasound_module_pcm_pulse.so'
    if 'libasound.so' in imports and not pulse_found:
        raise SystemExit('Missing ALSA PulseAudio plugin. Fedora: sudo dnf install alsa-plugins-pulseaudio')
    alsa = pathlib.Path('/usr/share/alsa')
    if alsa.is_dir():
        target = appdir / 'usr/share/alsa'
        shutil.copytree(alsa, target, dirs_exist_ok=True)
        package_sources.update(p for p in alsa.rglob('*') if p.is_file())
        config_path = target / 'alsa.conf'
        config = config_path.read_text()
        for path in ('/usr/etc/alsa/conf.d', '/etc/alsa/conf.d', '/var/lib/alsa/conf.d'):
            config = config.replace('"' + path + '"', '"/nonexistent-tk-app-alsa-config"')
        config += ('\n# TK App desktop audio (PulseAudio / PipeWire-pulse)\n'
                   'pcm.pulse { type pulse }\nctl.pulse { type pulse }\n'
                   'pcm.!default { type pulse fallback "sysdefault" }\n'
                   'ctl.!default { type pulse fallback "sysdefault" }\n')
        config_path.write_text(config)
    exclude = re.compile(r'^(linux-vdso|ld-linux|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|libresolv\.so|libutil\.so|libnss_|libGL(?:\.so|X|dispatch)|libEGL|libOpenGL|libdrm|libvulkan)')
    seen = set()
    queue = seeds[:]
    while queue:
        source_binary = queue.pop()
        key = str(source_binary.resolve())
        if key in seen:
            continue
        seen.add(key)
        package_sources.add(source_binary)
        result = run(['ldd', str(source_binary)])
        if 'not found' in result.stdout:
            raise SystemExit('Unresolved ELF import:\n' + result.stdout)
        for soname, path in re.findall(r'^\s*(\S+)\s+=>\s+(/\S+)\s+\(', result.stdout, re.M):
            if exclude.match(soname):
                continue
            source = pathlib.Path(path)
            destination = libdir / soname
            if not destination.exists():
                shutil.copy2(source.resolve(), destination)
                print('Bundled', soname, flush=True)
            queue.append(source)
    for f in libdir.glob('*.so*'):
        run(['patchelf', '--set-rpath', '$ORIGIN', str(f)])
    run(['patchelf', '--set-rpath', '$ORIGIN/../lib', str(appdir / 'usr/bin/tk-app')])
    for f in (appdir / 'usr/plugins').rglob('*.so'):
        run(['patchelf', '--set-rpath', '$ORIGIN/../../lib', str(f)])
    for f in (libdir / 'alsa-lib').glob('*.so'):
        run(['patchelf', '--set-rpath', '$ORIGIN/..', str(f)])
    docs = appdir / 'usr/share/tk-app'
    docs.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / 'LICENSE', docs / 'LICENSE')
    # No project Markdown documentation or historical test reports are bundled.
    licenses = docs / 'licenses'
    licenses.mkdir(exist_ok=True)
    if (root / 'packaging/licenses').is_dir():
        shutil.copytree(root / 'packaging/licenses', licenses, dirs_exist_ok=True)
    shutil.copy2(root / 'packaging/LICENSE.AppImage-runtime.txt', licenses / 'AppImage-runtime.txt')
    # Brand attribution is self-contained: no README or extra source file needed.
    (licenses / 'TELEKOMUNIKACJA-KALISKA-LOGO.txt').write_text(BRANDING_NOTICE, encoding='utf-8')
    packages = owners(package_sources, backend)
    copy_package_licenses(packages, backend, licenses)
    # Includes license notices supplied as .md by RPM/DEB packages or ALSA data.
    normalize_markdown_files(appdir)
    host = pathlib.Path('/etc/os-release').read_text() if pathlib.Path('/etc/os-release').is_file() else 'unknown host'
    (docs / 'BUILD-INFO.txt').write_text(
        'TK App Linux x86_64 AppImage package.\nQt: ' + qt_version + '\n'
        'Input executable: ' + binary.name + '\n'
        'Tests of this executable: NOT RUN by the packaging script.\n'
        'glibc and graphics drivers are NOT bundled.\n'
        'Compatibility depends on the distribution/SDK used for compilation.\n\n' + host)
    print('AppDir ready:', appdir)


if __name__ == '__main__':
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        if isinstance(error, subprocess.CalledProcessError):
            if error.stdout:
                print(error.stdout, file=sys.stderr)
            if error.stderr:
                print(error.stderr, file=sys.stderr)
        sys.exit('Packaging failed: ' + str(error))
