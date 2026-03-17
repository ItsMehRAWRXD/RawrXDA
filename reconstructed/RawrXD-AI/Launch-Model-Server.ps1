<#
.SYNOPSIS
    RawrXD-AI Model Server Launcher
    Starts llama-server with the appropriate model for IDE integration.

.PARAMETER Model
    Which model variant to load: daily_driver, balanced, high_quality, maximum

.PARAMETER Port
    Server port (default: 8080)

.EXAMPLE
    .\Launch-Model-Server.ps1
    .\Launch-Model-Server.ps1 -Model balanced
    .\Launch-Model-Server.ps1 -Model high_quality -Port 8081
#>

param(
    [ValidateSet("daily_driver", "balanced", "high_quality", "maximum")]
    [string]$Model = "daily_driver",
    [int]$Port = 8080
)

$ErrorActionPreference = "Stop"

# Load bridge config
$configPath = Join-Path $PSScriptRoot "ide_bridge_config.json"
if (-not (Test-Path $configPath)) {
    Write-Host "ERROR: Bridge config not found at $configPath" -ForegroundColor Red
    exit 1
}

$config = Get-Content $configPath | ConvertFrom-Json
$serverBin = $config.server.binary
$modelConfig = $config.server.models.$Model
$params = $config.server.parameters

if (-not (Test-Path $serverBin)) {
    Write-Host "ERROR: llama-server not found at $serverBin" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $modelConfig.path)) {
    Write-Host "ERROR: Model not found at $($modelConfig.path)" -ForegroundColor Red
    Write-Host "Run the training pipeline first: python run_pipeline.py" -ForegroundColor Yellow
    exit 1
}

$modelSizeGB = [math]::Round((Get-Item $modelConfig.path).Length / 1GB, 1)

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  RawrXD-AI Model Server" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Model:    $Model ($modelSizeGB GB)" -ForegroundColor White
Write-Host "  Path:     $($modelConfig.path)" -ForegroundColor Gray
Write-Host "  GPU:      $($modelConfig.n_gpu_layers) layers offloaded" -ForegroundColor White
Write-Host "  VRAM fit: $($modelConfig.fits_vram)" -ForegroundColor White
Write-Host "  Port:     $Port" -ForegroundColor White
Write-Host "  Ctx:      $($params.ctx_size) tokens" -ForegroundColor White
Write-Host "  Threads:  $($params.threads)" -ForegroundColor White
Write-Host "  Desc:     $($modelConfig.description)" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Build command
$args = @(
    "-m", $modelConfig.path,
    "-ngl", $modelConfig.n_gpu_layers,
    "-c", $params.ctx_size,
    "-b", $params.batch_size,
    "-t", $params.threads,
    "-n", $params.n_predict,
    "--host", $config.server.host,
    "--port", $Port
)

if ($params.flash_attn) {
    $args += "--flash-attn"
}

Write-Host "Starting server..." -ForegroundColor Green
Write-Host "  Endpoint: http://$($config.server.host):$Port" -ForegroundColor Green
Write-Host "  Health:   http://$($config.server.host):$Port/health" -ForegroundColor Green
Write-Host "  Chat:     http://$($config.server.host):$Port/v1/chat/completions" -ForegroundColor Green
Write-Host ""
Write-Host "Press Ctrl+C to stop the server." -ForegroundColor Yellow
Write-Host ""

# Launch
& $serverBin @args
