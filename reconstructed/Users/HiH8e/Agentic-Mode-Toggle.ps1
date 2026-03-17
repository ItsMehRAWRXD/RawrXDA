# Local Agentic Copilot - Agentic Mode Toggle Script
# Controls the agentic mode configuration for the Local Agentic Copilot extension

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("on", "off", "status")]
    [string]$Action = "status"
)

$ConfigPath = Join-Path $env:APPDATA "Ollama\agentic-config.json"

function Get-Config {
    if (Test-Path $ConfigPath) {
        try {
            $content = Get-Content $ConfigPath -Raw
            return $content | ConvertFrom-Json
        }
        catch {
            Write-Host "⚠️  Error reading config file. Creating new config." -ForegroundColor Yellow
            return @{ agenticMode = $false }
        }
    }
    else {
        return @{ agenticMode = $false }
    }
}

function Set-Config {
    param([bool]$AgenticMode)
    
    $config = @{
        agenticMode = $AgenticMode
        lastUpdated = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    }
    
    $config | ConvertTo-Json | Set-Content $ConfigPath -Encoding UTF8
}

function Show-Status {
    $config = Get-Config
    
    if ($config.agenticMode) {
        Write-Host "🚀 AGENTIC MODE: ON" -ForegroundColor Green
        Write-Host "   Autonomous reasoning active"
    }
    else {
        Write-Host "⏸️  AGENTIC MODE: OFF (Standard Mode)" -ForegroundColor Gray
        Write-Host "   Direct, safe code suggestions"
    }
    
    Write-Host ""
    Write-Host "Configuration file: $ConfigPath" -ForegroundColor Cyan
}

# Main logic
switch ($Action.ToLower()) {
    "on" {
        Set-Config -AgenticMode $true
        Show-Status
        Write-Host ""
        Write-Host "✅ Agentic Mode enabled! Your Local Agentic Copilot will now use autonomous reasoning." -ForegroundColor Green
    }
    
    "off" {
        Set-Config -AgenticMode $false
        Show-Status
        Write-Host ""
        Write-Host "✅ Agentic Mode disabled! Your Local Agentic Copilot will now use standard mode." -ForegroundColor Blue
    }
    
    "status" {
        Show-Status
    }
    
    default {
        Write-Host "❌ Invalid action. Use: on, off, or status" -ForegroundColor Red
        exit 1
    }
}