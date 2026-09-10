; Inno Setup script for Small Controls for OBS
;
; Build with:  build-installer.ps1   (or)   ISCC.exe /DVersion=1.0.0 small-controls-for-obs.iss
; Expects the plugin to have been staged first with
;   cmake --install build_x64 --config RelWithDebInfo --prefix release
; so that release\small-controls-for-obs\{bin,data} exist.

#ifndef Version
  #define Version "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\release\small-controls-for-obs"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\release"
#endif

#define AppName "Small Controls for OBS"
#define PluginId "small-controls-for-obs"
#define Publisher "Nathan V"
#define URL "https://github.com/nathan-v/small-controls-for-obs"

[Setup]
AppId={{1E08A7C9-1D7F-4992-B0E3-BB710B182A82}
AppName={#AppName}
AppVersion={#Version}
AppVerName={#AppName} {#Version}
AppPublisher={#Publisher}
AppPublisherURL={#URL}
AppSupportURL={#URL}
AppUpdatesURL={#URL}
; Plugins live in the machine-wide OBS plugin folder that OBS 28+ scans:
;   C:\ProgramData\obs-studio\plugins\<plugin>\bin\64bit
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginId}
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName={#AppName}
UninstallFilesDir={app}
OutputDir={#OutputDir}
OutputBaseFilename={#PluginId}-{#Version}-windows-x64-installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; OBS's own installer asks for admin; allow a per-user override for portable-ish setups.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
; OBS holds this mutex while running; Setup refuses to continue until it is closed.
AppMutex=OBSStudioCore
LicenseFile=..\..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\bin\64bit\{#PluginId}.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\bin\64bit\{#PluginId}.pdb"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Messages]
; Shown when the AppMutex is held.
SetupAppRunningError=OBS Studio is running.%n%nPlease close OBS Studio, then click OK to continue or Cancel to exit.

[Code]
// Warn (but do not block) if OBS Studio does not appear to be installed.
function InitializeSetup(): Boolean;
var
  ObsPath: String;
begin
  Result := True;
  if not RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', ObsPath) and
     not RegQueryStringValue(HKCU, 'SOFTWARE\OBS Studio', '', ObsPath) and
     not DirExists(ExpandConstant('{commonpf64}\obs-studio')) then
  begin
    if MsgBox('OBS Studio was not found on this computer.' + #13#10 + #13#10 +
              'The plugin will be installed to the OBS plugin folder anyway, ' +
              'and OBS will pick it up once installed. Continue?',
              mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
  end;
end;
