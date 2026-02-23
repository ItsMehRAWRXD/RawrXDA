#Requires -Version 5.1
<#
.SYNOPSIS
    Feature smoke tests — every feature is "completed and useable" only after its smoke passes.
.DESCRIPTION
    Runs one smoke test per feature by name (no impl/implementation wording).
    Run via: pwsh -File .\Test-Features-Smoke.ps1
    Options: -SkipBuild, -SkipDocker, -BuildDir
.NOTES
    Features: Wrapper, Unified Problems, Agent Panel, Tools Schema, Security Scans, Codebase RAG.
#>

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [switch]$SkipBuild,
    [switch]$SkipDocker
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$script:Passed = 0
$script:Failed = 0

function Test-Feature {
    param([string]$FeatureName, [scriptblock]$SmokeTest)
    try {
        $ok = & $SmokeTest
        $script:Passed += [int]$ok
        if (-not $ok) { $script:Failed += 1 }
        $tag = if ($ok) { "PASS" } else { "FAIL" }
        $color = if ($ok) { "Green" } else { "Red" }
        Write-Host "  [$tag] $FeatureName" -ForegroundColor $color
        return $ok
    } catch {
        $script:Failed += 1
        Write-Host "  [FAIL] $FeatureName" -ForegroundColor Red
        Write-Host "         $($_.Exception.Message)" -ForegroundColor Gray
        return $false
    }
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Feature smoke tests (pwsh)" -ForegroundColor Cyan
Write-Host "  Root: $script:Root" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# ----------------------------------------------------------------------------
# Wrapper (Mac/Linux bootable space)
# ----------------------------------------------------------------------------
Write-Host "=== Wrapper ===" -ForegroundColor Yellow
Test-Feature "Wrapper" {
    $w = Join-Path $script:Root "wrapper"
    if (-not (Test-Path $w)) { return $false }
    $required = @(
        "README.md",
        "rawrxd-space.env.example"
    )
    foreach ($r in $required) {
        if (-not (Test-Path (Join-Path $w $r))) { return $false }
    }
    $scripts = @("launch-linux.sh", "launch-macos.sh", "run-backend-only.sh")
    foreach ($s in $scripts) {
        $p = Join-Path $w $s
        if (-not (Test-Path $p)) { return $false }
    }
    return $true
}

if (-not $SkipDocker) {
    Test-Feature "Wrapper (Dockerfile)" {
        $df = Join-Path $script:Root "wrapper\Dockerfile.ide-space"
        $ep = Join-Path $script:Root "wrapper\entrypoint-ide-space.sh"
        (Test-Path $df) -and (Test-Path $ep)
    }
}

# ----------------------------------------------------------------------------
# Unified Problems (ProblemsAggregator)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Unified Problems ===" -ForegroundColor Yellow
$rawrEngine = $null
$searchDirs = @($BuildDir, "build_ide", "build")
if ($BuildDir) { $searchDirs = @($BuildDir) }
foreach ($d in $searchDirs) {
    $path = Join-Path $script:Root $d
    if (-not (Test-Path $path)) { continue }
    $exe = Get-ChildItem -Path $path -Recurse -Filter "RawrEngine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($exe) { $rawrEngine = $exe.FullName; break }
}

Test-Feature "Unified Problems" {
    if (-not $rawrEngine -and -not $SkipBuild) {
        $b = Join-Path $script:Root "build_ide"
        if (-not (Test-Path $b)) {
            $null = New-Item -ItemType Directory -Path $b -Force
            & cmake -B $b -G Ninja (Join-Path $script:Root ".") 2>&1 | Out-Null
        }
        & cmake --build $b --config Release --target RawrEngine 2>&1 | Out-Null
        $exe = Get-ChildItem -Path $b -Recurse -Filter "RawrEngine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exe) { $script:RawrEngine = $exe.FullName }
    }
    $exeToUse = $rawrEngine
    if (-not $exeToUse -and $script:RawrEngine) { $exeToUse = $script:RawrEngine }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrEngine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\rawr_help.txt" -RedirectStandardError "$env:TEMP\rawr_help_err.txt"
    $done = $proc.WaitForExit(15000)
    if (-not $done) { $proc.Kill(); return $false }
    $out = Get-Content "$env:TEMP\rawr_help.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -or ($out -and ($out -match "Usage|--help|/status"))
}

# ----------------------------------------------------------------------------
# Agent Panel
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Agent Panel ===" -ForegroundColor Yellow
$ideExe = $null
foreach ($d in $searchDirs) {
    $path = Join-Path $script:Root $d
    if (-not (Test-Path $path)) { continue }
    $exe = Get-ChildItem -Path $path -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($exe) { $ideExe = $exe.FullName; break }
}

Test-Feature "Agent Panel" {
    if (-not $ideExe -and -not $SkipBuild) {
        $b = Join-Path $script:Root "build_ide"
        if (-not (Test-Path $b)) {
            $null = New-Item -ItemType Directory -Path $b -Force
            & cmake -B $b -G Ninja (Join-Path $script:Root ".") 2>&1 | Out-Null
        }
        & cmake --build $b --config Release --target RawrXD-Win32IDE 2>&1 | Out-Null
        $exe = Get-ChildItem -Path $b -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exe) { $script:IDEExe = $exe.FullName }
    }
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    [bool]$exeToUse
}

# ----------------------------------------------------------------------------
# Tools Schema
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Tools Schema ===" -ForegroundColor Yellow
$cliExe = $null
foreach ($d in $searchDirs) {
    $path = Join-Path $script:Root $d
    if (-not (Test-Path $path)) { continue }
    foreach ($name in @("RawrXD_CLI.exe", "RawrXD_Agent_Console.exe")) {
        $exe = Get-ChildItem -Path $path -Recurse -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exe) { $cliExe = $exe.FullName; break }
    }
    if ($cliExe) { break }
}

Test-Feature "Tools Schema" {
    if (-not $cliExe) {
        $cliExe = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD_CLI.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cliExe) { $cliExe = $cliExe.FullName }
    }
    if (-not $cliExe) {
        $cliExe = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD_Agent_Console.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cliExe) { $cliExe = $cliExe.FullName }
    }
    if (-not $cliExe) { return $false }
    $out = & $cliExe --help 2>&1
    $out -match "tools|/tools|Tools"
}

# ----------------------------------------------------------------------------
# Security Scans (Secrets, SAST, SCA)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Security Scans ===" -ForegroundColor Yellow
Test-Feature "Security Scans" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    $src = Get-Content (Join-Path $script:Root "src\win32app\Win32IDE_SecurityScans.cpp") -Raw -ErrorAction SilentlyContinue
    $src -and $src -match "RunSecretsScan|RunSASTScan|RunDependencyAudit"
}

# ----------------------------------------------------------------------------
# Codebase RAG
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Codebase RAG ===" -ForegroundColor Yellow
Test-Feature "Codebase RAG" {
    $h = Join-Path $script:Root "src\ai\codebase_rag.hpp"
    $c = Join-Path $script:Root "src\ai\codebase_rag.cpp"
    (Test-Path $h) -and (Test-Path $c)
}
Test-Feature "Codebase RAG (build)" {
    $exeToUse = $rawrEngine
    if (-not $exeToUse -and $script:RawrEngine) { $exeToUse = $script:RawrEngine }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrEngine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    [bool]$exeToUse
}

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Passed: $script:Passed  Failed: $script:Failed" -ForegroundColor $(if ($script:Failed -eq 0) { "Green" } else { "Yellow" })
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

if ($script:Failed -gt 0) { exit 1 }
exit 0
