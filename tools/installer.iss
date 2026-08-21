#define AppName       "QontrolPanel"
#define AppSourceDir  "..\build\QontrolPanel\"
#define AppExeName    "QontrolPanel.exe"
#define                MajorVersion
#define                MinorVersion
#define                RevisionVersion
#define                BuildVersion
#define TempVersion    GetVersionComponents(AppSourceDir + "bin\" + AppExeName, MajorVersion, MinorVersion, RevisionVersion, BuildVersion)
#define AppVersion     str(MajorVersion) + "." + str(MinorVersion) + "." + str(RevisionVersion)
#define AppPublisher  "ChrisLauinger77"
#define AppURL        "https://github.com/ChrisLauinger77/QontrolPanel"
#define AppIcon       "..\Resources\icons\icon.ico"
#define CurrentYear   GetDateTimeString('yyyy','','')

; MSVC redistributable
#define VCRedistURL "https://aka.ms/vs/17/release/vc_redist.x64.exe"
#define VCRedistFile "vc_redist.x64.exe"

; Windows App Runtime
#define WindowsAppRuntimeURL "https://aka.ms/windowsappsdk/2.4/2.4.0/windowsappruntimeinstall-x64.exe"
#define WindowsAppRuntimeFile "windowsappruntimeinstall-x64.exe"
#define WindowsAppRuntimeSHA256 "851c35b0b0a59ce4c55f9171f601193322fc3413143b0dc3390ea11e14cfa7fc"

[Setup]
AppId={{8A9C6942-5CA3-4A02-B701-E7B4E862D635}}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}

VersionInfoDescription={#AppName} installer
VersionInfoProductName={#AppName}
VersionInfoVersion={#AppVersion}

AppCopyright=(c) {#CurrentYear} {#AppPublisher}

UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\bin\{#AppExeName}
AppPublisher={#AppPublisher}

AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

ShowLanguageDialog=yes
UsePreviousLanguage=no
LanguageDetectionMethod=uilanguage

WizardStyle=modern

DisableProgramGroupPage=yes
DisableWelcomePage=yes

SetupIconFile={#AppIcon}

DefaultGroupName={#AppName}
DefaultDirName={localappdata}\Programs\{#AppName}

PrivilegesRequired=lowest
OutputBaseFilename=QontrolPanel_installer
Compression=lzma
SolidCompression=yes
UsedUserAreasWarning=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english";    MessagesFile: "compiler:Default.isl"
Name: "french";     MessagesFile: "compiler:Languages\French.isl"
Name: "german";     MessagesFile: "compiler:Languages\German.isl"
Name: "italian";    MessagesFile: "compiler:Languages\Italian.isl"
Name: "korean";     MessagesFile: "compiler:Languages\Korean.isl"
Name: "russian";    MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#AppSourceDir}*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\{#VCRedistFile}"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"; IconFilename: "{app}\bin\{#AppExeName}"; AppUserModelID: "ChrisLauinger77.QontrolPanel"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\bin\{#AppExeName}"; AppUserModelID: "ChrisLauinger77.QontrolPanel"

[Run]
Filename: "{tmp}\{#VCRedistFile}"; Parameters: "/quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ Redistributable..."; Check: VCRedistNeedsInstall
Filename: "{app}\bin\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function VCRedistNeedsInstall: Boolean;
var
  Version: String;
begin
  // Check if VC++ 2015-2022 Redistributable (x64) is installed
  // The minimum version we check for is 14.30 (VS 2022)
  if RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Version', Version) then
  begin
    Log('VC++ Redistributable already installed, version: ' + Version);
    Result := False;
  end
  else
  begin
    Log('VC++ Redistributable not found, will install.');
    Result := True;
  end;
end;

function WindowsAppRuntimeNeedsInstall: Boolean;
var
  ResultCode: Integer;
  Parameters: String;
begin
  Parameters := '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "' +
    '$min=[version]''2.4.0.0'';' +
    '$p=Get-AppxPackage *appruntime*;' +
    '$f=$p|Where-Object{$_.Name -eq ''Microsoft.WindowsAppRuntime.2'' -and $_.Architecture -eq ''X64'' -and $_.Version -ge $min};' +
    '$m=$p|Where-Object{$_.Name -eq ''MicrosoftCorporationII.WinAppRuntime.Main.2'' -and $_.Architecture -eq ''X64'' -and $_.Version -ge $min};' +
    '$s=$p|Where-Object{$_.Name -eq ''MicrosoftCorporationII.WinAppRuntime.Singleton'' -and $_.Architecture -eq ''X64'' -and $_.Version -ge $min};' +
    '$d=$p|Where-Object{$_.Name -like ''Microsoft.WinAppRuntime.DDLM.2.4.*-x6*'' -and $_.Version -ge $min};' +
    'if($f -and $m -and $s -and $d){exit 0}else{exit 1}"';

  if Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Result := ResultCode <> 0;
  end
  else
  begin
    Log('Could not query Windows App Runtime packages; runtime installation is required.');
    Result := True;
  end;

  if Result then
    Log('Complete Windows App Runtime 2.4 x64 installation not found; download is required.')
  else
    Log('Windows App Runtime 2.4 x64 is already installed; skipping download.');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  RuntimeInstaller: String;
  ResultCode: Integer;
begin
  Result := '';
  if not WindowsAppRuntimeNeedsInstall then
    Exit;

  try
    RuntimeInstaller := DownloadTemporaryFile(
      '{#WindowsAppRuntimeURL}',
      '{#WindowsAppRuntimeFile}',
      '{#WindowsAppRuntimeSHA256}',
      nil);

    if (not Exec(RuntimeInstaller, '--quiet', '', SW_HIDE,
      ewWaitUntilTerminated, ResultCode)) or (ResultCode <> 0) then
    begin
      Result := 'Microsoft Windows App Runtime 2.4 could not be installed.';
    end;
  except
    Result := 'Microsoft Windows App Runtime 2.4 could not be downloaded: ' +
      GetExceptionMessage;
  end;
end;

