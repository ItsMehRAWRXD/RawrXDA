#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Natural Language to Machine Command Translator

.DESCRIPTION
    Translates human requests into executable machine commands.
    The chatbot speaks "machine language" to your tools.
    
    Examples:
    - "send 5 agents to analyze D:\code" → .\swarm_control.ps1 -Operation deploy -TargetDirectory "D:\code" -SwarmSize 5
    - "create a 7B model for coding" → .\model_agent_making_station.ps1 -Operation create -Template "Small-7B"
    - "add a todo to fix the parser" → .\todo_manager.ps1 -Operation add -Title "fix parser"

.PARAMETER Request
    Natural language request

.PARAMETER Execute
    Actually execute the command (default: just show it)

.EXAMPLE
    .\command_translator.ps1 -Request "send 3 agents to D:\test"
    
.EXAMPLE
    .\command_translator.ps1 -Request "create a 13B model" -Execute
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Request,
    
    [Parameter(Mandatory=$false)]
    [switch]$Execute,
    
    [Parameter(Mandatory=$false)]
    [switch]$ShowExplanation
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# COMMAND TRANSLATOR ENGINE
# ═══════════════════════════════════════════════════════════════════════════════

class CommandTranslator {
    [hashtable]$CommandPatterns
    [System.Collections.ArrayList]$TranslationLog
    
    CommandTranslator() {
        $this.TranslationLog = [System.Collections.ArrayList]::new()
        $this.InitializePatterns()
    }
    
