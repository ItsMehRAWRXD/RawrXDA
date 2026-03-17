#Requires -Version 5.1
<#
.SYNOPSIS
    Launch RawrXD IDE in Agentic Mode
    
.DESCRIPTION
    Starts the RawrXD PowerShell IDE with full agentic capabilities enabled.
    Automatically initializes the agentic module, connects to Ollama, and
    launches the IDE in one seamless operation.
    
.PARAMETER Model
    The Ollama model to use for agentic operations.
    Default: 'bigdaddyg-fast:latest'
    
.PARAMETER Temperature
    Model temperature for creativity vs consistency (0-1).
    Default: 0.7
    
.PARAMETER AutoInit
    Automatically initialize agentic mode without prompts.
    Default: $true
    
.PARAMETER Verbose
    Enable verbose output for debugging.
    
.EXAMPLE
    .\Launch-RawrXD-Agentic.ps1
    # Launches with default BigDaddyG model
    
.EXAMPLE
    .\Launch-RawrXD-Agentic.ps1 -Model "cheetah-stealth-agentic:latest"
    # Launches with Cheetah model
    
.EXAMPLE
    .\Launch-RawrXD-Agentic.ps1 -Temperature 0.9
    # Higher creativity setting
#>

param(
    [string]$Model = 'bigdaddyg-fast:latest',
    [decimal]$Temperature = 0.7,
    [switch]$AutoInit = $true,
    [switch]$Terminal,
    [switch]$Verbose
)

$ErrorActionPreference = 'Continue'
$VerbosePreference = if ($Verbose) { 'Continue' } else { 'SilentlyContinue' }

# Script metadata
$script:LauncherVersion = '1.0'
$script:RawrXDPath = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$script:ModulePath = Join-Path $script:RawrXDPath 'RawrXD-Agentic-Module.psm1'
$script:MainIDEPath = Join-Path $script:RawrXDPath 'RawrXD.ps1'

function Write-Banner {
    Write-Host "`n" -ForegroundColor White
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                                                                ║" -ForegroundColor Magenta
    Write-Host "║             🚀 RawrXD AGENTIC IDE LAUNCHER 🚀                ║" -ForegroundColor Magenta
    Write-Host "║                                                                ║" -ForegroundColor Magenta
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host "`n"
}

function Write-Status {
    param([string]$Message, [string]$Status = 'INFO')
    
    $color = switch ($Status) {
        'SUCCESS' { 'Green' }
        'ERROR' { 'Red' }
        'WARNING' { 'Yellow' }
        'INFO' { 'Cyan' }
        default { 'White' }
    }
    
    $symbol = switch ($Status) {
        'SUCCESS' { '✅' }
        'ERROR' { '❌' }
        'WARNING' { '⚠️' }
        'INFO' { 'ℹ️' }
        default { '➜' }
    }
    
    Write-Host "$symbol $Message" -ForegroundColor $color
}

function Test-Prerequisites {
    Write-Host "`n" -ForegroundColor White
    Write-Host "🔍 CHECKING PREREQUISITES" -ForegroundColor Cyan
    Write-Host "─" * 60 -ForegroundColor Gray
    
    $allGood = $true
    
    # Check Ollama connection
    Write-Verbose "Testing Ollama connection..."
    try {
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5 -ErrorAction SilentlyContinue
        if ($response.models) {
            Write-Status "Ollama connection: OK ($(($response.models | Measure-Object).Count) models available)" -Status 'SUCCESS'
            
            # Check if specified model exists
            $modelExists = $response.models | Where-Object { $_.name -eq $Model }
            if ($modelExists) {
                Write-Status "Model '$Model': FOUND" -Status 'SUCCESS'
            } else {
                Write-Status "Model '$Model': NOT FOUND (available: $(($response.models.name) -join ', '))" -Status 'WARNING'
                $allGood = $false
            }
        }
    } catch {
        Write-Status "Ollama connection: FAILED (is Ollama running? Try: ollama serve)" -Status 'ERROR'
        $allGood = $false
    }
    
    # Check module exists
    Write-Verbose "Testing module file..."
    if (Test-Path $script:ModulePath) {
        Write-Status "Agentic module: FOUND" -Status 'SUCCESS'
    } else {
        Write-Status "Agentic module: NOT FOUND ($($script:ModulePath))" -Status 'ERROR'
        $allGood = $false
    }
    
    # Check IDE exists
    Write-Verbose "Testing IDE file..."
    if (Test-Path $script:MainIDEPath) {
        Write-Status "RawrXD IDE: FOUND" -Status 'SUCCESS'
    } else {
        Write-Status "RawrXD IDE: NOT FOUND ($($script:MainIDEPath))" -Status 'ERROR'
        $allGood = $false
    }
    
    # Check PowerShell version
    Write-Verbose "Checking PowerShell version..."
    if ($PSVersionTable.PSVersion.Major -ge 5) {
        Write-Status "PowerShell version: $($PSVersionTable.PSVersion) (OK)" -Status 'SUCCESS'
    } else {
        Write-Status "PowerShell version: $($PSVersionTable.PSVersion) (requires 5.1+)" -Status 'ERROR'
        $allGood = $false
    }
    
    Write-Host "`n"
    return $allGood
}

