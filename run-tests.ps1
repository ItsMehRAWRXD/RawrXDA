# ============================================================
# RawrXD Test Runner Script
# ============================================================
# Automatically sets up Qt environment and runs tests with proper configuration
# Usage:
#   .\run-tests.ps1                    # Run all tests
#   .\run-tests.ps1 -Filter "smoke"    # Run specific tests (e.g., smoke tests)
#   .\run-tests.ps1 -Verbose            # Show detailed output
#   .\run-tests.ps1 -BuildFirst        # Build tests before running

param(
    [string]$Filter = "",
    [switch]$Verbose = $false,
    [switch]$BuildFirst = $false,
    [string]$BuildDir = "build"
)

# Configuration
$QtPath = "C:\Qt\6.7.3\msvc2022_64\bin"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildPath = Join-Path $scriptDir $BuildDir

# Colors
$success = "Green"
$error = "Red"
$warning = "Yellow"
$info = "Cyan"

function Write-Header([string]$text) {
    Write-Host "`n$('=' * 60)" -ForegroundColor $info
    Write-Host $text -ForegroundColor $info
    Write-Host "$('=' * 60)`n" -ForegroundColor $info
}

function Write-Success([string]$text) {
    Write-Host "✓ $text" -ForegroundColor $success
}

function Write-Error-Custom([string]$text) {
    Write-Host "✗ $text" -ForegroundColor $error
}

function Write-Warning-Custom([string]$text) {
    Write-Host "⚠ $text" -ForegroundColor $warning
}

# Main script
Write-Header "RawrXD Test Runner"

# Verify build directory exists
if (!(Test-Path $buildPath)) {
    Write-Error-Custom "Build directory not found: $buildPath"
    Write-Host "Please run: cmake -DRAWRXD_BUILD_TESTS=ON -B build" -ForegroundColor $warning
    exit 1
}

cd $buildPath

# Set Qt environment
Write-Host "Setting Qt environment..." -ForegroundColor $info
$env:PATH = "$QtPath;$env:PATH"
Write-Success "Qt 6.7.3 added to PATH"

# Build tests if requested
if ($BuildFirst) {
    Write-Header "Building Test Targets"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    
    cmake --build . --config Release --target test_agent_coordinator quant_correctness_tests test_gguf_loader --parallel 8 2>&1 | `
        Select-String "error|warning|Built" | Select-Object -Last 10
    
    $sw.Stop()
    Write-Success "Build time: $($sw.Elapsed.TotalSeconds)s"
    Write-Host ""
}

# Prepare ctest command
$ctestCmd = "ctest -C Release --output-on-failure"
if ($Filter) {
    $ctestCmd += " -R `"$Filter`""
}
if ($Verbose) {
    $ctestCmd += " -V"
}

# Run tests
Write-Header "Running Tests"
Write-Host "Command: $ctestCmd`n" -ForegroundColor $info

$sw = [Diagnostics.Stopwatch]::StartNew()
$result = Invoke-Expression $ctestCmd
$sw.Stop()

# Parse results
if ($LASTEXITCODE -eq 0) {
    Write-Success "All tests passed!"
} else {
    Write-Warning-Custom "Some tests failed (exit code: $LASTEXITCODE)"
}

Write-Host "`nTest execution time: $($sw.Elapsed.TotalSeconds)s" -ForegroundColor $info

# Summary
Write-Header "Test Summary"
$testOutput = ctest -C Release -N 2>&1 | Select-String "Total Tests"
Write-Host $testOutput -ForegroundColor $info

# Show common commands
Write-Header "Common Commands"
Write-Host "Run all smoke tests:              .\run-tests.ps1 -Filter 'Smoke'" -ForegroundColor $info
Write-Host "Run agent coordinator tests:      .\run-tests.ps1 -Filter 'coordinator'" -ForegroundColor $info
Write-Host "Build and run tests:              .\run-tests.ps1 -BuildFirst" -ForegroundColor $info
Write-Host "Run specific test (verbose):      .\run-tests.ps1 -Filter 'test_name' -Verbose" -ForegroundColor $info
Write-Host "Run quant tests:                  .\run-tests.ps1 -Filter 'quant'" -ForegroundColor $info
Write-Host ""

exit $LASTEXITCODE
