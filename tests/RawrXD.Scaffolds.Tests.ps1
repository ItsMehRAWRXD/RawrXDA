# RawrXD.Scaffolds.Tests.ps1

$root = Split-Path -Parent $PSScriptRoot

Import-Module (Join-Path $root 'RawrXD.Logging.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Config.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Tracing.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.ErrorHandling.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Metrics.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.ModelLoader.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Agentic.Autonomy.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Win32Deployment.psm1') -Force

Initialize-RawrXDMetrics
Increment-RawrXDCounter -Name 'rawrxd_test_counter' -Value 1
Set-RawrXDGauge -Name 'rawrxd_test_gauge' -Value 1.0
Observe-RawrXDHistogram -Name 'rawrxd_test_hist' -Value 12

$plan = Get-RawrXDAutonomyPlan -Goal 'test'
if (-not $plan -or $plan.Count -eq 0) { throw 'Autonomy plan is empty' }

$result = Invoke-RawrXDSafeOperation -Name 'TestOp' -Script { return 'ok' }
if (-not $result.Success) { throw 'Safe operation failed' }

$metricsFile = Export-RawrXDPrometheusText
if (-not (Test-Path $metricsFile)) { throw 'Metrics export failed' }

Write-Host 'Scaffold tests completed' -ForegroundColor Green
