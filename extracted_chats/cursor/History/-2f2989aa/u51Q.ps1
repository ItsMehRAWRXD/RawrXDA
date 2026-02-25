# Start Ollama GUI with Local Server
# This fixes CORS issues by running a local web server

Write-Host "`n🚀 Starting Ollama AI Interface...`n" -ForegroundColor Cyan

# Check if Ollama is running
Write-Host "📡 Checking Ollama status..." -ForegroundColor Yellow
$ollamaRunning = Get-Process -Name "ollama*" -ErrorAction SilentlyContinue

if (-not $ollamaRunning) {
    Write-Host "⚡ Starting Ollama server..." -ForegroundColor Yellow
    Start-Process -FilePath "D:\MyCoPilot-Complete-Portable\ollama\ollama.exe" -ArgumentList "serve" -WindowStyle Hidden
    Start-Sleep -Seconds 3
}

Write-Host "✅ Ollama is running on localhost:11434" -ForegroundColor Green

# Start a simple Python HTTP server
Write-Host "`n🌐 Starting local web server..." -ForegroundColor Yellow
Write-Host "   Open: http://localhost:8000/REAL-AI-Connected.html" -ForegroundColor Cyan
Write-Host "`n⚠️  Press Ctrl+C to stop the server`n" -ForegroundColor Yellow

Set-Location "D:\Security Research aka GitHub Repos"

# Try Python 3 first, then Python 2
if (Get-Command python -ErrorAction SilentlyContinue) {
    python -m http.server 8000
} elseif (Get-Command python3 -ErrorAction SilentlyContinue) {
    python3 -m http.server 8000
} else {
    Write-Host "❌ Python not found. Opening with Chrome flags instead..." -ForegroundColor Red
    Write-Host "`n💡 Alternative: Opening Chrome with CORS disabled..." -ForegroundColor Yellow
    
    $chromePath = "C:\Program Files\Google\Chrome\Application\chrome.exe"
    if (Test-Path $chromePath) {
        Start-Process $chromePath -ArgumentList "--disable-web-security --user-data-dir=C:\temp\chrome-temp file:///D:/Security%20Research%20aka%20GitHub%20Repos/REAL-AI-Connected.html"
        Write-Host "✅ Opened in Chrome with CORS disabled" -ForegroundColor Green
    } else {
        Write-Host "❌ Chrome not found. Please:" -ForegroundColor Red
        Write-Host "   1. Install Python" -ForegroundColor White
        Write-Host "   2. Or run Chrome with: --disable-web-security" -ForegroundColor White
    }
}

