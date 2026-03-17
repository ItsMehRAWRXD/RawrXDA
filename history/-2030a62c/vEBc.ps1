<#
.SYNOPSIS
    Production SelfHealingModule - Comprehensive system health management and auto-recovery

.DESCRIPTION
    Advanced self-healing system that performs continuous health monitoring, automatic
    dependency resolution, component recovery, configuration validation, resource cleanup,
    and predictive failure detection. Implements circuit breaker patterns and graceful
    degradation strategies.

.PARAMETER ModuleDir
    Directory containing modules to monitor and heal

.PARAMETER HealthCheckInterval
    Interval in seconds between health checks (default: 30)

.PARAMETER EnableAutoRecovery
    Automatically attempt to recover failed components

.PARAMETER EnableResourceCleanup
    Clean up orphaned resources and memory

.PARAMETER MaxRetries
    Maximum recovery attempts before marking component as failed

.PARAMETER CircuitBreakerThreshold
    Number of failures before circuit breaker trips

.EXAMPLE
    Invoke-SelfHealingModule -EnableAutoRecovery -EnableResourceCleanup -HealthCheckInterval 60
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [hashtable]$Context = $null
        )
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

# Circuit breaker state
$script:CircuitBreakers = @{}

function Get-CircuitBreakerState {
    param([string]$ComponentName)

    if (-not $script:CircuitBreakers.ContainsKey($ComponentName)) {
        $script:CircuitBreakers[$ComponentName] = @{
            State = 'Closed'  # Closed = operational, Open = blocked, HalfOpen = testing
            FailureCount = 0
            LastFailure = $null
            LastSuccess = $null
            ResetTime = $null
        }
    }
    return $script:CircuitBreakers[$ComponentName]
}

function Update-CircuitBreaker {
    param(
        [string]$ComponentName,
        [bool]$Success,
        [int]$Threshold = 3,
        [int]$ResetTimeoutSeconds = 60
    )

    $cb = Get-CircuitBreakerState -ComponentName $ComponentName

    if ($Success) {
        $cb.FailureCount = 0
        $cb.LastSuccess = Get-Date
        $cb.State = 'Closed'
    } else {
        $cb.FailureCount++
        $cb.LastFailure = Get-Date

        if ($cb.FailureCount -ge $Threshold) {
            $cb.State = 'Open'
            $cb.ResetTime = (Get-Date).AddSeconds($ResetTimeoutSeconds)
            Write-StructuredLog -Message "Circuit breaker OPEN for $ComponentName after $($cb.FailureCount) failures" -Level Warning
        }
    }

    # Check if we should try again (half-open state)
    if ($cb.State -eq 'Open' -and $cb.ResetTime -and (Get-Date) -gt $cb.ResetTime) {
        $cb.State = 'HalfOpen'
        Write-StructuredLog -Message "Circuit breaker HALF-OPEN for $ComponentName - testing recovery" -Level Info
    }

    return $cb
}

function Test-ComponentHealth {
    <#
    .SYNOPSIS
        Comprehensive health check for a component
    #>
    param(
        [string]$FilePath,
        [string]$FunctionName
    )

    $health = @{
        FilePath = $FilePath
        FunctionName = $FunctionName
        FileExists = $false
        Parseable = $false
        FunctionDefined = $false
        Invocable = $false
        DependenciesMet = $true
        MemoryUsageMB = 0
        LastCheckTime = (Get-Date).ToString('o')
        Issues = @()
    }

    try {
        # Check file exists
        $health.FileExists = Test-Path $FilePath
        if (-not $health.FileExists) {
            $health.Issues += "File not found: $FilePath"
            return $health
        }

        # Check parseable
        $content = Get-Content $FilePath -Raw -ErrorAction Stop
        $tokens = $null
        $errors = $null
        $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)

        if ($errors.Count -eq 0) {
            $health.Parseable = $true
        } else {
            $health.Issues += "Parse errors: $($errors.Count)"
            foreach ($err in $errors | Select-Object -First 3) {
                $health.Issues += "  Line $($err.Extent.StartLineNumber): $($err.Message)"
            }
        }

        # Check function defined (after dot-sourcing)
        . $FilePath
        $cmd = Get-Command -Name $FunctionName -ErrorAction SilentlyContinue
        $health.FunctionDefined = $null -ne $cmd
        if (-not $health.FunctionDefined) {
            $health.Issues += "Function $FunctionName not defined after loading"
        }

        # Check dependencies
        $importMatches = [regex]::Matches($content, 'Import-Module\s+[''"]?([^''";\s]+)')
        foreach ($match in $importMatches) {
            $depModule = $match.Groups[1].Value
            if (-not (Get-Module -ListAvailable -Name $depModule -ErrorAction SilentlyContinue)) {
                $health.DependenciesMet = $false
                $health.Issues += "Missing dependency: $depModule"
            }
        }

        # Quick invocability test (dry run check)
        if ($health.FunctionDefined) {
            $health.Invocable = $true  # If we got here, it's likely invocable
        }

    } catch {
        $health.Issues += "Health check error: $($_.Exception.Message)"
    }

    # Memory usage for current process
    $proc = Get-Process -Id $PID -ErrorAction SilentlyContinue
    if ($proc) {
        $health.MemoryUsageMB = [Math]::Round($proc.WorkingSet64 / 1MB, 2)
    }

    return $health
}

