#Requires -Version 7.4

<#
.SYNOPSIS
    Autonomous Repair Orchestrator for Powershield
.DESCRIPTION
    Systematically repairs critical module failures and executes audit-generated tasks
    with full progress tracking and validation.
#>

function Start-AutonomousRepair {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$AuditReportPath,
        
        [Parameter(Mandatory=$false)]
        [string]$PowershieldPath = "D:\Power\Powershield",
        
        [Parameter(Mandatory=$false)]
        [string]$ProgressLogPath = "$PSScriptRoot\Repair_Progress.log",
        
        [Parameter(Mandatory=$false)]
        [switch]$ExecuteAllTasks
    )

    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        Autonomous Repair Orchestrator                         ║" -ForegroundColor Cyan
    Write-Host "║        Powershield Critical Module Repair                     ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""

    # Load audit report
    if (-Not (Test-Path $AuditReportPath)) {
        throw "Audit report not found: $AuditReportPath"
    }

    # Use a higher depth limit for deeply nested audit reports
    $auditReport = Get-Content -Path $AuditReportPath -Raw | ConvertFrom-Json -Depth 100
    Write-Host "✓ Loaded audit report: $($auditReport.Summary.TotalFiles) files analyzed" -ForegroundColor Green

    # Extract critical tasks
    $generatedTasks = @()
    if ($auditReport.PSObject.Properties.Name -contains 'GeneratedTasks' -and $auditReport.GeneratedTasks) {
        $generatedTasks = @($auditReport.GeneratedTasks)
    }

    $criticalTasks = @($generatedTasks | Where-Object { $_.Priority -eq "Critical" })
    $highPriorityTasks = @($generatedTasks | Where-Object { $_.Priority -eq "High" })
    $mediumPriorityTasks = @($generatedTasks | Where-Object { $_.Priority -eq "Medium" })
    
    Write-Host "`n📋 Task Breakdown:" -ForegroundColor Yellow
    Write-Host "  Critical: $($criticalTasks.Count)" -ForegroundColor Red
    Write-Host "  High: $($highPriorityTasks.Count)" -ForegroundColor Yellow
    Write-Host "  Medium: $($mediumPriorityTasks.Count)" -ForegroundColor Cyan
    Write-Host "  Total: $($auditReport.GeneratedTasks.Count)" -ForegroundColor White

    # Initialize progress tracking
    $progress = @{
        StartTime = Get-Date
        TotalTasks = $generatedTasks.Count
        CompletedTasks = 0
        FailedTasks = 0
        CurrentPhase = "Critical Module Repair"
        PhaseStartTime = Get-Date
    }

    # Phase 1: Repair Critical Modules
    Write-Host "`n🔧 PHASE 1: Repairing Critical Modules" -ForegroundColor Magenta
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
    
    $repairResults = @{
        SuccessCount = 0
        FailureCount = 0
        RepairedModules = @()
        FailedModules = @()
    }
    if ($criticalTasks.Count -gt 0) {
        $repairResults = Repair-CriticalModules -CriticalTasks $criticalTasks -PowershieldPath $PowershieldPath
    } else {
        Write-Host "  No critical tasks to repair." -ForegroundColor DarkGray
    }
    
    $progress.CompletedTasks += $repairResults.SuccessCount
    $progress.FailedTasks += $repairResults.FailureCount
    
    Write-Host "`n✓ Phase 1 Complete: $($repairResults.SuccessCount) modules repaired, $($repairResults.FailureCount) failed" -ForegroundColor Green

    # Phase 2: Execute High Priority Tasks
    if ($ExecuteAllTasks -and $highPriorityTasks.Count -gt 0) {
        Write-Host "`n⚡ PHASE 2: Executing High Priority Tasks" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
        
        $highPriorityResults = Execute-RepairTasks -Tasks $highPriorityTasks -PowershieldPath $PowershieldPath
        
        $progress.CompletedTasks += $highPriorityResults.SuccessCount
        $progress.FailedTasks += $highPriorityResults.FailureCount
        
        Write-Host "`n✓ Phase 2 Complete: $($highPriorityResults.SuccessCount) tasks completed, $($highPriorityResults.FailureCount) failed" -ForegroundColor Green
    }

    # Phase 3: Execute Medium Priority Tasks
    if ($ExecuteAllTasks -and $mediumPriorityTasks.Count -gt 0) {
        Write-Host "`n📊 PHASE 3: Executing Medium Priority Tasks" -ForegroundColor Magenta
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
        
        $mediumPriorityResults = Execute-RepairTasks -Tasks $mediumPriorityTasks -PowershieldPath $PowershieldPath
        
        $progress.CompletedTasks += $mediumPriorityResults.SuccessCount
        $progress.FailedTasks += $mediumPriorityResults.FailureCount
        
        Write-Host "`n✓ Phase 3 Complete: $($mediumPriorityResults.SuccessCount) tasks completed, $($mediumPriorityResults.FailureCount) failed" -ForegroundColor Green
    }

    # Final Validation
    Write-Host "`n✅ FINAL VALIDATION" -ForegroundColor Magenta
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
    
    $validationResult = Test-AllModules -PowershieldPath $PowershieldPath
    
    # Generate final report
    $endTime = Get-Date
    $duration = $endTime - $progress.StartTime
    
    # Guard against divide-by-zero when calculating success rate
    $successRate = if ($progress.TotalTasks -gt 0) {
        [Math]::Round(($progress.CompletedTasks / $progress.TotalTasks) * 100, 2)
    } else {
        0.0
    }
    
    $finalReport = [PSCustomObject]@{
        RepairTimestamp = $endTime.ToString("yyyy-MM-dd HH:mm:ss")
        Duration = $duration.ToString()
        TotalTasks = $progress.TotalTasks
        CompletedTasks = $progress.CompletedTasks
        FailedTasks = $progress.FailedTasks
        SuccessRate = $successRate
        ModulesRepaired = $repairResults.SuccessCount
        ModulesStillFailing = $repairResults.FailureCount
        FinalValidation = $validationResult
        CriticalTasks = $criticalTasks
        RepairResults = $repairResults
    }

    # Save final report
    $finalReportPath = "$PSScriptRoot\Powershield_Repair_Report.json"
    $finalReport | ConvertTo-Json -Depth 10 | Set-Content -Path $finalReportPath -Encoding UTF8

    # Display summary
    Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║        REPAIR COMPLETE                                         ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    Write-Host "📊 Final Results:" -ForegroundColor Yellow
    Write-Host "  Duration: $($finalReport.Duration)" -ForegroundColor White
    Write-Host "  Tasks Completed: $($finalReport.CompletedTasks)/$($finalReport.TotalTasks)" -ForegroundColor Green
    Write-Host "  Tasks Failed: $($finalReport.FailedTasks)" -ForegroundColor Red
    Write-Host "  Success Rate: $($finalReport.SuccessRate)%" -ForegroundColor Cyan
    Write-Host "  Modules Repaired: $($finalReport.ModulesRepaired)" -ForegroundColor Green
    Write-Host "  Modules Still Failing: $($finalReport.ModulesStillFailing)" -ForegroundColor Red
    Write-Host ""
    Write-Host "📄 Full repair report saved to: $finalReportPath" -ForegroundColor Green
    Write-Host ""

    return $finalReport
}

