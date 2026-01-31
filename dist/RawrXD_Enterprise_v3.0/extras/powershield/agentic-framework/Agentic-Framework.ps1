<#
.SYNOPSIS
    Enhanced Agentic Framework for Ollama - PowerShell version
.DESCRIPTION
    Turns any Ollama model into an agentic one using a ReAct loop:
    1. Think
    2. Call tools
    3. Observe results
    4. Repeat until done

    Uses the enhanced AgentTools.psm1 module for tool execution.
    All bugs fixed, enhanced with proper error handling and context management.
#>

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Prompt,

    [Parameter(Mandatory=$false)]
    [string]$Model = "bigdaddyg-personalized-agentic:latest",

    [Parameter(Mandatory=$false)]
    [int]$MaxIterations = 10,

    [Parameter(Mandatory=$false)]
    [string]$OllamaServer = "http://localhost:11434"
)

$ErrorActionPreference = "Continue"

# Ensure TLS 1.2+ for web requests
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13

# ============================================
# LOAD AGENT TOOLS MODULE
# ============================================

$toolsModulePath = Join-Path $PSScriptRoot "tools\AgentTools.psm1"
if (Test-Path $toolsModulePath) {
    try {
        Import-Module $toolsModulePath -Force -ErrorAction Stop
        Write-Host "✅ Loaded AgentTools module" -ForegroundColor Green
    }
    catch {
        Write-Host "⚠️  Could not load AgentTools module: $_" -ForegroundColor Yellow
        Write-Host "   Continuing with basic tools..." -ForegroundColor Yellow
    }
}

# Get available tools for system prompt
$availableTools = @()
if (Get-Command Get-AgentToolsList -ErrorAction SilentlyContinue) {
    $toolList = Get-AgentToolsList
    $availableTools = $toolList.Keys | Sort-Object
}
else {
    # Fallback to basic tools if module not loaded
    $availableTools = @("shell", "powershell", "read_file", "write_file", "web_search", "list_dir", "git_status", "task_orchestrator")
}

# ============================================
# AGENT LOOP
# ============================================

$SYSTEM_PROMPT = @"
You are Agent-1B, an agentic AI assistant powered by Ollama.

You may invoke these tools:
$($availableTools | ForEach-Object { "- $_" } | Out-String)

Tool parameters:
- shell: {"cmd": "command"}
- powershell: {"code": "powershell code"}
- read_file: {"path": "file path"}
- write_file: {"path": "file path", "content": "file content"}
- web_search: {"query": "search query"}
- list_dir: {"path": "directory path"}
- git_status: {"path": "directory path"}
- task_orchestrator: {"goal": "goal description", "max_steps": 10}

Call a tool by replying EXACTLY in this format:
TOOL:{name}:{json_args}

Example:
TOOL:shell:{"cmd": "Get-ChildItem | Measure-Object | Select-Object -ExpandProperty Count"}

When you have the final answer, reply:
ANSWER: <your answer>

Think step by step. Use tools when needed. Always provide a final ANSWER.
"@

