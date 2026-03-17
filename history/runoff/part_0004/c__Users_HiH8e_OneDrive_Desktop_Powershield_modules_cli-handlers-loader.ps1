# ============================================
# CLI HANDLER LOADER FOR RAWRXD
# ============================================
# File: cli-handlers-loader.ps1
# Purpose: Dynamic discovery and loading of CLI command handlers
# Features:
#   - Auto-discover handler modules
#   - Lazy loading for fast startup
#   - Handler registry with metadata
#   - Centralized command routing
# ============================================

# Global handler registry
$script:CliHandlerRegistry = @{
    Handlers     = @{}  # CommandName → HandlerInfo
    LoadedModules = @() # List of loaded module paths
    IsInitialized = $false
}

function Initialize-CliHandlers {
    <#
    .SYNOPSIS
        Initialize and discover all CLI handler modules
    
    .DESCRIPTION
        Scans the cli-handlers directory for handler modules and registers
        them for lazy loading. Does not load handlers until they are invoked.
    
    .PARAMETER HandlerPath
        Path to the cli-handlers directory (default: script root/modules/cli-handlers)
    
    .EXAMPLE
        Initialize-CliHandlers
        $handlers = Initialize-CliHandlers -HandlerPath "C:\RawrXD\modules\cli-handlers"
    
    .OUTPUTS
        [hashtable] Handler registry with command mappings
    #>
    param(
        [string]$HandlerPath = $null
    )
    
    try {
        # Determine handler path
        if (-not $HandlerPath) {
            $HandlerPath = Join-Path $PSScriptRoot "cli-handlers"
        }
        
        if (-not (Test-Path $HandlerPath)) {
            Write-Warning "CLI handler path does not exist: $HandlerPath"
            return $script:CliHandlerRegistry
        }
        
        # Discover handler modules
        $handlerFiles = Get-ChildItem -Path $HandlerPath -Filter "*.ps1" -Recurse -File
        
        foreach ($file in $handlerFiles) {
            # Skip loader file
            if ($file.Name -eq "cli-handlers-loader.ps1") { continue }
            
            # Extract handler metadata from filename
            # Convention: command-name.ps1 → command-name command
            $commandName = $file.BaseName
            
            # Parse handler file for function name and metadata
            $content = Get-Content -Path $file.FullName -Raw
            
            # Find exported function
            $functionMatch = [regex]::Match($content, 'function\s+(Invoke-Cli\w+)')
            $functionName = if ($functionMatch.Success) { $functionMatch.Groups[1].Value } else { $null }
            
            # Extract description from comment block
            $descriptionMatch = [regex]::Match($content, '\.SYNOPSIS\s*\r?\n\s*(.+?)(?=\r?\n\s*\.)')
            $description = if ($descriptionMatch.Success) { $descriptionMatch.Groups[1].Value.Trim() } else { "CLI command: $commandName" }
            
            # Determine category from folder structure
            $relativePath = $file.FullName.Replace($HandlerPath, "").TrimStart("\", "/")
            $category = if ($relativePath -match "^([^\\\/]+)[\\/]") { $matches[1] } else { "general" }
            
            # Register handler
            $script:CliHandlerRegistry.Handlers[$commandName] = @{
                CommandName    = $commandName
                FunctionName   = $functionName
                FilePath       = $file.FullName
                Category       = $category
                Description    = $description
                IsLoaded       = $false
                LoadedFunction = $null
            }
        }
        
        $script:CliHandlerRegistry.IsInitialized = $true
        Write-Verbose "Initialized $($script:CliHandlerRegistry.Handlers.Count) CLI handlers" -Verbose
        
        return $script:CliHandlerRegistry
    }
    catch {
        Write-Error "Failed to initialize CLI handlers: $_"
        return $script:CliHandlerRegistry
    }
}

function Get-CliHandler {
    <#
    .SYNOPSIS
        Get handler info for a specific command
    
    .PARAMETER CommandName
        The command name to look up
    
    .OUTPUTS
        [hashtable] Handler info or $null if not found
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandName
    )
    
    if (-not $script:CliHandlerRegistry.IsInitialized) {
        Initialize-CliHandlers | Out-Null
    }
    
    return $script:CliHandlerRegistry.Handlers[$CommandName]
}

function Import-CliHandler {
    <#
    .SYNOPSIS
        Load a CLI handler module into memory
    
    .DESCRIPTION
        Lazy-loads a handler module when first invoked.
        Subsequent calls return cached function reference.
    
    .PARAMETER CommandName
        The command name to load handler for
    
    .OUTPUTS
        [scriptblock] Handler function or $null
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandName
    )
    
    try {
        $handler = Get-CliHandler -CommandName $CommandName
        
        if (-not $handler) {
            Write-Warning "Handler not found for command: $CommandName"
            return $null
        }
        
        if ($handler.IsLoaded -and $handler.LoadedFunction) {
            return $handler.LoadedFunction
        }
        
        # Load the handler module
        if (Test-Path $handler.FilePath) {
            . $handler.FilePath
            
            # Get the function reference
            $function = Get-Command -Name $handler.FunctionName -ErrorAction SilentlyContinue
            
            if ($function) {
                $handler.IsLoaded = $true
                $handler.LoadedFunction = $function
                $script:CliHandlerRegistry.LoadedModules += $handler.FilePath
                
                Write-Verbose "Loaded CLI handler: $($handler.FunctionName) from $($handler.FilePath)" -Verbose
                return $function
            } else {
                Write-Warning "Function $($handler.FunctionName) not found in $($handler.FilePath)"
            }
        }
        
        return $null
    }
    catch {
        Write-Error "Failed to import CLI handler for '$CommandName': $_"
        return $null
    }
}