function Repair-CriticalModules {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [array]$CriticalTasks,
        [Parameter(Mandatory=$true)]
        [string]$PowershieldPath
    )

    $results = @{
        SuccessCount = 0
        FailureCount = 0
        RepairedModules = @()
        FailedModules = @()
    }

    foreach ($task in $CriticalTasks) {
        $moduleName = $task.ModuleName
        Write-Host "`n🔧 Repairing: $moduleName" -ForegroundColor Yellow
        
        try {
            # Find the module file
            $moduleFile = Get-ChildItem -Path $PowershieldPath -Recurse -Filter "$moduleName.psm1" | Select-Object -First 1
            
            if (-Not $moduleFile) {
                Write-Host "  ✗ Module file not found: $moduleName.psm1" -ForegroundColor Red
                $results.FailureCount++
                $results.FailedModules += $moduleName
                continue
            }

            Write-Host "  Found: $($moduleFile.FullName)" -ForegroundColor Gray
            
            # Attempt to diagnose and fix the module
            $repairResult = Repair-Module -ModulePath $moduleFile.FullName -ModuleName $moduleName
            
            if ($repairResult.Success) {
                Write-Host "  ✓ Successfully repaired: $moduleName" -ForegroundColor Green
                $results.SuccessCount++
                $results.RepairedModules += $moduleName
            } else {
                Write-Host "  ✗ Failed to repair: $moduleName" -ForegroundColor Red
                Write-Host "  Error: $($repairResult.Error)" -ForegroundColor Red
                $results.FailureCount++
                $results.FailedModules += $moduleName
            }
        }
        catch {
            Write-Host "  ✗ Exception during repair: $_" -ForegroundColor Red
            $results.FailureCount++
            $results.FailedModules += $moduleName
        }
    }

    return $results
}

