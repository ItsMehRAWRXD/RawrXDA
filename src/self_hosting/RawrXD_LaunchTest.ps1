# Mock wrapper to verify Handover doesn't break the environment
$ErrorActionPreference = 'Stop'
Write-Host 'Testing Native UI Handover...'
& 'D:\rawrxd\RawrXD.ps1' -BypassInit -TestMode
