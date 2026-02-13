param(
  [string]$Src = 'RawrXD.ps1',
  [int]$Start = 13830,
  [int]$End = 13870,
  [string]$Out = 'tools/excerpt_fixed.txt'
)
$reader = [System.IO.File]::OpenText($Src)
try {
  $lineNo = 0
  $outLines = New-Object System.Collections.Generic.List[string]
  while (-not $reader.EndOfStream) {
    $line = $reader.ReadLine()
    $lineNo++
    if ($lineNo -ge $Start -and $lineNo -le $End) {
      $outLines.Add(("{0,6}: {1}" -f $lineNo, $line))
    }
    if ($lineNo -gt $End) { break }
  }
  $outLines | Set-Content -LiteralPath $Out -Encoding UTF8
  Write-Host "Wrote lines $Start..$([Math]::Min($End,$lineNo)) to $Out"
}
finally {
  $reader.Close()
}