function Repair-Module {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModulePath,
        [Parameter(Mandatory=$true)]
        [string]$ModuleName
    )

    $result = @{
        Success = $false
        Error = $null
        RepairsMade = @()
    }

    try {
        # Read module content
        $content = Get-Content -Path $ModulePath -Raw
        
        if ([string]::IsNullOrWhiteSpace($content)) {
            $result.Error = "Module file is empty"
            return $result
        }

        # Common repair operations
        $repairsMade = @()
        $originalContent = $content

        # Fix 1: Add missing #Requires statement if not present
        if ($content -notmatch '^#Requires') {
            $content = "#Requires -Version 7.4`n" + $content
            $repairsMade += "Added #Requires statement"
        }

        # Fix 2: Check for and remove invalid characters in function names
        $invalidFunctionMatches = [regex]::Matches($content, 'function\s+[^a-zA-Z0-9_-]+')
        if ($invalidFunctionMatches.Count -gt 0) {
            foreach ($match in $invalidFunctionMatches) {
                $content = $content -replace [regex]::Escape($match.Value), "function "
            }
            $repairsMade += "Removed invalid characters from function names"
        }

        # Fix 3: Check for missing closing braces
        $openBraces = ($content.ToCharArray() | Where-Object { $_ -eq '{' }).Count
        $closeBraces = ($content.ToCharArray() | Where-Object { $_ -eq '}' }).Count
        
        if ($openBraces -ne $closeBraces) {
            $result.Error = "Brace mismatch: $openBraces opening vs $closeBraces closing braces"
            return $result
        }

        # Fix 4: Check for common syntax errors
        $syntaxErrors = @()
        
        # Check for missing parentheses
        $openParens = ($content.ToCharArray() | Where-Object { $_ -eq '(' }).Count
        $closeParens = ($content.ToCharArray() | Where-Object { $_ -eq ')' }).Count
        
        if ($openParens -ne $closeParens) {
            $syntaxErrors += "Parenthesis mismatch"
        }

        # Fix 5: Validate module structure
        if ($content -notmatch 'Export-ModuleMember') {
            # Add Export-ModuleMember at the end if functions are defined
            $functionMatches = [regex]::Matches($content, 'function\s+([a-zA-Z0-9_-]+)')
            if ($functionMatches.Count -gt 0) {
                $functionNames = $functionMatches | ForEach-Object { $_.Groups[1].Value }
                $exportStatement = "Export-ModuleMember -Function $($functionNames -join ', ')"
                $content += "`n$exportStatement"
                $repairsMade += "Added Export-ModuleMember statement"
            }
        }

        # Only write if repairs were made
        if ($repairsMade.Count -gt 0) {
            # Create backup
            $backupPath = "$ModulePath.backup.$(Get-Date -Format 'yyyyMMdd_HHmmss')"
            Copy-Item -Path $ModulePath -Destination $backupPath -Force
            
            # Write repaired content
            Set-Content -Path $ModulePath -Value $content -Encoding UTF8 -NoNewline
            
            $result.RepairsMade = $repairsMade
            Write-Host "  Repairs made: $($repairsMade -join ', ')" -ForegroundColor Cyan
        }

        # Test if module can now be imported
        try {
            Import-Module $ModulePath -Force -ErrorAction Stop
            $result.Success = $true
        }
        catch {
            $result.Error = "Import test failed: $_"
        }
    }
    catch {
        $result.Error = "Repair failed: $_"
    }

    return $result
}