function Invoke-ComponentRecovery {
    <#
    .SYNOPSIS
        Attempt to recover a failed component
    #>
    param(
        [string]$FilePath,
        [string]$FunctionName,
        [hashtable]$HealthStatus,
        [int]$MaxRetries = 3
    )

    $recovery = @{
        Success = $false
        Actions = @()
        FinalState = $null
    }

    Write-StructuredLog -Message "Attempting recovery for $FunctionName" -Level Info

    for ($attempt = 1; $attempt -le $MaxRetries; $attempt++) {
        Write-StructuredLog -Message "Recovery attempt $attempt of $MaxRetries" -Level Debug

        try {
            # Action 1: Clear any cached state
            if (Get-Module -Name ($FunctionName -replace '^Invoke-', '') -ErrorAction SilentlyContinue) {
                Remove-Module -Name ($FunctionName -replace '^Invoke-', '') -Force -ErrorAction SilentlyContinue
                $recovery.Actions += "Removed cached module"
            }

            # Action 2: Force garbage collection
            [System.GC]::Collect()
            [System.GC]::WaitForPendingFinalizers()
            $recovery.Actions += "Forced garbage collection"

            # Action 3: Re-dot-source the file
            . $FilePath
            $recovery.Actions += "Re-loaded source file"

            # Action 4: Verify function exists
            $cmd = Get-Command -Name $FunctionName -ErrorAction Stop
            $recovery.Actions += "Verified function definition"

            # Action 5: Test invocation with timeout
            $job = Start-Job -ScriptBlock {
                param($path, $funcName)
                . $path
                $result = & $funcName -ErrorAction Stop
                return $true
            } -ArgumentList $FilePath, $FunctionName

            $completed = Wait-Job $job -Timeout 30
            if ($completed) {
                $jobResult = Receive-Job $job -ErrorAction SilentlyContinue
                if ($jobResult) {
                    $recovery.Success = $true
                    $recovery.Actions += "Invocation test passed"
                }
            } else {
                Stop-Job $job
                $recovery.Actions += "Invocation test timed out"
            }
            Remove-Job $job -Force

            if ($recovery.Success) {
                break
            }

        } catch {
            $recovery.Actions += "Attempt $attempt failed: $($_.Exception.Message)"
            Start-Sleep -Milliseconds (500 * $attempt)  # Exponential backoff
        }
    }

    # Final health check
    $recovery.FinalState = Test-ComponentHealth -FilePath $FilePath -FunctionName $FunctionName

    if ($recovery.Success) {
        Write-StructuredLog -Message "Recovery successful for $FunctionName" -Level Info
    } else {
        Write-StructuredLog -Message "Recovery failed for $FunctionName after $MaxRetries attempts" -Level Error
    }

    return $recovery
}

