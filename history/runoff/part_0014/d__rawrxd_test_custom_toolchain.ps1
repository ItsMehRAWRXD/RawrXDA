# Test Custom Toolchain Configuration
Write-Host "=== Testing RawrXD Custom Toolchain ===" -ForegroundColor Cyan

# Clean build directory
if (Test-Path "d:\rawrxd\build") {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "d:\rawrxd\build"
}

# Configure with custom toolchain
Write-Host "Configuring with custom toolchain (NO SDK)..." -ForegroundColor Yellow
cmake -S "d:\rawrxd" -B "d:\rawrxd\build" -DRAWRXD_USE_CUSTOM_TOOLCHAIN=ON

Write-Host "`n=== Configuration Complete ===" -ForegroundColor Green
Write-Host "Custom compilers:" -ForegroundColor Cyan
Write-Host "  C/C++: d:\rawrxd\compilers\universal_cross_platform_compiler.exe"
Write-Host "  MASM:  d:\rawrxd\compilers\masm_ide\ml64.exe"
Write-Host "  Status: No SDK dependencies required"
