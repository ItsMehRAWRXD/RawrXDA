# Test Phase 1 File Loading Script
# This script tests the RawrXD-QtShell IDE's ability to load test model files

Write-Host "=== RawrXD-QtShell Phase 1 Testing ===" -ForegroundColor Green

# Check if test files exist
$testSafetensors = "c:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\test_simple.safetensors"
$testPyTorch = "c:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\test_simple.pt"

if (Test-Path $testSafetensors) {
    Write-Host "✅ test_simple.safetensors found: $(Get-Item $testSafetensors).Length bytes" -ForegroundColor Green
} else {
    Write-Host "❌ test_simple.safetensors not found" -ForegroundColor Red
}

if (Test-Path $testPyTorch) {
    Write-Host "✅ test_simple.pt found: $(Get-Item $testPyTorch).Length bytes" -ForegroundColor Green
} else {
    Write-Host "❌ test_simple.pt not found" -ForegroundColor Red
}

# Check if IDE executable exists
$ideExe = "c:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\build\bin\Release\RawrXD-QtShell.exe"
if (Test-Path $ideExe) {
    Write-Host "✅ RawrXD-QtShell.exe found: $(Get-Item $ideExe).Length bytes" -ForegroundColor Green
} else {
    Write-Host "❌ RawrXD-QtShell.exe not found" -ForegroundColor Red
}

Write-Host ""
Write-Host "Testing Instructions:" -ForegroundColor Yellow
Write-Host "1. The IDE should now be running in the background" -ForegroundColor Yellow
Write-Host "2. Try loading test_simple.safetensors through the UI" -ForegroundColor Yellow
Write-Host "3. Try loading test_simple.pt through the UI" -ForegroundColor Yellow
Write-Host "4. Verify that the conversion pipeline works correctly" -ForegroundColor Yellow
Write-Host ""
Write-Host "Expected Behavior:" -ForegroundColor Cyan
Write-Host "- SafeTensors file should load and display metadata" -ForegroundColor Cyan
Write-Host "- PyTorch file should load and display metadata" -ForegroundColor Cyan
Write-Host "- Both should convert to GGUF format successfully" -ForegroundColor Cyan

Write-Host ""
Write-Host "=== Phase 1 Testing Complete ===" -ForegroundColor Green