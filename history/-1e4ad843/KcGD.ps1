#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Toolchain Command-Line Interface

.DESCRIPTION
    Unified CLI for interacting with the RawrXD production toolchain.
    Provides easy access to PE generation, encoding, and toolchain utilities.

.EXAMPLE
    .\RawrXD-CLI.ps1 generate-pe output.exe
    .\RawrXD-CLI.ps1 test-encoder
    .\RawrXD-CLI.ps1 info
#>

param(
    [Parameter(Position = 0, Mandatory = $false)]
    [ValidateSet("generate-pe", "test-encoder", "info", "list-libs", "help", "verify",
                 "install-extension", "list-extensions", "uninstall-extension")]
    [string]$Command = "help",

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$RawrXDRoot = "C:\RawrXD"
$BinDir = Join-Path $RawrXDRoot "bin"
$LibDir = Join-Path $RawrXDRoot "Libraries"
$DocsDir = Join-Path $RawrXDRoot "Docs"
$ExtensionsDir = Join-Path $env:APPDATA "RawrXD\extensions"
$ExtRegistryPath = "D:\rawrxd\extensions\registry.json"

function Write-Banner {
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  RawrXD Production Toolchain CLI v2.0" -ForegroundColor Green
    Write-Host "  Pure x64 Assembly - PE Generation & Encoding" -ForegroundColor Gray
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
}

function Invoke-GeneratePE {
    param([string]$OutputFile = "output.exe")
    
    Write-Host "🔨 Generating PE executable: $OutputFile" -ForegroundColor Yellow
    
    $generator = Join-Path $BinDir "pe_generator.exe"
    if (!(Test-Path $generator)) {
        Write-Error "PE generator not found at: $generator"
        return 1
    }
    
    $originalDir = Get-Location
    try {
        Set-Location $RawrXDRoot
        & $generator
        
        if (Test-Path "output.exe") {
            $size = (Get-Item "output.exe").Length
            Write-Host "✓ Generated PE executable: output.exe ($size bytes)" -ForegroundColor Green
            
            if ($OutputFile -ne "output.exe") {
                Move-Item "output.exe" $OutputFile -Force
                Write-Host "✓ Renamed to: $OutputFile" -ForegroundColor Green
            }
            return 0
        } else {
            Write-Error "PE generation failed"
            return 1
        }
    } finally {
        Set-Location $originalDir
    }
}

function Invoke-TestEncoder {
    Write-Host "🧪 Testing instruction encoder..." -ForegroundColor Yellow
    
    $tester = Join-Path $BinDir "instruction_encoder_test.exe"
    if (!(Test-Path $tester)) {
        Write-Warning "Encoder test executable not found at: $tester"
        Write-Host "You can still link against the libraries:" -ForegroundColor Gray
        Write-Host "  - $LibDir\rawrxd_encoder.lib" -ForegroundColor Gray
        return 1
    }
    
    Write-Host ""
    & $tester
    return $LASTEXITCODE
}

function Show-Info {
    Write-Banner
    
    Write-Host "📍 Installation Directory:" -ForegroundColor Yellow
    Write-Host "   $RawrXDRoot" -ForegroundColor White
    Write-Host ""
    
    Write-Host "📦 Executables:" -ForegroundColor Yellow
    if (Test-Path $BinDir) {
        Get-ChildItem $BinDir\*.exe | ForEach-Object {
            $sizeKB = [math]::Round($_.Length / 1KB, 2)
            Write-Host "   ✓ $($_.Name) ($sizeKB KB)" -ForegroundColor Green
        }
    } else {
        Write-Host "   [No bin directory]" -ForegroundColor Gray
    }
    Write-Host ""
    
    Write-Host "📚 Static Libraries:" -ForegroundColor Yellow
    if (Test-Path $LibDir) {
        Get-ChildItem $LibDir\*.lib | ForEach-Object {
            $sizeKB = [math]::Round($_.Length / 1KB, 2)
            Write-Host "   ✓ $($_.Name) ($sizeKB KB)" -ForegroundColor Green
        }
    } else {
        Write-Host "   [Libraries not found]" -ForegroundColor Gray
    }
    Write-Host ""
    
    Write-Host "📖 Documentation:" -ForegroundColor Yellow
    if (Test-Path $DocsDir) {
        $docCount = (Get-ChildItem $DocsDir\*.md).Count
        Write-Host "   $docCount markdown files in Docs\" -ForegroundColor White
    }
    Write-Host ""
}

function Show-Libraries {
    Write-Host "📚 Available RawrXD Libraries:" -ForegroundColor Cyan
    Write-Host ""
    
    if (!(Test-Path $LibDir)) {
        Write-Error "Libraries directory not found: $LibDir"
        return 1
    }
    
    $libs = Get-ChildItem $LibDir\*.lib
    foreach ($lib in $libs) {
        $sizeKB = [math]::Round($lib.Length / 1KB, 2)
        Write-Host "  📦 $($lib.Name)" -ForegroundColor Green
        Write-Host "     Size: $sizeKB KB" -ForegroundColor Gray
        Write-Host "     Path: $($lib.FullName)" -ForegroundColor Gray
        
        # Show corresponding header if exists
        $headerName = $lib.BaseName + ".h"
        $headerPath = Join-Path $RawrXDRoot "Headers\$headerName"
        if (Test-Path $headerPath) {
            Write-Host "     Header: Headers\$headerName" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    Write-Host "💡 Link against these libraries in your C++ projects:" -ForegroundColor Yellow
    Write-Host "   link.exe your_code.obj $LibDir\rawrxd_encoder.lib /OUT:your_app.exe" -ForegroundColor White
    Write-Host ""
}

function Invoke-Verify {
    Write-Host "🔍 Verifying RawrXD Toolchain Installation..." -ForegroundColor Yellow
    Write-Host ""
    
    $allGood = $true
    
    # Check bin directory
    if (Test-Path (Join-Path $BinDir "pe_generator.exe")) {
        Write-Host "✓ PE Generator found" -ForegroundColor Green
    } else {
        Write-Host "✗ PE Generator missing" -ForegroundColor Red
        $allGood = $false
    }
    
    # Check libraries
    $requiredLibs = @("rawrxd_encoder.lib", "rawrxd_pe_gen.lib")
    foreach ($lib in $requiredLibs) {
        if (Test-Path (Join-Path $LibDir $lib)) {
            Write-Host "✓ $lib found" -ForegroundColor Green
        } else {
            Write-Host "✗ $lib missing" -ForegroundColor Red
            $allGood = $false
        }
    }
    
    # Check headers
    if (Test-Path (Join-Path $RawrXDRoot "Headers")) {
        $headerCount = (Get-ChildItem (Join-Path $RawrXDRoot "Headers\*.h")).Count
        Write-Host "✓ $headerCount header files found" -ForegroundColor Green
    } else {
        Write-Host "✗ Headers directory missing" -ForegroundColor Red
        $allGood = $false
    }
    
    # Check docs
    if (Test-Path $DocsDir) {
        $docCount = (Get-ChildItem $DocsDir\*.md).Count
        Write-Host "✓ $docCount documentation files found" -ForegroundColor Green
    } else {
        Write-Host "⚠ Documentation directory missing" -ForegroundColor Yellow
    }
    
    Write-Host ""
    if ($allGood) {
        Write-Host "✅ Installation verified successfully!" -ForegroundColor Green
        return 0
    } else {
        Write-Host "❌ Installation incomplete - run Build-And-Wire.ps1 to rebuild" -ForegroundColor Red
        return 1
    }
}

function Show-Help {
    Write-Banner
    
    Write-Host "USAGE:" -ForegroundColor Yellow
    Write-Host "  .\RawrXD-CLI.ps1 <command> [arguments]"
    Write-Host ""
    
    Write-Host "COMMANDS:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  generate-pe [output]  Generate a PE executable" -ForegroundColor Green
    Write-Host "                        Default: output.exe"
    Write-Host "                        Example: .\RawrXD-CLI.ps1 generate-pe myapp.exe"
    Write-Host ""
    
    Write-Host "  test-encoder          Run instruction encoder tests" -ForegroundColor Green
    Write-Host "                        Tests the x64 instruction encoding library"
    Write-Host ""
    
    Write-Host "  info                  Show toolchain information" -ForegroundColor Green
    Write-Host "                        Display installed components and versions"
    Write-Host ""
    
    Write-Host "  list-libs             List available static libraries" -ForegroundColor Green
    Write-Host "                        Show all .lib files with sizes and paths"
    Write-Host ""
    
    Write-Host "  verify                Verify installation integrity" -ForegroundColor Green
    Write-Host "                        Check that all required components are present"
    Write-Host ""
    
    Write-Host "  help                  Show this help message" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "EXAMPLES:" -ForegroundColor Yellow
    Write-Host "  .\RawrXD-CLI.ps1 generate-pe"
    Write-Host "  .\RawrXD-CLI.ps1 generate-pe custom_output.exe"
    Write-Host "  .\RawrXD-CLI.ps1 test-encoder"
    Write-Host "  .\RawrXD-CLI.ps1 info"
    Write-Host "  .\RawrXD-CLI.ps1 list-libs"
    Write-Host "  .\RawrXD-CLI.ps1 verify"
    Write-Host ""
    
    Write-Host "DOCUMENTATION:" -ForegroundColor Yellow
    Write-Host "  Quick Start:  $DocsDir\PE_GENERATOR_QUICK_REF.md"
    Write-Host "  Full Docs:    $DocsDir\PRODUCTION_TOOLCHAIN_DOCS.md"
    Write-Host ""
}

# Main execution
switch ($Command) {
    "generate-pe" {
        $outputFile = if ($Arguments) { $Arguments[0] } else { "output.exe" }
        exit (Invoke-GeneratePE -OutputFile $outputFile)
    }
    "test-encoder" {
        exit (Invoke-TestEncoder)
    }
    "info" {
        Show-Info
        exit 0
    }
    "list-libs" {
        Show-Libraries
        exit 0
    }
    "verify" {
        exit (Invoke-Verify)
    }
    "help" {
        Show-Help
        exit 0
    }
    default {
        Show-Help
        exit 0
    }
}
