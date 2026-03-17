Write-Host "Testing RawrXD /review command..." -ForegroundColor Green
Write-Host ""

# Test 1: Check if Ollama is accessible
Write-Host "1. Testing Ollama connectivity..." -ForegroundColor Yellow
try {
  $response = Invoke-RestMethod -Uri http://localhost:11434/api/tags -Method GET -TimeoutSec 5
  Write-Host "   ✓ Ollama is running with $($response.models.Count) models" -ForegroundColor Green
    
  # Show available models
  Write-Host "   Available models:" -ForegroundColor Cyan
  foreach ($model in $response.models) {
    Write-Host "     - $($model.name)" -ForegroundColor Gray
  }
}
catch {
  Write-Host "   ✗ Ollama not accessible: $_" -ForegroundColor Red
  Write-Host "     Please ensure Ollama is running: ollama serve" -ForegroundColor Yellow
  exit 1
}

Write-Host ""

# Test 2: Verify test file exists
$testFile = "c:\Users\HiH8e\OneDrive\Desktop\test_code_for_review.ps1"
if (Test-Path $testFile) {
  Write-Host "2. Test file created: $testFile" -ForegroundColor Green
  $content = Get-Content $testFile -Raw
  Write-Host "   File size: $($content.Length) characters" -ForegroundColor Cyan
}
else {
  Write-Host "2. ✗ Test file not found: $testFile" -ForegroundColor Red
  exit 1
}

Write-Host ""

# Test 3: Quick model test
Write-Host "3. Testing model response..." -ForegroundColor Yellow
try {
  $testRequest = @{
    model  = "bigdaddyg-fast:latest"
    prompt = "Please respond with exactly: 'Model working correctly'"
    stream = $false
  } | ConvertTo-Json
    
  $testResponse = Invoke-RestMethod -Uri http://localhost:11434/api/generate -Method POST -Body $testRequest -ContentType 'application/json'
    
  if ($testResponse.response) {
    Write-Host "   ✓ Model responded: $($testResponse.response.Trim())" -ForegroundColor Green
  }
  else {
    Write-Host "   ✗ Empty model response" -ForegroundColor Red
  }
}
catch {
  Write-Host "   ✗ Model test failed: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "INSTRUCTIONS FOR TESTING /review COMMAND:" -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host ""
Write-Host "1. Launch RawrXD:" -ForegroundColor Yellow
Write-Host "   cd 'c:\Users\HiH8e\OneDrive\Desktop\Powershield'" -ForegroundColor Gray
Write-Host "   .\RawrXD.ps1" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Open the test file in the editor:" -ForegroundColor Yellow
Write-Host "   - Click File > Open" -ForegroundColor Gray
Write-Host "   - Browse to: $testFile" -ForegroundColor Gray
Write-Host "   - Open the file (should see PowerShell code with issues)" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Make sure Agent Mode is ON:" -ForegroundColor Yellow
Write-Host "   - Check the menu bar shows 'Agent Mode: ON' in green" -ForegroundColor Gray
Write-Host "   - If not, click the toggle to enable it" -ForegroundColor Gray
Write-Host ""
Write-Host "4. Test the /review command:" -ForegroundColor Yellow
Write-Host "   - Go to the Chat tab" -ForegroundColor Gray
Write-Host "   - Type: /review" -ForegroundColor Gray
Write-Host "   - Press Enter" -ForegroundColor Gray
Write-Host ""
Write-Host "5. Expected behavior:" -ForegroundColor Yellow
Write-Host "   - Should show: 'Reviewing code (XXX characters, YY lines)...'" -ForegroundColor Gray
Write-Host "   - Then provide detailed code review with issues found" -ForegroundColor Gray
Write-Host "   - Should identify problems like missing error handling, poor naming, etc." -ForegroundColor Gray
Write-Host ""
Write-Host "6. Check Dev Tools tab for debugging info" -ForegroundColor Yellow
Write-Host ""
Write-Host "Ready to test! Launch RawrXD now." -ForegroundColor Green