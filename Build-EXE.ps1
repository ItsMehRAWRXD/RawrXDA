# Build Script to Convert PowerShell Script to EXE
# This script will install ps2exe module if needed and compile RawrXD.ps1 to RawrXD.exe

Write-Host "=== Building RawrXD Desktop Application ===" -ForegroundColor Cyan

# Check if ps2exe module is installed
$ps2exeInstalled = Get-Module -ListAvailable -Name ps2exe

if (-not $ps2exeInstalled) {
    Write-Host "Installing ps2exe module..." -ForegroundColor Yellow
    try {
        Install-Module -Name ps2exe -Scope CurrentUser -Force -AllowClobber
        Write-Host "ps2exe module installed successfully!" -ForegroundColor Green
    } catch {
        Write-Host "Error installing ps2exe: $_" -ForegroundColor Red
        Write-Host "Please run PowerShell as Administrator and try again." -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "ps2exe module found!" -ForegroundColor Green
}

# Import the module
Import-Module ps2exe -Force

# Get script directory
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceScript = Join-Path $scriptDir "RawrXD.ps1"
$outputExe = Join-Path $scriptDir "RawrXD.exe"

# Check if source script exists
if (-not (Test-Path $sourceScript)) {
    Write-Host "Error: RawrXD.ps1 not found at $sourceScript" -ForegroundColor Red
    exit 1
}

Write-Host "`nCompiling $sourceScript to $outputExe..." -ForegroundColor Yellow

# Compile to EXE with options
try {
    Invoke-ps2exe -inputFile $sourceScript -outputFile $outputExe `
        -title "AI Text Editor - 3 Pane" `
        -description "AI-Powered Text Editor with 3-Pane Layout" `
        -company "Powershield" `
        -product "RawrXD" `
        -version "1.0.0.0" `
        -iconFile $null `
        -noConsole `
        -requireAdmin $false `
        -supportOS $true `
        -longPaths $true

    if (Test-Path $outputExe) {
        Write-Host "`n✓ Successfully created: $outputExe" -ForegroundColor Green
        Write-Host "`nYou can now run RawrXD.exe as a standalone application!" -ForegroundColor Cyan
        
        # Ask if user wants to launch the EXE
        $response = Read-Host "`nLaunch RawrXD.exe now? (Y/N)"
        if ($response -eq "Y" -or $response -eq "y") {
            Write-Host "Launching RawrXD.exe..." -ForegroundColor Yellow
            Start-Process $outputExe
        }
    } else {
        Write-Host "`n✗ Build failed - EXE file was not created." -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "`n✗ Error during compilation: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== Build Complete ===" -ForegroundColor Cyan

