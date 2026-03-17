# =============================================================================
# Standalone GGUF Robust Tools Build
# Minimal build focused only on gguf_robust_tools_lib
# =============================================================================

$ErrorActionPreference = "Stop"

Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host "RawrXD GGUF Robust Tools - Standalone Build" -ForegroundColor Cyan
Write-Host "==============================================================================" -ForegroundColor Cyan

$PROJECT_ROOT = "D:\RawrXD-production-lazy-init"
$BUILD_DIR = "$PROJECT_ROOT\build_robust"
$ASM_SOURCE = "$PROJECT_ROOT\src\asm\gguf_robust_tools.asm"
$OBJ_OUTPUT = "$BUILD_DIR\gguf_robust_tools.obj"
$LIB_OUTPUT = "$BUILD_DIR\gguf_robust_tools.lib"

# Find ML64 (MASM x64 assembler)
$ML64_PATHS = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
)

$ML64 = $null
foreach ($pattern in $ML64_PATHS) {
    $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ML64 = $found.FullName
        break
    }
}

if (-not $ML64) {
    Write-Host "❌ ML64.EXE not found. Please install Visual Studio with C++ tools." -ForegroundColor Red
    exit 1
}

Write-Host "✅ Found ML64: $ML64" -ForegroundColor Green

# Create build directory
if (!(Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
}

# Step 1: Verify ASM source exists
Write-Host "`n[1/3] Verifying source files..." -ForegroundColor Yellow
if (!(Test-Path $ASM_SOURCE)) {
    Write-Host "❌ Source not found: $ASM_SOURCE" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Source verified: gguf_robust_tools.asm" -ForegroundColor Green

# Step 2: Assemble with ML64
Write-Host "`n[2/3] Assembling with ML64..." -ForegroundColor Yellow
Write-Host "   Command: ml64 /c /nologo /Fo$OBJ_OUTPUT $ASM_SOURCE" -ForegroundColor Gray

& $ML64 /c /nologo "/Fo$OBJ_OUTPUT" $ASM_SOURCE

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Assembly failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

if (!(Test-Path $OBJ_OUTPUT)) {
    Write-Host "❌ Object file not created: $OBJ_OUTPUT" -ForegroundColor Red
    exit 1
}

$OBJ_SIZE = (Get-Item $OBJ_OUTPUT).Length
Write-Host "✅ Object file created: $OBJ_SIZE bytes" -ForegroundColor Green

# Step 3: Create static library (optional)
Write-Host "`n[3/3] Creating static library..." -ForegroundColor Yellow

# Find LIB.EXE
$LIB_EXE = $ML64 -replace "ml64\.exe$", "lib.exe"

if (Test-Path $LIB_EXE) {
    & $LIB_EXE /nologo "/OUT:$LIB_OUTPUT" $OBJ_OUTPUT
    
    if (Test-Path $LIB_OUTPUT) {
        $LIB_SIZE = (Get-Item $LIB_OUTPUT).Length
        Write-Host "✅ Static library created: $LIB_SIZE bytes" -ForegroundColor Green
    } else {
        Write-Host "⚠️  Library creation skipped (lib.exe not available)" -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠️  LIB.EXE not found, skipping static library" -ForegroundColor Yellow
}

# Summary
Write-Host "`n==============================================================================" -ForegroundColor Cyan
Write-Host "✅ GGUF Robust Tools Standalone Build Complete" -ForegroundColor Green
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "📦 Output Files:" -ForegroundColor White
Write-Host "   Object:  $OBJ_OUTPUT" -ForegroundColor Gray
if (Test-Path $LIB_OUTPUT) {
    Write-Host "   Library: $LIB_OUTPUT" -ForegroundColor Gray
}
Write-Host ""
Write-Host "🔧 Exported Functions:" -ForegroundColor White
Write-Host "   ✅ StrSafe_SkipChunk        - 64-bit safe seek" -ForegroundColor Green
Write-Host "   ✅ MemSafe_PeekU64          - Reads uint64 without allocation" -ForegroundColor Green
Write-Host "   ✅ GGUF_SkipStringValue     - Skips corrupted string lengths" -ForegroundColor Green
Write-Host "   ✅ GGUF_SkipArrayValue      - Skips arrays with overflow checks" -ForegroundColor Green
Write-Host "   ✅ GGUF_StreamInit          - Buffered I/O context (64KB ring)" -ForegroundColor Green
Write-Host "   ✅ GGUF_StreamFree          - Cleanup buffered reader" -ForegroundColor Green
Write-Host ""
Write-Host "🎯 Integration:" -ForegroundColor White
Write-Host "   Link: $OBJ_OUTPUT (or $LIB_OUTPUT)" -ForegroundColor Gray
Write-Host '   Header: #include "gguf_robust_masm_bridge_v2.hpp"' -ForegroundColor Gray
Write-Host ""
Write-Host "==============================================================================" -ForegroundColor Cyan
