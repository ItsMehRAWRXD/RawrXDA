<#
.SYNOPSIS
    RawrXD Agentic Integration Module
.DESCRIPTION
    Integrates agentic Ollama models directly into RawrXD IDE
    Adds autonomous code generation, suggestions, and analysis
#>

# ============================================
# AGENTIC INTEGRATION FOR RAWRXD
# ============================================

$AgenticConfig = @{
    Enabled = $false
    Model = "cheetah-stealth-agentic:latest"
    Temperature = 0.9
    OllamaEndpoint = "http://localhost:11434"
    ContextWindow = 4096
}

function Enable-RawrXDAgentic {
    <#
    .SYNOPSIS
        Enable agentic mode in RawrXD
    .DESCRIPTION
        Activates autonomous reasoning for code generation and analysis
    #>
    param(
        [string]$Model = "cheetah-stealth-agentic:latest",
        [double]$Temperature = 0.9
    )
    
    Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║  🚀 ENABLING AGENTIC MODE FOR RAWRXD             ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Green
    
    try {
        # Test Ollama connection
        $response = Invoke-RestMethod -Uri "$($AgenticConfig.OllamaEndpoint)/api/tags" -TimeoutSec 3 -ErrorAction Stop
        
        $AgenticConfig.Enabled = $true
        $AgenticConfig.Model = $Model
        $AgenticConfig.Temperature = $Temperature
        
        Write-Host "`n✅ Agentic mode ACTIVATED for RawrXD" -ForegroundColor Green
        Write-Host "   Model: $Model" -ForegroundColor Cyan
        Write-Host "   Temperature: $Temperature" -ForegroundColor Cyan
        Write-Host "   Features:" -ForegroundColor Green
        Write-Host "      ✓ Autonomous code generation" -ForegroundColor Green
        Write-Host "      ✓ Smart code completions" -ForegroundColor Green
        Write-Host "      ✓ Context-aware suggestions" -ForegroundColor Green
        Write-Host "      ✓ Multi-file analysis" -ForegroundColor Green
        
        return $true
    }
    catch {
        Write-Host "❌ Failed to enable agentic mode: $_" -ForegroundColor Red
        return $false
    }
}

function Invoke-RawrXDAgenticCodeGen {
    <#
    .SYNOPSIS
        Generate code using agentic reasoning in RawrXD
    .DESCRIPTION
        Creates code based on prompt with full project context
    .PARAMETER Prompt
        Description of code to generate
    .PARAMETER Context
        Additional context (current file content, selection, etc.)
    .PARAMETER Language
        Programming language
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Prompt,
        
        [string]$Context = "",
        
        [string]$Language = "python"
    )
    
    if (-not $AgenticConfig.Enabled) {
        Write-Host "❌ Agentic mode is not enabled. Call Enable-RawrXDAgentic first." -ForegroundColor Red
        return $null
    }
    
    $fullPrompt = @"
You are an autonomous coding agent integrated into RawrXD IDE.
Your task: Generate code based on the following request.

LANGUAGE: $Language
REQUEST: $Prompt

$(if ($Context) { "`nCONTEXT:`n$Context" })

INSTRUCTIONS:
1. Generate ONLY the code, no explanations
2. Ensure code is production-ready
3. Include error handling where appropriate
4. Add comments for clarity
5. Follow best practices for $Language

BEGIN CODE:
"@
    
    try {
        Write-Host "⏳ Generating code..." -ForegroundColor Yellow -NoNewline
        
        $body = @{
            model = $AgenticConfig.Model
            prompt = $fullPrompt
            stream = $false
            options = @{
                temperature = $AgenticConfig.Temperature
                num_predict = 2000
            }
        } | ConvertTo-Json -Depth 5
        
        $response = Invoke-RestMethod -Uri "$($AgenticConfig.OllamaEndpoint)/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 120 `
            -ErrorAction Stop
        
        Write-Host " Done!" -ForegroundColor Green
        return $response.response
    }
    catch {
        Write-Host " Error!" -ForegroundColor Red
        Write-Host "Error: $_" -ForegroundColor Yellow
        return $null
    }
}

function Invoke-RawrXDAgenticCompletion {
    <#
    .SYNOPSIS
        Get smart code completions using agentic reasoning
    .DESCRIPTION
        Provides context-aware code suggestions
    .PARAMETER LinePrefix
        The current line prefix to complete
    .PARAMETER FileContext
        Complete file content for context
    .PARAMETER Language
        Programming language
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$LinePrefix,
        
        [string]$FileContext = "",
        
        [string]$Language = "python"
    )
    
    if (-not $AgenticConfig.Enabled) {
        return $null
    }
    
    $prompt = @"
Complete this $Language code line (return ONLY the completion, no explanation):

File Context:
$FileContext

Current Line to Complete:
$LinePrefix
"@
    
    try {
        $body = @{
            model = $AgenticConfig.Model
            prompt = $prompt
            stream = $false
            options = @{
                temperature = $AgenticConfig.Temperature
                num_predict = 100
            }
        } | ConvertTo-Json -Depth 5
        
        $response = Invoke-RestMethod -Uri "$($AgenticConfig.OllamaEndpoint)/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 30 `
            -ErrorAction Stop
        
        return $response.response.Trim()
    }
    catch {
        return $null
    }
}

