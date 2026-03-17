#!/usr/bin/env pwsh
<#
.SYNOPSIS
    The hammer of its mountains — one control surface for the RawrXD fortress.

.DESCRIPTION
    Reverse-engineered twenty times; this is the purest wire. One script to:
    - Resolve the single canonical root (D:\RawrXD or LAZY_INIT_IDE_ROOT).
    - Audit E: (or any root) and bring source/compilers into the fortress.
    - Run build and test through the same root.
    Wrapped elegantly in PowerShell. No scattered paths; one hammer.

.EXAMPLE
    .\RawrXD-Fortress.ps1 Status
    .\RawrXD-Fortress.ps1 Audit -AuditOnly
    .\RawrXD-Fortress.ps1 Wire -CopySource -CopyCompilers
    .\RawrXD-Fortress.ps1 Build
    .\RawrXD-Fortress.ps1 Test
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Wire the root once; all paths flow from here.
$Script:FortressScriptRoot = $PSScriptRoot
. (Join-Path $Script:FortressScriptRoot "RawrXD_Root.ps1")

function Get-FortressRoot {
    Get-RawrXDRoot
}

function Invoke-FortressStatus {
    $root = Get-FortressRoot
    Write-Host ""
    Write-Host "  Fortress root: $root" -ForegroundColor Cyan
    Write-Host "  LAZY_INIT_IDE_ROOT: $(if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { '(not set)' })" -ForegroundColor Gray
    $bin = Join-Path $root "bin"
    $cli = Join-Path $root "src\cli\rawrxd_cli_compiler.cpp"
    $audit = Join-Path $root "audit_manifest"
    Write-Host "  bin exists: $(Test-Path $bin)" -ForegroundColor $(if (Test-Path $bin) { "Green" } else { "Yellow" })
    Write-Host "  CLI source: $(Test-Path $cli)" -ForegroundColor $(if (Test-Path $cli) { "Green" } else { "Yellow" })
    Write-Host "  audit_manifest: $(Test-Path $audit)" -ForegroundColor Gray
    Write-Host ""
}

function Invoke-FortressAudit {
    param(
        [switch]$AuditOnly,
        [switch]$CopySource,
        [switch]$CopyCompilers,
        [switch]$CopyAll,
        [string]$SourceRoot = "E:\RawrXD",
        [switch]$OverwriteNewer
    )
    $dest = Get-FortressRoot
    $auditScript = Join-Path $Script:FortressScriptRoot "Audit-E-Drive-And-BringTo-RawrXD.ps1"
    if (-not (Test-Path $auditScript)) {
        Write-Error "Audit script not found: $auditScript"
        return
    }
    $params = @{
        DestRoot = $dest
        SourceRoot = $SourceRoot
    }
    if ($AuditOnly) { $params.AuditOnly = $true }
    if ($CopySource) { $params.CopySource = $true }
    if ($CopyCompilers) { $params.CopyCompilers = $true }
    if ($CopyAll) { $params.CopyAll = $true }
    if ($OverwriteNewer) { $params.OverwriteNewer = $true }
    & $auditScript @params
}

function Invoke-FortressWire {
    param(
        [switch]$Source,
        [switch]$Compilers,
        [switch]$All,
        [string]$From = "E:\RawrXD",
        [switch]$OverwriteNewer
    )
    $copySource = $Source -or $All
    $copyCompilers = $Compilers -or $All
    if (-not ($copySource -or $copyCompilers)) {
        Write-Host "Specify -Source, -Compilers, or -All to copy." -ForegroundColor Yellow
        return
    }
    Invoke-FortressAudit -CopySource:$copySource -CopyCompilers:$copyCompilers -SourceRoot $From -OverwriteNewer:$OverwriteNewer
}

function Invoke-FortressBuild {
    param(
        [ValidateSet("Phase1", "MASMBridge", "Universal")]
        [string]$Target = "Phase1"
    )
    $root = Get-FortressRoot
    Push-Location $root
    try {
        if ($Target -eq "Phase1") {
            $script = Join-Path $Script:FortressScriptRoot "Build-Phase1.ps1"
            if (Test-Path $script) { & $script } else { Write-Host "Build-Phase1.ps1 not found." -ForegroundColor Yellow }
        } elseif ($Target -eq "MASMBridge") {
            $script = Join-Path $Script:FortressScriptRoot "Build-MASMBridge.ps1"
            if (Test-Path $script) { & $script } else { Write-Host "Build-MASMBridge.ps1 not found." -ForegroundColor Yellow }
        } else {
            $script = Join-Path $root "tests\Test-UniversalCompiler.ps1"
            if (Test-Path $script) { & $script } else { Write-Host "Test-UniversalCompiler.ps1 not found." -ForegroundColor Yellow }
        }
    } finally {
        Pop-Location
    }
}

function Invoke-FortressTest {
    param([string]$RootDir = "")
    $root = Get-FortressRoot
    $testScript = Join-Path $root "tests\Test-UniversalCompiler.ps1"
    if (-not (Test-Path $testScript)) {
        Write-Host "Test script not found: $testScript" -ForegroundColor Yellow
        return
    }
    if ($RootDir) {
        & $testScript -RootDir $RootDir
    } else {
        & $testScript -RootDir $root
    }
}

# Single entry point: verb + optional args
$verb = $args[0]
if (-not $verb) {
    Write-Host "RawrXD Fortress — the hammer of its mountains." -ForegroundColor Cyan
    Write-Host "  Status  : show root and key paths" -ForegroundColor Gray
    Write-Host "  Audit   : E→D audit (-AuditOnly, -CopySource, -CopyCompilers, -CopyAll)" -ForegroundColor Gray
    Write-Host "  Wire    : bring E: into fortress (-Source, -Compilers, -All, -From E:\RawrXD)" -ForegroundColor Gray
    Write-Host "  Build   : Phase1 | MASMBridge | Universal" -ForegroundColor Gray
    Write-Host "  Test    : universal compiler test (optional -RootDir)" -ForegroundColor Gray
    Write-Host ""
    Invoke-FortressStatus
    return
}

$verb = $verb.ToLowerInvariant()
$remaining = $args[1..($args.Length - 1)]

switch ($verb) {
    "status"  { Invoke-FortressStatus }
    "audit"   { Invoke-FortressAudit @remaining }
    "wire"    { Invoke-FortressWire @remaining }
    "build"   { Invoke-FortressBuild @remaining }
    "test"    { Invoke-FortressTest @remaining }
    default   {
        Write-Host "Unknown verb: $verb" -ForegroundColor Red
        Write-Host "Use: Status | Audit | Wire | Build | Test"
    }
}
