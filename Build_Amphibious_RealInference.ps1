#!/usr/bin/env powershell
#=============================================================================
# Build_amphibious_RealInference.ps1
# Build CLI + GUI with real ML inference integration
# Outputs: RawrXD_CLI_Real.exe, RawrXD_GUI_Real.exe
#=============================================================================

$ErrorActionPreference = "Stop"
$WarningPreference = "SilentlyContinue"

# Directories
$MASM64_DIR = "C:\masm64"
$ML64_EXE = Join-Path $MASM64_DIR "bin64\ml64.exe"
$LINK_EXE = "link.exe"
$BUILD_DIR = "d:\rawrxd\build_out"
$SRC_DIR = "d:\rawrxd"

Write-Host "=== RawrXD Amphibious Build (Real Inference) ===" -ForegroundColor Cyan

# Step 1: Assemble Sovereign Core (unchanged)
Write-Host "`n[1/4] Assembling Sovereign Core..." -ForegroundColor Yellow
& $ML64_EXE /c /nologo `
    /I "$MASM64_DIR\include64" `
    /Fo "$BUILD_DIR\sovereign_core.obj" `
    "$SRC_DIR\RawrXD_Sovereign_Core.asm"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: ml64 failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ sovereign_core.obj" -ForegroundColor Green

# Step 2: Compile C++ Inference Engine
Write-Host "`n[2/4] Compiling ML Inference Engine..." -ForegroundColor Yellow
$CL_CMD = "cl.exe"
$INFER_CPP = "$SRC_DIR\src\inference\MLInferenceEngine.cpp"
$INFER_OBJ = "$BUILD_DIR\ml_inference.obj"

& $CL_CMD /c /nologo /std:c++17 /EHsc `
    /I "$SRC_DIR\src\inference" `
    /I "C:\vcpkg\installed\x64-windows\include" `
    /Fo "$INFER_OBJ" `
    "$INFER_CPP"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: cl.exe failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ ml_inference.obj" -ForegroundColor Green

# Step 3: Compile GUI Token Stream Display
Write-Host "`n[3/4] Compiling GUI Token Stream Display..." -ForegroundColor Yellow
$GUI_CPP = "$SRC_DIR\src\gui\TokenStreamDisplay.cpp"
$GUI_OBJ = "$BUILD_DIR\token_stream_display.obj"

& $CL_CMD /c /nologo /std:c++17 /EHsc `
    /I "$SRC_DIR\src\gui" `
    /Fo "$GUI_OBJ" `
    "$GUI_CPP"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: cl.exe failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ token_stream_display.obj" -ForegroundColor Green

# Step 4: Assemble CLI with Inference
Write-Host "`n[4/4] Assembling CLI/GUI Entry Points..." -ForegroundColor Yellow

# CLI
& $ML64_EXE /c /nologo `
    /I "$MASM64_DIR\include64" `
    /Fo "$BUILD_DIR\cli_real.obj" `
    "$SRC_DIR\RawrXD_CLI_RealInference.asm"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: ml64 CLI failed" -ForegroundColor Red
    exit 1
}

# GUI
& $ML64_EXE /c /nologo `
    /I "$MASM64_DIR\include64" `
    /Fo "$BUILD_DIR\gui_real.obj" `
    "$SRC_DIR\RawrXD_GUI_RealInference.asm"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: ml64 GUI failed" -ForegroundColor Red
    exit 1
}

# Step 5: Link CLI
Write-Host "`nLinking CLI executable..." -ForegroundColor Yellow
& $LINK_EXE /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main `
    "$BUILD_DIR\cli_real.obj" `
    "$BUILD_DIR\sovereign_core.obj" `
    "$BUILD_DIR\ml_inference.obj" `
    "$BUILD_DIR\token_stream_display.obj" `
    "kernel32.lib" "user32.lib" "gdi32.lib" `
    "libcurl.lib" "ws2_32.lib" `
    /OUT:"$BUILD_DIR\RawrXD_CLI_Real.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Link CLI failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ RawrXD_CLI_Real.exe" -ForegroundColor Green

# Step 6: Link GUI
Write-Host "`nLinking GUI executable..." -ForegroundColor Yellow
& $LINK_EXE /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:wWinMain `
    "$BUILD_DIR\gui_real.obj" `
    "$BUILD_DIR\sovereign_core.obj" `
    "$BUILD_DIR\ml_inference.obj" `
    "$BUILD_DIR\token_stream_display.obj" `
    "kernel32.lib" "user32.lib" "gdi32.lib" `
    "libcurl.lib" "ws2_32.lib" `
    /OUT:"$BUILD_DIR\RawrXD_GUI_Real.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Link GUI failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ RawrXD_GUI_Real.exe" -ForegroundColor Green

Write-Host "`n=== BUILD SUCCESS ===" -ForegroundColor Cyan
Write-Host "CLI: $BUILD_DIR\RawrXD_CLI_Real.exe" -ForegroundColor Green
Write-Host "GUI: $BUILD_DIR\RawrXD_GUI_Real.exe" -ForegroundColor Green
