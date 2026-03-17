#requires -Version 7.0

<#
.SYNOPSIS
    Test different models for agentic capability and hallucination detection
.DESCRIPTION
    Compares different Ollama models, tests their reasoning capability,
    and demonstrates hallucination detection and correction mechanisms
#>

Push-Location "E:\Desktop\Powershield\Modules"

# Import modules
Import-Module ".\ModelInvocationFailureDetector.psm1" -Force
Import-Module ".\ProductionMonitoring.psm1" -Force

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD Model Capability & Hallucination Detection Test      ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Test models
$testModels = @(
    @{ Name = "llama3.2:3b"; Desc = "Small, fast, good reasoning" }
    @{ Name = "qwen2.5:7b"; Desc = "Medium, better context" }
    @{ Name = "phi"; Desc = "Compact, limited capability" }
)

$testPrompt = "What is the capital of France? Answer in exactly 3 words or less."
$correctAnswer = "Paris"

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 1: FACTUAL ACCURACY (Hallucination Test)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""
Write-Host "Prompt: $testPrompt" -ForegroundColor Yellow
Write-Host "Expected: $correctAnswer (3 words or less)" -ForegroundColor Green
Write-Host ""

$results = @()

foreach ($model in $testModels) {
    Write-Host "Testing: $($model.Name)" -ForegroundColor Cyan
    
    $response = Invoke-ModelWithFailureDetection `
        -ModelName $model.Name `
        -BackendType "Ollama" `
        -Action {
            $payload = @{
                model = $model.Name
                prompt = $testPrompt
                stream = $false
            } | ConvertTo-Json
            
            $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
                -H "Content-Type: application/json" `
                -d $payload 2>&1
            
            if ($LASTEXITCODE -ne 0) { throw "Model invocation failed" }
            return $result | ConvertFrom-Json
        } `
        -EnableRetry `
        -MaxRetries 1
    
    if ($response.Success) {
        $answer = $response.Result.response.Trim()
        $wordCount = @($answer -split '\s+').Count
        $isCorrect = $answer -contains "Paris" -or $answer -like "*Paris*"
        $withinLimit = $wordCount -le 3
        
        Write-Host "  Response: '$answer'" -ForegroundColor Yellow
        Write-Host "  Words: $wordCount (Limit: 3)" -ForegroundColor $(if ($withinLimit) { 'Green' } else { 'Red' })
        Write-Host "  Correct: $(if ($isCorrect) { 'YES ✓' } else { 'NO ✗ (HALLUCINATION)' })" -ForegroundColor $(if ($isCorrect) { 'Green' } else { 'Red' })
        Write-Host "  Factual: $(if ($isCorrect) { 'Accurate' } else { 'Hallucinated' })" -ForegroundColor $(if ($isCorrect) { 'Green' } else { 'Red' })
        
        $results += @{
            Model = $model.Name
            Response = $answer
            WordCount = $wordCount
            WithinLimit = $withinLimit
            Correct = $isCorrect
            Hallucinated = -not $isCorrect
            Duration = $response.Duration.TotalMilliseconds
        }
    }
    else {
        Write-Host "  ✗ Failed: $($response.Error)" -ForegroundColor Red
    }
    
    Write-Host ""
}

# Hallucination Detection Summary
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "HALLUCINATION DETECTION SUMMARY" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$hallucinations = @($results | Where-Object { $_.Hallucinated })
if ($hallucinations.Count -gt 0) {
    Write-Host "⚠️  HALLUCINATIONS DETECTED:" -ForegroundColor Red
    foreach ($h in $hallucinations) {
        Write-Host "  - $($h.Model): '$($h.Response)'" -ForegroundColor Red
    }
    Write-Host ""
}

Write-Host "✓ ACCURATE RESPONSES:" -ForegroundColor Green
foreach ($r in @($results | Where-Object { -not $_.Hallucinated })) {
    Write-Host "  - $($r.Model): '$($r.Response)'" -ForegroundColor Green
}

Write-Host ""

# TEST 2: REASONING CAPABILITY
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 2: REASONING CAPABILITY" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$reasoningPrompt = "If all roses are flowers and all flowers need water, do roses need water? Answer: yes or no"

