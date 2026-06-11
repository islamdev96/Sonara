# diagnose-engine.ps1 — Sonara: prove whether the APO is actually loaded & processing
$ErrorActionPreference = 'SilentlyContinue'

$LogFile = "c:\Users\Islam Glab\Desktop\New folder\Sonara\WinAudioBoosterPro\diagnose_result.txt"
Start-Transcript -Path $LogFile -Force

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[WARN] Not running as Administrator. Registry endpoint details might be incomplete." -ForegroundColor Yellow
}

$Clsid   = '{538B6BB6-27D6-4D50-A09D-6E1883A66888}'
$PkeySfx = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},5'
$PkeyMfx = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},6'
$Dll     = Join-Path $env:WINDIR 'System32\BoosterAPO.dll'
$DataDir = Join-Path $env:ProgramData 'WinAudioBoosterPro'

function Pass($t){ Write-Host "[PASS] $t" -ForegroundColor Green }
function Fail($t){ Write-Host "[FAIL] $t" -ForegroundColor Red }
function Warn($t){ Write-Host "[WARN] $t" -ForegroundColor Yellow }

Write-Host "=== Sonara engine diagnostics ===" -ForegroundColor Cyan

# 1) DLL present in System32
if (Test-Path $Dll) { Pass "DLL present: $Dll" } else { Fail "DLL missing in System32. Run install first." }

# 2) COM registration
$inproc = "Registry::HKEY_CLASSES_ROOT\CLSID\$Clsid\InprocServer32"
if (Test-Path $inproc) { Pass "COM InprocServer32 -> $((Get-ItemProperty $inproc).'(default)')" }
else { Fail "COM server NOT registered (regsvr32 failed?)." }

# 3) APO registration
$apo = "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$Clsid"
if (Test-Path $apo) { Pass "APO registered under AudioEngine\AudioProcessingObjects." } else { Fail "APO NOT registered." }

# 4) Endpoint attachment + DATA-TYPE check (the suspected bug) + format gate
$base = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
Get-ChildItem $base | ForEach-Object {
    if ((Get-ItemProperty $_.PSPath -Name DeviceState).DeviceState -ne 1) { return } # active only
    $devId = $_.PSChildName
    $name  = (Get-ItemProperty (Join-Path $_.PSPath 'Properties') -Name '{a45c254e-df1c-4efd-8020-67d146a850e0},2').'{a45c254e-df1c-4efd-8020-67d146a850e0},2'
    Write-Host "`n-- Active device: $name [$devId]"
    $fx = Join-Path $_.PSPath 'FxProperties'
    if (Test-Path $fx) {
        $k = Get-Item $fx
        foreach ($pk in @($PkeySfx,$PkeyMfx)) {
            if ($null -ne $k.GetValue($pk)) {
                $kind = $k.GetValueKind($pk); $val = $k.GetValue($pk)
                if ("$kind" -eq 'MultiString') { Pass "  $pk OK (REG_MULTI_SZ) -> $($val -join ', ')" }
                else { Fail "  $pk WRONG TYPE = $kind (must be REG_MULTI_SZ). This alone prevents loading. Value: $val" }
            } else { Warn "  $pk not set on this device." }
        }
        $fmt = (Get-ItemProperty (Join-Path $_.PSPath 'Properties') -Name '{f19f064d-082c-4e27-bc73-6882a1bb8e4c},0').'{f19f064d-082c-4e27-bc73-6882a1bb8e4c},0'
        if ($fmt) {
            # Serialized PROPVARIANT type indicator check (VT_BLOB = 65).
            $offset = 0
            if ($fmt.Length -ge 8 -and [BitConverter]::ToUInt16($fmt, 0) -eq 65) {
                $offset = 8
            }
            if ($fmt.Length -ge ($offset + 18)) {
                $tag = [BitConverter]::ToUInt16($fmt, $offset)
                if ($tag -eq 0xFFFE -and $fmt.Length -ge ($offset + 40)) {
                    $sub = [Guid]::new([byte[]]($fmt[($offset + 24)..($offset + 39)]))
                    if ($sub -eq [Guid]'00000003-0000-0010-8000-00aa00389b71') { Pass "  Device format = IEEE Float (accepted)." }
                    else { Fail "  Device format NOT IEEE Float ($sub). APO rejects it in IsInputFormatSupported -> never loads." }
                } elseif ($tag -eq 3) { Pass "  Device format = IEEE Float." }
                else { Fail "  Device format tag=$tag NOT IEEE Float -> APO rejects format." }
            } else {
                Fail "  Device format property too short to parse."
            }
        }
    } else { Warn "  No FxProperties (driver exposes no effect slots)." }
}

# 5) DEFINITIVE #1 — is the DLL inside audiodg.exe?
Write-Host "`n-- audiodg.exe module check --"
$tl = tasklist /m BoosterAPO.dll 2>$null | Out-String
if ($tl -match 'audiodg') { Pass "BoosterAPO.dll IS loaded in audiodg.exe." }
else { Fail "BoosterAPO.dll NOT found in audiodg.exe (note: audiodg is protected, may be a false negative -> rely on heartbeat below)." }

# 6) DEFINITIVE #2 (ground truth) — status.bin heartbeat while audio plays
Write-Host "`n-- status.bin heartbeat (only updates if APO code runs inside audiodg) --"
$status = Join-Path $DataDir 'status.bin'
if (Test-Path $status) {
    $t1 = (Get-Item $status).LastWriteTime; Start-Sleep -Seconds 2; $t2 = (Get-Item $status).LastWriteTime
    if ($t2 -gt $t1) { Pass "status.bin updating ($t1 -> $t2) => APO IS processing audio NOW." }
    else { Fail "status.bin NOT updating => APO is not running (make sure audio is playing, then re-run)." }
} else { Warn "status.bin not found in $DataDir => engine never wrote status." }

Write-Host "`n=== The truth is the heartbeat line (#6). If it is FAIL while audio plays, the APO is not in the path. ===" -ForegroundColor Cyan

Stop-Transcript
