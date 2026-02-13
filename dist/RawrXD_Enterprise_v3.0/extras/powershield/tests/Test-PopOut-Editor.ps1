# Test Open-FileInPopOutEditor function
Write-Host "🧪 Testing Open-FileInPopOutEditor function..." -ForegroundColor Cyan

# Load RawrXD functions
Write-Host "📚 Loading RawrXD functions..." -ForegroundColor Yellow
. ".\RawrXD.ps1" 2>$null

# Test file path
$testFilePath = Join-Path $PSScriptRoot "test-file.txt"
Write-Host "🎯 Testing with file: $testFilePath" -ForegroundColor Cyan

# Check if file exists
if (-not (Test-Path $testFilePath)) {
    Write-Host "❌ Test file not found" -ForegroundColor Red
    exit
}

# Read file content manually
$content = Get-Content $testFilePath -Raw
Write-Host "📖 Manual file read: $($content.Length) characters" -ForegroundColor Green

# Try to call Open-FileInPopOutEditor
Write-Host "🚀 Calling Open-FileInPopOutEditor..." -ForegroundColor Cyan
try {
    Open-FileInPopOutEditor -FilePath $testFilePath
    Write-Host "✅ Open-FileInPopOutEditor completed" -ForegroundColor Green
} catch {
    Write-Host "❌ Open-FileInPopOutEditor failed: $_" -ForegroundColor Red
}

Write-Host "🎯 Test complete. Check if pop-out editor opened with content." -ForegroundColor Cyan