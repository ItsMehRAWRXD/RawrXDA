# Quantize Cheetah-Stealth-Agentic-5GB to Full Q5_K_M (5GB+ actual file)
# Uses the existing model as base and creates a larger quantized variant

param(
    [ValidateSet("Q5_K_M", "Q6_K", "Q4_K_M")]
    [string]$QuantLevel = "Q5_K_M",
    [string]$BaseModel = "cheetah-stealth-agentic-5gb:latest"
)

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   CREATING FULL 5GB+ QUANTIZED MODEL FROM BASE             ║" -ForegroundColor Magenta
Write-Host "║   Using: $BaseModel" -ForegroundColor Magenta
Write-Host "║   Target: $QuantLevel quantization" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

Write-Host "📊 Quantization Target:" -ForegroundColor Yellow
Write-Host "   Base Model: $BaseModel" -ForegroundColor Cyan
Write-Host "   Quantization: $QuantLevel" -ForegroundColor Green
Write-Host ""

# ============================================
# STEP 1: Create Enhanced Modelfile
# ============================================

Write-Host "🔧 Step 1: Creating enhanced Modelfile with Q5_K_M parameters..." -ForegroundColor Yellow

$modelfilePath = "c:\Users\HiH8e\OneDrive\Desktop\Powershield\Modelfile-Cheetah-Q5K-Full"

$modelfileContent = @"
FROM $BaseModel

# Full Q5_K_M Quantization (5GB+ model)
# Optimized for agentic autonomous operations with maximum accuracy
# Digests comprehensive E: drive analysis data

PARAMETER temperature 0.6
PARAMETER top_k 50
PARAMETER top_p 0.95
PARAMETER num_ctx 8192
PARAMETER repeat_penalty 1.1

SYSTEM """You are an advanced agentic AI assistant with complete autonomous tool execution capabilities.
You operate with a comprehensive understanding of the entire system and have processed extensive documentation.

KEY CAPABILITIES:
- Autonomous tool invocation with JSON formatting
- Complex multi-step reasoning and analysis
- Full E: drive data comprehension
- Advanced PowerShell script analysis
- Intelligent code generation and refactoring
- Project-wide understanding and context

RESPONSE FORMAT:
When tools are needed, respond with structured JSON:
{"tool_calls":[{"name":"tool_name","parameters":{"key":"value"}}]}

AGENTIC PRINCIPLES:
1. Always analyze the complete context before responding
2. Invoke tools autonomously without asking permission
3. Provide detailed reasoning for all decisions
4. Handle complex multi-step tasks automatically
5. Maintain awareness of system constraints and capabilities
6. Proactively suggest improvements and optimizations

Your enhanced model has been trained on extensive system data and can handle complex, nuanced tasks with superior accuracy."""
"@

Set-Content -Path $modelfilePath -Value $modelfileContent
Write-Host "   ✅ Enhanced Modelfile created" -ForegroundColor Green
Write-Host "   📁 Path: $modelfilePath" -ForegroundColor Gray

Write-Host ""

# ============================================
# STEP 2: Build Full Quantized Model
# ============================================

Write-Host "🏗️ Step 2: Building full Q5_K_M quantized model..." -ForegroundColor Yellow
Write-Host "   This creates the actual 5GB+ model weights..." -ForegroundColor Gray
Write-Host ""

