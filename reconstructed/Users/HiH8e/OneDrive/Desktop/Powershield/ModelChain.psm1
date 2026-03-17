function Invoke-ModelChain {
    <#
    .SYNOPSIS
        Execute a model chain for agentic code analysis
        
    .PARAMETER Chain
        Chain to execute (code_review, secure_coding, documentation, optimization, debugging)
        
    .PARAMETER Code
        Code content to process
        
    .PARAMETER File
        File path to read code from
        
    .PARAMETER Language
        Programming language (auto-detected if not specified)
        
    .PARAMETER Loops
        Number of feedback loops (default: 1)
        
    .EXAMPLE
        # Code review on file
        Invoke-ModelChain -Chain code_review -File "script.ps1"
        
        # Multiple feedback loops for security analysis
        Invoke-ModelChain -Chain secure_coding -File "app.js" -Loops 2
    #>
    
    param(
        [Parameter(Mandatory = $false)]
        [ValidateSet("code_review", "secure_coding", "documentation", "optimization", "debugging", "list")]
        [string]$Chain = "code_review",
        
        [Parameter(Mandatory = $false)]
        [string]$Code = "",
        
        [Parameter(Mandatory = $false)]
        [string]$File = "",
        
        [Parameter(Mandatory = $false)]
        [string]$Language = "unknown",
        
        [Parameter(Mandatory = $false)]
        [int]$Loops = 1
    )
    
    $scriptPath = Join-Path $PSScriptRoot "Model-Chain-Orchestrator.ps1"
    
    if (-not (Test-Path $scriptPath)) {
        Write-Error "Model-Chain-Orchestrator.ps1 not found at $scriptPath"
        return
    }
    
    $params = @{
        ChainId = $Chain
        Code = $Code
        FilePath = $File
        Language = $Language
        FeedbackLoops = $Loops
    }
    
    & $scriptPath @params
}

function Get-ModelChains {
    <#
    .SYNOPSIS
        List all available model chains
    #>
    
    $scriptPath = Join-Path $PSScriptRoot "Model-Chain-Orchestrator.ps1"
    & $scriptPath -ChainId "list"
}

# Export functions
Export-ModuleFunction -Function @("Invoke-ModelChain", "Get-ModelChains")
