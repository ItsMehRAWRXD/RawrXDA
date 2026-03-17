# Prepare-CleanBuild.ps1
# Clean build environment preparation for Qt-free RawrXD
# Removes all Qt artifacts and prepares for Windows/MSVC compilation

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         RAWRXD CLEAN BUILD PREPARATION                         ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# 1. Clean all previous builds
Write-Host "`n🧹 Cleaning previous builds..." -ForegroundColor Yellow

$buildDirs = @(
    "D:\RawrXD\build",
    "D:\RawrXD\build_win32",
    "D:\RawrXD\cmake-build-debug",
    "D:\RawrXD\cmake-build-release",
    "D:\RawrXD\out",
    "D:\RawrXD\x64",
    "D:\RawrXD\Win32"
)

foreach ($dir in $buildDirs) {
    if (Test-Path $dir) {
        Write-Host "  Removing: $dir" -ForegroundColor DarkGray
        Remove-Item -Path $dir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# 2. Clean Qt-specific files that might remain
Write-Host "`n🧹 Cleaning Qt artifacts..." -ForegroundColor Yellow

$qtArtifacts = @(
    "*.pro",
    "*.pro.user",
    "CMakeLists.txt.user",
    "moc_*.cpp",
    "moc_*.h",
    "qrc_*.cpp",
    "ui_*.h",
    "*.moc",
    ".qmake.stash",
    "Makefile*",
    ".qtc_clangd"
)

$cleanedCount = 0

Write-Host "  Scanning for artifacts (this may take a moment)..." -ForegroundColor DarkGray
# Optimized single-pass scan to prevent performance issues
$files = Get-ChildItem -Path "D:\RawrXD" -Recurse -Include $qtArtifacts -Exclude ".git",".vs",".vscode" -ErrorAction SilentlyContinue

if ($files) {
    foreach ($file in $files) {
        Write-Host "  Removing: $($file.FullName)" -ForegroundColor DarkGray
        Remove-Item $file.FullName -Force -ErrorAction SilentlyContinue
        $cleanedCount++
    }
}
Write-Host "  ✅ Cleaned $cleanedCount Qt artifact files" -ForegroundColor Green

# 3. Verify Qt path is NOT in environment
Write-Host "`n🔍 Checking environment for Qt..." -ForegroundColor Yellow

$qtEnvVars = @(
    "QTDIR",
    "QT_DIR",
    "Qt5_DIR",
    "Qt6_DIR"
)

$foundQt = $false
foreach ($var in $qtEnvVars) {
    $value = [Environment]::GetEnvironmentVariable($var)
    if ($value) {
        Write-Host "  ⚠️  Found: $var = $value" -ForegroundColor Magenta
        $foundQt = $true
    }
}

if (-not $foundQt) {
    Write-Host "  ✅ No Qt environment variables found" -ForegroundColor Green
}

# 4. Verify compiler toolchain
Write-Host "`n🔧 Verifying MSVC compiler..." -ForegroundColor Yellow

$vsInstallPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
)

$vcvarsPath = $null
foreach ($vsPath in $vsInstallPaths) {
    $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $candidate) {
        $vcvarsPath = $candidate
        break
    }
}

if ($vcvarsPath) {
    Write-Host "  ✅ MSVC found: $vcvarsPath" -ForegroundColor Green
    
    # Extract version info
    $vsVersion = Split-Path (Split-Path $vcvarsPath) -Parent | Split-Path -Parent | Split-Path -Parent | Split-Path -Leaf
    Write-Host "  Visual Studio: $vsVersion" -ForegroundColor Gray
} else {
    Write-Host "  ❌ MSVC not found! Install Visual Studio 2022." -ForegroundColor Red
    exit 1
}

# 5. Create clean build directory structure
Write-Host "`n📁 Creating build structure..." -ForegroundColor Yellow

$buildStructure = @(
    "D:\RawrXD\build_clean",
    "D:\RawrXD\build_clean\Debug",
    "D:\RawrXD\build_clean\Release",
    "D:\RawrXD\build_clean\intermediate",
    "D:\RawrXD\build_clean\logs"
)

foreach ($dir in $buildStructure) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "  Created: $dir" -ForegroundColor DarkGray
    }
}
Write-Host "  ✅ Build directories created" -ForegroundColor Green

# 6. Verify source file structure
Write-Host "`n📋 Verifying source structure..." -ForegroundColor Yellow

$srcDir = "D:\RawrXD\src"
$totalFiles = (Get-ChildItem -Path $srcDir -Recurse -Include "*.cpp", "*.h", "*.hpp" | Measure-Object).Count
Write-Host "  Source files: $totalFiles C++ files" -ForegroundColor Gray

# Check for QtReplacements.hpp
if (Test-Path "$srcDir\QtReplacements.hpp") {
    Write-Host "  ✅ QtReplacements.hpp found" -ForegroundColor Green
} else {
    Write-Host "  ❌ QtReplacements.hpp NOT FOUND!" -ForegroundColor Red
    exit 1
}

# 7. Verify CMakeLists.txt exists
Write-Host "`n📝 Checking CMake configuration..." -ForegroundColor Yellow

if (Test-Path "D:\RawrXD\CMakeLists.txt") {
    Write-Host "  ✅ CMakeLists.txt found" -ForegroundColor Green
    $cmakelists = Get-Content "D:\RawrXD\CMakeLists.txt" -Raw
    
    # Check for Qt references
    if ($cmakelists -match "find_package\s*\(\s*Qt") {
        Write-Host "  ⚠️  WARNING: CMakeLists.txt still contains Qt references!" -ForegroundColor Magenta
        Write-Host "     These need to be removed before building." -ForegroundColor Magenta
    } else {
        Write-Host "  ✅ No Qt find_package() calls found" -ForegroundColor Green
    }
} else {
    Write-Host "  ❌ CMakeLists.txt NOT FOUND!" -ForegroundColor Red
    exit 1
}

Write-Host "`n✅ Clean build environment prepared!" -ForegroundColor Green
Write-Host "`nNext steps:" -ForegroundColor Cyan
Write-Host "  1. Review CMakeLists.txt for any Qt references" -ForegroundColor White
Write-Host "  2. Run: .\Build-Clean.ps1" -ForegroundColor White
Write-Host "  3. Check: .\Verify-Build.ps1" -ForegroundColor White

Write-Host "`n" -ForegroundColor Gray
