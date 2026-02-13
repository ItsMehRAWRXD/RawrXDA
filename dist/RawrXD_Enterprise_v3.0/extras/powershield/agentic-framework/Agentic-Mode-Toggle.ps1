<#
.SYNOPSIS
    Agentic Mode Toggle - Enable/Disable Autonomous Reasoning for Local Models
.DESCRIPTION
    Simple toggle to switch models between standard mode and agentic (autonomous) mode
    Works with your local Ollama models running in VS Code or terminal
#>

param(
    [ValidateSet("on", "off", "status")]
    [string]$Action = "status"
)

$OllamaEndpoint = "http://localhost:11434"
$ConfigFile = "$env:APPDATA\Ollama\agentic-config.json"
$ConfigDir = Split-Path -Path $ConfigFile -Parent

# Ensure config directory exists
if (-not (Test-Path $ConfigDir)) {
    New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
}

# Default config
$defaultConfig = @{
    AgenticMode = $false
    ActiveModel = "cheetah-stealth-agentic:latest"
    StandardModel = "bigdaddyg-fast:latest"
    AgenticModel = "cheetah-stealth-agentic:latest"
    Temperature = 0.7
    AgenticTemperature = 0.9
    Timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
}

function Load-Config {
    if (Test-Path $ConfigFile) {
        return Get-Content $ConfigFile | ConvertFrom-Json
    }
    return $defaultConfig
}

function Save-Config {
    param([hashtable]$Config)
    $Config | ConvertTo-Json | Set-Content -Path $ConfigFile -Force
}

function Test-OllamaConnection {
    try {
        $response = Invoke-RestMethod -Uri "$OllamaEndpoint/api/tags" -TimeoutSec 3 -ErrorAction Stop
        return @{
            Status = $true
            Models = $response.models.name
        }
    }
    catch {
        return @{
            Status = $false
            Error = $_
        }
    }
}

function Enable-AgenticMode {
    Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║  🚀 ENABLING AGENTIC MODE                          ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Green
    
    # Test Ollama connection
    $connection = Test-OllamaConnection
    if (-not $connection.Status) {
        Write-Host "❌ Ollama is not running!" -ForegroundColor Red
        Write-Host "   Error: $($connection.Error)" -ForegroundColor Yellow
        return $false
    }
    
    Write-Host "`n✅ Ollama is running" -ForegroundColor Green
    Write-Host "   Available models: $($connection.Models.Count)" -ForegroundColor Cyan
    
    # Load current config
    $config = Load-Config
    
    # Check if agentic model is available
    if ($connection.Models -notcontains $config.AgenticModel) {
        Write-Host "`n⚠️  Agentic model '$($config.AgenticModel)' not found!" -ForegroundColor Yellow
        Write-Host "   Available models:" -ForegroundColor Cyan
        $connection.Models | ForEach-Object { Write-Host "      - $_" -ForegroundColor Gray }
        
        # Try to find an agentic model
        $agenticModel = $connection.Models | Where-Object { $_ -match "agentic|stealth|unleashed" } | Select-Object -First 1
        if ($agenticModel) {
            Write-Host "`n   Using: $agenticModel" -ForegroundColor Green
            $config.AgenticModel = $agenticModel
        } else {
            Write-Host "`n❌ No agentic models found!" -ForegroundColor Red
            return $false
        }
    }
    
    # Enable agentic mode
    $config.AgenticMode = $true
    $config.Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Save-Config $config
    
    Write-Host "`n" -NoNewline
    Write-Host "✅ AGENTIC MODE ENABLED" -ForegroundColor Green -BackgroundColor Black
    Write-Host "`n   Model: $($config.AgenticModel)" -ForegroundColor Cyan
    Write-Host "   Temperature: $($config.AgenticTemperature)" -ForegroundColor Cyan
    Write-Host "   Mode: AUTONOMOUS REASONING & PLANNING" -ForegroundColor Yellow
    Write-Host "`n   Features Activated:" -ForegroundColor Green
    Write-Host "      ✓ Multi-step planning" -ForegroundColor Green
    Write-Host "      ✓ Autonomous decision making" -ForegroundColor Green
    Write-Host "      ✓ Code generation without approval" -ForegroundColor Green
    Write-Host "      ✓ Complex reasoning chains" -ForegroundColor Green
    Write-Host "      ✓ Adaptive problem solving" -ForegroundColor Green
    
    return $true
}

