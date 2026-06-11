#requires -RunAsAdministrator
<#
.SYNOPSIS  Cleanly removes the Sonara audio engine from every
           render endpoint and unregisters it. Restores stock Windows audio.
#>
$ErrorActionPreference = 'SilentlyContinue'
$ClsidSfx = '{538B6BB6-27D6-4D50-A09D-6E1883A66888}'
$PKEY_SFX = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},5'
$PKEY_MFX = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},6'

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
