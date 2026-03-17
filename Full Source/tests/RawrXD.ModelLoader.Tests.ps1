# RawrXD.ModelLoader.Tests.ps1

$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root 'RawrXD.Logging.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Config.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Tracing.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.ErrorHandling.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.ModelLoader.psm1') -Force

$config = Get-RawrXDModelConfig
if (-not $config.models) { throw 'Model config not loaded' }

$defaultModel = $config.defaults.default_model
if (-not $defaultModel) { throw 'Default model missing' }

$definition = Get-RawrXDModelDefinition -ModelName $defaultModel
if (-not $definition) { throw "Model not found: $defaultModel" }

Write-Host "ModelLoader tests passed for $defaultModel" -ForegroundColor Green
