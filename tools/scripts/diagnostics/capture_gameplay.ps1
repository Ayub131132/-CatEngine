<#
.SYNOPSIS
    Real-Time Gameplay Diagnostic Collector for CatEngine
.DESCRIPTION
    Captures timestamped screen frames, memory stats, CPU metrics, and logcat telemetry
    from the connected Android testing device for analysis.
#>
param(
    [string]$DeviceSerial = "10ME8DF1CT0001Z",
    [int]$IntervalSec = 3,
    [int]$MaxFrames = 30,
    [switch]$SingleShot
)

$OutputDir = Join-Path $PSScriptRoot "..\..\..\diagnostics\gameplay_session"
$FramesDir = Join-Path $OutputDir "frames"
$MetricsFile = Join-Path $OutputDir "metrics.jsonl"
$LogcatFile = Join-Path $OutputDir "logcat_telemetry.log"

if (-not (Test-Path $FramesDir)) {
    New-Item -ItemType Directory -Force -Path $FramesDir | Out-Null
}

Write-Host "=== CatEngine Diagnostics Session Started ===" -ForegroundColor Cyan
Write-Host "Device: $DeviceSerial"
Write-Host "Output: $OutputDir"

$count = 0
$runLoop = $true

while ($runLoop) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
    $frameFile = "frame_$timestamp.png"
    $devicePath = "/sdcard/catengine_diag_$timestamp.png"
    $localPath = Join-Path $FramesDir $frameFile

    # 1. Capture screen
    & adb -s $DeviceSerial shell "screencap -p $devicePath"
    if ($LASTEXITCODE -eq 0) {
        & adb -s $DeviceSerial pull $devicePath $localPath 2>$null
        & adb -s $DeviceSerial shell "rm $devicePath"
    }

    # 2. Get active window / focus
    $focus = & adb -s $DeviceSerial shell "dumpsys window | grep -E '(mCurrentFocus|mFocusedApp)'"
    
    # 3. Get memory info for CatEngine process
    $mem = & adb -s $DeviceSerial shell "dumpsys meminfo com.engine.cat | grep -E '(TOTAL PSS|Native Heap|Dalvik Heap)'"
    
    # 4. Get recent CatEngine logcat
    $logs = & adb -s $DeviceSerial logcat -d -s CatEngine:I CatEngine-V2:I BlackBox:I AndroidRuntime:E
    if ($logs) {
        Add-Content -Path $LogcatFile -Value "--- Snapshot at $timestamp ---"
        Add-Content -Path $LogcatFile -Value $logs
    }

    # 5. Record JSONL metric entry
    $metric = [PSCustomObject]@{
        timestamp = $timestamp
        frame = $frameFile
        focus = ($focus -join " | ")
        mem = ($mem -join " | ")
    } | ConvertTo-Json -Compress

    Add-Content -Path $MetricsFile -Value $metric
    $count++
    Write-Host "[$count] Captured $frameFile | Focus: $(($focus | Select-Object -First 1).Trim())" -ForegroundColor Green

    if ($SingleShot -or ($count -ge $MaxFrames)) {
        $runLoop = $false
    } else {
        Start-Sleep -Seconds $IntervalSec
    }
}

Write-Host "=== CatEngine Diagnostics Session Finished ($count frames recorded) ===" -ForegroundColor Cyan

