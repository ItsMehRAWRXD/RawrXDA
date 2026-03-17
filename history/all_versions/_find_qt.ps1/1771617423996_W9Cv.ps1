$files = Get-ChildItem d:\rawrxd\src,d:\rawrxd\include -Recurse -Include '*.h','*.hpp','*.cpp' -ErrorAction SilentlyContinue

Write-Host "=== Qt Type Names (Q-prefixed) ==="
$files | Select-String '\bQ[A-Z][a-zA-Z]+\b' | ForEach-Object {
    [regex]::Matches($_.Line, '\bQ[A-Z][a-zA-Z]+\b') | ForEach-Object { $_.Value }
} | Sort-Object -Unique

Write-Host ""
Write-Host "=== Qt Macros (Q_ prefixed) ==="
$files | Select-String '\bQ_[A-Z_]+\b' | ForEach-Object {
    [regex]::Matches($_.Line, '\bQ_[A-Z_]+\b') | ForEach-Object { $_.Value }
} | Sort-Object -Unique

Write-Host ""
Write-Host "=== signals:/slots:/emit usage count ==="
$count = ($files | Select-String '\bsignals\s*:|public\s+slots\s*:|private\s+slots\s*:|protected\s+slots\s*:|\bemit\s+' -List | Measure-Object).Count
Write-Host "Files with signals/slots/emit: $count"

Write-Host ""
Write-Host "=== signals:/slots:/emit detail ==="
$files | Select-String '\bsignals\s*:|public\s+slots\s*:|private\s+slots\s*:|protected\s+slots\s*:|\bemit\s+' | ForEach-Object {
    "$($_.Filename):$($_.LineNumber): $($_.Line.Trim())"
}
