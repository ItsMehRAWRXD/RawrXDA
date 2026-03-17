# Build Scalar Agentic IDE
# Pure scalar autonomous IDE with integrated server, file browser, chat, and agent

param(
    [string]$BuildType = "Release"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Building Scalar Agentic IDE          " -ForegroundColor Cyan
Write-Host "  100% Scalar | No Threading | No GPU  " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-agentic"

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

Set-Location $BuildDir

Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -S $ProjectRoot `
    -B . `
    -C "$ProjectRoot/CMakeLists-agentic.txt"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Building..." -ForegroundColor Yellow
cmake --build . --config $BuildType --target RawrXD-Agentic-IDE

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Build Successful!                    " -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

$ExePath = Join-Path $BuildDir "$BuildType\RawrXD-Agentic-IDE.exe"

if (Test-Path $ExePath) {
    Write-Host "Executable: $ExePath" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Features:" -ForegroundColor Yellow
    Write-Host "  ✓ Integrated HTTP/WebSocket Server (port 8080)" -ForegroundColor Green
    Write-Host "  ✓ Recursive File Browser" -ForegroundColor Green
    Write-Host "  ✓ Two-Way AI Chat Interface" -ForegroundColor Green
    Write-Host "  ✓ Autonomous Agentic Engine" -ForegroundColor Green
    Write-Host "  ✓ 100% Scalar Operations" -ForegroundColor Green
    Write-Host "  ✓ No Threading/GPU/SIMD" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run with:" -ForegroundColor Yellow
    Write-Host "  $ExePath" -ForegroundColor Cyan
    Write-Host "  or" -ForegroundColor Gray
    Write-Host "  $ExePath `"C:\Your\Project\Path`"" -ForegroundColor Cyan
    Write-Host ""
    
    # Offer to run
    $run = Read-Host "Run IDE now? (y/n)"
    if ($run -eq 'y') {
        Write-Host ""
        Write-Host "Starting Scalar Agentic IDE..." -ForegroundColor Green
        & $ExePath "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
    }
} else {
    Write-Host "Warning: Executable not found at expected location" -ForegroundColor Yellow
}

Set-Location $ProjectRoot
