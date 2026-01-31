# Fully Random Agentic Pipeline Test
# Tests the agentic system with real AI models performing random tasks

param(
    [int]$NumTests = 5,
    [string]$Model = "cheetah-stealth-agentic:latest"
)

Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   FULLY RANDOM AGENTIC PIPELINE TEST (Real AI Models)    ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# ============================================
# SETUP
# ============================================

$script:agentTools = @{}
function Write-DevConsole { }
function Register-AgentTool {
    param([string]$Name, [string]$Description, [string]$Category, [string]$Version, [hashtable]$Parameters, [scriptblock]$Handler)
    $script:agentTools[$Name] = @{ Name=$Name; Handler=$Handler }
}

. .\BuiltInTools.ps1
Initialize-BuiltInTools

. .\AutoToolInvocation.ps1

function Invoke-AgentTool {
    param([string]$ToolName, [hashtable]$Parameters = @{})
    if ($script:agentTools.ContainsKey($ToolName)) {
        & $script:agentTools[$ToolName].Handler @Parameters
    } else { @{ success = $false; error = "Tool not found" } }
}

Write-Host "📦 Modules Loaded:" -ForegroundColor Green
Write-Host "   - BuiltInTools: $($script:agentTools.Count) tools" -ForegroundColor Cyan
Write-Host "   - AutoToolInvocation: Ready" -ForegroundColor Cyan
Write-Host "   - Model: $Model" -ForegroundColor Cyan
Write-Host ""

# ============================================
# RANDOM TEST GENERATORS
# ============================================

$fileOperationPrompts = @(
    "What's the size of RawrXD.ps1?",
    "Show me the git status of this repository",
    "Find all PowerShell files in this directory",
    "Read the first 3 lines of BuiltInTools.ps1",
    "How many files are in the current folder?",
    "Search for 'function' in .ps1 files",
    "Create a test file and tell me its path",
    "List the contents of the .git directory",
    "Find the largest file in this directory"
)

$codeAnalysisPrompts = @(
    "Analyze RawrXD.ps1 for potential errors",
    "Check AutoToolInvocation.ps1 for any issues",
    "What programming patterns do you see in BuiltInTools.ps1?",
    "Find all function definitions in the .ps1 files here",
    "Do any of these files have security issues?"
)

$thinkingPrompts = @(
    "Based on the file list here, what does this project do?",
    "After looking at these files, what would you recommend improving?",
    "Summarize what tools are available in BuiltInTools.ps1",
    "What is the architecture of RawrXD based on the files?"
)

function Get-RandomPrompt {
    $allPrompts = $fileOperationPrompts + $codeAnalysisPrompts + $thinkingPrompts
    return $allPrompts | Get-Random
}

# ============================================
# TEST RUNNER
# ============================================

$passed = 0
$failed = 0
$totalToolsUsed = 0

