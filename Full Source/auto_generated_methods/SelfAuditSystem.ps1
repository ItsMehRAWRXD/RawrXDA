#Requires -Version 7.4

<#
.SYNOPSIS
    Self-Auditing System for RawrXD OMEGA-1 Modules
.DESCRIPTION
    Comprehensive audit system that scans all created modules, checks for completeness,
    identifies gaps, and generates detailed reports with prioritized todos.
#>

function Start-ComprehensiveAudit {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModuleDirectory,
        
        [Parameter(Mandatory=$false)]
        [string]$AuditReportPath = "$env:TEMP\RawrXD_Audit_Report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json",
        
        [Parameter(Mandatory=$false)]
        [string]$TodoListPath = "$env:TEMP\RawrXD_Todo_List_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
    )

    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        RawrXD OMEGA-1 Self-Auditing System                     ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""

    # Phase 1: Discover all modules
    Write-Host "📂 Phase 1: Discovering modules..." -ForegroundColor Yellow
    $modules = Discover-AllModules -Directory $ModuleDirectory
    Write-Host "✓ Found $($modules.Count) modules" -ForegroundColor Green
    Write-Host ""

    # Phase 2: Audit each module
    Write-Host "🔍 Phase 2: Auditing modules..." -ForegroundColor Yellow
    $auditResults = @()
    foreach ($module in $modules) {
        Write-Host "  Auditing: $($module.Name)" -ForegroundColor Gray
        $result = Audit-SingleModule -ModulePath $module.FullPath
        $auditResults += $result
    }
    Write-Host "✓ Completed auditing $($auditResults.Count) modules" -ForegroundColor Green
    Write-Host ""

    # Phase 3: Generate comprehensive report
    Write-Host "📊 Phase 3: Generating audit report..." -ForegroundColor Yellow
    $auditReport = Generate-AuditReport -AuditResults $auditResults -ModuleDirectory $ModuleDirectory
    $auditReport | ConvertTo-Json -Depth 10 | Set-Content -Path $AuditReportPath -Encoding UTF8
    Write-Host "✓ Audit report saved to: $AuditReportPath" -ForegroundColor Green
    Write-Host ""

    # Phase 4: Generate prioritized todo list
    Write-Host "📝 Phase 4: Generating prioritized todo list..." -ForegroundColor Yellow
    $todoList = Generate-PrioritizedTodos -AuditResults $auditResults
    $todoList | ConvertTo-Json -Depth 10 | Set-Content -Path $TodoListPath -Encoding UTF8
    Write-Host "✓ Todo list saved to: $TodoListPath" -ForegroundColor Green
    Write-Host ""

    # Phase 5: Display summary
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                      Audit Summary                             ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    Display-AuditSummary -AuditReport $auditReport -TodoList $todoList
    
    return [PSCustomObject]@{
        AuditReportPath = $AuditReportPath
        TodoListPath = $TodoListPath
        ModulesAudited = $modules.Count
        TotalIssues = ($auditResults | Measure-Object -Property TotalIssues -Sum).Sum
        CriticalIssues = ($auditResults | Measure-Object -Property CriticalIssues -Sum).Sum
        HighPriorityTodos = ($todoList | Where-Object { $_.Priority -eq 'Critical' }).Count
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
}

function Discover-AllModules {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Directory
    )

    $modules = @()
    
    # Discover PowerShell modules
    $psModules = Get-ChildItem -Path $Directory -Filter "*.psm1" -Recurse
    foreach ($module in $psModules) {
        $modules += [PSCustomObject]@{
            Name = $module.BaseName
            FullPath = $module.FullName
            Type = "PowerShellModule"
            Size = $module.Length
            LastModified = $module.LastWriteTime
        }
    }
    
    # Discover PowerShell scripts
    $psScripts = Get-ChildItem -Path $Directory -Filter "*.ps1" -Recurse
    foreach ($script in $psScripts) {
        $modules += [PSCustomObject]@{
            Name = $script.BaseName
            FullPath = $script.FullName
            Type = "PowerShellScript"
            Size = $script.Length
            LastModified = $script.LastWriteTime
        }
    }
    
    # Discover JSON configuration files
    $jsonFiles = Get-ChildItem -Path $Directory -Filter "*.json" -Recurse
    foreach ($json in $jsonFiles) {
        $modules += [PSCustomObject]@{
            Name = $json.BaseName
            FullPath = $json.FullName
            Type = "ConfigurationFile"
            Size = $json.Length
            LastModified = $json.LastWriteTime
        }
    }
    
    return $modules
}