    [void] InitializePatterns() {
        $this.CommandPatterns = @{
            # SWARM OPERATIONS
            "swarm_deploy" = @{
                Patterns = @(
                    "send\s+(\d+)?\s*(?:agents?|swarms?)\s+to\s+(.+?)(?:\s+to\s+(.+))?$"
                    "deploy\s+(\d+)?\s*(?:agents?|swarms?)\s+(?:to|at)\s+(.+?)(?:\s+(?:to|for)\s+(.+))?$"
                    "create\s+(?:a\s+)?swarm\s+(?:of\s+)?(\d+)?\s*(?:agents?)?\s+(?:in|at)\s+(.+?)(?:\s+(?:to|for)\s+(.+))?$"
                )
                Template = '.\swarm_control.ps1 -Operation deploy -TargetDirectory "{1}" -SwarmSize {0} -Task "{2}"'
                Defaults = @{ SwarmSize = 5; Task = "analyze and process files" }
                Explanation = "Deploys AI agents to specified directory to perform tasks"
            }
            
            "swarm_monitor" = @{
                Patterns = @(
                    "monitor\s+(?:the\s+)?swarms?"
                    "check\s+swarm\s+status"
                    "show\s+(?:me\s+)?(?:the\s+)?swarm\s+(?:status|activity)"
                    "watch\s+(?:the\s+)?swarms?"
                )
                Template = '.\swarm_control.ps1 -Operation monitor -Watch'
                Explanation = "Monitors active swarm agents in real-time"
            }
            
            "swarm_stop" = @{
                Patterns = @(
                    "stop\s+(?:the\s+)?swarms?"
                    "kill\s+(?:all\s+)?swarms?"
                    "terminate\s+swarms?"
                )
                Template = '.\swarm_control.ps1 -Operation stop'
                Explanation = "Stops all running swarm agents"
            }
            
            # MODEL OPERATIONS
            "model_create" = @{
                Patterns = @(
                    "create\s+(?:a\s+)?(\d+[BM])\s+model(?:\s+(?:for|about)\s+(.+))?$"
                    "make\s+(?:a\s+)?(\d+[BM])\s+model(?:\s+(?:for|about)\s+(.+))?$"
                    "build\s+(?:a\s+)?(\d+[BM])\s+model(?:\s+(?:for|about)\s+(.+))?$"
                )
                Template = '.\model_agent_making_station.ps1 -Operation create -Template "{0}" -CustomPrompt "{1}"'
                SizeMap = @{
                    "7B" = "Small-7B"; "13B" = "Standard-13B"; "30B" = "Medium-30B"
                    "50B" = "Large-50B"; "120B" = "Master-120B"; "800B" = "Supreme-800B"
                }
                Defaults = @{ CustomPrompt = "You are a helpful AI assistant" }
                Explanation = "Creates a new AI model with specified parameters"
            }
            
            "model_train" = @{
                Patterns = @(
                    "train\s+(?:the\s+)?model\s+(?:at\s+|from\s+)?(.+?)(?:\s+with\s+(.+))?$"
                    "train\s+(.+?)(?:\s+with\s+(?:data\s+from\s+)?(.+))?$"
                )
                Template = '.\model_agent_making_station.ps1 -Operation train -ModelPath "{0}" -DataPath "{1}"'
                Explanation = "Trains a model with specified training data"
            }
            
            "model_quantize" = @{
                Patterns = @(
                    "quantize\s+(?:the\s+)?model\s+(?:at\s+)?(.+?)(?:\s+to\s+)?(\w+)?$"
                    "compress\s+(?:the\s+)?model\s+(?:at\s+)?(.+?)(?:\s+to\s+)?(\w+)?$"
                )
                Template = '.\model_agent_making_station.ps1 -Operation quantize -ModelPath "{0}" -QuantType "{1}"'
                Defaults = @{ QuantType = "Q4_K_M" }
                Explanation = "Quantizes model to reduce size while maintaining quality"
            }
            
            # TODO OPERATIONS
            "todo_add" = @{
                Patterns = @(
                    "add\s+(?:a\s+)?todo\s+(?:to\s+)?(.+)$"
                    "create\s+(?:a\s+)?todo\s+(?:to\s+)?(.+)$"
                    "remind\s+me\s+to\s+(.+)$"
                    "i\s+need\s+to\s+(.+)$"
                )
                Template = '.\todo_manager.ps1 -Operation add -Title "{0}"'
                Explanation = "Adds a new todo item to your task list"
            }
            
            "todo_list" = @{
                Patterns = @(
                    "(?:show|list|get)\s+(?:my\s+)?todos?"
                    "what\s+(?:are\s+my|do\s+i\s+have\s+for)\s+todos?"
                )
                Template = '.\todo_manager.ps1 -Operation list'
                Explanation = "Lists all pending todo items"
            }
            
            "todo_complete" = @{
                Patterns = @(
                    "complete\s+todo\s+(\d+)"
                    "(?:finish|done)\s+(?:todo\s+)?(\d+)"
                    "mark\s+(\d+)\s+(?:as\s+)?(?:complete|done)"
                )
                Template = '.\todo_manager.ps1 -Operation complete -TodoId {0}'
                Explanation = "Marks a todo item as completed"
            }
            
            # BENCHMARK OPERATIONS
            "benchmark_format" = @{
                Patterns = @(
                    "benchmark\s+(?:model\s+)?formats?"
                    "compare\s+(?:model\s+)?formats?"
                    "test\s+format\s+performance"
                )
                Template = '.\benchmark_formats.ps1 -Operation compare'
                Explanation = "Benchmarks different model formats for performance comparison"
            }
            
            "benchmark_model" = @{
                Patterns = @(
                    "benchmark\s+(?:the\s+)?model\s+(?:at\s+)?(.+)$"
                    "test\s+(?:the\s+)?model\s+(?:at\s+)?(.+)$"
                    "profile\s+(?:the\s+)?model\s+(?:at\s+)?(.+)$"
                )
                Template = '.\benchmark_formats.ps1 -Operation benchmark -ModelPath "{0}"'
                Explanation = "Runs comprehensive benchmark on specified model"
            }
            
            # FILE OPERATIONS
            "file_search" = @{
                Patterns = @(
                    "find\s+(?:files?\s+)?(?:named\s+|called\s+)?(.+)$"
                    "search\s+for\s+(?:files?\s+)?(.+)$"
                    "locate\s+(?:files?\s+)?(.+)$"
                )
                Template = 'Get-ChildItem -Path "D:\lazy init ide" -Recurse -Filter "*{0}*" -File'
                Explanation = "Searches for files matching the pattern"
            }
            
            "file_open" = @{
                Patterns = @(
                    "open\s+(?:the\s+)?(?:file\s+)?(.+)$"
                    "show\s+(?:me\s+)?(?:the\s+)?(?:file\s+)?(.+)$"
                )
                Template = 'code "{0}"'
                Explanation = "Opens the specified file in editor"
            }
            
            # BROWSER OPERATIONS
            "browser_search" = @{
                Patterns = @(
                    "search\s+(?:the\s+)?(?:web|internet|online)\s+for\s+(.+)$"
                    "look\s+up\s+(.+)\s+online"
                    "google\s+(.+)$"
                    "find\s+(?:information|info)\s+about\s+(.+)\s+online"
                )
                Template = 'Start-Process "https://www.google.com/search?q={0}"'
                Explanation = "Searches the web for information"
            }
            
            "browser_open" = @{
                Patterns = @(
                    "open\s+(?:the\s+)?(?:url|website|link)\s+(.+)$"
                    "go\s+to\s+(.+)$"
                    "browse\s+(?:to\s+)?(.+)$"
                )
                Template = 'Start-Process "{0}"'
                Explanation = "Opens the specified URL in browser"
            }
            
            # ADVANCED OPERATIONS
            "virtual_quant" = @{
                Patterns = @(
                    "(?:enable|activate|turn\s+on)\s+virtual\s+quant(?:ization)?"
                    "use\s+virtual\s+quant(?:ization)?"
                )
                Template = 'Import-Module .\Advanced-Model-Operations.psm1; Set-VirtualQuantizationState -Enable'
                Explanation = "Enables virtual quantization for on-the-fly model compression"
            }
            
            "prune_model" = @{
                Patterns = @(
                    "prune\s+(?:the\s+)?model\s+(?:at\s+)?(.+)$"
                    "optimize\s+(?:the\s+)?model\s+(?:at\s+)?(.+)$"
                )
                Template = 'Import-Module .\Advanced-Model-Operations.psm1; Invoke-IntelligentPruning -ModelPath "{0}"'
                Explanation = "Intelligently prunes model to reduce size and increase speed"
            }
        }
    }
    
