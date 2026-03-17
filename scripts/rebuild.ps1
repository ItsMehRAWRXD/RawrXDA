# rebuild.ps1 - Clean build with Ninja, handling corrupted deps
$ErrorActionPreference = "Continue"

# Kill any stale build processes
taskkill /f /im cl.exe 2>$null
taskkill /f /im nmake.exe 2>$null  
taskkill /f /im ninja.exe 2>$null
taskkill /f /im link.exe 2>$null
Start-Sleep 2

$buildDir = "D:\rawrxd\build_ninja"

# Ensure build dir configured with Ninja
if (-not (Test-Path "$buildDir\build.ninja")) {
    Write-Host "[*] Configuring with Ninja..."
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory $buildDir -Force | Out-Null }
    Push-Location $buildDir
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl D:\rawrxd 2>&1
    Pop-Location
}

# Build
Write-Host "[*] Building RawrXD-Win32IDE with Ninja..."
Push-Location $buildDir
$result = & ninja RawrXD-Win32IDE 2>&1
$exitCode = $LASTEXITCODE
Pop-Location

# Show last 30 lines 
$lines = $result -split "`n"
if ($lines.Count -gt 30) { $result = ($lines | Select-Object -Last 30) -join "`n" }
Write-Host $result

if ($exitCode -eq 0) {
    Write-Host "`n✅ BUILD SUCCEEDED" -ForegroundColor Green
    # Copy to bin/ if needed
    $exe = Get-ChildItem "$buildDir" -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue
    if ($exe) {
        Copy-Item $exe.FullName "D:\rawrxd\bin\RawrXD-Win32IDE.exe" -Force
        Write-Host "Copied to D:\rawrxd\bin\RawrXD-Win32IDE.exe ($([math]::Round($exe.Length/1MB,1)) MB)"
    }
} else {
    Write-Host "`n❌ BUILD FAILED (exit code $exitCode)" -ForegroundColor Red
}
