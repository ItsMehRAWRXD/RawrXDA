# ============================================
# CLI HANDLER: create-agent
# ============================================
# Category: agents
# Command: create-agent
# Purpose: Create a new agent task
# ============================================

function Invoke-CliCreateAgent {
    <#
    .SYNOPSIS
        Create a new agent task
    .DESCRIPTION
        Creates a new automated agent task with the specified name and prompt.
        Validates input and stores agent configuration for later execution.
    .PARAMETER AgentName
        Name for the new agent (required)
    .PARAMETER Prompt
        Task prompt or description for the agent
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command create-agent -AgentName "CodeReviewer" -Prompt "Review code for security issues"
    .OUTPUTS
        [bool] $true if agent created successfully, $false otherwise
    #>
    param(
        [Parameter(Mandatory = $true)]
        [ValidateNotNullOrEmpty()]
        [string]$AgentName,
        
        [Parameter(Mandatory = $false)]
        [string]$Prompt = ""
    )
    
    Write-Host "`n=== Creating Agent: $AgentName ===" -ForegroundColor Cyan
    
    try {
        # Validate agent name (alphanumeric, dashes, underscores only)
        if ($AgentName -notmatch '^[a-zA-Z0-9_-]+$') {
            Write-Host "✗ Invalid agent name. Use only letters, numbers, dashes, and underscores." -ForegroundColor Red
            return $false
        }
        
        # Limit name length
        if ($AgentName.Length -gt 50) {
            Write-Host "✗ Agent name too long (max 50 characters)" -ForegroundColor Red
            return $false
        }
        
        # Create agents directory if not exists
        $agentsPath = Join-Path $PSScriptRoot "agents"
        if (-not (Test-Path $agentsPath)) {
            New-Item -ItemType Directory -Path $agentsPath -Force | Out-Null
        }
        
        # Check if agent already exists
        $agentFile = Join-Path $agentsPath "$AgentName.json"
        if (Test-Path $agentFile) {
            Write-Host "⚠ Agent '$AgentName' already exists. Overwrite? (y/n): " -NoNewline -ForegroundColor Yellow
            $confirm = Read-Host
            if ($confirm -ne 'y') {
                Write-Host "Cancelled." -ForegroundColor Gray
                return $false
            }
        }
        
        # Create agent configuration
        $agentConfig = @{
            Name        = $AgentName
            Prompt      = $Prompt
            CreatedAt   = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
            Status      = "Active"
            Model       = "llama2"
            RunCount    = 0
            LastRun     = $null
            Settings    = @{
                MaxTokens   = 2048
                Temperature = 0.7
                TopP        = 0.9
            }
        }
        
        # Save agent
        $agentConfig | ConvertTo-Json -Depth 3 | Set-Content -Path $agentFile -Encoding UTF8
        
        Write-Host "✓ Agent '$AgentName' created successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Agent Configuration:" -ForegroundColor Yellow
        Write-Host "  Name: $AgentName" -ForegroundColor White
        Write-Host "  Prompt: $(if ($Prompt) { $Prompt } else { '(none)' })" -ForegroundColor White
        Write-Host "  Status: Active" -ForegroundColor White
        Write-Host "  File: $agentFile" -ForegroundColor Gray
        Write-Host ""
        Write-Host "To run this agent:" -ForegroundColor Cyan
        Write-Host "  .\RawrXD.ps1 -CliMode -Command run-agent -AgentName $AgentName" -ForegroundColor Gray
        Write-Host ""
        
        return $true
    }
    catch {
        Write-Host "✗ Failed to create agent" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliCreateAgent
}
