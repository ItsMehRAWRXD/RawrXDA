# ============================================
# CLI HANDLER: list-agents
# ============================================
# Category: agents
# Command: list-agents
# Purpose: List all agent tasks
# ============================================

function Invoke-CliListAgents {
    <#
    .SYNOPSIS
        List all agent tasks
    .DESCRIPTION
        Displays all configured agents with their status and configuration details.
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command list-agents
    .OUTPUTS
        [bool] $true if successful, $false otherwise
    #>
    
    Write-Host "`n=== Agent Tasks ===" -ForegroundColor Cyan
    
    try {
        # Find agents directory
        $agentsPath = Join-Path $PSScriptRoot "agents"
        
        if (-not (Test-Path $agentsPath)) {
            Write-Host "`nNo agents directory found." -ForegroundColor Yellow
            Write-Host "Create an agent: .\RawrXD.ps1 -CliMode -Command create-agent -AgentName MyAgent" -ForegroundColor Gray
            return $true
        }
        
        # Get all agent files
        $agentFiles = Get-ChildItem -Path $agentsPath -Filter "*.json" -File -ErrorAction SilentlyContinue
        
        if ($agentFiles.Count -eq 0) {
            Write-Host "`nNo agents configured." -ForegroundColor Yellow
            Write-Host "Create an agent: .\RawrXD.ps1 -CliMode -Command create-agent -AgentName MyAgent" -ForegroundColor Gray
            return $true
        }
        
        Write-Host "`nFound $($agentFiles.Count) agent(s):`n" -ForegroundColor Green
        
        foreach ($file in $agentFiles) {
            try {
                $agent = Get-Content -Path $file.FullName -Raw | ConvertFrom-Json
                
                # Status indicator
                $statusIcon = switch ($agent.Status) {
                    "Active" { "🟢" }
                    "Paused" { "🟡" }
                    "Error" { "🔴" }
                    default { "⚪" }
                }
                
                Write-Host "$statusIcon $($agent.Name)" -ForegroundColor White
                Write-Host "   Status: $($agent.Status)" -ForegroundColor $(if ($agent.Status -eq "Active") { "Green" } else { "Yellow" })
                Write-Host "   Model: $($agent.Model)" -ForegroundColor Gray
                Write-Host "   Created: $($agent.CreatedAt)" -ForegroundColor Gray
                
                if ($agent.Prompt) {
                    $promptPreview = if ($agent.Prompt.Length -gt 60) { 
                        $agent.Prompt.Substring(0, 60) + "..." 
                    } else { 
                        $agent.Prompt 
                    }
                    Write-Host "   Prompt: $promptPreview" -ForegroundColor Gray
                }
                
                if ($agent.RunCount -gt 0) {
                    Write-Host "   Runs: $($agent.RunCount) | Last: $($agent.LastRun)" -ForegroundColor Gray
                }
                
                Write-Host ""
            }
            catch {
                Write-Host "⚠ Error reading agent file: $($file.Name)" -ForegroundColor Yellow
            }
        }
        
        Write-Host "Commands:" -ForegroundColor Cyan
        Write-Host "  Create: .\RawrXD.ps1 -CliMode -Command create-agent -AgentName <name>" -ForegroundColor Gray
        Write-Host "  Run:    .\RawrXD.ps1 -CliMode -Command run-agent -AgentName <name>" -ForegroundColor Gray
        Write-Host ""
        
        return $true
    }
    catch {
        Write-Host "✗ Failed to list agents" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliListAgents
}
