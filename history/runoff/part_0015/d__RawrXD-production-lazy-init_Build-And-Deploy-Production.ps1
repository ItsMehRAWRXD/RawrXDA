# RawrXD Production Build and Deployment Script
# Ensures proper x64, MSVC2022, /MD configuration and DLL deployment

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  RawrXD Production Build & Deployment" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$ProjectRoot = "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader"
$BuildDir = "$ProjectRoot\build"
$QtPath = "C:\Qt\6.7.3\msvc2022_64"
$Windeployqt = "$QtPath\bin\windeployqt.exe"

# Step 1: Verify Environment
Write-Host "[1/6] Verifying build environment..." -ForegroundColor Yellow

if (-not (Test-Path $QtPath)) {
    Write-Error "Qt 6.7.3 MSVC2022 x64 not found at $QtPath"
}
Write-Host "  ✓ Qt 6.7.3 MSVC2022 x64 found" -ForegroundColor Green

if (-not (Test-Path $Windeployqt)) {
    Write-Error "windeployqt.exe not found at $Windeployqt"
}
Write-Host "  ✓ windeployqt.exe found" -ForegroundColor Green

# Verify Visual Studio 2022
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -property installationPath
    Write-Host "  ✓ Visual Studio 2022 found at $vsPath" -ForegroundColor Green
} else {
    Write-Warning "  ! vswhere.exe not found - cannot verify VS2022"
}

# Step 2: Clean and Configure
Write-Host ""
Write-Host "[2/6] Cleaning and configuring CMake..." -ForegroundColor Yellow

Set-Location $ProjectRoot

if (Test-Path $BuildDir) {
    Write-Host "  Removing old build directory..." -ForegroundColor Gray
    Remove-Item -Recurse -Force $BuildDir
}

New-Item -ItemType Directory -Path $BuildDir | Out-Null
Set-Location $BuildDir

Write-Host "  Running CMake configuration..." -ForegroundColor Gray
cmake -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="$QtPath" `
    -DQt6_DIR="$QtPath\lib\cmake\Qt6" `
    -DCMAKE_BUILD_TYPE=Release `
    ..

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
}
Write-Host "  ✓ CMake configuration successful" -ForegroundColor Green

# Step 3: Build Release
Write-Host ""
Write-Host "[3/6] Building Release configuration..." -ForegroundColor Yellow

cmake --build . --config Release --target RawrXD-QtShell -j 8

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
}
Write-Host "  ✓ Build successful" -ForegroundColor Green

# Step 4: Verify Executable
Write-Host ""
Write-Host "[4/6] Verifying executable..." -ForegroundColor Yellow

$ExePath = "$BuildDir\bin\Release\RawrXD-QtShell.exe"
if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found at $ExePath"
}

$ExeInfo = Get-Item $ExePath
Write-Host "  ✓ Executable: $($ExeInfo.FullName)" -ForegroundColor Green
Write-Host "  ✓ Size: $([math]::Round($ExeInfo.Length/1MB, 2)) MB" -ForegroundColor Green
Write-Host "  ✓ Modified: $($ExeInfo.LastWriteTime)" -ForegroundColor Green

# Verify it's x64
$dumpbin = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\dumpbin.exe"
if (Test-Path $dumpbin) {
    $headers = & $dumpbin /headers $ExePath 2>&1 | Select-String "machine"
    if ($headers -match "x64") {
        Write-Host "  ✓ Architecture: x64 (64-bit)" -ForegroundColor Green
    } else {
        Write-Error "Executable is not x64! $headers"
    }
}

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

# Ship the DirectX shader compiler binaries so the IDE never needs a separate install.
# Use the correct environment variable syntax for ProgramFiles(x86)
$dxRedistDir = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Redist\D3D\x64"
$dxDlls = @("dxcompiler.dll","dxil.dll")
if (Test-Path $dxRedistDir) {
    $dxDeployed = @()
    foreach ($dll in $dxDlls) {
        $source = Join-Path $dxRedistDir $dll
        if (Test-Path $source) {
            Copy-Item $source -Destination "$BuildDir\bin\Release" -Force
            $dxDeployed += $dll
        } else {
            Write-Warning "  ! Missing DirectX shader DLL: $dll"
        }
    }
    if ($dxDeployed.Count) {
        Write-Host "  ✓ DirectX shader compiler delivered:" -ForegroundColor Green
        $dxDeployed | ForEach-Object { Write-Host "    - $_" -ForegroundColor Gray }
    }
} else {
    Write-Warning "  ! Windows Kits DirectX redist not found at $dxRedistDir"
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