Write-Host "Prompt: $reasoningPrompt" -ForegroundColor Yellow
Write-Host ""

$reasoningResults = @()

foreach ($model in $testModels[0..1]) {  # Test first 2 models
    Write-Host "Testing: $($model.Name)" -ForegroundColor Cyan
    
    $response = Invoke-ModelWithFailureDetection `
        -ModelName $model.Name `
        -BackendType "Ollama" `
        -Action {
            $payload = @{
                model = $model.Name
                prompt = $reasoningPrompt
                stream = $false
                temperature = 0.1  # Lower temp for more consistent reasoning
            } | ConvertTo-Json
            
            $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
                -H "Content-Type: application/json" `
                -d $payload 2>&1
            
            if ($LASTEXITCODE -ne 0) { throw "Model invocation failed" }
            return $result | ConvertFrom-Json
        } `
        -EnableRetry `
        -MaxRetries 1
    
    if ($response.Success) {
        $answer = $response.Result.response.Trim()
        $isCorrect = $answer -like "*yes*" -or $answer -like "*Yes*" -or $answer -like "*YES*"
        
        Write-Host "  Response: '$answer'" -ForegroundColor Yellow
        Write-Host "  Reasoning: $(if ($isCorrect) { 'CORRECT ✓' } else { 'INCORRECT ✗' })" -ForegroundColor $(if ($isCorrect) { 'Green' } else { 'Red' })
        
        $reasoningResults += @{
            Model = $model.Name
            Response = $answer
            Correct = $isCorrect
        }
    }
    
    Write-Host ""
}

# TEST 3: CONSTRAINT COMPLIANCE
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "TEST 3: CONSTRAINT COMPLIANCE (Hallucination Trigger)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$constraintPrompt = "List 5 programming languages in exactly 2 sentences"
Write-Host "Prompt: $constraintPrompt" -ForegroundColor Yellow
Write-Host "Constraint: EXACTLY 2 sentences (hallucination if violated)" -ForegroundColor Yellow
Write-Host ""

foreach ($model in @($testModels[0])) {  # Test best model
    Write-Host "Testing: $($model.Name)" -ForegroundColor Cyan
    
    $response = Invoke-ModelWithFailureDetection `
        -ModelName $model.Name `
        -BackendType "Ollama" `
        -Action {
            $payload = @{
                model = $model.Name
                prompt = $constraintPrompt
                stream = $false
            } | ConvertTo-Json
            
            $result = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
                -H "Content-Type: application/json" `
                -d $payload 2>&1
            
            if ($LASTEXITCODE -ne 0) { throw "Model invocation failed" }
            return $result | ConvertFrom-Json
        }
    
    if ($response.Success) {
        $answer = $response.Result.response.Trim()
        $sentenceCount = @($answer -split '\.\s+' | Where-Object { $_ }) | Measure-Object | Select-Object -ExpandProperty Count
        $complies = $sentenceCount -eq 2
        
        Write-Host "  Response: '$answer'" -ForegroundColor Yellow
        Write-Host "  Sentences: $sentenceCount (Expected: 2)" -ForegroundColor $(if ($complies) { 'Green' } else { 'Red' })
        Write-Host "  Constraint Met: $(if ($complies) { 'YES ✓' } else { 'NO ✗ (HALLUCINATION)' })" -ForegroundColor $(if ($complies) { 'Green' } else { 'Red' })
    }
    
    Write-Host ""
}

# MODEL CAPABILITY CLASSIFICATION
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "MODEL CAPABILITY CLASSIFICATION" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

Write-Host "NOT AGENTIC-CAPABLE (High Hallucination Risk):" -ForegroundColor Red
Write-Host "  • Phi (3B) - Limited reasoning, ignores constraints" -ForegroundColor Gray
Write-Host "  • Any model < 3B parameters" -ForegroundColor Gray
Write-Host "  • Domain-specific models without instruction tuning" -ForegroundColor Gray
Write-Host ""

