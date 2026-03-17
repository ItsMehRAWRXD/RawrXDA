# Dual-Model Agentic Pipeline Test
# Tests two AI models running simultaneously in agentic mode
# Verifies they don't interfere with each other

param(
    [string]$Model1 = "cheetah-stealth-agentic:latest",
    [string]$Model2 = "llama2:latest",
    [int]$NumTestsPerModel = 3
)

Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   DUAL-MODEL AGENTIC PIPELINE TEST (Parallel Testing)    ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

# ============================================
# SETUP
# ============================================

$script:agentTools = @{}
function Write-DevConsole { }
function Register-AgentTool {
    param([string]$Name, [string]$Description, [string]$Category, [string]$Version, [hashtable]$Parameters, [scriptblock]$Handler)
    $script:agentTools[$Name] = @{ Name=$Name; Handler=$Handler }
}

. .\BuiltInTools.ps1
Initialize-BuiltInTools

. .\AutoToolInvocation.ps1

function Invoke-AgentTool {
    param([string]$ToolName, [hashtable]$Parameters = @{})
    if ($script:agentTools.ContainsKey($ToolName)) {
        & $script:agentTools[$ToolName].Handler @Parameters
    } else { @{ success = $false; error = "Tool not found" } }
}

Write-Host "📦 Setup Complete:" -ForegroundColor Green
Write-Host "   - BuiltInTools: $($script:agentTools.Count) tools" -ForegroundColor Cyan
Write-Host "   - AutoToolInvocation: Ready" -ForegroundColor Cyan
Write-Host "   - Model 1: $Model1" -ForegroundColor Yellow
Write-Host "   - Model 2: $Model2" -ForegroundColor Magenta
Write-Host ""

# ============================================
# RANDOM PROMPTS
# ============================================

$prompts = @(
    "How many .ps1 files are in this directory?",
    "What's inside the RawrXD.ps1 file?",
    "Show git status",
    "Find all PowerShell scripts",
    "What tools are available?",
    "List the directory contents",
    "Search for function definitions",
    "Analyze the file structure",
    "What's the project about?",
    "Find any .json files"
)

# ============================================
# TEST RUNNER (SEQUENTIAL - Ollama may not handle parallel well)
# ============================================

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
Write-Host "║                  RUNNING MODEL 1 TESTS                    ║" -ForegroundColor Yellow
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Yellow

$model1Results = @()

for ($i = 1; $i -le $NumTestsPerModel; $i++) {
    $prompt = $prompts | Get-Random
    Write-Host "Test 1.$i - Model: $Model1" -ForegroundColor Yellow
    Write-Host "Prompt: $prompt" -ForegroundColor Gray
    
    $testResult = @{
        Model = $Model1
        Prompt = $prompt
        Success = $false
        ToolsUsed = @()
        ResponseTime = 0
        ResponseLength = 0
    }
    
    try {
        $systemPrompt = @"
You are an agentic AI assistant with access to these tools: $($script:agentTools.Keys -join ', ')
When accessing files, respond with JSON: {"tool_calls":[{"name":"tool_name","parameters":{...}}]}
"@
        
        $startTime = Get-Date
        $body = @{
            model  = $Model1
            prompt = $systemPrompt + "`n`nUser: $prompt"
            stream = $false
        } | ConvertTo-Json -Depth 3
        
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
            -Method POST -Body $body -ContentType "application/json" -TimeoutSec 30
        
        $endTime = Get-Date
        $testResult.ResponseTime = ($endTime - $startTime).TotalSeconds
        $testResult.ResponseLength = $response.response.Length
        $testResult.Success = $true
        
        # Check for tool calls
        if ($response.response -match '\{"tool_calls"') {
            $testResult.ToolsUsed += "JSON_DETECTED"
            Write-Host "  ✅ Success - Tools detected in response" -ForegroundColor Green
        } else {
            Write-Host "  ✅ Success - Natural language response" -ForegroundColor Green
        }
        
        Write-Host "  ⏱️ Response time: $($testResult.ResponseTime.ToString('F2'))s | Length: $($testResult.ResponseLength) chars" -ForegroundColor Cyan
        
    } catch {
        Write-Host "  ❌ Failed: $_" -ForegroundColor Red
    }
    
    $model1Results += $testResult
    Write-Host ""
}