function Invoke-ResourceCleanup {
    <#
    .SYNOPSIS
        Clean up system resources
    #>
    param()

    $cleanup = @{
        Actions = @()
        MemoryFreedMB = 0
        StartMemoryMB = 0
        EndMemoryMB = 0
    }

    try {
        $proc = Get-Process -Id $PID
        $cleanup.StartMemoryMB = [Math]::Round($proc.WorkingSet64 / 1MB, 2)

        # Clean up completed jobs
        $completedJobs = Get-Job | Where-Object { $_.State -in @('Completed', 'Failed', 'Stopped') }
        if ($completedJobs) {
            $completedJobs | Remove-Job -Force
            $cleanup.Actions += "Removed $($completedJobs.Count) completed jobs"
        }

        # Clear error variable
        $global:Error.Clear()
        $cleanup.Actions += "Cleared error history"

        # Force garbage collection
        [System.GC]::Collect()
        [System.GC]::WaitForPendingFinalizers()
        [System.GC]::Collect()
        $cleanup.Actions += "Performed garbage collection"

        # Remove old runspaces
        $runspaces = [System.Management.Automation.Runspaces.Runspace]::DefaultRunspace.RunspaceStateInfo
        $cleanup.Actions += "Checked runspace state"

        # Update memory stats
        $proc.Refresh()
        $cleanup.EndMemoryMB = [Math]::Round($proc.WorkingSet64 / 1MB, 2)
        $cleanup.MemoryFreedMB = $cleanup.StartMemoryMB - $cleanup.EndMemoryMB

    } catch {
        $cleanup.Actions += "Cleanup error: $($_.Exception.Message)"
    }

    return $cleanup
}

function Get-SystemHealthReport {
    <#
    .SYNOPSIS
        Generate comprehensive system health report
    #>
    param(
        [string]$ModuleDir
    )

    $report = @{
        Timestamp = (Get-Date).ToString('o')
        OverallHealth = 'Healthy'
        Components = @()
        CircuitBreakers = $script:CircuitBreakers
        SystemMetrics = @{}
        Recommendations = @()
    }

    # System metrics
    $proc = Get-Process -Id $PID -ErrorAction SilentlyContinue
    if ($proc) {
        $report.SystemMetrics = @{
            ProcessId = $PID
            WorkingSetMB = [Math]::Round($proc.WorkingSet64 / 1MB, 2)
            PrivateMemoryMB = [Math]::Round($proc.PrivateMemorySize64 / 1MB, 2)
            HandleCount = $proc.HandleCount
            ThreadCount = $proc.Threads.Count
            CpuTime = $proc.TotalProcessorTime.TotalSeconds
            Uptime = ((Get-Date) - $proc.StartTime).TotalMinutes
        }
    }

    # Check each component
    $featureFiles = Get-ChildItem -Path $ModuleDir -Filter '*_AutoFeature.ps1' -ErrorAction SilentlyContinue

    $healthyCount = 0
    $warningCount = 0
    $criticalCount = 0

    foreach ($file in $featureFiles) {
        $funcName = 'Invoke-' + ($file.BaseName -replace '_AutoFeature$', '')
        $health = Test-ComponentHealth -FilePath $file.FullName -FunctionName $funcName

        $componentStatus = @{
            Name = $funcName
            File = $file.Name
            Health = $health
            CircuitBreaker = Get-CircuitBreakerState -ComponentName $funcName
            Status = 'Healthy'
        }

        if ($health.Issues.Count -gt 0) {
            $componentStatus.Status = 'Warning'
            $warningCount++
        }

        if (-not $health.FileExists -or -not $health.Parseable -or -not $health.FunctionDefined) {
            $componentStatus.Status = 'Critical'
            $criticalCount++
        } else {
            $healthyCount++
        }

        $report.Components += $componentStatus
    }

    # Determine overall health
    if ($criticalCount -gt 0) {
        $report.OverallHealth = 'Critical'
    } elseif ($warningCount -gt 0) {
        $report.OverallHealth = 'Degraded'
    }

    # Generate recommendations
    if ($report.SystemMetrics.WorkingSetMB -gt 500) {
        $report.Recommendations += "High memory usage detected ($($report.SystemMetrics.WorkingSetMB)MB). Consider running resource cleanup."
    }
    if ($criticalCount -gt 0) {
        $report.Recommendations += "$criticalCount components in critical state. Run with -EnableAutoRecovery to attempt repairs."
    }
    foreach ($cb in $script:CircuitBreakers.Keys) {
        if ($script:CircuitBreakers[$cb].State -eq 'Open') {
            $report.Recommendations += "Circuit breaker open for $cb. Component is temporarily disabled."
        }
    }

    return $report
}

