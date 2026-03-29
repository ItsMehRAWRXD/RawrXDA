#!/usr/bin/env powershell
# Update error messages to reflect HTTP-only Ollama routing

$filePath = "D:\rawrxd\src\main.cpp"
$content = [System.IO.File]::ReadAllText($filePath)

# Update error messages
$content = $content -replace '\[ERROR\] Ollama unavailable. Start Ollama or use /backend use ollama and pull a "model.\\n', '[ERROR] Ollama API unavailable. Ensure local HTTP API is running on 127.0.0.1 (check /api/chat endpoint).'
$content = $content -replace '\[Ollama error: ensure /api/chat is available\]', '[ERROR: Ollama HTTP API not responding on /api/chat]'
$content = $content -replace '\[Ollama error: start ollama serve\]', '[ERROR: start Ollama with `ollama serve` and ensure HTTP listening]'

# Write the modified content back
[System.IO.File]::WriteAllText($filePath, $content)

Write-Host "Error messages updated for HTTP-only routing." -ForegroundColor Green
