; installer.nsh - NSIS hooks for the Sonara installer.
; On install, register + attach the self-contained engine. On uninstall,
; cleanly remove it and restore stock Windows audio.

!macro customInstall
  DetailPrint "Installing Sonara audio engine..."
  ; Run the engine installer (elevated; perMachine install already elevated).
  nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\resources\engine\install-engine.ps1" -DllPath "$INSTDIR\resources\engine\SonaraAPO.dll"'
  Pop $0
  DetailPrint "Engine install exit code: $0"
!macroend

!macro customUnInstall
  DetailPrint "Removing Sonara audio engine..."
  nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\resources\engine\uninstall-engine.ps1"'
  Pop $0
!macroend
