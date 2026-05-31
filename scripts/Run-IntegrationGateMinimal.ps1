#Requires -Version 5.1
<#
.SYNOPSIS
  Minimal production gate: offline ExtensionInstallerSmoke + Run-TurnkeyIdeSmoke, one JSON summary.

.DESCRIPTION
  No separate "integration-full" product — this script is the smallest deterministic sequence
  that exercises extension-installer stack (offline) and the turnkey agentic/copilot smoke chain.

.PARAMETER BuildDir
  CMake binary dir. If empty, first match among build-win32, build-ninja, build-ninja-ctx2, build with CMakeCache.txt.

.PARAMETER SkipExtensionInstaller
  Skip ExtensionInstallerSmoke.exe (e.g. when target not built).

.PARAMETER SkipTurnkey
  Skip scripts/Run-TurnkeyIdeSmoke.ps1.

.PARAMETER ExtensionInstallerArgs
  Extra args for ExtensionInstallerSmoke (default: none = full offline suite per CMake comment).

.PARAMETER TryBuildExtensionInstaller
  If the exe is missing, run: cmake --build <BuildDir> --target ExtensionInstallerSmoke

.PARAMETER TurnkeyArgs
  Additional arguments passed to Run-TurnkeyIdeSmoke.ps1 (e.g. -SkipCopilotCli).
#>
param(
    [string]$BuildDir = "",
    [switch]$SkipExtensionInstaller,
    [switch]$SkipTurnkey,
    [string[]]$ExtensionInstallerArgs = @(),
    [switch]$TryBuildExtensionInstaller,
    [string[]]$TurnkeyArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$logDir = Join-Path $repoRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$summaryPath = Join-Path $logDir "integration_gate_minimal_last.json"

function Resolve-BuildDir {
    param([string]$Prefer)
    if ($Prefer -and (Test-Path -LiteralPath (Join-Path $Prefer "CMakeCache.txt"))) {
        return (Resolve-Path -LiteralPath $Prefer).Path
    }
    foreach ($c in @(
            (Join-Path $repoRoot "build-win32"),
            (Join-Path $repoRoot "build-ninja"),
            (Join-Path $repoRoot "build-ninja-ctx2"),
            (Join-Path $repoRoot "build_smoke_auto"),
            (Join-Path $repoRoot "build"))) {
        $cache = Join-Path $c "CMakeCache.txt"
        if (Test-Path -LiteralPath $cache) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }
    return $null
}

function Find-ExtensionInstallerExe([string]$Bd) {
    if (-not $Bd) { return $null }
    foreach ($p in @(
            (Join-Path $Bd "bin\Release\ExtensionInstallerSmoke.exe"),
            (Join-Path $Bd "bin\Debug\ExtensionInstallerSmoke.exe"),
            (Join-Path $Bd "bin\ExtensionInstallerSmoke.exe"),
            (Join-Path $Bd "ExtensionInstallerSmoke.exe"))) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

$bd = Resolve-BuildDir -Prefer $BuildDir
$steps = [ordered]@{}
$failed = $false

function Invoke-Step([string]$name, [scriptblock]$block) {
    try {
        & $block
        $script:steps[$name] = "ok"
        Write-Host "[gate] PASS: $name" -ForegroundColor Green
    }
    catch {
        $script:steps[$name] = "fail: $($_.Exception.Message)"
        $script:failed = $true
        Write-Host "[gate] FAIL: $name — $($_.Exception.Message)" -ForegroundColor Red
    }
}

if (-not $SkipExtensionInstaller) {
    Invoke-Step "extension_installer_smoke_offline" {
        if (-not $bd) {
            throw "No CMake build dir (CMakeCache.txt). Pass -BuildDir or configure build-win32|build-ninja|build."
        }
        $exe = Find-ExtensionInstallerExe -Bd $bd
        if (-not $exe -and $TryBuildExtensionInstaller) {
            Write-Host "[gate] Building ExtensionInstallerSmoke in $bd" -ForegroundColor Cyan
            & cmake --build $bd --target ExtensionInstallerSmoke --parallel
            if ($LASTEXITCODE -ne 0) { throw "cmake build ExtensionInstallerSmoke exit $LASTEXITCODE" }
            $exe = Find-ExtensionInstallerExe -Bd $bd
        }
        if (-not $exe) {
            throw "ExtensionInstallerSmoke.exe not under $bd\bin (build target ExtensionInstallerSmoke or -TryBuildExtensionInstaller)"
        }
        & $exe @ExtensionInstallerArgs
        if ($LASTEXITCODE -ne 0) { throw "ExtensionInstallerSmoke exit $LASTEXITCODE" }
    }
}
else {
    $steps["extension_installer_smoke_offline"] = "skipped: -SkipExtensionInstaller"
}

if (-not $SkipTurnkey) {
    $turnkey = Join-Path $repoRoot "scripts\Run-TurnkeyIdeSmoke.ps1"
    if (-not (Test-Path -LiteralPath $turnkey)) {
        Invoke-Step "turnkey_ide_smoke" { throw "missing Run-TurnkeyIdeSmoke.ps1" }
    }
    else {
        Invoke-Step "turnkey_ide_smoke" {
            $tk = @("-NoProfile", "-File", $turnkey, "-BuildDir", $bd) + $TurnkeyArgs
            & pwsh @tk
            if ($LASTEXITCODE -ne 0) { throw "Run-TurnkeyIdeSmoke exit $LASTEXITCODE" }
        }
    }
}
else {
    $steps["turnkey_ide_smoke"] = "skipped: -SkipTurnkey"
}

$obj = [ordered]@{
    ok = (-not $failed)
    timestamp = (Get-Date).ToString("o")
    repoRoot  = $repoRoot
    buildDir  = $bd
    steps     = $steps
}
$obj | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Host "[gate] Wrote $summaryPath" -ForegroundColor Cyan

if ($failed) { exit 1 }
exit 0
