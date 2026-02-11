# =============================================================================
# sync_command_registry.ps1 — Automated Command Registry Synchronization
# =============================================================================
# Purpose: Runs auto_register_commands.py, validates output, integrates with build
# Architecture: C++20, Win32, no Qt, no exceptions
# Usage:
#   .\scripts\sync_command_registry.ps1              # Full sync
#   .\scripts\sync_command_registry.ps1 -DryRun      # Preview only
#   .\scripts\sync_command_registry.ps1 -Validate     # Check coverage only
#   .\scripts\sync_command_registry.ps1 -Watch        # File watcher mode
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

param(
    [switch]$DryRun,
    [switch]$Validate,
    [switch]$Watch,
    [switch]$Verbose,
    [string]$SrcRoot = ""
)

$ErrorActionPreference = "Stop"
# Resolve project root: scripts/ is one level below project root
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -or $ScriptDir -eq "") { 
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path 
}
if (-not $ScriptDir -or $ScriptDir -eq "") { $ScriptDir = "D:\rawrxd\scripts" }
$ProjectRoot = Split-Path -Parent $ScriptDir
if (-not $ProjectRoot -or $ProjectRoot -eq "" -or $ProjectRoot -eq [System.IO.Path]::GetPathRoot($ProjectRoot)) { 
    $ProjectRoot = "D:\rawrxd" 
}
if ($SrcRoot -eq "") { $SrcRoot = Join-Path $ProjectRoot "src" }

$PythonScript = Join-Path $ProjectRoot "scripts\auto_register_commands.py"
$OutputHpp = Join-Path $SrcRoot "core\auto_feature_registry.hpp"
$OutputCpp = Join-Path $SrcRoot "core\auto_feature_registry.cpp"
$ReportDir = Join-Path $ProjectRoot "reports"
$CoverageReport = Join-Path $ReportDir "command_coverage_report.md"
$RegistryJson = Join-Path $ReportDir "command_registry.json"

# =============================================================================
# HASH TRACKING — Detect if source files changed since last generation
# =============================================================================

$HashFile = Join-Path $ProjectRoot ".command_registry_hash"

function Get-SourceHash {
    $filesToHash = @(
        (Join-Path $SrcRoot "win32app\Win32IDE.h"),
        (Join-Path $SrcRoot "core\feature_handlers.h"),
        (Join-Path $SrcRoot "core\feature_registration.cpp"),
        (Join-Path $SrcRoot "core\shared_feature_dispatch.h")
    )
    
    $hashInput = ""
    foreach ($f in $filesToHash) {
        if (Test-Path $f) {
            $hashInput += (Get-FileHash -Path $f -Algorithm MD5).Hash
        }
    }
    
    $md5 = [System.Security.Cryptography.MD5]::Create()
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($hashInput)
    $hash = $md5.ComputeHash($bytes)
    return [BitConverter]::ToString($hash).Replace("-", "").Substring(0, 16)
}

function Get-LastHash {
    if (Test-Path $HashFile) {
        return (Get-Content $HashFile -Raw).Trim()
    }
    return ""
}

function Set-LastHash($hash) {
    Set-Content -Path $HashFile -Value $hash -NoNewline
}

# =============================================================================
# CORE: Run the Python generator
# =============================================================================

function Invoke-AutoRegister {
    param([switch]$DryRun, [switch]$Verbose)
    
    $args_list = @("--src-root", $SrcRoot)
    if ($DryRun) { $args_list += "--dry-run" }
    if ($Verbose) { $args_list += "--verbose" }
    
    Write-Host "`n[SyncRegistry] Running auto_register_commands.py..." -ForegroundColor Cyan
    
    $result = & python $PythonScript @args_list 2>&1
    $exitCode = $LASTEXITCODE
    
    foreach ($line in $result) {
        if ($line -match "COMPLETE|Coverage") {
            Write-Host "  $line" -ForegroundColor Green
        } elseif ($line -match "ERROR|FAIL") {
            Write-Host "  $line" -ForegroundColor Red
        } else {
            Write-Host "  $line" -ForegroundColor Gray
        }
    }
    
    if ($exitCode -ne 0) {
        Write-Host "[SyncRegistry] FAILED (exit code $exitCode)" -ForegroundColor Red
        return $false
    }
    
    return $true
}

