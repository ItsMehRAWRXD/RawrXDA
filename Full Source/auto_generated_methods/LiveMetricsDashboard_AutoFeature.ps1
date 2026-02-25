<#
.SYNOPSIS
    Production LiveMetricsDashboard - Real-time system monitoring and metrics collection

.DESCRIPTION
    Comprehensive metrics collection engine providing real-time system performance monitoring,
    application health tracking, resource utilization analysis, performance baselining,
    anomaly detection, and historical trend analysis with exportable dashboards.

.PARAMETER MethodsDir
    Directory containing AutoFeature methods to monitor

.PARAMETER RefreshIntervalSeconds
    Interval between metric collection cycles (default: 5)

.PARAMETER CollectSystemMetrics
    Enable system-level metrics (CPU, RAM, Disk)

.PARAMETER CollectProcessMetrics
    Enable process-level metrics for PowerShell

.PARAMETER HistoryDepth
    Number of historical data points to retain (default: 100)

.PARAMETER EnableAnomalyDetection
    Enable statistical anomaly detection on metrics

.EXAMPLE
    Invoke-LiveMetricsDashboard -CollectSystemMetrics -CollectProcessMetrics -RefreshIntervalSeconds 10
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [hashtable]$Data = $null
        )
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

# Global metrics storage
$script:MetricsHistory = @{
    System = [System.Collections.Generic.List[PSCustomObject]]::new()
    Process = [System.Collections.Generic.List[PSCustomObject]]::new()
    Features = [System.Collections.Generic.List[PSCustomObject]]::new()
    Custom = [System.Collections.Generic.List[PSCustomObject]]::new()
}

$script:Baselines = @{}
$script:Anomalies = @()

function Get-SystemMetrics {
    <#
    .SYNOPSIS
        Collect comprehensive system-level performance metrics
    #>
    param()

    $metrics = @{
        Timestamp = (Get-Date).ToString('o')
        Hostname = $env:COMPUTERNAME
    }

    try {
        # CPU Usage
        $cpuCounter = Get-Counter '\Processor(_Total)\% Processor Time' -ErrorAction SilentlyContinue
        $metrics.CpuPercent = if ($cpuCounter) { [Math]::Round($cpuCounter.CounterSamples[0].CookedValue, 2) } else { 0 }

        # Memory Usage
        $os = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
        if ($os) {
            $totalMemGB = [Math]::Round($os.TotalVisibleMemorySize / 1MB, 2)
            $freeMemGB = [Math]::Round($os.FreePhysicalMemory / 1MB, 2)
            $usedMemGB = $totalMemGB - $freeMemGB
            $metrics.MemoryTotalGB = $totalMemGB
            $metrics.MemoryUsedGB = $usedMemGB
            $metrics.MemoryFreeGB = $freeMemGB
            $metrics.MemoryPercent = [Math]::Round(($usedMemGB / $totalMemGB) * 100, 2)
        }

        # Disk Usage
        $disks = Get-CimInstance Win32_LogicalDisk -Filter "DriveType=3" -ErrorAction SilentlyContinue
        $metrics.Disks = @()
        foreach ($disk in $disks) {
            $metrics.Disks += @{
                Drive = $disk.DeviceID
                TotalGB = [Math]::Round($disk.Size / 1GB, 2)
                FreeGB = [Math]::Round($disk.FreeSpace / 1GB, 2)
                UsedPercent = [Math]::Round((($disk.Size - $disk.FreeSpace) / $disk.Size) * 100, 2)
            }
        }

        # Network I/O
        $networkCounters = Get-Counter '\Network Interface(*)\Bytes Total/sec' -ErrorAction SilentlyContinue
        if ($networkCounters) {
            $totalBytesPerSec = ($networkCounters.CounterSamples | Where-Object { $_.InstanceName -ne '_total' } | Measure-Object -Property CookedValue -Sum).Sum
            $metrics.NetworkBytesPerSec = [Math]::Round($totalBytesPerSec, 0)
            $metrics.NetworkMbps = [Math]::Round(($totalBytesPerSec * 8) / 1MB, 2)
        }

        # Disk I/O
        $diskReadCounter = Get-Counter '\PhysicalDisk(_Total)\Disk Read Bytes/sec' -ErrorAction SilentlyContinue
        $diskWriteCounter = Get-Counter '\PhysicalDisk(_Total)\Disk Write Bytes/sec' -ErrorAction SilentlyContinue
        $metrics.DiskReadBytesPerSec = if ($diskReadCounter) { [Math]::Round($diskReadCounter.CounterSamples[0].CookedValue, 0) } else { 0 }
        $metrics.DiskWriteBytesPerSec = if ($diskWriteCounter) { [Math]::Round($diskWriteCounter.CounterSamples[0].CookedValue, 0) } else { 0 }

        # System Uptime
        $uptime = (Get-Date) - (Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue).LastBootUpTime
        $metrics.UptimeHours = [Math]::Round($uptime.TotalHours, 2)

        # Process Count
        $metrics.ProcessCount = (Get-Process -ErrorAction SilentlyContinue).Count

        # Thread Count
        $metrics.ThreadCount = (Get-Process -ErrorAction SilentlyContinue | Measure-Object -Property Threads -Sum).Sum

    } catch {
        Write-StructuredLog -Message "Error collecting system metrics: $_" -Level Warning
    }

    return [PSCustomObject]$metrics
}

