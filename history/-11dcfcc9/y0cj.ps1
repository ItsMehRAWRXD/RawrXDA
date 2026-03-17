param(
    [string]$BuildDir = 'D:\RawrXD-production-lazy-init\build',
    [string]$ReleaseDir = 'D:\RawrXD-production-lazy-init\build\Release',
    [string[]]$Expected = @(
        'RawrXD-AgenticIDE.exe',
        'RawrXD-Agent.exe',
        'gguf_hotpatch_tester.exe',
        'test_chat_streaming.exe',
        'benchmark_completions.exe'
    ),
    [int]$IntervalSeconds = 25
)

Write-Host "[Watch-Build-All] Watching for all executables..." -ForegroundColor Cyan
Write-Host "[Watch-Build-All] Expected:`n  - " ($Expected -join "`n  - ") -ForegroundColor Cyan

function Get-Missing {
    param([string]$dir,[string[]]$names)
    $missing = @()
    foreach ($n in $names) {
        if (-not (Test-Path (Join-Path $dir $n))) { $missing += $n }
    }
    return $missing
}

while ($true) {
    $missing = Get-Missing -dir $ReleaseDir -names $Expected
    if ($missing.Count -eq 0) {
        Write-Host "[Watch-Build-All] All expected executables present. Exiting watcher." -ForegroundColor Green
        break
    }

    $cl = Get-Process -Name 'cl' -ErrorAction SilentlyContinue
    if (-not $cl) {
        Write-Host "[Watch-Build-All] Missing: $($missing -join ', '). Compilers idle — building all." -ForegroundColor Yellow
        try {
            pwsh -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Build-All.ps1')
        } catch {
            Write-Host "[Watch-Build-All] Build-All failed: $($_.Exception.Message)" -ForegroundColor Red
        }
    } else {
        Write-Host "[Watch-Build-All] Compilers active ($(($cl | Measure-Object).Count)); waiting..." -ForegroundColor DarkGray
    }

    Start-Sleep -Seconds $IntervalSeconds
}
