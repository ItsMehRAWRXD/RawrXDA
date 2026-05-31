param(
    [string]$BuildDir = "",
    [int]$Jobs = 8,
    [switch]$ForceKill
)

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    foreach ($c in @(
            (Join-Path $repoRoot "build-ninja-ctx2"),
            (Join-Path $repoRoot "build-win32"),
            (Join-Path $repoRoot "build-ninja"))) {
        if (Test-Path -LiteralPath (Join-Path $c "CMakeCache.txt")) {
            $BuildDir = $c
            break
        }
    }
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build-ninja-ctx2"
}

$unlockScript = Join-Path $PSScriptRoot "Unlock-SWEBenchBinary.ps1"

if (-not (Test-Path -LiteralPath $unlockScript)) {
    Write-Error "[UNLOCK] Script not found: $unlockScript"
    exit 1
}

Write-Host "[UNLOCK] Releasing binary locks..." -ForegroundColor Cyan

$maxRetries = 3
$unlockExit = 1
for ($i = 1; $i -le $maxRetries; $i++) {
    & $unlockScript -BuildDir $BuildDir -Force:$ForceKill
    $unlockExit = $LASTEXITCODE
    if ($unlockExit -eq 0) {
        break
    }

    if ($i -lt $maxRetries) {
        Write-Warning "[UNLOCK] Attempt $i failed (exit code $unlockExit). Retrying..."
        Start-Sleep -Seconds (2 * $i)
    }
}

if ($unlockExit -ne 0) {
    Write-Error "[UNLOCK] FAILED (exit code $unlockExit). Aborting build to prevent LNK1104."
    exit $unlockExit
}

Write-Host "[UNLOCK] Success. Proceeding to build..." -ForegroundColor Green

$cmd = @(
    "cmake",
    "--build", $BuildDir,
    "--target", "RawrXD-SWEBench",
    "-j", "$Jobs"
)

Write-Host "[build] $($cmd -join ' ')" -ForegroundColor Cyan
& $cmd[0] $cmd[1] $cmd[2] $cmd[3] $cmd[4] $cmd[5] $cmd[6]
exit $LASTEXITCODE
