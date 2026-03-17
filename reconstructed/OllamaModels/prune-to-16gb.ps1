# Prune layers to reduce model to under 16GB
$ErrorActionPreference = "Stop"

$InputModel = "D:\OllamaModels\BigDaddyG-Q2_K-ULTRA.gguf"
$OutputModel = "D:\OllamaModels\BigDaddyG-Q2_K-PRUNED-16GB.gguf"
$QuantizeTool = "D:\OllamaModels\llama.cpp\llama-quantize.exe"

Write-Host "🔪 Pruning layers to reduce from 23.71 GB to under 16 GB..." -ForegroundColor Cyan
Write-Host ""

# Model has 80 layers (blk.0 to blk.79)
# To get from 23.71 GB to under 16 GB, we need to remove ~33% of layers
# Remove layers 53-79 (27 layers = 33.75% reduction)

$layersToPrune = 53..79 -join ","

Write-Host "Removing layers: $layersToPrune" -ForegroundColor Yellow
Write-Host "Keeping layers: 0-52 (53 layers total)" -ForegroundColor Green
Write-Host ""

& $QuantizeTool --prune-layers $layersToPrune $InputModel $OutputModel COPY

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✅ Pruning complete!" -ForegroundColor Green
    $inputSize = (Get-Item $InputModel).Length / 1GB
    $outputSize = (Get-Item $OutputModel).Length / 1GB
    Write-Host "  Input:  $([math]::Round($inputSize, 2)) GB" -ForegroundColor Gray
    Write-Host "  Output: $([math]::Round($outputSize, 2)) GB" -ForegroundColor Green
    Write-Host "  Saved:  $([math]::Round($inputSize - $outputSize, 2)) GB" -ForegroundColor Green
    Write-Host ""
    
    if ($outputSize -lt 16) {
        Write-Host "✅ Target achieved! Model is under 16 GB" -ForegroundColor Green
    } else {
        Write-Host "⚠ Model is still above 16 GB, need more pruning..." -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "Creating Ollama model..." -ForegroundColor Cyan
    
    # Create modelfile
    $modelfile = @"
FROM $OutputModel

PARAMETER temperature 0.3
PARAMETER top_p 0.7
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1

SYSTEM """You are BigDaddy-G-IDE-PRUNED, a concise technical AI assistant with executable tool capabilities.

TOOLS AVAILABLE:
- CHEETAH_execute("command") - Executes PowerShell commands
- write_file("path", "content") - Writes files to disk
- read_file("path") - Reads files from disk

OPERATIONAL RULES:
1. Be extremely concise and direct
2. Use tools when appropriate
3. No explanations unless asked
4. Output only tool calls or minimal responses"""
"@
    
    $modelfile | Out-File -FilePath "D:\OllamaModels\Modelfile-pruned-16gb" -Encoding utf8
    
    ollama create bigdaddyg-16gb -f "D:\OllamaModels\Modelfile-pruned-16gb"
    
    Write-Host ""
    Write-Host "✅ Model created: bigdaddyg-16gb" -ForegroundColor Green
    Write-Host "Usage: ollama run bigdaddyg-16gb" -ForegroundColor Cyan
} else {
    Write-Host "❌ Pruning failed" -ForegroundColor Red
}