function Get-ProcessMetrics {
    <#
    .SYNOPSIS
        Collect detailed metrics for PowerShell and related processes
    #>
    param()

    $metrics = @{
        Timestamp = (Get-Date).ToString('o')
        Processes = @()
    }

    try {
        $psProcesses = Get-Process -Name pwsh, powershell, code -ErrorAction SilentlyContinue

        foreach ($proc in $psProcesses) {
            $metrics.Processes += @{
                Name = $proc.ProcessName
                Id = $proc.Id
                CpuSeconds = $proc.CPU
                WorkingSetMB = [Math]::Round($proc.WorkingSet64 / 1MB, 2)
                PrivateMemoryMB = [Math]::Round($proc.PrivateMemorySize64 / 1MB, 2)
                VirtualMemoryMB = [Math]::Round($proc.VirtualMemorySize64 / 1MB, 2)
                ThreadCount = $proc.Threads.Count
                HandleCount = $proc.HandleCount
                StartTime = $proc.StartTime.ToString('o')
                RunningTimeMinutes = [Math]::Round(((Get-Date) - $proc.StartTime).TotalMinutes, 2)
            }
        }

        # Current process detailed metrics
        $currentProcess = Get-Process -Id $PID -ErrorAction SilentlyContinue
        if ($currentProcess) {
            $metrics.CurrentProcess = @{
                Id = $PID
                WorkingSetMB = [Math]::Round($currentProcess.WorkingSet64 / 1MB, 2)
                GCTotalMemoryMB = [Math]::Round([GC]::GetTotalMemory($false) / 1MB, 2)
                ThreadCount = $currentProcess.Threads.Count
            }
        }

    } catch {
        Write-StructuredLog -Message "Error collecting process metrics: $_" -Level Warning
    }

    return [PSCustomObject]$metrics
}

