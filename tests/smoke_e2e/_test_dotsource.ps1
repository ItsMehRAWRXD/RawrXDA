$ErrorActionPreference = 'Stop'
. 'D:\rxdn_smoke_tests\modules\IpcFramingHelper.psm1'
Write-Host "Get-IpcConstants:" (Get-Command Get-IpcConstants)
$ipc = Get-IpcConstants
Write-Host "MaxSingle:" $ipc.MaxSinglePayload
