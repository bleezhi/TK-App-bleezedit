#ifndef AppVersion
  #define AppVersion "0.0.2"
#endif
#ifndef StageDir
  #error StageDir must point to the prepared TK-App-Windows-x64 directory.
#endif

#define AppName "TK App"
#define AppPublisher "Telekomunikacja Kaliska"
#define AppExeName "tk-app.exe"

[Setup]
AppId={{6F1A2E3B-71D4-4D09-9F18-9A70A1C42B10}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/bleezhi/TK-App-bleezedit
DefaultDirName={autopf}\TK App
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=../../releases
OutputBaseFilename=TK-App-{#AppVersion}-Windows-x64-Setup
SetupIconFile=../tk-app.ico
UninstallDisplayIcon={app}\{#AppExeName}
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
LicenseFile=../../LICENSE

[Tasks]
Name: "desktopicon"; Description: "Utwórz skrót na pulpicie"; GroupDescription: "Dodatkowe skróty:"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\TK App"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\TK App"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Uruchom TK App"; Flags: postinstall nowait skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"