function Initialize-RequiredAssemblies {
    Write-Verbose "Pre-loading required assemblies..."
    try {
        # Load System.Windows.Forms for GUI
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction SilentlyContinue
        Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue
        Write-Verbose "Assemblies loaded successfully"
        return $true
    } catch {
        Write-Verbose "Warning: Could not pre-load assemblies: $($_.Exception.Message)"
        return $false
    }
}

function Initialize-AgenticMode {
    Write-Host "`n" -ForegroundColor White
    Write-Host "⚙️  INITIALIZING AGENTIC MODE" -ForegroundColor Cyan
    Write-Host "─" * 60 -ForegroundColor Gray
    
    # Pre-load assemblies
    Initialize-RequiredAssemblies
    
    try {
        Write-Verbose "Importing agentic module..."
        Import-Module $script:ModulePath -Force -ErrorAction Stop
        Write-Status "Module imported successfully" -Status 'SUCCESS'
        
        Write-Verbose "Enabling agentic capabilities..."
        Enable-RawrXDAgentic -Model $Model -Temperature $Temperature -ErrorAction Stop
        Write-Status "Agentic mode enabled with '$Model' model (Temperature: $Temperature)" -Status 'SUCCESS'
        
        # Get status
        $status = Get-RawrXDAgenticStatus
        Write-Status "Agentic context initialized and ready" -Status 'SUCCESS'
        
        return $true
    } catch {
        Write-Status "Failed to initialize agentic mode: $($_.Exception.Message)" -Status 'ERROR'
        return $false
    }
}

function Start-RawrXDIDE {
    Write-Host "`n" -ForegroundColor White
    Write-Host "🎯 LAUNCHING RAWRXD IDE" -ForegroundColor Cyan
    Write-Host "─" * 60 -ForegroundColor Gray
    
    try {
        Write-Verbose "Starting RawrXD.ps1..."
        Write-Status "Loading IDE interface..." -Status 'INFO'
        Write-Host "`n"
        
        # Attempt to launch with error handling
        & $script:MainIDEPath
        
    } catch {
        $errorMsg = $_.Exception.Message
        
        # Check if it's an assembly loading error
        if ($errorMsg -like "*System.Windows.Forms*" -or $errorMsg -like "*assembly*" -or $errorMsg -like "*null*") {
            Write-Status "IDE GUI initialization error. Staying in terminal mode..." -Status 'WARNING'
            
            Write-Host "`nℹ️  You have full agentic access in PowerShell terminal!" -ForegroundColor Cyan
            Write-Host "Use the functions directly to start coding:" -ForegroundColor Gray
            Write-Host "`n  `$code = Invoke-RawrXDAgenticCodeGen -Prompt 'your request'" -ForegroundColor Yellow
            Write-Host "  Invoke-RawrXDAgenticAnalysis -Type 'debug' -Code `$code" -ForegroundColor Yellow
            Write-Host "`n"
            return $false
        } else {
            Write-Status "Error launching IDE: $errorMsg" -Status 'ERROR'
        }
        return $false
    }
}



