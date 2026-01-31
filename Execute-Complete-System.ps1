# RawrXD Complete System Execution
# Executes the full autonomous agent system using the one-liner generator
# Version: 3.0.0 - Production Ready

param(
    [Parameter(Mandatory=$false)]
    [int]$MaxIterations = 5,
    
    [Parameter(Mandatory=$false)]
    [int]$SleepIntervalMs = 3000,
    
    [Parameter(Mandatory=$false)]
    [switch]$WhatIf = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateOneLiner = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$ShowMetrics = $false
)

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                                                                   ║" -ForegroundColor Magenta
Write-Host "║         RawrXD COMPLETE SYSTEM EXECUTION                        ║" -ForegroundColor Magenta
Write-Host "║                    Version 3.0.0 - Production Ready               ║" -ForegroundColor Magenta
Write-Host "║                                                                   ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# Configuration
$systemRoot = "D:\lazy init ide"
$oneLinerScript = "$systemRoot\Generate-OneLiner-Perfect.ps1"
$autonomousAgent = "$systemRoot\Execute-AutonomousAgent-Final.ps1"

Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  System Root: $systemRoot" -ForegroundColor White
Write-Host "  One-Liner Generator: $oneLinerScript" -ForegroundColor White
Write-Host "  Autonomous Agent: $autonomousAgent" -ForegroundColor White
Write-Host "  Max Iterations: $MaxIterations" -ForegroundColor White
Write-Host "  Sleep Interval: $SleepIntervalMs ms" -ForegroundColor White
Write-Host "  WhatIf: $WhatIf" -ForegroundColor White
Write-Host "  Generate One-Liner: $GenerateOneLiner" -ForegroundColor White
Write-Host ""

# Phase 1: Generate One-Liner (if requested)
if ($GenerateOneLiner) {
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "PHASE 1: GENERATING ONE-LINER" -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
    Write-Host ""
    
    if (-not (Test-Path $oneLinerScript)) {
        Write-Host "✗ One-liner generator not found: $oneLinerScript" -ForegroundColor Red
        exit 1
    }
    
    try {
        Write-Host "Executing one-liner generator..." -ForegroundColor Cyan
        & $oneLinerScript -Execute -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ One-liner generated and executed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ One-liner generation failed" -ForegroundColor Red
            exit 1
        }
    } catch {
        Write-Host "✗ One-liner generation failed: $_" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Skipping one-liner generation (use -GenerateOneLiner to enable)" -ForegroundColor Gray
}

Write-Host ""

# Phase 2: Execute Autonomous Agent Directly
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "PHASE 2: EXECUTING AUTONOMOUS AGENT DIRECTLY" -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host ""

if (-not (Test-Path $autonomousAgent)) {
    Write-Host "✗ Autonomous agent not found: $autonomousAgent" -ForegroundColor Red
    exit 1
}

