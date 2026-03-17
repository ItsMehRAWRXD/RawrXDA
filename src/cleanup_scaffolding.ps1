<# 
.SYNOPSIS
    RawrXD Production Cleanup Script
    Identifies scaffolding, test, and artifact files for cleanup
    
.DESCRIPTION
    This script analyzes the RawrXD source tree and categorizes files as:
    - PRODUCTION: Keep these (the 3 monolithic files)
    - SCAFFOLDING: Test/artifact/duplicate files (safe to archive)
    
.NOTES
    Run with -WhatIf to preview, or -Execute to actually move files
#>

param(
    [switch]$Execute,
    [switch]$ListAll
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host ""
Write-Host "============================================"
Write-Host "   RawrXD Production Cleanup Analyzer"
Write-Host "============================================"
Write-Host ""

# Production files - DO NOT TOUCH
$ProductionFiles = @(
    "RawrXD_Sovereign_Monolith.asm",
    "pe_writer_production\RawrXD_Monolithic_PE_Emitter.asm",
    "asm\RawrCodex.asm",
    "PRODUCTION_MANIFEST.md",
    "build_production.bat",
    "cleanup_scaffolding.ps1"
)

# Get all ASM files
$AllAsmFiles = Get-ChildItem -Path $ScriptDir -Filter "*.asm" -Recurse -File

# Categorize files
$Production = @()
$Scaffolding = @()

foreach ($file in $AllAsmFiles) {
    $relativePath = $file.FullName.Replace("$ScriptDir\", "")
    
    $isProduction = $false
    foreach ($prodFile in $ProductionFiles) {
        if ($relativePath -eq $prodFile) {
            $isProduction = $true
            break
        }
    }
    
    if ($isProduction) {
        $Production += $file
    } else {
        $Scaffolding += $file
    }
}

Write-Host "PRODUCTION FILES ($($Production.Count)):"
Write-Host "========================================="
foreach ($f in $Production) {
    $size = [math]::Round($f.Length / 1KB, 1)
    Write-Host "  [KEEP] $($f.Name) ($size KB)"
}

Write-Host ""
Write-Host "SCAFFOLDING FILES ($($Scaffolding.Count)):"
Write-Host "==========================================="

if ($ListAll) {
    foreach ($f in $Scaffolding) {
        $size = [math]::Round($f.Length / 1KB, 1)
        Write-Host "  [SCAFFOLD] $($f.Name) ($size KB)"
    }
} else {
    Write-Host "  (showing first 20, use -ListAll to see all)"
    $Scaffolding | Select-Object -First 20 | ForEach-Object {
        $size = [math]::Round($_.Length / 1KB, 1)
        Write-Host "  [SCAFFOLD] $($_.Name) ($size KB)"
    }
    if ($Scaffolding.Count -gt 20) {
        Write-Host "  ... and $($Scaffolding.Count - 20) more"
    }
}

# Calculate totals
$ProdSize = ($Production | Measure-Object -Property Length -Sum).Sum / 1MB
$ScaffSize = ($Scaffolding | Measure-Object -Property Length -Sum).Sum / 1MB

Write-Host ""
Write-Host "SUMMARY:"
Write-Host "========"
Write-Host "  Production:  $($Production.Count) files, $([math]::Round($ProdSize, 2)) MB"
Write-Host "  Scaffolding: $($Scaffolding.Count) files, $([math]::Round($ScaffSize, 2)) MB"
Write-Host ""

if ($Execute) {
    Write-Host "EXECUTING CLEANUP..."
    
    # Create archive directory
    $ArchiveDir = Join-Path $ScriptDir "archive_scaffolding"
    if (-not (Test-Path $ArchiveDir)) {
        New-Item -ItemType Directory -Path $ArchiveDir | Out-Null
    }
    
    foreach ($f in $Scaffolding) {
        $dest = Join-Path $ArchiveDir $f.Name
        $counter = 1
        while (Test-Path $dest) {
            $dest = Join-Path $ArchiveDir "$($f.BaseName)_$counter$($f.Extension)"
            $counter++
        }
        Move-Item -Path $f.FullName -Destination $dest
        Write-Host "  Moved: $($f.Name)"
    }
    
    Write-Host ""
    Write-Host "Cleanup complete. Files moved to: $ArchiveDir"
} else {
    Write-Host "To archive scaffolding files, run: .\cleanup_scaffolding.ps1 -Execute"
}

Write-Host ""