function Audit-SingleModule {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModulePath
    )

    $content = Get-Content -Path $ModulePath -Raw -ErrorAction SilentlyContinue
    $issues = @()
    $warnings = @()
    $missingFeatures = @()
    
    if (-not $content) {
        return [PSCustomObject]@{
            ModuleName = Split-Path $ModulePath -Leaf
            ModulePath = $ModulePath
            Status = "Unreadable"
            TotalIssues = 1
            CriticalIssues = 1
            Warnings = 0
            MissingFeatures = @("File is empty or unreadable")
            Score = 0
            Recommendations = @("Check file permissions and encoding")
        }
    }
    
    # Check 1: Documentation completeness
    if ($content -notmatch '<#.*SYNOPSIS.*#>') {
        $issues += "Missing SYNOPSIS documentation"
        $missingFeatures += "Complete SYNOPSIS block"
    }
    
    if ($content -notmatch '<#.*DESCRIPTION.*#>') {
        $issues += "Missing DESCRIPTION documentation"
        $missingFeatures += "Complete DESCRIPTION block"
    }
    
    if ($content -notmatch '<#.*PARAMETER.*#>') {
        $issues += "Missing PARAMETER documentation"
        $missingFeatures += "Complete PARAMETER blocks"
    }
    
    # Check 2: Error handling
    if ($content -notmatch 'try\s*\{.*\}\s*catch') {
        $issues += "Missing comprehensive error handling"
        $missingFeatures += "Try-Catch blocks in all functions"
    }
    
    if ($content -notmatch '\$ErrorActionPreference') {
        $warnings += "No explicit ErrorActionPreference setting"
    }
    
    # Check 3: Logging and telemetry
    if ($content -notmatch 'Write-RawrXDLog|Write-Verbose|Write-Debug') {
        $issues += "Missing structured logging"
        $missingFeatures += "Write-RawrXDLog calls for all operations"
    }
    
    # Check 4: Function structure
    if ($content -match 'function\s+\w+') {
        $functionMatches = [regex]::Matches($content, 'function\s+(\w+)')
        $functionCount = $functionMatches.Count
        
        if ($functionCount -eq 0) {
            $issues += "No functions found in module"
            $missingFeatures += "At least 3-5 well-structured functions"
        } elseif ($functionCount -lt 3) {
            $warnings += "Only $functionCount functions found (recommended: 3-5)"
            $missingFeatures += "Additional functions for better modularity"
        }
        
        # Check for [CmdletBinding()] in functions
        foreach ($match in $functionMatches) {
            $functionName = $match.Groups[1].Value
            $functionStart = $match.Index
            $nextFunctionMatch = $functionMatches | Where-Object { $_.Index -gt $functionStart } | Select-Object -First 1
            $functionEnd = if ($nextFunctionMatch) { $nextFunctionMatch.Index } else { $content.Length }
            $functionContent = $content.Substring($functionStart, $functionEnd - $functionStart)
            
            if ($functionContent -notmatch '\[CmdletBinding\(\)\]') {
                $issues += "Function '$functionName' missing [CmdletBinding()]"
            }
        }
    }
    
    # Check 5: Export statements
    if ($content -match '\.psm1$' -and $content -notmatch 'Export-ModuleMember') {
        $issues += "Missing Export-ModuleMember statements"
        $missingFeatures += "Explicit Export-ModuleMember for all public functions"
    }
    
    # Check 6: Input validation
    if ($content -notmatch 'param\s*\(') {
        $warnings += "No parameter blocks found"
    } else {
        if ($content -notmatch '\[Parameter\(.*Mandatory\s*=\s*\$true') {
            $warnings += "No mandatory parameters defined"
        }
        
        if ($content -notmatch '\[ValidateSet\(|\[ValidatePattern\(|\[ValidateRange\(') {
            $warnings += "No parameter validation attributes found"
        }
    }
    
    # Check 7: Code quality indicators
    $lines = $content -split '\r?\n'
    $codeLines = $lines | Where-Object { $_ -match '\S' -and $_ -notmatch '^\s*#' }
    $commentLines = $lines | Where-Object { $_ -match '^\s*#' }
    
    $commentRatio = if ($codeLines.Count -gt 0) { $commentLines.Count / $codeLines.Count } else { 0 }
    if ($commentRatio -lt 0.1) {
        $warnings += "Low comment-to-code ratio: $($commentRatio.ToString('P'))"
    }
    
    # Check 8: Advanced features
    if ($content -notmatch 'class\s+\w+') {
        $missingFeatures += "PowerShell classes for object-oriented design"
    }
    
    if ($content -notmatch 'enum\s+\w+') {
        $missingFeatures += "Enums for type-safe constants"
    }
    
    if ($content -notmatch 'interface\s+\w+') {
        $missingFeatures += "Interfaces for contract-based design"
    }
    
    # Calculate score
    $maxScore = 100
    $issuePenalty = $issues.Count * 10
    $warningPenalty = $warnings.Count * 2
    $score = [Math]::Max(0, $maxScore - $issuePenalty - $warningPenalty)
    
    # Determine status
    $status = if ($score -ge 80) { "Excellent" }
              elseif ($score -ge 60) { "Good" }
              elseif ($score -ge 40) { "Fair" }
              elseif ($score -ge 20) { "Poor" }
              else { "Critical" }
    
    return [PSCustomObject]@{
        ModuleName = Split-Path $ModulePath -Leaf
        ModulePath = $ModulePath
        Status = $status
        Score = $score
        TotalIssues = $issues.Count
        CriticalIssues = ($issues | Where-Object { $_ -match "Missing|No functions" }).Count
        Warnings = $warnings.Count
        Issues = $issues
        WarningsList = $warnings
        MissingFeatures = $missingFeatures
        FunctionCount = if ($content -match 'function\s+\w+') { ([regex]::Matches($content, 'function\s+\w+')).Count } else { 0 }
        CodeLines = $codeLines.Count
        CommentRatio = $commentRatio.ToString("P")
        LastModified = (Get-Item $ModulePath).LastWriteTime
        Recommendations = Generate-Recommendations -Issues $issues -Warnings $warnings -MissingFeatures $missingFeatures
    }
}

