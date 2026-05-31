# debug_characters.ps1

$testContent = @"
### ✅ What's Done
- **[Task 1]**: Description ✓
- **[Task 2]**: Description ✓
### 📊
"@

# Check each character
Write-Host "Character analysis:"
for ($i = 0; $i -lt $testContent.Length; $i++) {
    $char = $testContent[$i]
    $code = [int][char]$char
    
    if ($char -eq "-" -or $char -eq "[" -or $code -gt 127) {
        Write-Host "Position $i : '$char' (U+$($code.ToString('X4')))"
    }
}

# Check the specific line with dashes
$lines = $testContent -split "\n"
Write-Host ""
Write-Host "Line analysis:"
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match "^-") {
        Write-Host "Line $i : '$($lines[$i])'"
        for ($j = 0; $j -lt $lines[$i].Length; $j++) {
            $char = $lines[$i][$j]
            $code = [int][char]$char
            Write-Host "  Char $j : '$char' (U+$($code.ToString('X4')))"
        }
    }
}