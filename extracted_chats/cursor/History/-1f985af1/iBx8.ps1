#Requires -Version 7.2
<#
.SYNOPSIS
    Basic example demonstrating non-streaming Ollama generation.
.DESCRIPTION
    This script shows how to use Invoke-OllamaGenerate for a simple,
    non-streaming request that waits for the complete response.
#>

# Import the module
$modulePath = Join-Path $PSScriptRoot "..\OllamaTools.psm1"
Import-Module $modulePath -Force

# Configuration
$model = "llama2"  # Change to your available model (e.g., "mistral", "qwen2:7b")
$prompt = "Explain what PowerShell is in one sentence."

Write-Host "`n📝 Prompt: $prompt`n" -ForegroundColor Cyan
Write-Host "🤖 Response:" -ForegroundColor Green
Write-Host ("-" * 60) -ForegroundColor Gray

try {
    $response = Invoke-OllamaGenerate -Model $model -Prompt $prompt
    Write-Host $response -ForegroundColor White
    Write-Host ("-" * 60) -ForegroundColor Gray
    Write-Host "`n✅ Response received (length: $($response.Length) chars)" -ForegroundColor Green
} catch {
    Write-Host "`n❌ Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nMake sure:" -ForegroundColor Yellow
    Write-Host "  1. Ollama server is running (ollama serve)" -ForegroundColor Yellow
    Write-Host "  2. Model '$model' is pulled (ollama pull $model)" -ForegroundColor Yellow
}

