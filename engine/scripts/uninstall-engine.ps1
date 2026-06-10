#requires -RunAsAdministrator
<#
.SYNOPSIS  Cleanly removes the Sonara audio engine from every
           render endpoint and unregisters it. Restores stock Windows audio.
#>
$ErrorActionPreference = 'SilentlyContinue'
$ClsidSfx = '{A1B2C3D4-E5F6-47A8-9B0C-1D2E3F4A5B6C}'
$PKEY_SFX = '{D04E05A6-594B-4FB6-A80D-01AF5EEC5217},5'
$PKEY_MFX = '{D04E05A6-594B-4FB6-A80D-01AF5EEC5217},6'

Write-Host '== Sonara :: engine uninstall ==' -ForegroundColor Cyan

$base = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
Get-ChildItem $base | ForEach-Object {
  $fx = Join-Path $base "$($_.PSChildName)\FxProperties"
  foreach ($k in @($PKEY_SFX, $PKEY_MFX)) {
    $v = (Get-ItemProperty -Path $fx -Name $k -ErrorAction SilentlyContinue).$k
    if ($v -eq $ClsidSfx) { Remove-ItemProperty -Path $fx -Name $k -Force }
  }
}

$dest = Join-Path $env:WINDIR 'System32\BoosterAPO.dll'
if (Test-Path $dest) {
  & regsvr32.exe /u /s $dest
  Restart-Service -Name Audiosrv -Force
  Start-Sleep -Seconds 2
  Remove-Item $dest -Force
}
Write-Host 'Engine removed. Stock Windows audio restored.' -ForegroundColor Green
