# Basic Example - Non-Streaming Response
Import-Module "$PSScriptRoot\..\OllamaTools.psm1" -Force

$model = "llama2"  # Change to your available model
$prompt = "Explain what PowerShell is in one sentence."

Write-Host "`n📝 Prompt: $prompt`n" -ForegroundColor Cyan
Write-Host "🤖 Response:" -ForegroundColor Green

try {
    $response = Invoke-OllamaGenerate -Model $model -Prompt $prompt
    Write-Host $response -ForegroundColor White
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
}

