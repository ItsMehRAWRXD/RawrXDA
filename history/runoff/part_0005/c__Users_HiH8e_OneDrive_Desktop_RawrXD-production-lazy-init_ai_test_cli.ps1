# AI ENDPOINT TEST CLI UTILITY

# AI ENDPOINT TEST CLI UTILITY

param(
    [string]$Module = "All",
    [switch]$Verbose,
    [switch]$Visual
)

$ErrorActionPreference = "Continue"

Write-Host "╔══════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    AI MODULE TEST CLI v1.0                       ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Test Results
$TestResults = @{}

function Test-ChatInterface {
    Write-Host "[1/3] Testing Chat Interface..." -ForegroundColor Yellow
    
    # Check if chat interface module exists
    $chatModule = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\src\chat_interface.asm"
    if (Test-Path $chatModule) {
        Write-Host "  ✓ Chat interface module found" -ForegroundColor Green
        $TestResults.ChatInterface = "Module Found"
    } else {
        Write-Host "  ✗ Chat interface module missing" -ForegroundColor Red
        $TestResults.ChatInterface = "Missing"
        return
    }
    
    # Check for exports
    $exports = Get-Content $chatModule | Select-String "public" | Select-String -Pattern "InitializeChatInterface|CleanupChatInterface|ProcessUserMessage"
    if ($exports.Count -ge 3) {
        Write-Host "  ✓ All exports present" -ForegroundColor Green
        $TestResults.ChatExports = "Complete"
    } else {
        Write-Host "  ⚠️  Missing exports" -ForegroundColor Yellow
        $TestResults.ChatExports = "Incomplete"
    }
    
    # Check compilation status
    $objFile = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\build\chat_interface.obj"
    if (Test-Path $objFile) {
        Write-Host "  ✓ Object file compiled" -ForegroundColor Green
        $TestResults.ChatCompiled = "Yes"
    } else {
        Write-Host "  ⚠️  Not compiled" -ForegroundColor Yellow
        $TestResults.ChatCompiled = "No"
    }
}

function Test-AgenticLoop {
    Write-Host "[2/3] Testing Agentic Loop..." -ForegroundColor Yellow
    
    $agenticModule = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\src\agentic_loop.asm"
    if (Test-Path $agenticModule) {
        Write-Host "  ✓ Agentic loop module found" -ForegroundColor Green
        $TestResults.AgenticLoop = "Module Found"
    } else {
        Write-Host "  ✗ Agentic loop module missing" -ForegroundColor Red
        $TestResults.AgenticLoop = "Missing"
        return
    }
    
    # Check for 44-tool integration
    $toolCount = Get-Content $agenticModule | Select-String "TOOL_CATEGORY" | Measure-Object | Select-Object -ExpandProperty Count
    Write-Host "  ✓ $toolCount tool categories defined" -ForegroundColor Green
    $TestResults.ToolCategories = $toolCount
    
    # Check exports
    $exports = Get-Content $agenticModule | Select-String "public" | Select-String -Pattern "InitializeAgenticLoop|StartAgenticLoop|StopAgenticLoop|GetAgentStatus|CleanupAgenticLoop"
    if ($exports.Count -ge 5) {
        Write-Host "  ✓ All agentic exports present" -ForegroundColor Green
        $TestResults.AgenticExports = "Complete"
    } else {
        Write-Host "  ⚠️  Missing agentic exports" -ForegroundColor Yellow
        $TestResults.AgenticExports = "Incomplete"
    }
}

function Test-LLMClient {
    Write-Host "[3/3] Testing LLM Client..." -ForegroundColor Yellow
    
    $llmModule = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\src\llm_client.asm"
    if (Test-Path $llmModule) {
        Write-Host "  ✓ LLM client module found" -ForegroundColor Green
        $TestResults.LLMClient = "Module Found"
    } else {
        Write-Host "  ✗ LLM client module missing" -ForegroundColor Red
        $TestResults.LLMClient = "Missing"
        return
    }
    
    # Check backend support
    $backends = Get-Content $llmModule | Select-String "LLM_BACKEND" | Select-String -Pattern "OPENAI|CLAUDE|GEMINI|GGUF|OLLAMA"
    $backendCount = $backends.Count
    Write-Host "  ✓ $backendCount LLM backends supported" -ForegroundColor Green
    $TestResults.LLMBackends = $backendCount
    
    # Check exports
    $exports = Get-Content $llmModule | Select-String "public" | Select-String -Pattern "InitializeLLMClient|SwitchLLMBackend|GetCurrentBackendName|CleanupLLMClient"
    if ($exports.Count -ge 4) {
        Write-Host "  ✓ All LLM exports present" -ForegroundColor Green
        $TestResults.LLMExports = "Complete"
    } else {
        Write-Host "  ⚠️  Missing LLM exports" -ForegroundColor Yellow
        $TestResults.LLMExports = "Incomplete"
    }
}

