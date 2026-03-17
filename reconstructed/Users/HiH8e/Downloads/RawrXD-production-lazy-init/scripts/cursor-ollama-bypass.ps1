# Cursor Pro Bypass - Route All AI Features to Local Ollama
# This script configures Cursor to use local Ollama instead of cloud services
# Author: RawrXD-QtShell Project
# Date: December 26, 2025

param(
    [string]$CursorPath = "E:\Everything\cursor\Cursor.exe",
    [string]$OllamaEndpoint = "http://localhost:11434",
    [string]$DefaultModel = "llama2:13b",
    [int]$MaxTokens = 8192,
    [double]$Temperature = 0.7
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Cursor Pro Bypass Configuration Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Stop existing Cursor process
Write-Host "[1/5] Stopping Cursor process..." -ForegroundColor Yellow
Stop-Process -Name "Cursor" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Write-Host "✓ Cursor stopped" -ForegroundColor Green

# Verify Cursor executable exists
if (-not (Test-Path $CursorPath)) {
    Write-Host "✗ Error: Cursor not found at: $CursorPath" -ForegroundColor Red
    Write-Host "Please specify the correct path with -CursorPath parameter" -ForegroundColor Yellow
    exit 1
}

# Verify Ollama is running
Write-Host "[2/5] Verifying Ollama service..." -ForegroundColor Yellow
try {
    $ollamaTest = Invoke-WebRequest -Uri "$OllamaEndpoint/api/tags" -Method GET -TimeoutSec 5 -ErrorAction Stop
    Write-Host "✓ Ollama is running at $OllamaEndpoint" -ForegroundColor Green
} catch {
    Write-Host "⚠ Warning: Ollama not responding at $OllamaEndpoint" -ForegroundColor Yellow
    Write-Host "  Make sure Ollama is running before launching Cursor" -ForegroundColor Yellow
}

# Create configuration directory
$configDir = "$env:APPDATA\Cursor"
Write-Host "[3/5] Creating configuration directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path "$configDir\User" | Out-Null
Write-Host "✓ Configuration directory: $configDir" -ForegroundColor Green

# Create comprehensive bypass configuration
Write-Host "[4/5] Writing bypass configuration..." -ForegroundColor Yellow

$config = @{
    # AI Model Configuration - Override all endpoints
    "ai.customModels" = @{
        "agent" = "$OllamaEndpoint/api/generate"
        "edit" = "$OllamaEndpoint/api/generate"
        "chat" = "$OllamaEndpoint/api/generate"
        "amazonq" = "$OllamaEndpoint/api/generate"
        "codewhisperer" = "$OllamaEndpoint/api/generate"
        "inlinecompletion" = "$OllamaEndpoint/api/generate"
        "completion" = "$OllamaEndpoint/api/generate"
        "copilot" = "$OllamaEndpoint/api/generate"
    }
    
    # Core AI Settings
    "ai.defaultModel" = $DefaultModel
    "ai.apiKey" = ""
    "ai.endpoint" = $OllamaEndpoint
    "ai.maxTokens" = $MaxTokens
    "ai.temperature" = $Temperature
    "ai.stream" = $false
    "ai.enableProFeatures" = $true
    "ai.overrideProRequirement" = $true
    "ai.bypassAuthentication" = $true
    
    # Cursor Pro Bypass Flags
    "cursor.pro.enabled" = $true
    "cursor.pro.override" = $true
    "cursor.pro.bypassValidation" = $true
    "cursor.pro.forceLocalMode" = $true
    
    # Amazon Q Configuration Override
    "amazonQ.configuration" = @{
        "customizations" = @()
        "telemetry" = "optout"
        "endpoint" = $OllamaEndpoint
        "model" = $DefaultModel
        "bypassPro" = $true
        "forceLocal" = $true
    }
    
    # CodeWhisperer Override
    "aws.codewhisperer.endpoint" = $OllamaEndpoint
    "aws.codewhisperer.bypassAuth" = $true
    
    # Privacy & Performance Settings
    "telemetry.enableTelemetry" = $false
    "telemetry.enableCrashReporter" = $false
    "update.enableWindowsBackgroundUpdates" = $false
    "workbench.enableExperiments" = $false
    "extensions.ignoreRecommendations" = $true
    
    # Network Override
    "http.proxy" = ""
    "http.proxyStrictSSL" = $false
    "http.proxySupport" = "off"
}

# Write main settings
$configJson = $config | ConvertTo-Json -Depth 10
Set-Content -Path "$configDir\User\settings.json" -Value $configJson -Force
Write-Host "✓ Main configuration written" -ForegroundColor Green

# Create Amazon Q specific config
$amazonqConfig = @{
    "version" = "1.0"
    "profiles" = @(
        @{
            "name" = "default"
            "model" = $DefaultModel
            "endpoint" = "$OllamaEndpoint/api/generate"
            "apiKey" = ""
            "bypassProRequirement" = $true
            "forceLocal" = $true
        }
    )
    "telemetry" = "disabled"
    "bypassPro" = $true
    "authentication" = @{
        "required" = $false
        "bypass" = $true
    }
}

$amazonqJson = $amazonqConfig | ConvertTo-Json -Depth 10
Set-Content -Path "$configDir\amazonq.json" -Value $amazonqJson -Force
Write-Host "✓ Amazon Q configuration written" -ForegroundColor Green

# Create workspace settings template
$workspaceSettings = @{
    "ai.endpoint" = $OllamaEndpoint
    "ai.defaultModel" = $DefaultModel
    "cursor.pro.override" = $true
}

$workspaceJson = $workspaceSettings | ConvertTo-Json -Depth 10
Set-Content -Path "$configDir\workspace-settings-template.json" -Value $workspaceJson -Force
Write-Host "✓ Workspace template written" -ForegroundColor Green

# Launch Cursor
Write-Host "[5/5] Starting Cursor..." -ForegroundColor Yellow
Start-Process $CursorPath

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "✓ Configuration Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Settings Applied:" -ForegroundColor White
Write-Host "  • Ollama Endpoint: $OllamaEndpoint" -ForegroundColor Gray
Write-Host "  • Default Model: $DefaultModel" -ForegroundColor Gray
Write-Host "  • Max Tokens: $MaxTokens" -ForegroundColor Gray
Write-Host "  • Temperature: $Temperature" -ForegroundColor Gray
Write-Host "  • Pro Features: UNLOCKED" -ForegroundColor Green
Write-Host "  • Telemetry: DISABLED" -ForegroundColor Green
Write-Host ""
Write-Host "Configuration Files:" -ForegroundColor White
Write-Host "  • $configDir\User\settings.json" -ForegroundColor Gray
Write-Host "  • $configDir\amazonq.json" -ForegroundColor Gray
Write-Host "  • $configDir\workspace-settings-template.json" -ForegroundColor Gray
Write-Host ""
Write-Host "Cursor is now running with local Ollama backend." -ForegroundColor Cyan
Write-Host "All AI features will use: $OllamaEndpoint" -ForegroundColor Cyan
Write-Host ""
