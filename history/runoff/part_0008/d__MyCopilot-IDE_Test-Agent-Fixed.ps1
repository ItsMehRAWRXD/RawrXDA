# Fixed test script using Invoke-Expression to load classes
$moduleContent = Get-Content "$PSScriptRoot\UnifiedAgentProcessor-Fixed.psm1" -Raw
Invoke-Expression $moduleContent

Write-Host "=== Testing Fixed UnifiedAgentProcessor ===" -ForegroundColor Cyan

# Test 1: Create instance
Write-Host "`n1. Creating UnifiedAgentProcessor instance..." -ForegroundColor Yellow
try {
    $processor = [UnifiedAgentProcessor]::new()
    Write-Host "✓ Instance created: $($processor.GetProcessorName())" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to create instance: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Test 2: Code generation
Write-Host "`n2. Testing Code Generation..." -ForegroundColor Yellow
$response = $processor.ProcessRequest("generate code for fibonacci")
Write-Host "Request: generate code for fibonacci"
Write-Host "Success: $($response.Success)"
Write-Host "Response: $($response.Content)" -ForegroundColor Green

# Test 3: IDE processing
Write-Host "`n3. Testing IDE Processing..." -ForegroundColor Yellow
$response = $processor.ProcessRequest("search all files")
Write-Host "Request: search all files"
Write-Host "Success: $($response.Success)"
Write-Host "Response: $($response.Content)" -ForegroundColor Green

# Test 4: Todo management
Write-Host "`n4. Testing Todo Management..." -ForegroundColor Yellow
$response = $processor.ProcessRequest("todo review code")
Write-Host "Request: todo review code"
Write-Host "Success: $($response.Success)"
Write-Host "Response: $($response.Content)" -ForegroundColor Green

# Test 5: No matching processor
Write-Host "`n5. Testing No Matching Processor..." -ForegroundColor Yellow
$response = $processor.ProcessRequest("what is 2+2")
Write-Host "Request: what is 2+2"
Write-Host "Success: $($response.Success)"
if (-not $response.Success) {
    Write-Host "Error (expected): $($response.Error)" -ForegroundColor Yellow
}

# Test 6: Capabilities
Write-Host "`n6. Testing Capabilities..." -ForegroundColor Yellow
$capabilities = $processor.GetCapabilities()
$capabilities.GetEnumerator() | ForEach-Object {
    Write-Host "  $($_.Key): $($_.Value)" -ForegroundColor Cyan
}

Write-Host "`n✓ All tests completed!" -ForegroundColor Green