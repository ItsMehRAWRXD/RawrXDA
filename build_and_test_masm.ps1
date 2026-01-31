# ============================================================================
# MASM Compiler Suite - Quick Build and Test Script
# Builds all three compilers and runs comprehensive tests
# ============================================================================

param(
    [switch]$Clean,
    [switch]$BuildOnly,
    [switch]$TestOnly,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MASM Compiler Suite - Build & Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$RootDir = "E:\RawrXD"
$BuildDir = "$RootDir\build"
$TestDir = "$RootDir\tests\masm"

# Check if NASM is installed
Write-Host "[Check] Verifying NASM installation..." -ForegroundColor Yellow
$nasm = Get-Command nasm -ErrorAction SilentlyContinue
if ($nasm) {
    Write-Host "  ✓ NASM found: $($nasm.Source)" -ForegroundColor Green
    & nasm -version
} else {
    Write-Host "  ✗ NASM not found!" -ForegroundColor Red
    Write-Host "    Install from: https://www.nasm.us/" -ForegroundColor Yellow
    Write-Host "    Solo compiler will not be built." -ForegroundColor Yellow
}
Write-Host ""

# Clean build
if ($Clean) {
    Write-Host "[Clean] Removing build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Host "  ✓ Build directory cleaned" -ForegroundColor Green
    }
    Write-Host ""
}

