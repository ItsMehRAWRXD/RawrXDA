<#
.SYNOPSIS
    RawrXD - Modular AI-Powered Text Editor with Ollama Integration (Rebuilt Version)

.DESCRIPTION
    A comprehensive, modular text editor featuring:
    - File Explorer with syntax highlighting
    - AI Chat integration via Ollama
    - Embedded web browser
    - Integrated terminal
    - Git version control
    - Agent task automation
    
    This is a complete rebuild of the original RawrXD with a modular, maintainable architecture.

.PARAMETER CliMode
    Run in command-line interface mode without GUI

.PARAMETER Command
    Command to execute in CLI mode

.PARAMETER FilePath
    File path for file-related commands

.PARAMETER Model
    Ollama model to use (default: llama2)

.EXAMPLE
    .\RawrXD-New.ps1
    Launch the GUI editor

.EXAMPLE
    .\RawrXD-New.ps1 -CliMode -Command test-ollama
    Test Ollama connection from command line
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [switch]$CliMode,
    
    [Parameter(Mandatory = $false)]
    [ValidateSet(
        'test-ollama', 'list-models', 'chat', 'analyze-file', 
        'git-status', 'help'
    )]
    [string]$Command,
    
    [Parameter(Mandatory = $false)]
    [string]$FilePath,
    
    [Parameter(Mandatory = $false)]
    [string]$Model = "llama2",
    
    [Parameter(Mandatory = $false)]
    [string]$Prompt
)

# Set script location for module loading
$script:ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:ModulesPath = Join-Path $script:ScriptRoot "RawrXD-Modules"

# Global application state
$global:RawrXD = @{
    Version = "2.0.0"
    Build = "Modular-Rebuild"
    StartTime = Get-Date
    CurrentFile = $null
    Settings = @{}
    IsInitialized = $false
    Form = $null
    Components = @{}
}

# Import required assemblies
try {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName Microsoft.VisualBasic
    $global:RawrXD.WindowsFormsAvailable = $true
    Write-Host "✅ Windows Forms assemblies loaded successfully" -ForegroundColor Green
}
catch {
    $global:RawrXD.WindowsFormsAvailable = $false
    Write-Warning "⚠️  Windows Forms not available: $($_.Exception.Message)"
}

# Load core modules
function Import-RawrXDModule {
    param([string]$ModuleName)
    
    $modulePath = Join-Path $script:ModulesPath "$ModuleName.psm1"
    if (Test-Path $modulePath) {
        try {
            Import-Module $modulePath -Force -Scope Global
            Write-Host "✅ Loaded module: $ModuleName" -ForegroundColor Green
            return $true
        }
        catch {
            Write-Warning "⚠️  Failed to load module $ModuleName`: $($_.Exception.Message)"
            return $false
        }
    }
    else {
        Write-Warning "⚠️  Module not found: $modulePath"
        return $false
    }
}

# Load all required modules
$requiredModules = @(
    'RawrXD-Core',
    'RawrXD-UI-Main',
    'RawrXD-UI-FileExplorer', 
    'RawrXD-UI-TextEditor',
    'RawrXD-UI-ChatPanel',
    'RawrXD-UI-Browser',
    'RawrXD-CLI',
    'RawrXD-Ollama',
    'RawrXD-Git'
)

$loadedModules = 0
foreach ($module in $requiredModules) {
    if (Import-RawrXDModule -ModuleName $module) {
        $loadedModules++
    }
}

Write-Host "📊 Loaded $loadedModules of $($requiredModules.Count) modules" -ForegroundColor Cyan

# Initialize application
function Initialize-RawrXD {
    try {
        # Initialize core
        if (Get-Command Initialize-RawrXDCore -ErrorAction SilentlyContinue) {
            Initialize-RawrXDCore
        }
        
        # Load settings
        if (Get-Command Import-RawrXDSettings -ErrorAction SilentlyContinue) {
            $global:RawrXD.Settings = Import-RawrXDSettings
        }
        
        $global:RawrXD.IsInitialized = $true
        Write-Host "✅ RawrXD initialized successfully" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Error "❌ Failed to initialize RawrXD: $($_.Exception.Message)"
        return $false
    }
}

# Main execution logic
function Start-RawrXD {
    Write-Host "🚀 Starting RawrXD v$($global:RawrXD.Version)..." -ForegroundColor Cyan
    
    if (-not (Initialize-RawrXD)) {
        Write-Error "❌ Failed to initialize application"
        exit 1
    }
    
    if ($CliMode) {
        if (Get-Command Start-RawrXDCLI -ErrorAction SilentlyContinue) {
            Start-RawrXDCLI -Command $Command -FilePath $FilePath -Model $Model -Prompt $Prompt
        }
        else {
            Write-Error "❌ CLI module not available"
            exit 1
        }
    }
    else {
        if ($global:RawrXD.WindowsFormsAvailable -and (Get-Command Start-RawrXDGUI -ErrorAction SilentlyContinue)) {
            Start-RawrXDGUI
        }
        else {
            Write-Warning "⚠️  GUI not available, falling back to CLI mode"
            if (Get-Command Start-RawrXDCLI -ErrorAction SilentlyContinue) {
                Start-RawrXDCLI -Command "help"
            }
            else {
                Write-Error "❌ No interface modules available"
                exit 1
            }
        }
    }
}

# Exception handling and cleanup
try {
    Start-RawrXD
}
catch {
    Write-Error "❌ Critical error: $($_.Exception.Message)"
    Write-Error "Stack trace: $($_.ScriptStackTrace)"
    exit 1
}
finally {
    # Cleanup
    if ($global:RawrXD.Form) {
        try {
            $global:RawrXD.Form.Dispose()
        }
        catch { }
    }
    Write-Host "👋 RawrXD session ended" -ForegroundColor Yellow
}