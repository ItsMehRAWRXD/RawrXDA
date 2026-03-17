# ============================================================================
# MASTER BUILD SYSTEM - All Security Research Projects
# Compiles and builds all 9 security research projects
# ============================================================================

param(
  [switch]$All,
  [switch]$Mirai,
  [switch]$Zencoder,
  [switch]$Star,
  [switch]$RawrZDesktop,
  [switch]$OhGee,
  [switch]$RawrzHttp,
  [switch]$RawrzPlatform,
  [switch]$SaaSSecurity,
  [switch]$Star5IDE
)

$ErrorActionPreference = "Continue"
$Global:BuildResults = @()

function Write-Section {
  param([string]$Title)
  Write-Host "`n============================================" -ForegroundColor Cyan
  Write-Host " $Title" -ForegroundColor Cyan
  Write-Host "============================================`n" -ForegroundColor Cyan
}

function Add-BuildResult {
  param([string]$Project, [string]$Status, [string]$Output, [string]$ErrorMessage)
  $Global:BuildResults += [PSCustomObject]@{
    Project = $Project
    Status  = $Status
    Output  = $Output
    Error   = $ErrorMessage
    Time    = Get-Date
  }
}

function Build-Mirai {
  Write-Section "Building Mirai Windows Bot (C)"
    
  $MiraiDir = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"
  Set-Location $MiraiDir
    
  try {
    & .\Build-Mirai-Windows.ps1
    if ($LASTEXITCODE -eq 0) {
      Add-BuildResult -Project "Mirai" -Status "SUCCESS" -Output "build\windows\mirai_bot.exe"
      return $true
    }
    else {
      Add-BuildResult -Project "Mirai" -Status "FAILED" -Error "Compilation error"
      return $false
    }
  }
  catch {
    Add-BuildResult -Project "Mirai" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-Zencoder {
  Write-Section "Building Zencoder (C++ with CMake)"
    
  $ZencoderDir = "D:\Security Research aka GitHub Repos\Zencoder\ItsMehRAWRXD-Zencoder-e2a414d"
    
  if (!(Test-Path $ZencoderDir)) {
    Add-BuildResult -Project "Zencoder" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $ZencoderDir
    
  try {
    # Create build directory
    if (!(Test-Path "build")) {
      New-Item -ItemType Directory -Path "build" | Out-Null
    }
        
    Set-Location "build"
        
    # Run CMake
    Write-Host "Running CMake..." -ForegroundColor Yellow
    cmake .. 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Write-Host "Building with CMake..." -ForegroundColor Yellow
      cmake --build . --config Release 2>&1 | Out-Null
            
      if ($LASTEXITCODE -eq 0) {
        Add-BuildResult -Project "Zencoder" -Status "SUCCESS" -Output "build\Release\EncryptionPacker.exe"
        return $true
      }
    }
        
    Add-BuildResult -Project "Zencoder" -Status "FAILED" -Error "CMake build failed"
    return $false
  }
  catch {
    Add-BuildResult -Project "Zencoder" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-Star {
  Write-Section "Building Star Triple Encryptor (C++)"
    
  $StarDir = "D:\Security Research aka GitHub Repos\Star\ItsMehRAWRXD-Star-9e3ac78"
    
  if (!(Test-Path $StarDir)) {
    Add-BuildResult -Project "Star" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $StarDir
    
  try {
    if (Test-Path "build.bat") {
      Write-Host "Running build.bat..." -ForegroundColor Yellow
      cmd /c build.bat 2>&1 | Out-Null
            
      if (Test-Path "VS2022_TripleEncryptor.exe") {
        Add-BuildResult -Project "Star" -Status "SUCCESS" -Output "VS2022_TripleEncryptor.exe"
        return $true
      }
    }
        
    Add-BuildResult -Project "Star" -Status "FAILED" -Error "Build script failed"
    return $false
  }
  catch {
    Add-BuildResult -Project "Star" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-RawrZDesktop {
  Write-Section "Building RawrZDesktop (C#/.NET 9.0)"
    
  $RawrZDir = "D:\Security Research aka GitHub Repos\RawrZDesktop\ItsMehRAWRXD-RawrZDesktop-ca34aa4"
    
  if (!(Test-Path $RawrZDir)) {
    Add-BuildResult -Project "RawrZDesktop" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $RawrZDir
    
  try {
    Write-Host "Restoring NuGet packages..." -ForegroundColor Yellow
    dotnet restore 2>&1 | Out-Null
        
    Write-Host "Building project..." -ForegroundColor Yellow
    dotnet build --configuration Release 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Add-BuildResult -Project "RawrZDesktop" -Status "SUCCESS" -Output "bin\Release\net9.0\RawrZDesktop.exe"
      return $true
    }
    else {
      Add-BuildResult -Project "RawrZDesktop" -Status "FAILED" -Error "dotnet build failed"
      return $false
    }
  }
  catch {
    Add-BuildResult -Project "RawrZDesktop" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-OhGee {
  Write-Section "Building OhGee AI Assistant Hub (C#/.NET 8.0)"
    
  $OhGeeDir = "D:\Security Research aka GitHub Repos\OhGee\ItsMehRAWRXD-OhGee-86e21b2"
    
  if (!(Test-Path $OhGeeDir)) {
    Add-BuildResult -Project "OhGee" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $OhGeeDir
    
  try {
    Write-Host "Restoring NuGet packages..." -ForegroundColor Yellow
    dotnet restore 2>&1 | Out-Null
        
    Write-Host "Building project..." -ForegroundColor Yellow
    dotnet build --configuration Release 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Add-BuildResult -Project "OhGee" -Status "SUCCESS" -Output "bin\Release\net8.0-windows\KimiAppNative.exe"
      return $true
    }
    else {
      Add-BuildResult -Project "OhGee" -Status "FAILED" -Error "dotnet build failed"
      return $false
    }
  }
  catch {
    Add-BuildResult -Project "OhGee" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-RawrzHttp {
  Write-Section "Building rawrz-http-encryptor (Node.js)"
    
  $RawrzHttpDir = "D:\Security Research aka GitHub Repos\rawrz-http-encryptor\ItsMehRAWRXD-rawrz-http-encryptor-47724c6"
    
  if (!(Test-Path $RawrzHttpDir)) {
    Add-BuildResult -Project "rawrz-http-encryptor" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $RawrzHttpDir
    
  try {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    npm install 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Add-BuildResult -Project "rawrz-http-encryptor" -Status "SUCCESS" -Output "Ready to run with: npm start"
      return $true
    }
    else {
      Add-BuildResult -Project "rawrz-http-encryptor" -Status "FAILED" -Error "npm install failed"
      return $false
    }
  }
  catch {
    Add-BuildResult -Project "rawrz-http-encryptor" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-RawrzPlatform {
  Write-Section "Building rawrz-security-platform (Node.js)"
    
  $RawrzPlatformDir = "D:\Security Research aka GitHub Repos\rawrz-security-platform\ItsMehRAWRXD-rawrz-security-platform-3243228"
    
  if (!(Test-Path $RawrzPlatformDir)) {
    Add-BuildResult -Project "rawrz-security-platform" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $RawrzPlatformDir
    
  try {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    npm install 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Add-BuildResult -Project "rawrz-security-platform" -Status "SUCCESS" -Output "Ready to run with: node rawrz-standalone.js"
      return $true
    }
    else {
      Add-BuildResult -Project "rawrz-security-platform" -Status "FAILED" -Error "npm install failed"
      return $false
    }
  }
  catch {
    Add-BuildResult -Project "rawrz-security-platform" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-SaaSSecurity {
  Write-Section "Building SaaSEncryptionSecurity (Next.js + Java)"
    
  $SaaSDir = "D:\Security Research aka GitHub Repos\SaaSEncryptionSecurity\ItsMehRAWRXD-SaaSEncryptionSecurity-59480fa"
    
  if (!(Test-Path $SaaSDir)) {
    Add-BuildResult -Project "SaaSEncryptionSecurity" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $SaaSDir
    
  try {
    # Compile Java π-Engine
    Write-Host "Compiling Java π-Engine..." -ForegroundColor Yellow
    javac src\pi-engine\PiEngine.java 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
      npm install 2>&1 | Out-Null
            
      if ($LASTEXITCODE -eq 0) {
        Add-BuildResult -Project "SaaSEncryptionSecurity" -Status "SUCCESS" -Output "π-Engine compiled, npm ready"
        return $true
      }
    }
        
    Add-BuildResult -Project "SaaSEncryptionSecurity" -Status "FAILED" -Error "Build failed"
    return $false
  }
  catch {
    Add-BuildResult -Project "SaaSEncryptionSecurity" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Build-Star5IDE {
  Write-Section "Building Star5IDE (Node.js)"
    
  $Star5IDEDir = "D:\Security Research aka GitHub Repos\Star5IDE\ItsMehRAWRXD-Star5IDE-950cfb1"
    
  if (!(Test-Path $Star5IDEDir)) {
    Add-BuildResult -Project "Star5IDE" -Status "SKIPPED" -Error "Directory not found"
    return $false
  }
    
  Set-Location $Star5IDEDir
    
  try {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    npm install 2>&1 | Out-Null
        
    if ($LASTEXITCODE -eq 0) {
      Add-BuildResult -Project "Star5IDE" -Status "SUCCESS" -Output "Ready to run with: node rawrz-standalone.js"
      return $true
    }
    else {
      Add-BuildResult -Project "Star5IDE" -Status "FAILED" -Error "npm install failed"
      return $false
    }
  }
  catch {
    Add-BuildResult -Project "Star5IDE" -Status "FAILED" -Error $_.Exception.Message
    return $false
  }
}

function Show-BuildReport {
  Write-Section "BUILD REPORT"
    
  $SuccessCount = ($Global:BuildResults | Where-Object { $_.Status -eq "SUCCESS" }).Count
  $FailedCount = ($Global:BuildResults | Where-Object { $_.Status -eq "FAILED" }).Count
  $SkippedCount = ($Global:BuildResults | Where-Object { $_.Status -eq "SKIPPED" }).Count
    
  Write-Host "Total Projects: $($Global:BuildResults.Count)" -ForegroundColor Cyan
  Write-Host "Success: $SuccessCount" -ForegroundColor Green
  Write-Host "Failed: $FailedCount" -ForegroundColor Red
  Write-Host "Skipped: $SkippedCount" -ForegroundColor Yellow
  Write-Host ""
    
  foreach ($Result in $Global:BuildResults) {
    $Color = switch ($Result.Status) {
      "SUCCESS" { "Green" }
      "FAILED" { "Red" }
      "SKIPPED" { "Yellow" }
    }
        
    Write-Host "[$($Result.Status)]" -ForegroundColor $Color -NoNewline
    Write-Host " $($Result.Project)" -ForegroundColor White
        
    if ($Result.Output) {
      Write-Host "  Output: $($Result.Output)" -ForegroundColor Gray
    }
    if ($Result.Error) {
      Write-Host "  Error: $($Result.Error)" -ForegroundColor Gray
    }
  }
    
  # Export to JSON
  $ReportFile = "build-report-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
  $Global:BuildResults | ConvertTo-Json -Depth 5 | Out-File $ReportFile
  Write-Host "`nBuild report saved to: $ReportFile" -ForegroundColor Cyan
}

# Main execution
Write-Host @"
╔════════════════════════════════════════════════════════════╗
║                                                            ║
║       MASTER BUILD SYSTEM - Security Research Suite       ║
║                                                            ║
║  Building all 9 security research projects...             ║
║                                                            ║
╚════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

Write-Host ""

if ($All -or (!$Mirai -and !$Zencoder -and !$Star -and !$RawrZDesktop -and !$OhGee -and !$RawrzHttp -and !$RawrzPlatform -and !$SaaSSecurity -and !$Star5IDE)) {
  # Build all projects
  Build-Mirai
  Build-Zencoder
  Build-Star
  Build-RawrZDesktop
  Build-OhGee
  Build-RawrzHttp
  Build-RawrzPlatform
  Build-SaaSSecurity
  Build-Star5IDE
}
else {
  if ($Mirai) { Build-Mirai }
  if ($Zencoder) { Build-Zencoder }
  if ($Star) { Build-Star }
  if ($RawrZDesktop) { Build-RawrZDesktop }
  if ($OhGee) { Build-OhGee }
  if ($RawrzHttp) { Build-RawrzHttp }
  if ($RawrzPlatform) { Build-RawrzPlatform }
  if ($SaaSSecurity) { Build-SaaSSecurity }
  if ($Star5IDE) { Build-Star5IDE }
}

Show-BuildReport

Write-Host "`n✓ Master build system execution completed!`n" -ForegroundColor Green
