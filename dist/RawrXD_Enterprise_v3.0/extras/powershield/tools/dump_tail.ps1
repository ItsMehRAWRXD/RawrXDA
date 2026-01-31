param(
  [int]$N = 200
)
$src = 'RawrXD.ps1'
$lines = Get-Content -LiteralPath $src -ErrorAction Stop
$total = $lines.Count
$start = [Math]::Max(1, $total - $N + 1)
$out = 'tools/rawrxd_tail_excerpt.txt'
$buffer = New-Object System.Collections.Generic.List[string]
for ($i = $start; $i -le $total; $i++) {
    $buffer.Add(('{0,6}: {1}' -f $i, $lines[$i-1]))
}
$buffer | Set-Content -LiteralPath $out -Encoding UTF8
Write-Host "Wrote lines $start..$total to $out"
