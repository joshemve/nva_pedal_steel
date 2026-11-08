; NVA Pedal Steel installer (Inno Setup 6). Build: ISCC.exe installer\NVA_Pedal_Steel.iss
; Installs the VST3 into the standard Common Files\VST3 folder (where DAWs look) and the Standalone app.
#define AppName "NVA Pedal Steel"
#define AppVersion "1.2.0"
#define Root ".."

[Setup]
AppId={{6E4C2B1A-9F3D-4E7A-B2C5-NVAPEDALSTEEL}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Northern Valley Audio
DefaultDirName={autopf}\Northern Valley Audio\{#AppName}
DefaultGroupName=Northern Valley Audio
DisableProgramGroupPage=yes
OutputDir={#Root}\build\installer
OutputBaseFilename=NVA_Pedal_Steel_{#AppVersion}_Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#Root}\build\PedalSteel_artefacts\Release\VST3\NVA Pedal Steel.vst3\*"; DestDir: "{commoncf64}\VST3\NVA Pedal Steel.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\build\PedalSteel_artefacts\Release\Standalone\NVA Pedal Steel.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\NVA Pedal Steel.exe"; Components: standalone
