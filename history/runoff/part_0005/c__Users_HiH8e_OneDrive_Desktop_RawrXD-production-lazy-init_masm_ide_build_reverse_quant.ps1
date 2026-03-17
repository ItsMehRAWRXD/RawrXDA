#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build script for PIRAM Reverse Quantization module
.DESCRIPTION
    Compiles and tests the reverse quantization engine for Q4/Q5/Q8 formats
.PARAMETER Test
    Run test harness after build
.PARAMETER Clean
    Clean build artifacts before compiling
.PARAMETER BuildDir
    Output directory for compiled binaries
#>

param(
    [switch]$Test,
    [switch]$Clean,
    [string]$BuildDir = ".\build"
)

# Configuration
$MASM32_BIN = "C:\masm32\bin"
$ML_EXE = "$MASM32_BIN\ml.exe"
$LINK_EXE = "$MASM32_BIN\link.exe"

# Source files
$SrcDir = ".\src"
$ReverseQuantFile = "$SrcDir\piram_reverse_quantization.asm"
$TestFile = "$SrcDir\piram_reverse_quant_test.asm"

# Output files
$ReverseQuantObj = "$BuildDir\piram_reverse_quantization.obj"
$TestObj = "$BuildDir\piram_reverse_quant_test.obj"
$TestExe = "$BuildDir\piram_reverse_quant_test.exe"

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   PIRAM REVERSE QUANTIZATION MODULE - BUILD SYSTEM            ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Clean if requested
if ($Clean) {
    Write-Host "[*] Cleaning previous builds..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }
    Write-Host "[✓] Cleaned" -ForegroundColor Green
    Write-Host ""
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# Check for MASM32
if (-not (Test-Path $ML_EXE)) {
    Write-Host "[✗] ERROR: MASM32 not found at $MASM32_BIN" -ForegroundColor Red
    Write-Host "    Please install MASM32 from http://www.masm32.com/" -ForegroundColor Red
    exit 1
}

Write-Host "[✓] MASM32 toolchain found" -ForegroundColor Green
Write-Host "    ML.EXE: $ML_EXE" -ForegroundColor Gray
Write-Host "    LINK.EXE: $LINK_EXE" -ForegroundColor Gray
Write-Host ""

# Step 1: Compile reverse quantization module
Write-Host "[*] Compiling reverse quantization module..." -ForegroundColor Cyan
Write-Host "    Source: $ReverseQuantFile" -ForegroundColor Gray

$CompileArgs = @(
    "/c",                           # Compile only
    "/Cp",                          # Preserve case
    "/Fl`"$BuildDir\build.lst`"",   # Generate listing
    "/Fo`"$ReverseQuantObj`"",      # Output object file
    $ReverseQuantFile
)

& $ML_EXE @CompileArgs 2>&1 | Tee-Object -Variable CompileOutput | ForEach-Object {
    if ($_ -match "error") {
        Write-Host "    $($_)" -ForegroundColor Red
    } elseif ($_ -match "warning") {
        Write-Host "    $($_)" -ForegroundColor Yellow
    }
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "[✗] Compilation failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

Write-Host "[✓] Reverse quantization module compiled" -ForegroundColor Green
Write-Host "    Output: $ReverseQuantObj" -ForegroundColor Gray
Write-Host ""

# Step 2: Compile test harness
if ($Test) {
    Write-Host "[*] Compiling test harness..." -ForegroundColor Cyan
    Write-Host "    Source: $TestFile" -ForegroundColor Gray
    
    $CompileArgs = @(
        "/c",
        "/Cp",
        "/Fl`"$BuildDir\test_build.lst`"",
        "/Fo`"$TestObj`"",
        $TestFile
    )
    
    & $ML_EXE @CompileArgs 2>&1 | Tee-Object -Variable TestCompileOutput | ForEach-Object {
        if ($_ -match "error") {
            Write-Host "    $($_)" -ForegroundColor Red
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[✗] Test compilation failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "[✓] Test harness compiled" -ForegroundColor Green
    Write-Host ""
    
    # Step 3: Link test executable
    Write-Host "[*] Linking test executable..." -ForegroundColor Cyan
    
    $LinkArgs = @(
        "/SUBSYSTEM:CONSOLE",
        "/OUT:`"$TestExe`"",
        $ReverseQuantObj,
        $TestObj,
        "kernel32.lib"
    )
    
    & $LINK_EXE @LinkArgs 2>&1 | Tee-Object -Variable LinkOutput | ForEach-Object {
        if ($_ -match "error") {
            Write-Host "    $($_)" -ForegroundColor Red
        } elseif ($_ -match "warning") {
            Write-Host "    $($_)" -ForegroundColor Yellow
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[✗] Linking failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "[✓] Test executable created" -ForegroundColor Green
    Write-Host "    Output: $TestExe" -ForegroundColor Gray
    Write-Host ""
    
    # Step 4: Run tests
    Write-Host "[*] Running test harness..." -ForegroundColor Cyan
    Write-Host ""
    
    if (Test-Path $TestExe) {
        & $TestExe
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "[✓] All tests passed" -ForegroundColor Green
        } else {
            Write-Host ""
            Write-Host "[⚠] Test execution completed with exit code $LASTEXITCODE" -ForegroundColor Yellow
        }
    } else {
        Write-Host "[✗] Test executable not found" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    BUILD COMPLETE                             ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Summary
Write-Host "Build Summary:" -ForegroundColor Green
Write-Host "  Build Directory: $BuildDir" -ForegroundColor Gray
Write-Host "  Object Files:" -ForegroundColor Gray
Get-Item $BuildDir\*.obj -ErrorAction SilentlyContinue | ForEach-Object {
    $size = [math]::Round($_.Length / 1KB, 2)
    Write-Host "    - $($_.Name) ($size KB)" -ForegroundColor Gray
}

if ($Test) {
    Write-Host "  Test Executable:" -ForegroundColor Gray
    if (Test-Path $TestExe) {
        $size = [math]::Round((Get-Item $TestExe).Length / 1KB, 2)
        Write-Host "    - piram_reverse_quant_test.exe ($size KB)" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Green
Write-Host "  1. Review build output in $BuildDir" -ForegroundColor Gray
Write-Host "  2. Run test harness: .\build\piram_reverse_quant_test.exe" -ForegroundColor Gray
Write-Host "  3. Integrate with PiFabric GGUF loader" -ForegroundColor Gray
Write-Host ""

exit 0