function Invoke-CliCommand {
    <#
    .SYNOPSIS
        Execute a CLI command by name
    
    .DESCRIPTION
        Routes CLI commands to their handlers. Supports both modular handlers
        and legacy inline handlers for backward compatibility.
    
    .PARAMETER CommandName
        The command to execute (e.g., "test-ollama", "chat", "diagnose")
    
    .PARAMETER Parameters
        Hashtable of parameters to pass to the handler
    
    .EXAMPLE
        Invoke-CliCommand -CommandName "test-ollama"
        Invoke-CliCommand -CommandName "chat" -Parameters @{Model = "llama2"}
    
    .OUTPUTS
        [bool] $true if command succeeded, $false otherwise
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$CommandName,
        
        [hashtable]$Parameters = @{}
    )
    
    try {
        # Try modular handler first
        $handler = Import-CliHandler -CommandName $CommandName
        
        if ($handler) {
            # Invoke modular handler
            $result = & $handler @Parameters
            return $result
        }
        
        # Fall back to legacy inline handler lookup
        $legacyFunctionName = "Invoke-Cli" + (($CommandName -split '-' | ForEach-Object { 
            $_.Substring(0, 1).ToUpper() + $_.Substring(1).ToLower() 
        }) -join '')
        
        $legacyHandler = Get-Command -Name $legacyFunctionName -ErrorAction SilentlyContinue
        
        if ($legacyHandler) {
            Write-Verbose "Using legacy handler: $legacyFunctionName" -Verbose
            $result = & $legacyHandler @Parameters
            return $result
        }
        
        Write-Error "No handler found for command: $CommandName"
        return $false
    }
    catch {
        Write-Error "Failed to execute CLI command '$CommandName': $_"
        return $false
    }
}

function Get-CliCommands {
    <#
    .SYNOPSIS
        List all available CLI commands
    
    .DESCRIPTION
        Returns a list of all registered CLI commands with their descriptions.
        Useful for generating help text or command completion.
    
    .PARAMETER Category
        Optional filter by category (e.g., "ollama", "git", "testing")
    
    .EXAMPLE
        Get-CliCommands
        Get-CliCommands -Category "ollama"
    
    .OUTPUTS
        [array] List of command info objects
    #>
    param(
        [string]$Category = $null
    )
    
    if (-not $script:CliHandlerRegistry.IsInitialized) {
        Initialize-CliHandlers | Out-Null
    }
    
    $commands = $script:CliHandlerRegistry.Handlers.Values
    
    if ($Category) {
        $commands = $commands | Where-Object { $_.Category -eq $Category }
    }
    
    return $commands | Sort-Object Category, CommandName | ForEach-Object {
        [PSCustomObject]@{
            Command     = $_.CommandName
            Category    = $_.Category
            Description = $_.Description
            Loaded      = $_.IsLoaded
        }
    }
}

function Show-CliHandlerHelp {
    <#
    .SYNOPSIS
        Display help for CLI handler system
    
    .DESCRIPTION
        Shows information about the modular CLI handler system,
        including how to add new handlers and available commands.
    #>
    
    Write-Host "`n=== RawrXD CLI Handler System ===" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "The CLI handler system provides modular command handling for RawrXD." -ForegroundColor White
    Write-Host ""
    
    Write-Host "Available Commands by Category:" -ForegroundColor Yellow
    Write-Host ""
    
    $commands = Get-CliCommands
    $categories = $commands | Group-Object -Property Category
    
    foreach ($cat in $categories) {
        Write-Host "  $($cat.Name.ToUpper())" -ForegroundColor Cyan
        foreach ($cmd in $cat.Group) {
            $status = if ($cmd.Loaded) { "✓" } else { " " }
            Write-Host "    $status $($cmd.Command.PadRight(20)) $($cmd.Description)" -ForegroundColor White
        }
        Write-Host ""
    }
    
    Write-Host "Handler Locations:" -ForegroundColor Yellow
    Write-Host "  modules/cli-handlers/<category>/<command-name>.ps1" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "To add a new handler:" -ForegroundColor Yellow
    Write-Host "  1. Create modules/cli-handlers/<category>/<command>.ps1" -ForegroundColor Gray
    Write-Host "  2. Define function: Invoke-Cli<CommandName>" -ForegroundColor Gray
    Write-Host "  3. Add ValidateSet entry in param block (optional)" -ForegroundColor Gray
    Write-Host ""
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-CliHandlers',
    'Get-CliHandler',
    'Import-CliHandler',
    'Invoke-CliCommand',
    'Get-CliCommands',
    'Show-CliHandlerHelp'
)
