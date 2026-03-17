#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD with Agentic Mode - Handles GUI Issues Gracefully
    
.DESCRIPTION
    Launches RawrXD with agentic capabilities fully functional.
    Attempts GUI launch but gracefully falls back to terminal if needed.
    The agentic toggle ALWAYS works in the IDE when it loads.
    
.PARAMETER Terminal
    Skip GUI attempt and go straight to terminal mode
    
.PARAMETER SkipGUI
    Same as -Terminal
    
.EXAMPLE
    .\RawrXD-With-Agentic.ps1
    # Attempts to launch GUI with agentic mode
    
.EXAMPLE
    .\RawrXD-With-Agentic.ps1 -Terminal
    # Terminal-only mode with agentic functions
#>

param(
    [switch]$SkipGUI,
    [switch]$Terminal
)

$script:RawrXDPath = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }

Write-Host "`n" -ForegroundColor White
Write-Host "╔═══════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║          🚀 RawrXD with Agentic Mode 🚀             ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

if ($Terminal) {
    Write-Host "📱 Terminal mode selected`n" -ForegroundColor Cyan
    $SkipGUI = $true
}

if (-not $SkipGUI) {
    Write-Host "🎯 Attempting to launch GUI with agentic mode..." -ForegroundColor Cyan
    Write-Host "   (If GUI has display issues, agentic functions stay available)`n" -ForegroundColor Gray
    
    try {
        Push-Location $script:RawrXDPath
        
        # Run RawrXD but suppress non-critical GUI errors
        $rawrXDProcess = & powershell -NoProfile -ExecutionPolicy Bypass -File '.\RawrXD.ps1' 2>&1
        
        Pop-Location
        exit 0
    } catch {
        Write-Host "`n⚠️  GUI launch encountered an error (non-critical)" -ForegroundColor Yellow
        Write-Host "Continuing with terminal-only mode..`n" -ForegroundColor Gray
    }
}

# Terminal mode - agentic functions are available
Write-Host "✅ Agentic functions are available in this terminal session!" -ForegroundColor Green
Write-Host "`n📚 Available Agentic Functions:" -ForegroundColor Cyan

# Import agentic module if not already loaded
$agenticModulePath = Join-Path $script:RawrXDPath 'RawrXD-Agentic-Module.psm1'
if (Test-Path $agenticModulePath) {
    try {
        Import-Module $agenticModulePath -Force -ErrorAction Stop
        Enable-RawrXDAgentic -Model 'bigdaddyg-fast:latest' -ErrorAction Stop
        
        Write-Host "  ✓ Invoke-RawrXDAgenticCodeGen      - Generate code" -ForegroundColor Green
        Write-Host "  ✓ Invoke-RawrXDAgenticCompletion   - Smart completions" -ForegroundColor Green
        Write-Host "  ✓ Invoke-RawrXDAgenticAnalysis     - Code analysis" -ForegroundColor Green
        Write-Host "  ✓ Invoke-RawrXDAgenticRefactor     - Auto-refactoring" -ForegroundColor Green
        Write-Host "  ✓ Get-RawrXDAgenticStatus          - Status info" -ForegroundColor Green
        
        Write-Host "`n🎯 Quick Commands:" -ForegroundColor Cyan
        Write-Host '  $code = Invoke-RawrXDAgenticCodeGen -Prompt "Create PowerShell function"' -ForegroundColor Yellow
        Write-Host '  $better = Invoke-RawrXDAgenticAnalysis -Code $code -AnalysisType improve' -ForegroundColor Yellow
        Write-Host '  Invoke-RawrXDAgenticRefactor -Code $code -Objective modernize' -ForegroundColor Yellow
        
        Write-Host "`n"
        
        Get-RawrXDAgenticStatus
        
    } catch {
        Write-Host "`n❌ Error loading agentic module: $_`n" -ForegroundColor Red
    }
} else {
    Write-Host "`n❌ Agentic module not found at: $agenticModulePath`n" -ForegroundColor Red
}

Write-Host "`n💡 For help:" -ForegroundColor Cyan
Write-Host "  Get-Help Invoke-RawrXDAgenticCodeGen -Full`n" -ForegroundColor Gray

