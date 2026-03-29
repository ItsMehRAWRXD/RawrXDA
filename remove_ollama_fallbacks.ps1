#!/usr/bin/env powershell
# Remove all Ollama direct fallbacks to enforce HTTP-only routing

$filePath = "D:\rawrxd\src\main.cpp"
$content = [System.IO.File]::ReadAllText($filePath)

# Pattern 1: /chat api response (lines 954-961)
# Replace: if (AgenticHttpChatGenerateSync(...) || OllamaGenerateSync(...)) with just AgenticHttpChatGenerateSync
$pattern1 = 'if \(AgenticHttpChatGenerateSync\("127\.0\.0\.1", localApiPort, ollamaModel, message,\s+response\) \|\|\s+OllamaGenerateSync\(host, port, ollamaModel, message, response\)\)'
$replacement1 = 'if (AgenticHttpChatGenerateSync("127.0.0.1", localApiPort, ollamaModel, message, response))'
$content = [System.Text.RegularExpressions.Regex]::Replace($content, $pattern1, $replacement1)

# Also update the error message for pattern 1
$content = $content -replace '\[Ollama error: start Ollama or check OLLAMA_HOST / backend\]', '[Ollama API error: ensure /api/chat is available]'

# Pattern 2: /chat command (lines 1041-1042)
$pattern2 = 'if \(AgenticHttpChatGenerateSync\("127\.0\.0\.1", localApiPort, model, msg, response\) \|\|\s+OllamaGenerateSync\(host, port, model, msg, response\)\)'
$replacement2 = 'if (AgenticHttpChatGenerateSync("127.0.0.1", localApiPort, model, msg, response))'
$content = [System.Text.RegularExpressions.Regex]::Replace($content, $pattern2, $replacement2)

# Pattern 3: /agent command - remove the && OllamaGenerateSync fallback (lines 1118-1120)
$pattern3 = 'if \(\!AgenticHttpChatGenerateSync\("127\.0\.0\.1", localApiPort, model, currentPrompt,\s+response\) &&\s+\!OllamaGenerateSync\(host, port, model, currentPrompt, response\)\)'
$replacement3 = 'if (!AgenticHttpChatGenerateSync("127.0.0.1", localApiPort, model, currentPrompt, response))'
$content = [System.Text.RegularExpressions.Regex]::Replace($content, $pattern3, $replacement3)

# Pattern 4: /subagent command - remove the && OllamaGenerateSync fallback (lines 1272-1273)
$pattern4 = 'if \(\!AgenticHttpChatGenerateSync\("127\.0\.0\.1", localApiPort, model, currentPrompt, response\) &&\s+\!OllamaGenerateSync\(host, port, model, currentPrompt, response\)\)'
$replacement4 = 'if (!AgenticHttpChatGenerateSync("127.0.0.1", localApiPort, model, currentPrompt, response))'
$content = [System.Text.RegularExpressions.Regex]::Replace($content, $pattern4, $replacement4)

# Write the modified content back
[System.IO.File]::WriteAllText($filePath, $content)

Write-Host "Ollama fallbacks removed successfully. HTTP-only routing now enforced." -ForegroundColor Green
