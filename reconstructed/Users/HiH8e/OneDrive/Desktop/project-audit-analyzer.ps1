# Project Audit Analyzer
# Analyzes Desktop files to identify projects and assess completion/quality metrics

param(
    [string]$RootPath = "C:\Users\HiH8e\OneDrive\Desktop"
)

# Configuration
$script:AuditResults = @()
$script:ProjectGroups = @{}

# Project indicators
$projectIndicators = @{
    'package.json' = 10
    'README.md' = 5
    'build.gradle' = 10
    '.git' = 8
    'CMakeLists.txt' = 10
    'Makefile' = 8
    'requirements.txt' = 7
    'setup.py' = 7
    '.sln' = 10
    '.csproj' = 10
    'tsconfig.json' = 8
    'webpack.config.js' = 8
}

# Quality indicators
$qualityIndicators = @{
    'Documentation' = @('README.md', '*.md', 'docs/')
    'Tests' = @('*test*.js', '*test*.py', '*test*.java', 'test/', 'tests/', '__test__/')
    'Configuration' = @('*.json', '*.yaml', '*.yml', '*.config', '*.ini')
    'BuildSystem' = @('package.json', 'build.gradle', 'CMakeLists.txt', 'Makefile')
    'SourceControl' = @('.git/', '.gitignore')
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  PROJECT AUDIT ANALYZER" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

function Get-FileCount {
    param([string]$Path, [string[]]$Patterns)
    
    $count = 0
    foreach ($pattern in $Patterns) {
        if ($pattern.EndsWith('/')) {
            $dirPattern = $pattern.TrimEnd('/')
            $count += (Get-ChildItem -Path $Path -Directory -Filter $dirPattern -Recurse -ErrorAction SilentlyContinue | Measure-Object).Count
        } else {
            $count += (Get-ChildItem -Path $Path -File -Filter $pattern -Recurse -ErrorAction SilentlyContinue | Measure-Object).Count
        }
    }
    return $count
}

function Measure-ProjectCompleteness {
    param(
        [string]$ProjectPath,
        [string]$ProjectName
    )
    
    $metrics = @{
        Name = $ProjectName
        Path = $ProjectPath
        Type = "Unknown"
        CompletionScore = 0
        QualityScore = 0
        TotalFiles = 0
        SourceFiles = 0
        HasDocumentation = $false
        HasTests = $false
        HasBuildSystem = $false
        HasSourceControl = $false
        Issues = @()
        Strengths = @()
    }
    
    # Count files
    $allFiles = Get-ChildItem -Path $ProjectPath -File -Recurse -ErrorAction SilentlyContinue
    $metrics.TotalFiles = ($allFiles | Measure-Object).Count
    
    # Detect project type
    if (Test-Path (Join-Path $ProjectPath "package.json")) {
        $metrics.Type = "Node.js/JavaScript"
        $metrics.SourceFiles = ($allFiles | Where-Object { $_.Extension -in @('.js', '.ts', '.jsx', '.tsx') } | Measure-Object).Count
    }
    elseif (Test-Path (Join-Path $ProjectPath "*.sln")) {
        $metrics.Type = ".NET/C#"
        $metrics.SourceFiles = ($allFiles | Where-Object { $_.Extension -in @('.cs', '.vb', '.fs') } | Measure-Object).Count
    }
    elseif (Test-Path (Join-Path $ProjectPath "build.gradle")) {
        $metrics.Type = "Java/Gradle"
        $metrics.SourceFiles = ($allFiles | Where-Object { $_.Extension -eq '.java' } | Measure-Object).Count
    }
    elseif (Test-Path (Join-Path $ProjectPath "CMakeLists.txt")) {
        $metrics.Type = "C/C++"
        $metrics.SourceFiles = ($allFiles | Where-Object { $_.Extension -in @('.c', '.cpp', '.h', '.hpp') } | Measure-Object).Count
    }
    elseif (Test-Path (Join-Path $ProjectPath "requirements.txt")) {
        $metrics.Type = "Python"
        $metrics.SourceFiles = ($allFiles | Where-Object { $_.Extension -eq '.py' } | Measure-Object).Count
    }
    elseif (($allFiles | Where-Object { $_.Extension -eq '.html' } | Measure-Object).Count -gt 0) {
        $metrics.Type = "Web/HTML"
        $metrics.SourceFiles = ($allFiles | Where-Object { $_.Extension -in @('.html', '.css', '.js') } | Measure-Object).Count
    }
    
    # Check quality indicators
    $qualityPoints = 0
    $maxQualityPoints = 0
    
    # Documentation check
    $docCount = Get-FileCount -Path $ProjectPath -Patterns $qualityIndicators['Documentation']
    if ($docCount -gt 0) {
        $metrics.HasDocumentation = $true
        $metrics.Strengths += "Has documentation ($docCount files)"
        $qualityPoints += 20
    } else {
        $metrics.Issues += "Missing documentation"
    }
    $maxQualityPoints += 20
    
    # Tests check
    $testCount = Get-FileCount -Path $ProjectPath -Patterns $qualityIndicators['Tests']
    if ($testCount -gt 0) {
        $metrics.HasTests = $true
        $metrics.Strengths += "Has tests ($testCount files)"
        $qualityPoints += 25
    } else {
        $metrics.Issues += "No test files found"
    }
    $maxQualityPoints += 25
    
    # Build system check
    $buildCount = Get-FileCount -Path $ProjectPath -Patterns $qualityIndicators['BuildSystem']
    if ($buildCount -gt 0) {
        $metrics.HasBuildSystem = $true
        $metrics.Strengths += "Has build configuration"
        $qualityPoints += 20
    } else {
        $metrics.Issues += "No build system detected"
    }
    $maxQualityPoints += 20
    
    # Source control check
    if (Test-Path (Join-Path $ProjectPath ".git")) {
        $metrics.HasSourceControl = $true
        $metrics.Strengths += "Uses Git source control"
        $qualityPoints += 15
    } else {
        $metrics.Issues += "Not under source control"
    }
    $maxQualityPoints += 15
    
    # Check for source files
    if ($metrics.SourceFiles -gt 0) {
        $qualityPoints += 20
        $metrics.Strengths += "Contains $($metrics.SourceFiles) source files"
    } else {
        $metrics.Issues += "No source code files found"
    }
    $maxQualityPoints += 20
    
    $metrics.QualityScore = if ($maxQualityPoints -gt 0) { [math]::Round(($qualityPoints / $maxQualityPoints) * 100, 1) } else { 0 }
    
    # Calculate completion score based on multiple factors
    $completionPoints = 0
    $maxCompletionPoints = 0
    
    # Has actual code
    if ($metrics.SourceFiles -gt 5) {
        $completionPoints += 30
    } elseif ($metrics.SourceFiles -gt 0) {
        $completionPoints += 15
    }
    $maxCompletionPoints += 30
    
    # Has README
    if (Test-Path (Join-Path $ProjectPath "README.md")) {
        $completionPoints += 15
    }
    $maxCompletionPoints += 15
    
    # Has build/package config
    if ($metrics.HasBuildSystem) {
        $completionPoints += 20
    }
    $maxCompletionPoints += 20
    
    # Has multiple file types (indicates developed project)
    $uniqueExtensions = ($allFiles | Select-Object -ExpandProperty Extension -Unique | Measure-Object).Count
    if ($uniqueExtensions -gt 5) {
        $completionPoints += 15
    } elseif ($uniqueExtensions -gt 3) {
        $completionPoints += 10
    }
    $maxCompletionPoints += 15
    
    # Has reasonable file count (not just scaffolding)
    if ($metrics.TotalFiles -gt 20) {
        $completionPoints += 10
    } elseif ($metrics.TotalFiles -gt 10) {
        $completionPoints += 5
    }
    $maxCompletionPoints += 10
    
    # Has tests
    if ($metrics.HasTests) {
        $completionPoints += 10
    }
    $maxCompletionPoints += 10
    
    $metrics.CompletionScore = if ($maxCompletionPoints -gt 0) { [math]::Round(($completionPoints / $maxCompletionPoints) * 100, 1) } else { 0 }
    
    return $metrics
}

function Find-Projects {
    param([string]$Path)
    
    Write-Host "Scanning for projects in: $Path" -ForegroundColor Yellow
    Write-Host ""
    
    $items = Get-ChildItem -Path $Path -Directory -ErrorAction SilentlyContinue
    $projects = @()
    
    foreach ($item in $items) {
        # Skip system/organizational folders
        if ($item.Name -in @('.vscode', '.idea', 'node_modules', 'Screenshots', 'Documentation', 'Text-Notes', 'Duplicates-Archive')) {
            continue
        }
        
        # Check if it looks like a project
        $projectScore = 0
        
        foreach ($indicator in $projectIndicators.Keys) {
            $indicatorPath = Join-Path $item.FullName $indicator
            if (Test-Path $indicatorPath) {
                $projectScore += $projectIndicators[$indicator]
            }
        }
        
        # Also check for multiple source files
        $sourceFiles = Get-ChildItem -Path $item.FullName -File -Recurse -ErrorAction SilentlyContinue | 
            Where-Object { $_.Extension -in @('.js', '.ts', '.py', '.java', '.cpp', '.cs', '.html', '.c', '.h', '.hpp') }
        
        if (($sourceFiles | Measure-Object).Count -gt 3) {
            $projectScore += 5
        }
        
        if ($projectScore -ge 5) {
            $projects += $item
            Write-Host "  ✓ Found project: $($item.Name) (Score: $projectScore)" -ForegroundColor Green
        }
    }
    
    # Also check Mirai-Source-Code-master
    $miraiPath = Join-Path $Path "Mirai-Source-Code-master"
    if (Test-Path $miraiPath) {
        $miraiItem = Get-Item $miraiPath
        $projects += $miraiItem
        Write-Host "  ✓ Found project: Mirai-Source-Code-master (Known project)" -ForegroundColor Green
    }
    
    Write-Host ""
    return $projects
}

# Main execution
$projects = Find-Projects -Path $RootPath

if ($projects.Count -eq 0) {
    Write-Host "No projects found!" -ForegroundColor Red
    exit
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  ANALYZING $($projects.Count) PROJECT(S)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

foreach ($project in $projects) {
    Write-Host "Analyzing: $($project.Name)" -ForegroundColor Cyan
    $metrics = Measure-ProjectCompleteness -ProjectPath $project.FullName -ProjectName $project.Name
    $script:AuditResults += $metrics
    
    Write-Host "  Type: $($metrics.Type)" -ForegroundColor Gray
    Write-Host "  Files: $($metrics.TotalFiles) total, $($metrics.SourceFiles) source" -ForegroundColor Gray
    Write-Host "  Completion: $($metrics.CompletionScore)%" -ForegroundColor $(if($metrics.CompletionScore -ge 70){"Green"}elseif($metrics.CompletionScore -ge 40){"Yellow"}else{"Red"})
    Write-Host "  Quality: $($metrics.QualityScore)%" -ForegroundColor $(if($metrics.QualityScore -ge 70){"Green"}elseif($metrics.QualityScore -ge 40){"Yellow"}else{"Red"})
    Write-Host ""
}

# Generate report
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  AUDIT REPORT" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$report = @"
# PROJECT AUDIT REPORT
Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

## Summary
- Total Projects Found: $($projects.Count)
- Average Completion: $([math]::Round(($script:AuditResults | Measure-Object -Property CompletionScore -Average).Average, 1))%
- Average Quality: $([math]::Round(($script:AuditResults | Measure-Object -Property QualityScore -Average).Average, 1))%

## Project Details

"@

foreach ($result in ($script:AuditResults | Sort-Object -Property CompletionScore -Descending)) {
    $status = if ($result.CompletionScore -ge 70) { "🟢 Production Ready" } 
              elseif ($result.CompletionScore -ge 50) { "🟡 In Development" }
              elseif ($result.CompletionScore -ge 30) { "🟠 Early Stage" }
              else { "🔴 Incomplete/Abandoned" }
    
    $report += @"

### $($result.Name)
**Status:** $status  
**Type:** $($result.Type)  
**Completion:** $($result.CompletionScore)% | **Quality:** $($result.QualityScore)%  
**Files:** $($result.TotalFiles) total, $($result.SourceFiles) source files

**Strengths:**
"@
    
    if ($result.Strengths.Count -gt 0) {
        foreach ($strength in $result.Strengths) {
            $report += "`n- ✓ $strength"
        }
    } else {
        $report += "`n- None identified"
    }
    
    $report += "`n`n**Issues:**"
    if ($result.Issues.Count -gt 0) {
        foreach ($issue in $result.Issues) {
            $report += "`n- ⚠ $issue"
        }
    } else {
        $report += "`n- None identified"
    }
    
    $report += "`n"
}

# Recommendations
$report += @"

## Recommendations

### High Priority Projects (>70% completion)
"@

$highPriority = $script:AuditResults | Where-Object { $_.CompletionScore -ge 70 }
if ($highPriority.Count -gt 0) {
    foreach ($proj in $highPriority) {
        $report += "`n- **$($proj.Name)** - Focus on quality improvements and finalization"
    }
} else {
    $report += "`n- None found"
}

$report += "`n`n### Medium Priority Projects (40-70% completion)"

$medPriority = $script:AuditResults | Where-Object { $_.CompletionScore -ge 40 -and $_.CompletionScore -lt 70 }
if ($medPriority.Count -gt 0) {
    foreach ($proj in $medPriority) {
        $report += "`n- **$($proj.Name)** - Continue development to reach production readiness"
    }
} else {
    $report += "`n- None found"
}

$report += "`n`n### Low Priority Projects (<40% completion)"

$lowPriority = $script:AuditResults | Where-Object { $_.CompletionScore -lt 40 }
if ($lowPriority.Count -gt 0) {
    foreach ($proj in $lowPriority) {
        $report += "`n- **$($proj.Name)** - Consider archiving or major refactor needed"
    }
} else {
    $report += "`n- None found"
}

$report += @"


## Action Items
1. Archive projects with <30% completion to Duplicates-Archive
2. Focus development efforts on 40-70% completion projects
3. Add missing documentation to all projects
4. Implement testing for projects lacking test coverage
5. Set up proper source control for untracked projects

---
*Report generated by Project Audit Analyzer*
"@

# Save report
$reportPath = Join-Path $RootPath "PROJECT_AUDIT_REPORT.md"
$report | Out-File -FilePath $reportPath -Encoding UTF8

Write-Host "Report saved to: $reportPath" -ForegroundColor Green
Write-Host ""

# Display summary table
Write-Host "PROJECT SUMMARY TABLE" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host ""
Write-Host ("{0,-40} {1,-15} {2,12} {3,12}" -f "Project", "Type", "Completion", "Quality") -ForegroundColor Yellow

foreach ($result in ($script:AuditResults | Sort-Object -Property CompletionScore -Descending)) {
    $color = if ($result.CompletionScore -ge 70) { "Green" } 
             elseif ($result.CompletionScore -ge 40) { "Yellow" } 
             else { "Red" }
    
    Write-Host ("{0,-40} {1,-15} {2,11}% {3,11}%" -f 
        $result.Name.Substring(0, [Math]::Min(39, $result.Name.Length)),
        $result.Type.Substring(0, [Math]::Min(14, $result.Type.Length)),
        $result.CompletionScore,
        $result.QualityScore) -ForegroundColor $color
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Audit Complete! Check PROJECT_AUDIT_REPORT.md for details" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