# =============================================================================
# VALIDATE: Check generated files exist and parse correctly
# =============================================================================

function Test-GeneratedFiles {
    $valid = $true
    
    Write-Host "`n[SyncRegistry] Validating generated files..." -ForegroundColor Cyan
    
    # Check files exist
    foreach ($f in @($OutputHpp, $OutputCpp)) {
        if (-not (Test-Path $f)) {
            Write-Host "  MISSING: $f" -ForegroundColor Red
            $valid = $false
        } else {
            $size = (Get-Item $f).Length
            Write-Host "  OK: $f ($size bytes)" -ForegroundColor Green
        }
    }
    
    # Check C++ syntax (basic — look for initAutoFeatureRegistry function)
    if (Test-Path $OutputCpp) {
        $content = Get-Content $OutputCpp -Raw
        if ($content -match "void initAutoFeatureRegistry\(\)") {
            Write-Host "  OK: initAutoFeatureRegistry() found" -ForegroundColor Green
        } else {
            Write-Host "  ERROR: initAutoFeatureRegistry() not found in cpp" -ForegroundColor Red
            $valid = $false
        }
        
        # Count autoReg() calls
        $regCount = ([regex]::Matches($content, 'autoReg\(')).Count
        Write-Host "  OK: $regCount autoReg() calls found" -ForegroundColor Green
        
        if ($regCount -lt 100) {
            Write-Host "  WARNING: Expected 200+, only found $regCount" -ForegroundColor Yellow
        }
    }
    
    # Check header syntax
    if (Test-Path $OutputHpp) {
        $content = Get-Content $OutputHpp -Raw
        $stubCount = ([regex]::Matches($content, 'CommandResult handle\w+\(')).Count
        Write-Host "  OK: $stubCount stub handler declarations in header" -ForegroundColor Green
    }
    
    # Check coverage report
    if (Test-Path $CoverageReport) {
        $report = Get-Content $CoverageReport -Raw
        if ($report -match 'Coverage\s*\|\s*([\d.]+)%') {
            Write-Host "  Coverage: $($Matches[1])%" -ForegroundColor Cyan
        }
    }
    
    # Check JSON manifest
    if (Test-Path $RegistryJson) {
        try {
            $json = Get-Content $RegistryJson -Raw | ConvertFrom-Json
            Write-Host "  OK: JSON manifest has $($json.totalAutoRegistered) entries" -ForegroundColor Green
        } catch {
            Write-Host "  ERROR: Invalid JSON manifest" -ForegroundColor Red
            $valid = $false
        }
    }
    
    return $valid
}

# =============================================================================
# COVERAGE: Display detailed coverage stats
# =============================================================================

