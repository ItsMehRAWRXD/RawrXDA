#Requires -Version 5.1
<#
.SYNOPSIS
    Build RawrXD Agentic EXE from PowerShell script
    
.DESCRIPTION
    Converts Launch-RawrXD-Agentic.ps1 into a standalone executable using PS2EXE-GUI.
    Creates a distribution-ready EXE that can be run without PowerShell knowledge.
    
.PARAMETER Install
    Install PS2EXE module if not already installed
    
.PARAMETER Architecture
    Target architecture: 'x64' (default) or 'x86'
    
.PARAMETER OutputPath
    Where to save the EXE (default: current directory)
    
.EXAMPLE
    .\Build-RawrXD-Agentic-EXE.ps1 -Install
    # Downloads PS2EXE and builds the EXE
    
.EXAMPLE
    .\Build-RawrXD-Agentic-EXE.ps1 -OutputPath "C:\Program Files\RawrXD\"
    # Builds EXE in Program Files
#>

param(
    [switch]$Install,
    [ValidateSet('x64', 'x86')]
    [string]$Architecture = 'x64',
    [string]$OutputPath = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

function Write-Status {
    param([string]$Message, [string]$Status = 'INFO')
    
    $color = switch ($Status) {
        'SUCCESS' { 'Green' }
        'ERROR' { 'Red' }
        'WARNING' { 'Yellow' }
        'INFO' { 'Cyan' }
        default { 'White' }
    }
    
    $symbol = switch ($Status) {
        'SUCCESS' { '✅' }
        'ERROR' { '❌' }
        'WARNING' { '⚠️' }
        'INFO' { 'ℹ️' }
        default { '➜' }
    }
    
    Write-Host "$symbol $Message" -ForegroundColor $color
}

Write-Host "`n╔═══════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║     Building RawrXD Agentic EXE                      ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

# Check PS2EXE installation
Write-Status "Checking PS2EXE..." -Status 'INFO'

if ($Install) {
    Write-Status "Installing PS2EXE module..." -Status 'INFO'
    try {
        # Install from PSGallery
        $null = Find-Module ps2exe -ErrorAction Stop
        Install-Module ps2exe -Scope CurrentUser -Force -ErrorAction Stop
        Write-Status "PS2EXE installed successfully" -Status 'SUCCESS'
    } catch {
        # Fallback: Install ps2exe.ps1 manually
        Write-Status "Installing ps2exe.ps1 manually..." -Status 'WARNING'
        $ps2exePath = "$env:TEMP\ps2exe.ps1"
        
        try {
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile("https://github.com/MScholtes/PS2EXE/raw/master/ps2exe.ps1", $ps2exePath)
            Write-Status "ps2exe.ps1 downloaded" -Status 'SUCCESS'
            
            # Source the script
            . $ps2exePath
        } catch {
            Write-Status "Failed to download ps2exe: $($_.Exception.Message)" -Status 'ERROR'
            Write-Status "Alternative: Install from PSGallery manually:" -Status 'WARNING'
            Write-Host "  Install-Module ps2exe -Scope CurrentUser" -ForegroundColor Yellow
            exit 1
        }
    }
} else {
    # Check if PS2EXE is available
    try {
        $ps2exeCmd = Get-Command Invoke-PS2EXE -ErrorAction Stop
        Write-Status "PS2EXE found: $($ps2exeCmd.Source)" -Status 'SUCCESS'
    } catch {
        Write-Status "PS2EXE not found. Use -Install flag to install it first." -Status 'ERROR'
        Write-Host "`nTo install: .\Build-RawrXD-Agentic-EXE.ps1 -Install`n" -ForegroundColor Yellow
        exit 1
    }
}

# Define paths
$scriptPath = Join-Path $PSScriptRoot 'Launch-RawrXD-Agentic.ps1'
$exeName = "RawrXD-Agentic.exe"
$exePath = Join-Path $OutputPath $exeName

# Verify source script exists
Write-Status "Checking source script..." -Status 'INFO'
if (-not (Test-Path $scriptPath)) {
    Write-Status "Source script not found: $scriptPath" -Status 'ERROR'
    exit 1
}
Write-Status "Source script found" -Status 'SUCCESS'

# Create output directory if needed
if (-not (Test-Path $OutputPath)) {
    Write-Status "Creating output directory..." -Status 'INFO'
    $null = New-Item -ItemType Directory -Path $OutputPath -Force
    Write-Status "Directory created: $OutputPath" -Status 'SUCCESS'
}

# Build the EXE
Write-Host "`n" -ForegroundColor White
Write-Status "Building EXE ($Architecture architecture)..." -Status 'INFO'
Write-Host "  Source: $scriptPath" -ForegroundColor Gray
Write-Host "  Output: $exePath" -ForegroundColor Gray
Write-Host "`n"

try {
    # Use PS2EXE to convert
    if (Get-Command Invoke-PS2EXE -ErrorAction SilentlyContinue) {
        Invoke-PS2EXE -InputFile $scriptPath `
                      -OutputFile $exePath `
                      -Verbose
    } else {
        # Fallback method using ps2exe.ps1
        & "$env:TEMP\ps2exe.ps1" -InputFile $scriptPath `
                                 -OutputFile $exePath
    }
    
    # Verify EXE was created
    if (Test-Path $exePath) {
        $fileSize = (Get-Item $exePath).Length / 1MB
        Write-Status "EXE built successfully! ($([math]::Round($fileSize, 2)) MB)" -Status 'SUCCESS'
    } else {
        throw "EXE file was not created"
    }
    
} catch {
    Write-Status "Build failed: $($_.Exception.Message)" -Status 'ERROR'
    exit 1
}

# Create batch shortcut
Write-Host "`n" -ForegroundColor White
Write-Status "Creating batch shortcut..." -Status 'INFO'

$batchPath = Join-Path $OutputPath "RawrXD-Agentic.bat"
$batchContent = "@echo off
cd /d `"%~dp0`"
powershell -NoProfile -ExecutionPolicy Bypass -File `"Launch-RawrXD-Agentic.ps1`" -Terminal
pause"

$batchContent | Out-File -FilePath $batchPath -Encoding ASCII -Force
Write-Status "Batch file created: $batchPath" -Status 'SUCCESS'

# Summary
Write-Host "`n╔═══════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║               ✨ BUILD SUCCESSFUL ✨                 ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "📦 DELIVERABLES:" -ForegroundColor Cyan
Write-Host "  EXE:   $exePath" -ForegroundColor Yellow
Write-Host "  BAT:   $batchPath" -ForegroundColor Yellow

Write-Host "`n🚀 USAGE:" -ForegroundColor Cyan
Write-Host "  Double-click: $exeName" -ForegroundColor Yellow
Write-Host "  Or run:       $batchPath" -ForegroundColor Yellow

Write-Host "`n📋 DISTRIBUTION:" -ForegroundColor Cyan
Write-Host "  Include these files in same directory:" -ForegroundColor Gray
Write-Host "    • RawrXD-Agentic.exe" -ForegroundColor Cyan
Write-Host "    • RawrXD-Agentic-Module.psm1" -ForegroundColor Cyan
Write-Host "    • Launch-RawrXD-Agentic.ps1" -ForegroundColor Cyan
Write-Host "    • RawrXD-Agentic.bat (optional)" -ForegroundColor Cyan

Write-Host "`n✅ Ready to distribute!`n" -ForegroundColor Green
