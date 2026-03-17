# RawrXD Universal Compiler - PowerShell Wrapper
# Provides CLI access to compiler, build system, and validation tools
# Usage: .\rawrxd-build.ps1 -Validate -Json
# Usage: .\rawrxd-build.ps1 -Build
# Usage: .\rawrxd-build.ps1 -Audit

param(
    [string]$ProjectRoot = "D:\RawrXD-production-lazy-init",
    [switch]$Validate,
    [switch]$Audit,
    [switch]$Build,
    [switch]$CompileDirect,
    [switch]$Json,
    [switch]$Menu,
    [switch]$Help
)

function Show-Help {
    Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        RawrXD Universal Compiler & Build System           ║" -ForegroundColor Cyan
    Write-Host "║                 PowerShell Wrapper v1.0                   ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

    Write-Host "`n📋 USAGE:" -ForegroundColor Yellow
    Write-Host "  .\rawrxd-build.ps1 [options]`n"

    Write-Host "🎯 OPTIONS:" -ForegroundColor Yellow
    Write-Host "  -Validate          Run full validation suite (default)" -ForegroundColor Green
    Write-Host "  -Audit             Audit compilation environment only" -ForegroundColor Green
    Write-Host "  -Build             Compile with CMake" -ForegroundColor Green
    Write-Host "  -CompileDirect     Direct compilation without CMake" -ForegroundColor Green
    Write-Host "  -Menu              Interactive menu mode" -ForegroundColor Green
    Write-Host "  -Json              Output results as JSON" -ForegroundColor Green
    Write-Host "  -ProjectRoot       Project root directory" -ForegroundColor Green
    Write-Host "  -Help              Show this help message`n"

    Write-Host "📌 EXAMPLES:" -ForegroundColor Yellow
    Write-Host "  .\rawrxd-build.ps1                     # Full validation" -ForegroundColor Cyan
    Write-Host "  .\rawrxd-build.ps1 -Build             # Build with CMake" -ForegroundColor Cyan
    Write-Host "  .\rawrxd-build.ps1 -Audit -Json       # Audit as JSON" -ForegroundColor Cyan
    Write-Host "  .\rawrxd-build.ps1 -Menu              # Interactive menu`n"
}

function Show-Menu {
    Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║         RawrXD Build & Validation - Interactive Menu       ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

    $choice = Read-Host @"
Select operation:
  [1] Full Validation (recommended)
  [2] Environment Audit Only
  [3] Build with CMake
  [4] Direct Compilation
  [5] Generate JSON Report
  [6] Exit

Enter choice (1-6)
"@

    switch ($choice) {
        "1" { Invoke-Python -Args "--validate" }
        "2" { Invoke-Python -Args "--audit" }
        "3" { Invoke-Python -Args "--build" }
        "4" { Invoke-Python -Args "--compile-direct" }
        "5" { Invoke-Python -Args "--validate --json" }
        "6" { Write-Host "`nExiting..."; exit 0 }
        default { Write-Host "Invalid choice!"; Show-Menu }
    }
}

function Invoke-Python {
    param([string]$Args)
    
    $pythonScript = Join-Path $ProjectRoot "universal_compiler.py"
    
    if (-not (Test-Path $pythonScript)) {
        Write-Host "❌ Error: universal_compiler.py not found at $pythonScript" -ForegroundColor Red
        return
    }

    Write-Host "`n🚀 Executing: python $pythonScript $Args`n" -ForegroundColor Yellow
    
    $cmd = "python `"$pythonScript`" --project-root `"$ProjectRoot`" $Args"
    Invoke-Expression $cmd
}

function Verify-Environment {
    Write-Host "`n🔍 Pre-flight checks:" -ForegroundColor Yellow
    
    $pythonExists = python --version 2>&1 | Select-String "Python"
    if ($pythonExists) {
        Write-Host "  ✓ Python: $pythonExists" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Python: NOT FOUND - Required for full functionality" -ForegroundColor Red
        Write-Host "    Install from: https://www.python.org/downloads/" -ForegroundColor Yellow
        return $false
    }

    $cmakeExists = cmake --version 2>&1 | Select-Object -First 1
    if ($cmakeExists) {
        Write-Host "  ✓ CMake: Found" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ CMake: Not found (optional for direct compilation)" -ForegroundColor Yellow
    }

    $gppExists = g++ --version 2>&1 | Select-Object -First 1
    if ($gppExists) {
        Write-Host "  ✓ G++: Found" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ G++: Not found (optional)" -ForegroundColor Yellow
    }

    return $true
}

# Main execution
if ($Help -or ($PSBoundParameters.Count -eq 0 -and -not $Menu)) {
    Show-Help
    exit 0
}

if (-not (Verify-Environment)) {
    Write-Host "`n⚠️  Missing critical dependencies. Please install Python 3.8+`n" -ForegroundColor Yellow
    exit 1
}

$args_list = @()
if ($Validate) { $args_list += "--validate" }
if ($Audit) { $args_list += "--audit" }
if ($Build) { $args_list += "--build" }
if ($CompileDirect) { $args_list += "--compile-direct" }
if ($Json) { $args_list += "--json" }
if ($args_list.Count -eq 0) { $args_list += "--validate" }

if ($Menu) {
    Show-Menu
} else {
    Invoke-Python -Args ($args_list -join " ")
}
