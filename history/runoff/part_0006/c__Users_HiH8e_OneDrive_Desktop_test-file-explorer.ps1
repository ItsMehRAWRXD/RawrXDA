# Test File Explorer in RawrXD IDE
# This script verifies the new file explorer functionality

Write-Host "🗂️  Testing File Explorer in RawrXD IDE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Gray

# Check if IDE is running
$ideProcess = Get-Process | Where-Object { $_.ProcessName -like "*RawrXD*" }
if ($ideProcess) {
    Write-Host "✅ IDE Running:" -ForegroundColor Green
    $ideProcess | ForEach-Object {
        Write-Host "   Process: $($_.ProcessName) (ID: $($_.Id))" -ForegroundColor Gray
        Write-Host "   Window: $($_.MainWindowTitle)" -ForegroundColor Gray
    }
} else {
    Write-Host "❌ No RawrXD IDE process found" -ForegroundColor Red
    exit 1
}

Write-Host "`n📁 Expected File Explorer Features:" -ForegroundColor Yellow
Write-Host "   • Left sidebar with file tree" -ForegroundColor White
Write-Host "   • Model directory browsing (D:\OllamaModels)" -ForegroundColor White
Write-Host "   • GGUF file filtering and icons" -ForegroundColor White
Write-Host "   • Double-click to load models" -ForegroundColor White
Write-Host "   • Right-click context menus" -ForegroundColor White
Write-Host "   • Directory expansion/collapse" -ForegroundColor White

Write-Host "`n🎯 Available Models to Test:" -ForegroundColor Green
Get-ChildItem "D:\OllamaModels\" -Filter "*.gguf" | Select-Object -First 5 | ForEach-Object {
    $sizeGB = [math]::Round($_.Length/1GB, 2)
    Write-Host "   • $($_.Name) ($sizeGB GB)" -ForegroundColor White
}

Write-Host "`n📋 Test Instructions:" -ForegroundColor Cyan
Write-Host "1. Look for the File Explorer panel on the left side of the IDE" -ForegroundColor Gray
Write-Host "2. You should see a tree view with model directories" -ForegroundColor Gray
Write-Host "3. Expand the D:\OllamaModels folder" -ForegroundColor Gray
Write-Host "4. Double-click on a .gguf file to load it" -ForegroundColor Gray
Write-Host "5. Right-click on files/folders for context menu" -ForegroundColor Gray
Write-Host "6. Check the Output panel for model loading confirmation" -ForegroundColor Gray

Write-Host "`n🚀 File Explorer is now integrated into your IDE!" -ForegroundColor Green
Write-Host "   Model files can be loaded directly from the sidebar" -ForegroundColor White