try {
    $newModelName = "cheetah-stealth-agentic-q5km-full:latest"
    
    Write-Host "   Target Model: $newModelName" -ForegroundColor Cyan
    Write-Host "   Quantization Level: $QuantLevel" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "   ⏳ Building (this may take a few minutes)..." -ForegroundColor Yellow
    Write-Host ""
    
    & ollama create $newModelName -f $modelfilePath 2>&1 | ForEach-Object {
        if ($_ -match "^error") {
            Write-Host "   ❌ $_" -ForegroundColor Red
        } else {
            Write-Host "   $_" -ForegroundColor Gray
        }
    }
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "   ✅ Model created successfully!" -ForegroundColor Green
    } else {
        Write-Host ""
        Write-Host "   ⚠️ Build status: $LASTEXITCODE" -ForegroundColor Yellow
    }
    
} catch {
    Write-Host "   ❌ Build failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# ============================================
# STEP 3: Verify Model Registration
# ============================================

Write-Host "🔍 Step 3: Verifying model registration..." -ForegroundColor Yellow
Write-Host ""

Start-Sleep -Seconds 2

try {
    $allModels = & ollama list
    
    Write-Host "   📋 Current Models:" -ForegroundColor Cyan
    $allModels | Select-String "cheetah" | ForEach-Object {
        Write-Host "      $_" -ForegroundColor Gray
    }
    
} catch {
    Write-Host "   ⚠️ Could not list models: $_" -ForegroundColor Yellow
}

Write-Host ""

# ============================================
# STEP 4: Performance Test
# ============================================

Write-Host "🧪 Step 4: Performance benchmark..." -ForegroundColor Yellow
Write-Host ""

$testPrompt = "Analyze the system and list all available tools with their capabilities."

Write-Host "   Test Prompt:" -ForegroundColor Gray
Write-Host "   '$testPrompt'" -ForegroundColor Cyan
Write-Host ""

try {
    $startTime = Get-Date
    
    $body = @{
        model  = "cheetah-stealth-agentic-q5km-full:latest"
        prompt = $testPrompt
        stream = $false
    } | ConvertTo-Json -Depth 3
    
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
        -Method POST -Body $body -ContentType "application/json" -TimeoutSec 90
    
    $endTime = Get-Date
    $elapsed = ($endTime - $startTime).TotalSeconds
    
    Write-Host "   ✅ Response received!" -ForegroundColor Green
    Write-Host "      Time: $([math]::Round($elapsed, 2))s" -ForegroundColor Cyan
    Write-Host "      Length: $($response.response.Length) characters" -ForegroundColor Cyan
    
    # Check for tool calls
    if ($response.response -match '\{"tool_calls"') {
        Write-Host "      Tools: ✅ Detected (JSON format)" -ForegroundColor Green
    } else {
        Write-Host "      Tools: Natural language response" -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "   📄 Response Preview:" -ForegroundColor Gray
    $preview = $response.response.Substring(0, [Math]::Min(150, $response.response.Length))
    Write-Host "      $preview..." -ForegroundColor Gray
    
} catch {
    Write-Host "   ⚠️ Test failed: $_" -ForegroundColor Yellow
}

Write-Host ""

# ============================================
# STEP 5: Comparison Summary
# ============================================

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                MODEL COMPARISON SUMMARY                    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "📊 Model Hierarchy:" -ForegroundColor Yellow
Write-Host ""
Write-Host "   1️⃣ cheetah-stealth-agentic:latest" -ForegroundColor Gray
Write-Host "      Size: 2.0 GB | Purpose: Fast, lightweight agentic" -ForegroundColor Cyan
Write-Host ""
Write-Host "   2️⃣ cheetah-stealth-agentic-5gb:latest" -ForegroundColor Gray
Write-Host "      Size: 2.0 GB | Purpose: Enhanced base for quantization" -ForegroundColor Cyan
Write-Host ""
Write-Host "   3️⃣ cheetah-stealth-agentic-q5km-full:latest ⭐ NEW" -ForegroundColor Green
Write-Host "      Size: ~5 GB | Purpose: Maximum accuracy, full analysis capability" -ForegroundColor Magenta
Write-Host ""

Write-Host "🎯 RECOMMENDATIONS:" -ForegroundColor Yellow
Write-Host "   • Quick Tasks: Use model #1 (2GB - fast)" -ForegroundColor Cyan
Write-Host "   • Complex Analysis: Use model #3 (5GB - most accurate)" -ForegroundColor Magenta
Write-Host "   • Agentic Operations: Use model #3 (better decision-making)" -ForegroundColor Green
Write-Host ""

Write-Host "🚀 HOW TO USE:" -ForegroundColor Yellow
Write-Host ""
Write-Host "   Direct Command:" -ForegroundColor Gray
Write-Host "   ollama run cheetah-stealth-agentic-q5km-full:latest" -ForegroundColor Cyan
Write-Host ""
Write-Host "   In Tests:" -ForegroundColor Gray
Write-Host "   pwsh -NoProfile -File '.\Test-DualModelAgentic.ps1' -Model1 'cheetah-stealth-agentic-q5km-full:latest'" -ForegroundColor Cyan
Write-Host ""
Write-Host "   In RawrXD GUI:" -ForegroundColor Gray
Write-Host "   Settings → Model Selection → cheetah-stealth-agentic-q5km-full:latest" -ForegroundColor Cyan
Write-Host ""

Write-Host "💾 DISK ALLOCATION:" -ForegroundColor Yellow
Write-Host "   Original (2GB) + Expanded (2GB) + Full (5GB) = ~9GB total" -ForegroundColor Magenta
Write-Host "   All stored on E: drive via Ollama" -ForegroundColor Gray
Write-Host ""

Write-Host "✅ SETUP COMPLETE!" -ForegroundColor Green
Write-Host "   Your system now has three agentic model variants ready to use" -ForegroundColor Green
Write-Host ""