function Invoke-RawrXDAgenticAnalysis {
    <#
    .SYNOPSIS
        Analyze code with agentic reasoning
    .DESCRIPTION
        Provides intelligent code analysis and suggestions
    .PARAMETER Code
        Code to analyze
    .PARAMETER AnalysisType
        Type of analysis: 'improve', 'debug', 'refactor', 'test', 'document'
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Code,
        
        [ValidateSet('improve', 'debug', 'refactor', 'test', 'document')]
        [string]$AnalysisType = 'improve'
    )
    
    if (-not $AgenticConfig.Enabled) {
        Write-Host "❌ Agentic mode is not enabled." -ForegroundColor Red
        return $null
    }
    
    $analysisPrompts = @{
        improve = "Analyze this code and suggest improvements for performance, readability, and best practices:"
        debug = "Analyze this code and identify potential bugs, edge cases, and issues:"
        refactor = "Analyze this code and suggest refactoring for better structure and maintainability:"
        test = "Analyze this code and suggest comprehensive test cases and edge cases to cover:"
        document = "Analyze this code and suggest documentation improvements and docstrings:"
    }
    
    $prompt = @"
$($analysisPrompts[$AnalysisType])

CODE:
$Code

Provide detailed analysis with actionable suggestions.
"@
    
    try {
        Write-Host "⏳ Analyzing code..." -ForegroundColor Yellow -NoNewline
        
        $body = @{
            model = $AgenticConfig.Model
            prompt = $prompt
            stream = $false
            options = @{
                temperature = $AgenticConfig.Temperature
                num_predict = 1500
            }
        } | ConvertTo-Json -Depth 5
        
        $response = Invoke-RestMethod -Uri "$($AgenticConfig.OllamaEndpoint)/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 120 `
            -ErrorAction Stop
        
        Write-Host " Done!" -ForegroundColor Green
        return $response.response
    }
    catch {
        Write-Host " Error!" -ForegroundColor Red
        return $null
    }
}

function Invoke-RawrXDAgenticRefactor {
    <#
    .SYNOPSIS
        Autonomously refactor code
    .DESCRIPTION
        Uses agentic reasoning to refactor code with explanations
    .PARAMETER Code
        Code to refactor
    .PARAMETER Language
        Programming language
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Code,
        
        [string]$Language = "python"
    )
    
    if (-not $AgenticConfig.Enabled) {
        return $null
    }
    
    $prompt = @"
Autonomously refactor this $Language code to be cleaner, more efficient, and follow best practices.
Return the refactored code followed by a brief explanation of changes.

ORIGINAL CODE:
$Code

REFACTORED CODE + EXPLANATION:
"@
    
    try {
        Write-Host "⏳ Refactoring code..." -ForegroundColor Yellow -NoNewline
        
        $body = @{
            model = $AgenticConfig.Model
            prompt = $prompt
            stream = $false
            options = @{
                temperature = $AgenticConfig.Temperature
                num_predict = 2000
            }
        } | ConvertTo-Json -Depth 5
        
        $response = Invoke-RestMethod -Uri "$($AgenticConfig.OllamaEndpoint)/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 120 `
            -ErrorAction Stop
        
        Write-Host " Done!" -ForegroundColor Green
        return $response.response
    }
    catch {
        Write-Host " Error!" -ForegroundColor Red
        return $null
    }
}

function Get-RawrXDAgenticStatus {
    <#
    .SYNOPSIS
        Show agentic mode status in RawrXD
    #>
    Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  📊 RAWRXD AGENTIC STATUS                          ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    $status = if ($AgenticConfig.Enabled) { "🚀 ACTIVE" } else { "⏸️  INACTIVE" }
    $statusColor = if ($AgenticConfig.Enabled) { "Green" } else { "Gray" }
    
    Write-Host "`nStatus: $status" -ForegroundColor $statusColor
    Write-Host "Model: $($AgenticConfig.Model)" -ForegroundColor Cyan
    Write-Host "Temperature: $($AgenticConfig.Temperature)" -ForegroundColor Cyan
    Write-Host "Endpoint: $($AgenticConfig.OllamaEndpoint)" -ForegroundColor Cyan
    
    if ($AgenticConfig.Enabled) {
        Write-Host "`n📚 Available Functions:" -ForegroundColor Green
        Write-Host "  • Invoke-RawrXDAgenticCodeGen      - Generate code" -ForegroundColor Green
        Write-Host "  • Invoke-RawrXDAgenticCompletion   - Get completions" -ForegroundColor Green
        Write-Host "  • Invoke-RawrXDAgenticAnalysis     - Analyze code" -ForegroundColor Green
        Write-Host "  • Invoke-RawrXDAgenticRefactor     - Refactor code" -ForegroundColor Green
    }
    
    Write-Host "`n"
}

# Export functions for use in RawrXD
Export-ModuleMember -Function @(
    'Enable-RawrXDAgentic',
    'Invoke-RawrXDAgenticCodeGen',
    'Invoke-RawrXDAgenticCompletion',
    'Invoke-RawrXDAgenticAnalysis',
    'Invoke-RawrXDAgenticRefactor',
    'Get-RawrXDAgenticStatus'
)

Write-Host "✅ RawrXD Agentic Module loaded" -ForegroundColor Green
