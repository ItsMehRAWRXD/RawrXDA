# Parse RawrXD Logger lines containing [MoEPathDiverge] and print per-layer counts + sample rows.
# Usage: Get-Content staging.log | .\Summarize-MoePathDivergeLogs.ps1
#    or: .\Summarize-MoePathDivergeLogs.ps1 -Path D:\logs\rawr.log
param([string] $Path = "")

$ErrorActionPreference = "Stop"
$lines = if ($Path) { Get-Content -LiteralPath $Path } else { $input | ForEach-Object { $_ } }
$rows = foreach ($line in $lines) {
    if ($line -notmatch '\[MoEPathDiverge\]') { continue }
    $o = [ordered]@{}
    if ($line -match 'planGen=(\d+)') { $o.planGen = [uint64]$Matches[1] }
    if ($line -match 'layer=(\d+)') { $o.layer = [int]$Matches[1] }
    if ($line -match 'work=([\d.eE+-]+)') { $o.work = $Matches[1] }
    if ($line -match 'R_dyn=([\d.eE+-]+)') { $o.R_dyn = $Matches[1] }
    if ($line -match 'chosen=(\w+)') { $o.chosen = $Matches[1] }
    if ($line -match 'staticChoice=(\w+)') { $o.staticChoice = $Matches[1] }
    if ($o.Count -gt 0) { [pscustomobject]$o }
}
if (-not $rows) {
    Write-Host "No [MoEPathDiverge] lines found."
    exit 0
}
Write-Host "Total divergence lines: $($rows.Count)"
$rows | Group-Object layer | Sort-Object Name | ForEach-Object {
    Write-Host ("  layer {0}: {1}" -f $_.Name, $_.Count)
}
Write-Host "Sample (first 5):"
$rows | Select-Object -First 5 | Format-Table -AutoSize
