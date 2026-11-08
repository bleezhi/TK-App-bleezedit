#!/usr/bin/env python3
"""Copy the full PE import closure, fail if any non-system DLL is missing."""
import argparse, pathlib, re, shutil, subprocess
p=argparse.ArgumentParser();p.add_argument('stage');p.add_argument('prefix');p.add_argument('--objdump',default='objdump');a=p.parse_args()
stage=pathlib.Path(a.stage).resolve();prefix=pathlib.Path(a.prefix).resolve()
system=set('advapi32 avrt bcrypt bcryptprimitives cabinet cfgmgr32 comctl32 comdlg32 crypt32 cryptbase cryptsp d2d1 d3d9 d3d11 d3d12 dbghelp dhcpcsvc dnsapi dwrite dwmapi dxgi dxguid gdi32 glu32 imm32 iphlpapi kernel32 mpr msimg32 msvcrt msvcp_win mswsock netapi32 ntdll normaliz ole32 oleaut32 opengl32 pdh powrprof propsys psapi rpcrt4 secur32 setupapi shell32 shcore shlwapi user32 userenv usp10 uxtheme ucrtbase version winhttp wininet winmm winspool ws2_32 wsock32 wtsapi32 wintrust winusb hid usp10'.split())
available={f.name.lower():f for f in (prefix/'bin').glob('*.dll')}
seen=set();queue=list(stage.rglob('*.dll'))+list(stage.glob('*.exe'))
while queue:
    binary=queue.pop();key=str(binary).lower()
    if key in seen:continue
    seen.add(key)
    output=subprocess.check_output([a.objdump,'-p',str(binary)],text=True,errors='replace')
    for name in re.findall(r'DLL Name:\s*(\S+)',output):
        low=name.lower();stem=low[:-4] if low.endswith('.dll') else low
        if stem in system or low.startswith(('api-ms-','ext-ms-')):continue
        destination=next((f for f in stage.glob('*.dll') if f.name.lower()==low),None)
        if destination is None:
            source=available.get(low)
            if source is None:raise SystemExit('Missing required DLL: '+name+' (imported by '+str(binary)+')')
            destination=stage/source.name;shutil.copy2(source,destination);print('Bundled',source.name)
        queue.append(destination)
licenses=prefix/'share/licenses'
if licenses.exists():shutil.copytree(licenses,stage/'licenses/msys2',dirs_exist_ok=True)
print('Verified import closure for',len(seen),'PE files.')
