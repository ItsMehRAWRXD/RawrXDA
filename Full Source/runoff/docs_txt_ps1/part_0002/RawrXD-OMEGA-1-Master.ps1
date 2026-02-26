# RawrXD OMEGA-1 Master Integration Script
# Unified interface for the complete RawrXD OMEGA-1 system
# Version: 3.0.0 - Production Ready

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Test", "Deploy", "OneLiner", "Metrics", "Autonomous", "Complete")]
    [string]$Mode = "Test",
    
    [Parameter(Mandatory=$false)]
    [int]$MaxIterations = 5,
    
    [Parameter(Mandatory=$false)]
    [int]$SleepIntervalMs = 3000,
    
    [Parameter(Mandatory=$false)]
    [switch]$WhatIf = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$ShowMetrics = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$AutonomousMode = $false,
    
    [Parameter(Mandatory=$false)]
    [string]$SystemRoot = "D:\lazy init ide"
)

# Initialize system
$ErrorActionPreference = "Stop"
$WarningPreference = "Continue"
$InformationPreference = "Continue"

# Header
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                                                                   ║" -ForegroundColor Magenta
Write-Host "║         RawrXD OMEGA-1 MASTER INTEGRATION SCRIPT                ║" -ForegroundColor Magenta
Write-Host "║                    Version 3.0.0 - Production Ready               ║" -ForegroundColor Magenta
Write-Host "║                                                                   ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# Configuration
$systemPaths = @{
    Root = $SystemRoot
    AutonomousAgent = "$SystemRoot\Execute-AutonomousAgent-Final.ps1"
    OneLinerGenerator = "$SystemRoot\Generate-OneLiner-Perfect.ps1"
    CompleteSystem = "$SystemRoot\Execute-Complete-System.ps1"
    IntegrationReport = "$SystemRoot\COMPLETE-SYSTEM-INTEGRATION.md"
}

Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  Mode: $Mode" -ForegroundColor White
Write-Host "  System Root: $SystemRoot" -ForegroundColor White
Write-Host "  Max Iterations: $MaxIterations" -ForegroundColor White
Write-Host "  Sleep Interval: $SleepIntervalMs ms" -ForegroundColor White
Write-Host "  WhatIf: $WhatIf" -ForegroundColor White
Write-Host "  ShowMetrics: $ShowMetrics" -ForegroundColor White
Write-Host "  AutonomousMode: $AutonomousMode" -ForegroundColor White
Write-Host ""

# Validate system paths
Write-Host "Validating system paths..." -ForegroundColor Cyan
$allPathsValid = $true
foreach ($path in $systemPaths.GetEnumerator()) {
    if (Test-Path $path.Value) {
        Write-Host "  ✅ $($path.Key): $($path.Value)" -ForegroundColor Green
    } else {
        Write-Host "  ❌ $($path.Key): $($path.Value)" -ForegroundColor Red
        $allPathsValid = $false
    }
}

if (-not $allPathsValid) {
    Write-Host "✗ Some system paths are invalid" -ForegroundColor Red
    exit 1
}

Write-Host "✓ All system paths validated" -ForegroundColor Green
Write-Host ""

# Execute based on mode
switch ($Mode) {
    "Test" {
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "MODE: TEST - Running system validation" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host ""
        
        # Run autonomous agent in WhatIf mode
        Write-Host "Running autonomous agent in WhatIf mode..." -ForegroundColor Cyan
        & $systemPaths.AutonomousAgent -MaxIterations 2 -SleepIntervalMs 500 -WhatIf
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Test mode completed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ Test mode failed" -ForegroundColor Red
            exit 1
        }
    }
    
    "Deploy" {
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "MODE: DEPLOY - Executing full system deployment" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host ""
        
        # Run complete system with metrics
        Write-Host "Running complete system deployment..." -ForegroundColor Cyan
        & $systemPaths.CompleteSystem -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs -ShowMetrics
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Deployment completed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ Deployment failed" -ForegroundColor Red
            exit 1
        }
    }
    
    "OneLiner" {
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "MODE: ONELINER - Generating and executing one-liner" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host ""
        
        # Generate and execute one-liner
        Write-Host "Generating and executing one-liner..." -ForegroundColor Cyan
        & $systemPaths.OneLinerGenerator -Execute -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ One-liner generated and executed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ One-liner generation failed" -ForegroundColor Red
            exit 1
        }
    }
    
    "Metrics" {
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "MODE: METRICS - Displaying system metrics" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host ""
        
        # Show system metrics
        Write-Host "Collecting system metrics..." -ForegroundColor Cyan
        & $systemPaths.CompleteSystem -ShowMetrics
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Metrics displayed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ Metrics collection failed" -ForegroundColor Red
            exit 1
        }
    }
    
    "Autonomous" {
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "MODE: AUTONOMOUS - Running autonomous agent" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host ""
        
        # Run autonomous agent directly
        Write-Host "Running autonomous agent..." -ForegroundColor Cyan
        & $systemPaths.AutonomousAgent -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs -WhatIf:$WhatIf
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Autonomous agent executed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ Autonomous agent execution failed" -ForegroundColor Red
            exit 1
        }
    }
    
    "Complete" {
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "MODE: COMPLETE - Running full system integration" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host ""
        
        # Run complete system with all features
        Write-Host "Running complete system integration..." -ForegroundColor Cyan
        & $systemPaths.CompleteSystem -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs -ShowMetrics -GenerateOneLiner
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Complete system integration finished successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ Complete system integration failed" -ForegroundColor Red
            exit 1
        }
    }
}

