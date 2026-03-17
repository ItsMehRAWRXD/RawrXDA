# ═══════════════════════════════════════════════════════════════════════════════
# build-kernels.ps1
# Kernel Dispatch Layer Build Verification
# ═══════════════════════════════════════════════════════════════════════════════

param(
    [switch]$Clean,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Paths
$BUILD_DIR = "d:\rawrxd\build"
$SOURCE_DIR = "d:\rawrxd"
$BIN_DIR = "$BUILD_DIR\..\bin"

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Kernel Dispatch Layer Build Verification                     ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check build directory
if (!(Test-Path $BUILD_DIR)) {
    Write-Host "❌ Build directory not found: $BUILD_DIR" -ForegroundColor Red
    Write-Host "    Run: cmake -B build -G Ninja" -ForegroundColor Yellow
    exit 1
}

Write-Host "✓ Build directory exists: $BUILD_DIR" -ForegroundColor Green

# Check source files
$SOURCES = @(
    "include/kernel_dispatch/KernelDispatcher.hpp",
    "src/kernel_dispatch/KernelDispatcher.cpp",
    "include/agentic/Phase3_Agent_Kernel.h",
    "src/agentic/Phase3_Agent_Kernel_Bridge.cpp",
    "include/thermal/pocket_lab_turbo.h",
    "src/thermal/masm/pocket_lab_turbo.cpp",
    "src/agentic/Phase3_Agent_Kernel_Complete.asm",
    "src/thermal/masm/pocket_lab_turbo.asm"
)

Write-Host ""
Write-Host "Checking source files..." -ForegroundColor Cyan
$MISSING = @()
foreach ($src in $SOURCES) {
    $path = Join-Path $SOURCE_DIR $src
    if (Test-Path $path) {
        Write-Host "  ✓ $src" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $src" -ForegroundColor Red
        $MISSING += $src
    }
}

if ($MISSING.Count -gt 0) {
    Write-Host ""
    Write-Host "❌ Missing $($MISSING.Count) source files:" -ForegroundColor Red
    foreach ($m in $MISSING) {
        Write-Host "   - $m" -ForegroundColor Red
    }
    exit 1
}

Write-Host ""
Write-Host "✓ All source files present" -ForegroundColor Green

# Build kernel dispatch DLLs
Write-Host ""
Write-Host "Building kernel dispatch DLLs..." -ForegroundColor Cyan
Write-Host ""

if ($Clean) {
    Write-Host "Cleaning previous builds..." -ForegroundColor Yellow
    cmake --build $BUILD_DIR --target clean 2>&1 | Select-String -Pattern "^" | % { Write-Host "  $_" }
}

# Build pocket_lab_turbo
Write-Host "Building pocket_lab_turbo.dll..." -ForegroundColor Cyan
$OUTPUT = cmake --build $BUILD_DIR --target pocket_lab_turbo 2>&1
$OUTPUT | ForEach-Object { 
    if ($Verbose) { Write-Host "  $_" }
    elseif ($_ -match "error|Error|ERROR") { Write-Host "  ✗ $_" -ForegroundColor Red }
    elseif ($_ -match "Linking|linking") { Write-Host "  $_" -ForegroundColor Cyan }
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ pocket_lab_turbo build failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ pocket_lab_turbo.dll built successfully" -ForegroundColor Green

# Build Phase3_Agent_Kernel
Write-Host ""
Write-Host "Building Phase3_Agent_Kernel.dll..." -ForegroundColor Cyan
$OUTPUT = cmake --build $BUILD_DIR --target Phase3_Agent_Kernel 2>&1
$OUTPUT | ForEach-Object {
    if ($Verbose) { Write-Host "  $_" }
    elseif ($_ -match "error|Error|ERROR") { Write-Host "  ✗ $_" -ForegroundColor Red }
    elseif ($_ -match "Linking|linking") { Write-Host "  $_" -ForegroundColor Cyan }
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Phase3_Agent_Kernel build failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Phase3_Agent_Kernel.dll built successfully" -ForegroundColor Green

# Verify DLL outputs
Write-Host ""
Write-Host "Verifying DLL outputs..." -ForegroundColor Cyan
$DLLS = @(
    "$BIN_DIR\pocket_lab_turbo.dll",
    "$BIN_DIR\Phase3_Agent_Kernel.dll"
)

$MISSING_DLLS = @()
foreach ($dll in $DLLS) {
    if (Test-Path $dll) {
        $size = (Get-Item $dll).Length
        $size_kb = [math]::Round($size / 1024, 2)
        Write-Host "  ✓ $(Split-Path $dll -Leaf) ($size_kb KB)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $(Split-Path $dll -Leaf) not found" -ForegroundColor Red
        $MISSING_DLLS += $dll
    }
}

if ($MISSING_DLLS.Count -gt 0) {
    Write-Host ""
    Write-Host "⚠️  Expected DLL outputs:" -ForegroundColor Yellow
    foreach ($dll in $DLLS) {
        Write-Host "   $dll" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Build Verification Complete                                  ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Next steps
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Build main IDE:  cmake --build $BUILD_DIR --target RawrXD-Win32IDE" -ForegroundColor Yellow
Write-Host "  2. Test dispatch:   .\bin\RawrXD-Win32IDE --kernel-test" -ForegroundColor Yellow
Write-Host "  3. Load model:      .\bin\RawrXD-Win32IDE --model <model.gguf>" -ForegroundColor Yellow
