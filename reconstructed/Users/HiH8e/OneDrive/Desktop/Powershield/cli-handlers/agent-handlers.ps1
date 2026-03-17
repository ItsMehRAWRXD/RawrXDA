<#
.SYNOPSIS
    Agent and diagnostic CLI command handlers
.DESCRIPTION
    Handles git-status, create-agent, list-agents, diagnose, and help commands
#>

function Invoke-GitStatusHandler {
  if (-not (Invoke-CliGitStatus)) { return 1 }
  return 0
}

function Invoke-CreateAgentHandler {
  param(
    [string]$AgentName,
    [string]$Prompt
  )
    
  if (-not $AgentName) {
    Write-Host "Error: -AgentName parameter is required for create-agent command" -ForegroundColor Red
    return 1
  }
    
  if (-not (Invoke-CliCreateAgent -AgentName $AgentName -Prompt $Prompt)) { return 1 }
  return 0
}

function Invoke-ListAgentsHandler {
  if (-not (Invoke-CliListAgents)) { return 1 }
  return 0
}

function Invoke-DiagnoseHandler {
  if (-not (Invoke-CliDiagnose)) { return 1 }
  return 0
}

function Invoke-HelpHandler {
  Show-CliHelp
  return 0
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Functions exported:
#   'Invoke-GitStatusHandler',
#   'Invoke-CreateAgentHandler',
#   'Invoke-ListAgentsHandler',
#   'Invoke-DiagnoseHandler',
#   'Invoke-HelpHandler'






