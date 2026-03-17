#!/usr/bin/env pwsh
# =============================================================================
# RawrXD IDE — Phase 33: Gold Master Packaging Script
# =============================================================================
# Creates a portable ZIP distribution with everything needed to run the IDE.
# No installer required — extract and run.
#
# Usage:
#   .\scripts\package_release.ps1                     # Default: Release build
#   .\scripts\package_release.ps1 -SkipBuild          # Package existing binary
#   .\scripts\package_release.ps1 -Channel "beta"     # Tag as beta
# =============================================================================

param(
    [switch]$SkipBuild,
    [string]$Channel = "release",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Version (must match include/rawrxd_version.h)
$Major = 7
$Minor = 4
$Patch = 0
$Version = "$Major.$Minor.$Patch"
$PackageName = "RawrXD-IDE-v$Version-win64"

Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  RawrXD IDE — Gold Master Packaging" -ForegroundColor Cyan
Write-Host "  Version: $Version  Channel: $Channel" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan

$RootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$RootDir\CMakeLists.txt")) {
    $RootDir = $PSScriptRoot | Split-Path -Parent
}
if (-not (Test-Path "$RootDir\CMakeLists.txt")) {
    $RootDir = Get-Location
}

$BuildDir = Join-Path $RootDir "build"
$BinDir   = Join-Path $BuildDir "bin"
$Binary   = Join-Path $BinDir "RawrXD-Win32IDE.exe"

