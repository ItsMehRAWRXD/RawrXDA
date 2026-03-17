# Test CHEETAH Tool Execution Integration
# This verifies the C# host can parse and execute CHEETAH_execute commands

Write-Host "`n🐆 CHEETAH TOOL EXECUTION TEST 🐆" -ForegroundColor Yellow
Write-Host "=================================" -ForegroundColor Yellow
Write-Host ""

# Test 1: Simple command execution
Write-Host "Test 1: Testing CHEETAH_execute with whoami..." -ForegroundColor Cyan
$prompt1 = "Execute this command: whoami. Use the CHEETAH_execute function."

# Test 2: File write
Write-Host "Test 2: Testing write_file..." -ForegroundColor Cyan
$testFile = "$env:TEMP\cheetah_test.txt"
$prompt2 = "Create a file at '$testFile' with content 'CHEETAH STEALTH MODE ACTIVE'. Use the write_file function."

# Test 3: File read
Write-Host "Test 3: Testing read_file..." -ForegroundColor Cyan
$prompt3 = "Read the file at '$testFile'. Use the read_file function."

# Test 4: Stealth file hide
Write-Host "Test 4: Testing stealth file operations..." -ForegroundColor Cyan
$prompt4 = "Hide the file '$testFile' using the attrib command. Use CHEETAH_execute."

Write-Host ""
Write-Host "Prompts ready. Now run RawrXD and send these prompts to bigdaddyg-cheetah" -ForegroundColor Green
Write-Host ""
Write-Host "Expected Model Response Patterns:" -ForegroundColor White
Write-Host "  - CHEETAH_execute(`"whoami`")" -ForegroundColor Gray
Write-Host "  - write_file(`"$testFile`", `"CHEETAH STEALTH MODE ACTIVE`")" -ForegroundColor Gray
Write-Host "  - read_file(`"$testFile`")" -ForegroundColor Gray
Write-Host "  - CHEETAH_execute(`"attrib +h +s $testFile`")" -ForegroundColor Gray
Write-Host ""

# Quick API test
Write-Host "Quick API Test (if RawrXD is running)..." -ForegroundColor Yellow

$apiUrl = "http://localhost:5000/api/ollama/chat"
$testPayload = @{
    model = "bigdaddyg-cheetah"
    prompt = "Execute the whoami command using CHEETAH_execute."
} | ConvertTo-Json

try {
    Write-Host "Sending test prompt to RawrXD..." -ForegroundColor White
    $response = Invoke-RestMethod -Uri $apiUrl -Method Post -Body $testPayload -ContentType "application/json" -ErrorAction Stop
    
    Write-Host "`n✓ Response received!" -ForegroundColor Green
    Write-Host "Model Response:" -ForegroundColor Cyan
    Write-Host $response.response -ForegroundColor White
    
    if ($response.toolResults) {
        Write-Host "`nTool Execution Results:" -ForegroundColor Yellow
        $response.toolResults | ForEach-Object {
            Write-Host "  Tool: $($_.tool)" -ForegroundColor Cyan
            Write-Host "  Result: $($_.result)" -ForegroundColor White
        }
    }
    
} catch {
    Write-Host "⚠ RawrXD not running or API not available" -ForegroundColor Yellow
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "To test manually:" -ForegroundColor White
    Write-Host "1. Start RawrXD" -ForegroundColor Gray
    Write-Host "2. Send prompt: 'Execute whoami using CHEETAH_execute'" -ForegroundColor Gray
    Write-Host "3. Check if the model outputs: CHEETAH_execute(`"whoami`")" -ForegroundColor Gray
    Write-Host "4. Verify the C# host actually runs the command" -ForegroundColor Gray
}

Write-Host ""
Write-Host "🐆 Test setup complete!" -ForegroundColor Green
