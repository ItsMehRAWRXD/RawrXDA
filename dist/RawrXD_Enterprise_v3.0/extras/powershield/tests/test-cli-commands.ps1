# Test script for CLI chat commands
# This demonstrates how the AI can use the new commands

Write-Host "=== Testing RawrXD CLI Chat Commands ===" -ForegroundColor Cyan
Write-Host ""

# Test 1: GREP command
Write-Host "Test 1: GREP command" -ForegroundColor Yellow
Write-Host "Command: grep Send-OllamaRequest" -ForegroundColor Gray
Write-Host ""

# Test 2: LIST command
Write-Host "Test 2: LIST command" -ForegroundColor Yellow
Write-Host "Command: list" -ForegroundColor Gray
Write-Host ""

# Test 3: SEARCH command
Write-Host "Test 3: SEARCH command" -ForegroundColor Yellow
Write-Host "Command: search file browser" -ForegroundColor Gray
Write-Host ""

# Test 4: SUMMARIZE command
Write-Host "Test 4: SUMMARIZE command" -ForegroundColor Yellow
Write-Host "Command: /summarize" -ForegroundColor Gray
Write-Host ""

Write-Host "To test interactively, run:" -ForegroundColor Green
Write-Host "  .\RawrXD.ps1 -CliMode -Command chat -Model cheetah-stealth" -ForegroundColor White
Write-Host ""
Write-Host "Then try these commands in the chat:" -ForegroundColor Green
Write-Host "  grep Send-OllamaRequest" -ForegroundColor White
Write-Host "  /list" -ForegroundColor White
Write-Host "  search file browser" -ForegroundColor White
Write-Host "  /summarize" -ForegroundColor White
Write-Host "  exit" -ForegroundColor White
Write-Host ""

