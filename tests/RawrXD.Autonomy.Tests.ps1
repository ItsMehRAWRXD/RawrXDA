# RawrXD.Autonomy.Tests.ps1

$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root 'RawrXD.Logging.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Tracing.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.ErrorHandling.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Agentic.Autonomy.psm1') -Force

Set-RawrXDAutonomyGoal -Goal 'Test autonomy goal'
$plan = Get-RawrXDAutonomyPlan -Goal 'Test autonomy goal'
if (-not $plan -or $plan.Count -eq 0) { throw 'Autonomy plan empty' }

$result = Invoke-RawrXDAutonomyAction -Action $plan[0]
if (-not $result.Success) { throw 'Autonomy action failed' }

Write-Host 'Autonomy scaffolding tests passed' -ForegroundColor Green
