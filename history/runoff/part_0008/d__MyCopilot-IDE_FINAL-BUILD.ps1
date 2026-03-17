# Final Build Script for MyCopilot IDE
# Ensures we stay in the correct directory throughout the build

$ErrorActionPreference = "Stop"
$projectRoot = "D:\MyCopilot-IDE"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MyCopilot IDE - Final Build"  -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Change to project directory
Set-Location $projectRoot
Write-Host "✓ Working directory: $projectRoot" -ForegroundColor Green

# Verify package.json exists
if (-not (Test-Path "package.json")) {
    Write-Error "package.json not found!"
    exit 1
}
Write-Host "✓ package.json found" -ForegroundColor Green

# Verify node_modules exists
if (-not (Test-Path "node_modules")) {
    Write-Host "Installing dependencies..." -ForegroundColor Yellow
    npm install
}
Write-Host "✓ Dependencies installed" -ForegroundColor Green

# Build the Electron app
Write-Host ""
Write-Host "Building Electron app to EXE..." -ForegroundColor Yellow
Write-Host "This may take a few minutes..." -ForegroundColor Gray
Write-Host ""

npx electron-builder --win --x64

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  ✓ BUILD SUCCESSFUL!"  -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Your EXE is ready in the build folder!" -ForegroundColor Cyan
    Write-Host "Look for: build-*/win-unpacked/MyCopilot-IDE.exe" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ✗ BUILD FAILED"  -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
