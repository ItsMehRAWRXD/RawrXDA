# Quick verification script - no cmake needed
$ErrorActionPreference = "Stop"

Write-Host "=== COMPILER VERIFICATION ===" -ForegroundColor Cyan

# Find dumpbin
$dumpbin = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio" -Recurse -Filter "dumpbin.exe" -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match "x64\\dumpbin.exe" } | Select-Object -First 1

if ($dumpbin) {
    Write-Host "`n✓ Dumpbin: $($dumpbin.FullName)" -ForegroundColor Green
    
    # Check IDE dependencies
    if (Test-Path "d:\rawrxd\Ship\RawrXD_Win32_IDE.exe") {
        Write-Host "`n=== IDE DEPENDENCIES ===" -ForegroundColor Cyan
        & $dumpbin.FullName /DEPENDENTS "d:\rawrxd\Ship\RawrXD_Win32_IDE.exe" | Select-String -Pattern "\.dll" | ForEach-Object { $_.Line.Trim() }
    }
} else {
    Write-Host "✗ dumpbin not found" -ForegroundColor Red
}

# Find cl.exe
$cl = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio" -Recurse -Filter "cl.exe" -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq "x64" } | Select-Object -First 1

if ($cl) {
    Write-Host "`n✓ Compiler: $($cl.FullName)" -ForegroundColor Green
    Write-Host "✓ Version:" -ForegroundColor Green
    & $cl.FullName 2>&1 | Select-Object -First 1
}

# Check MASM CLI
Write-Host "`n=== MASM COMPILER ===" -ForegroundColor Cyan
$nasm = Get-Command nasm -ErrorAction SilentlyContinue
if ($nasm) {
    Write-Host "✓ NASM: $($nasm.Source)" -ForegroundColor Green
    & nasm -version
} else {
    Write-Host "✗ NASM not in PATH" -ForegroundColor Yellow
}

# Check model
Write-Host "`n=== MODEL STATUS ===" -ForegroundColor Cyan
$model = "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
if (Test-Path $model) {
    $size = (Get-Item $model).Length / 1GB
    Write-Host "✓ Model: $model" -ForegroundColor Green
    Write-Host "  Size: $([math]::Round($size, 2)) GB" -ForegroundColor Gray
} else {
    Write-Host "✗ Model not found: $model" -ForegroundColor Red
}
