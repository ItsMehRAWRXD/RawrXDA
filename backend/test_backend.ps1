# Test script for MASM Backend
# Run after starting masm_backend.exe

Write-Host "`n=== RawrXD MASM Backend Test ===" -ForegroundColor Cyan
Write-Host ""

$baseUrl = "http://localhost:8080"

# Test 1: GET /models
Write-Host "Test 1: GET /models" -ForegroundColor Yellow
try {
    $response = Invoke-RestMethod -Uri "$baseUrl/models" -Method GET -TimeoutSec 5
    Write-Host "  [PASS] Response received" -ForegroundColor Green
    Write-Host "  Models: $($response.models | ConvertTo-Json -Compress)" -ForegroundColor Gray
} catch {
    Write-Host "  [FAIL] $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""

# Test 2: POST /ask
Write-Host "Test 2: POST /ask" -ForegroundColor Yellow
try {
    $body = @{
        question = "Hello MASM backend!"
        model = "rawrxd-7b"
        language = "English"
    } | ConvertTo-Json
    
    $response = Invoke-RestMethod -Uri "$baseUrl/ask" -Method POST -Body $body -ContentType "application/json" -TimeoutSec 5
    Write-Host "  [PASS] Response received" -ForegroundColor Green
    Write-Host "  Answer: $($response.answer)" -ForegroundColor Gray
} catch {
    Write-Host "  [FAIL] $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""

# Test 3: OPTIONS (CORS preflight)
Write-Host "Test 3: OPTIONS /ask (CORS)" -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "$baseUrl/ask" -Method OPTIONS -TimeoutSec 5
    if ($response.Headers["Access-Control-Allow-Origin"] -eq "*") {
        Write-Host "  [PASS] CORS headers present" -ForegroundColor Green
    } else {
        Write-Host "  [WARN] CORS header missing" -ForegroundColor Yellow
    }
} catch {
    Write-Host "  [FAIL] $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""

# Test 4: 404 for unknown route
Write-Host "Test 4: GET /unknown (404)" -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "$baseUrl/unknown" -Method GET -TimeoutSec 5
    Write-Host "  [FAIL] Should have returned 404" -ForegroundColor Red
} catch {
    if ($_.Exception.Response.StatusCode -eq 404) {
        Write-Host "  [PASS] Got 404 as expected" -ForegroundColor Green
    } else {
        Write-Host "  [INFO] $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=== Tests Complete ===" -ForegroundColor Cyan
Write-Host ""
