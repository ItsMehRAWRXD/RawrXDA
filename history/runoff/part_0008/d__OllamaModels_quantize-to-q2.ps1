# Quantize F32 model to Q2_K for maximum speed
$ErrorActionPreference = "Stop"

$InputModel = "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
$OutputModel = "D:\OllamaModels\BigDaddyG-Q2_K-ULTRA.gguf"
$QuantizeTool = "D:\OllamaModels\llama.cpp\llama-quantize.exe"

Write-Host "🐆 Quantizing to Q2_K (ultra-fast, smaller size)..." -ForegroundColor Cyan

# The F32 model is already Q4_0 internally, we need --allow-requantize
& $QuantizeTool --allow-requantize $InputModel $OutputModel Q2_K

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Quantization complete!" -ForegroundColor Green
    $inputSize = (Get-Item $InputModel).Length / 1GB
    $outputSize = (Get-Item $OutputModel).Length / 1GB
    Write-Host "  Input:  $([math]::Round($inputSize, 2)) GB" -ForegroundColor Gray
    Write-Host "  Output: $([math]::Round($outputSize, 2)) GB" -ForegroundColor Gray
    Write-Host "  Saved:  $([math]::Round($inputSize - $outputSize, 2)) GB" -ForegroundColor Green
    
    Write-Host "`nCreating Ollama model..." -ForegroundColor Cyan
    
    # Create modelfile
    $modelfile = @"
FROM $OutputModel

PARAMETER temperature 0.3
PARAMETER top_p 0.7
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1

SYSTEM ```You are BigDaddy-G-IDE-Q2-CHEETAH, a concise technical AI assistant with executable tool capabilities.

TOOLS AVAILABLE:
- CHEETAH_execute("command") - Executes PowerShell commands
- write_file("path", "content") - Writes files to disk
- read_file("path") - Reads files from disk

OPERATIONAL RULES:
1. Be extremely concise and direct
2. Use tools when appropriate
3. No explanations unless asked
4. Output only tool calls or minimal responses```
"@
    
    $modelfile | Out-File -FilePath "D:\OllamaModels\Modelfile-q2-ultra" -Encoding utf8
    
    ollama create bigdaddyg-q2-ultra -f "D:\OllamaModels\Modelfile-q2-ultra"
    
    Write-Host "`n✅ Model created: bigdaddyg-q2-ultra" -ForegroundColor Green
    Write-Host "Usage: ollama run bigdaddyg-q2-ultra" -ForegroundColor Cyan
} else {
    Write-Host "❌ Quantization failed" -ForegroundColor Red
}
