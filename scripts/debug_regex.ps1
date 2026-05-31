# debug_regex.ps1

$testContent = @"
### ✅ What's Done
- **[Task 1]**: Description ✓
- **[Task 2]**: Description ✓
### 📊
"@

# Test exact pattern matching
$lineWithDash = $testContent -split "\n" | Where-Object { $_ -match "^-" } | Select-Object -First 1

Write-Host "Line with dash: '$lineWithDash'"
Write-Host "Length: $($lineWithDash.Length)"

# Test character by character around the dash
for ($i = 0; $i -lt 5; $i++) {
    $char = $lineWithDash[$i]
    $code = [int][char]$char
    Write-Host "Char $i : '$char' (U+$($code.ToString('X4')))"
}

# Test exact regex patterns
Write-Host ""
Write-Host "Regex tests:"
Write-Host "Pattern '^-' matches: $($lineWithDash -match '^-')"
Write-Host "Pattern '^\\-' matches: $($lineWithDash -match '^\\-')"
Write-Host "Pattern '^- ' matches: $($lineWithDash -match '^- ')"
Write-Host "Pattern '^\\- ' matches: $($lineWithDash -match '^\\- ')"

# Test with [regex] class
Write-Host ""
Write-Host "[regex] class tests:"
Write-Host "Matches for '^-': $([regex]::Matches($lineWithDash, '^-').Count)"
Write-Host "Matches for '^\\-': $([regex]::Matches($lineWithDash, '^\\-').Count)"
Write-Host "Matches for '^- ': $([regex]::Matches($lineWithDash, '^- ').Count)"
Write-Host "Matches for '^\\- ': $([regex]::Matches($lineWithDash, '^\\- ').Count)"