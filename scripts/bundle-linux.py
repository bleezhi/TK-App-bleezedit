#!/usr/bin/env python3
"""Create a Qt/PJSIP AppDir, excluding glibc and host graphics drivers.
Run on the SAME baseline distribution used for compilation (Ubuntu 22.04).
"""
import argparse, os, pathlib, re, shutil, subprocess
p=argparse.ArgumentParser();p.add_argument('binary');p.add_argument('appdir');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];appdir=pathlib.Path(a.appdir).resolve()
(appdir/'usr/bin').mkdir(parents=True,exist_ok=True);libdir=appdir/'usr/lib';libdir.mkdir(parents=True,exist_ok=True)
shutil.copy2(a.binary,appdir/'usr/bin/tk-app')
for name in ['tk-app.desktop','tk-app.png','tk-app.svg']:shutil.copy2(root/'packaging'/name,appdir/name)
shutil.copy2(root/'packaging/tk-app.png',appdir/'.DirIcon')
shutil.copy2(root/'packaging/AppRun',appdir/'AppRun');os.chmod(appdir/'AppRun',0o755)
imports=subprocess.check_output(['ldd',str(a.binary)],text=True)
query='qmake6' if 'libQt6Core' in imports else 'qmake'
plugins=pathlib.Path(subprocess.check_output([query,'-query','QT_INSTALL_PLUGINS'],text=True).strip())
seeds=[pathlib.Path(a.binary).resolve()]
for category,names in {'platforms':['libqxcb.so','libqoffscreen.so','libqminimal.so'], 'xcbglintegrations':['libqxcb-glx-integration.so','libqxcb-egl-integration.so']}.items():
    out=appdir/'usr/plugins'/category;out.mkdir(parents=True,exist_ok=True)
    for name in names:
        source=plugins/category/name
        if source.exists():shutil.copy2(source,out/name);seeds.append(source)
# PulseAudio ALSA plugin works with PulseAudio and PipeWire's pulse server.
for base in [pathlib.Path('/usr/lib/x86_64-linux-gnu/alsa-lib'),pathlib.Path('/usr/lib/alsa-lib')]:
    for name in ['libasound_module_pcm_pulse.so','libasound_module_ctl_pulse.so']:
        source=base/name
        if source.exists():
            out=libdir/'alsa-lib';out.mkdir(exist_ok=True);shutil.copy2(source,out/name);seeds.append(source)
alsa=pathlib.Path('/usr/share/alsa')
if alsa.exists():
    target=appdir/'usr/share/alsa';shutil.copytree(alsa,target,dirs_exist_ok=True)
    # Avoid loading incompatible host PipeWire/JACK plug-ins into bundled ALSA.
    config=(target/'alsa.conf').read_text()
    config=config.replace('"/usr/etc/alsa/conf.d"','"/nonexistent-tk-app-alsa-config"').replace('"/etc/alsa/conf.d"','"/nonexistent-tk-app-alsa-config"')
    config=config.replace('"/var/lib/alsa/conf.d"','"/nonexistent-tk-app-alsa-config"')
    (target/'alsa.conf').write_text(config)
    # Deliberately use PulseAudio if available; ALSA hardware remains selectable.
    with (target/'alsa.conf').open('a') as f:
        f.write('\n# TK App desktop audio (PulseAudio / PipeWire-pulse)\npcm.pulse { type pulse }\nctl.pulse { type pulse }\npcm.!default { type pulse fallback "sysdefault" }\nctl.!default { type pulse fallback "sysdefault" }\n')
exclude=re.compile(r'^(linux-vdso|ld-linux|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|libresolv\.so|libutil\.so|libnss_|libGL(?:\.so|X|dispatch)|libEGL|libOpenGL|libdrm|libvulkan)')
seen=set();packages=set();queue=seeds[:]
while queue:
    binary=queue.pop();key=str(binary.resolve())
    if key in seen:continue
    seen.add(key)
    result=subprocess.run(['ldd',str(binary)],capture_output=True,text=True,check=True)
    if 'not found' in result.stdout:raise SystemExit('Unresolved ELF import: '+result.stdout)
    for soname,path in re.findall(r'^\s*(\S+)\s+=>\s+(/\S+)\s+\(',result.stdout,re.M):
        if exclude.match(soname):continue
        source=pathlib.Path(path);destination=libdir/soname
        if not destination.exists():shutil.copy2(source.resolve(),destination);print('Bundled',soname)
        queue.append(source)
    owner=subprocess.run(['dpkg-query','-S',str(binary)],capture_output=True,text=True)
    if owner.returncode==0:
        for line in owner.stdout.splitlines():packages.add(line.split(': ',1)[0].split(':',1)[0])
for f in libdir.glob('*.so*'):
    subprocess.run(['patchelf','--set-rpath','$ORIGIN',str(f)],check=True)
subprocess.run(['patchelf','--set-rpath','$ORIGIN/../lib',str(appdir/'usr/bin/tk-app')],check=True)
for f in (appdir/'usr/plugins').rglob('*.so'):
    subprocess.run(['patchelf','--set-rpath','$ORIGIN/../../lib',str(f)],check=True)
for f in (libdir/'alsa-lib').glob('*.so'):
    subprocess.run(['patchelf','--set-rpath','$ORIGIN/..',str(f)],check=True)
docs=appdir/'usr/share/tk-app';docs.mkdir(parents=True,exist_ok=True)
for name in ['LICENSE','README.md','THIRD_PARTY.md','BUILDING.md','TEST-REPORT.md']:shutil.copy2(root/name,docs/name)
lic=docs/'licenses';lic.mkdir(exist_ok=True)
if (root/'packaging/licenses').exists():shutil.copytree(root/'packaging/licenses',lic,dirs_exist_ok=True)
if (root/'packaging/LICENSE.AppImage-runtime.txt').exists():shutil.copy2(root/'packaging/LICENSE.AppImage-runtime.txt',lic/'AppImage-runtime.txt')
shutil.copy2(root/'packaging/branding/README.md',lic/'TELEKOMUNIKACJA-KALISKA-LOGO.md')
for package in sorted(packages):
    source=pathlib.Path('/usr/share/doc')/package/'copyright'
    if source.exists():shutil.copy2(source,lic/(package+'.txt'))
(lic/'package-list.txt').write_text('\n'.join(sorted(packages))+'\n')
print('AppDir ready:',appdir)
