#ifndef StageDirectory
  #error StageDirectory is required
#endif
#ifndef OutputDirectory
  #error OutputDirectory is required
#endif
#ifndef PackageVersion
  #define PackageVersion "0.1.0"
#endif
#ifndef NumericVersion
  #define NumericVersion "0.1.0"
#endif

[Setup]
AppId={{B8374B6D-457E-4A32-B2BC-B0E10597887F}
AppName=Mini-NIDS
AppVersion={#PackageVersion}
VersionInfoVersion={#NumericVersion}
VersionInfoTextVersion={#PackageVersion}
AppPublisher=Mini-NIDS contributors
DefaultDirName={localappdata}\Programs\Mini-NIDS
DefaultGroupName=Mini-NIDS
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
OutputDir={#OutputDirectory}
OutputBaseFilename=Mini-NIDS-{#PackageVersion}-windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Mini-NIDS.exe
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#StageDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Mini-NIDS"; Filename: "{app}\Mini-NIDS.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Mini-NIDS"; Filename: "{app}\Mini-NIDS.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Mini-NIDS.exe"; Description: "Launch Mini-NIDS"; Flags: nowait postinstall skipifsilent