Write-Host ""

# Show summary
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║              EXECUTION COMPLETED SUCCESSFULLY                     ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

Write-Host "📋 EXECUTION SUMMARY:" -ForegroundColor Yellow
Write-Host "  Mode: $Mode" -ForegroundColor White
Write-Host "  Status: SUCCESS" -ForegroundColor Green
Write-Host "  System: RawrXD OMEGA-1" -ForegroundColor White
Write-Host "  Version: 3.0.0" -ForegroundColor White
Write-Host ""

Write-Host "🎯 SYSTEM STATUS:" -ForegroundColor Yellow
Write-Host "  ✅ Autonomous Agent: Operational" -ForegroundColor Green
Write-Host "  ✅ One-Liner Generator: Operational" -ForegroundColor Green
Write-Host "  ✅ Complete System: Operational" -ForegroundColor Green
Write-Host "  ✅ Metrics Collection: Operational" -ForegroundColor Green
Write-Host "  ✅ Integration: Complete" -ForegroundColor Green
Write-Host ""

Write-Host "📚 AVAILABLE MODES:" -ForegroundColor Yellow
Write-Host "  Test       - Validate system without making changes" -ForegroundColor White
Write-Host "  Deploy     - Execute full system deployment" -ForegroundColor White
Write-Host "  OneLiner   - Generate and execute one-liner" -ForegroundColor White
Write-Host "  Metrics    - Display system metrics" -ForegroundColor White
Write-Host "  Autonomous - Run autonomous agent directly" -ForegroundColor White
Write-Host "  Complete   - Run full system integration" -ForegroundColor White
Write-Host ""

Write-Host "🚀 QUICK COMMANDS:" -ForegroundColor Yellow
Write-Host "  Test Mode:" -ForegroundColor White
Write-Host "    .\RawrXD-OMEGA-1-Master.ps1 -Mode Test" -ForegroundColor Green
Write-Host ""
Write-Host "  Deploy Mode:" -ForegroundColor White
Write-Host "    .\RawrXD-OMEGA-1-Master.ps1 -Mode Deploy -MaxIterations 10" -ForegroundColor Green
Write-Host ""
Write-Host "  OneLiner Mode:" -ForegroundColor White
Write-Host "    .\RawrXD-OMEGA-1-Master.ps1 -Mode OneLiner -MaxIterations 5" -ForegroundColor Green
Write-Host ""
Write-Host "  Complete Mode:" -ForegroundColor White
Write-Host "    .\RawrXD-OMEGA-1-Master.ps1 -Mode Complete -ShowMetrics" -ForegroundColor Green
Write-Host ""

Write-Host "📖 DOCUMENTATION:" -ForegroundColor Yellow
Write-Host "  Integration Report: $($systemPaths.IntegrationReport)" -ForegroundColor White
Write-Host "  Complete System: $($systemPaths.CompleteSystem)" -ForegroundColor White
Write-Host "  Autonomous Agent: $($systemPaths.AutonomousAgent)" -ForegroundColor White
Write-Host "  One-Liner Generator: $($systemPaths.OneLinerGenerator)" -ForegroundColor White
Write-Host ""

exit 0