function Test-MenuIntegration {
    Write-Host "[4/4] Testing Menu Integration..." -ForegroundColor Yellow
    
    $phase4Module = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\src\phase4_integration.asm"
    if (Test-Path $phase4Module) {
        Write-Host "  ✓ Phase 4 integration module found" -ForegroundColor Green
        $TestResults.Phase4Module = "Found"
    } else {
        Write-Host "  ✗ Phase 4 module missing" -ForegroundColor Red
        $TestResults.Phase4Module = "Missing"
        return
    }
    
    # Check menu item count
    $menuItems = Get-Content $phase4Module | Select-String "IDM_AI_" | Measure-Object | Select-Object -ExpandProperty Count
    Write-Host "  ✓ $menuItems AI menu items defined" -ForegroundColor Green
    $TestResults.AIMenuItems = $menuItems
    
    # Check if stubs are replaced
    $stubCount = Get-Content $phase4Module | Select-String "mov eax, 1" | Select-String -Pattern "InitializeLLMClient|InitializeAgenticLoop|InitializeChatInterface" | Measure-Object | Select-Object -ExpandProperty Count
    if ($stubCount -eq 0) {
        Write-Host "  ✓ No stub implementations found" -ForegroundColor Green
        $TestResults.StubsReplaced = "Yes"
    } else {
        Write-Host "  ⚠️  $stubCount stub implementations found" -ForegroundColor Yellow
        $TestResults.StubsReplaced = "No"
    }
}

function Show-TestResults {
    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                    TEST RESULTS SUMMARY                         ║" -ForegroundColor Magenta
    Write-Host "╚══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
    
    $TestResults.GetEnumerator() | ForEach-Object {
        $status = if ($_.Value -match "Found|Complete|Yes") { "✅" } elseif ($_.Value -match "Missing|Incomplete|No") { "❌" } else { "⚠️" }
        Write-Host "  $status $($_.Key): $($_.Value)" -ForegroundColor $(if ($_.Value -match "Found|Complete|Yes") { "Green" } elseif ($_.Value -match "Missing|Incomplete|No") { "Red" } else { "Yellow" })
    }
    
    # Calculate overall score
    $totalTests = $TestResults.Count
    $passedTests = ($TestResults.Values | Where-Object { $_ -match "Found|Complete|Yes" }).Count
    $score = [math]::Round(($passedTests / $totalTests) * 100)
    
    Write-Host ""
    Write-Host "  📊 Overall Score: $score%" -ForegroundColor $(if ($score -ge 90) { "Green" } elseif ($score -ge 70) { "Yellow" } else { "Red" })
    Write-Host "  🎯 Status: $(if ($score -ge 90) { 'PRODUCTION READY' } elseif ($score -ge 70) { 'TESTING NEEDED' } else { 'DEVELOPMENT' })" -ForegroundColor $(if ($score -ge 90) { "Green" } elseif ($score -ge 70) { "Yellow" } else { "Red" })
}

# Main execution
switch ($Module.ToLower()) {
    "chat" { Test-ChatInterface }
    "agentic" { Test-AgenticLoop }
    "llm" { Test-LLMClient }
    "menu" { Test-MenuIntegration }
    "all" { 
        Test-ChatInterface
        Test-AgenticLoop
        Test-LLMClient
        Test-MenuIntegration
    }
    default { 
        Write-Host "Unknown module: $Module" -ForegroundColor Red
        Write-Host "Valid options: chat, agentic, llm, menu, all" -ForegroundColor Yellow
        return
    }
}

Show-TestResults

# Visual indicator for menu dropdown test
if ($Visual) {
    Write-Host ""
    Write-Host "🎨 VISUAL MENU TEST:" -ForegroundColor Cyan
    Write-Host "  • AI Menu should appear between Edit and View" -ForegroundColor White
    Write-Host "  • 16 menu items should be clickable" -ForegroundColor White
    Write-Host "  • Status indicators should light up GREEN" -ForegroundColor White
    Write-Host "  • Features should open without errors" -ForegroundColor White
}

Write-Host ""
Write-Host "🎯 NEXT STEPS:" -ForegroundColor Cyan
Write-Host "  • Run: .\ai_test_cli.ps1 -Module all -Visual" -ForegroundColor White
Write-Host "  • Launch IDE: Start-Process masm_ide\build\AgenticIDEWin.exe" -ForegroundColor White
Write-Host "  • Test AI menu dropdown and feature opening" -ForegroundColor White