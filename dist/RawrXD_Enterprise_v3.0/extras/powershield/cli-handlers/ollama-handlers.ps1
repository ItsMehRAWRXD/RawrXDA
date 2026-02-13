<#
.SYNOPSIS
    Ollama-related CLI command handlers
.DESCRIPTION
    Handles test-ollama, list-models, chat, and analyze-file commands
#>

function Invoke-OllamaTestHandler {
    if (-not (Invoke-CliTestOllama)) { return 1 }
    return 0
}

function Invoke-OllamaListModelsHandler {
    if (-not (Invoke-CliListModels)) { return 1 }
    return 0
}

function Invoke-OllamaChatHandler {
    param([string]$Model)
    if (-not (Invoke-CliChat -Model $Model)) { return 1 }
    return 0
}

function Invoke-OllamaAnalyzeFileHandler {
    param(
        [string]$FilePath,
        [string]$Model,
        [double]$ThresholdValue
    )
    
    if (-not $FilePath) {
        Write-Host "Error: -FilePath parameter is required for analyze-file command" -ForegroundColor Red
        return 1
    }
    
    $params = @{
        FilePath = $FilePath
    }
    if ($Model) { $params.Model = $Model }
    if ($ThresholdValue) { $params.ThresholdValue = $ThresholdValue }
    
    if (-not (Invoke-CliAnalyzeFile @params)) { return 1 }
    return 0
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
