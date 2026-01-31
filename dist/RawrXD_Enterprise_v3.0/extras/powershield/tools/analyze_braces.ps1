$path = Resolve-Path 'RawrXD.ps1'
$lines = Get-Content -LiteralPath $path.Path
$balance = 0
for ($i=0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    $opens = ([regex]::Matches($line, '{')).Count
    $closes = ([regex]::Matches($line, '}')).Count
    $balance += $opens - $closes
    if ($balance -lt 0) {
        Write-Output "NEGATIVE balance at line $($i+1): $balance -> $line"
    }
}
Write-Output "FINAL BALANCE: $balance"
if ($balance -ne 0) { exit 1 } else { exit 0 }
