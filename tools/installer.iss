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

[Setup]
AppId={{8A9C6942-5CA3-4A02-B701-E7B4E862D635}}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}

VersionInfoDescription={#AppName} installer
VersionInfoProductName={#AppName}
VersionInfoVersion={#AppVersion}

AppCopyright=(c) {#CurrentYear} {#AppPublisher}

UninstallDisplayName={#AppName} {#AppVersion}
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
Name: "{group}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"; IconFilename: "{app}\bin\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\bin\{#AppExeName}"

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

