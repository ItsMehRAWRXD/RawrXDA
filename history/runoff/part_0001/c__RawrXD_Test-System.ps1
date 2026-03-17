#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Complete RawrXD Toolchain System Test

.DESCRIPTION
    Comprehensive test suite that validates all components of the RawrXD toolchain.
    Tests build system, libraries, executables, CLI tools, and integration examples.
#>

$ErrorActionPreference = "Stop"
$TestResults = @()

function Test-Component {
    param(
        [string]$Name,
        [scriptblock]$Test
    )
    
    Write-Host "`n🔍 Testing: $Name" -ForegroundColor Cyan
    try {
        $result = & $Test
        if ($result) {
            Write-Host "   ✅ PASS" -ForegroundColor Green
            $TestResults += @{ Name = $Name; Status = "PASS" }
            return $true
        } else {
            Write-Host "   ❌ FAIL" -ForegroundColor Red
            $TestResults += @{ Name = $Name; Status = "FAIL" }
            return $false
        }
    } catch {
        Write-Host "   ❌ ERROR: $_" -ForegroundColor Red
        $TestResults += @{ Name = $Name; Status = "ERROR"; Error = $_.Exception.Message }
        return $false
    }
}

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RawrXD Toolchain - Comprehensive System Test          ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Test 1: Installation Directory
Test-Component "Installation Directory (C:\RawrXD)" {
    Test-Path "C:\RawrXD"
}

# Test 2: Build Script
Test-Component "Build Script (Build-And-Wire.ps1)" {
    Test-Path "D:\RawrXD-Compilers\Build-And-Wire.ps1"
}

# Test 3: PE Generator Executable
Test-Component "PE Generator Executable" {
    $exe = "C:\RawrXD\bin\pe_generator.exe"
    if (!(Test-Path $exe)) { return $false }
    $size = (Get-Item $exe).Length
    $size -gt 0 -and $size -lt 10KB
}

# Test 4: Encoder Test Executable
Test-Component "Encoder Test Executable" {
    Test-Path "C:\RawrXD\bin\instruction_encoder_test.exe"
}

# Test 5: Production Libraries
Test-Component "Production Libraries (2 required)" {
    $libs = @("rawrxd_encoder.lib", "rawrxd_pe_gen.lib")
    $found = 0
    foreach ($lib in $libs) {
        if (Test-Path "C:\RawrXD\Libraries\$lib") { $found++ }
    }
    $found -eq 2
}

# Test 6: API Headers
Test-Component "API Headers (4 required)" {
    $headers = Get-ChildItem "C:\RawrXD\Headers\*.h" -ErrorAction SilentlyContinue
    $headers.Count -ge 4
}

# Test 7: Documentation
Test-Component "Documentation (8+ files)" {
    $docs = Get-ChildItem "C:\RawrXD\Docs\*.md" -ErrorAction SilentlyContinue
    $docs.Count -ge 8
}

# Test 8: CLI Tool
Test-Component "RawrXD CLI Tool" {
    Test-Path "C:\RawrXD\RawrXD-CLI.ps1"
}

# Test 9: CLI Commands Work
Test-Component "CLI 'info' Command" {
    $output = & "C:\RawrXD\RawrXD-CLI.ps1" info 2>&1
    $LASTEXITCODE -eq 0
}

# Test 10: PATH Installers
Test-Component "PATH Installation Scripts" {
    (Test-Path "C:\RawrXD\Install-PATH.ps1") -and (Test-Path "C:\RawrXD\Install-PATH.bat")
}

# Test 11: Integration Examples
Test-Component "Integration Examples" {
    (Test-Path "C:\RawrXD\Examples\advanced_integration_example.cpp") -and
    (Test-Path "C:\RawrXD\Examples\CMakeLists.txt") -and
    (Test-Path "C:\RawrXD\Examples\README.md")
}

# Test 12: PE Generator Functionality
Test-Component "PE Generator Functionality" {
    Push-Location "C:\RawrXD"
    try {
        # Clean up any existing output
        if (Test-Path "output.exe") { Remove-Item "output.exe" -Force }
        
        # Run generator
        & ".\bin\pe_generator.exe" | Out-Null
        
        # Check output
        if (Test-Path "output.exe") {
            $size = (Get-Item "output.exe").Length
            Remove-Item "output.exe" -Force
            return $size -eq 1024
        }
        return $false
    } finally {
        Pop-Location
    }
}

# Test 13: Object Files Exist
Test-Component "Compiled Object Files (5+ required)" {
    $objs = Get-ChildItem "D:\RawrXD-Compilers\obj\*.obj" -ErrorAction SilentlyContinue
    $objs.Count -ge 5
}

# Test 14: Documentation Index
Test-Component "Master Documentation Index" {
    Test-Path "C:\RawrXD\INDEX.md"
}

# Test 15: CLI Reference Documentation
Test-Component "CLI Reference Documentation" {
    $doc = "C:\RawrXD\Docs\CLI_REFERENCE.md"
    if (!(Test-Path $doc)) { return $false }
    $size = (Get-Item $doc).Length
    $size -gt 10KB
}

# Summary Report
Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                     TEST SUMMARY                           ║" -ForegroundColor Yellow
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

$passCount = ($TestResults | Where-Object { $_.Status -eq "PASS" }).Count
$failCount = ($TestResults | Where-Object { $_.Status -eq "FAIL" }).Count
$errorCount = ($TestResults | Where-Object { $_.Status -eq "ERROR" }).Count
$totalCount = $TestResults.Count

Write-Host "`nResults:" -ForegroundColor Cyan
Write-Host "  ✅ Passed:  $passCount" -ForegroundColor Green
if ($failCount -gt 0) {
    Write-Host "  ❌ Failed:  $failCount" -ForegroundColor Red
}
if ($errorCount -gt 0) {
    Write-Host "  ⚠️  Errors:  $errorCount" -ForegroundColor Yellow
}
Write-Host "  📊 Total:   $totalCount" -ForegroundColor White

$successRate = [math]::Round(($passCount / $totalCount) * 100, 1)
Write-Host "`nSuccess Rate: $successRate%" -ForegroundColor $(if ($successRate -eq 100) { "Green" } else { "Yellow" })

# Failed Tests Detail
if ($failCount -gt 0 -or $errorCount -gt 0) {
    Write-Host "`n⚠️  Failed/Error Tests:" -ForegroundColor Yellow
    foreach ($result in $TestResults) {
        if ($result.Status -ne "PASS") {
            Write-Host "   • $($result.Name): $($result.Status)" -ForegroundColor Red
            if ($result.Error) {
                Write-Host "     Error: $($result.Error)" -ForegroundColor Gray
            }
        }
    }
}

# Final Status
Write-Host ""
if ($successRate -eq 100) {
    Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║  ✅ ALL TESTS PASSED - SYSTEM FULLY OPERATIONAL ✅         ║" -ForegroundColor Green -BackgroundColor DarkGreen
    Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    exit 0
} else {
    Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
    Write-Host "║  ⚠️  SOME TESTS FAILED - CHECK DETAILS ABOVE ⚠️            ║" -ForegroundColor Yellow
    Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Yellow
    exit 1
}