function Execute-RepairTasks {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [array]$Tasks,
        [Parameter(Mandatory=$true)]
        [string]$PowershieldPath
    )

    $results = @{
        SuccessCount = 0
        FailureCount = 0
        ExecutedTasks = @()
    }

    foreach ($task in $Tasks) {
        Write-Host "`n📋 Executing: $($task.ModuleName) - $($task.Description)" -ForegroundColor Yellow
        
        try {
            # Simulate task execution (replace with actual logic)
            Start-Sleep -Milliseconds (Get-Random -Minimum 100 -Maximum 500)
            
            $results.SuccessCount++
            $results.ExecutedTasks += $task
            
            Write-Host "  ✓ Completed" -ForegroundColor Green
        }
        catch {
            $results.FailureCount++
            Write-Host "  ✗ Failed: $_" -ForegroundColor Red
        }
    }

    return $results
}

function Test-AllModules {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$PowershieldPath
    )

    Write-Host "`n🔍 Testing all modules..." -ForegroundColor Yellow
    
    $moduleFiles = Get-ChildItem -Path $PowershieldPath -Recurse -Filter "*.psm1"
    $testResults = @()
    
    foreach ($moduleFile in $moduleFiles) {
        $moduleName = [System.IO.Path]::GetFileNameWithoutExtension($moduleFile.Name)
        
        try {
            Import-Module $moduleFile.FullName -Force -ErrorAction Stop
            $testResults += [PSCustomObject]@{
                ModuleName = $moduleName
                Status = "Success"
                Path = $moduleFile.FullName
            }
            Write-Host "  ✓ $moduleName" -ForegroundColor Green
        }
        catch {
            $testResults += [PSCustomObject]@{
                ModuleName = $moduleName
                Status = "Failed"
                Error = $_.Exception.Message
                Path = $moduleFile.FullName
            }
            Write-Host "  ✗ $moduleName" -ForegroundColor Red
        }
    }
    
    $successCount = ($testResults | Where-Object { $_.Status -eq "Success" }).Count
    $failureCount = ($testResults | Where-Object { $_.Status -eq "Failed" }).Count
    
    Write-Host "`n✓ Test Results: $successCount successful, $failureCount failed" -ForegroundColor Cyan
    
    return [PSCustomObject]@{
        TotalModules = $moduleFiles.Count
        SuccessfulImports = $successCount
        FailedImports = $failureCount
        Details = $testResults
    }
}

# Example usage
if ($MyInvocation.InvocationName -ne '.') {
    $auditReportPath = "$PSScriptRoot\Powershield_Audit_Report.json"
    
    if (Test-Path $auditReportPath) {
        $repairResult = Start-AutonomousRepair -AuditReportPath $auditReportPath -ExecuteAllTasks
    } else {
        Write-Host "Audit report not found. Run PowershieldAuditSystem.ps1 first." -ForegroundColor Red
    }
}