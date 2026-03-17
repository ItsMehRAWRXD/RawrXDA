# ============================================
# CLI HANDLERS MODULE INDEX
# ============================================
# This file documents all available CLI handlers
# and provides quick reference for developers
# ============================================

<#
.SYNOPSIS
    RawrXD CLI Handlers Module System

.DESCRIPTION
    The CLI handlers are organized into categories:
    
    📁 modules/cli-handlers/
    ├── 📁 ollama/           - Ollama AI integration
    │   ├── test-ollama.ps1
    │   └── list-models.ps1
    │
    ├── 📁 git/              - Git version control
    │   └── git-status.ps1
    │
    ├── 📁 agents/           - Agent task automation
    │   ├── create-agent.ps1
    │   └── list-agents.ps1
    │
    ├── 📁 marketplace/      - Extension marketplace
    │   └── marketplace-sync.ps1
    │
    ├── 📁 vscode/           - VSCode Marketplace API
    │   ├── vscode-popular.ps1
    │   └── vscode-search.ps1
    │
    ├── 📁 testing/          - Diagnostic and testing
    │   └── diagnose.ps1
    │
    └── 📁 general/          - General utilities
        └── help.ps1

.NOTES
    Each handler file must:
    1. Define a function named Invoke-Cli<CommandName>
    2. Include .SYNOPSIS documentation
    3. Return $true (success) or $false (failure)
    4. Handle errors gracefully with try/catch
    
.EXAMPLE
    # Load all handlers
    . .\modules\cli-handlers-loader.ps1
    Initialize-CliHandlers
    
    # Invoke a command
    Invoke-CliCommand -CommandName "test-ollama"
    
    # Get available commands
    Get-CliCommands
#>

# ============================================
# HANDLER REGISTRY
# ============================================
# Quick reference for all available handlers

$HandlerRegistry = @{
    # Ollama Commands
    "test-ollama"         = @{
        Function    = "Invoke-CliTestOllama"
        File        = "ollama/test-ollama.ps1"
        Description = "Test Ollama connection and available models"
        Parameters  = @()
    }
    "list-models"         = @{
        Function    = "Invoke-CliListModels"
        File        = "ollama/list-models.ps1"
        Description = "List all available Ollama models with details"
        Parameters  = @()
    }
    
    # Git Commands
    "git-status"          = @{
        Function    = "Invoke-CliGitStatus"
        File        = "git/git-status.ps1"
        Description = "Show formatted git status"
        Parameters  = @()
    }
    
    # Agent Commands
    "create-agent"        = @{
        Function    = "Invoke-CliCreateAgent"
        File        = "agents/create-agent.ps1"
        Description = "Create a new agent task"
        Parameters  = @("AgentName", "Prompt")
    }
    "list-agents"         = @{
        Function    = "Invoke-CliListAgents"
        File        = "agents/list-agents.ps1"
        Description = "List all agent tasks"
        Parameters  = @()
    }
    
    # Marketplace Commands
    "marketplace-sync"    = @{
        Function    = "Invoke-CliMarketplaceSync"
        File        = "marketplace/marketplace-sync.ps1"
        Description = "Sync marketplace catalog"
        Parameters  = @()
    }
    
    # VSCode Commands
    "vscode-popular"      = @{
        Function    = "Invoke-CliVSCodePopular"
        File        = "vscode/vscode-popular.ps1"
        Description = "Get top VSCode extensions from live API"
        Parameters  = @()
    }
    "vscode-search"       = @{
        Function    = "Invoke-CliVSCodeSearch"
        File        = "vscode/vscode-search.ps1"
        Description = "Search VSCode Marketplace"
        Parameters  = @("Prompt")
    }
    
    # Testing Commands
    "diagnose"            = @{
        Function    = "Invoke-CliDiagnose"
        File        = "testing/diagnose.ps1"
        Description = "Run comprehensive diagnostic checks"
        Parameters  = @()
    }
    
    # General Commands
    "help"                = @{
        Function    = "Invoke-CliHelp"
        File        = "general/help.ps1"
        Description = "Show CLI help and available commands"
        Parameters  = @()
    }
}

# ============================================
# ADDING NEW HANDLERS
# ============================================
<#
To add a new CLI handler:

1. Create a new .ps1 file in the appropriate category folder:
   modules/cli-handlers/<category>/<command-name>.ps1

2. Define the handler function with this template:

   function Invoke-Cli<CommandName> {
       <#
       .SYNOPSIS
           Brief description of the command
       .DESCRIPTION
           Detailed description
       .PARAMETER ParamName
           Parameter description
       .EXAMPLE
           .\RawrXD.ps1 -CliMode -Command <command-name>
       .OUTPUTS
           [bool] $true if successful, $false otherwise
       #>
       param(
           [Parameter(Mandatory = $false)]
           [string]$ParamName
       )
       
       try {
           # Command implementation
           Write-Host "Command output" -ForegroundColor Cyan
           return $true
       }
       catch {
           Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
           return $false
       }
   }

3. Add the command to RawrXD.ps1 ValidateSet (optional, for tab completion):
   [ValidateSet('test-ollama', 'new-command', ...)]

4. The handler loader will automatically discover and register the new handler.

#>

# ============================================
# HANDLER CONVENTIONS
# ============================================
<#
Naming Conventions:
- File name: <command-name>.ps1 (lowercase, hyphen-separated)
- Function name: Invoke-Cli<CommandName> (PascalCase after Cli prefix)
- Example: git-status.ps1 → Invoke-CliGitStatus

Output Conventions:
- Use Write-Host for user-facing output
- Color coding:
  - Cyan: Headers and section titles
  - Green: Success messages
  - Yellow: Warnings and highlights
  - Red: Errors
  - Gray: Secondary information
  - White: Primary content

Return Values:
- $true: Command completed successfully
- $false: Command failed (errors should be displayed)

Error Handling:
- Always use try/catch blocks
- Display user-friendly error messages
- Log detailed errors for debugging

Parameter Validation:
- Use [ValidateNotNullOrEmpty()] for required strings
- Use regex patterns for format validation
- Limit string lengths to prevent abuse
#>

# Export registry for external use
$script:CliHandlerRegistry = $HandlerRegistry
