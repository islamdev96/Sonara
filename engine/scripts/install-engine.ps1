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
$ClsidSfx = '{538B6BB6-27D6-4D50-A09D-6E1883A66888}'

# Property keys (PKEY) used by the Windows audio engine for software effects.
$PKEY_SFX = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},5'   # SFX (stream effects) CLSID list
$PKEY_MFX = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},6'   # MFX (mode effects) CLSID list
$PKEY_COMPOSITE = '{D3993A3F-99C2-4402-B5EC-A92A0367664B},5' # composite FX

function Set-MultiStringProperty {
  param(
    [string]$Path,
    [string]$Name,
    [string]$ValueToAdd
  )
  $regPath = $Path
  if ($regPath -like "HKLM:\*") {
    $regPath = $regPath.Substring(6)
  }
  
  $list = @()
  $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($regPath, $false)
  if ($null -ne $key) {
    $val = $key.GetValue($Name)
    if ($null -ne $val) {
      $flatString = ""
      if ($val -is [array]) {
        $flatString = $val -join ","
      } else {
        $flatString = $val
      }
      if ($flatString -match '\{[a-fA-F0-9-]{36}\}') {
        $list = @([regex]::Matches($flatString, '\{[a-fA-F0-9-]{36}\}') | ForEach-Object { $_.Value })
      } else {
        $list = @($flatString.Split(@("`r`n", "`n", ",", ";"), [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
      }
    }
    $key.Close()
  }
  
  $list = @($list | Where-Object { $_ -ne $ValueToAdd })
  $list += $ValueToAdd
  
  $rights = [System.Security.AccessControl.RegistryRights]::SetValue -bor [System.Security.AccessControl.RegistryRights]::QueryValues -bor [System.Security.AccessControl.RegistryRights]::ReadKey
  $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($regPath, [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree, $rights)
  if ($null -eq $key) {
    throw "Failed to open registry key $regPath with SetValue rights."
  }
  $key.SetValue($Name, [string[]]$list, [Microsoft.Win32.RegistryValueKind]::MultiString)
  $key.Close()
}

Write-Host '== Sonara :: engine install ==' -ForegroundColor Cyan

if (!(Test-Path $DllPath)) { throw "BoosterAPO.dll not found at $DllPath" }

# 1) Copy DLL to System32 (audiodg.exe loads APOs from a protected location).
$dest = Join-Path $env:WINDIR 'System32\BoosterAPO.dll'
Copy-Item -Path $DllPath -Destination $dest -Force
Write-Host "Copied engine to $dest"

# 2) Register COM + APO.
& regsvr32.exe /s $dest
Write-Host 'Registered COM/APO server.'

# 3) Resolve target endpoints.
$base = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
$devices = @()

if ([string]::IsNullOrWhiteSpace($DeviceId)) {
  # Query the registry to find all active render endpoints (DeviceState = 1)
  $activeDevices = Get-ChildItem $base | Where-Object {
    (Get-ItemProperty -Path $_.PSPath -Name DeviceState -ErrorAction SilentlyContinue).DeviceState -eq 1
  }
  
  # Fallback to all devices if no active devices are found
  if (-not $activeDevices) {
    $devices = Get-ChildItem $base
  } else {
    $devices = $activeDevices
  }
} else {
  $devices = Get-ChildItem $base | Where-Object { $_.PSChildName -eq $DeviceId }
}

# 4) Attach our APO CLSID to the SFX and MFX slots for all target devices.
if ($devices.Count -eq 0) {
  Write-Warning "No target audio devices found to attach the engine."
} else {
  foreach ($dev in $devices) {
    $devId = $dev.PSChildName
    $fx = Join-Path $base "$devId\FxProperties"
    if (!(Test-Path $fx)) { New-Item -Path $fx -Force | Out-Null }
    
    Set-MultiStringProperty -Path $fx -Name $PKEY_SFX -ValueToAdd $ClsidSfx
    Set-MultiStringProperty -Path $fx -Name $PKEY_MFX -ValueToAdd $ClsidSfx
    Write-Host "Attached engine to endpoint $devId (as REG_MULTI_SZ)"
  }
}

# 5) Restart the audio service so audiodg reloads the effect chain.
Restart-Service -Name Audiosrv -Force
Write-Host 'Restarted Windows Audio. Engine active - no reboot needed.' -ForegroundColor Green
