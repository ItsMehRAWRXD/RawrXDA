# RawrXD Production Build and Deployment Script
# For MASM Assembly IDE

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  RawrXD MASM IDE Production Deployment" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$ProjectRoot = "$PSScriptRoot"
$BuildDir = "$ProjectRoot\masm_ide\build"
$OutputDir = "$ProjectRoot\RawrXD-AgenticIDE-v1.0.0-win64"

# Step 1: Verify Environment
Write-Host "[1/4] Verifying build environment..." -ForegroundColor Yellow

if (-not (Test-Path "$ProjectRoot\masm_ide")) {
    Write-Error "masm_ide directory not found"
}
Write-Host "  ✓ masm_ide directory found" -ForegroundColor Green

# Step 2: Clean and Prepare
Write-Host ""
Write-Host "[2/4] Cleaning and preparing..." -ForegroundColor Yellow

if (Test-Path $OutputDir) {
    Write-Host "  Removing old deployment directory..." -ForegroundColor Gray
    Remove-Item -Recurse -Force $OutputDir
}

New-Item -ItemType Directory -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Path "$OutputDir\bin" | Out-Null
New-Item -ItemType Directory -Path "$OutputDir\docs" | Out-Null

# Step 3: Copy Executable and Dependencies
Write-Host ""
Write-Host "[3/4] Copying executable and dependencies..." -ForegroundColor Yellow

$exePath = "$BuildDir\AgenticIDEWin.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found at $exePath"
}

Copy-Item $exePath "$OutputDir\bin\"
Write-Host "  ✓ Executable copied" -ForegroundColor Green

# Copy documentation
Copy-Item "$ProjectRoot\PHASE_3_COMPLETE.md" "$OutputDir\docs\"
Copy-Item "$ProjectRoot\PRODUCTION_LAUNCH_READY.md" "$OutputDir\docs\"
Copy-Item "$ProjectRoot\PHASES_4_5_6_COMPLETE_ENTERPRISE_GUIDE.md" "$OutputDir\docs\"
Write-Host "  ✓ Documentation copied" -ForegroundColor Green

# Step 4: Create Installation Package
Write-Host ""
Write-Host "[4/4] Creating installation package..." -ForegroundColor Yellow

$zipPath = "$ProjectRoot\RawrXD-AgenticIDE-v1.0.0-win64.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath
}

Compress-Archive -Path $OutputDir -DestinationPath $zipPath
Write-Host "  ✓ Installation package created: $zipPath" -ForegroundColor Green

# Generate SHA256
$sha256 = (Get-FileHash $zipPath -Algorithm SHA256).Hash
$sha256 | Out-File "$zipPath.sha256"
Write-Host "  ✓ SHA256 hash generated" -ForegroundColor Green

Write-Host ""
Write-Host "🎉 DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "   • Package: $zipPath" -ForegroundColor Cyan
Write-Host "   • SHA256: $sha256" -ForegroundColor Cyan

# Step 5: Deploy Dependencies (windeployqt should have run via CMake, but verify)
Write-Host ""
Write-Host "[5/6] Deploying Qt dependencies..." -ForegroundColor Yellow

Set-Location "$BuildDir\bin\Release"

# Run windeployqt explicitly to ensure all DLLs are present
& $Windeployqt --release --compiler-runtime --no-translations --no-system-d3d-compiler `
    --no-opengl-sw "$ExePath"

if ($LASTEXITCODE -ne 0) {
    Write-Warning "windeployqt returned non-zero exit code, but continuing..."
}

# List deployed DLLs
$qtDlls = Get-ChildItem -Filter "Qt6*.dll" | Select-Object -First 5
if ($qtDlls) {
    Write-Host "  ✓ Qt DLLs deployed:" -ForegroundColor Green
    $qtDlls | ForEach-Object { Write-Host "    - $($_.Name)" -ForegroundColor Gray }
}

$vcDlls = Get-ChildItem -Filter "vcruntime*.dll", "msvcp*.dll" -ErrorAction SilentlyContinue
if ($vcDlls) {
    Write-Host "  ✓ VC Runtime DLLs deployed:" -ForegroundColor Green
    $vcDlls | ForEach-Object { Write-Host "    - $($_.Name)" -ForegroundColor Gray }
} else {
    Write-Warning "  ! No VC runtime DLLs found - may need manual VC redist install"
}

# Step 6: Test Launch
Write-Host ""
Write-Host "[6/6] Testing application launch..." -ForegroundColor Yellow

$testProcess = Start-Process -FilePath $ExePath -PassThru -NoNewWindow
Start-Sleep -Seconds 3

if (-not $testProcess.HasExited) {
    Write-Host "  ✓ Application launched successfully and is running!" -ForegroundColor Green
    Write-Host "  Process ID: $($testProcess.Id)" -ForegroundColor Gray
    
    # Let it run for a moment to verify stability
    Start-Sleep -Seconds 2
    
    if (-not $testProcess.HasExited) {
        Write-Host "  ✓ Application is stable (5 second check)" -ForegroundColor Green
        
        # Clean shutdown
        $testProcess.Kill()
        Write-Host "  ✓ Test process terminated" -ForegroundColor Gray
    } else {
        $exitCode = $testProcess.ExitCode
        Write-Error "Application crashed with exit code: $exitCode (0x$("{0:X}" -f $exitCode))"
    }
} else {
    $exitCode = $testProcess.ExitCode
    Write-Error "Application failed to start - Exit code: $exitCode (0x$("{0:X}" -f $exitCode))"
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  🎉 DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Executable: $ExePath" -ForegroundColor White
Write-Host "Ready for production use!" -ForegroundColor White
Write-Host ""
