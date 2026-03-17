param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
)

$extPath = Join-Path $RepoRoot "src/core/ssot_handlers_ext.cpp"
$extDedicatedPath = Join-Path $RepoRoot "src/core/ssot_handlers_ext_dedicated.cpp"
$beaconSymbolsPath = Join-Path $RepoRoot "src/core/ssot_beacon.symbols.txt"

foreach ($p in @($extPath, $extDedicatedPath, $beaconSymbolsPath)) {
    if (!(Test-Path $p)) { throw "Missing required file: $p" }
}

$extHandlers = Select-String -Path $extPath -Pattern '^\s*CommandResult\s+(handle[A-Za-z0-9_]+)\s*\(' |
    ForEach-Object { $_.Matches[0].Groups[1].Value }

$dups = $extHandlers | Group-Object | Where-Object { $_.Count -gt 1 }
if ($dups.Count -gt 0) {
    $names = ($dups | ForEach-Object { $_.Name }) -join ', '
    throw "Duplicate handler definitions in ssot_handlers_ext.cpp: $names"
}

$extSet = $extHandlers | Sort-Object -Unique
$extDedicatedHandlers = Select-String -Path $extDedicatedPath -Pattern '^\s*CommandResult\s+(handle[A-Za-z0-9_]+)\s*\(' |
    ForEach-Object { $_.Matches[0].Groups[1].Value } |
    Sort-Object -Unique
$beaconSymbols = Get-Content $beaconSymbolsPath |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith('#') } |
    Sort-Object -Unique

$missingDedicated = $extDedicatedHandlers | Where-Object { $extSet -notcontains $_ }
if ($missingDedicated.Count -gt 0) {
    throw "Dedicated EXT handlers missing in ssot_handlers_ext.cpp: $($missingDedicated -join ', ')"
}

$missingBeacons = $beaconSymbols | Where-Object { $extSet -notcontains $_ }
if ($missingBeacons.Count -gt 0) {
    throw "Beacon symbols missing in ssot_handlers_ext.cpp: $($missingBeacons -join ', ')"
}

$missingFromBeaconManifest = $extSet | Where-Object { $beaconSymbols -notcontains $_ }
if ($missingFromBeaconManifest.Count -gt 0) {
    throw "ssot_beacon.symbols.txt missing EXT handlers: $($missingFromBeaconManifest -join ', ')"
}

Write-Host "SSOT ownership verify OK: ext=$($extSet.Count) ext_dedicated=$($extDedicatedHandlers.Count) beacons=$($beaconSymbols.Count)"