function Show-QuickReference {
    Write-Host "`n" -ForegroundColor White
    Write-Host "📚 QUICK REFERENCE" -ForegroundColor Cyan
    Write-Host "─" * 60 -ForegroundColor Gray
    
    Write-Host "`nAGENTIC FUNCTIONS:" -ForegroundColor Yellow
    Write-Host "  Invoke-RawrXDAgenticCodeGen" -ForegroundColor Cyan
    Write-Host "    → Generate autonomous code with full context" -ForegroundColor Gray
    
    Write-Host "`n  Invoke-RawrXDAgenticCompletion" -ForegroundColor Cyan
    Write-Host "    → Get intelligent code suggestions" -ForegroundColor Gray
    
    Write-Host "`n  Invoke-RawrXDAgenticAnalysis" -ForegroundColor Cyan
    Write-Host "    → Analyze code (improve/debug/refactor/test/document)" -ForegroundColor Gray
    
    Write-Host "`n  Invoke-RawrXDAgenticRefactor" -ForegroundColor Cyan
    Write-Host "    → Autonomous code refactoring" -ForegroundColor Gray
    
    Write-Host "`n  Get-RawrXDAgenticStatus" -ForegroundColor Cyan
    Write-Host "    → View agentic capabilities and status" -ForegroundColor Gray
    
    Write-Host "`nEXAMPLES:" -ForegroundColor Yellow
    Write-Host "  Generate function:" -ForegroundColor Gray
    Write-Host "    `$code = Invoke-RawrXDAgenticCodeGen -Prompt 'Create async function'" -ForegroundColor Cyan
    
    Write-Host "`n  Analyze code:" -ForegroundColor Gray
    Write-Host "    Invoke-RawrXDAgenticAnalysis -Type 'debug' -Code `$code" -ForegroundColor Cyan
    
    Write-Host "`n  Smart completion:" -ForegroundColor Gray
    Write-Host "    Invoke-RawrXDAgenticCompletion -Partial 'function Get-' -Context 'cmdlet'" -ForegroundColor Cyan
    
    Write-Host "`n  Refactor:" -ForegroundColor Gray
    Write-Host "    Invoke-RawrXDAgenticRefactor -Code `$oldCode -Objective 'modernize'" -ForegroundColor Cyan
    
    Write-Host "`n"
}

function Show-ConfigInfo {
    Write-Host "`n" -ForegroundColor White
    Write-Host "⚙️  CONFIGURATION" -ForegroundColor Cyan
    Write-Host "─" * 60 -ForegroundColor Gray
    
    Write-Host "`n  Model:" -ForegroundColor Yellow
    Write-Host "    $Model" -ForegroundColor Cyan
    
    Write-Host "`n  Temperature:" -ForegroundColor Yellow
    Write-Host "    $Temperature (0=consistent, 1=creative)" -ForegroundColor Cyan
    
    Write-Host "`n  Agentic Module:" -ForegroundColor Yellow
    Write-Host "    $script:ModulePath" -ForegroundColor Cyan
    
    Write-Host "`n  IDE Path:" -ForegroundColor Yellow
    Write-Host "    $script:MainIDEPath" -ForegroundColor Cyan
    
    Write-Host "`n  Ollama Endpoint:" -ForegroundColor Yellow
    Write-Host "    http://localhost:11434" -ForegroundColor Cyan
    
    Write-Host "`n"
}

# Main execution
function Main {
    Write-Banner
    
    # Show startup info
    Write-Host "Version: $script:LauncherVersion" -ForegroundColor Gray
    Write-Host "Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
    
    # Check prerequisites
    if (-not (Test-Prerequisites)) {
        Write-Status "Prerequisites check failed. Please resolve issues above." -Status 'ERROR'
        Write-Host "`nFor help: https://github.com/ItsMehRAWRXD/RawrXD" -ForegroundColor Gray
        return
    }
    
    # Initialize agentic mode
    if (-not (Initialize-AgenticMode)) {
        Write-Status "Could not initialize agentic mode. Starting IDE in standard mode." -Status 'WARNING'
    }
    
    # Show configuration
    Show-ConfigInfo
    
    # Show quick reference
    Show-QuickReference
    
    # Launch IDE or stay in terminal
    if ($Terminal) {
        Write-Status "Terminal mode enabled. Functions available in this session." -Status 'INFO'
        Write-Host "`n💡 Tip: Try: `$code = Invoke-RawrXDAgenticCodeGen -Prompt 'Create a hello world function'" -ForegroundColor Yellow
        Write-Host "`n"
    } else {
        Write-Status "Ready to launch! Starting RawrXD IDE..." -Status 'INFO'
        Write-Host "`n"
        Start-RawrXDIDE
    }
}

# Execute
Main
