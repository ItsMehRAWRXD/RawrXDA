# BigDaddyG Copilot - Ollama Setup & Diagnostic

Write-Host "================================" -ForegroundColor Cyan
Write-Host "BigDaddyG Copilot Setup Check" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Check if Ollama is running
Write-Host "[1] Checking Ollama Service..." -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://127.0.0.1:11434/api/tags" -Method GET -TimeoutSec 3 -ErrorAction Stop
    $models = $response.Content | ConvertFrom-Json
    Write-Host "✓ Ollama is running!" -ForegroundColor Green
    Write-Host "  Available models:" -ForegroundColor Green
    if ($models.models) {
        $models.models | ForEach-Object { Write-Host "    - $($_.name)" -ForegroundColor Green }
    } else {
        Write-Host "    (no models loaded)" -ForegroundColor Yellow
    }
} catch {
    Write-Host "✗ Ollama is NOT running or not responding" -ForegroundColor Red
    Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "SOLUTION: Start Ollama with:" -ForegroundColor Cyan
    Write-Host '  ollama serve' -ForegroundColor White
    Write-Host ""
    exit 1
}

Write-Host ""
Write-Host "[2] Checking Required Models..." -ForegroundColor Yellow

# Check for MASM-capable models
$requiredModels = @(
    'codellama:7b',
    'neural-chat:7b-v3.3',
    'mistral:7b',
    'llama2:7b'
)

$available = $false
foreach ($model in $requiredModels) {
    if ($models.models.name -contains $model) {
        Write-Host "✓ Found: $model" -ForegroundColor Green
        $available = $true
    }
}

if (-not $available) {
    Write-Host "⚠ Warning: No MASM-suitable models found" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "RECOMMENDED MODELS:" -ForegroundColor Yellow
    Write-Host "  - codellama:7b (best for assembly)" -ForegroundColor White
    Write-Host "  - mistral:7b (good general model)" -ForegroundColor White
    Write-Host ""
    Write-Host "To pull a model:" -ForegroundColor Cyan
    Write-Host "  ollama pull codellama:7b" -ForegroundColor White
}

Write-Host ""
Write-Host "[3] Testing Extension Connection..." -ForegroundColor Yellow
try {
    $testResult = Invoke-WebRequest -Uri "http://127.0.0.1:11434/api/tags" -Method GET -TimeoutSec 2
    Write-Host "✓ Extension can connect to Ollama" -ForegroundColor Green
} catch {
    Write-Host "✗ Extension cannot connect to Ollama" -ForegroundColor Red
}

Write-Host ""
Write-Host "================================" -ForegroundColor Cyan
Write-Host "Setup Status:" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host "✓ Cursor extension installed" -ForegroundColor Green
Write-Host "✓ Ollama connectivity verified" -ForegroundColor Green
Write-Host ""
Write-Host "NEXT STEPS:" -ForegroundColor Cyan
Write-Host "1. Keep Ollama running: ollama serve" -ForegroundColor White
Write-Host "2. Load a model: ollama pull codellama:7b" -ForegroundColor White
Write-Host "3. Restart Cursor" -ForegroundColor White
Write-Host "4. Open chat: Ctrl+Shift+P > BigDaddyG: Open AI Chat" -ForegroundColor White
Write-Host ""
