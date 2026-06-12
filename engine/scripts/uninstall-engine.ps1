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

function Remove-ValueFromMultiString {
  param(
    [string]$Path,
    [string]$Name,
    [string]$ValueToRemove
  )
  $regPath = $Path
  if ($regPath -like "HKLM:\*") {
    $regPath = $regPath.Substring(6)
  }
  
  $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($regPath, $false)
  if ($null -eq $key) { return }
  $val = $key.GetValue($Name)
  $key.Close()
  
  if ($null -ne $val) {
    $list = @()
    if ($val -is [array]) {
      $list = $val
    } elseif ($val -is [string]) {
      $list = $val.Split(@("`r`n", "`n", ",", ";"), [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() }
    }
    
    $filtered = $list | Where-Object { $_ -ne $ValueToRemove }
    
    $rights = [System.Security.AccessControl.RegistryRights]::SetValue -bor [System.Security.AccessControl.RegistryRights]::QueryValues -bor [System.Security.AccessControl.RegistryRights]::ReadKey
    $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($regPath, [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree, $rights)
    if ($null -eq $key) {
      throw "Failed to open registry key $regPath with SetValue rights."
    }
    
    if ($filtered.Count -eq 0) {
      # Write empty array since we don't have Delete rights in the standard ACL
      $key.SetValue($Name, [string[]]@(), [Microsoft.Win32.RegistryValueKind]::MultiString)
    } else {
      $key.SetValue($Name, [string[]]$filtered, [Microsoft.Win32.RegistryValueKind]::MultiString)
    }
    $key.Close()
  }
}

$base = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
Get-ChildItem $base | ForEach-Object {
  $fx = Join-Path $base "$($_.PSChildName)\FxProperties"
  foreach ($k in @($PKEY_SFX, $PKEY_MFX)) {
    Remove-ValueFromMultiString -Path $fx -Name $k -ValueToRemove $ClsidSfx
  }
}

$dest = Join-Path $env:WINDIR 'System32\SonaraAPO.dll'
if (Test-Path $dest) {
  & regsvr32.exe /u /s $dest
  Restart-Service -Name Audiosrv -Force
  Start-Sleep -Seconds 2
  Remove-Item $dest -Force
}
Write-Host 'Engine removed. Stock Windows audio restored.' -ForegroundColor Green