    [hashtable] Translate([string]$request) {
        $requestLower = $request.ToLower().Trim()
        
        foreach ($commandType in $this.CommandPatterns.Keys) {
            $config = $this.CommandPatterns[$commandType]
            
            foreach ($pattern in $config.Patterns) {
                if ($requestLower -match $pattern) {
                    return $this.BuildCommand($commandType, $config, $matches)
                }
            }
        }
        
        return @{
            Success = $false
            Message = "I couldn't understand that request. Try being more specific."
            Suggestions = $this.GetSuggestions($request)
        }
    }
    
    [hashtable] BuildCommand([string]$type, [hashtable]$config, [System.Text.RegularExpressions.Match]$matches) {
        $command = $config.Template
        
        # Extract captured groups
        $params = @()
        for ($i = 1; $i -lt $matches.Groups.Count; $i++) {
            $value = $matches.Groups[$i].Value.Trim()
            
            # Apply size mapping if available
            if ($config.ContainsKey("SizeMap") -and $config.SizeMap.ContainsKey($value)) {
                $value = $config.SizeMap[$value]
            }
            
            # Use default if empty
            if ([string]::IsNullOrWhiteSpace($value) -and $config.ContainsKey("Defaults")) {
                $defaultKey = @("SwarmSize", "Task", "CustomPrompt", "QuantType", "DataPath")[$i - 1]
                if ($config.Defaults.ContainsKey($defaultKey)) {
                    $value = $config.Defaults[$defaultKey]
                }
            }
            
            $params += $value
        }
        
        # Format command with parameters
        try {
            $command = $command -f $params
        }
        catch {
            Write-Verbose "Failed to format command: $_"
        }
        
        # Clean up empty parameters
        $command = $command -replace '\s+-\w+\s+""', ''
        $command = $command -replace '\s+-\w+\s+\{\}', ''
        
        # Log translation
        $this.TranslationLog.Add(@{
            Timestamp = Get-Date
            Type = $type
            Request = $matches[0].Value
            Command = $command
        }) | Out-Null
        
        return @{
            Success = $true
            Type = $type
            Command = $command
            Explanation = $config.Explanation
            Parameters = $params
        }
    }
    
    [array] GetSuggestions([string]$request) {
        $suggestions = @()
        
        if ($request -match 'swarm|agent') {
            $suggestions += "send 5 agents to D:\path"
            $suggestions += "monitor swarm status"
        }
        if ($request -match 'model|create|train') {
            $suggestions += "create a 7B model"
            $suggestions += "train model.gguf with training_data"
        }
        if ($request -match 'todo|task') {
            $suggestions += "add a todo to fix the parser"
            $suggestions += "show my todos"
        }
        if ($request -match 'search|find|look') {
            $suggestions += "search the web for information"
            $suggestions += "find files named script"
        }
        
        return $suggestions
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

$translator = [CommandTranslator]::new()
$result = $translator.Translate($Request)

if ($result.Success) {
    Write-Host "`n╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                    COMMAND TRANSLATOR                                         ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    Write-Host "  📝 Your Request:" -ForegroundColor Yellow
    Write-Host "     $Request`n" -ForegroundColor White
    
    Write-Host "  🤖 Machine Command:" -ForegroundColor Green
    Write-Host "     $($result.Command)`n" -ForegroundColor Cyan
    
    if ($ShowExplanation) {
        Write-Host "  💡 What it does:" -ForegroundColor Magenta
        Write-Host "     $($result.Explanation)`n" -ForegroundColor Gray
    }
    
    if ($Execute) {
        Write-Host "  ⚡ Executing...`n" -ForegroundColor Yellow
        Write-Host "─────────────────────────────────────────────────────────────────────────────`n" -ForegroundColor DarkGray
        
        try {
            Invoke-Expression $result.Command
        }
        catch {
            Write-Host "`n  ❌ Execution failed: $_`n" -ForegroundColor Red
        }
    }
    else {
        Write-Host "  ℹ️  Add -Execute flag to run this command" -ForegroundColor Gray
    }
}
else {
    Write-Host "`n❓ I couldn't translate that request.`n" -ForegroundColor Yellow
    Write-Host "  Message: $($result.Message)`n" -ForegroundColor Gray
    
    if ($result.Suggestions.Count -gt 0) {
        Write-Host "  Try something like:" -ForegroundColor Cyan
        foreach ($suggestion in $result.Suggestions) {
            Write-Host "    • $suggestion" -ForegroundColor White
        }
    }
    
    Write-Host "`n  💡 Tip: Use natural language like:" -ForegroundColor Magenta
    Write-Host "     'send 5 agents to D:\code'" -ForegroundColor Gray
    Write-Host "     'create a 7B model for coding'" -ForegroundColor Gray
    Write-Host "     'add a todo to fix bug'" -ForegroundColor Gray
    Write-Host ""
}
