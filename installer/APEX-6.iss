; Inno Setup script - builds APEX-6-Setup.exe
[Setup]
AppName=APEX-6 Guitar Tone Matrix
AppVersion=0.1.0
AppPublisher=TRANCE-6
DefaultDirName={autopf}\APEX-6 Guitar Tone Matrix
DefaultGroupName=APEX-6 Guitar Tone Matrix
OutputBaseFilename=APEX-6-Setup
OutputDir=Output
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
DisableProgramGroupPage=yes
WizardStyle=modern

[Files]
; The VST3 plugin goes where every DAW (including FL Studio) looks for it
Source: "..\build\Apex6_artefacts\Release\VST3\APEX-6 Guitar Tone Matrix.vst3\*"; DestDir: "{commoncf64}\VST3\APEX-6 Guitar Tone Matrix.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Stand-alone app for playing it without a DAW
Source: "..\build\Apex6_artefacts\Release\Standalone\APEX-6 Guitar Tone Matrix.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\APEX-6 Guitar Tone Matrix (stand-alone)"; Filename: "{app}\APEX-6 Guitar Tone Matrix.exe"

[Run]
Filename: "{app}\APEX-6 Guitar Tone Matrix.exe"; Description: "Try APEX-6 Guitar Tone Matrix now (stand-alone)"; Flags: nowait postinstall skipifsilent unchecked
