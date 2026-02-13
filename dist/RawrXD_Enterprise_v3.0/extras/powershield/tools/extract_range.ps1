param(
  [int]$Start = 13840,
  [int]$End = 13860,
  [string]$Src = 'RawrXD.ps1',
  [string]$Out = 'tools/range_excerpt.txt'
)
$lines = Get-Content -LiteralPath $Src -ErrorAction Stop
$total = $lines.Count
if ($Start -lt 1) { $Start = 1 }
if ($End -gt $total) { $End = $total }
$selection = $lines[$Start-1..($End-1)]
Set-Content -LiteralPath $Out -Value ($selection | ForEach-Object -Begin {$i=$Start-1} -Process { $i++; ('{0,6}: {1}' -f $i, $_) }) -Encoding UTF8
Write-Host "Wrote lines $Start..$End to $Out (total lines: $total)"
