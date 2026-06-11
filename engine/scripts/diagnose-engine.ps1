#requires -RunAsAdministrator
# diagnose-engine.ps1 - Diagnostic script to verify the health and status of Sonara Audio Engine.

$ErrorActionPreference = 'Continue'
$ClsidSfx = '{538B6BB6-27D6-4D50-A09D-6E1883A66888}'
$PKEY_SFX = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},5'
$PKEY_MFX = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},6'

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "        Sonara Engine Diagnostic Tool             " -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""

$AllOk = $true

# 1. Check if the DLL exists in System32
Write-Host "1. Checking BoosterAPO.dll in System32..." -NoNewline
$System32Dll = Join-Path $env:WINDIR "System32\BoosterAPO.dll"
if (Test-Path $System32Dll) {
    Write-Host " [OK]" -ForegroundColor Green
    $fileInfo = Get-Item $System32Dll
    Write-Host "   Size: $($fileInfo.Length) bytes" -ForegroundColor Gray
    Write-Host "   Last Modified: $($fileInfo.LastWriteTime)" -ForegroundColor Gray
    
    # Check signature/digital signing (notice)
    $sig = Get-AuthenticodeSignature $System32Dll -ErrorAction SilentlyContinue
    if ($sig -and $sig.Status -eq 'Valid') {
        Write-Host "   Digital Signature: VALID ($($sig.SignerCertificate.Subject))" -ForegroundColor Green
    } else {
        Write-Host "   Digital Signature: Not signed or invalid signature." -ForegroundColor Yellow
        Write-Host "   (Note: Unsigned APOs might be blocked by audiodg.exe depending on system settings. EV certificate recommended.)" -ForegroundColor Gray
    }
} else {
    Write-Host " [FAILED]" -ForegroundColor Red
    Write-Host "   BoosterAPO.dll was NOT found in System32." -ForegroundColor Red
    $AllOk = $false
}
Write-Host ""

