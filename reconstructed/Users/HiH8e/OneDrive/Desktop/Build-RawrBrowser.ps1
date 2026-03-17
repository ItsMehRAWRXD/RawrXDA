#Requires -Version 5.1
<#
.SYNOPSIS
    Build script to compile RawrBrowser.ps1 into an executable
.DESCRIPTION
    This script uses ps2exe to convert the PowerShell script into a Windows executable
    with proper error handling and logging capabilities.
.NOTES
    Requires: ps2exe module (Install-Module ps2exe -Force)
#>

# Set execution policy for this session
Set-ExecutionPolicy Bypass -Scope Process -Force

# Color output functions
function Write-BuildLog {
  param(
    [string]$Message,
    [string]$Level = "INFO"
  )
    
  $timestamp = Get-Date -Format "HH:mm:ss"
  $color = switch ($Level) {
    "ERROR" { "Red" }
    "WARNING" { "Yellow" }
    "SUCCESS" { "Green" }
    "INFO" { "Cyan" }
    default { "White" }
  }
  Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $color
}

Write-BuildLog "═══════════════════════════════════════════════" "INFO"
Write-BuildLog "RawrBrowser Build Script Started" "INFO"
Write-BuildLog "═══════════════════════════════════════════════" "INFO"

# Check if ps2exe is installed
Write-BuildLog "Checking for ps2exe module..." "INFO"
if (-not (Get-Module -ListAvailable ps2exe)) {
  Write-BuildLog "ps2exe module not found. Installing..." "WARNING"
  try {
    Install-Module ps2exe -Force -AllowClobber -Scope CurrentUser
    Write-BuildLog "ps2exe module installed successfully" "SUCCESS"
  }
  catch {
    Write-BuildLog "Failed to install ps2exe module: $_" "ERROR"
    Write-BuildLog "Please run: Install-Module ps2exe -Force" "ERROR"
    exit 1
  }
}
else {
  Write-BuildLog "ps2exe module found" "SUCCESS"
}

# Import ps2exe module
try {
  Import-Module ps2exe -Force
  Write-BuildLog "ps2exe module imported" "SUCCESS"
}
catch {
  Write-BuildLog "Failed to import ps2exe module: $_" "ERROR"
  exit 1
}

# Define paths
$scriptPath = Join-Path $PSScriptRoot "RawrBrowser.ps1"
$outputPath = Join-Path $PSScriptRoot "RawrBrowser.exe"
$iconPath = Join-Path $PSScriptRoot "rawr-icon.ico"

# Check if source script exists
if (-not (Test-Path $scriptPath)) {
  Write-BuildLog "Source script not found: $scriptPath" "ERROR"
  exit 1
}

Write-BuildLog "Source script found: $scriptPath" "SUCCESS"

# Create a simple icon if none exists
if (-not (Test-Path $iconPath)) {
  Write-BuildLog "No icon file found, will use default" "WARNING"
  $iconPath = $null
}

# Build parameters
$buildParams = @{
  inputFile        = $scriptPath
  outputFile       = $outputPath
  noConsole        = $true
  noError          = $false
  noOutput         = $false
  runtime40        = $false
  runtime20        = $false
  x64              = $true
  x86              = $false
  DPIAware         = $true
  winFormsDPIAware = $true
  requireAdmin     = $false
  supportOS        = $true
  longPaths        = $true
  title            = "RawrBrowser"
  description      = "Advanced PowerShell Browser with WebView2 Support"
  company          = "RawrXD Development"
  product          = "RawrBrowser"
  copyright        = "(c) 2025 RawrXD"
  version          = "2.0.0.0"
  verbose          = $true
}

# Add icon if available
if ($iconPath) {
  $buildParams.iconFile = $iconPath
  Write-BuildLog "Using icon: $iconPath" "INFO"
}

Write-BuildLog "Starting compilation with ps2exe..." "INFO"
Write-BuildLog "Output file: $outputPath" "INFO"

try {
  # Remove existing exe if present
  if (Test-Path $outputPath) {
    Remove-Item $outputPath -Force
    Write-BuildLog "Removed existing executable" "INFO"
  }
    
  # Compile the script
  ps2exe @buildParams
    
  # Check if compilation was successful
  if (Test-Path $outputPath) {
    $fileInfo = Get-Item $outputPath
    $fileSizeMB = [Math]::Round($fileInfo.Length / 1MB, 2)
        
    Write-BuildLog "═══════════════════════════════════════════════" "SUCCESS"
    Write-BuildLog "BUILD SUCCESSFUL!" "SUCCESS"
    Write-BuildLog "═══════════════════════════════════════════════" "SUCCESS"
    Write-BuildLog "Executable created: $outputPath" "SUCCESS"
    Write-BuildLog "File size: ${fileSizeMB} MB" "INFO"
    Write-BuildLog "Build completed at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
        
    # Optional: Test the executable
    $testExe = Read-Host "Would you like to test the executable? (y/N)"
    if ($testExe -eq "y" -or $testExe -eq "Y") {
      Write-BuildLog "Launching executable for testing..." "INFO"
      Start-Process $outputPath
    }
        
    # Show log location
    $logPath = Join-Path $env:TEMP "RawrBrowser_Startup.log"
    Write-BuildLog "Startup logs will be written to: $logPath" "INFO"
  }
  else {
    Write-BuildLog "Build failed - executable not created" "ERROR"
    exit 1
  }
}
catch {
  Write-BuildLog "Build failed with error: $_" "ERROR"
  Write-BuildLog "Check ps2exe documentation for troubleshooting" "INFO"
  exit 1
}

Write-BuildLog "Build script completed" "INFO"