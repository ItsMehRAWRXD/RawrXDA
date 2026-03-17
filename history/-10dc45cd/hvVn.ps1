# RawrXD Production Deployment Script
# Follows AI Toolkit Production Readiness Instructions for Deployment and Isolation

$ErrorActionPreference = "Stop"

# Configuration
$QtPath = "C:\Qt\6.7.3\msvc2022_64"
$QtBin = "$QtPath\bin"
$BuildDir = "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build"
$ReleaseBin = "$BuildDir\bin\Release"
$Executable = "$ReleaseBin\RawrXD-QtShell.exe"
$Windeployqt = "$QtBin\windeployqt.exe"

Write-Host "🚀 Starting RawrXD Production Deployment..." -ForegroundColor Cyan

# 1. Verify Environment
if (-not (Test-Path $Windeployqt)) {
    Write-Error "❌ windeployqt.exe not found at $Windeployqt"
}
Write-Host "✓ Found Qt tools" -ForegroundColor Green

# 2. Ensure Build Exists (Assuming previous build success)
if (-not (Test-Path $Executable)) {
    Write-Error "❌ Executable not found at $Executable. Please build Release target first."
}
Write-Host "✓ Found executable" -ForegroundColor Green

# 3. Run windeployqt to resolve dependencies
Write-Host "📦 Running windeployqt to gather dependencies..." -ForegroundColor Yellow
& $Windeployqt --release --compiler-runtime --no-translations --no-opengl-sw $Executable

if ($LASTEXITCODE -ne 0) {
    Write-Error "❌ windeployqt failed with exit code $LASTEXITCODE"
}
Write-Host "✓ Dependencies deployed" -ForegroundColor Green

# 4. Create qt.conf for explicit plugin loading
$QtConfPath = "$ReleaseBin\qt.conf"
$QtConfContent = @"
[Paths]
Plugins=.
"@
Set-Content -Path $QtConfPath -Value $QtConfContent
Write-Host "✓ Created qt.conf" -ForegroundColor Green

# 5. Copy additional runtime assets if needed (e.g., Vulkan, GGML models)
# (Add specific copy commands here if needed)

Write-Host "✅ Deployment Complete!" -ForegroundColor Green
Write-Host "📂 Output Directory: $ReleaseBin" -ForegroundColor Cyan

# 6. Test Run (Optional)
Write-Host "🧪 Attempting dry-run..." -ForegroundColor Yellow
$ProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
$ProcessInfo.FileName = $Executable
$ProcessInfo.WorkingDirectory = $ReleaseBin
$ProcessInfo.RedirectStandardOutput = $true
$ProcessInfo.RedirectStandardError = $true
$ProcessInfo.UseShellExecute = $false
$ProcessInfo.CreateNoWindow = $true

try {
    $Process = [System.Diagnostics.Process]::Start($ProcessInfo)
    Start-Sleep -Seconds 3
    if (-not $Process.HasExited) {
        Write-Host "✓ Application started and stayed running (3s check)" -ForegroundColor Green
        $Process.Kill()
    } else {
        $ExitCode = $Process.ExitCode
        Write-Host "⚠️ Application exited immediately with code: $ExitCode" -ForegroundColor Red
        Write-Host "Stdout: $($Process.StandardOutput.ReadToEnd())"
        Write-Host "Stderr: $($Process.StandardError.ReadToEnd())"
    }
} catch {
    Write-Error "❌ Failed to launch application: $_"
}