function Invoke-Tool {
    param([string]$toolCall)

    if (-not $toolCall -match "^TOOL:([^:]+):(.+)$") {
        return "Error: Invalid tool call format. Use TOOL:name:json_args"
    }

    $toolName = $matches[1]
    $argJson = $matches[2]

    # Use module function if available
    if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
        return Invoke-AgentTool -ToolCall $toolCall
    }

    # Fallback to basic tool execution
    try {
        $args = $argJson | ConvertFrom-Json

        switch ($toolName) {
            "shell" {
                if ($args.cmd) {
                    return Invoke-ShellTool -cmd $args.cmd
                }
            }
            "powershell" {
                if ($args.code) {
                    return Invoke-PowerShellTool -code $args.code
                }
            }
            "read_file" {
                if ($args.path) {
                    return Invoke-ReadFileTool -path $args.path
                }
            }
            "write_file" {
                if ($args.path -and $args.content) {
                    return Invoke-WriteFileTool -path $args.path -content $args.content
                }
            }
            "web_search" {
                if ($args.query) {
                    return Invoke-WebSearchTool -query $args.query
                }
            }
            "list_dir" {
                $path = if ($args.path) { $args.path } else { "." }
                return Invoke-ListDirTool -path $path
            }
            "git_status" {
                $path = if ($args.path) { $args.path } else { "." }
                return Invoke-GitStatusTool -path $path
            }
            "task_orchestrator" {
                if ($args.goal) {
                    $maxSteps = if ($args.max_steps) { $args.max_steps } else { 10 }
                    return Invoke-TaskOrchestratorTool -goal $args.goal -max_steps $maxSteps
                }
            }
            default {
                return "Error: Unknown tool '$toolName'"
            }
        }

        return "Error: Missing required arguments for tool '$toolName'"
    }
    catch {
        return "Error parsing tool arguments: $_"
    }
}

function Start-AgenticLoop {
    param([string]$UserPrompt)

    $messages = @(
        @{ role = "system"; content = $SYSTEM_PROMPT },
        @{ role = "user"; content = $UserPrompt }
    )

    $iteration = 0

    while ($iteration -lt $MaxIterations) {
        $iteration++
        Write-Host "`n[Iteration $iteration]" -ForegroundColor Cyan

        try {
            # Call Ollama
            $requestBody = @{
                model = $Model
                messages = $messages
                stream = $false
            } | ConvertTo-Json -Depth 10

            $response = Invoke-RestMethod -Uri "$OllamaServer/api/chat" -Method POST `
                -Body $requestBody -ContentType "application/json" -TimeoutSec 60

            $reply = $response.message.content
            Write-Host "🤖 Agent: $reply" -ForegroundColor Yellow

            # Check for final answer
            if ($reply -match "^ANSWER:\s*(.+)$") {
                $finalAnswer = $matches[1].Trim()
                Write-Host "`n✅ FINAL ANSWER: $finalAnswer" -ForegroundColor Green
                return $finalAnswer
            }

            # Check for tool call
            if ($reply -match "^TOOL:") {
                $observation = Invoke-Tool -toolCall $reply

                # FIX: Truncate large observations to prevent context overflow
                $obsToSend = if ($observation.Length -gt 4000) {
                    $observation.Substring(0, 4000) + "… (truncated, original length: $($observation.Length) chars)"
                } else {
                    $observation
                }

                $obsPreview = if ($observation.Length -gt 200) {
                    $observation.Substring(0, 200) + "…"
                } else {
                    $observation
                }
                Write-Host "🔧 Tool Result: $obsPreview" -ForegroundColor Magenta

                # Add to conversation
                $messages += @{ role = "assistant"; content = $reply }
                $messages += @{ role = "user"; content = "Observation: $obsToSend" }
            }
            else {
                # Model forgot format - remind it
                Write-Host "⚠️  Format reminder sent" -ForegroundColor Yellow
                $messages += @{ role = "assistant"; content = $reply }
                $messages += @{ role = "user"; content = "Use TOOL:name:json_args or ANSWER: your_response" }
            }
        }
        catch {
            Write-Host "❌ Error: $_" -ForegroundColor Red
            $messages += @{ role = "user"; content = "Error occurred: $_" }
        }
    }

    Write-Host "`n⚠️  Max iterations reached. Returning last response." -ForegroundColor Yellow
    return $reply
}

# ============================================
# MAIN
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Enhanced Agentic Framework - PowerShell Edition" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Model: $Model" -ForegroundColor Gray
Write-Host "Available Tools: $($availableTools.Count)" -ForegroundColor Gray
Write-Host "Prompt: $Prompt" -ForegroundColor Gray
Write-Host ""

$result = Start-AgenticLoop -UserPrompt $Prompt

Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
return $result
