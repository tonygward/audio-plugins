; Songsmith Analog Compressor - Windows installer (Inno Setup 6).
;
; Built with build-installer.ps1, which passes the version and the build folder:
;   iscc /DAppVersion=<VERSION> /DBuildDir=<analog-compressor\build\SongsmithAnalogCompressor_artefacts\Release> /O<out> SongsmithAnalogCompressor.iss
;
; Same pattern as eq\installer\SongsmithEQ.iss: the VST3 goes into the fixed VST3 folder
; (Common Files\VST3) and the standalone app into
; Program Files\Songsmith Studio\Songsmith Analog Compressor. Not code-signed.
;
; It is a separate product from Songsmith EQ (its own AppId, bundle and folders): installing,
; upgrading or uninstalling it never touches Songsmith EQ or anyone's presets.
;
; Test-only command-line parameter (used by test-installer.ps1, not for users):
;   /VST3DIR=<folder>  install the VST3 there instead of Common Files\VST3. Only accepted
;                      together with /CURRENTUSER.

#ifndef AppVersion
  #error Pass the version: iscc /DAppVersion=x.y.z (build-installer.ps1 does this)
#endif
#ifndef BuildDir
  #error Pass the Release artefacts folder: iscc /DBuildDir=...\SongsmithAnalogCompressor_artefacts\Release
#endif

#define AppName      "Songsmith Analog Compressor"
#define AppPublisher "Songsmith Studio"
#define AppURL       "https://github.com/tonygward/audio-plugins"
#define BundleName   "Songsmith Analog Compressor.vst3"
#define StandaloneExe "Songsmith Analog Compressor.exe"

[Setup]
; NEVER CHANGE THIS AppId (and never reuse Songsmith EQ's). It is how Windows and newer
; installers recognise an existing install, so upgrades replace it in place.
AppId={{FF3BD6A4-58D4-4AFA-BFA3-26B221E29340}
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

DefaultDirName={autopf}\{#AppPublisher}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableReadyPage=yes

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

CloseApplications=no
RestartIfNeededByRun=no

OutputBaseFilename=Songsmith-Analog-Compressor-{#AppVersion}-win64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Messages]
WelcomeLabel2=This installs [name/ver] on your computer.%n%nThe VST3 plugin goes into the standard VST3 folder (Common Files\VST3), where every VST3 host looks for it. It also installs a standalone app that runs without recording software.%n%nPlease close your DAW before continuing. Songsmith EQ, if you have it, isn't affected.
FinishedLabel=Setup has installed [name] on your computer.%n%nRescan plugins in your DAW (or restart it) to find Songsmith Analog Compressor under Songsmith Studio.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Flags: unchecked

[InstallDelete]
; Replace the bundle cleanly: files from an older version must not linger.
Type: filesandordirs; Name: "{code:Vst3Dir}\{#BundleName}"

[Dirs]
; The shared VST3 folder holds other plugins (Songsmith EQ among them): never remove it.
Name: "{code:Vst3Dir}"; Flags: uninsneveruninstall

[Files]
Source: "{#BuildDir}\VST3\{#BundleName}\*"; DestDir: "{code:Vst3Dir}\{#BundleName}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#StandaloneExe}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#StandaloneExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#StandaloneExe}"; Description: "Open the Songsmith Analog Compressor standalone app"; Flags: nowait postinstall skipifsilent unchecked

; Uninstall removes only what Setup installed. User presets in
; %APPDATA%\Songsmith Studio\Songsmith Analog Compressor\Presets belong to the user and are never
; touched, and neither is anything of Songsmith EQ's.

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

function Vst3Dir (Param: String): String;
begin
  Result := ExpandConstant ('{param:VST3DIR|{commoncf64}\VST3}');
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not IsAdminInstallMode and (ExpandConstant ('{param:VST3DIR|}') = '') then
  begin
    SuppressibleMsgBox ('Songsmith Analog Compressor must be installed for all users (the VST3 folder needs administrator rights). Run Setup without /CURRENTUSER.',
                        mbCriticalError, MB_OK, IDOK);
    Result := False;
  end;
end;

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

function PrepareToInstall (var NeedsRestart: Boolean): String;
begin
  Result := '';
  if IsLocked (Vst3Dir ('') + '\{#BundleName}\Contents\x86_64-win\{#BundleName}') then
    Result := 'Songsmith Analog Compressor is in use: a DAW or another program has the plugin loaded.' + #13#10#13#10 +
              'Close your DAW and run Setup again. Nothing has been changed.'
  else if IsLocked (ExpandConstant ('{app}\{#StandaloneExe}')) then
    Result := 'The Songsmith Analog Compressor standalone app is running.' + #13#10#13#10 +
              'Close it and run Setup again. Nothing has been changed.';
end;