function Show-Coverage {
    if (-not (Test-Path $RegistryJson)) {
        Write-Host "[SyncRegistry] No registry JSON found. Run sync first." -ForegroundColor Yellow
        return
    }
    
    $json = Get-Content $RegistryJson -Raw | ConvertFrom-Json
    $total = $json.totalAutoRegistered + $json.totalManualRegistered
    $withHandler = ($json.entries | Where-Object { $_.hasRealHandler }).Count
    $stubs = ($json.entries | Where-Object { -not $_.hasRealHandler }).Count
    
    Write-Host "`n╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║         COMMAND REGISTRY COVERAGE REPORT          ║" -ForegroundColor Cyan
    Write-Host "╠══════════════════════════════════════════════════╣" -ForegroundColor Cyan
    Write-Host "║  Manual registrations:     $($json.totalManualRegistered.ToString().PadLeft(5))             ║" -ForegroundColor White
    Write-Host "║  Auto registrations:       $($json.totalAutoRegistered.ToString().PadLeft(5))             ║" -ForegroundColor White
    Write-Host "║  ─────────────────────────────────               ║" -ForegroundColor DarkGray
    Write-Host "║  Total features:           $($total.ToString().PadLeft(5))             ║" -ForegroundColor Green
    Write-Host "║  With real handler:        $($withHandler.ToString().PadLeft(5))             ║" -ForegroundColor Green
    Write-Host "║  With stub handler:        $($stubs.ToString().PadLeft(5))             ║" -ForegroundColor Yellow
    Write-Host "╠══════════════════════════════════════════════════╣" -ForegroundColor Cyan
    
    # Per-group stats
    $groups = $json.entries | Group-Object { $_.group }
    Write-Host "║  BY GROUP:                                        ║" -ForegroundColor Cyan
    foreach ($g in ($groups | Sort-Object -Property Name)) {
        $name = $g.Name.Replace("FeatureGroup::", "").PadRight(20)
        $count = $g.Count.ToString().PadLeft(4)
        $real = ($g.Group | Where-Object { $_.hasRealHandler }).Count
        Write-Host "║    $name  $count  (real: $real)" -ForegroundColor White
    }
    
    Write-Host "╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

# =============================================================================
# WATCH MODE: Monitor source files and auto-regenerate
# =============================================================================

function Start-WatchMode {
    Write-Host "[SyncRegistry] Starting file watcher..." -ForegroundColor Cyan
    Write-Host "  Watching: $SrcRoot" -ForegroundColor Gray
    Write-Host "  Press Ctrl+C to stop" -ForegroundColor Gray
    
    $watchFiles = @(
        "win32app\Win32IDE.h",
        "core\feature_handlers.h",
        "core\feature_registration.cpp"
    )
    
    $lastHash = Get-SourceHash
    
    while ($true) {
        Start-Sleep -Seconds 3
        $currentHash = Get-SourceHash
        
        if ($currentHash -ne $lastHash) {
            Write-Host "`n[SyncRegistry] Source change detected! Regenerating..." -ForegroundColor Yellow
            $success = Invoke-AutoRegister
            if ($success) {
                Test-GeneratedFiles | Out-Null
                Set-LastHash $currentHash
            }
            $lastHash = $currentHash
        }
    }
}

# =============================================================================
# MAIN
# =============================================================================

Write-Host "═══════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Command Registry Sync                    " -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Project: $ProjectRoot" -ForegroundColor Gray
Write-Host "  Source:  $SrcRoot" -ForegroundColor Gray

if ($Watch) {
    Start-WatchMode
    return
}

# Check if regeneration needed
$currentHash = Get-SourceHash
$lastHash = Get-LastHash

if (-not $DryRun -and -not $Validate -and $currentHash -eq $lastHash -and (Test-Path $OutputCpp)) {
    Write-Host "`n[SyncRegistry] Source files unchanged since last sync. Skipping." -ForegroundColor DarkGray
    Write-Host "  Use -DryRun to force preview, or delete $HashFile to force regeneration." -ForegroundColor DarkGray
    Show-Coverage
    return
}

if ($Validate) {
    if (Test-GeneratedFiles) {
        Show-Coverage
        Write-Host "`n[SyncRegistry] Validation PASSED" -ForegroundColor Green
    } else {
        Write-Host "`n[SyncRegistry] Validation FAILED" -ForegroundColor Red
        exit 1
    }
    return
}

# Run generation
$success = Invoke-AutoRegister -DryRun:$DryRun -Verbose:$Verbose

if (-not $DryRun) {
    if ($success) {
        $valid = Test-GeneratedFiles
        if ($valid) {
            Set-LastHash $currentHash
            Show-Coverage
            Write-Host "`n[SyncRegistry] Sync COMPLETE" -ForegroundColor Green
        } else {
            Write-Host "`n[SyncRegistry] Generated files have issues" -ForegroundColor Red
            exit 1
        }
    }
} else {
    Write-Host "`n[SyncRegistry] DRY RUN complete (no files written)" -ForegroundColor Yellow
}
