#Requires -Version 7.0
<#
.SYNOPSIS
  Minimal post-link smoke for RawrXD_Gold: process starts and exits cleanly.

.DESCRIPTION
  RawrXD_Gold does not define RAWRXD_STANDALONE_MAIN (unlike RawrEngine), so
  src/main.cpp does not contribute a CLI --help path. The Gold binary still
  runs a short scheduler path and prints "[Scheduler] Shutdown complete" on
  success.

  Does not start HTTP/REPL. Use after: cmake --build <dir> --target RawrXD_Gold

.PARAMETER BuildDir
  CMake binary dir containing gold/RawrXD_Gold.exe. If empty, first configured dir among build-win32, build-ninja, build.

.PARAMETER RepoRoot
  Repository root; default: env RAWRXD_REPO_ROOT or parent of this script's folder.
#>
param(
    [string]$BuildDir = "",
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $RepoRoot = $env:RAWRXD_REPO_ROOT
}
if (-not $RepoRoot) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}

function Resolve-FirstCMakeBuildDir {
    param([string]$Root, [string]$Prefer)
    if ($Prefer -and (Test-Path -LiteralPath (Join-Path $Prefer "CMakeCache.txt"))) {
        return (Resolve-Path -LiteralPath $Prefer).Path
    }
    foreach ($c in @(
            (Join-Path $Root "build-win32"),
            (Join-Path $Root "build-ninja"),
            (Join-Path $Root "build-ninja-ctx2"),
            (Join-Path $Root "build"))) {
        if (Test-Path -LiteralPath (Join-Path $c "CMakeCache.txt")) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }
    return $null
}

if (-not $BuildDir) {
    $BuildDir = Resolve-FirstCMakeBuildDir -Root $RepoRoot -Prefer ""
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build-win32"
}

function Find-GoldExe([string]$Bd) {
    if (-not $Bd) { return $null }
    foreach ($p in @(
            (Join-Path $Bd "gold\Release\RawrXD_Gold.exe"),
            (Join-Path $Bd "gold\Debug\RawrXD_Gold.exe"),
            (Join-Path $Bd "gold\RawrXD_Gold.exe"))) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

$goldExe = Find-GoldExe -Bd $BuildDir
if (-not $goldExe) {
    throw "Missing RawrXD_Gold.exe under '$BuildDir\gold' (tried Release/Debug/flat). Build with: cmake --build `"$BuildDir`" --target RawrXD_Gold"
}

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $goldExe
# No argv: Gold's entry is not the standalone main.cpp CLI; args are ignored for this smoke.
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.CreateNoWindow = $true

$p = [System.Diagnostics.Process]::Start($psi)
if (-not $p) {
    throw "Failed to start RawrXD_Gold"
}
$out = $p.StandardOutput.ReadToEnd()
$err = $p.StandardError.ReadToEnd()
$p.WaitForExit()

$combined = "$out$err"

if ($p.ExitCode -ne 0) {
    Write-Host $out
    Write-Host $err
    throw "RawrXD_Gold exited with code $($p.ExitCode)"
}

if ($combined -notmatch "Shutdown complete") {
    Write-Host $out
    Write-Host $err
    throw "RawrXD_Gold output did not contain expected scheduler shutdown marker."
}

Write-Host "smoke_gold: OK ($goldExe, exit 0, scheduler shutdown)"