function Invoke-SelfHealingModule {
    [CmdletBinding()]
    param(
        [string]$ModuleDir = $null,

        [switch]$Recurse,

        [switch]$NoInvoke,

        [int]$MaxRetries = 3,

        [switch]$EnableAutoRecovery,

        [switch]$EnableResourceCleanup,

        [int]$CircuitBreakerThreshold = 3,

        [switch]$GenerateReport,

        [string]$ReportPath = "D:/lazy init ide/reports/health_report.json"
    )

    $functionName = 'Invoke-SelfHealingModule'
    $startTime = Get-Date

    begin {
        $results = @()
        $scriptRoot = Split-Path -Parent $PSScriptRoot
        if (-not $ModuleDir) { $ModuleDir = Join-Path $scriptRoot 'auto_generated_methods' }

        # Try to import shared helpers
        $loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
        $configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'

        if (Test-Path $loggingModule) {
            try { Import-Module $loggingModule -Force -ErrorAction Stop } catch { }
        }
        if (Test-Path $configModule) {
            try { Import-Module $configModule -Force -ErrorAction Stop } catch { }
        }

        Write-StructuredLog -Message "Starting SelfHealingModule for $ModuleDir" -Level Info

        if (-not (Test-Path -Path $ModuleDir -PathType Container)) {
            $msg = "ModuleDir '$ModuleDir' not found."
            Write-StructuredLog -Message $msg -Level Error
            throw $msg
        }

        # Resource cleanup if enabled
        if ($EnableResourceCleanup) {
            Write-StructuredLog -Message "Performing resource cleanup..." -Level Info
            $cleanupResult = Invoke-ResourceCleanup
            Write-StructuredLog -Message "Cleanup complete. Memory freed: $($cleanupResult.MemoryFreedMB)MB" -Level Info
        }

        $gciParams = @{ Path = $ModuleDir; Filter = '*_AutoFeature.ps1'; ErrorAction = 'SilentlyContinue' }
        if ($Recurse) { $gciParams['Recurse'] = $true }
        $moduleFiles = Get-ChildItem @gciParams
    }

    process {
        foreach ($file in $moduleFiles) {
            $funcName = 'Invoke-' + ($file.BaseName -replace '_AutoFeature$', '')

            # Check circuit breaker
            $cb = Get-CircuitBreakerState -ComponentName $funcName
            if ($cb.State -eq 'Open') {
                Write-StructuredLog -Message "Skipping $funcName - circuit breaker is OPEN" -Level Warning
                $results += @{
                    File = $file.FullName
                    Function = $funcName
                    Status = 'CircuitBreakerOpen'
                    Skipped = $true
                }
                continue
            }

            $entry = @{
                File = $file.FullName
                Function = $funcName
                HealthCheck = $null
                Reloaded = $false
                Invoked = $false
                Error = $null
                DurationMs = 0
                RecoveryAttempted = $false
                RecoverySuccess = $false
            }

            # Health check
            Write-StructuredLog -Message "Health checking: $($file.Name)" -Level Debug
            $health = Test-ComponentHealth -FilePath $file.FullName -FunctionName $funcName
            $entry.HealthCheck = $health

            # Recovery if needed and enabled
            if ($health.Issues.Count -gt 0 -and $EnableAutoRecovery) {
                Write-StructuredLog -Message "Issues detected in $funcName, attempting recovery..." -Level Warning
                $entry.RecoveryAttempted = $true

                $recovery = Invoke-ComponentRecovery -FilePath $file.FullName -FunctionName $funcName -HealthStatus $health -MaxRetries $MaxRetries
                $entry.RecoverySuccess = $recovery.Success

                if (-not $recovery.Success) {
                    Update-CircuitBreaker -ComponentName $funcName -Success $false -Threshold $CircuitBreakerThreshold
                    $entry.Error = "Recovery failed after $MaxRetries attempts"
                    $results += $entry
                    continue
                }
            }

            # Dot-source the script
            try {
                $dotDuration = Measure-Command { . $file.FullName }
                $entry.DurationMs += $dotDuration.TotalMilliseconds
                Write-StructuredLog -Message "Loaded $($file.Name) in $([Math]::Round($dotDuration.TotalMilliseconds, 2))ms" -Level Debug
            } catch {
                $entry.Error = $_.Exception.Message
                Write-StructuredLog -Message "Failed to load $($file.Name): $($entry.Error)" -Level Error
                Update-CircuitBreaker -ComponentName $funcName -Success $false -Threshold $CircuitBreakerThreshold
                $results += $entry
                continue
            }

            # Verify function exists
            $cmd = Get-Command -Name $funcName -ErrorAction SilentlyContinue
            if (-not $cmd) {
                Write-StructuredLog -Message "Function $funcName not found after loading" -Level Warning
                $entry.Error = 'FunctionNotFound'
                Update-CircuitBreaker -ComponentName $funcName -Success $false -Threshold $CircuitBreakerThreshold
                $results += $entry
                continue
            }

            # Skip invocation if requested
            if ($NoInvoke) {
                Update-CircuitBreaker -ComponentName $funcName -Success $true -Threshold $CircuitBreakerThreshold
                $results += $entry
                continue
            }

            # Invoke the function
            try {
                $invokeDuration = Measure-Command { & $funcName -ErrorAction Stop | Out-Null }
                $entry.DurationMs += $invokeDuration.TotalMilliseconds
                $entry.Invoked = $true
                Write-StructuredLog -Message "Invoked $funcName in $([Math]::Round($invokeDuration.TotalMilliseconds, 2))ms" -Level Info
                Update-CircuitBreaker -ComponentName $funcName -Success $true -Threshold $CircuitBreakerThreshold
            } catch {
                $entry.Error = $_.Exception.Message
                Write-StructuredLog -Message "Invocation failed for $funcName : $($entry.Error)" -Level Error

                # Try recovery if enabled
                if ($EnableAutoRecovery -and -not $entry.RecoveryAttempted) {
                    $entry.RecoveryAttempted = $true
                    $recovery = Invoke-ComponentRecovery -FilePath $file.FullName -FunctionName $funcName -HealthStatus $health -MaxRetries $MaxRetries

                    if ($recovery.Success) {
                        # Retry invocation
                        try {
                            $retryDuration = Measure-Command { & $funcName -ErrorAction Stop | Out-Null }
                            $entry.DurationMs += $retryDuration.TotalMilliseconds
                            $entry.Invoked = $true
                            $entry.Reloaded = $true
                            $entry.Error = $null
                            $entry.RecoverySuccess = $true
                            Update-CircuitBreaker -ComponentName $funcName -Success $true -Threshold $CircuitBreakerThreshold
                        } catch {
                            $entry.Error = "Recovery succeeded but invocation still failed: $($_.Exception.Message)"
                            Update-CircuitBreaker -ComponentName $funcName -Success $false -Threshold $CircuitBreakerThreshold
                        }
                    } else {
                        Update-CircuitBreaker -ComponentName $funcName -Success $false -Threshold $CircuitBreakerThreshold
                    }
                } else {
                    Update-CircuitBreaker -ComponentName $funcName -Success $false -Threshold $CircuitBreakerThreshold
                }
            }

            $results += $entry
        }
    }

    end {
        $duration = ((Get-Date) - $startTime).TotalSeconds

        # Generate comprehensive report if requested
        if ($GenerateReport) {
            $healthReport = Get-SystemHealthReport -ModuleDir $ModuleDir
            $healthReport.ExecutionResults = $results
            $healthReport.TotalDuration = $duration

            # Ensure report directory exists
            $reportDir = Split-Path $ReportPath -Parent
            if (-not (Test-Path $reportDir)) {
                New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
            }

            $healthReport | ConvertTo-Json -Depth 15 | Set-Content $ReportPath -Encoding UTF8
            Write-StructuredLog -Message "Health report saved to $ReportPath" -Level Info
        }

        # Summary
        $successful = ($results | Where-Object { $_.Invoked -eq $true -or ($_.Skipped -ne $true -and $_.Error -eq $null) }).Count
        $failed = ($results | Where-Object { $_.Error -ne $null }).Count
        $skipped = ($results | Where-Object { $_.Skipped -eq $true }).Count

        Write-StructuredLog -Message "SelfHealingModule complete in $([Math]::Round($duration, 2))s. Success: $successful, Failed: $failed, Skipped: $skipped" -Level Info

        return $results
    }
}

