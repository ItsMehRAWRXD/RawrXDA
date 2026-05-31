$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\modules\IpcFramingHelper.ps1"

$f = New-LegacyFrame -PayloadUtf8 ('a' * 1024)
$w = Add-WirePrefix -PhysicalFrame $f
$m = New-Object System.IO.MemoryStream
$m.Write($w, 0, $w.Length)
$m.Position = 0
$h = [MockIpcStreamHandler]::new($m)
$e = $h.TryExtractPhysicalFrame()
Write-Host "small extract ok=$($null -ne $e) len=$($e.Length)"

$m2 = New-Object System.IO.MemoryStream
$big = New-LegacyFrame -PayloadUtf8 ('b' * 65522)
$w2 = Add-WirePrefix -PhysicalFrame $big
$m2.Write($w2, 0, $w2.Length)
$m2.Position = 0
$h2 = [MockIpcStreamHandler]::new($m2)
$e2 = $h2.TryExtractPhysicalFrame()
Write-Host "max extract ok=$($null -ne $e2) len=$($e2.Length)"
