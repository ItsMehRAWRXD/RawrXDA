<#
.SYNOPSIS
    Agent Tools Module - Extensible tool collection for agentic AI
.DESCRIPTION
    Provides a collection of tools that can be called by agentic AI models
#>

# Export all tool functions
$ExportFunctions = @()

# Shell command execution
function Invoke-ShellTool {
    param([string]$cmd)
    try {
        $output = & powershell -Command $cmd 2>&1 | Out-String
        return $output.Trim()
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-ShellTool"

# PowerShell code execution
function Invoke-PowerShellTool {
    param([string]$code)
    try {
        $result = Invoke-Expression $code
        if ($null -eq $result) { return "ok" }
        return ($result | Out-String).Trim()
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-PowerShellTool"

# File operations
function Invoke-ReadFileTool {
    param([string]$path)
    try {
        if (Test-Path $path) {
            return Get-Content $path -Raw
        }
        return "Error: File not found: $path"
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-ReadFileTool"

function Invoke-WriteFileTool {
    param([string]$path, [string]$content)
    try {
        $content | Set-Content -Path $path -Force
        return "File written successfully: $path"
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-WriteFileTool"

# Web search
function Invoke-WebSearchTool {
    param([string]$query)
    try {
        $url = "https://api.duckduckgo.com/?q=$([System.Web.HttpUtility]::UrlEncode($query))&format=json&no_html=1&skip_disambig=1"
        $response = Invoke-RestMethod -Uri $url -TimeoutSec 10
        $results = @()
        if ($response.RelatedTopics) {
            $results = $response.RelatedTopics | Select-Object -First 5 | ForEach-Object {
                @{
                    Text = $_.Text
                    FirstURL = $_.FirstURL
                }
            }
        }
        return ($results | ConvertTo-Json -Depth 3)
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-WebSearchTool"

# List directory
function Invoke-ListDirTool {
    param([string]$path = ".")
    try {
        $items = Get-ChildItem -Path $path -ErrorAction SilentlyContinue | Select-Object Name, Length, LastWriteTime
        return ($items | ConvertTo-Json -Depth 2)
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-ListDirTool"

# Git operations
function Invoke-GitStatusTool {
    param([string]$path = ".")
    try {
        Push-Location $path
        $status = git status --short 2>&1 | Out-String
        Pop-Location
        return $status.Trim()
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-GitStatusTool"

# Task Orchestrator
function Invoke-TaskOrchestratorTool {
    param([string]$goal, [int]$max_steps = 10)
    try {
        $scriptPath = Join-Path $PSScriptRoot "..\TaskOrchestrator.ps1"
        if (Test-Path $scriptPath) {
            $output = & $scriptPath -Goal $goal -MaxSteps $max_steps 2>&1 | Out-String
            return $output.Trim()
        }
        return "Error: TaskOrchestrator.ps1 not found at $scriptPath"
    }
    catch {
        return "Error: $_"
    }
}
$ExportFunctions += "Invoke-TaskOrchestratorTool"

# Export module members
Export-ModuleMember -Function $ExportFunctions

