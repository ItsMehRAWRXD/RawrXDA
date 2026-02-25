
$filePath = "d:\rawrxd\src\asm\quantum_beaconism_backend.asm"
$content = Get-Content $filePath -Raw

$content = $content -replace "RawrXD_RawrXD_", "RawrXD_"

Set-Content $filePath $content
