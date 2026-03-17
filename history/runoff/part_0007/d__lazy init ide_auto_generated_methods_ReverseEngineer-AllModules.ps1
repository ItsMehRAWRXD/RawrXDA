# RawrXD Complete Module Reverse Engineering Analysis
# Comprehensive analysis and fine-tuning of all modules

#Requires -Version 5.1

param(
    [Parameter(Mandatory=$false)]
    [string]$ModulePath = $PSScriptRoot,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateFineTuningReport = $true,
    
    [Parameter(Mandatory=$false)]
    [switch]$ApplyOptimizations = $false
)

Write-Host "=== RawrXD Complete Module Reverse Engineering Analysis ===" -ForegroundColor Cyan
Write-Host "Module Path: $ModulePath" -ForegroundColor Yellow
Write-Host ""

# Get all RawrXD modules
$modules = Get-ChildItem -Path $ModulePath -Filter "RawrXD*.psm1" | Sort-Object Name

Write-Host "Found $($modules.Count) modules to analyze" -ForegroundColor Green
Write-Host ""

# Initialize analysis results
$analysisResults = @{
    Modules = @()
    TotalLines = 0
    TotalFunctions = 0
    TotalClasses = 0
    TotalExports = 0
    TotalSizeKB = 0
    Issues = @()
    Optimizations = @()
    Recommendations = @()
}

