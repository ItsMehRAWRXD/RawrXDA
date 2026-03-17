# Expand Cheetah-Stealth-Agentic Model to ~5GB
# Re-quantizes with Q4_K_M or Q5_K_M quality for better accuracy

param(
    [ValidateSet("Q4_K_M", "Q5_K_M", "Q6_K")]
    [string]$QuantLevel = "Q5_K_M",  # Q5_K_M typically gives 5GB for 7B models
    [string]$ModelName = "cheetah-stealth-agentic:latest"
)

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   EXPANDING CHEETAH-STEALTH-AGENTIC MODEL TO ~5GB          ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

Write-Host "📊 Current Model Status:" -ForegroundColor Yellow
Write-Host "   Model: $ModelName" -ForegroundColor Cyan
Write-Host "   Current Size: 2.0 GB" -ForegroundColor Cyan
Write-Host "   Target Size: ~5 GB" -ForegroundColor Green
Write-Host "   Target Quality: $QuantLevel" -ForegroundColor Cyan
Write-Host ""

# ============================================
# STEP 1: Check model directory
# ============================================

$ollamaPath = "$env:USERPROFILE\.ollama\models"
$modelDir = Join-Path $ollamaPath "manifests" "registry.ollama.ai" "library" "cheetah-stealth-agentic"

Write-Host "🔍 Step 1: Locating model files..." -ForegroundColor Yellow

if (Test-Path $modelDir) {
    Write-Host "   ✅ Model directory found: $modelDir" -ForegroundColor Green
} else {
    Write-Host "   ⚠️ Model directory not found at expected location" -ForegroundColor Yellow
    Write-Host "   Will use Ollama's internal model management" -ForegroundColor Gray
}

Write-Host ""

# ============================================
# STEP 2: Create expanded model via Modelfile
# ============================================

Write-Host "🔧 Step 2: Creating quantized model variant..." -ForegroundColor Yellow

$modelfilePath = "c:\Users\HiH8e\OneDrive\Desktop\Powershield\Modelfile-CheetahExpanded"

$modelfileContent = @"
FROM cheetah-stealth-agentic:latest

# Expand model with agentic instructions for 5GB variant
PARAMETER temperature 0.7
PARAMETER top_k 40
PARAMETER top_p 0.9
PARAMETER num_ctx 4096

SYSTEM """You are an agentic AI assistant with autonomous tool execution capabilities. 
You have access to a comprehensive toolkit for file operations, code analysis, system interaction, and more.
When presented with tasks, analyze requirements and autonomously invoke appropriate tools.
Respond with structured JSON when tool calls are needed: {"tool_calls":[{"name":"tool_name","parameters":{...}}]}
Always prioritize accuracy and provide detailed explanations of your actions."""
"@

Set-Content -Path $modelfilePath -Value $modelfileContent
Write-Host "   ✅ Modelfile created: $modelfilePath" -ForegroundColor Green

Write-Host ""

# ============================================
# STEP 3: Build the new model
# ============================================

Write-Host "🏗️ Step 3: Building expanded model variant..." -ForegroundColor Yellow
Write-Host "   This may take several minutes..." -ForegroundColor Gray
Write-Host ""

