# BigDaddyG IDE - PowerShell Launcher
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "BigDaddyG IDE - Starting Services" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptPath

# Install dependencies
Write-Host "[1/4] Installing dependencies..." -ForegroundColor Yellow
npm install
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to install dependencies" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""

# Start Backend Server
Write-Host "[2/4] Starting Backend Server..." -ForegroundColor Yellow
Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd '$scriptPath\backend'; node backend-server.js" -WindowStyle Normal

Start-Sleep -Seconds 3

# Start Orchestra Server
Write-Host "[3/4] Starting Orchestra Server..." -ForegroundColor Yellow
Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd '$scriptPath\server'; node Orchestra-Server.js" -WindowStyle Normal

Start-Sleep -Seconds 3

# Open IDE
Write-Host "[4/4] Opening IDE in browser..." -ForegroundColor Yellow
$idePath = Join-Path $scriptPath "BigDaddyG-IDE.html"
Start-Process $idePath

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "✅ All services started!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Backend Server: http://localhost:9000" -ForegroundColor Cyan
Write-Host "Orchestra Server: http://localhost:11442" -ForegroundColor Cyan
Write-Host "IDE: BigDaddyG-IDE.html" -ForegroundColor Cyan
Write-Host ""
Write-Host "Services are running in separate windows." -ForegroundColor Yellow
Write-Host "Close those windows to stop the services." -ForegroundColor Yellow
Write-Host ""
Read-Host "Press Enter to exit"

