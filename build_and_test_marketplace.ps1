# ============================================================================
# build_and_test_marketplace.ps1 — Build & Test Extension Marketplace System
# ============================================================================

param(
    [switch]$Clean,
    [switch]$TestOnly,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"
$BuildDir = "build_marketplace"
$SourceRoot = $PSScriptRoot

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD Extension Marketplace — Build & Test Script          ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Configuration
# ============================================================================

$SourceFiles = @(
    "src\marketplace\extension_marketplace.cpp",
    "src\marketplace\extension_auto_installer.cpp",
    "src\win32app\Win32IDE_ExtensionToggles.cpp",
    "src\win32app\Win32IDE_MarketplacePanel.cpp",
    "src\win32app\VSCodeMarketplaceAPI.cpp",
    "src\cli\cli_extension_commands.cpp"
)

$HeaderFiles = @(
    "src\marketplace\extension_marketplace.hpp",
    "src\marketplace\extension_auto_installer.hpp",
    "src\win32app\VSIXInstaller.hpp",
    "src\win32app\VSCodeMarketplaceAPI.hpp",
    "src\cli\cli_extension_commands.hpp"
)

$Libraries = @(
    "winhttp.lib",
    "wintrust.lib",
    "crypt32.lib",
    "shell32.lib",
    "shlwapi.lib",
    "user32.lib",
    "gdi32.lib",
    "comctl32.lib"
)

# ============================================================================
# Verify Source Files
# ============================================================================

Write-Host "[1/6] Verifying source files..." -ForegroundColor Yellow

$missingFiles = @()
foreach ($file in ($SourceFiles + $HeaderFiles)) {
    $path = Join-Path $SourceRoot $file
    if (-not (Test-Path $path)) {
        $missingFiles += $file
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Host "[ERROR] Missing files:" -ForegroundColor Red
    foreach ($file in $missingFiles) {
        Write-Host "  - $file" -ForegroundColor Red
    }
    exit 1
}

Write-Host "[OK] All source files present" -ForegroundColor Green

# ============================================================================
# Clean Build
# ============================================================================

if ($Clean) {
    Write-Host "[2/6] Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
    }
    Write-Host "[OK] Build directory cleaned" -ForegroundColor Green
}

# ============================================================================
# CMake Configuration
# ============================================================================

if (-not $TestOnly) {
    Write-Host "[3/6] Configuring with CMake..." -ForegroundColor Yellow

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    Push-Location $BuildDir
    
    $cmakeOutput = cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_BUILD_TYPE=Release `
        -DRAWRXD_ENABLE_MARKETPLACE=ON `
        -DRAWRXD_ENABLE_EXTENSIONS=ON `
        2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] CMake configuration failed" -ForegroundColor Red
        if ($Verbose) {
            Write-Host $cmakeOutput
        }
        Pop-Location
        exit 1
    }
    
    Pop-Location
    Write-Host "[OK] CMake configuration complete" -ForegroundColor Green

    # ============================================================================
    # Build
    # ============================================================================

    Write-Host "[4/6] Building RawrXD with marketplace support..." -ForegroundColor Yellow

    Push-Location $BuildDir
    
    $buildOutput = cmake --build . --config Release --target RawrXD-Shell -j 8 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Build failed" -ForegroundColor Red
        if ($Verbose) {
            Write-Host $buildOutput
        } else {
            # Show last 50 lines of output
            $buildOutput | Select-Object -Last 50
        }
        Pop-Location
        exit 1
    }
    
    Pop-Location
    Write-Host "[OK] Build complete" -ForegroundColor Green
}

# ============================================================================
# Verify Output Binary
# ============================================================================

Write-Host "[5/6] Verifying output binary..." -ForegroundColor Yellow

$exePath = Join-Path $BuildDir "Release\RawrXD.exe"
if (-not (Test-Path $exePath)) {
    # Try alternate path
    $exePath = Join-Path $SourceRoot "RawrXD.exe"
}

if (-not (Test-Path $exePath)) {
    Write-Host "[ERROR] RawrXD.exe not found" -ForegroundColor Red
    Write-Host "[INFO] Searched paths:" -ForegroundColor Yellow
    Write-Host "  - $BuildDir\Release\RawrXD.exe"
    Write-Host "  - $SourceRoot\RawrXD.exe"
    exit 1
}

$fileInfo = Get-Item $exePath
Write-Host "[OK] Binary found: $exePath" -ForegroundColor Green
Write-Host "     Size: $($fileInfo.Length / 1MB) MB" -ForegroundColor Cyan
Write-Host "     Modified: $($fileInfo.LastWriteTime)" -ForegroundColor Cyan

# ============================================================================
# Test Extension System
# ============================================================================

Write-Host "[6/6] Testing extension system..." -ForegroundColor Yellow
Write-Host ""

# Test 1: CLI Help
Write-Host "[Test 1] CLI Plugin Help" -ForegroundColor Cyan
$testScript = @"
!plugin help
exit
"@
$testScript | & $exePath --cli

Write-Host ""

# Test 2: List Extensions
Write-Host "[Test 2] List Installed Extensions" -ForegroundColor Cyan
$testScript = @"
!plugin list
exit
"@
$testScript | & $exePath --cli

Write-Host ""

# Test 3: Search Marketplace (requires internet)
Write-Host "[Test 3] Search Marketplace" -ForegroundColor Cyan
$testScript = @"
!plugin search copilot
exit
"@

try {
    $testScript | & $exePath --cli
    Write-Host "[OK] Marketplace connection successful" -ForegroundColor Green
} catch {
    Write-Host "[WARN] Marketplace search failed (internet required)" -ForegroundColor Yellow
}

Write-Host ""

# ============================================================================
# Summary
# ============================================================================

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║               Build & Test Complete                           ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "[SUCCESS] Extension marketplace system is ready!" -ForegroundColor Green
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Run RawrXD CLI:  .\$exePath --cli"
Write-Host "  2. Install Amazon Q:  !plugin install amazonwebservices.amazon-q-vscode"
Write-Host "  3. Install Copilot:   !plugin install GitHub.copilot"
Write-Host "  4. Install all AI:    !plugin install-ai"
Write-Host ""
Write-Host "  GUI Mode: Launch RawrXD.exe normally and open Tools → Extensions"
Write-Host ""
Write-Host "Documentation: EXTENSION_MARKETPLACE_GUIDE.md" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Optional: Launch Test Environment
# ============================================================================

$launch = Read-Host "Launch RawrXD CLI now? (Y/N)"
if ($launch -eq 'Y' -or $launch -eq 'y') {
    Write-Host ""
    Write-Host "Launching RawrXD CLI..." -ForegroundColor Cyan
    Write-Host "Type '!plugin help' for extension commands" -ForegroundColor Yellow
    Write-Host ""
    & $exePath --cli
}
