#Requires -Version 7.0
<#
.SYNOPSIS
    Detects scaffolding/stub modules vs real functionality modules
.DESCRIPTION
    Analyzes PowerShell modules to identify which ones are:
    - Real functionality (actual implementation)
    - Scaffolding (TODO comments, empty functions, placeholders)
    - Mixed (some real code but incomplete)
.PARAMETER Path
    Directory to analyze
.PARAMETER OutputPath
    Path for the output report (optional)
#>

param(
    [string]$Path = "d:\lazy init ide",
    [string]$OutputPath = ""
)

# Generate timestamped filename if not specified
if (-not $OutputPath) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputPath = Join-Path $Path "ScaffoldingDetection_$timestamp.txt"
}

# Create StringBuilder for efficient string building
$report = [System.Text.StringBuilder]::new()

# Helper function to add content to report
function Add-ReportContent {
    param([string]$Content, [string]$Color = "White")
    $null = $report.AppendLine($Content)
    # Also write to console with color
    Write-Host $Content -ForegroundColor $Color
}

# Get all modules
$modules = Get-ChildItem -Path $Path -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue

Add-ReportContent "=== SCAFFOLDING DETECTION FOR: $Path ===" "Cyan"
Add-ReportContent ""
Add-ReportContent "Total modules found: $($modules.Count)" "Green"
Add-ReportContent ""

# Analysis functions
function Test-IsScaffolding {
    param([string]$Content, [string]$FileName)
    
    $score = 0
    $issues = @()
    
    # Check for TODO comments
    $todoMatches = [regex]::Matches($Content, 'TODO|FIXME|XXX|HACK', 'IgnoreCase')
    if ($todoMatches.Count -gt 0) {
        $score += 10
        $issues += "TODO comments: $($todoMatches.Count)"
    }
    
    # Check for empty/placeholder functions
    $functionMatches = [regex]::Matches($Content, 'function\s+\w+')
    $emptyFunctionMatches = [regex]::Matches($Content, 'function\s+\w+\s*\{[^}]*\}', 'IgnoreCase')
    if ($functionMatches.Count -gt 0 -and $emptyFunctionMatches.Count -gt 0) {
        $emptyRatio = $emptyFunctionMatches.Count / $functionMatches.Count
        if ($emptyRatio -gt 0.5) {
            $score += 15
            $issues += "Empty functions: $emptyRatio ratio"
        }
    }
    
    # Check for placeholder text patterns
    $placeholderPatterns = @(
        'placeholder',
        'stub',
        'not implemented',
        'coming soon',
        'under construction',
        'your code here',
        'insert logic here'
    )
    
    $placeholderCount = 0
    foreach ($pattern in $placeholderPatterns) {
        $matches = [regex]::Matches($Content, $pattern, 'IgnoreCase')
        $placeholderCount += $matches.Count
    }
    if ($placeholderCount -gt 0) {
        $score += 8
        $issues += "Placeholder text: $placeholderCount occurrences"
    }
    
    # Check for minimal content (very small files)
    $lineCount = ($Content -split "`n").Count
    if ($lineCount -lt 50) {
        $score += 12
        $issues += "Very small file: $lineCount lines"
    }
    
    # Check for high comment-to-code ratio
    $commentLines = [regex]::Matches($Content, '^\s*#.*', 'Multiline').Count
    $codeLines = [regex]::Matches($Content, '^\s*[^#\s]', 'Multiline').Count
    if ($codeLines -gt 0) {
        $commentRatio = $commentLines / $codeLines
        if ($commentRatio -gt 2) {
            $score += 7
            $issues += "High comment ratio: $([math]::Round($commentRatio, 1)):1"
        }
    }
    
    # Check for Write-Host only (no real functions)
    $writeHostMatches = [regex]::Matches($Content, 'Write-Host', 'IgnoreCase')
    $functionDefMatches = [regex]::Matches($Content, 'function\s+\w+', 'IgnoreCase')
    if ($writeHostMatches.Count -gt 5 -and $functionDefMatches.Count -lt 3) {
        $score += 10
        $issues += "Mostly Write-Host output with few functions"
    }
    
    # Check for repeated template patterns
    $templatePatterns = @(
        'Add-ReportContent',
        'Write-Host',
        'function Write-'
    )
    
    $templateCount = 0
    foreach ($pattern in $templatePatterns) {
        $matches = [regex]::Matches($Content, $pattern, 'IgnoreCase')
        $templateCount += $matches.Count
    }
    if ($templateCount -gt 10) {
        $score += 5
        $issues += "Template pattern repetition"
    }
    
    return @{
        Score = $score
        Issues = $issues
        IsScaffolding = $score -gt 20
        Category = if ($score -gt 30) { "Scaffolding" } elseif ($score -gt 15) { "Mixed" } else { "Real Functionality" }
    }
}

