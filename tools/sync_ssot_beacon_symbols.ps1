param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
)

$extPath = Join-Path $RepoRoot "src/core/ssot_handlers_ext.cpp"
$symbolsPath = Join-Path $RepoRoot "src/core/ssot_beacon.symbols.txt"

if (!(Test-Path $extPath)) { throw "Missing $extPath" }

$handlers = Select-String -Path $extPath -Pattern '^\s*CommandResult\s+(handle[A-Za-z0-9_]+)\s*\(' |
    ForEach-Object { $_.Matches[0].Groups[1].Value } |
    Sort-Object -Unique

$attempts = 0
$maxAttempts = 10
while ($true) {
    try {
        Set-Content -Path $symbolsPath -Value $handlers -Encoding ascii -ErrorAction Stop
        break
    } catch {
        $attempts++
        if ($attempts -ge $maxAttempts) { throw }
        Start-Sleep -Milliseconds 100
    }
}

Write-Host "Synced $symbolsPath from ssot_handlers_ext.cpp ($($handlers.Count) symbols)"
