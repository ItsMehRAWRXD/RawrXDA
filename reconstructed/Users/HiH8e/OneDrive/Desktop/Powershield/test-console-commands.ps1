# Test Console Commands for RawrXD
# This tests the /vscode-* commands in interactive console mode

Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Testing RawrXD Console Commands" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

# Test 1: Help command (should show VSCode commands)
Write-Host "Test 1: Display help with VSCode marketplace commands" -ForegroundColor Yellow
Write-Host "Command: /help" -ForegroundColor Gray
Write-Host ""

# Simulate console input by calling the function directly
# First we need to dot-source and setup the environment
$env:RAWRXD_TEST_MODE = "true"

# Create a test script that will run console commands
$testScript = @'
# Load RawrXD in memory
$script:ConsoleMode = $true
$script:SkipGUI = $true

# Dot-source the main script (this will load all functions)
. ".\RawrXD.ps1"

# Now test the commands
Write-Host "`n=== TEST 1: Help Command ===" -ForegroundColor Cyan
Show-ConsoleHelp

Write-Host "`n=== TEST 2: /vscode-categories ===" -ForegroundColor Cyan
Process-ConsoleCommand "/vscode-categories"

Write-Host "`n=== TEST 3: /vscode-popular ===" -ForegroundColor Cyan
Process-ConsoleCommand "/vscode-popular"

Write-Host "`n=== TEST 4: /vscode-search copilot ===" -ForegroundColor Cyan
Process-ConsoleCommand "/vscode-search copilot"

Write-Host "`n=== TEST 5: Regular marketplace commands ===" -ForegroundColor Cyan
Process-ConsoleCommand "/marketplace"

Write-Host "`n=== TEST 6: Search local marketplace ===" -ForegroundColor Cyan
Process-ConsoleCommand "/search python"

Write-Host "`n`n=== ALL TESTS COMPLETE ===" -ForegroundColor Green
'@

# Save and execute the test script
$testScript | Out-File -FilePath ".\temp-console-test.ps1" -Encoding UTF8
powershell -ExecutionPolicy Bypass -File ".\temp-console-test.ps1"

# Cleanup
Remove-Item ".\temp-console-test.ps1" -ErrorAction SilentlyContinue

Write-Host "`n" + ("=" * 70) -ForegroundColor Cyan
Write-Host "Test Complete!" -ForegroundColor Green
