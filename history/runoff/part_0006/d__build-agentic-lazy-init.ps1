# Build Script for Agentic Lazy Init Integration
# Tests both CLI and GUI builds with new lazy init features

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║    RawrXD Agentic Lazy Init - Build Test Script          ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Continue"
$ProjectRoot = "D:\RawrXD-production-lazy-init"
$BuildDir = "$ProjectRoot\build"

# Verify project exists
if (-not (Test-Path $ProjectRoot)) {
    Write-Host "❌ Error: Project directory not found: $ProjectRoot" -ForegroundColor Red
    exit 1
}

Write-Host "[1/5] Checking for existing build directory..." -ForegroundColor Yellow
if (Test-Path $BuildDir) {
    Write-Host "   ✓ Build directory exists: $BuildDir" -ForegroundColor Green
} else {
    Write-Host "   Creating build directory: $BuildDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

Write-Host ""
Write-Host "[2/5] Configuring CMake..." -ForegroundColor Yellow
Push-Location $BuildDir

try {
    # Run CMake configuration
    $cmakeOutput = & cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_BUILD_TYPE=Release `
        -DQt6_DIR="C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6" `
        2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   ✓ CMake configuration successful" -ForegroundColor Green
    } else {
        Write-Host "   ⚠ CMake configuration had warnings" -ForegroundColor Yellow
        Write-Host "   Last 10 lines of output:" -ForegroundColor Gray
        $cmakeOutput | Select-Object -Last 10 | ForEach-Object { Write-Host "     $_" -ForegroundColor Gray }
    }
} catch {
    Write-Host "   ❌ CMake configuration failed: $_" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host ""
Write-Host "[3/5] Building RawrXD-Core library..." -ForegroundColor Yellow

try {
    $coreOutput = & cmake --build . --config Release --target RawrXD-Core 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   ✓ RawrXD-Core built successfully" -ForegroundColor Green
    } else {
        Write-Host "   ❌ RawrXD-Core build failed" -ForegroundColor Red
        Write-Host "   Last 20 lines of output:" -ForegroundColor Gray
        $coreOutput | Select-Object -Last 20 | ForEach-Object { Write-Host "     $_" -ForegroundColor Gray }
        Pop-Location
        exit 1
    }
} catch {
    Write-Host "   ❌ Core build exception: $_" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host ""
Write-Host "[4/5] Building CLI executable..." -ForegroundColor Yellow

try {
    $cliOutput = & cmake --build . --config Release --target rawrxd-cli-new 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   ✓ CLI executable built successfully" -ForegroundColor Green
        
        # Check if executable exists
        $cliExe = "$BuildDir\bin\Release\rawrxd-cli.exe"
        if (Test-Path $cliExe) {
            $cliSize = (Get-Item $cliExe).Length / 1MB
            Write-Host "   📦 CLI size: $($cliSize.ToString('F2')) MB" -ForegroundColor Cyan
        }
    } else {
        Write-Host "   ⚠ CLI build had issues" -ForegroundColor Yellow
        Write-Host "   Last 20 lines of output:" -ForegroundColor Gray
        $cliOutput | Select-Object -Last 20 | ForEach-Object { Write-Host "     $_" -ForegroundColor Gray }
    }
} catch {
    Write-Host "   ⚠ CLI build exception: $_" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "[5/5] Building GUI executable..." -ForegroundColor Yellow

try {
    $guiOutput = & cmake --build . --config Release --target RawrXD-QtShell 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "   ✓ GUI executable built successfully" -ForegroundColor Green
        
        # Check if executable exists
        $guiExe = "$BuildDir\bin\Release\RawrXD-QtShell.exe"
        if (Test-Path $guiExe) {
            $guiSize = (Get-Item $guiExe).Length / 1MB
            Write-Host "   📦 GUI size: $($guiSize.ToString('F2')) MB" -ForegroundColor Cyan
        }
    } else {
        Write-Host "   ⚠ GUI build had issues" -ForegroundColor Yellow
        Write-Host "   Last 20 lines of output:" -ForegroundColor Gray
        $guiOutput | Select-Object -Last 20 | ForEach-Object { Write-Host "     $_" -ForegroundColor Gray }
    }
} catch {
    Write-Host "   ⚠ GUI build exception: $_" -ForegroundColor Yellow
}

Pop-Location

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    Build Summary                          ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check what was successfully built
$results = @()

$coreLib = "$BuildDir\src\core\Release\RawrXD-Core.lib"
if (Test-Path $coreLib) {
    $results += "✓ RawrXD-Core library"
    Write-Host "  ✓ Core Library: $(Split-Path $coreLib -Leaf)" -ForegroundColor Green
}

$cliExe = "$BuildDir\bin\Release\rawrxd-cli.exe"
if (Test-Path $cliExe) {
    $results += "✓ CLI executable"
    Write-Host "  ✓ CLI Executable: $cliExe" -ForegroundColor Green
}

$guiExe = "$BuildDir\bin\Release\RawrXD-QtShell.exe"
if (Test-Path $guiExe) {
    $results += "✓ GUI executable"
    Write-Host "  ✓ GUI Executable: $guiExe" -ForegroundColor Green
}

Write-Host ""

if ($results.Count -eq 3) {
    Write-Host "🎯 SUCCESS: All components built successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  1. Test CLI: $BuildDir\bin\Release\rawrxd-cli.exe --help" -ForegroundColor White
    Write-Host "  2. Test GUI: $BuildDir\bin\Release\RawrXD-QtShell.exe" -ForegroundColor White
    Write-Host "  3. Check logs in $BuildDir\bin\Release\terminal_diagnostics.log" -ForegroundColor White
    exit 0
} elseif ($results.Count -gt 0) {
    Write-Host "⚠ PARTIAL SUCCESS: $($results.Count)/3 components built" -ForegroundColor Yellow
    $results | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    exit 0
} else {
    Write-Host "❌ FAILURE: No components were built successfully" -ForegroundColor Red
    Write-Host "Check the build output above for error details" -ForegroundColor Yellow
    exit 1
}