function Disable-AgenticMode {
    Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Yellow
    Write-Host "║  🔒 DISABLING AGENTIC MODE                         ║" -ForegroundColor Yellow
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Yellow
    
    $config = Load-Config
    $config.AgenticMode = $false
    $config.Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Save-Config $config
    
    Write-Host "`n" -NoNewline
    Write-Host "✅ AGENTIC MODE DISABLED" -ForegroundColor Yellow -BackgroundColor Black
    Write-Host "`n   Model: $($config.StandardModel)" -ForegroundColor Cyan
    Write-Host "   Temperature: $($config.Temperature)" -ForegroundColor Cyan
    Write-Host "   Mode: STANDARD ASSISTANT" -ForegroundColor Gray
    Write-Host "`n   Features Active:" -ForegroundColor Gray
    Write-Host "      • Standard code suggestions" -ForegroundColor Gray
    Write-Host "      • Single-step responses" -ForegroundColor Gray
    Write-Host "      • Conservative approach" -ForegroundColor Gray
    
    return $true
}

function Show-Status {
    Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  📊 AGENTIC MODE STATUS                            ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    $connection = Test-OllamaConnection
    
    if (-not $connection.Status) {
        Write-Host "`n❌ Ollama is NOT running" -ForegroundColor Red
        Write-Host "   Start Ollama to use agentic mode" -ForegroundColor Yellow
        return
    }
    
    Write-Host "`n✅ Ollama Status: RUNNING" -ForegroundColor Green
    Write-Host "   Models Available: $($connection.Models.Count)" -ForegroundColor Cyan
    
    $config = Load-Config
    
    $modeStatus = if ($config.AgenticMode) { "🚀 ACTIVE" } else { "⏸️  INACTIVE" }
    $modeColor = if ($config.AgenticMode) { "Green" } else { "Gray" }
    
    Write-Host "`n┌────────────────────────────────────────────────────┐" -ForegroundColor Cyan
    Write-Host "│ AGENTIC MODE: $modeStatus" -ForegroundColor $modeColor
    Write-Host "└────────────────────────────────────────────────────┘" -ForegroundColor Cyan
    
    Write-Host "`nConfiguration:" -ForegroundColor Cyan
    Write-Host "   Agentic Model: $($config.AgenticModel)" -ForegroundColor Green
    Write-Host "   Standard Model: $($config.StandardModel)" -ForegroundColor Gray
    Write-Host "   Agentic Temperature: $($config.AgenticTemperature)" -ForegroundColor Yellow
    Write-Host "   Standard Temperature: $($config.Temperature)" -ForegroundColor Gray
    Write-Host "   Last Updated: $($config.Timestamp)" -ForegroundColor Gray
    
    Write-Host "`nAvailable Models:" -ForegroundColor Cyan
    $connection.Models | ForEach-Object {
        $isAgentic = $_ -match "agentic|stealth|unleashed"
        $icon = if ($isAgentic) { "🚀" } else { "📦" }
        $isCurrent = if ($_ -eq $config.AgenticModel -and $config.AgenticMode) { " ← ACTIVE" } else { "" }
        Write-Host "   $icon $_$isCurrent" -ForegroundColor $(if ($isAgentic) { "Green" } else { "Gray" })
    }
    
    Write-Host "`n"
}

function Show-Usage {
    Write-Host "
╔════════════════════════════════════════════════════╗
║   🤖 AGENTIC MODE TOGGLE - Usage Guide            ║
╚════════════════════════════════════════════════════╝

USAGE:
    .\Agentic-Mode-Toggle.ps1 [on|off|status]

COMMANDS:
    on      - Enable agentic mode (autonomous reasoning)
    off     - Disable agentic mode (standard mode)
    status  - Show current agentic mode status

EXAMPLES:
    .\Agentic-Mode-Toggle.ps1 on
    .\Agentic-Mode-Toggle.ps1 off
    .\Agentic-Mode-Toggle.ps1

WHAT IS AGENTIC MODE?
    When enabled, your local Ollama models operate in autonomous mode with:
    • Multi-step planning and reasoning
    • Autonomous decision making
    • Complex problem solving
    • Code generation without approval
    • Adaptive strategies for challenges

CONFIG LOCATION:
    $ConfigFile

REQUIREMENTS:
    • Ollama running locally (http://localhost:11434)
    • At least one agentic model (cheetah-stealth, bigdaddyg-agentic, etc.)

" -ForegroundColor White
}

# Main execution
Write-Host "`n" -ForegroundColor White

switch ($Action.ToLower()) {
    "on" {
        $result = Enable-AgenticMode
        if ($result) {
            Write-Host "`n💡 Tip: Use this with your IDE or terminal for autonomous code assistance" -ForegroundColor Cyan
        }
    }
    "off" {
        $result = Disable-AgenticMode
        if ($result) {
            Write-Host "`n💡 Tip: You can re-enable agentic mode anytime with 'on' command" -ForegroundColor Cyan
        }
    }
    "status" {
        Show-Status
    }
    default {
        Show-Status
    }
}

Write-Host "`n"
