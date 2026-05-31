# debug_pattern.ps1

$testLine = "- **[Completion Protocol Definition]**: Created comprehensive markdown protocol specification ✓"

Write-Host "Test line: '$testLine'"
Write-Host "Length: $($testLine.Length)"

# Test different patterns
$patterns = @(
    "- \[",
    "- \*\*\[",
    "- ",
    "\[",
    "\*\*\["
)

foreach ($pattern in $patterns) {
    $count = [regex]::Matches($testLine, $pattern).Count
    Write-Host "Pattern '$pattern' matches: $count"
}

# Check character by character
Write-Host ""
Write-Host "Character analysis:"
for ($i = 0; $i -lt 15; $i++) {
    $char = $testLine[$i]
    $code = [int][char]$char
    Write-Host "Char $i : '$char' (U+$($code.ToString('X4')))"
}