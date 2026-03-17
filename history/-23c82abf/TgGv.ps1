# ============================================
# CLI HANDLER: help
# ============================================
# Category: general
# Command: help
# Purpose: Show CLI help and available commands
# ============================================

function Invoke-CliHelp {
  <#
    .SYNOPSIS
        Display help for CLI commands
    .DESCRIPTION
        Shows all available CLI commands with descriptions and usage examples.
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command help
    .OUTPUTS
        [bool] $true always
    #>
    
  Write-Host "`n=== RawrXD CLI Help ===" -ForegroundColor Cyan
  Write-Host "`nUsage: .\RawrXD.ps1 -CliMode -Command <command> [options]`n" -ForegroundColor Yellow
    
  Write-Host "Available Commands:" -ForegroundColor Green
  Write-Host ""
    
  $commands = @(
    # Ollama Commands
    @{Name = "test-ollama"; Description = "Test Ollama connection and list available models"; Category = "Ollama" }
    @{Name = "list-models"; Description = "List all available Ollama models with details"; Category = "Ollama" }
    @{Name = "chat"; Description = "Start an interactive chat session"; Options = "-Model <model_name>"; Category = "Ollama" }
    @{Name = "analyze-file"; Description = "Analyze a file with AI"; Options = "-FilePath <path> -Model <model_name>"; Category = "Ollama" }
        
    # Git Commands
    @{Name = "git-status"; Description = "Show formatted git status"; Category = "Git" }
        
    # Agent Commands
    @{Name = "create-agent"; Description = "Create a new agent task"; Options = "-AgentName <name> -Prompt <task>"; Category = "Agents" }
    @{Name = "list-agents"; Description = "List all agent tasks"; Category = "Agents" }
        
    # Marketplace Commands
    @{Name = "marketplace-sync"; Description = "Sync marketplace catalog and browse extensions"; Category = "Marketplace" }
    @{Name = "marketplace-search"; Description = "Search for extensions in marketplace"; Options = "-Prompt <search_term>"; Category = "Marketplace" }
    @{Name = "marketplace-install"; Description = "Install an extension from marketplace"; Options = "-Prompt <extension_id>"; Category = "Marketplace" }
    @{Name = "list-extensions"; Description = "List installed extensions"; Category = "Marketplace" }
        
    # VSCode Marketplace Commands
    @{Name = "vscode-popular"; Description = "🌐 Get top 15 popular VSCode extensions (Live API)"; Category = "VSCode" }
    @{Name = "vscode-search"; Description = "🌐 Search VSCode Marketplace (Live API)"; Options = "-Prompt <search_term>"; Category = "VSCode" }
    @{Name = "vscode-install"; Description = "🌐 Install VSCode extension (Live API)"; Options = "-Prompt <extension_id>"; Category = "VSCode" }
    @{Name = "vscode-categories"; Description = "🌐 Browse VSCode extension categories"; Category = "VSCode" }
        
    # Testing Commands
    @{Name = "diagnose"; Description = "Run comprehensive diagnostic checks"; Category = "Testing" }
    @{Name = "test-editor-settings"; Description = "Test editor settings (colors, fonts, syntax)"; Category = "Testing" }
    @{Name = "test-file-operations"; Description = "Test file operations (open, save, read)"; Category = "Testing" }
    @{Name = "test-settings-persistence"; Description = "Test settings save/load persistence"; Category = "Testing" }
    @{Name = "test-all-features"; Description = "Run comprehensive test suite"; Category = "Testing" }
        
    # Settings Commands
    @{Name = "get-settings"; Description = "Get current settings"; Options = "-SettingName <name> (optional)"; Category = "Settings" }
    @{Name = "set-setting"; Description = "Set a specific setting"; Options = "-SettingName <name> -SettingValue <value>"; Category = "Settings" }
        
    # General Commands
    @{Name = "help"; Description = "Show this help message"; Category = "General" }
  )
    
  # Group by category
  $categories = $commands | Group-Object -Property Category
    
  foreach ($cat in $categories) {
    Write-Host "  $($cat.Name.ToUpper())" -ForegroundColor Cyan
        
    foreach ($cmd in $cat.Group) {
      Write-Host "    " -NoNewline
      Write-Host $cmd.Name.PadRight(22) -NoNewline -ForegroundColor Yellow
      Write-Host $cmd.Description -ForegroundColor White
            
      if ($cmd.Options) {
        Write-Host " ".PadRight(26) -NoNewline
        Write-Host $cmd.Options -ForegroundColor Gray
      }
    }
    Write-Host ""
  }
    
  Write-Host "Examples:" -ForegroundColor Green
  Write-Host "  .\RawrXD.ps1 -CliMode -Command test-ollama" -ForegroundColor Gray
  Write-Host "  .\RawrXD.ps1 -CliMode -Command chat -Model llama2" -ForegroundColor Gray
  Write-Host "  .\RawrXD.ps1 -CliMode -Command analyze-file -FilePath script.ps1" -ForegroundColor Gray
  Write-Host "  .\RawrXD.ps1 -CliMode -Command vscode-popular" -ForegroundColor Cyan
  Write-Host "  .\RawrXD.ps1 -CliMode -Command vscode-search -Prompt 'copilot'" -ForegroundColor Cyan
  Write-Host "  .\RawrXD.ps1 -CliMode -Command diagnose" -ForegroundColor Gray
  Write-Host ""
  Write-Host "🌐 VSCode Marketplace commands connect to the official Microsoft API" -ForegroundColor Cyan
  Write-Host ""
    
  return $true
}

