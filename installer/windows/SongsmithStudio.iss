; Songsmith Studio - one Windows installer for every plugin (Inno Setup 6).
;
; Build it with build-installer.ps1, which reads every version and passes the build folders:
;   iscc /DSuiteVersion=1.0.0 /DEqVersion=0.5.0 /DFetVersion=0.1.0 /DMaxVersion=0.1.0
;        /DEqDir=<...\SongsmithEQ_artefacts\Release> /DFetDir=<...> /DMaxDir=<...> /O<out> SongsmithStudio.iss
;
; Each plugin is one component, ticked by default, and brings its standalone app with it.
; Every plugin goes where its own installer puts it: the VST3 into the fixed
; VST3 folder (Common Files\VST3), the app into Program Files\Songsmith Studio\<plugin>.
; Not code-signed (see README), so SmartScreen warns on first run.
;
; Living with the per-plugin installers: when a plugin that its own installer put there is
; ticked, that installer's uninstaller runs silently first, so there is one copy and one entry
; in Settings > Apps. Running a newer suite installer upgrades in place, remembers the ticks,
; and removes plugins that were installed by the suite and are now unticked.
;
; Test-only command-line parameter (used by test-installer.ps1, not for users):
;   /VST3DIR=<folder>  install the VST3 bundles there instead of Common Files\VST3. Accepted
;                      only together with /CURRENTUSER, so a test never needs admin rights and
;                      never touches the real VST3 folder.

#ifndef SuiteVersion
  #error Pass the versions and build folders (build-installer.ps1 does this)
#endif

#define AppName      "Songsmith Studio"
#define AppPublisher "Songsmith Studio"
#define AppURL       "https://github.com/tonygward/audio-plugins"

; NEVER CHANGE THIS. It is how Windows and newer suite installers recognise an existing install,
; so upgrades replace it in place and it appears once in Settings > Apps.
#define SuiteAppId   "9AC9C845-D81C-4667-BBD4-C412BC291211"
; "{{" is Inno Setup's escaped "{" in [Setup].
#define SuiteAppIdSetup "{{9AC9C845-D81C-4667-BBD4-C412BC291211}"

; The per-plugin installers' AppIds (from eq/, analog-compressor/ and maximiser/installer/*.iss): an install
; made by one of those is replaced when the suite installs the same plugin.
#define EqAppId      "2F8C1884-56C1-45D8-A78D-CF117EC69B44"
#define FetAppId     "FF3BD6A4-58D4-4AFA-BFA3-26B221E29340"
#define MaxAppId     "A888B7E4-3776-4CCD-9FA2-3B5CC0415C6C"

#define EqName       "Songsmith EQ"
#define FetName      "Songsmith Analog Compressor"
#define MaxName      "Songsmith Maximiser"

