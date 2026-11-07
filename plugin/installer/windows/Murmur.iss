; Inno Setup script for Murmur (Windows). Build with installer\windows\build-windows.bat
; or: ISCC.exe /DMyAppVersion=0.1.0 installer\windows\Murmur.iss
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#define MyAppName "Murmur"
#define MyPublisher "Murmur Audio"
#define ArtDir "..\..\build-win\Murmur_artefacts\Release"

[Setup]
AppId={{6C1E2B7A-4D3F-4E8B-9A51-2F7C0D9E8B14}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=Murmur-{#MyAppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Types]
Name: "full"; Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, Reaper, FL Studio, Bitwig, Cubase)"; Types: full custom
Name: "app";  Description: "Standalone app"; Types: full custom

[Files]
; VST3 is a folder bundle; install it into the standard shared VST3 location
Source: "{#ArtDir}\VST3\Murmur.vst3\*"; DestDir: "{commoncf64}\VST3\Murmur.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtDir}\Standalone\Murmur.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Murmur"; Filename: "{app}\Murmur.exe"; Components: app
Name: "{group}\Uninstall Murmur"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Murmur.exe"; Description: "Launch Murmur"; Flags: nowait postinstall skipifsilent; Components: app
