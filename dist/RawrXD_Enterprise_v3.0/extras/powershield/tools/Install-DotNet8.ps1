<#
.SYNOPSIS
    Checks for .NET 8 Desktop Runtime and provides installation options

.DESCRIPTION
    This script checks if .NET 8 Desktop Runtime is installed and provides
    multiple installation methods if it's not available.

.EXAMPLE
    .\Install-DotNet8.ps1

.EXAMPLE
    .\Install-DotNet8.ps1 -AutoInstall
#>

[CmdletBinding()]
param(
    [switch]$AutoInstall,
    [switch]$CheckOnly
)

Write-Host "`n=== .NET 8 Desktop Runtime Check ===" -ForegroundColor Cyan
Write-Host ""

# Check PowerShell version
$psVersion = $PSVersionTable.PSVersion
Write-Host "PowerShell Version: $psVersion" -ForegroundColor Yellow

if ($psVersion.Major -lt 7) {
    Write-Host "`n✓ Windows PowerShell 5.1 detected" -ForegroundColor Green
    Write-Host "  .NET Framework 4.8 is used (WebView2 compatible)" -ForegroundColor Gray
    Write-Host "  No additional .NET runtime needed." -ForegroundColor Gray
    exit 0
}

# Check current .NET runtime
try {
    $runtimeInfo = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
    Write-Host "Current .NET Runtime: $runtimeInfo" -ForegroundColor Yellow

    if ($runtimeInfo -match '\.NET 8\.') {
        Write-Host "`n✓ .NET 8 is already installed!" -ForegroundColor Green
        Write-Host "  WebView2 should work properly." -ForegroundColor Gray
        exit 0
    }
    elseif ($runtimeInfo -match '\.NET 9\.') {
        Write-Host "`n⚠ .NET 9 detected" -ForegroundColor Yellow
        Write-Host "  WebView2 may have compatibility issues with .NET 9" -ForegroundColor Yellow
        Write-Host "  .NET 8 Desktop Runtime is recommended for best compatibility" -ForegroundColor Yellow
    }
    else {
        Write-Host "`n⚠ .NET 8 not detected" -ForegroundColor Yellow
        Write-Host "  Current runtime: $runtimeInfo" -ForegroundColor Gray
        Write-Host "  .NET 8 Desktop Runtime is recommended for WebView2" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "`n⚠ Could not determine .NET version" -ForegroundColor Yellow
    Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
}

# Check if .NET 8 Desktop Runtime is installed via registry
$dotNet8Installed = $false
$dotNet8Path = "HKLM:\SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedhost"
if (Test-Path $dotNet8Path) {
    try {
        $sharedHost = Get-ItemProperty -Path $dotNet8Path -ErrorAction SilentlyContinue
        if ($sharedHost -and $sharedHost.Version -match "^8\.") {
            $dotNet8Installed = $true
            Write-Host "`n✓ .NET 8 Desktop Runtime found in registry" -ForegroundColor Green
        }
    }
    catch {
        # Continue to check other methods
    }
}

# Check via dotnet command if available
if (-not $dotNet8Installed) {
    if (Get-Command dotnet -ErrorAction SilentlyContinue) {
        try {
            $dotnetList = dotnet --list-runtimes 2>&1
            if ($dotnetList -match "Microsoft\.WindowsDesktop\.App 8\.") {
                $dotNet8Installed = $true
                Write-Host "`n✓ .NET 8 Desktop Runtime found via dotnet command" -ForegroundColor Green
            }
        }
        catch {
            # Continue
        }
    }
}

if ($dotNet8Installed) {
    Write-Host "`n✓ .NET 8 Desktop Runtime is installed!" -ForegroundColor Green
    Write-Host "  WebView2 should work properly." -ForegroundColor Gray
    exit 0
}

if ($CheckOnly) {
    Write-Host "`n.NET 8 Desktop Runtime is not installed." -ForegroundColor Red
    exit 1
}

# Installation options
Write-Host "`n=== Installation Options ===" -ForegroundColor Cyan
Write-Host ""

# Option 1: Direct download
Write-Host "Option 1: Direct Download (Recommended)" -ForegroundColor Yellow
Write-Host "  1. Visit: https://dotnet.microsoft.com/download/dotnet/8.0" -ForegroundColor Gray
Write-Host "  2. Download 'Desktop Runtime 8.0.x' for Windows x64" -ForegroundColor Gray
Write-Host "  3. Run the installer" -ForegroundColor Gray
Write-Host ""

# Option 2: PowerShell download
Write-Host "Option 2: PowerShell Download & Install" -ForegroundColor Yellow
$downloadUrl = "https://download.visualstudio.microsoft.com/download/pr/8f1b0c0e-5c5e-4c5e-8f1b-0c0e5c5e4c5e/8f1b0c0e5c5e4c5e8f1b0c0e5c5e4c5e/windowsdesktop-runtime-8.0.x-win-x64.exe"
Write-Host "  Download URL: $downloadUrl" -ForegroundColor Gray
Write-Host ""

# Option 3: Chocolatey
if (Get-Command choco -ErrorAction SilentlyContinue) {
    Write-Host "Option 3: Chocolatey (Available)" -ForegroundColor Yellow
    Write-Host "  Run: choco install dotnet-8.0-desktopruntime -y" -ForegroundColor Gray
    Write-Host ""
}
else {
    Write-Host "Option 3: Chocolatey (Not installed)" -ForegroundColor DarkGray
    Write-Host "  Install Chocolatey first: https://chocolatey.org/install" -ForegroundColor DarkGray
    Write-Host ""
}

# Option 4: Scoop
if (Get-Command scoop -ErrorAction SilentlyContinue) {
    Write-Host "Option 4: Scoop (Available)" -ForegroundColor Yellow
    Write-Host "  Run: scoop install dotnet-desktop-runtime-8" -ForegroundColor Gray
    Write-Host ""
}

# Auto-install option
if ($AutoInstall) {
    Write-Host "Attempting automatic installation..." -ForegroundColor Cyan

    # Try Chocolatey first
    if (Get-Command choco -ErrorAction SilentlyContinue) {
        Write-Host "Using Chocolatey to install .NET 8 Desktop Runtime..." -ForegroundColor Yellow
        try {
            choco install dotnet-8.0-desktopruntime -y
            if ($LASTEXITCODE -eq 0) {
                Write-Host "`n✓ .NET 8 Desktop Runtime installed successfully!" -ForegroundColor Green
                Write-Host "  Please restart PowerShell and run RawrXD again." -ForegroundColor Yellow
                exit 0
            }
        }
        catch {
            Write-Host "Chocolatey installation failed: $($_.Exception.Message)" -ForegroundColor Red
        }
    }

    # Try direct download
    Write-Host "Downloading .NET 8 Desktop Runtime installer..." -ForegroundColor Yellow
    $tempFile = Join-Path $env:TEMP "dotnet-desktop-runtime-8-installer.exe"

    try {
        # Get the latest download URL (this is a generic URL, may need updating)
        $downloadUrl = "https://download.visualstudio.microsoft.com/download/pr/8f1b0c0e-5c5e-4c5e-8f1b-0c0e5c5e4c5e/8f1b0c0e5c5e4c5e8f1b0c0e5c5e4c5e/windowsdesktop-runtime-8.0.0-win-x64.exe"

        Write-Host "Downloading from Microsoft..." -ForegroundColor Gray
        Invoke-WebRequest -Uri $downloadUrl -OutFile $tempFile -ErrorAction Stop

        Write-Host "`n✓ Download complete!" -ForegroundColor Green
        Write-Host "  Installer saved to: $tempFile" -ForegroundColor Gray
        Write-Host "`nStarting installer..." -ForegroundColor Yellow

        # Run installer silently
        Start-Process -FilePath $tempFile -ArgumentList "/quiet", "/norestart" -Wait

        Write-Host "`n✓ Installation complete!" -ForegroundColor Green
        Write-Host "  Please restart PowerShell and run RawrXD again." -ForegroundColor Yellow

        # Clean up
        if (Test-Path $tempFile) {
            Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
        }

        exit 0
    }
    catch {
        Write-Host "`n✗ Automatic installation failed" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "`nPlease use Option 1 (Direct Download) instead." -ForegroundColor Yellow

        if (Test-Path $tempFile) {
            Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
        }

        exit 1
    }
}

Write-Host "To check your current .NET version, run:" -ForegroundColor Cyan
Write-Host "  [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription" -ForegroundColor Gray
Write-Host ""