[Setup]
AppId={#SuiteAppIdSetup}
AppName={#AppName}
AppVersion={#SuiteVersion}
AppVerName={#AppName} {#SuiteVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
AppComments={#EqName} {#EqVersion}, {#FetName} {#FetVersion}, {#MaxName} {#MaxVersion}
VersionInfoVersion={#SuiteVersion}
VersionInfoProductName={#AppName}
VersionInfoCompany={#AppPublisher}
; The Settings > Apps entry names every plugin's own version.
UninstallDisplayName={#AppName} (EQ {#EqVersion}, Analog Compressor {#FetVersion}, Maximiser {#MaxVersion})

; The standalone apps (each in its own folder) and the uninstaller live here.
DefaultDirName={autopf}\{#AppPublisher}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableReadyPage=no
; Remember the ticks from the last install (the setup type and its components).
UsePreviousSetupType=yes

; Writing to Common Files\VST3 needs admin rights. /CURRENTUSER is allowed only for the tests.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; A host with a plugin loaded is handled in PrepareToInstall with a clear message, rather than
; by the Restart Manager closing the user's recording software (and unsaved work) for them.
CloseApplications=no
RestartIfNeededByRun=no

OutputBaseFilename=Songsmith-Studio-{#SuiteVersion}-win64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Messages]
SetupAppTitle=Songsmith Studio plugins
SetupWindowTitle=Songsmith Studio plugins %1
WelcomeLabel1=Songsmith Studio plugins
WelcomeLabel2=This installs the Songsmith Studio plugins on your computer:%n%n    {#EqName} {#EqVersion}%n    {#FetName} {#FetVersion}%n    {#MaxName} {#MaxVersion}%n%nOn the next page, untick any plugin you don't want. Each plugin goes into the standard VST3 folder (Common Files\VST3), where every VST3 host looks for it, and comes with a standalone app that runs on its own.%n%nPlease close your recording software before continuing.
SelectComponentsLabel2=Tick the plugins to install. Unticking a plugin that is already installed removes it. Your presets are always kept.
FinishedLabel=Setup has installed the Songsmith Studio plugins on your computer.%n%nRescan plugins in your recording software (or restart it) to find them under Songsmith Studio.

[Types]
Name: "full";   Description: "Every plugin"
Name: "custom"; Description: "Choose plugins"; Flags: iscustom

[Components]
; The VST3 comes with its plugin and can't be unticked on its own: untick the plugin instead.
Name: "eq";             Description: "{#EqName} {#EqVersion}: shape the tone of a track";                        Types: full custom; Flags: checkablealone
Name: "fet";            Description: "{#FetName} {#FetVersion}: even out the level and add analogue warmth";     Types: full custom; Flags: checkablealone
Name: "max";            Description: "{#MaxName} {#MaxVersion}: make a finished mix louder without clipping";    Types: full custom; Flags: checkablealone

[InstallDelete]
; Replace each ticked plugin's bundle cleanly: files from an older version must not linger.
Type: filesandordirs; Name: "{code:Vst3Dir}\{#EqName}.vst3";  Components: eq
Type: filesandordirs; Name: "{code:Vst3Dir}\{#FetName}.vst3"; Components: fet
Type: filesandordirs; Name: "{code:Vst3Dir}\{#MaxName}.vst3"; Components: max
; Songsmith EQ's earlier names. Same plugin ID, so they'd show up twice.
Type: filesandordirs; Name: "{code:Vst3Dir}\TW Pro EQ.vst3";  Components: eq
Type: filesandordirs; Name: "{code:Vst3Dir}\3-Band EQ.vst3";  Components: eq

; Unticked since the last suite install: remove what the suite installed before. (Only if the
; suite installed it: a plugin from its own installer is never touched by an unticked box.)
Type: filesandordirs; Name: "{code:Vst3Dir}\{#EqName}.vst3";      Components: not eq;  Check: SuiteHad ('eq')
Type: filesandordirs; Name: "{code:Vst3Dir}\{#FetName}.vst3";     Components: not fet; Check: SuiteHad ('fet')
Type: filesandordirs; Name: "{code:Vst3Dir}\{#MaxName}.vst3";     Components: not max; Check: SuiteHad ('max')
; ...and its standalone app and Start menu shortcut go with it. An earlier suite 1.x install
; may have recorded "eq\standalone" as well; the plugin's own entry is the one that decides.
Type: files;          Name: "{app}\{#EqName}\{#EqName}.exe";      Components: not eq;  Check: SuiteHad ('eq')
Type: files;          Name: "{app}\{#FetName}\{#FetName}.exe";    Components: not fet; Check: SuiteHad ('fet')
Type: files;          Name: "{app}\{#MaxName}\{#MaxName}.exe";    Components: not max; Check: SuiteHad ('max')
Type: files;          Name: "{autoprograms}\{#EqName}.lnk";       Components: not eq;  Check: SuiteHad ('eq')
Type: files;          Name: "{autoprograms}\{#FetName}.lnk";      Components: not fet; Check: SuiteHad ('fet')
Type: files;          Name: "{autoprograms}\{#MaxName}.lnk";      Components: not max; Check: SuiteHad ('max')
Type: dirifempty;     Name: "{app}\{#EqName}";                    Components: not eq;  Check: SuiteHad ('eq')
Type: dirifempty;     Name: "{app}\{#FetName}";                   Components: not fet; Check: SuiteHad ('fet')
Type: dirifempty;     Name: "{app}\{#MaxName}";                   Components: not max; Check: SuiteHad ('max')

[Dirs]
; The shared VST3 folder holds other plugins: never remove it on uninstall.
Name: "{code:Vst3Dir}"; Flags: uninsneveruninstall

[Files]
Source: "{#EqDir}\VST3\{#EqName}.vst3\*";   DestDir: "{code:Vst3Dir}\{#EqName}.vst3";  Components: eq;  Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#FetDir}\VST3\{#FetName}.vst3\*"; DestDir: "{code:Vst3Dir}\{#FetName}.vst3"; Components: fet; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MaxDir}\VST3\{#MaxName}.vst3\*"; DestDir: "{code:Vst3Dir}\{#MaxName}.vst3"; Components: max; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#EqDir}\Standalone\{#EqName}.exe";   DestDir: "{app}\{#EqName}";  Components: eq;  Flags: ignoreversion
Source: "{#FetDir}\Standalone\{#FetName}.exe"; DestDir: "{app}\{#FetName}"; Components: fet; Flags: ignoreversion
Source: "{#MaxDir}\Standalone\{#MaxName}.exe"; DestDir: "{app}\{#MaxName}"; Components: max; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#EqName}";  Filename: "{app}\{#EqName}\{#EqName}.exe";   Components: eq
Name: "{autoprograms}\{#FetName}"; Filename: "{app}\{#FetName}\{#FetName}.exe"; Components: fet
Name: "{autoprograms}\{#MaxName}"; Filename: "{app}\{#MaxName}\{#MaxName}.exe"; Components: max

; Uninstall removes only what Setup installed (the bundles, the apps, the shortcuts). User
; presets in %APPDATA%\Songsmith Studio\<plugin> (and the old %APPDATA%\TW Pro EQ) belong to
; the user and are never touched.

[Code]
const
  GENERIC_READ  = $80000000;
  GENERIC_WRITE = $40000000;
  OPEN_EXISTING = 3;
  UninstallKey  = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\';

function CreateFileW (lpFileName: String; dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                      dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile: Cardinal): Cardinal;
  external 'CreateFileW@kernel32.dll stdcall';
function CloseHandle (hObject: Cardinal): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

{ Where the VST3 bundles go: the fixed VST3 folder, or /VST3DIR= in tests. }
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
    SuppressibleMsgBox ('The Songsmith Studio plugins must be installed for all users (the VST3 folder needs administrator rights). Run Setup without /CURRENTUSER.',
                        mbCriticalError, MB_OK, IDOK);
    Result := False;
  end;
end;

{ --- What is installed now ---------------------------------------------------------------- }

{ The components the last suite install selected, as ",eq,fet," (empty if none). Installs made by suite 1.x may also list "eq\standalone" and
  friends; those names are ignored, because each plugin now brings its app with it. }
function PreviousComponents(): String;
var
  S: String;
begin
  Result := '';
  if RegQueryStringValue (HKA, UninstallKey + '{' + '{#SuiteAppId}' + '}_is1', 'Inno Setup: Selected Components', S) then
    Result := ',' + Lowercase (S) + ',';
end;

{ True if the last suite install installed this component. }
function SuiteHad (Component: String): Boolean;
begin
  Result := Pos (',' + Component + ',', PreviousComponents()) > 0;
end;

{ The uninstaller of a per-plugin install (its own installer), or '' if there is none. }
function PerPluginUninstaller (AppId: String): String;
var
  S: String;
begin
  Result := '';
  if RegQueryStringValue (HKA, UninstallKey + '{' + AppId + '}_is1', 'UninstallString', S)
     or RegQueryStringValue (HKLM, UninstallKey + '{' + AppId + '}_is1', 'UninstallString', S)
     or RegQueryStringValue (HKCU, UninstallKey + '{' + AppId + '}_is1', 'UninstallString', S) then
    Result := RemoveQuotes (S);
  if (Result <> '') and not FileExists (Result) then
    Result := '';
end;

function Selected (Component: String): Boolean;
begin
  Result := WizardIsComponentSelected (Component);
end;

{ True if the plugin will be removed: the suite installed it last time and it is now unticked. }
function Removing (Component: String): Boolean;
begin
  Result := SuiteHad (Component) and not Selected (Component);
end;

{ --- Plugins in use ------------------------------------------------------------------------- }

{ True if 'Path' exists and another process holds it open (a host has the plugin loaded, or the
  standalone app is running). Same test as the per-plugin installers: exclusive open. }
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

{ Why this plugin can't be changed now, or '' if it can. }
function InUse (Component, Name: String): String;
begin
  Result := '';
  if not (Selected (Component) or Removing (Component)) then
    Exit;
  if IsLocked (BundleDll (Name + '.vst3'))
     or ((Component = 'eq') and (IsLocked (BundleDll ('TW Pro EQ.vst3')) or IsLocked (BundleDll ('3-Band EQ.vst3')))) then
    Result := Name + ' is in use: your recording software or another program has the plugin loaded.'
  else if IsLocked (ExpandConstant ('{app}\') + Name + '\' + Name + '.exe') then
    Result := 'The ' + Name + ' standalone app is running.';
end;

{ --- Replacing a per-plugin install ------------------------------------------------------- }

{ Runs a per-plugin uninstaller silently and waits until it has finished. (Inno Setup
  uninstallers copy themselves to a temp folder and relaunch, so waiting for the first process
  isn't enough: wait until the uninstaller file itself is gone.) }
function RunPerPluginUninstaller (Uninstaller: String): Boolean;
var
  Code, Waited: Integer;
begin
  Result := Exec (Uninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, Code);
  Waited := 0;
  while Result and FileExists (Uninstaller) and (Waited < 60000) do
  begin
    Sleep (250);
    Waited := Waited + 250;
  end;
  Result := Result and not FileExists (Uninstaller);
  Log ('Per-plugin uninstaller ' + Uninstaller + ': ' + IntToStr (Code));
end;

function ReplacePerPlugin (Component, Name, AppId: String): String;
var
  Uninstaller: String;
begin
  Result := '';
  Uninstaller := PerPluginUninstaller (AppId);
  if Selected (Component) and (Uninstaller <> '') then
    if not RunPerPluginUninstaller (Uninstaller) then
      Result := 'Setup couldn''t remove the ' + Name + ' installed by its own installer. Uninstall it from Settings > Apps and run Setup again.';
end;

{ --- Pages ----------------------------------------------------------------------------------- }

function NothingSelected(): Boolean;
begin
  Result := not (Selected ('eq') or Selected ('fet') or Selected ('max'));
end;

function NextButtonClick (CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) and NothingSelected() then
  begin
    MsgBox ('Tick at least one plugin to install.', mbError, MB_OK);
    Result := False;
  end;
end;

{ The Ready page says which separate installs will be replaced and which plugins removed. }
function UpdateReadyMemo (Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
                          MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  Replaced, Removed: String;
begin
  Replaced := '';
  if Selected ('eq') and (PerPluginUninstaller ('{#EqAppId}') <> '') then Replaced := Replaced + Space + '{#EqName}' + NewLine;
  if Selected ('fet') and (PerPluginUninstaller ('{#FetAppId}') <> '') then Replaced := Replaced + Space + '{#FetName}' + NewLine;
  if Selected ('max') and (PerPluginUninstaller ('{#MaxAppId}') <> '') then Replaced := Replaced + Space + '{#MaxName}' + NewLine;

  Removed := '';
  if Removing ('eq') then Removed := Removed + Space + '{#EqName}' + NewLine;
  if Removing ('fet') then Removed := Removed + Space + '{#FetName}' + NewLine;
  if Removing ('max') then Removed := Removed + Space + '{#MaxName}' + NewLine;

  Result := MemoComponentsInfo;
  if Replaced <> '' then
    Result := Result + NewLine + NewLine + 'Installed on its own before (its separate entry in Settings > Apps is replaced by this one):' + NewLine + Replaced;
  if Removed <> '' then
    Result := Result + NewLine + NewLine + 'Will be removed (unticked; your presets are kept):' + NewLine + Removed;
end;

{ Runs before anything is changed: refuse cleanly instead of a half-finished install or a
  "restart to finish" surprise. In silent mode Setup exits with code 7. Then, once nothing is
  in use, remove the per-plugin installs the suite replaces. }
function PrepareToInstall (var NeedsRestart: Boolean): String;
begin
  Result := '';
  if NothingSelected() then
  begin
    Result := 'No plugin is ticked, so there is nothing to install. Nothing has been changed.';
    Exit;
  end;

  Result := InUse ('eq', '{#EqName}');
  if Result = '' then Result := InUse ('fet', '{#FetName}');
  if Result = '' then Result := InUse ('max', '{#MaxName}');
  if Result <> '' then
  begin
    Result := Result + #13#10#13#10 + 'Close it and run Setup again. Nothing has been changed.';
    Exit;
  end;

  Result := ReplacePerPlugin ('eq', '{#EqName}', '{#EqAppId}');
  if Result = '' then Result := ReplacePerPlugin ('fet', '{#FetName}', '{#FetAppId}');
  if Result = '' then Result := ReplacePerPlugin ('max', '{#MaxName}', '{#MaxAppId}');
end;