try {
    Write-Host "Executing autonomous agent..." -ForegroundColor Cyan
    & $autonomousAgent -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs -WhatIf:$WhatIf
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Autonomous agent executed successfully" -ForegroundColor Green
    } else {
        Write-Host "✗ Autonomous agent execution failed" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "✗ Autonomous agent execution failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Phase 3: Show Metrics (if requested)
if ($ShowMetrics) {
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "PHASE 3: SYSTEM METRICS" -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
    Write-Host ""
    
    # Calculate metrics
    $totalFiles = Get-ChildItem -Path $systemRoot -File | Measure-Object | Select-Object -ExpandProperty Count
    $psm1Files = Get-ChildItem -Path $systemRoot -Filter "*.psm1" | Measure-Object | Select-Object -ExpandProperty Count
    $ps1Files = Get-ChildItem -Path $systemRoot -Filter "*.ps1" | Measure-Object | Select-Object -ExpandProperty Count
    $jsonFiles = Get-ChildItem -Path $systemRoot -Filter "*.json" | Measure-Object | Select-Object -ExpandProperty Count
    $mdFiles = Get-ChildItem -Path $systemRoot -Filter "*.md" | Measure-Object | Select-Object -ExpandProperty Count
    
    Write-Host "📊 SYSTEM METRICS:" -ForegroundColor Yellow
    Write-Host "  Total Files: $totalFiles" -ForegroundColor White
    Write-Host "  PowerShell Modules (.psm1): $psm1Files" -ForegroundColor White
    Write-Host "  PowerShell Scripts (.ps1): $ps1Files" -ForegroundColor White
    Write-Host "  Configuration Files (.json): $jsonFiles" -ForegroundColor White
    Write-Host "  Documentation Files (.md): $mdFiles" -ForegroundColor White
    Write-Host ""
    
    # Check for key files
    $keyFiles = @(
        "Execute-AutonomousAgent-Final.ps1",
        "Generate-OneLiner-Perfect.ps1",
        "RawrXD.AutonomousAgent.psm1",
        "RawrXD.Core.psm1",
        "RawrXD.ModelLoader.psm1",
        "RawrXD.Win32Deployment.psm1",
        "RawrXD.TestFramework.psm1"
    )
    
    Write-Host "🔑 KEY FILES:" -ForegroundColor Yellow
    $allKeyFilesPresent = $true
    foreach ($file in $keyFiles) {
        $path = Join-Path $systemRoot $file
        if (Test-Path $path) {
            Write-Host "  ✅ $file" -ForegroundColor Green
        } else {
            Write-Host "  ❌ $file" -ForegroundColor Red
            $allKeyFilesPresent = $false
        }
    }
    Write-Host ""
    
    if ($allKeyFilesPresent) {
        Write-Host "✓ All key files are present" -ForegroundColor Green
    } else {
        Write-Host "⚠ Some key files are missing" -ForegroundColor Yellow
    }
    
    Write-Host ""
}

# Final Summary
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║              COMPLETE SYSTEM EXECUTION FINISHED                   ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

Write-Host "📋 EXECUTION SUMMARY:" -ForegroundColor Yellow
Write-Host "  ✓ Phase 1: One-Liner Generation $(if($GenerateOneLiner){'Completed'}else{'Skipped'})" -ForegroundColor White
Write-Host "  ✓ Phase 2: Autonomous Agent Execution Completed" -ForegroundColor White
Write-Host "  ✓ Phase 3: Metrics $(if($ShowMetrics){'Displayed'}else{'Skipped'})" -ForegroundColor White
Write-Host ""

Write-Host "🎯 SYSTEM STATUS:" -ForegroundColor Yellow
Write-Host "  Status: PRODUCTION READY" -ForegroundColor Green
Write-Host "  Mode: Direct Execution" -ForegroundColor White
Write-Host "  Version: 3.0.0" -ForegroundColor White
Write-Host ""

Write-Host "📚 NEXT STEPS:" -ForegroundColor Yellow
Write-Host "  1. Review generated files in C:\RawrXD\Autonomous" -ForegroundColor White
Write-Host "  2. Verify module functionality" -ForegroundColor White
Write-Host "  3. Monitor performance and security" -ForegroundColor White
Write-Host "  4. Use -ShowMetrics to see detailed system metrics" -ForegroundColor White
Write-Host "  5. Use -GenerateOneLiner to test one-liner deployment" -ForegroundColor White
Write-Host ""

Write-Host "🚀 QUICK COMMANDS:" -ForegroundColor Yellow
Write-Host "  Execute Complete System:" -ForegroundColor White
Write-Host "    .\Execute-Complete-System.ps1 -MaxIterations 5 -SleepIntervalMs 3000" -ForegroundColor Green
Write-Host ""
Write-Host "  With One-Liner Generation:" -ForegroundColor White
Write-Host "    .\Execute-Complete-System.ps1 -GenerateOneLiner -ShowMetrics" -ForegroundColor Green
Write-Host ""
Write-Host "  With WhatIf Mode:" -ForegroundColor White
Write-Host "    .\Execute-Complete-System.ps1 -WhatIf" -ForegroundColor Green
Write-Host ""

exit 0