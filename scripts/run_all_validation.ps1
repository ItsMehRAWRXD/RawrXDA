#requires -Version 5.1
<#
.SYNOPSIS
    One-command foundation validation: smoke → optional editor → optional first-token.

.DESCRIPTION
    Runs the full validation stack in order. Use after build to confirm runtime
    integration and editor surface. Exits with first failure code (or 0 if all pass).
    For X+4 hotpatch memory check, run scripts\test_x4_hotswap.ps1 separately.

.PARAMETER ExePath
    Passed to smoke and first-token scripts.

.PARAMETER SkipEditorCheck
    Do not run editor-surface validation after smoke.

.PARAMETER SkipFirstToken
    Do not run first-token script (faster run).

.PARAMETER FullFirstToken
    Run first-token with model + memory check (default is -SkipModelLoad -SkipMemoryCheck).

.EXAMPLE
    .\run_all_validation.ps1
    .\run_all_validation.ps1 -FullFirstToken
#>
param(
    [string]$ExePath = "",
    [switch]$SkipEditorCheck,
    [switch]$SkipFirstToken,
    [switch]$FullFirstToken
)

$ErrorActionPreference = "Stop"
$ProjectRoot = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
if (-not (Test-Path $ProjectRoot)) { $ProjectRoot = "D:\rawrxd" }
Set-Location $ProjectRoot

$scriptDir = Join-Path $ProjectRoot "scripts"
$failed = 0

Write-Host "`n=== RawrXD Foundation Validation ===" -ForegroundColor Cyan
Write-Host ""

# 1. Smoke (required)
Write-Host "[1/3] Smoke runtime..." -ForegroundColor Yellow
$smokeArgs = @()
if ($ExePath) { $smokeArgs += "-ExePath"; $smokeArgs += $ExePath }
if (-not $SkipEditorCheck) { $smokeArgs += "-CheckEditor" }
try {
    & (Join-Path $scriptDir "smoke_runtime.ps1") @smokeArgs
    if ($LASTEXITCODE -ne 0) { $failed = $LASTEXITCODE; throw "Smoke failed with exit $LASTEXITCODE" }
} catch {
    Write-Host "  FAILED: $_" -ForegroundColor Red
    exit 1
}
Write-Host ""

# 2. Editor surface (optional unless -SkipEditorCheck; already run with -CheckEditor in smoke)
if (-not $SkipEditorCheck) {
    Write-Host "[2/3] Editor surface (validate_editor_surface)..." -ForegroundColor Yellow
    try {
        $editArgs = @()
        if ($ExePath) { $editArgs += "-ExePath"; $editArgs += $ExePath }
        & (Join-Path $scriptDir "validate_editor_surface.ps1") @editArgs
        if ($LASTEXITCODE -eq 2) { $failed = 2 }
        if ($LASTEXITCODE -eq 1) { Write-Host "  Warning: no editor child (optional)" -ForegroundColor Yellow }
    } catch {
        Write-Host "  Skip or warn: $_" -ForegroundColor Gray
    }
    Write-Host ""
}

# 3. First-token (optional)
Write-Host "[3/3] First-token..." -ForegroundColor Yellow
if (-not $SkipFirstToken) {
    try {
        $tokenArgs = @()
        if ($ExePath) { $tokenArgs += "-ExePath"; $tokenArgs += $ExePath }
        if (-not $FullFirstToken) { $tokenArgs += "-SkipModelLoad"; $tokenArgs += "-SkipMemoryCheck" }
        & (Join-Path $scriptDir "test_first_token.ps1") @tokenArgs
        if ($LASTEXITCODE -ne 0) { $failed = $LASTEXITCODE }
    } catch {
        Write-Host "  FAILED: $_" -ForegroundColor Red
        exit 2
    }
} else {
    Write-Host "  Skipped (-SkipFirstToken)" -ForegroundColor Gray
}

Write-Host ""
if ($failed -eq 0) {
    Write-Host "=== All validation passed. Foundation solid. ===" -ForegroundColor Green
} else {
    Write-Host "=== Validation finished with failure(s). Exit $failed === " -ForegroundColor Red
}
exit $failed
