# simple_regex_test.ps1

$testContent = @"
### ✅ What's Done
- **[Task 1]**: Description ✓
- **[Task 2]**: Description ✓
### 📊
"@

Write-Host "Test content:"
Write-Host $testContent
Write-Host ""

$taskMatches = [regex]::Matches($testContent, "\\- ")
Write-Host "Task matches count: $($taskMatches.Count)"

foreach ($match in $taskMatches) {
    Write-Host "Match: $($match.Value)"
}