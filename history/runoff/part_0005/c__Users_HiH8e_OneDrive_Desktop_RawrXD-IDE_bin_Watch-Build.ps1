param(
    [string]$BuildDir = 'D:\RawrXD-production-lazy-init\build',
    [string]$TargetExe = 'D:\RawrXD-production-lazy-init\build\Release\RawrXD-AgenticIDE.exe',
    [int]$IntervalSeconds = 20
)

Write-Host "[Watch-Build] Watching for build completion..." -ForegroundColor Cyan
Write-Host "[Watch-Build] Target: $TargetExe" -ForegroundColor Cyan

while ($true) {
    if (Test-Path $TargetExe) {
        Write-Host "[Watch-Build] EXE detected. Exiting watcher." -ForegroundColor Green
        break
    }

    $cl = Get-Process -Name 'cl' -ErrorAction SilentlyContinue
    if (-not $cl) {
        Write-Host "[Watch-Build] No compilers running and EXE missing — triggering build." -ForegroundColor Yellow
        try {
            pwsh -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Build-IDE.ps1')
        } catch {
            Write-Host "[Watch-Build] Build invocation failed: $($_.Exception.Message)" -ForegroundColor Red
        }
    } else {
        Write-Host "[Watch-Build] Compilers active ($(($cl | Measure-Object).Count)). Waiting..." -ForegroundColor DarkGray
    }

    Start-Sleep -Seconds $IntervalSeconds
}
