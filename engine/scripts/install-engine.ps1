#requires -RunAsAdministrator
<#
.SYNOPSIS
  Installs and registers the Sonara self-contained audio engine
  (BoosterAPO.dll) and attaches it to the chosen render endpoint's effect chain.
  NO third-party software (no Equalizer APO) is required.

.DESCRIPTION
  1. Copies the signed BoosterAPO.dll into System32.
  2. Registers the COM server + APO (regsvr32).
  3. Writes the APO CLSID into the selected endpoint's FxProperties so Windows
     loads our effect into the audio graph for that device.
  4. Restarts the Windows Audio service so the change takes effect (no reboot).
#>
param(
  [string]$DllPath = "$PSScriptRoot\BoosterAPO.dll",
  [string]$DeviceId = ""   # empty = default render device
)

$ErrorActionPreference = 'Stop'
$ClsidSfx = '{A1B2C3D4-E5F6-47A8-9B0C-1D2E3F4A5B6C}'

# Property keys (PKEY) used by the Windows audio engine for software effects.
$PKEY_SFX = '{D04E05A6-594B-4FB6-A80D-01AF5EEC5217},5'   # SFX (stream effects) CLSID list
$PKEY_MFX = '{D04E05A6-594B-4FB6-A80D-01AF5EEC5217},6'   # MFX (mode effects) CLSID list
$PKEY_COMPOSITE = '{D3993A3F-99C2-4402-B5EC-A92A0367664B},5' # composite FX

Write-Host '== Sonara :: engine install ==' -ForegroundColor Cyan

if (!(Test-Path $DllPath)) { throw "BoosterAPO.dll not found at $DllPath" }

# 1) Copy DLL to System32 (audiodg.exe loads APOs from a protected location).
$dest = Join-Path $env:WINDIR 'System32\BoosterAPO.dll'
Copy-Item -Path $DllPath -Destination $dest -Force
Write-Host "Copied engine to $dest"

# 2) Register COM + APO.
& regsvr32.exe /s $dest
Write-Host 'Registered COM/APO server.'

# 3) Resolve the target endpoint registry path.
$base = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
if ([string]::IsNullOrWhiteSpace($DeviceId)) {
  # Pick the device that is currently the default (State = 1, role default).
  $DeviceId = (Get-ChildItem $base | Sort-Object Name | Select-Object -First 1).PSChildName
}
$fx = Join-Path $base "$DeviceId\FxProperties"
if (!(Test-Path $fx)) { New-Item -Path $fx -Force | Out-Null }

# 4) Attach our APO CLSID to the SFX and MFX slots.
New-ItemProperty -Path $fx -Name $PKEY_SFX -PropertyType String -Value $ClsidSfx -Force | Out-Null
New-ItemProperty -Path $fx -Name $PKEY_MFX -PropertyType String -Value $ClsidSfx -Force | Out-Null
Write-Host "Attached engine to endpoint $DeviceId"

# 5) Restart the audio service so audiodg reloads the effect chain.
Restart-Service -Name Audiosrv -Force
Write-Host 'Restarted Windows Audio. Engine active - no reboot needed.' -ForegroundColor Green