# Analyze each module
foreach ($module in $modules) {
    Write-Host "Analyzing: $($module.Name)" -ForegroundColor Yellow
    
    $content = Get-Content -Path $module.FullName -Raw
    $lines = $content -split "`n"
    
    # Extract functions
    $functionMatches = [regex]::Matches($content, 'function\s+(\w+)')
    $functions = @()
    foreach ($match in $functionMatches) {
        $functions += $match.Groups[1].Value
    }
    
    # Extract classes
    $classMatches = [regex]::Matches($content, 'class\s+(\w+)')
    $classes = @()
    foreach ($match in $classMatches) {
        $classes += $match.Groups[1].Value
    }
    
    # Extract exports
    $exportMatches = [regex]::Matches($content, 'Export-ModuleMember\s+-Function\s+(.+)')
    $exports = @()
    if ($exportMatches.Count -gt 0) {
        $exportLine = $exportMatches[0].Groups[1].Value
        $exportList = $exportLine -split ',' | ForEach-Object { $_.Trim() }
        $exports = $exportList
    }
    
    # Analyze for issues
    $issues = @()
    
    # Check for missing error handling
    if ($content -notlike '*try*' -or $content -notlike '*catch*') {
        $issues += "Missing comprehensive error handling"
    }
    
    # Check for missing logging
    if ($content -notlike '*Write-StructuredLog*') {
        $issues += "Missing structured logging"
    }
    
    # Check for missing documentation
    $undocumentedFunctions = @()
    foreach ($func in $functions) {
        $funcPattern = "function\s+$func\s*{[^}]*}"
        $funcMatch = [regex]::Match($content, $funcPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
        if ($funcMatch.Success) {
            $funcBody = $funcMatch.Value
            if ($funcBody -notlike '*<#*#>*') {
                $undocumentedFunctions += $func
            }
        }
    }
    if ($undocumentedFunctions.Count -gt 0) {
        $issues += "Missing documentation for functions: $($undocumentedFunctions -join ', ')"
    }
    
    # Check for unapproved verbs
    $unapprovedVerbs = @()
    $approvedVerbs = (Get-Verb).Verb
    foreach ($func in $functions) {
        $verb = $func -split '-' | Select-Object -First 1
        if ($verb -notin $approvedVerbs) {
            $unapprovedVerbs += $func
        }
    }
    if ($unapprovedVerbs.Count -gt 0) {
        $issues += "Unapproved verbs in functions: $($unapprovedVerbs -join ', ')"
    }
    
    # Calculate metrics
    $sizeKB = [Math]::Round($module.Length / 1KB, 2)
    
    $moduleAnalysis = @{
        Name = $module.Name
        Lines = $lines.Count
        Functions = $functions.Count
        Classes = $classes.Count
        Exports = $exports.Count
        SizeKB = $sizeKB
        FunctionNames = $functions
        ClassNames = $classes
        ExportNames = $exports
        Issues = $issues
        Path = $module.FullName
    }
    
    $analysisResults.Modules += $moduleAnalysis
    $analysisResults.TotalLines += $lines.Count
    $analysisResults.TotalFunctions += $functions.Count
    $analysisResults.TotalClasses += $classes.Count
    $analysisResults.TotalExports += $exports.Count
    $analysisResults.TotalSizeKB += $sizeKB
    $analysisResults.Issues += $issues
    
    Write-Host "  ✓ Lines: $($lines.Count), Functions: $($functions.Count), Classes: $($classes.Count), Exports: $($exports.Count), Size: ${sizeKB}KB" -ForegroundColor Green
    
    if ($issues.Count -gt 0) {
        Write-Host "  ⚠ Issues found: $($issues.Count)" -ForegroundColor Yellow
        foreach ($issue in $issues) {
            Write-Host "    - $issue" -ForegroundColor Gray
        }
    }
    
    Write-Host ""
}

# Generate summary
Write-Host "=== Analysis Summary ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "Overall Statistics:" -ForegroundColor Yellow
Write-Host "  Total Modules: $($analysisResults.Modules.Count)" -ForegroundColor White
Write-Host "  Total Lines: $($analysisResults.TotalLines)" -ForegroundColor White
Write-Host "  Total Functions: $($analysisResults.TotalFunctions)" -ForegroundColor White
Write-Host "  Total Classes: $($analysisResults.TotalClasses)" -ForegroundColor White
Write-Host "  Total Exports: $($analysisResults.TotalExports)" -ForegroundColor White
Write-Host "  Total Size: $($analysisResults.TotalSizeKB) KB" -ForegroundColor White
Write-Host "  Total Issues: $($analysisResults.Issues.Count)" -ForegroundColor $(if ($analysisResults.Issues.Count -eq 0) { "Green" } else { "Yellow" })
Write-Host ""

# Generate fine-tuning recommendations
if ($GenerateFineTuningReport) {
    Write-Host "=== Fine-Tuning Recommendations ===" -ForegroundColor Cyan
    Write-Host ""
    
    $recommendations = @()
    
    # Module size recommendations
    $largeModules = $analysisResults.Modules | Where-Object { $_.SizeKB -gt 100 }
    if ($largeModules.Count -gt 0) {
        Write-Host "Large Modules ( > 100KB ):" -ForegroundColor Yellow
        foreach ($module in $largeModules) {
            Write-Host "  - $($module.Name): $($module.SizeKB)KB" -ForegroundColor White
            $recommendations += "Consider splitting $($module.Name) into smaller modules"
        }
        Write-Host ""
    }
    
    # Function count recommendations
    $denseModules = $analysisResults.Modules | Where-Object { $_.Functions -gt 50 }
    if ($denseModules.Count -gt 0) {
        Write-Host "High Function Count ( > 50 ):" -ForegroundColor Yellow
        foreach ($module in $denseModules) {
            Write-Host "  - $($module.Name): $($module.Functions) functions" -ForegroundColor White
            $recommendations += "Consider refactoring $($module.Name) to reduce function count"
        }
        Write-Host ""
    }
    
    # Export ratio recommendations
    foreach ($module in $analysisResults.Modules) {
        if ($module.Functions -gt 0) {
            $exportRatio = $module.Exports / $module.Functions
            if ($exportRatio -lt 0.5) {
                Write-Host "Low Export Ratio in $($module.Name): $($exportRatio.ToString('P'))" -ForegroundColor Yellow
                $recommendations += "Consider exporting more functions from $($module.Name) (current: $($module.Exports)/$($module.Functions))"
            }
        }
    }
    
    # Documentation recommendations
    $modulesWithDocIssues = $analysisResults.Modules | Where-Object { $_.Issues -like '*documentation*' }
    if ($modulesWithDocIssues.Count -gt 0) {
        Write-Host "Documentation Issues:" -ForegroundColor Yellow
        foreach ($module in $modulesWithDocIssues) {
            $docIssues = $module.Issues | Where-Object { $_ -like '*documentation*' }
            Write-Host "  - $($module.Name): $($docIssues.Count) issues" -ForegroundColor White
            $recommendations += "Add documentation to $($module.Name)"
        }
        Write-Host ""
    }
    
    # Error handling recommendations
    $modulesWithErrorIssues = $analysisResults.Modules | Where-Object { $_.Issues -like '*error*' }
    if ($modulesWithErrorIssues.Count -gt 0) {
        Write-Host "Error Handling Issues:" -ForegroundColor Yellow
        foreach ($module in $modulesWithErrorIssues) {
            Write-Host "  - $($module.Name): Missing error handling" -ForegroundColor White
            $recommendations += "Add comprehensive error handling to $($module.Name)"
        }
        Write-Host ""
    }
    
    # Logging recommendations
    $modulesWithLoggingIssues = $analysisResults.Modules | Where-Object { $_.Issues -like '*logging*' }
    if ($modulesWithLoggingIssues.Count -gt 0) {
        Write-Host "Logging Issues:" -ForegroundColor Yellow
        foreach ($module in $modulesWithLoggingIssues) {
            Write-Host "  - $($module.Name): Missing structured logging" -ForegroundColor White
            $recommendations += "Add structured logging to $($module.Name)"
        }
        Write-Host ""
    }
    
    # Performance recommendations
    Write-Host "Performance Optimizations:" -ForegroundColor Yellow
    Write-Host "  • Consider implementing lazy loading for large modules" -ForegroundColor White
    Write-Host "  • Add caching for frequently accessed functions" -ForegroundColor White
    Write-Host "  • Implement parallel processing where applicable" -ForegroundColor White
    Write-Host "  • Optimize regex patterns for better performance" -ForegroundColor White
    Write-Host ""
    
    $recommendations += "Implement lazy loading for modules over 100KB"
    $recommendations += "Add caching layer for frequently used functions"
    $recommendations += "Optimize regex patterns in all modules"
    
    # Security recommendations
    Write-Host "Security Enhancements:" -ForegroundColor Yellow
    Write-Host "  • Add input validation to all public functions" -ForegroundColor White
    Write-Host "  • Implement secure string handling for sensitive data" -ForegroundColor White
    Write-Host "  • Add authentication/authorization where needed" -ForegroundColor White
    Write-Host "  • Implement audit logging for critical operations" -ForegroundColor White
    Write-Host ""
    
    $recommendations += "Add input validation to all public functions"
    $recommendations += "Implement secure string handling"
    $recommendations += "Add audit logging for critical operations"
    
    # Architecture recommendations
    Write-Host "Architecture Improvements:" -ForegroundColor Yellow
    Write-Host "  • Implement dependency injection for better testability" -ForegroundColor White
    Write-Host "  • Add interface definitions for key components" -ForegroundColor White
    Write-Host "  • Implement plugin architecture for extensibility" -ForegroundColor White
    Write-Host "  • Add configuration management system" -ForegroundColor White
    Write-Host ""
    
    $recommendations += "Implement dependency injection pattern"
    $recommendations += "Add interface definitions for key components"
    $recommendations += "Implement plugin architecture"
    $recommendations += "Add configuration management system"
    
    $analysisResults.Recommendations = $recommendations
}

# Generate optimization report
Write-Host "=== Optimization Report ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "Priority 1 - Critical:" -ForegroundColor Red
Write-Host "  1. Add comprehensive error handling to all modules" -ForegroundColor White
Write-Host "  2. Implement structured logging across all modules" -ForegroundColor White
Write-Host "  3. Add documentation to all public functions" -ForegroundColor White
Write-Host ""

Write-Host "Priority 2 - High:" -ForegroundColor Yellow
Write-Host "  4. Optimize large modules (>100KB)" -ForegroundColor White
Write-Host "  5. Add input validation to all public functions" -ForegroundColor White
Write-Host "  6. Implement caching for frequently used functions" -ForegroundColor White
Write-Host ""

Write-Host "Priority 3 - Medium:" -ForegroundColor Green
Write-Host "  7. Refactor modules with high function count (>50)" -ForegroundColor White
Write-Host "  8. Implement lazy loading for large modules" -ForegroundColor White
Write-Host "  9. Add audit logging for critical operations" -ForegroundColor White
Write-Host ""

Write-Host "Priority 4 - Low:" -ForegroundColor Cyan
Write-Host "  10. Implement dependency injection pattern" -ForegroundColor White
Write-Host "  11. Add interface definitions" -ForegroundColor White
Write-Host "  12. Implement plugin architecture" -ForegroundColor White
Write-Host ""

# Save analysis results
$analysisResults | Export-Clixml -Path (Join-Path $ModulePath "ModuleAnalysisResults.xml") -Force

Write-Host "Analysis complete! Results saved to: ModuleAnalysisResults.xml" -ForegroundColor Green
Write-Host ""

# Show detailed module breakdown if requested
Write-Host "=== Detailed Module Breakdown ===" -ForegroundColor Cyan
Write-Host ""

$analysisResults.Modules | Sort-Object Name | ForEach-Object {
    Write-Host "Module: $($_.Name)" -ForegroundColor Yellow
    Write-Host "  Path: $($_.Path)" -ForegroundColor Gray
    Write-Host "  Statistics: $($_.Lines) lines, $($_.Functions) functions, $($_.Classes) classes, $($_.Exports) exports, $($_.SizeKB)KB" -ForegroundColor White
    
    if ($_.FunctionNames.Count -gt 0) {
        Write-Host "  Functions: $($_.FunctionNames -join ', ')" -ForegroundColor Gray
    }
    
    if ($_.ClassNames.Count -gt 0) {
        Write-Host "  Classes: $($_.ClassNames -join ', ')" -ForegroundColor Gray
    }
    
    if ($_.ExportNames.Count -gt 0) {
        Write-Host "  Exports: $($_.ExportNames -join ', ')" -ForegroundColor Gray
    }
    
    if ($_.Issues.Count -gt 0) {
        Write-Host "  Issues:" -ForegroundColor Red
        $_.Issues | ForEach-Object {
            Write-Host "    - $_" -ForegroundColor Gray
        }
    }
    
    Write-Host ""
}

Write-Host "=== Reverse Engineering Analysis Complete ===" -ForegroundColor Cyan
Write-Host ""

# Apply optimizations if requested
if ($ApplyOptimizations) {
    Write-Host "Applying optimizations..." -ForegroundColor Yellow
    Write-Host "⚠ This feature is not yet implemented. Use the recommendations above to manually optimize modules." -ForegroundColor Red
}