#requires -RunAsAdministrator
# loopback-test.ps1 - Records audio loopback using ffmpeg and measures the RMS volume difference.

param(
    [string]$Label = "",
    [int]$Duration = 5,
    [switch]$Compare
)

$ErrorActionPreference = 'Stop'
$TempDir = Join-Path $env:TEMP "SonaraTest"
if (!(Test-Path $TempDir)) { New-Item -Path $TempDir -ItemType Directory | Out-Null }

$RefFile = Join-Path $TempDir "ref_recording.wav"
$TestFile = Join-Path $TempDir "test_recording.wav"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "         Sonara Loopback Audio Test               " -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Verify ffmpeg installation
$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
if (!$ffmpeg) {
    Write-Host "Error: ffmpeg.exe was not found in your PATH." -ForegroundColor Red
    Write-Host "Please install ffmpeg to use this loopback test script." -ForegroundColor Yellow
    Write-Host "You can install it via winget: 'winget install Gyan.FFmpeg'" -ForegroundColor Yellow
    exit 1
}

# 2. Determine target file based on arguments
$OutputFile = $TestFile
$IsReference = $false

if ($Label -eq "Reference" -or $Label -eq "Bypassed" -or (!$Compare -and !(Test-Path $RefFile))) {
    $OutputFile = $RefFile
    $IsReference = $true
    Write-Host "Recording REFERENCE / UNBOOSTED audio sample..." -ForegroundColor Yellow
} else {
    Write-Host "Recording BOOSTED / TEST audio sample..." -ForegroundColor Yellow
}

Write-Host "Please start playing some continuous audio (e.g., music, YouTube video) now." -ForegroundColor Gray
Write-Host "Press any key to start recording $Duration seconds of audio..." -ForegroundColor White
$null = [System.Console]::ReadKey($true)

Write-Host "Recording..." -ForegroundColor Green
# Start recording using WASAPI loopback (capturing the default render device output)
# ffmpeg wasapi loopback command: -f wasapi -i default -loopback 1 (captures default playback device)
# We use -y to overwrite, and redirect stderr to capture log.
$processArgs = @("-y", "-f", "wasapi", "-i", "default", "-loopback", "1", "-t", "$Duration", "$OutputFile")
$process = Start-Process -FilePath "ffmpeg" -ArgumentList $processArgs -NoNewWindow -PassThru -Wait

if ($process.ExitCode -ne 0) {
    Write-Host "Warning: Recording with default loopback failed. Retrying with alternative syntax..." -ForegroundColor Yellow
    # Try alternative syntax: -f wasapi -i "Loopback (default)" or -f wasapi -i "Loopback (Default Render Device)"
    # ffmpeg's WASAPI device list helper can print the device name. We'll try a fallback:
    $processArgs = @("-y", "-f", "wasapi", "-i", "Loopback (Default Render Device)", "-t", "$Duration", "$OutputFile")
    $process = Start-Process -FilePath "ffmpeg" -ArgumentList $processArgs -NoNewWindow -PassThru -Wait
    
    if ($process.ExitCode -ne 0) {
        Write-Host "Error: ffmpeg failed to capture audio. Make sure default render device is active." -ForegroundColor Red
        exit 1
    }
}

Write-Host "Recording complete! Analyzing audio volume..." -ForegroundColor Green

