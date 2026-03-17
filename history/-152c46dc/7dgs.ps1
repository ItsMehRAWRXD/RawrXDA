# Build-Enterprise-IDE.ps1 - Complete enterprise build with validation
param(
    [string]$BuildType = "Release",
    [switch]$RunTests,
    [switch]$StaticAnalysis,
    [switch]$GenerateDocs,
    [switch]$CreateInstaller
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Autonomous IDE Enterprise Build System" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"
$BuildDir = "E:\build"
$InstallDir = "E:\install"

# Step 1: Clean previous build
Write-Host "[1/8] Cleaning previous build..." -ForegroundColor Yellow
if (Test-Path $BuildDir) {
    Remove-Item -Path $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

# Step 2: Validate dependencies
Write-Host "[2/8] Validating dependencies..." -ForegroundColor Yellow

# Check CMake
try {
    $cmakeVersion = cmake --version
    Write-Host "  ✓ CMake found: $($cmakeVersion[0])" -ForegroundColor Green
} catch {
    Write-Host "  ✗ CMake not found! Please install CMake 3.20+" -ForegroundColor Red
    exit 1
}

# Check Qt6
$env:Path += ";C:\Qt\6.7.0\msvc2019_64\bin"
if (-not (Test-Path "C:\Qt\6.7.0\msvc2019_64\bin\qmake.exe")) {
    Write-Host "  ⚠ Qt6 not found at default location" -ForegroundColor Yellow
    Write-Host "    Please ensure Qt6 is installed and in PATH" -ForegroundColor Yellow
}

# Check compiler
try {
    $compilerInfo = cl 2>&1
    Write-Host "  ✓ MSVC compiler found" -ForegroundColor Green
} catch {
    Write-Host "  ⚠ MSVC compiler not found, attempting to load VS environment..." -ForegroundColor Yellow
    
    # Try to find and load Visual Studio environment
    $vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    if ($vsPath) {
        $vcvarsPath = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvarsPath) {
            cmd /c """$vcvarsPath"" && set" | ForEach-Object {
                if ($_ -match "^(.*?)=(.*)$") {
                    Set-Item -Path "env:$($matches[1])" -Value $matches[2]
                }
            }
            Write-Host "  ✓ VS environment loaded" -ForegroundColor Green
        }
    }
}

# Step 3: Static analysis (optional)
if ($StaticAnalysis) {
    Write-Host "[3/8] Running static analysis..." -ForegroundColor Yellow
    
    # cppcheck
    if (Get-Command cppcheck -ErrorAction SilentlyContinue) {
        Write-Host "  Running cppcheck..." -ForegroundColor Cyan
        cppcheck --enable=all --std=c++20 --suppress=missingIncludeSystem `
            --quiet E:\*.cpp E:\*.h 2>&1 | Tee-Object -FilePath "$BuildDir\cppcheck.log"
        Write-Host "  ✓ cppcheck complete" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ cppcheck not found, skipping" -ForegroundColor Yellow
    }
} else {
    Write-Host "[3/8] Skipping static analysis (use -StaticAnalysis to enable)" -ForegroundColor Gray
}

# Step 4: CMake configuration
Write-Host "[4/8] Configuring with CMake..." -ForegroundColor Yellow

Set-Location $BuildDir

$cmakeArgs = @(
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64",
    "-DCMAKE_INSTALL_PREFIX=$InstallDir",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "E:\"
)

try {
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    Write-Host "  ✓ CMake configuration successful" -ForegroundColor Green
} catch {
    Write-Host "  ✗ CMake configuration failed: $_" -ForegroundColor Red
    Set-Location E:\
    exit 1
}

# Step 5: Build
Write-Host "[5/8] Building project..." -ForegroundColor Yellow

try {
    & cmake --build . --config $BuildType --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    Write-Host "  ✓ Build successful" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Build failed: $_" -ForegroundColor Red
    Set-Location E:\
    exit 1
}

# Step 6: Run tests (optional)
if ($RunTests) {
    Write-Host "[6/8] Running tests..." -ForegroundColor Yellow
    
    try {
        & ctest --output-on-failure -C $BuildType
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  ⚠ Some tests failed" -ForegroundColor Yellow
        } else {
            Write-Host "  ✓ All tests passed" -ForegroundColor Green
        }
    } catch {
        Write-Host "  ⚠ Test execution failed: $_" -ForegroundColor Yellow
    }
} else {
    Write-Host "[6/8] Skipping tests (use -RunTests to enable)" -ForegroundColor Gray
}

# Step 7: Generate documentation (optional)
if ($GenerateDocs) {
    Write-Host "[7/8] Generating documentation..." -ForegroundColor Yellow
    
    if (Get-Command doxygen -ErrorAction SilentlyContinue) {
        try {
            & cmake --build . --target docs
            Write-Host "  ✓ Documentation generated" -ForegroundColor Green
        } catch {
            Write-Host "  ⚠ Documentation generation failed" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ⚠ Doxygen not found, skipping" -ForegroundColor Yellow
    }
} else {
    Write-Host "[7/8] Skipping documentation (use -GenerateDocs to enable)" -ForegroundColor Gray
}

# Step 8: Create installer (optional)
if ($CreateInstaller) {
    Write-Host "[8/8] Creating installer..." -ForegroundColor Yellow
    
    try {
        & cmake --build . --target package
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ Installer created successfully" -ForegroundColor Green
            
            # List created packages
            $packages = Get-ChildItem -Path $BuildDir -Filter "AutonomousIDE*" -File
            foreach ($package in $packages) {
                Write-Host "    📦 $($package.Name)" -ForegroundColor Cyan
            }
        }
    } catch {
        Write-Host "  ⚠ Installer creation failed: $_" -ForegroundColor Yellow
    }
} else {
    Write-Host "[8/8] Skipping installer creation (use -CreateInstaller to enable)" -ForegroundColor Gray
}

Set-Location E:\

# Build summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Build Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Type: $BuildType" -ForegroundColor White
Write-Host "Build Directory: $BuildDir" -ForegroundColor White
Write-Host "Install Directory: $InstallDir" -ForegroundColor White

if (Test-Path "$BuildDir\AutonomousIDE.exe") {
    $exeSize = (Get-Item "$BuildDir\AutonomousIDE.exe").Length / 1MB
    Write-Host "Executable Size: $([math]::Round($exeSize, 2)) MB" -ForegroundColor White
    Write-Host ""
    Write-Host "✓ Build completed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To run the application:" -ForegroundColor Cyan
    Write-Host "  cd $BuildDir" -ForegroundColor Yellow
    Write-Host "  .\AutonomousIDE.exe" -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "⚠ Build completed but executable not found!" -ForegroundColor Yellow
}

Write-Host "========================================" -ForegroundColor Cyan