function Generate-Recommendations {
    param(
        [array]$Issues,
        [array]$Warnings,
        [array]$MissingFeatures
    )
    
    $recommendations = @()
    
    # Critical recommendations
    if ($Issues -contains "Missing SYNOPSIS documentation") {
        $recommendations += "PRIORITY 1: Add complete SYNOPSIS block with clear purpose and usage"
    }
    
    if ($Issues -contains "Missing comprehensive error handling") {
        $recommendations += "PRIORITY 1: Implement try-catch blocks in all functions with proper error messages"
    }
    
    if ($Issues -contains "Missing structured logging") {
        $recommendations += "PRIORITY 1: Add Write-RawrXDLog calls for all operations, including success, warning, and error states"
    }
    
    # High priority recommendations
    if ($Issues -contains "Function '$functionName' missing [CmdletBinding()]") {
        $recommendations += "PRIORITY 2: Add [CmdletBinding()] to all functions for advanced parameter binding"
    }
    
    if ($Warnings -contains "No parameter validation attributes found") {
        $recommendations += "PRIORITY 2: Add ValidateSet, ValidatePattern, or ValidateRange to parameters"
    }
    
    if ($MissingFeatures -contains "PowerShell classes for object-oriented design") {
        $recommendations += "PRIORITY 2: Implement PowerShell classes for complex data structures"
    }
    
    # Medium priority recommendations
    if ($Warnings -contains "Low comment-to-code ratio") {
        $recommendations += "PRIORITY 3: Increase inline comments to at least 15% of code lines"
    }
    
    if ($MissingFeatures -contains "Enums for type-safe constants") {
        $recommendations += "PRIORITY 3: Add enums for magic numbers and string constants"
    }
    
    # Low priority recommendations
    if ($Warnings -contains "No mandatory parameters defined") {
        $recommendations += "PRIORITY 4: Review parameters and mark critical ones as Mandatory=$true"
    }
    
    return $recommendations
}

