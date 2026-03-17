# Test Model Loading in RawrXD IDE
# This script tests the IDE's model loading capabilities

Write-Host "🧪 Testing RawrXD IDE Model Loading Functionality" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Gray

# Check if IDE is running
$ideProcess = Get-Process | Where-Object { $_.ProcessName -like "*RawrXD*" }
if ($ideProcess) {
    Write-Host "✅ IDE Process Found:" -ForegroundColor Green
    $ideProcess | ForEach-Object {
        Write-Host "   Process: $($_.ProcessName) (ID: $($_.Id))" -ForegroundColor Gray
        Write-Host "   Window: $($_.MainWindowTitle)" -ForegroundColor Gray
        Write-Host "   Memory: $([math]::Round($_.WorkingSet64/1MB, 0)) MB" -ForegroundColor Gray
    }
} else {
    Write-Host "❌ No RawrXD IDE process found" -ForegroundColor Red
    exit 1
}

# List available models for testing
Write-Host "`n📁 Available GGUF Models:" -ForegroundColor Yellow
$models = Get-ChildItem "D:\OllamaModels\" -Filter "*.gguf"
$models | ForEach-Object {
    $sizeGB = [math]::Round($_.Length/1GB, 2)
    Write-Host "   • $($_.Name) ($sizeGB GB)" -ForegroundColor White
}

Write-Host "`n📋 Test Instructions:" -ForegroundColor Cyan
Write-Host "1. In the running IDE, go to File > Load Model" -ForegroundColor Gray
Write-Host "2. Browse to: D:\OllamaModels\" -ForegroundColor Gray
Write-Host "3. Select a smaller model first (e.g., BigDaddyG-Q2_K-PRUNED-16GB.gguf - 15.81 GB)" -ForegroundColor Gray
Write-Host "4. Check the Output panel for model loading status" -ForegroundColor Gray
Write-Host "5. Look for model info display after successful loading" -ForegroundColor Gray

Write-Host "`n🎯 Recommended Test Model:" -ForegroundColor Green
$testModel = $models | Sort-Object Length | Select-Object -First 1
Write-Host "   $($testModel.Name) - Smallest model ($([math]::Round($testModel.Length/1GB, 2)) GB)" -ForegroundColor White

Write-Host "`n⚡ Press any key to continue or Ctrl+C to exit..." -ForegroundColor Yellow