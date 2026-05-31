$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\modules\IpcFramingHelper.ps1"

$boundary = 'b' * 65522
$legacyFrame = New-LegacyFrame -PayloadUtf8 $boundary -MessageType 2
$reasm = [MockIpcReassembler]::new()
$outType = 0
$outPayload = ''
$ok = $reasm.FeedPhysicalFrame($legacyFrame, [ref]$outType, [ref]$outPayload)
Write-Host "reasm ok=$ok type=$outType payloadLen=$($outPayload.Length)"