function Generate-AuditReport {
    param(
        [array]$AuditResults,
        [string]$ModuleDirectory
    )
    
    $totalModules = $AuditResults.Count
    $criticalModules = ($AuditResults | Where-Object { $_.Status -eq "Critical" }).Count
    $poorModules = ($AuditResults | Where-Object { $_.Status -eq "Poor" }).Count
    $fairModules = ($AuditResults | Where-Object { $_.Status -eq "Fair" }).Count
    $goodModules = ($AuditResults | Where-Object { $_.Status -eq "Good" }).Count
    $excellentModules = ($AuditResults | Where-Object { $_.Status -eq "Excellent" }).Count
    
    $totalIssues = ($AuditResults | Measure-Object -Property TotalIssues -Sum).Sum
    $totalWarnings = ($AuditResults | Measure-Object -Property Warnings -Sum).Sum
    $totalFunctions = ($AuditResults | Measure-Object -Property FunctionCount -Sum).Sum
    
    $averageScore = if ($totalModules -gt 0) { 
        ($AuditResults | Measure-Object -Property Score -Average).Average 
    } else { 0 }
    
    return [PSCustomObject]@{
        AuditTimestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        ModuleDirectory = $ModuleDirectory
        TotalModules = $totalModules
        StatusDistribution = [PSCustomObject]@{
            Critical = $criticalModules
            Poor = $poorModules
            Fair = $fairModules
            Good = $goodModules
            Excellent = $excellentModules
        }
        QualityMetrics = [PSCustomObject]@{
            AverageScore = [Math]::Round($averageScore, 2)
            TotalIssues = $totalIssues
            TotalWarnings = $totalWarnings
            TotalFunctions = $totalFunctions
            IssuesPerModule = if ($totalModules -gt 0) { [Math]::Round($totalIssues / $totalModules, 2) } else { 0 }
        }
        Modules = $AuditResults
        Summary = [PSCustomObject]@{
            OverallStatus = if ($criticalModules -gt 0) { "CRITICAL" }
                           elseif ($poorModules -gt 0) { "NEEDS_IMPROVEMENT" }
                           elseif ($fairModules -gt 0) { "ACCEPTABLE" }
                           elseif ($goodModules -gt 0) { "GOOD" }
                           else { "EXCELLENT" }
            PriorityAreas = @(
                if ($criticalModules -gt 0) { "Critical modules require immediate attention" }
                if ($totalIssues -gt $totalModules * 2) { "High issue density across modules" }
                if ($totalFunctions -lt $totalModules * 3) { "Insufficient function coverage" }
            )
        }
    }
}

function Generate-PrioritizedTodos {
    param(
        [array]$AuditResults
    )
    
    $todos = @()
    $todoId = 1
    
    foreach ($result in $AuditResults) {
        $moduleName = $result.ModuleName
        
        # Critical todos
        foreach ($issue in $result.Issues) {
            if ($issue -match "Missing SYNOPSIS|Missing comprehensive error handling|Missing structured logging") {
                $todos += [PSCustomObject]@{
                    Id = $todoId++
                    Module = $moduleName
                    Priority = "Critical"
                    Category = "Documentation/Error Handling"
                    Task = $issue
                    EstimatedEffort = "2-4 hours"
                    Dependencies = @()
                    Status = "Not Started"
                }
            }
        }
        
        # High priority todos
        if ($result.Score -lt 40) {
            $todos += [PSCustomObject]@{
                Id = $todoId++
                Module = $moduleName
                Priority = "High"
                Category = "Overall Quality"
                Task = "Comprehensive refactoring required - Score: $($result.Score)/100"
                EstimatedEffort = "8-16 hours"
                Dependencies = @()
                Status = "Not Started"
            }
        }
        
        # Medium priority todos
        foreach ($warning in $result.WarningsList) {
            $todos += [PSCustomObject]@{
                Id = $todoId++
                Module = $moduleName
                Priority = "Medium"
                Category = "Code Quality"
                Task = $warning
                EstimatedEffort = "1-2 hours"
                Dependencies = @()
                Status = "Not Started"
            }
        }
        
        # Low priority todos (missing features)
        foreach ($feature in $result.MissingFeatures) {
            $todos += [PSCustomObject]@{
                Id = $todoId++
                Module = $moduleName
                Priority = "Low"
                Category = "Enhancement"
                Task = "Add missing feature: $feature"
                EstimatedEffort = "4-8 hours"
                Dependencies = @()
                Status = "Not Started"
            }
        }
    }
    
    # Sort by priority
    $priorityOrder = @{ "Critical" = 1; "High" = 2; "Medium" = 3; "Low" = 4 }
    $todos = $todos | Sort-Object { $priorityOrder[$_.Priority] }
    
    return $todos
}

