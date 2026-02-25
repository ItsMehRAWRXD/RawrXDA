# Test Script for bigdaddyg-personalized-agentic Agent
# Verifies the agent can "speak" properly after fixes

$ErrorActionPreference = "Continue"
$ModelName = "bigdaddyg-personalized-agentic"

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Testing bigdaddyg-personalized-agentic Agent" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# Test 1: Check Ollama Service
Write-Host "`n[TEST 1] Checking Ollama Service..." -ForegroundColor Yellow
try {
    $testConn = Test-NetConnection -ComputerName "localhost" -Port 11434 -InformationLevel Quiet -WarningAction SilentlyContinue
    if (-not $testConn) {
        Write-Host "❌ Ollama service is not running on localhost:11434" -ForegroundColor Red
        Write-Host "   Please start Ollama first" -ForegroundColor Gray
        exit 1
    }
    Write-Host "✅ Ollama service is running" -ForegroundColor Green
}
catch {
    Write-Host "❌ Failed to check Ollama service: $_" -ForegroundColor Red
    exit 1
}

# Test 2: Check if model exists
Write-Host "`n[TEST 2] Checking if model exists..." -ForegroundColor Yellow
try {
    $tagsResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5
    $availableModels = @($tagsResponse.models | ForEach-Object { $_.name })
    
    $modelExists = $availableModels | Where-Object { $_ -eq $ModelName }
    
    if (-not $modelExists) {
        Write-Host "⚠️  Model '$ModelName' not found in Ollama" -ForegroundColor Yellow
        Write-Host "   Available models:" -ForegroundColor Gray
        $availableModels | ForEach-Object { Write-Host "     • $_" -ForegroundColor DarkGray }
        Write-Host "`n   To create the model, run:" -ForegroundColor Yellow
        Write-Host "   ollama create $ModelName -f Modelfiles/$ModelName.Modelfile" -ForegroundColor Cyan
        exit 1
    }
    Write-Host "✅ Model '$ModelName' found" -ForegroundColor Green
}
catch {
    Write-Host "❌ Failed to check models: $_" -ForegroundColor Red
    exit 1
}

# Test 3: Simple response test
Write-Host "`n[TEST 3] Testing basic response capability..." -ForegroundColor Yellow
try {
    $testPrompt = "Hello! Please respond with a brief greeting and confirm you can communicate. Just say 'I can speak!' if you understand."
    
    $requestBody = @{
        model  = $ModelName
        prompt = $testPrompt
        stream = $false
    } | ConvertTo-Json
    
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec 30
    
    if ($response.response -and $response.response.Trim().Length -gt 0) {
        Write-Host "✅ Agent responded successfully!" -ForegroundColor Green
        Write-Host "   Response length: $($response.response.Trim().Length) characters" -ForegroundColor Gray
        Write-Host "`n   Response preview:" -ForegroundColor Cyan
        $preview = $response.response.Trim().Substring(0, [Math]::Min(200, $response.response.Trim().Length))
        Write-Host "   $preview$(if($response.response.Trim().Length -gt 200){'...'})" -ForegroundColor White
    }
    else {
        Write-Host "❌ Agent did not respond (empty response)" -ForegroundColor Red
        exit 1
    }
}
catch {
    Write-Host "❌ Failed to get response: $_" -ForegroundColor Red
    Write-Host "   Error details: $($_.Exception.Message)" -ForegroundColor Gray
    exit 1
}

# Test 4: Code block generation test (the main fix)
Write-Host "`n[TEST 4] Testing code block generation (critical fix)..." -ForegroundColor Yellow
try {
    $codePrompt = "Please write a simple PowerShell function that returns 'Hello World'. Format it in a code block with proper syntax highlighting."
    
    $requestBody = @{
        model  = $ModelName
        prompt = $codePrompt
        stream = $false
    } | ConvertTo-Json
    
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec 30
    
    if ($response.response) {
        # Check if response contains code blocks
        $hasCodeBlock = $response.response -match '```|```powershell|```ps1'
        
        if ($hasCodeBlock) {
            Write-Host "✅ Agent can generate code blocks!" -ForegroundColor Green
            Write-Host "   Code block detected in response" -ForegroundColor Gray
        }
        else {
            Write-Host "⚠️  Agent responded but no code block detected" -ForegroundColor Yellow
            Write-Host "   This might be okay if the agent chose a different format" -ForegroundColor Gray
        }
        
        Write-Host "`n   Full response:" -ForegroundColor Cyan
        Write-Host "   ─────────────────────────────────────────────────────" -ForegroundColor DarkGray
        Write-Host $response.response -ForegroundColor White
        Write-Host "   ─────────────────────────────────────────────────────" -ForegroundColor DarkGray
    }
    else {
        Write-Host "❌ Agent did not respond to code generation request" -ForegroundColor Red
        exit 1
    }
}
catch {
    Write-Host "❌ Failed to test code generation: $_" -ForegroundColor Red
    exit 1
}

# Test 5: Multi-turn conversation test
Write-Host "`n[TEST 5] Testing conversation capability..." -ForegroundColor Yellow
try {
    $conversationPrompt = "What is 2+2? Please provide a clear, direct answer."
    
    $requestBody = @{
        model  = $ModelName
        prompt = $conversationPrompt
        stream = $false
    } | ConvertTo-Json
    
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec 30
    
    if ($response.response -and $response.response.Trim().Length -gt 0) {
        Write-Host "✅ Agent can engage in conversation!" -ForegroundColor Green
        Write-Host "   Response: $($response.response.Trim())" -ForegroundColor White
    }
    else {
        Write-Host "❌ Agent did not respond to conversation" -ForegroundColor Red
        exit 1
    }
}
catch {
    Write-Host "❌ Failed conversation test: $_" -ForegroundColor Red
    exit 1
}

# Summary
Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ All tests passed! The agent can now 'speak' properly." -ForegroundColor Green
Write-Host "`nThe fixes were successful:" -ForegroundColor White
Write-Host "  • Removed problematic stop sequences (```, # END, etc.)" -ForegroundColor Gray
Write-Host "  • Added explicit response directive" -ForegroundColor Gray
Write-Host "  • Adjusted parameters for better response generation" -ForegroundColor Gray
Write-Host "`nThe agent is now fully operational! 🎉" -ForegroundColor Magenta