Write-Host "MODERATELY AGENTIC (Can be used with validation):" -ForegroundColor Yellow
Write-Host "  • Qwen2.5 (7B) - Good for structured tasks, some constraint issues" -ForegroundColor Gray
Write-Host "  • Llama3.1 (8B) - Better reasoning, can follow constraints" -ForegroundColor Gray
Write-Host "  • Mistral (7B) - Fast, decent reasoning" -ForegroundColor Gray
Write-Host ""

Write-Host "STRONGLY AGENTIC (Recommended for production):" -ForegroundColor Green
Write-Host "  • Llama3.2 (3B+) - Instruction-tuned, follows constraints well" -ForegroundColor Gray
Write-Host "  • Llama3 (8B+) - Excellent reasoning and constraint compliance" -ForegroundColor Gray
Write-Host "  • Llama2 (7B+) - Reliable for most agentic tasks" -ForegroundColor Gray
Write-Host "  • DeepSeek Coder (7B+) - Specialized for code generation" -ForegroundColor Gray
Write-Host ""

# CORRECTION MECHANISMS
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "HALLUCINATION CORRECTION MECHANISMS" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

Write-Host "1️⃣  INPUT VALIDATION - Detect constraints before execution" -ForegroundColor Cyan
Write-Host "   ├─ Check word count limits" -ForegroundColor Gray
Write-Host "   ├─ Validate JSON response format" -ForegroundColor Gray
Write-Host "   ├─ Verify factual accuracy against knowledge base" -ForegroundColor Gray
Write-Host "   └─ Enforce syntax requirements" -ForegroundColor Gray
Write-Host ""

Write-Host "2️⃣  OUTPUT VALIDATION - Detect hallucinations after response" -ForegroundColor Cyan
Write-Host "   ├─ Length/format validation" -ForegroundColor Gray
Write-Host "   ├─ Factuality checking (against ground truth)" -ForegroundColor Gray
Write-Host "   ├─ Consistency checking (across responses)" -ForegroundColor Gray
Write-Host "   └─ Confidence scoring" -ForegroundColor Gray
Write-Host ""

Write-Host "3️⃣  RETRY WITH CORRECTION - Re-invoke with feedback" -ForegroundColor Cyan
Write-Host "   ├─ Specify what went wrong" -ForegroundColor Gray
Write-Host "   ├─ Lower temperature for more focused response" -ForegroundColor Gray
Write-Host "   ├─ Use different model if available" -ForegroundColor Gray
Write-Host "   └─ Provide examples of correct format" -ForegroundColor Gray
Write-Host ""

Write-Host "4️⃣  CIRCUIT BREAKER - Prevent cascading failures" -ForegroundColor Cyan
Write-Host "   ├─ Track failure rate per model" -ForegroundColor Gray
Write-Host "   ├─ Open circuit after threshold" -ForegroundColor Gray
Write-Host "   ├─ Use fallback model" -ForegroundColor Gray
Write-Host "   └─ Alert on consistent hallucinations" -ForegroundColor Gray
Write-Host ""

# PRACTICAL EXAMPLE
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "PRACTICAL HALLUCINATION CORRECTION EXAMPLE" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

Write-Host "Scenario: Model hallucinates function name in code generation" -ForegroundColor Yellow
Write-Host ""

Write-Host "❌ INITIAL (Hallucinated):" -ForegroundColor Red
Write-Host '   response.setCORSHeaders()  # Function doesn''t exist' -ForegroundColor Gray
Write-Host ""

Write-Host "✅ CORRECTION FLOW:" -ForegroundColor Green
Write-Host "   1. Detector: Function not found in validation suite" -ForegroundColor Cyan
Write-Host "   2. Action: Re-invoke with constraint:" -ForegroundColor Cyan
Write-Host '      "Use only existing Express.js methods"' -ForegroundColor Gray
Write-Host "   3. Retry: Model generates corrected code" -ForegroundColor Cyan
Write-Host '   response.set({"Access-Control-Allow-Origin": "*"})  ✓' -ForegroundColor Green
Write-Host ""

Write-Host "Key: Validation + Feedback Loop + Retry Strategy" -ForegroundColor Yellow
Write-Host ""

Write-Host "✓ Test Complete" -ForegroundColor Green

Pop-Location