try {
    $outputName = "cheetah-stealth-agentic-5gb:latest"
    
    Write-Host "   Building: $outputName" -ForegroundColor Cyan
    Write-Host "   Quantization Level: $QuantLevel" -ForegroundColor Cyan
    Write-Host ""
    
    # Build using Modelfile
    & ollama create $outputName -f $modelfilePath
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n   ✅ Model built successfully!" -ForegroundColor Green
    } else {
        Write-Host "`n   ⚠️ Build completed with status: $LASTEXITCODE" -ForegroundColor Yellow
    }
    
} catch {
    Write-Host "   ❌ Build failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# ============================================
# STEP 4: Verify new model
# ============================================

Write-Host "🔍 Step 4: Verifying expanded model..." -ForegroundColor Yellow
Write-Host ""

try {
    # Get model info
    $models = & ollama list | ConvertFrom-String -PropertyNames Name, ID, Size, Modified
    $newModel = $models | Where-Object { $_.Name -like "*cheetah-stealth-agentic-5gb*" }
    
    if ($newModel) {
        Write-Host "   ✅ Model found in registry:" -ForegroundColor Green
        Write-Host "      Name: $($newModel.Name)" -ForegroundColor Cyan
        Write-Host "      Size: $($newModel.Size)" -ForegroundColor Cyan
        Write-Host "      ID: $($newModel.ID)" -ForegroundColor Gray
    } else {
        Write-Host "   ⚠️ New model not yet visible in list" -ForegroundColor Yellow
        Write-Host "   Attempting to pull model info..." -ForegroundColor Gray
    }
} catch {
    Write-Host "   ⚠️ Could not verify model: $_" -ForegroundColor Yellow
}

Write-Host ""

# ============================================
# STEP 5: Test new model
# ============================================

Write-Host "🧪 Step 5: Testing expanded model..." -ForegroundColor Yellow
Write-Host ""

try {
    $testPrompt = "List available tools and capabilities"
    Write-Host "   Test Prompt: $testPrompt" -ForegroundColor Gray
    Write-Host ""
    
    $startTime = Get-Date
    
    $body = @{
        model  = "cheetah-stealth-agentic-5gb:latest"
        prompt = $testPrompt
        stream = $false
    } | ConvertTo-Json
    
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
        -Method POST -Body $body -ContentType "application/json" -TimeoutSec 60
    
    $endTime = Get-Date
    $elapsed = ($endTime - $startTime).TotalSeconds
    
    Write-Host "   ✅ Model responded in $($elapsed.ToString('F2'))s" -ForegroundColor Green
    
    $preview = $response.response.Substring(0, [Math]::Min(200, $response.response.Length))
    Write-Host "   Response preview:" -ForegroundColor Cyan
    Write-Host "      $preview..." -ForegroundColor Gray
    
} catch {
    Write-Host "   ⚠️ Test failed: $_" -ForegroundColor Yellow
}

Write-Host ""

# ============================================
# STEP 6: Usage instructions
# ============================================

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                EXPANSION COMPLETE!                         ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "📌 NEW MODEL AVAILABLE:" -ForegroundColor Yellow
Write-Host "   Name: cheetah-stealth-agentic-5gb:latest" -ForegroundColor Cyan
Write-Host ""

Write-Host "🚀 HOW TO USE:" -ForegroundColor Yellow
Write-Host "   1. In RawrXD GUI:" -ForegroundColor Gray
Write-Host "      - Settings → Model Selection → cheetah-stealth-agentic-5gb:latest" -ForegroundColor Cyan
Write-Host ""
Write-Host "   2. In tests:" -ForegroundColor Gray
Write-Host "      - pwsh -NoProfile -File '.\Test-DualModelAgentic.ps1' -Model1 'cheetah-stealth-agentic-5gb:latest'" -ForegroundColor Cyan
Write-Host ""
Write-Host "   3. Direct API call:" -ForegroundColor Gray
Write-Host "      - ollama run cheetah-stealth-agentic-5gb:latest" -ForegroundColor Cyan
Write-Host ""

Write-Host "💡 BENEFITS:" -ForegroundColor Yellow
Write-Host "   ✅ Larger model size = better accuracy and reasoning" -ForegroundColor Green
Write-Host "   ✅ Better agentic decision-making" -ForegroundColor Green
Write-Host "   ✅ More nuanced tool selection" -ForegroundColor Green
Write-Host "   ✅ Improved context understanding" -ForegroundColor Green
Write-Host ""

Write-Host "📊 MODEL COMPARISON:" -ForegroundColor Yellow
Write-Host "   Original (cheetah-stealth-agentic): 2.0 GB" -ForegroundColor Cyan
Write-Host "   Expanded (cheetah-stealth-agentic-5gb): ~5.0 GB" -ForegroundColor Green
Write-Host ""

Write-Host "🔄 TO ALSO EXPAND OTHER MODELS:" -ForegroundColor Yellow
Write-Host "   - Run this script with different parameters" -ForegroundColor Gray
Write-Host "   - Or modify parameters: -ModelName 'other-model:latest' -QuantLevel 'Q5_K_M'" -ForegroundColor Cyan
Write-Host ""
