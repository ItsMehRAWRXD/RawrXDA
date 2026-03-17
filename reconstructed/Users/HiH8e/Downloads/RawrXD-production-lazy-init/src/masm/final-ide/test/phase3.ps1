# Phase 3 Compilation Test Script

$ml64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"

Write-Host "Testing Phase 3 Components..." -ForegroundColor Cyan
Write-Host ""

# Test Dialog System
Write-Host "[1/3] Testing Dialog System..." -ForegroundColor Yellow
& $ml64 /c /Fo obj\dialog_system.obj dialog_system.asm
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Dialog System compiled successfully" -ForegroundColor Green
} else {
    Write-Host "❌ Dialog System compilation failed" -ForegroundColor Red
    exit 1
}

# Test Tab Control
Write-Host "[2/3] Testing Tab Control..." -ForegroundColor Yellow
& $ml64 /c /Fo obj\tab_control.obj tab_control.asm
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Tab Control compiled successfully" -ForegroundColor Green
} else {
    Write-Host "❌ Tab Control compilation failed" -ForegroundColor Red
    exit 1
}

# Test ListView Control
Write-Host "[3/3] Testing ListView Control..." -ForegroundColor Yellow
& $ml64 /c /Fo obj\listview_control.obj listview_control.asm
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ ListView Control compiled successfully" -ForegroundColor Green
} else {
    Write-Host "❌ ListView Control compilation failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "✅ All Phase 3 components compiled successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Phase 3 Critical Blockers Implemented:" -ForegroundColor Cyan
Write-Host "  - Dialog System (modal dialog routing)" -ForegroundColor White
Write-Host "  - Tab Control (multi-tab interface)" -ForegroundColor White
Write-Host "  - ListView Control (file/model lists)" -ForegroundColor White
Write-Host ""
Write-Host "Ready for Phase 4: Settings Dialog Implementation" -ForegroundColor Cyan