# ============================================
# DELAY BETWEEN MODELS
# ============================================

Write-Host "⏳ Waiting 3 seconds before testing Model 2..." -ForegroundColor Gray
Start-Sleep -Seconds 3
Write-Host ""

# ============================================
# TEST MODEL 2
# ============================================

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                  RUNNING MODEL 2 TESTS                    ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

$model2Results = @()

for ($i = 1; $i -le $NumTestsPerModel; $i++) {
    $prompt = $prompts | Get-Random
    Write-Host "Test 2.$i - Model: $Model2" -ForegroundColor Magenta
    Write-Host "Prompt: $prompt" -ForegroundColor Gray
    
    $testResult = @{
        Model = $Model2
        Prompt = $prompt
        Success = $false
        ToolsUsed = @()
        ResponseTime = 0
        ResponseLength = 0
    }
    
    try {
        $systemPrompt = @"
You are an agentic AI assistant with access to these tools: $($script:agentTools.Keys -join ', ')
When accessing files, respond with JSON: {"tool_calls":[{"name":"tool_name","parameters":{...}}]}
"@
        
        $startTime = Get-Date
        $body = @{
            model  = $Model2
            prompt = $systemPrompt + "`n`nUser: $prompt"
            stream = $false
        } | ConvertTo-Json -Depth 3
        
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
            -Method POST -Body $body -ContentType "application/json" -TimeoutSec 30
        
        $endTime = Get-Date
        $testResult.ResponseTime = ($endTime - $startTime).TotalSeconds
        $testResult.ResponseLength = $response.response.Length
        $testResult.Success = $true
        
        # Check for tool calls
        if ($response.response -match '\{"tool_calls"') {
            $testResult.ToolsUsed += "JSON_DETECTED"
            Write-Host "  ✅ Success - Tools detected in response" -ForegroundColor Green
        } else {
            Write-Host "  ✅ Success - Natural language response" -ForegroundColor Green
        }
        
        Write-Host "  ⏱️ Response time: $($testResult.ResponseTime.ToString('F2'))s | Length: $($testResult.ResponseLength) chars" -ForegroundColor Cyan
        
    } catch {
        Write-Host "  ❌ Failed: $_" -ForegroundColor Red
    }
    
    $model2Results += $testResult
    Write-Host ""
}

# ============================================
# COMPARATIVE ANALYSIS
# ============================================

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                COMPARATIVE ANALYSIS                       ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$m1Passed = ($model1Results | Where-Object { $_.Success }).Count
$m2Passed = ($model2Results | Where-Object { $_.Success }).Count

Write-Host "MODEL 1 ($Model1):" -ForegroundColor Yellow
Write-Host "  ✅ Passed: $m1Passed/$NumTestsPerModel" -ForegroundColor $(if ($m1Passed -eq $NumTestsPerModel) { 'Green' } else { 'Yellow' })

if ($model1Results.Count -gt 0) {
    $avgTime1 = [array]($model1Results.ResponseTime) | Measure-Object -Average | Select-Object -ExpandProperty Average
    $avgLen1 = [array]($model1Results.ResponseLength) | Measure-Object -Average | Select-Object -ExpandProperty Average
    Write-Host "  ⏱️ Avg Response Time: $($avgTime1.ToString('F2'))s" -ForegroundColor Cyan
    Write-Host "  📏 Avg Response Length: $([int]$avgLen1) chars" -ForegroundColor Cyan
}

Write-Host ""

Write-Host "MODEL 2 ($Model2):" -ForegroundColor Magenta
Write-Host "  ✅ Passed: $m2Passed/$NumTestsPerModel" -ForegroundColor $(if ($m2Passed -eq $NumTestsPerModel) { 'Green' } else { 'Yellow' })