function Get-FeatureMetrics {
    <#
    .SYNOPSIS
        Collect execution metrics for AutoFeature methods
    #>
    param(
        [string]$MethodsDir
    )

    $metrics = @{
        Timestamp = (Get-Date).ToString('o')
        Features = @()
    }

    try {
        $featureFiles = Get-ChildItem -Path $MethodsDir -Filter "*_AutoFeature.ps1" -ErrorAction SilentlyContinue

        foreach ($file in $featureFiles) {
            $featureName = $file.BaseName -replace '_AutoFeature$', ''
            $funcName = "Invoke-$featureName"

            $featureMetric = @{
                Name = $featureName
                FileName = $file.Name
                Size = $file.Length
                LastModified = $file.LastWriteTime.ToString('o')
                FunctionExists = $false
                ExecutionTime = $null
                Status = 'Unknown'
            }

            try {
                # Dot-source without executing
                . $file.FullName

                if (Get-Command $funcName -ErrorAction SilentlyContinue) {
                    $featureMetric.FunctionExists = $true

                    # Measure cold load time (don't actually execute to avoid side effects)
                    $loadTime = Measure-Command { . $file.FullName } | Select-Object -ExpandProperty TotalMilliseconds
                    $featureMetric.LoadTimeMs = [Math]::Round($loadTime, 2)
                    $featureMetric.Status = 'Available'
                }
            } catch {
                $featureMetric.Status = 'Error'
                $featureMetric.Error = $_.Exception.Message
            }

            $metrics.Features += $featureMetric
        }

        $metrics.TotalFeatures = $metrics.Features.Count
        $metrics.AvailableFeatures = ($metrics.Features | Where-Object { $_.Status -eq 'Available' }).Count
        $metrics.ErrorFeatures = ($metrics.Features | Where-Object { $_.Status -eq 'Error' }).Count

    } catch {
        Write-StructuredLog -Message "Error collecting feature metrics: $_" -Level Warning
    }

    return [PSCustomObject]$metrics
}

function Update-MetricsBaseline {
    <#
    .SYNOPSIS
        Calculate statistical baselines from historical metrics
    #>
    param(
        [string]$MetricName,
        [double[]]$Values
    )

    if ($Values.Count -lt 5) { return $null }

    $stats = $Values | Measure-Object -Average -StandardDeviation -Minimum -Maximum

    return @{
        MetricName = $MetricName
        Average = [Math]::Round($stats.Average, 2)
        StdDev = [Math]::Round($stats.StandardDeviation, 2)
        Min = [Math]::Round($stats.Minimum, 2)
        Max = [Math]::Round($stats.Maximum, 2)
        Count = $stats.Count
        LowerBound = [Math]::Round($stats.Average - (2 * $stats.StandardDeviation), 2)
        UpperBound = [Math]::Round($stats.Average + (2 * $stats.StandardDeviation), 2)
        UpdatedAt = (Get-Date).ToString('o')
    }
}

function Test-MetricAnomaly {
    <#
    .SYNOPSIS
        Detect anomalies using statistical thresholds
    #>
    param(
        [string]$MetricName,
        [double]$Value,
        [hashtable]$Baseline
    )

    if (-not $Baseline) { return $null }

    $isAnomaly = $Value -lt $Baseline.LowerBound -or $Value -gt $Baseline.UpperBound
    $deviation = if ($Baseline.StdDev -gt 0) {
        [Math]::Abs($Value - $Baseline.Average) / $Baseline.StdDev
    } else { 0 }

    if ($isAnomaly) {
        return @{
            MetricName = $MetricName
            Value = $Value
            Expected = $Baseline.Average
            Deviation = [Math]::Round($deviation, 2)
            Direction = if ($Value -gt $Baseline.UpperBound) { 'Above' } else { 'Below' }
            Severity = switch ([Math]::Floor($deviation)) {
                { $_ -ge 4 } { 'Critical' }
                { $_ -ge 3 } { 'High' }
                { $_ -ge 2 } { 'Medium' }
                default { 'Low' }
            }
            DetectedAt = (Get-Date).ToString('o')
        }
    }

    return $null
}

