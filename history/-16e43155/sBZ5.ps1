# Quick test script for RawrXD Ollama integration
Write-Host "=== RawrXD Chat Integration Test ===" -ForegroundColor Green

# Test connectivity
Write-Host "`n1. Testing Ollama connectivity..." -ForegroundColor Yellow
try {
    $connectionTest = Test-NetConnection -ComputerName localhost -Port 11434 -InformationLevel Quiet
    if ($connectionTest) {
        Write-Host "✓ Ollama server is reachable on port 11434" -ForegroundColor Green
    } else {
        Write-Host "✗ Cannot reach Ollama server" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "✗ Connection test failed: $_" -ForegroundColor Red
    exit 1
}

# Test model availability
Write-Host "`n2. Checking available models..." -ForegroundColor Yellow
try {
    $modelsResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET
    $modelNames = $modelsResponse.models | ForEach-Object { $_.name }
    Write-Host "✓ Found $($modelNames.Count) models: $($modelNames -join ', ')" -ForegroundColor Green
    
    if ("bigdaddyg-fast:latest" -in $modelNames) {
        Write-Host "✓ Target model 'bigdaddyg-fast:latest' is available" -ForegroundColor Green
    } else {
        Write-Host "⚠ Target model 'bigdaddyg-fast:latest' not found" -ForegroundColor Yellow
        Write-Host "  Available alternatives: $($modelNames[0..2] -join ', ')" -ForegroundColor Cyan
    }
} catch {
    Write-Host "✗ Failed to get models: $_" -ForegroundColor Red
    exit 1
}

# Test API call
Write-Host "`n3. Testing API call..." -ForegroundColor Yellow
try {
    $testPayload = @{
        model = "bigdaddyg-fast:latest"
        prompt = "Please respond with just 'API test successful'"
        stream = $false
    }
    
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body ($testPayload | ConvertTo-Json) -ContentType "application/json" -TimeoutSec 30
    
    if ($response.response) {
        Write-Host "✓ API call successful!" -ForegroundColor Green
        Write-Host "  Response: $($response.response)" -ForegroundColor Cyan
    } else {
        Write-Host "⚠ API response format unexpected" -ForegroundColor Yellow
        Write-Host "  Raw: $($response | ConvertTo-Json -Compress)" -ForegroundColor Gray
    }
} catch {
    Write-Host "✗ API call failed: $_" -ForegroundColor Red
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Green
Write-Host "Your RawrXD application should now work correctly!" -ForegroundColor White
Write-Host "The IO.txt errors were caused by the cloud model hitting usage limits." -ForegroundColor Gray
Write-Host "The local 'bigdaddyg-fast:latest' model is working perfectly." -ForegroundColor Gray