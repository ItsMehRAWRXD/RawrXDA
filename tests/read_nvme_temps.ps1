param(
    [string]$MapName = "Global\SOVEREIGN_NVME_TEMPS",
    [int]$MaxDrives = 16
)

try {
    $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($MapName)
} catch {
    Write-Error "OpenExisting failed: $($_.Exception.Message)"
    exit 1
}

$accessor = $mmf.CreateViewAccessor(0, 0, [System.IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)

$signature = $accessor.ReadInt32(0)
$version = $accessor.ReadInt32(4)
$driveCount = $accessor.ReadInt32(8)
$timestamp = $accessor.ReadInt64(144)

if ($signature -ne 0x45564F53 -or $version -ne 1) {
    Write-Error "Invalid signature/version in shared memory."
    $accessor.Dispose()
    $mmf.Dispose()
    exit 1
}

if ($driveCount -gt $MaxDrives) { $driveCount = $MaxDrives }

Write-Host "Sovereign NVMe Temps (count=$driveCount)"
Write-Host "Timestamp(ms): $timestamp"

$tempsOffset = 16
$wearOffset = 16 + (4 * $MaxDrives)

for ($i = 0; $i -lt $driveCount; $i++) {
    $temp = $accessor.ReadInt32($tempsOffset + ($i * 4))
    $wear = $accessor.ReadInt32($wearOffset + ($i * 4))
    Write-Host ("Drive {0}: temp={1}C wear={2}" -f $i, $temp, $wear)
}

$accessor.Dispose()
$mmf.Dispose()
