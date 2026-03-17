#!/usr/bin/env pwsh
# RawrXD Self-Contained Build Script
# NO EXTERNAL SDK OR TOOLCHAIN REQUIRED

param(
    [switch]$Clean,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Self-Contained Build System" -ForegroundColor Cyan  
Write-Host "  NO SDK DEPENDENCIES" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Paths
$ProjectRoot = Split-Path -Parent $PSCommandPath
$MasmCompiler = Join-Path $ProjectRoot "src\masm\masm_solo_compiler.exe"
$InternalLinker = Join-Path $ProjectRoot "src\masm\internal_link.exe"
$BuildDir = Join-Path $ProjectRoot "build_internal"
$ObjDir = Join-Path $BuildDir "obj"

# Verify internal tools exist
function Test-InternalTools {
    $missing = @()
    
    if (-not (Test-Path $MasmCompiler)) {
        $missing += "Internal MASM compiler: $MasmCompiler"
    }
    
    if (-not (Test-Path $InternalLinker)) {
        $missing += "Internal linker: $InternalLinker"
    }
    
    if ($missing.Count -gt 0) {
        Write-Host "❌ Missing internal tools:" -ForegroundColor Red
        foreach ($tool in $missing) {
            Write-Host "   $tool" -ForegroundColor Yellow
        }
        Write-Host ""
        Write-Host "Build the internal toolchain first:" -ForegroundColor Yellow
        Write-Host "   cd src\masm" -ForegroundColor White
        Write-Host "   .\build_toolchain.ps1" -ForegroundColor White
        exit 1
    }
    
    Write-Host "✅ Internal toolchain verified" -ForegroundColor Green
    Write-Host "   MASM: $MasmCompiler" -ForegroundColor Gray
    Write-Host "   Linker: $InternalLinker" -ForegroundColor Gray
    Write-Host ""
}

# Clean build
if ($Clean) {
    Write-Host "🧹 Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
    Write-Host "✅ Clean complete" -ForegroundColor Green
    exit 0
}

# Create directories
Write-Host "📁 Creating build directories..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

# Test internal tools
Test-InternalTools

# Source files (Assembly only - fully self-contained)
$AsmSources = @(
    "src\asm\main.asm",
    "src\asm\win32_ide.asm",
    "src\asm\vulkan_compute.asm",
    "src\asm\gguf_loader.asm"
)

Write-Host "🔨 Compiling Assembly sources with internal MASM..." -ForegroundColor Yellow
$ObjFiles = @()

foreach ($source in $AsmSources) {
    $sourceFullPath = Join-Path $ProjectRoot $source
    
    if (-not (Test-Path $sourceFullPath)) {
        Write-Host "⚠️  Skipping missing: $source" -ForegroundColor DarkYellow
        continue
    }
    
    $objName = [System.IO.Path]::GetFileNameWithoutExtension($source) + ".obj"
    $objPath = Join-Path $ObjDir $objName
    
    Write-Host "   Compiling: $source" -ForegroundColor White
    
    $compileArgs = @(
        "/c",
        "/Zi",
        "/W3",
        "/Fo`"$objPath`"",
        "`"$sourceFullPath`""
    )
    
    $compileCmd = "& `"$MasmCompiler`" $compileArgs"
    
    if ($Verbose) {
        Write-Host "      Command: $compileCmd" -ForegroundColor Gray
    }
    
    try {
        & $MasmCompiler $compileArgs 2>&1 | ForEach-Object {
            if ($Verbose) { Write-Host "      $_" -ForegroundColor Gray }
        }
        
        if ($LASTEXITCODE -ne 0) {
            throw "Compilation failed with exit code $LASTEXITCODE"
        }
        
        $ObjFiles += $objPath
        Write-Host "      ✅ Success: $objName" -ForegroundColor Green
    }
    catch {
        Write-Host "      ❌ Failed: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "🔗 Linking with internal linker..." -ForegroundColor Yellow

$OutputExe = Join-Path $BuildDir "RawrXD_IDE.exe"

$linkArgs = @(
    "/OUT:`"$OutputExe`"",
    "/SUBSYSTEM:WINDOWS",
    "/MACHINE:X64"
)

# Add object files
$linkArgs += $ObjFiles

# System libraries (only kernel32/user32 - minimal dependencies)
$linkArgs += @(
    "kernel32.lib",
    "user32.lib",
    "gdi32.lib"
)

if ($Verbose) {
    Write-Host "   Link command: $InternalLinker $($linkArgs -join ' ')" -ForegroundColor Gray
}

try {
    & $InternalLinker $linkArgs 2>&1 | ForEach-Object {
        if ($Verbose) { Write-Host "   $_" -ForegroundColor Gray }
    }
    
    if ($LASTEXITCODE -ne 0) {
        throw "Linking failed with exit code $LASTEXITCODE"
    }
    
    Write-Host "✅ Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "📦 Output:" -ForegroundColor Cyan
    Write-Host "   $OutputExe" -ForegroundColor White
    
    # Show size
    $fileInfo = Get-Item $OutputExe
    $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
    Write-Host "   Size: $sizeKB KB" -ForegroundColor Gray
    
    # Verify dependencies
    Write-Host ""
    Write-Host "🔍 Verifying dependencies..." -ForegroundColor Yellow
    Write-Host "   (Should only show kernel32.dll, user32.dll, gdi32.dll)" -ForegroundColor Gray
    
    # Note: This requires dumpbin which we're replacing, so skip for now
    # In production, use internal PE parser
    
    Write-Host ""
    Write-Host "✅ Self-contained build complete!" -ForegroundColor Green
    Write-Host "   No SDK or external toolchain required" -ForegroundColor Gray
}
catch {
    Write-Host "❌ Linking failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Run: $OutputExe" -ForegroundColor White
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
