$srcDir = 'D:\rawrxd\src'
$pattern = '(?mi)(return\s+(false|true|nullptr|0|"");?\s*//.*(stub|todo|placeholder|fixme|not.?impl)|//\s*(TODO|STUB|FIXME|PLACEHOLDER|NOT.?IMPL))'

Get-ChildItem $srcDir -Filter '*.cpp' | ForEach-Object {
    $content = [IO.File]::ReadAllText($_.FullName)
    $matches = [regex]::Matches($content, $pattern)
    if ($matches.Count -gt 0) {
        Write-Host ('{0,-45} {1} hit(s)' -f $_.Name, $matches.Count) -ForegroundColor Yellow
        foreach ($m in $matches) {
            $lineNum = ($content.Substring(0, $m.Index) -split "`n").Count
            $line = $m.Value.Trim()
            if ($line.Length -gt 100) { $line = $line.Substring(0, 100) + '...' }
            Write-Host ('  L{0,-5} {1}' -f $lineNum, $line) -ForegroundColor Gray
        }
    }
}
# Also scan headers
Get-ChildItem $srcDir -Filter '*.h' | ForEach-Object {
    $content = [IO.File]::ReadAllText($_.FullName)
    $matches = [regex]::Matches($content, $pattern)
    if ($matches.Count -gt 0) {
        Write-Host ('{0,-45} {1} hit(s)' -f $_.Name, $matches.Count) -ForegroundColor Yellow
        foreach ($m in $matches) {
            $lineNum = ($content.Substring(0, $m.Index) -split "`n").Count
            $line = $m.Value.Trim()
            if ($line.Length -gt 100) { $line = $line.Substring(0, 100) + '...' }
            Write-Host ('  L{0,-5} {1}' -f $lineNum, $line) -ForegroundColor Gray
        }
    }
}
Get-ChildItem $srcDir -Filter '*.hpp' | ForEach-Object {
    $content = [IO.File]::ReadAllText($_.FullName)
    $matches = [regex]::Matches($content, $pattern)
    if ($matches.Count -gt 0) {
        Write-Host ('{0,-45} {1} hit(s)' -f $_.Name, $matches.Count) -ForegroundColor Yellow
        foreach ($m in $matches) {
            $lineNum = ($content.Substring(0, $m.Index) -split "`n").Count
            $line = $m.Value.Trim()
            if ($line.Length -gt 100) { $line = $line.Substring(0, 100) + '...' }
            Write-Host ('  L{0,-5} {1}' -f $lineNum, $line) -ForegroundColor Gray
        }
    }
}
Write-Host "`nScan complete." -ForegroundColor Cyan