function Display-AuditSummary {
    param(
        [PSCustomObject]$AuditReport,
        [array]$TodoList
    )
    
    # Overall status
    $statusColor = switch ($AuditReport.Summary.OverallStatus) {
        "CRITICAL" { "Red" }
        "NEEDS_IMPROVEMENT" { "Yellow" }
        "ACCEPTABLE" { "Cyan" }
        "GOOD" { "Green" }
        "EXCELLENT" { "Magenta" }
    }
    
    Write-Host "Overall System Status: " -NoNewline
    Write-Host $AuditReport.Summary.OverallStatus -ForegroundColor $statusColor
    Write-Host ""
    
    # Module distribution
    Write-Host "Module Quality Distribution:" -ForegroundColor Cyan
    Write-Host "  🟢 Excellent: $($AuditReport.StatusDistribution.Excellent) modules" -ForegroundColor Green
    Write-Host "  🔵 Good: $($AuditReport.StatusDistribution.Good) modules" -ForegroundColor Cyan
    Write-Host "  🟡 Fair: $($AuditReport.StatusDistribution.Fair) modules" -ForegroundColor Yellow
    Write-Host "  🟠 Poor: $($AuditReport.StatusDistribution.Poor) modules" -ForegroundColor Red
    Write-Host "  🔴 Critical: $($AuditReport.StatusDistribution.Critical) modules" -ForegroundColor Red
    Write-Host ""
    
    # Quality metrics
    Write-Host "Quality Metrics:" -ForegroundColor Cyan
    Write-Host "  📊 Average Score: $($AuditReport.QualityMetrics.AverageScore)/100" -ForegroundColor White
    Write-Host "  ❌ Total Issues: $($AuditReport.QualityMetrics.TotalIssues)" -ForegroundColor Red
    Write-Host "  ⚠️  Total Warnings: $($AuditReport.QualityMetrics.TotalWarnings)" -ForegroundColor Yellow
    Write-Host "  📈 Total Functions: $($AuditReport.QualityMetrics.TotalFunctions)" -ForegroundColor Green
    Write-Host "  📉 Issues per Module: $($AuditReport.QualityMetrics.IssuesPerModule)" -ForegroundColor White
    Write-Host ""
    
    # Priority areas
    if ($AuditReport.Summary.PriorityAreas.Count -gt 0) {
        Write-Host "Priority Areas:" -ForegroundColor Cyan
        foreach ($area in $AuditReport.Summary.PriorityAreas) {
            Write-Host "  🎯 $area" -ForegroundColor Red
        }
        Write-Host ""
    }
    
    # Todo summary
    $criticalTodos = ($TodoList | Where-Object { $_.Priority -eq "Critical" }).Count
    $highTodos = ($TodoList | Where-Object { $_.Priority -eq "High" }).Count
    $mediumTodos = ($TodoList | Where-Object { $_.Priority -eq "Medium" }).Count
    $lowTodos = ($TodoList | Where-Object { $_.Priority -eq "Low" }).Count
    
    Write-Host "Generated Todo List:" -ForegroundColor Cyan
    Write-Host "  🔴 Critical: $criticalTodos tasks" -ForegroundColor Red
    Write-Host "  🟠 High: $highTodos tasks" -ForegroundColor Red
    Write-Host "  🟡 Medium: $mediumTodos tasks" -ForegroundColor Yellow
    Write-Host "  🟢 Low: $lowTodos tasks" -ForegroundColor Green
    Write-Host "  📋 Total: $($TodoList.Count) tasks" -ForegroundColor Cyan
    Write-Host ""
    
    # Top 5 critical todos
    $topCritical = $TodoList | Where-Object { $_.Priority -eq "Critical" } | Select-Object -First 5
    if ($topCritical) {
        Write-Host "Top 5 Critical Tasks:" -ForegroundColor Cyan
        foreach ($todo in $topCritical) {
            Write-Host "  🔴 [$($todo.Id)] $($todo.Module) - $($todo.Task)" -ForegroundColor Red
        }
        Write-Host ""
    }
}

# Example usage
if ($MyInvocation.InvocationName -ne '.') {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        RawrXD OMEGA-1 Self-Auditing System                     ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    $moduleDir = Read-Host "Enter the directory containing RawrXD modules"
    
    if (-not (Test-Path $moduleDir)) {
        Write-Host "Directory not found: $moduleDir" -ForegroundColor Red
        exit 1
    }
    
    $results = Start-ComprehensiveAudit -ModuleDirectory $moduleDir
    
    Write-Host ""
    Write-Host "✨ Audit completed successfully!" -ForegroundColor Green
    Write-Host "📄 Audit report: $($results.AuditReportPath)" -ForegroundColor Cyan
    Write-Host "📝 Todo list: $($results.TodoListPath)" -ForegroundColor Cyan
    Write-Host ""
    
    $viewReport = Read-Host "View detailed audit report? (Y/N)"
    if ($viewReport -eq 'Y') {
        $report = Get-Content -Path $results.AuditReportPath | ConvertFrom-Json
        $report | Format-List | Out-String | Write-Host -ForegroundColor White
    }
}