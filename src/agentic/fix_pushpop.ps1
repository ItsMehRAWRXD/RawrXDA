$file = "RawrXD_Complete_Explicit.asm"
$content = Get-Content $file -Raw

# Replace multi-register push statements (3+ regs)
$content = $content -replace '(\s+)push\s+(\w+)\s+(\w+)\s+(\w+)(\s)', "`$1push `$2`n`$1push `$3`n`$1push `$4`$5"

# Replace multi-register push statements (2 regs)
$content = $content -replace '(\s+)push\s+(\w+)\s+(\w+)(\s)', "`$1push `$2`n`$1push `$3`$4"

# Replace multi-register pop statements (3+ regs, reverse order for pop)
$content = $content -replace '(\s+)pop\s+(\w+)\s+(\w+)\s+(\w+)(\s)', "`$1pop `$4`n`$1pop `$3`n`$1pop `$2`$5"

# Replace multi-register pop statements (2 regs, reverse order for pop)
$content = $content -replace '(\s+)pop\s+(\w+)\s+(\w+)(\s)', "`$1pop `$3`n`$1pop `$2`$4"

Set-Content $file $content
Write-Host "✓ Fixed all multi-register push/pop statements"
