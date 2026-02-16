# Run-VSIXAgenticTest.ps1
# Stops IDE if running, builds, runs VSIX agentic test with Amazon Q / GitHub Copilot
# Usage: .\Run-VSIXAgenticTest.ps1
#        .\Run-VSIXAgenticTest.ps1 -AmazonQVsix "C:\path\to\amazonq.vsix" -GitHubCopilotVsix "C:\path\to\copilot.vsix"

param(
    [string]$AmazonQVsix = $env:AMAZONQ_VSIX,
    [string]$GitHubCopilotVsix = $env:GITHUB_COPILOT_VSIX,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# 1. Stop RawrXD IDE if running (unlocks exe for rebuild)
$ideProc = Get-Process -Name "RawrXD-Win32IDE" -ErrorAction SilentlyContinue
if ($ideProc) {
    Write-Host "Stopping RawrXD-Win32IDE (PID $($ideProc.Id))..." -ForegroundColor Yellow
    $ideProc | Stop-Process -Force
    Start-Sleep -Seconds 2
}

# 2. Build (unless -NoBuild)
if (-not $NoBuild) {
    Write-Host "Building RawrXD-Win32IDE..." -ForegroundColor Cyan
    $buildDir = Join-Path $root "build_ide"
    if (-not (Test-Path $buildDir)) {
        & cmake -S $root -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
    }
    & cmake --build $buildDir --config Release --target RawrXD-Win32IDE 2>&1
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# 3. Run VSIX agentic test
& (Join-Path $root "tests\Test-VSIXLoaderAgentic.ps1") -AmazonQVsix $AmazonQVsix -GitHubCopilotVsix $GitHubCopilotVsix -SkipCopy:$false