# 2. Check COM registration in HKCR
Write-Host "2. Checking COM Server Registration..." -NoNewline
$comPath = "HKCR:\CLSID\$ClsidSfx"
if (Test-Path $comPath) {
    Write-Host " [OK]" -ForegroundColor Green
    $inproc = Join-Path $comPath "InprocServer32"
    if (Test-Path $inproc) {
        $dllRegistered = (Get-ItemProperty -Path $inproc -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
        $threading = (Get-ItemProperty -Path $inproc -Name "ThreadingModel" -ErrorAction SilentlyContinue).ThreadingModel
        Write-Host "   InprocServer32 Path: $dllRegistered" -ForegroundColor Gray
        Write-Host "   Threading Model: $threading" -ForegroundColor Gray
    } else {
        Write-Host "   Warning: InprocServer32 subkey missing!" -ForegroundColor Yellow
        $AllOk = $false
    }
} else {
    Write-Host " [FAILED]" -ForegroundColor Red
    Write-Host "   COM registration key $ClsidSfx not found under HKCR:\CLSID." -ForegroundColor Red
    $AllOk = $false
}
Write-Host ""

# 3. Check APO registration in MMDevices
Write-Host "3. Checking Audio Processing Object (APO) Registry Keys..." -NoNewline
$apoRegPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\AudioProcessingObjects\$ClsidSfx"
if (Test-Path $apoRegPath) {
    Write-Host " [OK]" -ForegroundColor Green
    $friendlyName = (Get-ItemProperty -Path $apoRegPath -Name "FriendlyName" -ErrorAction SilentlyContinue).FriendlyName
    $flags = (Get-ItemProperty -Path $apoRegPath -Name "Flags" -ErrorAction SilentlyContinue).Flags
    Write-Host "   APO Name: $friendlyName" -ForegroundColor Gray
    Write-Host "   APO Flags: $flags" -ForegroundColor Gray
} else {
    Write-Host " [FAILED]" -ForegroundColor Red
    Write-Host "   APO key not found under MMDevices\AudioProcessingObjects." -ForegroundColor Red
    $AllOk = $false
}
Write-Host ""

# 4. Check endpoint attachments and Enhancements state
Write-Host "4. Checking Audio Render Endpoints Attachment..."
$baseRenderPath = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
$endpoints = Get-ChildItem $baseRenderPath -ErrorAction SilentlyContinue
$attachedCount = 0

if ($endpoints) {
    foreach ($ep in $endpoints) {
        $epId = $ep.PSChildName
        $fxPropertiesPath = Join-Path $ep.PSPath "FxProperties"
        $propertiesPath = Join-Path $ep.PSPath "Properties"
        
        $devName = "Unknown Device"
        if (Test-Path $propertiesPath) {
            # Friendly name key is usually '{b3f833b1-30ca-4753-912f-d055b137c014},2'
            $friendlyNameProp = Get-ItemProperty -Path $propertiesPath -ErrorAction SilentlyContinue | 
                Get-Member -MemberType NoteProperty | 
                Where-Object { $_.Name -like "*{b3f833b1-30ca-4753-912f-d055b137c014},2*" } | 
                Select-Object -ExpandProperty Name
            if ($friendlyNameProp) {
                $devName = (Get-ItemProperty -Path $propertiesPath -Name $friendlyNameProp -ErrorAction SilentlyContinue).$friendlyNameProp
            }
        }
        
        $devState = (Get-ItemProperty -Path $ep.PSPath -Name DeviceState -ErrorAction SilentlyContinue).DeviceState
        
        if ($devState -eq 1) {
            Write-Host "   Active Device: $devName ($epId)" -ForegroundColor Gray
            if (Test-Path $fxPropertiesPath) {
                $sfx = (Get-ItemProperty -Path $fxPropertiesPath -Name $PKEY_SFX -ErrorAction SilentlyContinue).$PKEY_SFX
                $mfx = (Get-ItemProperty -Path $fxPropertiesPath -Name $PKEY_MFX -ErrorAction SilentlyContinue).$PKEY_MFX
                
                # Check for disable enhancements key
                # PKEY_AudioEndpoint_Disable_SysFx = '{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5'
                $disableSysFxProp = Get-ItemProperty -Path $fxPropertiesPath -ErrorAction SilentlyContinue | 
                    Get-Member -MemberType NoteProperty | 
                    Where-Object { $_.Name -like "*{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5*" } | 
                    Select-Object -ExpandProperty Name
                $disabled = $false
                if ($disableSysFxProp) {
                    $disabledVal = (Get-ItemProperty -Path $fxPropertiesPath -Name $disableSysFxProp -ErrorAction SilentlyContinue).$disableSysFxProp
                    if ($disabledVal -eq 1) {
                        $disabled = $true
                    }
                }
                
                if ($sfx -eq $ClsidSfx -or $mfx -eq $ClsidSfx) {
                    $attachedCount++
                    Write-Host "     -> Status: ATTACHED" -ForegroundColor Green
                    if ($disabled) {
                        Write-Host "     -> WARNING: 'Disable all enhancements' is CHECKED in Windows properties for this device!" -ForegroundColor Yellow
                        Write-Host "        The APO will not load or process audio for this device unless enhancements are enabled." -ForegroundColor Yellow
                    } else {
                        Write-Host "     -> Audio Enhancements: ENABLED" -ForegroundColor Green
                    }
                } else {
                    Write-Host "     -> Status: Not Attached" -ForegroundColor Gray
                }
            } else {
                Write-Host "     -> Status: No FxProperties subkey found." -ForegroundColor Gray
            }
        }
    }
} else {
    Write-Host "   No audio render devices found in registry." -ForegroundColor Red
    $AllOk = $false
}
Write-Host ""

# 5. Check if audiodg.exe has the DLL loaded
Write-Host "5. Checking if audiodg.exe has loaded BoosterAPO.dll..." -NoNewline
$audiodgProcesses = Get-Process -Name "audiodg" -ErrorAction SilentlyContinue
if ($audiodgProcesses) {
    $dllLoaded = $false
    foreach ($proc in $audiodgProcesses) {
        $modules = $proc.Modules | Where-Object { $_.ModuleName -eq "BoosterAPO.dll" }
        if ($modules) {
            $dllLoaded = $true
            break
        }
    }
    
    if ($dllLoaded) {
        Write-Host " [OK]" -ForegroundColor Green
        Write-Host "   audiodg.exe (PID: $($audiodgProcesses[0].Id)) has active module BoosterAPO.dll." -ForegroundColor Green
    } else {
        Write-Host " [WARNING/NOT LOADED]" -ForegroundColor Yellow
        Write-Host "   audiodg.exe is running, but BoosterAPO.dll is NOT loaded into it." -ForegroundColor Yellow
        Write-Host "   This could mean: " -ForegroundColor Gray
        Write-Host "     a) No audio is currently playing." -ForegroundColor Gray
        Write-Host "     b) The default render device Enhancements are disabled." -ForegroundColor Gray
        Write-Host "     c) The Windows Audio service needs to be restarted." -ForegroundColor Gray
        Write-Host "     d) Windows rejected the DLL signature (check Event Viewer -> Application)." -ForegroundColor Gray
    }
} else {
    Write-Host " [FAILED]" -ForegroundColor Red
    Write-Host "   audiodg.exe process is not running. (Windows Audio Service stopped or idle?)" -ForegroundColor Red
    $AllOk = $false
}
Write-Host ""

# 6. Check shared memory/binary files and heartbeat
Write-Host "6. Checking Shared Parameters and Status files..."
$programDataPath = Join-Path $env:ProgramData "WinAudioBoosterPro"
if (Test-Path $programDataPath) {
    Write-Host "   Directory: $programDataPath [OK]" -ForegroundColor Green
    
    $paramsFile = Join-Path $programDataPath "params.bin"
    if (Test-Path $paramsFile) {
        $pInfo = Get-Item $paramsFile
        Write-Host "   params.bin: FOUND (Size: $($pInfo.Length) bytes, Last Write: $($pInfo.LastWriteTime))" -ForegroundColor Green
    } else {
        Write-Host "   params.bin: NOT FOUND (Will be created when Electron UI starts)" -ForegroundColor Yellow
    }
    
    $statusFile = Join-Path $programDataPath "status.bin"
    if (Test-Path $statusFile) {
        $sInfo = Get-Item $statusFile
        $fileAge = (Get-Date) - $sInfo.LastWriteTime
        Write-Host "   status.bin: FOUND (Size: $($sInfo.Length) bytes, Last Write: $($sInfo.LastWriteTime))" -ForegroundColor Green
        
        if ($fileAge.TotalSeconds -lt 5) {
            Write-Host "   HEARTBEAT: ACTIVE (File modified $($fileAge.TotalSeconds.ToString("0.0"))s ago) [OK]" -ForegroundColor Green
        } else {
            Write-Host "   HEARTBEAT: STALE (File modified $($fileAge.TotalSeconds.ToString("0.0"))s ago)" -ForegroundColor Yellow
            Write-Host "   (APO is not actively processing audio right now, or audio is paused.)" -ForegroundColor Gray
        }
    } else {
        Write-Host "   status.bin: NOT FOUND (APO has not run or processed audio yet)" -ForegroundColor Yellow
    }
} else {
    Write-Host "   Directory $programDataPath does not exist yet. Run the app or scripts first." -ForegroundColor Yellow
}
Write-Host ""

Write-Host "==================================================" -ForegroundColor Cyan
if ($AllOk) {
    Write-Host "  DIAGNOSTIC SUMMARY: SYSTEM IS HEALTHY & CONFIGURED" -ForegroundColor Green
} else {
    Write-Host "  DIAGNOSTIC SUMMARY: PROBLEMS DETECTED" -ForegroundColor Red
    Write-Host "  Please review the red and yellow items above." -ForegroundColor Red
}
Write-Host "==================================================" -ForegroundColor Cyan
