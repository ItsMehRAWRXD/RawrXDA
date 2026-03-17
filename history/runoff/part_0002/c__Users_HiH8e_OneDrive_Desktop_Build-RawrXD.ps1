#Requires -Version 5.1
<#
.SYNOPSIS
    Build script to compile RawrXD.ps1 into an executable
.DESCRIPTION
    This script uses ps2exe to convert the PowerShell script into a Windows executable
    with enhanced error handling, startup logging, and file browser improvements.
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
    
  $timestamp = Get-Date -Format "HH:mm:ss.fff"
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
Write-BuildLog "RawrXD Build Script Started" "INFO"
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
$scriptPath = Join-Path $PSScriptRoot "Powershield\RawrXD.ps1"
$outputDir = Join-Path $PSScriptRoot "Build"
$outputPath = Join-Path $outputDir "RawrXD.exe"
$iconPath = Join-Path $PSScriptRoot "rawrxd-icon.ico"

# Create build directory
if (-not (Test-Path $outputDir)) {
  New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
  Write-BuildLog "Created build directory: $outputDir" "INFO"
}

# Check if source script exists
if (-not (Test-Path $scriptPath)) {
  Write-BuildLog "Source script not found: $scriptPath" "ERROR"
  Write-BuildLog "Expected location: .\Powershield\RawrXD.ps1" "ERROR"
  exit 1
}

Write-BuildLog "Source script found: $scriptPath" "SUCCESS"

# Check script size and complexity
$scriptInfo = Get-Item $scriptPath
$scriptLines = (Get-Content $scriptPath | Measure-Object).Count
$scriptSizeMB = [Math]::Round($scriptInfo.Length / 1MB, 2)

Write-BuildLog "Script size: ${scriptSizeMB} MB ($scriptLines lines)" "INFO"

# Create a simple icon if none exists
if (-not (Test-Path $iconPath)) {
  Write-BuildLog "No icon file found, will use default" "WARNING"
  $iconPath = $null
}

# Build parameters for RawrXD
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
  title            = "RawrXD AI Editor"
  description      = "AI-Powered Text Editor with Ollama Integration, Enhanced File Browser, and Agent Automation"
  company          = "PowerShield Development"
  product          = "RawrXD"
  copyright        = "(c) 2025 PowerShield Team"
  version          = "2.1.0.0"
  verbose          = $true
}

# Add icon if available
if ($iconPath) {
  $buildParams.iconFile = $iconPath
  Write-BuildLog "Using icon: $iconPath" "INFO"
}

Write-BuildLog "Starting compilation with ps2exe..." "INFO"
Write-BuildLog "Target: $outputPath" "INFO"
Write-BuildLog "Features: Enhanced file browser, startup logging, agent automation" "INFO"

try {
  # Remove existing exe if present
  if (Test-Path $outputPath) {
    Remove-Item $outputPath -Force
    Write-BuildLog "Removed existing executable" "INFO"
  }
    
  Write-BuildLog "Compiling PowerShell script to executable..." "INFO"
    
  # Simple ps2exe command
  ps2exe -inputFile $scriptPath -outputFile $outputPath -noConsole -x64 -DPIAware -winFormsDPIAware -supportOS -longPaths -title "RawrXD AI Editor" -description "AI-Powered Text Editor with Enhanced File Browser" -version "2.1.0.0" -company "PowerShield Development" -product "RawrXD" -copyright "(c) 2025 PowerShield Team"  # Check if compilation was successful
  if (Test-Path $outputPath) {
    $fileInfo = Get-Item $outputPath
    $fileSizeMB = [Math]::Round($fileInfo.Length / 1MB, 2)
        
    Write-BuildLog "═══════════════════════════════════════════════" "SUCCESS"
    Write-BuildLog "BUILD SUCCESSFUL!" "SUCCESS"
    Write-BuildLog "═══════════════════════════════════════════════" "SUCCESS"
    Write-BuildLog "Executable: $outputPath" "SUCCESS"
    Write-BuildLog "File size: ${fileSizeMB} MB" "INFO"
    Write-BuildLog "Build time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
        
    # Show feature summary
    Write-BuildLog "" "INFO"
    Write-BuildLog "🎯 RawrXD Features Built:" "INFO"
    Write-BuildLog "  • Enhanced file browser (500+ items per folder)" "INFO"
    Write-BuildLog "  • Startup logging to %APPDATA%\RawrXD\startup.log" "INFO"
    Write-BuildLog "  • Runtime logging in Dev Tools tab" "INFO"
    Write-BuildLog "  • AI chat integration with Ollama" "INFO"
    Write-BuildLog "  • Agent automation with 15+ tools" "INFO"
    Write-BuildLog "  • WebView2 browser integration" "INFO"
    Write-BuildLog "  • Extension marketplace system" "INFO"
    Write-BuildLog "  • Git integration and terminal" "INFO"
    Write-BuildLog "" "INFO"
        
    # Show log locations
    $startupLogPath = Join-Path $env:APPDATA "RawrXD\startup.log"
    Write-BuildLog "📋 Log Files:" "INFO"
    Write-BuildLog "  • Startup Log: $startupLogPath" "INFO"
    Write-BuildLog "  • Runtime Log: Available in Dev Tools tab" "INFO"
    Write-BuildLog "" "INFO"
        
    # Optional: Test the executable
    $testExe = Read-Host "Would you like to test the executable? (y/N)"
    if ($testExe -eq "y" -or $testExe -eq "Y") {
      Write-BuildLog "Launching RawrXD for testing..." "INFO"
      Write-BuildLog "Check startup log for initialization details" "INFO"
      Start-Process $outputPath
    }
        
    Write-BuildLog "Build completed successfully!" "SUCCESS"
  }
  else {
    Write-BuildLog "Build failed - executable not created" "ERROR"
    Write-BuildLog "Check ps2exe output above for error details" "ERROR"
    exit 1
  }
}
catch {
  Write-BuildLog "Build failed with error: $_" "ERROR"
  Write-BuildLog "Common solutions:" "INFO"
  Write-BuildLog "  • Ensure PowerShell execution policy allows scripts" "INFO"
  Write-BuildLog "  • Check that no antivirus is blocking ps2exe" "INFO"
  Write-BuildLog "  • Verify ps2exe module is properly installed" "INFO"
  exit 1
}

Write-BuildLog "Build script completed" "INFO"
Write-BuildLog "═══════════════════════════════════════════════" "INFO"