# Alias for Show-CliHelp (used in main script)
function Show-CliHelp {
  return Invoke-CliHelp
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
  Export-ModuleMember -Function Invoke-CliHelp, Show-CliHelp
}
# ============================================
# CLI HANDLER: help
# ============================================
# Category: general
# Command: help
# Purpose: Show CLI help and available commands
# ============================================

function Invoke-CliHelp {
  <#
    .SYNOPSIS
        Display help for CLI commands
    .DESCRIPTION
        Shows all available CLI commands with descriptions and usage examples.
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command help
    .OUTPUTS
        [bool] $true always
    #>
    
  Write-Host "`n=== RawrXD CLI Help ===" -ForegroundColor Cyan
  Write-Host "`nUsage: .\RawrXD.ps1 -CliMode -Command <command> [options]`n" -ForegroundColor Yellow
    
  Write-Host "Available Commands:" -ForegroundColor Green
  Write-Host ""
    
  $commands = @(
    # Ollama Commands
    @{Name = "test-ollama"; Description = "Test Ollama connection and list available models"; Category = "Ollama" }
    @{Name = "list-models"; Description = "List all available Ollama models with details"; Category = "Ollama" }
    @{Name = "chat"; Description = "Start an interactive chat session"; Options = "-Model <model_name>"; Category = "Ollama" }
    @{Name = "analyze-file"; Description = "Analyze a file with AI"; Options = "-FilePath <path> -Model <model_name>"; Category = "Ollama" }
        
    # Git Commands
    @{Name = "git-status"; Description = "Show formatted git status"; Category = "Git" }
        
    # Agent Commands
    @{Name = "create-agent"; Description = "Create a new agent task"; Options = "-AgentName <name> -Prompt <task>"; Category = "Agents" }
    @{Name = "list-agents"; Description = "List all agent tasks"; Category = "Agents" }
        
    # Marketplace Commands
    @{Name = "marketplace-sync"; Description = "Sync marketplace catalog and browse extensions"; Category = "Marketplace" }
    @{Name = "marketplace-search"; Description = "Search for extensions in marketplace"; Options = "-Prompt <search_term>"; Category = "Marketplace" }
    @{Name = "marketplace-install"; Description = "Install an extension from marketplace"; Options = "-Prompt <extension_id>"; Category = "Marketplace" }
    @{Name = "list-extensions"; Description = "List installed extensions"; Category = "Marketplace" }
        
    # VSCode Marketplace Commands
    @{Name = "vscode-popular"; Description = "🌐 Get top 15 popular VSCode extensions (Live API)"; Category = "VSCode" }
    @{Name = "vscode-search"; Description = "🌐 Search VSCode Marketplace (Live API)"; Options = "-Prompt <search_term>"; Category = "VSCode" }
    @{Name = "vscode-install"; Description = "🌐 Install VSCode extension (Live API)"; Options = "-Prompt <extension_id>"; Category = "VSCode" }
    @{Name = "vscode-categories"; Description = "🌐 Browse VSCode extension categories"; Category = "VSCode" }
        
    # Testing Commands
    @{Name = "diagnose"; Description = "Run comprehensive diagnostic checks"; Category = "Testing" }
    @{Name = "test-editor-settings"; Description = "Test editor settings (colors, fonts, syntax)"; Category = "Testing" }
    @{Name = "test-file-operations"; Description = "Test file operations (open, save, read)"; Category = "Testing" }
    @{Name = "test-settings-persistence"; Description = "Test settings save/load persistence"; Category = "Testing" }
    @{Name = "test-all-features"; Description = "Run comprehensive test suite"; Category = "Testing" }
        
    # Settings Commands
    @{Name = "get-settings"; Description = "Get current settings"; Options = "-SettingName <name> (optional)"; Category = "Settings" }
    @{Name = "set-setting"; Description = "Set a specific setting"; Options = "-SettingName <name> -SettingValue <value>"; Category = "Settings" }
        
    # General Commands
    @{Name = "help"; Description = "Show this help message"; Category = "General" }
  )
    
  # Group by category
  $categories = $commands | Group-Object -Property Category
    
  foreach ($cat in $categories) {
    Write-Host "  $($cat.Name.ToUpper())" -ForegroundColor Cyan
        
    foreach ($cmd in $cat.Group) {
      Write-Host "    " -NoNewline
      Write-Host $cmd.Name.PadRight(22) -NoNewline -ForegroundColor Yellow
      Write-Host $cmd.Description -ForegroundColor White
            
      if ($cmd.Options) {
        Write-Host " ".PadRight(26) -NoNewline
        Write-Host $cmd.Options -ForegroundColor Gray
      }
    }
    Write-Host ""
  }
    
  Write-Host "Examples:" -ForegroundColor Green
  Write-Host "  .\RawrXD.ps1 -CliMode -Command test-ollama" -ForegroundColor Gray
  Write-Host "  .\RawrXD.ps1 -CliMode -Command chat -Model llama2" -ForegroundColor Gray
  Write-Host "  .\RawrXD.ps1 -CliMode -Command analyze-file -FilePath script.ps1" -ForegroundColor Gray
  Write-Host "  .\RawrXD.ps1 -CliMode -Command vscode-popular" -ForegroundColor Cyan
  Write-Host "  .\RawrXD.ps1 -CliMode -Command vscode-search -Prompt 'copilot'" -ForegroundColor Cyan
  Write-Host "  .\RawrXD.ps1 -CliMode -Command diagnose" -ForegroundColor Gray
  Write-Host ""
  Write-Host "🌐 VSCode Marketplace commands connect to the official Microsoft API" -ForegroundColor Cyan
  Write-Host ""
    
  return $true
}

# Alias for Show-CliHelp (used in main script)
function Show-CliHelp {
  return Invoke-CliHelp
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
  Export-ModuleMember -Function Invoke-CliHelp, Show-CliHelp
}
