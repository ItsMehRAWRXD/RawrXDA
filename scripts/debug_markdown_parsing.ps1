# debug_markdown_parsing.ps1

$testContent = @"
## 🎯 Session Complete 

### ✅ What's Done
- **[Completion Protocol Definition]**: Created comprehensive markdown protocol specification ✓
- **[Agent Validator Enhancement]**: Updated validator to parse markdown completion format ✓
- **[Execution Wrapper]**: Built agent wrapper with strict contract enforcement ✓
- **[C++ Helper Library]**: Added native completion emission for Win32IDE integration ✓

### 📊 Current State
"@

# Test the regex pattern
if ($testContent -match "### ✅ What's Done[\s\S]*?### 📊") {
    Write-Host "MATCH FOUND:" -ForegroundColor Green
    $tasksSection = $Matches[0]
    Write-Host "Tasks section: $tasksSection" -ForegroundColor Cyan
    
    $taskMatches = [regex]::Matches($tasksSection, "\- \[.*?\]:")
    Write-Host "Task matches count: $($taskMatches.Count)" -ForegroundColor Yellow
    
    foreach ($match in $taskMatches) {
        Write-Host "Match: $($match.Value)" -ForegroundColor White
    }
} else {
    Write-Host "NO MATCH" -ForegroundColor Red
}