for ($i = 1; $i -le $NumTests; $i++) {
    Write-Host "TEST $i/$NumTests" -ForegroundColor Magenta
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkCyan
    
    # 1. Generate random prompt
    $prompt = Get-RandomPrompt
    Write-Host "📝 Prompt: $prompt" -ForegroundColor Yellow
    
    # 2. Auto-detect tools
    $mightNeedTools = Test-RequiresAutoTooling -UserMessage $prompt
    Write-Host "🔍 Auto-Detection: $(if ($mightNeedTools) { 'Tools needed ✅' } else { 'No tools needed ℹ️' })" -ForegroundColor $(if ($mightNeedTools) { 'Green' } else { 'Gray' })
    
    # 3. Invoke auto-tool pipeline
    if ($mightNeedTools) {
        $autoResult = Invoke-AutoToolCalling -UserMessage $prompt -CurrentWorkingDir $PWD.Path -ConfidenceThreshold 0.75
        if ($autoResult.autoInvoked -and $autoResult.results.Count -gt 0) {
            Write-Host "🔧 Pre-execution tools: $($autoResult.results.Count)" -ForegroundColor Cyan
            foreach ($r in $autoResult.results) {
                $icon = if ($r.success) { "✅" } else { "❌" }
                Write-Host "   $icon $($r.tool)" -ForegroundColor $(if ($r.success) { 'Green' } else { 'Red' })
            }
            $totalToolsUsed += $autoResult.results.Count
        }
    }
    
    # 4. Send to AI with system prompt (AGENTIC MODE)
    Write-Host "🤖 Querying AI model in agentic mode..." -ForegroundColor Cyan
    
    $systemPrompt = @"
You are an agentic AI assistant. You have access to these tools: $($script:agentTools.Keys -join ', ')

When you need to access files or information, RESPOND WITH JSON in this exact format:
{"tool_calls":[{"name":"tool_name","parameters":{"param1":"value1","param2":"value2"}}]}

IMPORTANT: Respond with ONLY JSON when calling tools. No explanations until tools execute.
If you don't need tools, respond naturally.
"@
    
    try {
        $body = @{
            model  = $Model
            prompt = $systemPrompt + "`n`nUser: $prompt`n`nAssistant:"
            stream = $false
        } | ConvertTo-Json -Depth 3
        
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 30 `
            -ErrorAction Stop
        
        $aiResponse = $response.response
        Write-Host "📢 AI Response received (${([int]$aiResponse.Length)} chars)" -ForegroundColor Gray
        
        # 5. Check for tool calls in response
        $toolsFound = @()
        if ($aiResponse -match '\{"tool_calls"\s*:\s*\[') {
            Write-Host "🔨 Detected tool call JSON in response" -ForegroundColor Green
            
            # Extract and execute all JSON objects
            $jsonMatches = [regex]::Matches($aiResponse, '\{"tool_calls"\s*:\s*\[[^\]]*\]\}')
            
            foreach ($match in $jsonMatches) {
                try {
                    $toolCall = ($match.Value | ConvertFrom-Json)
                    foreach ($call in $toolCall.tool_calls) {
                        Write-Host "   🔧 Executing: $($call.name)" -ForegroundColor Magenta
                        $params = @{}
                        if ($call.parameters) {
                            $call.parameters.PSObject.Properties | ForEach-Object { $params[$_.Name] = $_.Value }
                        }
                        
                        $toolResult = Invoke-AgentTool -ToolName $call.name -Parameters $params
                        $toolsFound += $call.name
                        
                        if ($toolResult.success) {
                            Write-Host "      ✅ Success" -ForegroundColor Green
                            $totalToolsUsed++
                        } else {
                            Write-Host "      ❌ Failed: $($toolResult.error)" -ForegroundColor Red
                        }
                    }
                } catch {
                    Write-Host "   ⚠️ Failed to parse tool JSON: $_" -ForegroundColor Yellow
                }
            }
        } else {
            Write-Host "💭 AI responded with natural language (no tools needed)" -ForegroundColor Cyan
        }
        
        # 6. Show preview of AI response
        $preview = $aiResponse.Substring(0, [Math]::Min(100, $aiResponse.Length))
        if ($aiResponse.Length -gt 100) { $preview += "..." }
        Write-Host "   Preview: $preview" -ForegroundColor Gray
        
        Write-Host "✅ Test $i passed" -ForegroundColor Green
        $passed++
        
    } catch {
        Write-Host "❌ Test $i failed: $_" -ForegroundColor Red
        $failed++
    }
    
    Write-Host ""
}

# ============================================
# SUMMARY
# ============================================

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    TEST SUMMARY                           ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "📊 Results:" -ForegroundColor Yellow
Write-Host "   ✅ Passed: $passed/$NumTests" -ForegroundColor Green
Write-Host "   ❌ Failed: $failed/$NumTests" -ForegroundColor $(if ($failed -gt 0) { 'Red' } else { 'Green' })
Write-Host "   🔧 Total Tools Used: $totalToolsUsed" -ForegroundColor Cyan
Write-Host ""

if ($failed -eq 0) {
    Write-Host "🎉 ALL TESTS PASSED - AGENTIC PIPELINE WORKING!" -ForegroundColor Green
} else {
    Write-Host "⚠️ SOME TESTS FAILED - CHECK CONFIGURATION" -ForegroundColor Yellow
}

Write-Host ""
