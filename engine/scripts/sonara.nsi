Unicode true
!include "MUI2.nsh"

Name "Sonara"
OutFile "..\..\build\Release\Sonara_Setup.exe"
InstallDir "$PROGRAMFILES64\Sonara"
InstallDirRegKey HKLM "Software\Sonara" "InstallDir"
RequestExecutionLevel admin

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\..\app\build\icon.ico"
!define MUI_UNICON "..\..\app\build\icon.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\SonaraGUI.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Copy files
    File "..\..\build\Release\SonaraGUI.exe"
    File "..\..\build\Release\SonaraHost.exe"
    File "..\..\build\Release\SonaraAPO.dll"
    File "install-engine.ps1"
    File "uninstall-engine.ps1"
    File /r "..\..\app\dist"

    ; Register the APO driver DLL using the powershell install script
    DetailPrint "Registering Sonara audio driver (restarting audio service)..."
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\install-engine.ps1" -DllPath "$INSTDIR\SonaraAPO.dll"'
    Pop $0
    DetailPrint "APO registration return code: $0"

    ; Registry keys for uninstallation & Startup
    WriteRegStr HKLM "Software\Sonara" "InstallDir" "$INSTDIR"
    
    ; Add to Startup (Run key) for all users
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "Sonara" '"$INSTDIR\SonaraGUI.exe"'

    ; Write Add/Remove Programs registry keys
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "DisplayName" "Sonara Audio Enhancer"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "DisplayIcon" "$INSTDIR\SonaraGUI.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "Publisher" "Sonara"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "DisplayVersion" "1.0.0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara" "NoRepair" 1

    ; Create Shortcuts
    CreateDirectory "$SMPROGRAMS\Sonara"
    CreateShortcut "$SMPROGRAMS\Sonara\Sonara.lnk" "$INSTDIR\SonaraGUI.exe"
    CreateShortcut "$SMPROGRAMS\Sonara\Uninstall Sonara.lnk" "$INSTDIR\uninstall.exe"
    CreateShortcut "$DESKTOP\Sonara.lnk" "$INSTDIR\SonaraGUI.exe"

    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    ; Run unregister script first to safely detach APO and restart audio service
    DetailPrint "Detaching Sonara audio driver (restarting audio service)..."
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\uninstall-engine.ps1"'
    Pop $0
    DetailPrint "APO unregistration return code: $0"

    ; Close running instances of GUI & Host
    nsExec::Exec 'taskkill.exe /f /im SonaraGUI.exe'
    nsExec::Exec 'taskkill.exe /f /im SonaraHost.exe'

    ; Clean up files
    Delete "$INSTDIR\SonaraGUI.exe"
    Delete "$INSTDIR\SonaraHost.exe"
    Delete "$INSTDIR\SonaraAPO.dll"
    Delete "$INSTDIR\install-engine.ps1"
    Delete "$INSTDIR\uninstall-engine.ps1"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$INSTDIR\dist"
    RMDir "$INSTDIR"

    ; Clean up Shortcuts
    Delete "$SMPROGRAMS\Sonara\Sonara.lnk"
    Delete "$SMPROGRAMS\Sonara\Uninstall Sonara.lnk"
    RMDir "$SMPROGRAMS\Sonara"
    Delete "$DESKTOP\Sonara.lnk"

    ; Clean up Registry
    DeleteRegKey HKLM "Software\Sonara"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sonara"
    DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "Sonara"
SectionEnd