# 3. Analyze volume using ffmpeg astats filter
function Get-AudioRMS([string]$filePath) {
    $analysisArgs = @("-i", $filePath, "-filter_complex", "astats=metadata=1:reset=1", "-f", "null", "-")
    # astats outputs to stderr, so we redirect it
    $tempLog = Join-Path $TempDir "astats_log.txt"
    Start-Process -FilePath "ffmpeg" -ArgumentList $analysisArgs -NoNewWindow -Wait -RedirectStandardError $tempLog
    
    $logContent = Get-Content $tempLog
    Remove-Item $tempLog -Force -ErrorAction SilentlyContinue
    
    # Extract RMS level dB from log (e.g. "RMS level dB: -18.42")
    # Note: astats prints stats per channel and overall. We look for overall RMS level.
    # Usually: "RMS level dB: -XX.XX" or "RMS level: -XX.XX" or similar.
    $rmsLine = $logContent | Where-Object { $_ -match "RMS level dB" -or $_ -match "RMS DB" } | Select-Object -First 1
    $rmsVal = 0.0
    if ($rmsLine -and $rmsLine -match "(-?\d+\.\d+)") {
        $rmsVal = [double]$Matches[1]
    }
    
    $peakLine = $logContent | Where-Object { $_ -match "Peak level dB" -or $_ -match "Peak DB" } | Select-Object -First 1
    $peakVal = 0.0
    if ($peakLine -and $peakLine -match "(-?\d+\.\d+)") {
        $peakVal = [double]$Matches[1]
    }

    return [PSCustomObject]@{
        RMS = $rmsVal
        Peak = $peakVal
    }
}

$stats = Get-AudioRMS $OutputFile
Write-Host "Recorded Audio Stats:" -ForegroundColor Cyan
Write-Host "  RMS Level: $($stats.RMS) dB" -ForegroundColor Gray
Write-Host "  Peak Level: $($stats.Peak) dB" -ForegroundColor Gray
Write-Host ""

if ($IsReference) {
    Write-Host "Reference sample saved successfully at $RefFile." -ForegroundColor Green
    Write-Host "Now, turn on Sonara's Audio Booster in the UI (e.g. set boost to 200% or 300%)" -ForegroundColor Yellow
    Write-Host "and run this script again without any arguments to record the test sample and see the difference." -ForegroundColor Yellow
} else {
    if (Test-Path $RefFile) {
        Write-Host "Comparing Boosted vs Reference (Bypassed)..." -ForegroundColor Yellow
        $refStats = Get-AudioRMS $RefFile
        
        $diffRMS = $stats.RMS - $refStats.RMS
        $diffPeak = $stats.Peak - $refStats.Peak
        
        Write-Host "Comparison Results:" -ForegroundColor Cyan
        Write-Host "  Reference RMS: $($refStats.RMS) dB" -ForegroundColor Gray
        Write-Host "  Boosted RMS:   $($stats.RMS) dB" -ForegroundColor Gray
        Write-Host "  ------------------------------------" -ForegroundColor Gray
        
        if ($diffRMS -gt 0.5) {
            Write-Host "  RMS Volume Difference: +$($diffRMS.ToString("0.00")) dB" -ForegroundColor Green
            Write-Host "  Peak Volume Difference: +$($diffPeak.ToString("0.00")) dB" -ForegroundColor Green
            Write-Host ""
            Write-Host "SUCCESS: Audio is boosted by +$($diffRMS.ToString("0.00")) dB!" -ForegroundColor Green
        } elseif ($diffRMS -lt -0.5) {
            Write-Host "  RMS Volume Difference: $($diffRMS.ToString("0.00")) dB" -ForegroundColor Red
            Write-Host "  Peak Volume Difference: $($diffPeak.ToString("0.00")) dB" -ForegroundColor Red
            Write-Host ""
            Write-Host "WARNING: Audio volume decreased or remains the same. Check if Sonara is active." -ForegroundColor Yellow
        } else {
            Write-Host "  RMS Volume Difference: $($diffRMS.ToString("0.00")) dB" -ForegroundColor Yellow
            Write-Host "  Peak Volume Difference: $($diffPeak.ToString("0.00")) dB" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "WARNING: No significant volume difference detected. Ensure the same audio was played and Sonara is active." -ForegroundColor Yellow
        }
    } else {
        Write-Host "Test sample saved successfully at $TestFile." -ForegroundColor Green
        Write-Host "No reference sample found. Run with '-Label Reference' first to save a reference sample." -ForegroundColor Yellow
    }
}
Write-Host "==================================================" -ForegroundColor Cyan
