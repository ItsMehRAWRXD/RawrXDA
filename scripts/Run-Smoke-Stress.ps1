# Extended smoke/soak runner — builds Win32 IDE and runs tests/smoke_e2e coordinator.
param(
    [switch]$Stress,
    [string]$BuildDir = "build-win32"
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Stop-Process -Name "RawrXD-Win32IDE", "RawrXD-ExtensionHost" -Force -ErrorAction SilentlyContinue
Get-Item -Path Env:RAWRXD_SMOKE_* -ErrorAction SilentlyContinue | Remove-Item -ErrorAction SilentlyContinue

Push-Location $RepoRoot
try {
    cmake --build (Join-Path $RepoRoot $BuildDir) --target RawrXD-Win32IDE -j 8
    $bin = Join-Path $RepoRoot "$BuildDir\bin\RawrXD-Win32IDE.exe"
    if (-not (Test-Path $bin)) { throw "Missing binary: $bin" }

    $coordinator = Join-Path $RepoRoot "tests\smoke_e2e\0_SmokeTestCoordinator.ps1"
    if ($Stress) {
        & $coordinator -BinaryPath $bin -Stress
    } else {
        & $coordinator -BinaryPath $bin
    }
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
