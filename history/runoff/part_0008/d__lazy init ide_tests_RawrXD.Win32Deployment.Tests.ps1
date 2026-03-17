# RawrXD.Win32Deployment.Tests.ps1

$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root 'RawrXD.Logging.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Config.psm1') -Force
Import-Module (Join-Path $root 'RawrXD.Win32Deployment.psm1') -Force

$checks = Test-RawrXDWin32Prereqs
if (-not $checks) { throw 'Prereq check returned empty result' }

Write-Host "Win32 prereq checks completed: $($checks.Count) items" -ForegroundColor Green