function Export-MetricsDashboard {
    <#
    .SYNOPSIS
        Export metrics as an HTML dashboard
    #>
    param(
        [PSCustomObject]$CurrentMetrics,
        [hashtable]$History,
        [string]$OutputPath
    )

    $html = @"
<!DOCTYPE html>
<html>
<head>
    <title>RawrXD Live Metrics Dashboard</title>
    <meta http-equiv="refresh" content="10">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Segoe UI', sans-serif; background: #1a1a2e; color: #eee; padding: 20px; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); padding: 20px; border-radius: 10px; margin-bottom: 20px; }
        .header h1 { font-size: 1.8em; margin-bottom: 5px; }
        .header .timestamp { opacity: 0.8; font-size: 0.9em; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; margin-bottom: 20px; }
        .card { background: #16213e; border-radius: 10px; padding: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .card h3 { color: #667eea; margin-bottom: 15px; font-size: 0.9em; text-transform: uppercase; letter-spacing: 1px; }
        .metric { margin-bottom: 15px; }
        .metric-label { font-size: 0.8em; color: #888; margin-bottom: 5px; }
        .metric-value { font-size: 2em; font-weight: bold; color: #4ecca3; }
        .metric-value.warning { color: #ffc107; }
        .metric-value.danger { color: #ff6b6b; }
        .progress-bar { height: 8px; background: #0f3460; border-radius: 4px; overflow: hidden; margin-top: 5px; }
        .progress-fill { height: 100%; transition: width 0.3s ease; }
        .progress-fill.good { background: linear-gradient(90deg, #4ecca3, #45b7d1); }
        .progress-fill.warning { background: linear-gradient(90deg, #ffc107, #ff9800); }
        .progress-fill.danger { background: linear-gradient(90deg, #ff6b6b, #ee5a24); }
        .feature-list { max-height: 300px; overflow-y: auto; }
        .feature-item { display: flex; justify-content: space-between; padding: 10px; background: #0f3460; margin-bottom: 5px; border-radius: 5px; }
        .feature-status { padding: 2px 8px; border-radius: 3px; font-size: 0.8em; }
        .feature-status.ok { background: #4ecca3; color: #000; }
        .feature-status.error { background: #ff6b6b; color: #fff; }
        .anomaly { background: #ff6b6b22; border-left: 3px solid #ff6b6b; padding: 10px; margin-bottom: 10px; border-radius: 0 5px 5px 0; }
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 RawrXD Live Metrics Dashboard</h1>
        <div class="timestamp">Last Updated: $($CurrentMetrics.System.Timestamp)</div>
    </div>

    <div class="grid">
        <div class="card">
            <h3>💻 CPU</h3>
            <div class="metric">
                <div class="metric-value $(if ($CurrentMetrics.System.CpuPercent -gt 80) { 'danger' } elseif ($CurrentMetrics.System.CpuPercent -gt 60) { 'warning' })">$($CurrentMetrics.System.CpuPercent)%</div>
                <div class="progress-bar">
                    <div class="progress-fill $(if ($CurrentMetrics.System.CpuPercent -gt 80) { 'danger' } elseif ($CurrentMetrics.System.CpuPercent -gt 60) { 'warning' } else { 'good' })" style="width: $($CurrentMetrics.System.CpuPercent)%"></div>
                </div>
            </div>
        </div>

        <div class="card">
            <h3>🧠 Memory</h3>
            <div class="metric">
                <div class="metric-value $(if ($CurrentMetrics.System.MemoryPercent -gt 85) { 'danger' } elseif ($CurrentMetrics.System.MemoryPercent -gt 70) { 'warning' })">$($CurrentMetrics.System.MemoryPercent)%</div>
                <div class="metric-label">$($CurrentMetrics.System.MemoryUsedGB) GB / $($CurrentMetrics.System.MemoryTotalGB) GB</div>
                <div class="progress-bar">
                    <div class="progress-fill $(if ($CurrentMetrics.System.MemoryPercent -gt 85) { 'danger' } elseif ($CurrentMetrics.System.MemoryPercent -gt 70) { 'warning' } else { 'good' })" style="width: $($CurrentMetrics.System.MemoryPercent)%"></div>
                </div>
            </div>
        </div>

        <div class="card">
            <h3>📡 Network</h3>
            <div class="metric">
                <div class="metric-value">$($CurrentMetrics.System.NetworkMbps) Mbps</div>
                <div class="metric-label">$($CurrentMetrics.System.NetworkBytesPerSec) bytes/sec</div>
            </div>
        </div>

        <div class="card">
            <h3>⏱️ Uptime</h3>
            <div class="metric">
                <div class="metric-value">$([Math]::Round($CurrentMetrics.System.UptimeHours, 1))h</div>
                <div class="metric-label">$($CurrentMetrics.System.ProcessCount) processes | $($CurrentMetrics.System.ThreadCount) threads</div>
            </div>
        </div>
    </div>

    <div class="grid">
        <div class="card">
            <h3>🔧 Features ($($CurrentMetrics.Features.TotalFeatures) total)</h3>
            <div class="feature-list">
$(foreach ($feature in $CurrentMetrics.Features.Features) {
    "<div class='feature-item'><span>$($feature.Name)</span><span class='feature-status $(if ($feature.Status -eq 'Available') { 'ok' } else { 'error' })'>$($feature.Status)</span></div>"
})
            </div>
        </div>

        <div class="card">
            <h3>💾 Disk Usage</h3>
$(foreach ($disk in $CurrentMetrics.System.Disks) {
    "<div class='metric'>
        <div class='metric-label'>$($disk.Drive) - $($disk.FreeGB) GB free</div>
        <div class='progress-bar'>
            <div class='progress-fill $(if ($disk.UsedPercent -gt 90) { 'danger' } elseif ($disk.UsedPercent -gt 75) { 'warning' } else { 'good' })' style='width: $($disk.UsedPercent)%'></div>
        </div>
    </div>"
})
        </div>
    </div>
</body>
</html>
"@

    $html | Set-Content $OutputPath -Encoding UTF8
}

function Invoke-LiveMetricsDashboard {
    [CmdletBinding()]
    param(
        [string]$MethodsDir = "D:/lazy init ide/auto_generated_methods",

        [int]$RefreshIntervalSeconds = 5,

        [switch]$CollectSystemMetrics,

        [switch]$CollectProcessMetrics,

        [int]$HistoryDepth = 100,

        [switch]$EnableAnomalyDetection,

        [switch]$Continuous,

        [int]$DurationSeconds = 60,

        [string]$OutputPath = "D:/lazy init ide/auto_generated_methods/FeatureMetrics.json",

        [switch]$ExportDashboard,

        [string]$DashboardPath = "D:/lazy init ide/reports/metrics_dashboard.html"
    )

    $functionName = 'Invoke-LiveMetricsDashboard'
    $startTime = Get-Date

    try {
        Write-StructuredLog -Message "Starting LiveMetricsDashboard" -Level Info

        # Ensure output directories exist
        $outputDir = Split-Path $OutputPath -Parent
        if (-not (Test-Path $outputDir)) {
            New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
        }
        if ($ExportDashboard) {
            $dashboardDir = Split-Path $DashboardPath -Parent
            if (-not (Test-Path $dashboardDir)) {
                New-Item -ItemType Directory -Path $dashboardDir -Force | Out-Null
            }
        }

        $iterations = 0
        $maxIterations = if ($Continuous) { [int]::MaxValue } else { [Math]::Ceiling($DurationSeconds / $RefreshIntervalSeconds) }

        $allMetrics = @()

        while ($iterations -lt $maxIterations) {
            $cycleStart = Get-Date
            $currentMetrics = @{
                Timestamp = (Get-Date).ToString('o')
                Iteration = $iterations + 1
            }

            # Collect system metrics
            if ($CollectSystemMetrics) {
                Write-StructuredLog -Message "Collecting system metrics..." -Level Debug
                $currentMetrics.System = Get-SystemMetrics

                # Store in history
                $script:MetricsHistory.System.Add($currentMetrics.System)
                if ($script:MetricsHistory.System.Count -gt $HistoryDepth) {
                    $script:MetricsHistory.System.RemoveAt(0)
                }

                # Update baselines and check anomalies
                if ($EnableAnomalyDetection -and $script:MetricsHistory.System.Count -ge 10) {
                    $cpuValues = $script:MetricsHistory.System | ForEach-Object { $_.CpuPercent }
                    $script:Baselines['CpuPercent'] = Update-MetricsBaseline -MetricName 'CpuPercent' -Values $cpuValues

                    $anomaly = Test-MetricAnomaly -MetricName 'CpuPercent' -Value $currentMetrics.System.CpuPercent -Baseline $script:Baselines['CpuPercent']
                    if ($anomaly) {
                        $script:Anomalies += $anomaly
                        Write-StructuredLog -Message "Anomaly detected: CPU at $($currentMetrics.System.CpuPercent)% (expected ~$($script:Baselines['CpuPercent'].Average)%)" -Level Warning
                    }
                }
            }

            # Collect process metrics
            if ($CollectProcessMetrics) {
                Write-StructuredLog -Message "Collecting process metrics..." -Level Debug
                $currentMetrics.Process = Get-ProcessMetrics

                $script:MetricsHistory.Process.Add($currentMetrics.Process)
                if ($script:MetricsHistory.Process.Count -gt $HistoryDepth) {
                    $script:MetricsHistory.Process.RemoveAt(0)
                }
            }

            # Always collect feature metrics
            Write-StructuredLog -Message "Collecting feature metrics..." -Level Debug
            $currentMetrics.Features = Get-FeatureMetrics -MethodsDir $MethodsDir

            $script:MetricsHistory.Features.Add($currentMetrics.Features)
            if ($script:MetricsHistory.Features.Count -gt $HistoryDepth) {
                $script:MetricsHistory.Features.RemoveAt(0)
            }

            $allMetrics += [PSCustomObject]$currentMetrics

            # Export HTML dashboard if requested
            if ($ExportDashboard) {
                Export-MetricsDashboard -CurrentMetrics $currentMetrics -History $script:MetricsHistory -OutputPath $DashboardPath
            }

            # Display current metrics summary
            if (-not $Continuous) {
                Write-StructuredLog -Message "Iteration $($iterations + 1): Features=$($currentMetrics.Features.TotalFeatures), Available=$($currentMetrics.Features.AvailableFeatures)" -Level Info
                if ($CollectSystemMetrics) {
                    Write-StructuredLog -Message "  System: CPU=$($currentMetrics.System.CpuPercent)%, Memory=$($currentMetrics.System.MemoryPercent)%" -Level Info
                }
            }

            $iterations++

            # Wait for next cycle
            if ($iterations -lt $maxIterations) {
                $cycleDuration = ((Get-Date) - $cycleStart).TotalSeconds
                $sleepTime = [Math]::Max(0, $RefreshIntervalSeconds - $cycleDuration)
                if ($sleepTime -gt 0) {
                    Start-Sleep -Seconds $sleepTime
                }
            }
        }

        # Build final report
        $report = [PSCustomObject]@{
            Timestamp = (Get-Date).ToString('o')
            Duration = ((Get-Date) - $startTime).TotalSeconds
            Configuration = @{
                MethodsDir = $MethodsDir
                RefreshIntervalSeconds = $RefreshIntervalSeconds
                CollectSystemMetrics = $CollectSystemMetrics.IsPresent
                CollectProcessMetrics = $CollectProcessMetrics.IsPresent
                HistoryDepth = $HistoryDepth
                EnableAnomalyDetection = $EnableAnomalyDetection.IsPresent
            }
            Summary = @{
                TotalIterations = $iterations
                TotalFeatures = $allMetrics[-1].Features.TotalFeatures
                AvailableFeatures = $allMetrics[-1].Features.AvailableFeatures
                AnomaliesDetected = $script:Anomalies.Count
            }
            Baselines = $script:Baselines
            Anomalies = $script:Anomalies
            LatestMetrics = $allMetrics[-1]
            History = @{
                System = $script:MetricsHistory.System | Select-Object -Last 10
                Process = $script:MetricsHistory.Process | Select-Object -Last 10
                Features = $script:MetricsHistory.Features | Select-Object -Last 10
            }
            AllMetrics = $allMetrics
        }

        $report | ConvertTo-Json -Depth 15 | Set-Content $OutputPath -Encoding UTF8
        Write-StructuredLog -Message "Metrics report saved to $OutputPath" -Level Info

        if ($ExportDashboard) {
            Write-StructuredLog -Message "Dashboard exported to $DashboardPath" -Level Info
        }

        $duration = ((Get-Date) - $startTime).TotalSeconds
        Write-StructuredLog -Message "LiveMetricsDashboard complete. Duration: $([Math]::Round($duration, 2))s, Iterations: $iterations" -Level Info

        return $report

    } catch {
        Write-StructuredLog -Message "LiveMetricsDashboard error: $_" -Level Error
        throw
    }
}

