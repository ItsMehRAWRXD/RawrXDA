# AV-Sense Setup Script for PowerShell
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   AV-Sense Setup Script" -ForegroundColor Cyan
Write-Host "   Private AV Scanning Service" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if Node.js is installed
Write-Host "[1/5] Checking Node.js installation..." -ForegroundColor Yellow
try {
  $nodeVersion = node --version
  Write-Host "Node.js found: $nodeVersion" -ForegroundColor Green
}
catch {
  Write-Host "ERROR: Node.js is not installed!" -ForegroundColor Red
  Write-Host "Please install Node.js from https://nodejs.org/" -ForegroundColor Red
  pause
  exit 1
}
Write-Host ""

# Install dependencies
Write-Host "[2/5] Installing dependencies..." -ForegroundColor Yellow
npm install
if ($LASTEXITCODE -ne 0) {
  Write-Host "ERROR: Failed to install dependencies!" -ForegroundColor Red
  pause
  exit 1
}
Write-Host "Dependencies installed successfully" -ForegroundColor Green
Write-Host ""

# Setup environment
Write-Host "[3/5] Setting up environment..." -ForegroundColor Yellow
if (-not (Test-Path ".env")) {
  Copy-Item ".env.example" ".env"
  Write-Host "Created .env file from template" -ForegroundColor Green
  Write-Host "IMPORTANT: Edit .env and set JWT_SECRET to a random value!" -ForegroundColor Yellow
}
else {
  Write-Host ".env file already exists" -ForegroundColor Green
}
Write-Host ""

# Create directories
Write-Host "[4/5] Creating directories..." -ForegroundColor Yellow
$directories = @("uploads", "database", "backend\reports")
foreach ($dir in $directories) {
  if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    Write-Host "Created: $dir" -ForegroundColor Green
  }
  else {
    Write-Host "Exists: $dir" -ForegroundColor Gray
  }
}
Write-Host ""

# Initialize database
Write-Host "[5/5] Initializing database..." -ForegroundColor Yellow
node database/init.js
if ($LASTEXITCODE -ne 0) {
  Write-Host "ERROR: Failed to initialize database!" -ForegroundColor Red
  pause
  exit 1
}
Write-Host "Database initialized successfully" -ForegroundColor Green
Write-Host ""

# Complete
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Setup Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Edit .env file and set a strong JWT_SECRET"
Write-Host "2. (Optional) Set TELEGRAM_BOT_TOKEN if using Telegram bot"
Write-Host "3. Run: npm start"
Write-Host "4. Open: http://localhost:3000"
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Ask if user wants to start the server
$response = Read-Host "Would you like to start the server now? (y/n)"
if ($response -eq 'y' -or $response -eq 'Y') {
  Write-Host ""
  Write-Host "Starting AV-Sense server..." -ForegroundColor Green
  npm start
}
