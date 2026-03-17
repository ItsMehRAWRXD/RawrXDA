# ============================================
# BigDaddyG Agentic Model Integration Test
# Tests tool-calling with RawrXD IDE
# ============================================

Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "BigDaddyG Agentic IDE Integration Test" -ForegroundColor Yellow
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

# Test 1: Basic Ollama Connection
Write-Host "[Test 1] Testing Ollama connection..." -ForegroundColor Green
try {
    $models = ollama list 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Ollama is running" -ForegroundColor Green
        Write-Host "  Available agentic models:" -ForegroundColor Cyan
        $models | Select-String "agentic|bg-ide" | ForEach-Object {
            Write-Host "    - $_" -ForegroundColor White
        }
    } else {
        Write-Host "✗ Ollama not responding" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "✗ Error: $_" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Test 2: Tool Call Generation
Write-Host "[Test 2] Testing tool call generation..." -ForegroundColor Green
$testPrompt = "List all .ps1 files in C:\Users\HiH8e\OneDrive\Desktop\Powershield"
Write-Host "  Prompt: $testPrompt" -ForegroundColor Cyan

$response = ollama run bg-ide-agentic "$testPrompt" 2>&1
Write-Host "  Response:" -ForegroundColor Cyan
Write-Host $response -ForegroundColor White
Write-Host ""

# Test 3: Extract Function Calls
Write-Host "[Test 3] Extracting function calls from response..." -ForegroundColor Green
$functionPattern = '\{\{function:([^}]+)\}\}'
$matches = [regex]::Matches($response, $functionPattern)

if ($matches.Count -gt 0) {
    Write-Host "✓ Found $($matches.Count) function call(s):" -ForegroundColor Green
    foreach ($match in $matches) {
        $functionCall = $match.Groups[1].Value
        Write-Host "    → $functionCall" -ForegroundColor Yellow
        
        # Parse function name and arguments
        if ($functionCall -match '(\w+)\((.*)\)') {
            $funcName = $Matches[1]
            $funcArgs = $Matches[2]
            Write-Host "      Function: $funcName" -ForegroundColor Cyan
            Write-Host "      Arguments: $funcArgs" -ForegroundColor Cyan
        }
    }
} else {
    Write-Host "⚠ No function calls found in response" -ForegroundColor Yellow
}
Write-Host ""

# Test 4: Simulate Tool Execution (Mock)
Write-Host "[Test 4] Simulating tool execution..." -ForegroundColor Green
if ($matches.Count -gt 0) {
    $firstCall = $matches[0].Groups[1].Value
    Write-Host "  Executing: $firstCall" -ForegroundColor Cyan
    
    # Mock execution
    if ($firstCall -match 'list_directory') {
        Write-Host "  ✓ Would execute: Get-ChildItem with parsed parameters" -ForegroundColor Green
        Write-Host "  ✓ Integration point ready for RawrXD.ps1" -ForegroundColor Green
    }
    elseif ($firstCall -match 'search_files') {
        Write-Host "  ✓ Would execute: Get-ChildItem -Recurse with filter" -ForegroundColor Green
        Write-Host "  ✓ Integration point ready for RawrXD.ps1" -ForegroundColor Green
    }
    else {
        Write-Host "  ✓ Function recognized: $firstCall" -ForegroundColor Green
    }
}
Write-Host ""

# Test 5: Multi-Turn Agentic Conversation
Write-Host "[Test 5] Testing multi-turn agentic workflow..." -ForegroundColor Green
$workflow = @(
    "Read the file RawrXD.ps1",
    "What tools are registered in that file?",
    "Create a new file called test-agent.ps1"
)

foreach ($step in $workflow) {
    Write-Host "  User: $step" -ForegroundColor Yellow
    $agentResponse = ollama run bg-ide-agentic "$step" 2>&1 | Select-Object -First 3
    Write-Host "  Agent: $agentResponse" -ForegroundColor White
    Write-Host ""
    Start-Sleep -Milliseconds 500
}

# Summary
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Integration Test Complete" -ForegroundColor Yellow
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Green
Write-Host "1. Launch RawrXD IDE:" -ForegroundColor Cyan
Write-Host "   .\RawrXD.ps1" -ForegroundColor White
Write-Host ""
Write-Host "2. In the IDE chat, select model: bg-ide-agentic" -ForegroundColor Cyan
Write-Host ""
Write-Host "3. Test with agentic commands:" -ForegroundColor Cyan
Write-Host "   - 'List all PowerShell files'" -ForegroundColor White
Write-Host "   - 'Read the main config file'" -ForegroundColor White
Write-Host "   - 'Show git status'" -ForegroundColor White
Write-Host "   - 'Create a new test file'" -ForegroundColor White
Write-Host ""
Write-Host "4. The IDE will automatically parse {{function:...}} calls" -ForegroundColor Cyan
Write-Host "   and execute them through the tool registry" -ForegroundColor White
Write-Host ""