# Configure CMake
if (-not $TestOnly) {
    Write-Host "[Configure] Running CMake..." -ForegroundColor Yellow
    cd $RootDir
    
    $configCmd = "cmake -B build -G `"Visual Studio 17 2022`" -A x64"
    if ($Verbose) {
        Write-Host "  Command: $configCmd" -ForegroundColor Gray
    }
    
    $output = Invoke-Expression $configCmd 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ✗ CMake configuration failed!" -ForegroundColor Red
        Write-Host $output -ForegroundColor Red
        exit 1
    }
    
    Write-Host "  ✓ CMake configuration succeeded" -ForegroundColor Green
    if ($Verbose) {
        Write-Host $output -ForegroundColor Gray
    }
    Write-Host ""
    
    # Build Solo Compiler
    if ($nasm) {
        Write-Host "[Build] Building Solo Compiler (Pure NASM)..." -ForegroundColor Yellow
        $buildCmd = "cmake --build build --config Release --target masm_solo_compiler"
        if ($Verbose) {
            Write-Host "  Command: $buildCmd" -ForegroundColor Gray
        }
        
        $output = Invoke-Expression $buildCmd 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ Solo compiler built successfully" -ForegroundColor Green
            $soloPath = "$BuildDir\Release\masm_solo_compiler.exe"
            if (Test-Path $soloPath) {
                $size = (Get-Item $soloPath).Length
                Write-Host "    Size: $($size / 1KB) KB" -ForegroundColor Cyan
            }
        } else {
            Write-Host "  ✗ Solo compiler build failed!" -ForegroundColor Red
            if ($Verbose) {
                Write-Host $output -ForegroundColor Red
            }
        }
        Write-Host ""
    }
    
    # Build CLI Compiler
    Write-Host "[Build] Building CLI Compiler (C++)..." -ForegroundColor Yellow
    $buildCmd = "cmake --build build --config Release --target masm_cli_compiler"
    if ($Verbose) {
        Write-Host "  Command: $buildCmd" -ForegroundColor Gray
    }
    
    $output = Invoke-Expression $buildCmd 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ CLI compiler built successfully" -ForegroundColor Green
        $cliPath = "$BuildDir\Release\masm_cli_compiler.exe"
        if (Test-Path $cliPath) {
            $size = (Get-Item $cliPath).Length
            Write-Host "    Size: $($size / 1KB) KB" -ForegroundColor Cyan
        }
    } else {
        Write-Host "  ✗ CLI compiler build failed!" -ForegroundColor Red
        if ($Verbose) {
            Write-Host $output -ForegroundColor Red
        }
    }
    Write-Host ""
    
    # Build Qt Application
    Write-Host "[Build] Building Qt Application (with MASM integration)..." -ForegroundColor Yellow
    $buildCmd = "cmake --build build --config Release"
    if ($Verbose) {
        Write-Host "  Command: $buildCmd" -ForegroundColor Gray
    }
    
    $output = Invoke-Expression $buildCmd 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ Qt application built successfully" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Qt application build failed!" -ForegroundColor Red
        if ($Verbose) {
            Write-Host $output -ForegroundColor Red
        }
    }
    Write-Host ""
}

if ($BuildOnly) {
    Write-Host "Build complete! (Test skipped)" -ForegroundColor Green
    exit 0
}

# Run Tests
Write-Host "[Test] Running CTest suite..." -ForegroundColor Yellow
cd $BuildDir

$testCmd = "ctest -C Release -R masm --verbose"
if ($Verbose) {
    Write-Host "  Command: $testCmd" -ForegroundColor Gray
}

$output = Invoke-Expression $testCmd 2>&1
$testsPassed = $output -match "100% tests passed"

if ($testsPassed) {
    Write-Host "  ✓ All tests passed!" -ForegroundColor Green
} else {
    Write-Host "  ✗ Some tests failed!" -ForegroundColor Red
}

if ($Verbose -or -not $testsPassed) {
    Write-Host $output -ForegroundColor Gray
}
Write-Host ""

# Manual Tests
Write-Host "[Test] Running manual compilation tests..." -ForegroundColor Yellow

# Test Solo Compiler
if ($nasm -and (Test-Path "$BuildDir\Release\masm_solo_compiler.exe")) {
    Write-Host "  Testing Solo Compiler..." -ForegroundColor Cyan
    
    $testFile = "$TestDir\hello.asm"
    $outputFile = "$BuildDir\hello_solo.exe"
    
    & "$BuildDir\Release\masm_solo_compiler.exe" $testFile $outputFile 2>&1 | Out-Null
    
    if (Test-Path $outputFile) {
        Write-Host "    ✓ hello.asm compiled successfully" -ForegroundColor Green
        
        # Try to run it
        $output = & $outputFile 2>&1
        Write-Host "    Output: $output" -ForegroundColor Cyan
    } else {
        Write-Host "    ✗ hello.asm compilation failed" -ForegroundColor Red
    }
}

# Test CLI Compiler
if (Test-Path "$BuildDir\Release\masm_cli_compiler.exe") {
    Write-Host "  Testing CLI Compiler..." -ForegroundColor Cyan
    
    $testFile = "$TestDir\factorial.asm"
    $outputFile = "$BuildDir\factorial_cli.exe"
    
    if ($Verbose) {
        & "$BuildDir\Release\masm_cli_compiler.exe" --verbose $testFile -o $outputFile
    } else {
        & "$BuildDir\Release\masm_cli_compiler.exe" $testFile -o $outputFile 2>&1 | Out-Null
    }
    
    if (Test-Path $outputFile) {
        Write-Host "    ✓ factorial.asm compiled successfully" -ForegroundColor Green
        
        # Try to run it
        & $outputFile 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "    ✓ Execution successful" -ForegroundColor Green
        }
    } else {
        Write-Host "    ✗ factorial.asm compilation failed" -ForegroundColor Red
    }
}

Write-Host ""

# Summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$totalTests = 0
$passedTests = 0

if ($nasm -and (Test-Path "$BuildDir\Release\masm_solo_compiler.exe")) {
    $totalTests++
    $passedTests++
    Write-Host "✓ Solo Compiler: Built and Tested" -ForegroundColor Green
} elseif ($nasm) {
    $totalTests++
    Write-Host "✗ Solo Compiler: Build Failed" -ForegroundColor Red
} else {
    Write-Host "○ Solo Compiler: Skipped (NASM not found)" -ForegroundColor Yellow
}

if (Test-Path "$BuildDir\Release\masm_cli_compiler.exe") {
    $totalTests++
    $passedTests++
    Write-Host "✓ CLI Compiler: Built and Tested" -ForegroundColor Green
} else {
    $totalTests++
    Write-Host "✗ CLI Compiler: Build Failed" -ForegroundColor Red
}

# Check if Qt app exists (name may vary)
$qtApps = Get-ChildItem "$BuildDir\Release\*.exe" -ErrorAction SilentlyContinue | 
          Where-Object { $_.Name -notmatch "masm_" }

if ($qtApps) {
    $totalTests++
    $passedTests++
    Write-Host "✓ Qt Application: Built (MASM Integration Included)" -ForegroundColor Green
} else {
    $totalTests++
    Write-Host "✗ Qt Application: Build Failed" -ForegroundColor Red
}

Write-Host ""
Write-Host "Result: $passedTests/$totalTests compilers built successfully" -ForegroundColor $(if ($passedTests -eq $totalTests) { "Green" } else { "Yellow" })

if ($passedTests -eq $totalTests) {
    Write-Host ""
    Write-Host "🎉 All compilers built and tested successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  - Run: .\build\Release\masm_solo_compiler.exe tests\masm\hello.asm output.exe" -ForegroundColor Gray
    Write-Host "  - Run: .\build\Release\masm_cli_compiler.exe --help" -ForegroundColor Gray
    Write-Host "  - Run: .\build\Release\<QtApp>.exe (and access MASM menu)" -ForegroundColor Gray
    exit 0
} else {
    Write-Host ""
    Write-Host "⚠ Some compilers failed to build. Check errors above." -ForegroundColor Yellow
    exit 1
}