# Analyze each module
$categories = @{
    "Real Functionality" = @()
    "Mixed" = @()
    "Scaffolding" = @()
}

foreach ($module in $modules | Sort-Object FullName) {
    try {
        $content = Get-Content -Path $module.FullName -Raw -ErrorAction Stop
        $analysis = Test-IsScaffolding -Content $content -FileName $module.Name
        
        $moduleInfo = [PSCustomObject]@{
            Name = $module.Name
            FullName = $module.FullName
            SizeKB = [math]::Round($module.Length / 1KB, 1)
            LineCount = ($content -split "`n").Count
            FunctionCount = [regex]::Matches($content, 'function\s+\w+', 'IgnoreCase').Count
            Category = $analysis.Category
            Score = $analysis.Score
            Issues = $analysis.Issues -join "; "
        }
        
        $categories[$analysis.Category] += $moduleInfo
    }
    catch {
        Add-ReportContent "Error reading $($module.Name): $_" "Red"
    }
}

# Generate report
Add-ReportContent "=== SCAFFOLDING DETECTION RESULTS ===" "Cyan"
Add-ReportContent ""

# Real Functionality
Add-ReportContent "=== REAL FUNCTIONALITY MODULES ($($categories['Real Functionality'].Count)) ===" "Green"
foreach ($mod in $categories['Real Functionality'] | Sort-Object SizeKB -Descending) {
    $relPath = $mod.FullName -replace [regex]::Escape($Path), ''
    $relPath = $relPath.TrimStart('\')
    Add-ReportContent "✓ $($mod.Name)" "Green" -NoNewline
    Add-ReportContent " ($($mod.SizeKB) KB, $($mod.LineCount) lines, $($mod.FunctionCount) functions)" "White"
    if ($mod.Issues) {
        Add-ReportContent "  Issues: $($mod.Issues)" "Gray"
    }
}
Add-ReportContent ""

# Mixed
Add-ReportContent "=== MIXED MODULES ($($categories['Mixed'].Count)) ===" "Yellow"
foreach ($mod in $categories['Mixed'] | Sort-Object Score -Descending) {
    $relPath = $mod.FullName -replace [regex]::Escape($Path), ''
    $relPath = $relPath.TrimStart('\')
    Add-ReportContent "~ $($mod.Name) (Score: $($mod.Score))" "Yellow" -NoNewline
    Add-ReportContent " ($($mod.SizeKB) KB, $($mod.LineCount) lines, $($mod.FunctionCount) functions)" "White"
    Add-ReportContent "  Issues: $($mod.Issues)" "Gray"
}
Add-ReportContent ""

# Scaffolding
Add-ReportContent "=== SCAFFOLDING/STUB MODULES ($($categories['Scaffolding'].Count)) ===" "Red"
foreach ($mod in $categories['Scaffolding'] | Sort-Object Score -Descending) {
    $relPath = $mod.FullName -replace [regex]::Escape($Path), ''
    $relPath = $relPath.TrimStart('\')
    Add-ReportContent "✗ $($mod.Name) (Score: $($mod.Score))" "Red" -NoNewline
    Add-ReportContent " ($($mod.SizeKB) KB, $($mod.LineCount) lines, $($mod.FunctionCount) functions)" "White"
    Add-ReportContent "  Issues: $($mod.Issues)" "Gray"
}
Add-ReportContent ""

# Summary
Add-ReportContent "=== SUMMARY ===" "Cyan"
Add-ReportContent "Total modules analyzed: $($modules.Count)" "White"
Add-ReportContent "Real functionality: $($categories['Real Functionality'].Count)" "Green"
Add-ReportContent "Mixed (some scaffolding): $($categories['Mixed'].Count)" "Yellow"
Add-ReportContent "Scaffolding/stubs: $($categories['Scaffolding'].Count)" "Red"
Add-ReportContent ""

# Recommendations
Add-ReportContent "=== RECOMMENDATIONS ===" "Magenta"
if ($categories['Scaffolding'].Count -gt 0) {
    Add-ReportContent "• Review scaffolding modules for deletion or completion" "Yellow"
}
if ($categories['Mixed'].Count -gt 0) {
    Add-ReportContent "• Complete mixed modules by removing TODOs and placeholders" "Yellow"
}
if ($duplicates.Count -gt 0) {
    Add-ReportContent "• Consolidate duplicate modules (see duplicate analysis)" "Yellow"
}
Add-ReportContent "• Consider moving auto-generated modules to separate archive" "Cyan"

# Save to file
$reportContent = $report.ToString()
$reportContent | Set-Content -Path $OutputPath -Encoding UTF8

Write-Host ""
Write-Host "Report saved to: $OutputPath" -ForegroundColor Green
Write-Host "File size: $([math]::Round((Get-Item $OutputPath).Length / 1KB, 1)) KB" -ForegroundColor White
