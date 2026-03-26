; OBS Lite AMD Edition - Inno Setup installer script
; Packages the CMake build output into a distributable installer

#define MyAppName "OBS Lite AMD Edition"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "George Karagioules"
#define MyAppExeName "obs64.exe"
#define MyAppId "{{E7A3F1B2-5D8C-4A6E-9F0B-3C7D2E1A4B5F}"
#define BuildDir "build_amd_lite\rundir\RelWithDebInfo"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\OBS Lite AMD Edition
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=dist-installer
OutputBaseFilename=OBS_Lite_AMD_Edition_Setup
Compression=lzma2
SolidCompression=yes
SetupIconFile=frontend\cmake\windows\obs-studio.ico
UninstallDisplayIcon={app}\bin\64bit\obs-studio.ico
WizardStyle=modern
WizardImageFile=Logo.png
WizardSmallImageFile=Logo-removebg-preview.png
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
CloseApplications=force
; Portable config lives next to the EXE, so install cleanly
AlwaysRestart=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checked
Name: "startmenu"; Description: "Create a Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checked

[Files]
; Entire OBS build output (bin, data, obs-plugins directories)
Source: "{#BuildDir}\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\obs-plugins\*"; DestDir: "{app}\obs-plugins"; Flags: ignoreversion recursesubdirs createallsubdirs
; Icon for uninstaller display
Source: "frontend\cmake\windows\obs-studio.ico"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
; Logo for About/branding
Source: "Logo-removebg-preview.png"; DestDir: "{app}\data"; DestName: "logo.png"; Flags: ignoreversion
; Create portable_mode sentinel so settings stay local to install dir
Source: "installer_assets\portable_mode"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; Pre-create config directory for portable settings
Name: "{app}\config"; Permissions: users-modify

[Icons]
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\bin\64bit\{#MyAppExeName}"; IconFilename: "{app}\bin\64bit\obs-studio.ico"; Tasks: desktopicon
Name: "{group}\{#MyAppName}"; Filename: "{app}\bin\64bit\{#MyAppExeName}"; IconFilename: "{app}\bin\64bit\obs-studio.ico"; Tasks: startmenu
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\64bit\{#MyAppExeName}"; Description: "Launch OBS Lite AMD Edition"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Clean up config/logs on uninstall (user data)
Type: filesandordirs; Name: "{app}\config"
