# Zero-Day Agent Test Script
# Pipes the exact test prompt into test_chat_streaming.exe

$prompt = @"
My build script is too slow. As the Zero-Day Agent, analyse the CMake configuration and generate a 3-step plan to reduce compile time by using parallelism. Do **not** execute the steps yet—just output the plan.
"@

Write-Host "=== Zero-Day Agent Production Test ===" -ForegroundColor Cyan
Write-Host "Sending prompt to test_chat_streaming.exe..." -ForegroundColor Yellow
Write-Host ""

# Set location to binary directory
Set-Location "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release"

# Pipe the prompt into the executable
# Using echo to send the prompt, then "quit" to exit cleanly
$input = $prompt + "`nquit"
$input | .\test_chat_streaming.exe

Write-Host ""
Write-Host "=== Test Complete ===" -ForegroundColor Green
