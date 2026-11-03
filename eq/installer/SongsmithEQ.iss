; Songsmith EQ - Windows installer (Inno Setup 6).
;
; Build it with build-installer.ps1, which passes the version and the build folder:
;   iscc /DAppVersion=0.2.0 /DBuildDir=<eq\build\SongsmithEQ_artefacts\Release> /O<out> SongsmithEQ.iss
;
; Installs the VST3 into the fixed VST3 folder (Common Files\VST3) and, optionally, the
; standalone app into Program Files\Songsmith Studio\Songsmith EQ. Not code-signed (see
; ROADMAP "Out of scope"), so SmartScreen warns on first run.
;
; Test-only command-line parameter (used by test-installer.ps1, not for users):
;   /VST3DIR=<folder>  install the VST3 there instead of Common Files\VST3. It's only accepted
;                      together with /CURRENTUSER, so a test never needs admin rights and
;                      never touches the real VST3 folder.

#ifndef AppVersion
  #error Pass the version: iscc /DAppVersion=x.y.z (build-installer.ps1 does this)
#endif
#ifndef BuildDir
  #error Pass the Release artefacts folder: iscc /DBuildDir=...\SongsmithEQ_artefacts\Release
#endif

#define AppName      "Songsmith EQ"
#define AppPublisher "Songsmith Studio"
#define AppURL       "https://github.com/tonygward/audio-plugins"
#define BundleName   "Songsmith EQ.vst3"
#define StandaloneExe "Songsmith EQ.exe"

[Setup]
; NEVER CHANGE THIS AppId. It is how Windows and newer installers recognise an existing
; Songsmith EQ install, so upgrades replace it in place and it appears once in "Installed apps".
AppId={{2F8C1884-56C1-45D8-A78D-CF117EC69B44}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoCompany={#AppPublisher}
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#StandaloneExe}

; The standalone app (and the uninstaller) live here.
DefaultDirName={autopf}\{#AppPublisher}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableReadyPage=yes

; Writing to Common Files\VST3 needs admin rights. /CURRENTUSER is allowed only for the
; tests (see InitializeSetup), which redirect every folder into a temp folder.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; A host with the plugin loaded is handled in PrepareToInstall with a clear message, rather
; than by the Restart Manager closing the user's DAW (and unsaved work) for them.
CloseApplications=no
RestartIfNeededByRun=no

OutputBaseFilename=Songsmith-EQ-{#AppVersion}-win64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Messages]
WelcomeLabel2=This installs [name/ver] on your computer.%n%nThe VST3 plugin goes into the standard VST3 folder (Common Files\VST3), where every VST3 host looks for it. You can also install the standalone app.%n%nPlease close your DAW before continuing.%n%nUpgrading from TW Pro EQ? Your sessions and presets carry over, and the old TW Pro EQ plugin is removed so it doesn't show up twice.
FinishedLabel=Setup has installed [name] on your computer.%n%nRescan plugins in your DAW (or restart it) to find Songsmith EQ under Songsmith Studio.

[Types]
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin";    Types: custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: standalone; Flags: unchecked

[InstallDelete]
; Replace the bundle cleanly: files from an older version must not linger.
Type: filesandordirs; Name: "{code:Vst3Dir}\{#BundleName}"
; Legacy bundles: earlier names of this plugin. Same plugin ID, so they'd show up twice.
Type: filesandordirs; Name: "{code:Vst3Dir}\TW Pro EQ.vst3"
Type: filesandordirs; Name: "{code:Vst3Dir}\3-Band EQ.vst3"
; Standalone app unticked on an upgrade: remove the copy an earlier install left.
Type: files; Name: "{app}\{#StandaloneExe}";            Components: not standalone
Type: files; Name: "{autoprograms}\{#AppName}.lnk";     Components: not standalone
Type: files; Name: "{autodesktop}\{#AppName}.lnk";      Components: not standalone

[Dirs]
; The shared VST3 folder holds other plugins: never remove it on uninstall.
Name: "{code:Vst3Dir}"; Flags: uninsneveruninstall

[Files]
Source: "{#BuildDir}\VST3\{#BundleName}\*"; DestDir: "{code:Vst3Dir}\{#BundleName}"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\{#StandaloneExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#StandaloneExe}"; Components: standalone
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#StandaloneExe}"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\{#StandaloneExe}"; Description: "Open the Songsmith EQ standalone app"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

; Uninstall removes only what Setup installed (the bundle, the app, the shortcuts). User
; presets in %APPDATA%\Songsmith Studio\Songsmith EQ\Presets (and the old
; %APPDATA%\TW Pro EQ\Presets) belong to the user and are never touched.

[Code]
const
  GENERIC_READ  = $80000000;
  GENERIC_WRITE = $40000000;
  OPEN_EXISTING = 3;

function CreateFileW (lpFileName: String; dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                      dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile: Cardinal): Cardinal;
  external 'CreateFileW@kernel32.dll stdcall';
function CloseHandle (hObject: Cardinal): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

{ Where the VST3 goes: the fixed VST3 folder, or /VST3DIR= in tests. }
function Vst3Dir (Param: String): String;
begin
  Result := ExpandConstant ('{param:VST3DIR|{commoncf64}\VST3}');
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  { Per-user mode exists only for the tests: a real install must reach Common Files. }
  if not IsAdminInstallMode and (ExpandConstant ('{param:VST3DIR|}') = '') then
  begin
    SuppressibleMsgBox ('Songsmith EQ must be installed for all users (the VST3 folder needs administrator rights). Run Setup without /CURRENTUSER.',
                        mbCriticalError, MB_OK, IDOK);
    Result := False;
  end;
end;

{ True if 'Path' exists and another process holds it open (a host has the plugin loaded,
  or the standalone app is running). Same test as install.ps1: exclusive read/write open. }
function IsLocked (Path: String): Boolean;
var
  H: Cardinal;
begin
  Result := False;
  if not FileExists (Path) then
    Exit;
  H := CreateFileW (Path, GENERIC_READ or GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
  if H = $FFFFFFFF then
    Result := True
  else
    CloseHandle (H);
end;

function BundleDll (Bundle: String): String;
begin
  Result := Vst3Dir ('') + '\' + Bundle + '\Contents\x86_64-win\' + Bundle;
end;

{ Runs before anything is changed: refuse cleanly instead of a half-finished install or
  a "restart to finish" surprise. In silent mode Setup exits with code 7. }
function PrepareToInstall (var NeedsRestart: Boolean): String;
begin
  Result := '';
  if IsLocked (BundleDll ('{#BundleName}')) or IsLocked (BundleDll ('TW Pro EQ.vst3'))
     or IsLocked (BundleDll ('3-Band EQ.vst3')) then
    Result := 'Songsmith EQ is in use: a DAW or another program has the plugin loaded.' + #13#10#13#10 +
              'Close your DAW and run Setup again. Nothing has been changed.'
  else if IsLocked (ExpandConstant ('{app}\{#StandaloneExe}')) then
    Result := 'The Songsmith EQ standalone app is running.' + #13#10#13#10 +
              'Close it and run Setup again. Nothing has been changed.';
end;
