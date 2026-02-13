param(
  [int]$StartLine = 13750,
  [int]$EndLine = 13950,
  [string]$Src = 'RawrXD.ps1',
  [string]$OutHex = 'tools/hex_region.txt'
)
$bytes = [System.IO.File]::ReadAllBytes($Src)
$lines = @()
$lineStart = 0
$lineNo = 1
for ($i = 0; $i -lt $bytes.Length; $i++) {
    if ($bytes[$i] -eq 10) { # LF
        $lines += @{Start=$lineStart; End=$i; Line=$lineNo}
        $lineNo++
        $lineStart = $i + 1
    }
}
# add last line
if ($lineStart -lt $bytes.Length) { $lines += @{Start=$lineStart; End=$bytes.Length-1; Line=$lineNo} }

# find requested range
$selected = $lines | Where-Object { $_.Line -ge $StartLine -and $_.Line -le $EndLine }
if ($selected.Count -eq 0) { Write-Host "No lines found in range $StartLine..$EndLine (file lines: $($lines.Count))"; exit 1 }

$out = New-Object System.Text.StringBuilder
foreach ($entry in $selected) {
    $s = $entry.Start; $e = $entry.End; $ln = $entry.Line
    $len = $e - $s + 1
    $chunk = $bytes[$s..$e]
    $hex = ($chunk | ForEach-Object { $_.ToString('X2') }) -join ' '
    # ASCII printable
    $ascii = ($chunk | ForEach-Object { if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { '.' } }) -join ''
    $out.AppendLine(('LINE {0} (bytes {1}-{2}, len {3}):' -f $ln, $s, $e, $len)) | Out-Null
    $out.AppendLine($hex) | Out-Null
    $out.AppendLine($ascii) | Out-Null
    $out.AppendLine('') | Out-Null
}
$out.ToString() | Set-Content -LiteralPath $OutHex -Encoding UTF8
Write-Host "Wrote hex+ascii of lines $StartLine..$EndLine to $OutHex"
