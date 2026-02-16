# ============================================================================
# Launch-And-Test.ps1 — Start RawrEngine, install extensions, run HTTP smoke test
# ============================================================================
# 1. Starts RawrEngine (build_ide\RawrEngine.exe or build\RawrEngine.exe) in a new window.
# 2. Waits for the HTTP server to be reachable on port 8080 (configurable).
# 3. Runs Install-Extensions-And-SmokeTest.ps1 (extension copy + SmokeTest-AgenticIDE.ps1).
#
# Usage: From repo root (e.g. D:\rawrxd or rwd worktree):
#   .\Launch-And-Test.ps1
#   .\Launch-And-Test.ps1 -WaitSeconds 20 -Port 8080
#
# To get the HTTP smoke test to pass, RawrEngine must listen on the given port.
# In the RawrEngine window look for: "[CompletionServer] Listening on port 8080..."
# ============================================================================

param(
    [int]$WaitSeconds = 12,
    [int]$Port = 8080,
    [string]$RepoRoot = $PSScriptRoot
)

$ErrorActionPreference = "Stop"
Set-Location $RepoRoot

# Resolve RawrEngine.exe: prefer build_ide, then build
$rawrExe = $null
foreach ($dir in @("build_ide", "build")) {
    $candidate = Join-Path $RepoRoot $dir "RawrEngine.exe"
    if (Test-Path $candidate) {
        $rawrExe = $candidate
        break
    }
}
if (-not $rawrExe) {
    Write-Host "RawrEngine.exe not found in build_ide or build. Build first, e.g.:" -ForegroundColor Red
    Write-Host "  cmake -B build_ide -G Ninja; cmake --build build_ide --config Release --target RawrEngine" -ForegroundColor Gray
    exit 1
}

Write-Host "Starting RawrEngine: $rawrExe" -ForegroundColor Cyan
Write-Host "Waiting $WaitSeconds seconds for HTTP server on port $Port..." -ForegroundColor Gray
Start-Process -FilePath $rawrExe -ArgumentList "--port", $Port -WorkingDirectory (Split-Path $rawrExe -Parent) -WindowStyle Normal

# Optional: wait for port to become reachable (retry every 1s up to WaitSeconds)
$baseUrl = "http://localhost:$Port"
$deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds)
$reached = $false
while ([DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Seconds 1
    try {
        $r = Invoke-WebRequest -Uri "$baseUrl/status" -Method GET -TimeoutSec 2 -UseBasicParsing -ErrorAction Stop
        $reached = $true
        break
    } catch {
        # keep waiting
    }
}
if (-not $reached) {
    Write-Host "RawrEngine did not respond on $baseUrl within $WaitSeconds seconds." -ForegroundColor Yellow
    Write-Host "In the RawrEngine window check for: [CompletionServer] Listening on port $Port..." -ForegroundColor Gray
    Write-Host "If the server is up later, run: .\SmokeTest-AgenticIDE.ps1 -BaseUrl $baseUrl" -ForegroundColor Gray
}

# Run extension install + smoke test (script must exist in repo)
$installScript = Join-Path $RepoRoot "Install-Extensions-And-SmokeTest.ps1"
if (Test-Path $installScript) {
    & $installScript -BaseUrl $baseUrl
    exit $LASTEXITCODE
}
$smokeScript = Join-Path $RepoRoot "SmokeTest-AgenticIDE.ps1"
if (Test-Path $smokeScript) {
    & $smokeScript -BaseUrl $baseUrl
    exit $LASTEXITCODE
}
Write-Host "Install-Extensions-And-SmokeTest.ps1 and SmokeTest-AgenticIDE.ps1 not found. Exiting." -ForegroundColor Red
exit 1