# ─────────────────────────────────────────────────────────────────────────────
# Step 1: Build (unless skipped)
# ─────────────────────────────────────────────────────────────────────────────
if (-not $SkipBuild) {
    Write-Host "`n[1/6] Building Release binary..." -ForegroundColor Yellow

    # Set up MSVC environment
    $msvcBase = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
    $sdkBase  = "C:\Program Files (x86)\Windows Kits\10"
    $sdkVer   = "10.0.22621.0"

    if (Test-Path $msvcBase) {
        $env:PATH    = "$msvcBase\bin\Hostx64\x64;$sdkBase\bin\$sdkVer\x64;$env:PATH"
        $env:INCLUDE = "$msvcBase\include;$sdkBase\Include\$sdkVer\ucrt;$sdkBase\Include\$sdkVer\um;$sdkBase\Include\$sdkVer\shared;$sdkBase\Include\$sdkVer\winrt;$sdkBase\Include\$sdkVer\cppwinrt"
        $env:LIB     = "$msvcBase\lib\x64;$sdkBase\Lib\$sdkVer\ucrt\x64;$sdkBase\Lib\$sdkVer\um\x64"
        $env:LIBPATH = "$msvcBase\lib\x64"
    }

    Push-Location $RootDir
    try {
        if (-not (Test-Path $BuildDir)) {
            cmake -B build -G Ninja `
                -DCMAKE_C_COMPILER=cl `
                -DCMAKE_CXX_COMPILER=cl `
                -DCMAKE_ASM_MASM_COMPILER=ml64 `
                -DCMAKE_RC_COMPILER=rc `
                -DCMAKE_BUILD_TYPE=Release
        }
        cmake --build build --config Release --target RawrXD-Win32IDE
        if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "`n[1/6] Skipping build (using existing binary)" -ForegroundColor DarkGray
}

if (-not (Test-Path $Binary)) {
    throw "Binary not found at $Binary — build may have failed"
}

$BinaryInfo = Get-Item $Binary
$BinarySizeKB = [math]::Round($BinaryInfo.Length / 1024, 1)
$BinarySizeMB = [math]::Round($BinaryInfo.Length / 1048576, 2)
Write-Host "  Binary: $($BinaryInfo.Name) ($BinarySizeMB MB)" -ForegroundColor Green

# ─────────────────────────────────────────────────────────────────────────────
# Step 2: Create staging directory
# ─────────────────────────────────────────────────────────────────────────────
Write-Host "`n[2/6] Creating staging directory..." -ForegroundColor Yellow

$StageDir = Join-Path $RootDir $OutputDir $PackageName
if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

# Create subdirectories
@("config", "models", "symbols", "plugins", "gui", "docs") | ForEach-Object {
    New-Item -ItemType Directory -Path (Join-Path $StageDir $_) -Force | Out-Null
}

# ─────────────────────────────────────────────────────────────────────────────
# Step 3: Copy binary + runtime
# ─────────────────────────────────────────────────────────────────────────────
Write-Host "`n[3/6] Copying binary and runtime..." -ForegroundColor Yellow

Copy-Item $Binary -Destination $StageDir -Force

# Copy VC runtime redistributable DLLs if they exist next to the binary
$RuntimeDLLs = @("MSVCP140.dll", "VCRUNTIME140.dll", "VCRUNTIME140_1.dll")
$VCRedistPath = "$msvcBase\bin\Hostx64\x64"

$needsRedist = $false
foreach ($dll in $RuntimeDLLs) {
    $dllPath = Join-Path $VCRedistPath $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath -Destination $StageDir -Force
        Write-Host "  + $dll (bundled)" -ForegroundColor DarkGray
    } else {
        # Try Microsoft.VC143.CRT merge module path
        $altPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\14.44.35207\x64\Microsoft.VC143.CRT\$dll"
        if (Test-Path $altPath) {
            Copy-Item $altPath -Destination $StageDir -Force
            Write-Host "  + $dll (from Redist)" -ForegroundColor DarkGray
        } else {
            $needsRedist = $true
            Write-Host "  ! $dll not found — users may need VC Redist" -ForegroundColor DarkYellow
        }
    }
}

# ─────────────────────────────────────────────────────────────────────────────
# Step 4: Copy config, GUI, and docs
# ─────────────────────────────────────────────────────────────────────────────
Write-Host "`n[4/6] Copying config, GUI assets, and docs..." -ForegroundColor Yellow

# Default config
$defaultConfig = @{
    OllamaHost     = "http://localhost:11434"
    OllamaModel    = "llama3"
    EnableThreading = $true
    EnableStreaming = $true
    EnableSecurity = $true
    Version        = $Version
    Channel        = $Channel
} | ConvertTo-Json -Depth 4
Set-Content -Path (Join-Path $StageDir "config\settings.json") -Value $defaultConfig -Encoding UTF8

# Default shortcuts
$defaultShortcuts = @{
    version = 1
    shortcuts = @(
        @{ action = "new_file";    keys = "Ctrl+N" }
        @{ action = "open_file";   keys = "Ctrl+O" }
        @{ action = "save_file";   keys = "Ctrl+S" }
        @{ action = "save_as";     keys = "Ctrl+Shift+S" }
        @{ action = "close_tab";   keys = "Ctrl+W" }
        @{ action = "find";        keys = "Ctrl+F" }
        @{ action = "replace";     keys = "Ctrl+H" }
        @{ action = "goto_line";   keys = "Ctrl+G" }
        @{ action = "build";       keys = "Ctrl+B" }
        @{ action = "run";         keys = "F5" }
        @{ action = "debug";       keys = "F9" }
        @{ action = "terminal";    keys = "Ctrl+`" }
        @{ action = "command_palette"; keys = "Ctrl+Shift+P" }
        @{ action = "audit_dashboard"; keys = "Ctrl+Shift+A" }
        @{ action = "voice_chat";  keys = "Ctrl+Shift+V" }
        @{ action = "cot_review";  keys = "Ctrl+Shift+R" }
    )
} | ConvertTo-Json -Depth 4
Set-Content -Path (Join-Path $StageDir "config\shortcuts.json") -Value $defaultShortcuts -Encoding UTF8

# Security config
$secConfig = @{
    allowedScripts = @("*.ps1")
    validateInputs = $true
    enforceTLS     = $true
    rateLimitRPS   = 60
} | ConvertTo-Json -Depth 4
Set-Content -Path (Join-Path $StageDir "config\security.json") -Value $secConfig -Encoding UTF8

# GUI assets
if (Test-Path (Join-Path $RootDir "gui\ide_chatbot.html")) {
    Copy-Item (Join-Path $RootDir "gui\ide_chatbot.html") -Destination (Join-Path $StageDir "gui\") -Force
    Write-Host "  + gui/ide_chatbot.html" -ForegroundColor DarkGray
}

# Icon
if (Test-Path (Join-Path $RootDir "RawrXD.ico")) {
    Copy-Item (Join-Path $RootDir "RawrXD.ico") -Destination $StageDir -Force
}

# Docs
foreach ($doc in @("docs\ARCHITECTURE.md", "docs\USER_GUIDE.md", "docs\API.md")) {
    $src = Join-Path $RootDir $doc
    if (Test-Path $src) {
        Copy-Item $src -Destination (Join-Path $StageDir "docs\") -Force
        Write-Host "  + $doc" -ForegroundColor DarkGray
    }
}

# LICENSE + README
foreach ($file in @("LICENSE", "README.md")) {
    $src = Join-Path $RootDir $file
    if (Test-Path $src) {
        Copy-Item $src -Destination $StageDir -Force
    }
}

# ─────────────────────────────────────────────────────────────────────────────
# Step 5: Generate manifest
# ─────────────────────────────────────────────────────────────────────────────
Write-Host "`n[5/6] Generating release manifest..." -ForegroundColor Yellow

$fileList = Get-ChildItem $StageDir -Recurse -File | ForEach-Object {
    @{
        Path = $_.FullName.Replace($StageDir, "").TrimStart("\")
        Size = $_.Length
        Hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
    }
}

$manifest = @{
    name        = "RawrXD IDE"
    version     = $Version
    channel     = $Channel
    buildDate   = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    platform    = "win64"
    compiler    = "MSVC 14.44 + ml64"
    binarySize  = $BinaryInfo.Length
    binarySHA256 = (Get-FileHash $Binary -Algorithm SHA256).Hash
    fileCount   = $fileList.Count
    files       = $fileList
    dependencies = @{
        vcRedist = if ($needsRedist) { "required" } else { "bundled" }
        dotnet   = "not required"
        ollama   = "optional (localhost:11434 for AI features)"
    }
} | ConvertTo-Json -Depth 5
Set-Content -Path (Join-Path $StageDir "MANIFEST.json") -Value $manifest -Encoding UTF8

# ─────────────────────────────────────────────────────────────────────────────
# Step 6: Create ZIP archive
# ─────────────────────────────────────────────────────────────────────────────
Write-Host "`n[6/6] Creating ZIP archive..." -ForegroundColor Yellow

$ZipPath = Join-Path $RootDir $OutputDir "$PackageName.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }

Compress-Archive -Path $StageDir -DestinationPath $ZipPath -CompressionLevel Optimal

$ZipInfo = Get-Item $ZipPath
$ZipSizeMB = [math]::Round($ZipInfo.Length / 1048576, 2)

Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  PACKAGING COMPLETE" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  Package: $($ZipInfo.Name)" -ForegroundColor White
Write-Host "  Size:    $ZipSizeMB MB (binary: $BinarySizeMB MB)" -ForegroundColor White
Write-Host "  Path:    $ZipPath" -ForegroundColor White
Write-Host "  Files:   $($fileList.Count) files in package" -ForegroundColor White
Write-Host "  SHA256:  $(($manifest | ConvertFrom-Json).binarySHA256)" -ForegroundColor DarkGray
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""

# Summary
Write-Host "Distribution layout:" -ForegroundColor Cyan
Get-ChildItem $StageDir -Recurse | ForEach-Object {
    $indent = "  " * ($_.FullName.Replace($StageDir, "").Split("\").Count - 1)
    if ($_.PSIsContainer) {
        Write-Host "$indent$($_.Name)/" -ForegroundColor Yellow
    } else {
        $sizeStr = if ($_.Length -gt 1MB) { "$([math]::Round($_.Length/1MB, 1)) MB" }
                   elseif ($_.Length -gt 1KB) { "$([math]::Round($_.Length/1KB, 1)) KB" }
                   else { "$($_.Length) B" }
        Write-Host "$indent$($_.Name) ($sizeStr)" -ForegroundColor Gray
    }
}

Write-Host "`nTo ship: Upload $($ZipInfo.Name) to GitHub Releases" -ForegroundColor Cyan
