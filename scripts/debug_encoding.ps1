# debug_encoding.ps1

# Test with simple pattern first
$simpleText = "- [Task 1]: Description"
Write-Host "Simple text: '$simpleText'"
Write-Host "Length: $($simpleText.Length)"

$matches = [regex]::Matches($simpleText, "\\- ")
Write-Host "Simple matches: $($matches.Count)"

# Test with actual content
$testContent = @"
### ✅ What's Done
- **[Task 1]**: Description ✓
- **[Task 2]**: Description ✓
### 📊
"@

Write-Host ""
Write-Host "Test content:"
Write-Host "'$testContent'"
Write-Host "Length: $($testContent.Length)"

# Try different patterns
$pattern1 = "\\- "
$pattern2 = "\["
$pattern3 = "Task"

Write-Host "Pattern '- ' matches: $([regex]::Matches($testContent, $pattern1).Count)"
Write-Host "Pattern '[' matches: $([regex]::Matches($testContent, $pattern2).Count)" 
Write-Host "Pattern 'Task' matches: $([regex]::Matches($testContent, $pattern3).Count)"