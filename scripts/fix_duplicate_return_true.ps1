# DO NOT RUN. This script was used once and caused mangles (wrong returns, for-loop breaks).
# Fix duplicate "return true" and stray returns BY HAND per file to avoid control-flow damage.
# Kept for reference only.

$dir = Join-Path $PSScriptRoot "..\src\win32app"
$files = Get-ChildItem -LiteralPath $dir -Filter "*.cpp" -File
$total = 0

foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    $orig = $text
    do {
        $prev = $text
        # Same indent
        $text = $text -replace '(\r?\n)([ \t]+)return true;\r?\n\2return true;', '$1$2return true;'
        # Different indent (second line is duplicate)
        $text = $text -replace '(\r?\n)([ \t]+)return true;(\r?\n)\s+return true;', '$1$2return true;'
    } while ($text -ne $prev)
    if ($text -ne $orig) {
        [System.IO.File]::WriteAllText($f.FullName, $text)
        $total++
        Write-Host $f.Name
    }
}

Write-Host "Updated $total files."
