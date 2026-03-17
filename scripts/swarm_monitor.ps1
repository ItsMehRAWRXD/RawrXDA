# Swarm progress monitor — run while spawn_subagent_fixers.ps1 is running
# Usage: .\scripts\swarm_monitor.ps1 [ -Watch ] [ -Interval 10 ]
param(
    [switch]$Watch,
    [int]$IntervalSeconds = 10
)

$root = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
$failedJson = Join-Path $root "failed_files.json"
$completionLog = Join-Path $root "completion_log.txt"
$batchFailed = Join-Path $root "batch_*_failed.json"

function Show-Status {
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "`n[$ts] Swarm status" -ForegroundColor Cyan

    if (Test-Path $failedJson) {
        $data = Get-Content $failedJson -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
        $n = if ($data -is [array]) { $data.Count } else { 1 }
        Write-Host "  failed_files.json: $n file(s) to fix" -ForegroundColor $(if ($n -eq 0) { "Green" } else { "Yellow" })
    } else {
        Write-Host "  failed_files.json: (none — smoke test not run or all passed)" -ForegroundColor Gray
    }

    $batches = @(Get-ChildItem $batchFailed -ErrorAction SilentlyContinue)
    if ($batches.Count -gt 0) {
        $still = 0
        foreach ($f in $batches) { $still += (Get-Content $f.FullName -Raw | ConvertFrom-Json).Count }
        Write-Host "  batch_*_failed.json: $($batches.Count) batch(es), ~$still still failing" -ForegroundColor Yellow
    }

    if (Test-Path $completionLog) {
        $lines = Get-Content $completionLog -Tail 5 -ErrorAction SilentlyContinue
        Write-Host "  completion_log.txt (last 5 lines):" -ForegroundColor Gray
        $lines | ForEach-Object { Write-Host "    $_" -ForegroundColor Gray }
    }

    # Background jobs in current session (if spawn was run here)
    $jobs = Get-Job -ErrorAction SilentlyContinue
    if ($jobs) {
        $running = ($jobs | Where-Object { $_.State -eq "Running" }).Count
        Write-Host "  PowerShell jobs: $running running, $($jobs.Count - $running) completed" -ForegroundColor Magenta
    }
}

Show-Status
if ($Watch) {
    Write-Host "`nWatching every ${IntervalSeconds}s (Ctrl+C to stop)..." -ForegroundColor Gray
    while ($true) {
        Start-Sleep -Seconds $IntervalSeconds
        Show-Status
    }
}
