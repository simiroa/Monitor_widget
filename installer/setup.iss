[Setup]
AppName=Monitor Widget
AppVersion=2024.1.15
AppPublisher=HG
DefaultDirName={autopf}\MonitorWidget
DefaultGroupName=Monitor Widget
OutputBaseFilename=MonitorWidget_Setup
Compression=lzma2
SolidCompression=yes
OutputDir=..\Release\installer
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\Release\portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Monitor Widget"; Filename: "{app}\monitor_widget.exe"
Name: "{autodesktop}\Monitor Widget"; Filename: "{app}\monitor_widget.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\monitor_widget.exe"; Description: "{cm:LaunchProgram, Monitor Widget}"; Flags: nowait postinstall skipifsilent runascurrentuser
