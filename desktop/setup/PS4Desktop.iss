#ifndef PayloadDir
  #error Define PayloadDir
#endif
#ifndef PackageOutput
  #error Define PackageOutput
#endif
[Setup]
AppId={{ACD4806C-B785-4F7C-A817-4ED0C778FE32}
AppName=PS4 Desktop — Native DirectX 12
AppVersion=0.1.20261009
AppPublisher=PS4 Desktop experimental fork
DefaultDirName={localappdata}\Programs\PS4Desktop-DX12
DefaultGroupName=PS4 Desktop DX12
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#PackageOutput}
OutputBaseFilename=PS4Desktop-DX12-Setup-20261009
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\PS4Desktop.exe
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes
SetupLogging=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\PS4 Desktop DX12"; Filename: "{app}\PS4Desktop.exe"
Name: "{userdesktop}\PS4 Desktop DX12"; Filename: "{app}\PS4Desktop.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PS4Desktop.exe"; Description: "Открыть библиотеку PS4 Desktop"; Flags: nowait postinstall skipifsilent