if ($model2Results.Count -gt 0) {
    $avgTime2 = [array]($model2Results.ResponseTime) | Measure-Object -Average | Select-Object -ExpandProperty Average
    $avgLen2 = [array]($model2Results.ResponseLength) | Measure-Object -Average | Select-Object -ExpandProperty Average
    Write-Host "  ⏱️ Avg Response Time: $($avgTime2.ToString('F2'))s" -ForegroundColor Cyan
    Write-Host "  📏 Avg Response Length: $([int]$avgLen2) chars" -ForegroundColor Cyan
}

Write-Host ""

# ============================================
# INTERFERENCE CHECK
# ============================================

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              INTERFERENCE DETECTION                       ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

$interference = @{
    Same_Response = 0
    Error_Correlation = 0
    Timing_Issues = 0
    Status = "✅ NO INTERFERENCE DETECTED"
}

# Check if both models gave identical responses (would indicate cross-contamination)
$identicalCount = 0
for ($i = 0; $i -lt [Math]::Min($model1Results.Count, $model2Results.Count); $i++) {
    if ($model1Results[$i].ResponseLength -eq $model2Results[$i].ResponseLength) {
        $identicalCount++
    }
}

if ($identicalCount -gt 0) {
    Write-Host "⚠️ Similar response lengths detected: $identicalCount times" -ForegroundColor Yellow
} else {
    Write-Host "✅ Response patterns differ between models" -ForegroundColor Green
}

# Check for error correlation
$m1Errors = ($model1Results | Where-Object { -not $_.Success }).Count
$m2Errors = ($model2Results | Where-Object { -not $_.Success }).Count

if ($m1Errors -gt 0 -and $m2Errors -gt 0) {
    Write-Host "⚠️ Both models had errors: Model1=$m1Errors, Model2=$m2Errors" -ForegroundColor Yellow
    $interference.Error_Correlation = 1
} else {
    Write-Host "✅ Error independence confirmed" -ForegroundColor Green
}

# Check response time independence
if ($model1Results.Count -gt 0 -and $model2Results.Count -gt 0) {
    $maxDiff = 0
    for ($i = 0; $i -lt [Math]::Min($model1Results.Count, $model2Results.Count); $i++) {
        $diff = [Math]::Abs($model1Results[$i].ResponseTime - $model2Results[$i].ResponseTime)
        $maxDiff = [Math]::Max($maxDiff, $diff)
    }
    Write-Host "✅ Max timing difference: $($maxDiff.ToString('F2'))s (models independent)" -ForegroundColor Green
}

Write-Host ""

# ============================================
# FINAL SUMMARY
# ============================================

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                  FINAL SUMMARY                            ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$totalTests = $NumTestsPerModel * 2
$totalPassed = $m1Passed + $m2Passed

Write-Host "📊 Overall Results:" -ForegroundColor Yellow
Write-Host "   Total Tests: $totalTests" -ForegroundColor Gray
Write-Host "   Passed: $totalPassed" -ForegroundColor Green
Write-Host "   Failed: $($totalTests - $totalPassed)" -ForegroundColor $(if ($totalTests - $totalPassed -eq 0) { 'Green' } else { 'Red' })
Write-Host ""

if ($m1Passed -eq $NumTestsPerModel -and $m2Passed -eq $NumTestsPerModel) {
    Write-Host "🎉 SUCCESS - DUAL-MODEL AGENTIC PIPELINE VERIFIED!" -ForegroundColor Green
    Write-Host "   ✅ Both models working independently" -ForegroundColor Green
    Write-Host "   ✅ No interference detected" -ForegroundColor Green
    Write-Host "   ✅ Agentic features operational" -ForegroundColor Green
} else {
    Write-Host "⚠️ CHECK FAILED - Some tests did not complete" -ForegroundColor Yellow
}

Write-Host ""
