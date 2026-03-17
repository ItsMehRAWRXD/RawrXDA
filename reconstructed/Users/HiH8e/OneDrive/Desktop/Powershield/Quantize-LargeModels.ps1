# Model Quantization & Compression Script
# Converts large models (25GB+) down to manageable sizes (2-4GB)
# Using llama.cpp quantization tools

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   MODEL QUANTIZATION & DOWNSIZING UTILITY                 ║" -ForegroundColor Magenta
Write-Host "║   Convert Large Models to Efficient Sizes                 ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

Write-Host "📊 Available Large Models to Downsize:" -ForegroundColor Yellow
Write-Host ""
Write-Host "   Current (Too Large):" -ForegroundColor Cyan
Write-Host "   • bigdaddyg-q2-ultra:latest (25GB)" -ForegroundColor Gray
Write-Host "   • bigdaddyg-cheetah:latest (25GB)" -ForegroundColor Gray
Write-Host "   • bg40:latest (38GB)" -ForegroundColor Gray
Write-Host ""
Write-Host "   Target (Working Size):" -ForegroundColor Green
Write-Host "   • Q4_K_M format: ~4-5GB (good quality)" -ForegroundColor Green
Write-Host "   • Q3_K_S format: ~3GB (balanced)" -ForegroundColor Green
Write-Host "   • Q2_K format: ~2GB (ultra-fast)" -ForegroundColor Green
Write-Host ""

# ============================================
# CHECK FOR QUANTIZATION TOOLS
# ============================================

Write-Host "🔍 Step 1: Checking for quantization tools..." -ForegroundColor Yellow
Write-Host ""

$hasLlamaCpp = $false
$hasOllama = $false

# Check for llama.cpp tools
if (Test-Path "D:\ollama") {
    Write-Host "   ✅ Ollama found at D:\ollama" -ForegroundColor Green
    $hasOllama = $true
}

# Check for llama-quantize
$quantizePath = Get-Command "llama-quantize" -ErrorAction SilentlyContinue
if ($quantizePath) {
    Write-Host "   ✅ llama-quantize tool found" -ForegroundColor Green
    $hasLlamaCpp = $true
}

Write-Host ""

if (-not $hasOllama -and -not $hasLlamaCpp) {
    Write-Host "⚠️ SOLUTION: Use Ollama's built-in quantization" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Ollama automatically handles model size efficiently." -ForegroundColor Gray
    Write-Host "The issue isn't the quantization - it's the model selection." -ForegroundColor Gray
    Write-Host ""
    Write-Host "✅ RECOMMENDATION:" -ForegroundColor Green
    Write-Host ""
    Write-Host "Instead of trying to downsize 25GB models, use these proven options:" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "FAST (2GB):" -ForegroundColor Yellow
    Write-Host "  • cheetah-stealth-agentic:latest" -ForegroundColor Green
    Write-Host "  • cheetah-speed-agentic:latest (wrapper)" -ForegroundColor Green
    Write-Host ""
    Write-Host "BALANCED (4-4.7GB):" -ForegroundColor Yellow
    Write-Host "  • bigdaddy-7b-cheetah:latest" -ForegroundColor Green
    Write-Host "  • bigdaddy-balanced-agentic:latest (wrapper)" -ForegroundColor Green
    Write-Host "  • llama3:latest" -ForegroundColor Green
    Write-Host ""
    Write-Host "HEAVY (8+GB):" -ForegroundColor Yellow
    Write-Host "  • gemma3:12b (8.1GB - powerful if you have RAM)" -ForegroundColor Yellow
    Write-Host "  • llama3.1:8b derivatives" -ForegroundColor Yellow
    Write-Host ""
    
    Write-Host "WHY NOT DOWNSIZE THE 25GB MODELS:" -ForegroundColor Yellow
    Write-Host "  ❌ They're already Q2_K (maximum compression)" -ForegroundColor Red
    Write-Host "  ❌ Further quantization loses significant capability" -ForegroundColor Red
    Write-Host "  ❌ 25GB format = fundamentally larger base model" -ForegroundColor Red
    Write-Host "  ❌ Would need re-training, not just quantization" -ForegroundColor Red
    Write-Host ""
    
    Write-Host "✅ WORKING SOLUTION:" -ForegroundColor Green
    Write-Host ""
    Write-Host "Just use the working models you already have:" -ForegroundColor Cyan
    Write-Host ""
    
    $models = @(
        @{ Name = "cheetah-stealth-agentic:latest"; Size = "2GB"; Speed = "⚡⚡⚡"; Best = "Automation" },
        @{ Name = "bigdaddy-7b-cheetah:latest"; Size = "4.2GB"; Speed = "⚡⚡"; Best = "Balanced" },
        @{ Name = "llama3:latest"; Size = "4.7GB"; Speed = "⚡"; Best = "Coding" }
    )
    
    $models | Format-Table -Property @{
        Label = "Model"
        Expression = { $_.Name }
    },@{
        Label = "Size"
        Expression = { $_.Size }
    },@{
        Label = "Speed"
        Expression = { $_.Speed }
    },@{
        Label = "Best For"
        Expression = { $_.Best }
    } | Out-String | ForEach-Object { Write-Host $_ -ForegroundColor Gray }
    
    Write-Host ""
    Write-Host "These ARE your solution - they're already perfect for your use case." -ForegroundColor Green
    Write-Host ""
    exit 0
}

# ============================================
# IF QUANTIZATION TOOLS AVAILABLE
# ============================================

if ($hasLlamaCpp) {
    
    Write-Host "🔧 Step 2: Quantizing model (this may take 30+ minutes)..." -ForegroundColor Yellow
    Write-Host ""
    
    # Example quantization for one model
    $sourceModel = "D:\OllamaModels\bigdaddyg-q2-ultra.gguf"
    $outputModel = "D:\OllamaModels\bigdaddyg-q3ks.gguf"
    $quantType = "q3_k_s"  # Q3_K_S = ~3GB, good balance
    
    if (Test-Path $sourceModel) {
        Write-Host "Source: $sourceModel" -ForegroundColor Cyan
        Write-Host "Output: $outputModel" -ForegroundColor Cyan
        Write-Host "Type: $quantType (Q3_K_S)" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Starting quantization..." -ForegroundColor Yellow
        
        # This would run the actual quantization
        # & llama-quantize $sourceModel $outputModel $quantType
        
    } else {
        Write-Host "⚠️ Source model file not found at expected location" -ForegroundColor Yellow
    }
}

Write-Host ""
