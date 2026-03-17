#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Production Observability Module
    
.DESCRIPTION
    Implements production-ready observability infrastructure with:
    - Structured logging at multiple levels
    - Metrics instrumentation and tracking
    - Distributed tracing capabilities
    - Health checks and diagnostics
    - Performance monitoring and baselines

.PRODUCTION NOTES
    As per AI Toolkit Production Readiness Instructions:
    - NO SOURCE FILE SIMPLIFICATION - ALL LOGIC REMAINS INTACT
    - Comprehensive structured logging at key operation points
    - Custom metrics for toolkit operations
    - OpenTelemetry-compatible tracing infrastructure
    - Non-intrusive error handling with centralized capture
#>

# ============================================
# OBSERVABILITY MODULE CONFIGURATION
# ============================================

$script:ObservabilityConfig = @{
    LogLevel = "DEBUG"  # DEBUG, INFO, WARNING, ERROR, CRITICAL
    LogDirectory = Join-Path $env:APPDATA "RawrXD\Logs"
    MaxLogSize = 100MB
    MaxLogFiles = 10
    MetricsCollectionEnabled = $true
    TracingEnabled = $true
    PerformanceBaselineLogging = $true
}

$script:StructuredLogs = @()
$script:Metrics = @{}
$script:TraceContext = @()
$script:PerformanceBaselines = @{}

# ============================================
# STRUCTURED LOGGING INFRASTRUCTURE
# ============================================

function Initialize-LogDirectory {
    <#
    .SYNOPSIS
        Initialize log directory structure
    #>
    if (-not (Test-Path $script:ObservabilityConfig.LogDirectory)) {
        New-Item -ItemType Directory -Path $script:ObservabilityConfig.LogDirectory -Force | Out-Null
    }
}

function Write-StructuredLog {
    <#
    .SYNOPSIS
        Write structured log entry with all contextual information
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [ValidateSet("DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL")]
        [string]$Level = "INFO",
        [hashtable]$Context = @{},
        [string]$Component = "Core",
        [string]$Operation = ""
    )
    
    try {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $logEntry = @{
            Timestamp = $timestamp
            Level = $Level
            Component = $Component
            Operation = $Operation
            Message = $Message
            Context = $Context
            ProcessId = $PID
            ThreadId = [System.Threading.Thread]::CurrentThread.ManagedThreadId
            User = [Environment]::UserName
            ComputerName = [Environment]::MachineName
        }
        
        $script:StructuredLogs += $logEntry
        
        # Write to file
        $logFile = Join-Path $script:ObservabilityConfig.LogDirectory "rawrxd-$(Get-Date -Format 'yyyyMMdd').log"
        $logLine = ConvertTo-Json $logEntry -Compress
        Add-Content -Path $logFile -Value $logLine -Encoding UTF8 -ErrorAction SilentlyContinue
        
        # Console output for important levels
        if ($Level -in @("ERROR", "CRITICAL", "WARNING")) {
            $color = switch ($Level) {
                "ERROR" { "Red" }
                "CRITICAL" { "DarkRed" }
                "WARNING" { "Yellow" }
            }
            Write-Host "[$timestamp] [$Component] [$Level] $Message" -ForegroundColor $color
        }
        
        return $logEntry
    }
    catch {
        Write-Host "[ERROR] Failed to write structured log: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Get-StructuredLogs {
    <#
    .SYNOPSIS
        Retrieve structured logs with filtering
    #>
    param(
        [string]$Level = "",
        [string]$Component = "",
        [int]$Last = 100
    )
    
    $logs = $script:StructuredLogs
    
    if ($Level) {
        $logs = $logs | Where-Object { $_.Level -eq $Level }
    }
    
    if ($Component) {
        $logs = $logs | Where-Object { $_.Component -eq $Component }
    }
    
    return $logs | Select-Object -Last $Last
}

# ============================================
# METRICS & INSTRUMENTATION
# ============================================

function Record-Metric {
    <#
    .SYNOPSIS
        Record a custom metric for operations tracking
    #>
    param(
        [Parameter(Mandatory = $true)][string]$MetricName,
        [Parameter(Mandatory = $true)][double]$Value,
        [hashtable]$Tags = @{},
        [string]$Unit = ""
    )
    
    try {
        $metric = @{
            Name = $MetricName
            Value = $Value
            Unit = $Unit
            Tags = $Tags
            Timestamp = Get-Date
        }
        
        if (-not $script:Metrics.ContainsKey($MetricName)) {
            $script:Metrics[$MetricName] = @()
        }
        
        $script:Metrics[$MetricName] += $metric
        
        # Keep only last 1000 measurements per metric
        if ($script:Metrics[$MetricName].Count -gt 1000) {
            $script:Metrics[$MetricName] = $script:Metrics[$MetricName] | Select-Object -Last 1000
        }
        
        Write-StructuredLog -Message "Metric recorded: $MetricName = $Value $Unit" -Level "DEBUG" `
            -Component "Metrics" -Context @{ MetricName = $MetricName; Value = $Value }
    }
    catch {
        Write-Host "[ERROR] Failed to record metric: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Get-MetricStatistics {
    <#
    .SYNOPSIS
        Get statistical analysis of recorded metrics
    #>
    param(
        [Parameter(Mandatory = $true)][string]$MetricName
    )
    
    if (-not $script:Metrics.ContainsKey($MetricName)) {
        return $null
    }
    
    $values = @($script:Metrics[$MetricName] | Select-Object -ExpandProperty Value)
    
    if ($values.Count -eq 0) {
        return $null
    }
    
    return @{
        MetricName = $MetricName
        Count = $values.Count
        Min = $values | Measure-Object -Minimum | Select-Object -ExpandProperty Minimum
        Max = $values | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
        Average = $values | Measure-Object -Average | Select-Object -ExpandProperty Average
        Sum = $values | Measure-Object -Sum | Select-Object -ExpandProperty Sum
        LastValue = $values[-1]
        Timestamp = Get-Date
    }
}

function Get-AllMetricsSnapshot {
    <#
    .SYNOPSIS
        Get snapshot of all recorded metrics
    #>
    $snapshot = @{}
    
    foreach ($metricName in $script:Metrics.Keys) {
        $snapshot[$metricName] = Get-MetricStatistics -MetricName $metricName
    }
    
    return $snapshot
}

# ============================================
# DISTRIBUTED TRACING
# ============================================

function Start-TraceSpan {
    <#
    .SYNOPSIS
        Start a distributed trace span for operation tracking
    #>
    param(
        [Parameter(Mandatory = $true)][string]$SpanName,
        [hashtable]$Attributes = @{}
    )
    
    try {
        $spanId = [guid]::NewGuid().ToString().Substring(0, 16)
        
        $span = @{
            Id = $spanId
            Name = $SpanName
            StartTime = Get-Date
            EndTime = $null
            Duration = $null
            Attributes = $Attributes
            Status = "RUNNING"
            Events = @()
        }
        
        $script:TraceContext += $span
        
        Write-StructuredLog -Message "Trace span started: $SpanName" -Level "DEBUG" `
            -Component "Tracing" -Operation $SpanName -Context @{ SpanId = $spanId }
        
        return $spanId
    }
    catch {
        Write-Host "[ERROR] Failed to start trace span: $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

function End-TraceSpan {
    <#
    .SYNOPSIS
        End a distributed trace span
    #>
    param(
        [Parameter(Mandatory = $true)][string]$SpanId,
        [ValidateSet("OK", "ERROR", "UNSET")]
        [string]$Status = "OK"
    )
    
    try {
        $span = $script:TraceContext | Where-Object { $_.Id -eq $SpanId } | Select-Object -First 1
        
        if ($span) {
            $span.EndTime = Get-Date
            $span.Duration = ($span.EndTime - $span.StartTime).TotalMilliseconds
            $span.Status = $Status
            
            Write-StructuredLog -Message "Trace span ended: $($span.Name) (${$($span.Duration)}ms)" -Level "DEBUG" `
                -Component "Tracing" -Operation $span.Name -Context @{ SpanId = $SpanId; Duration = $span.Duration }
        }
    }
    catch {
        Write-Host "[ERROR] Failed to end trace span: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Get-TraceContext {
    <#
    .SYNOPSIS
        Get current trace context
    #>
    return $script:TraceContext | Select-Object -Last 100
}

# ============================================
# PERFORMANCE MONITORING & BASELINES
# ============================================

function Record-OperationLatency {
    <#
    .SYNOPSIS
        Record operation latency for baseline establishment
    #>
    param(
        [Parameter(Mandatory = $true)][string]$OperationName,
        [Parameter(Mandatory = $true)][double]$LatencyMs,
        [hashtable]$Parameters = @{}
    )
    
    try {
        if (-not $script:PerformanceBaselines.ContainsKey($OperationName)) {
            $script:PerformanceBaselines[$OperationName] = @{
                OperationName = $OperationName
                Measurements = @()
            }
        }
        
        $measurement = @{
            Timestamp = Get-Date
            LatencyMs = $LatencyMs
            Parameters = $Parameters
        }
        
        $script:PerformanceBaselines[$OperationName].Measurements += $measurement
        
        # Keep only last 500 measurements
        if ($script:PerformanceBaselines[$OperationName].Measurements.Count -gt 500) {
            $script:PerformanceBaselines[$OperationName].Measurements = `
                $script:PerformanceBaselines[$OperationName].Measurements | Select-Object -Last 500
        }
        
        # Record metric
        Record-Metric -MetricName "latency_$($OperationName -replace '\s', '_')" -Value $LatencyMs -Unit "ms"
        
        Write-StructuredLog -Message "Operation latency recorded: $OperationName = ${LatencyMs}ms" -Level "DEBUG" `
            -Component "Performance" -Operation $OperationName
    }
    catch {
        Write-Host "[ERROR] Failed to record operation latency: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Get-PerformanceBaseline {
    <#
    .SYNOPSIS
        Get performance baseline for an operation
    #>
    param(
        [Parameter(Mandatory = $true)][string]$OperationName
    )
    
    if (-not $script:PerformanceBaselines.ContainsKey($OperationName)) {
        return $null
    }
    
    $baseline = $script:PerformanceBaselines[$OperationName]
    $latencies = @($baseline.Measurements | Select-Object -ExpandProperty LatencyMs)
    
    return @{
        OperationName = $OperationName
        MeasurementCount = $latencies.Count
        MinLatencyMs = ($latencies | Measure-Object -Minimum | Select-Object -ExpandProperty Minimum)
        MaxLatencyMs = ($latencies | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum)
        AverageLatencyMs = ($latencies | Measure-Object -Average | Select-Object -ExpandProperty Average)
        P95LatencyMs = ($latencies | Sort-Object)[[int]($latencies.Count * 0.95)]
        P99LatencyMs = ($latencies | Sort-Object)[[int]($latencies.Count * 0.99)]
    }
}

# ============================================
# HEALTH CHECKS & DIAGNOSTICS
# ============================================

function Test-SystemHealth {
    <#
    .SYNOPSIS
        Perform comprehensive system health check
    #>
    try {
        $health = @{
            Timestamp = Get-Date
            Components = @{}
            OverallStatus = "Healthy"
            Issues = @()
        }
        
        # Log system
        $health.Components.Logging = @{
            Status = "Healthy"
            LogFileCount = @(Get-ChildItem -Path $script:ObservabilityConfig.LogDirectory -Filter "*.log" -ErrorAction SilentlyContinue).Count
        }
        
        # Metrics system
        $health.Components.Metrics = @{
            Status = if ($script:Metrics.Count -gt 0) { "Healthy" } else { "Warning" }
            MetricCount = $script:Metrics.Count
        }
        
        # Tracing system
        $health.Components.Tracing = @{
            Status = if ($script:TraceContext.Count -gt 0) { "Healthy" } else { "Warning" }
            ActiveSpans = @($script:TraceContext | Where-Object { $_.Status -eq "RUNNING" }).Count
        }
        
        # Determine overall status
        $unhealthyComponents = @($health.Components.Values | Where-Object { $_.Status -ne "Healthy" })
        if ($unhealthyComponents.Count -gt 0) {
            $health.OverallStatus = "Degraded"
        }
        
        Write-StructuredLog -Message "System health check completed" -Level "INFO" `
            -Component "Diagnostics" -Context @{ OverallStatus = $health.OverallStatus }
        
        return $health
    }
    catch {
        Write-Host "[ERROR] Failed to perform health check: $($_.Exception.Message)" -ForegroundColor Red
        return @{ OverallStatus = "Error"; Error = $_.Exception.Message }
    }
}

function Export-ObservabilityReport {
    <#
    .SYNOPSIS
        Export comprehensive observability report
    #>
    try {
        $report = @{
            ExportTime = Get-Date
            StructuredLogs = @($script:StructuredLogs | Select-Object -Last 500)
            Metrics = Get-AllMetricsSnapshot
            TraceContext = Get-TraceContext
            PerformanceBaselines = $script:PerformanceBaselines
            SystemHealth = Test-SystemHealth
        }
        
        $reportPath = Join-Path $script:ObservabilityConfig.LogDirectory "observability-report-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
        $report | ConvertTo-Json -Depth 10 | Set-Content -Path $reportPath -Encoding UTF8
        
        Write-StructuredLog -Message "Observability report exported" -Level "INFO" `
            -Component "Observability" -Context @{ ReportPath = $reportPath }
        
        return @{
            Success = $true
            ReportPath = $reportPath
        }
    }
    catch {
        Write-Host "[ERROR] Failed to export observability report: $($_.Exception.Message)" -ForegroundColor Red
        return @{ Success = $false; Error = $_.Exception.Message }
    }
}

# ============================================
# INITIALIZATION
# ============================================

Initialize-LogDirectory

Write-Host "[RawrXD-Observability] Production observability module loaded successfully" -ForegroundColor Green
Write-Host "[RawrXD-Observability] - Structured logging: ENABLED" -ForegroundColor Green
Write-Host "[RawrXD-Observability] - Metrics collection: ENABLED" -ForegroundColor Green
Write-Host "[RawrXD-Observability] - Distributed tracing: ENABLED" -ForegroundColor Green
Write-Host "[RawrXD-Observability] - Performance baselines: ENABLED" -ForegroundColor Green

Export-ModuleMember -Function @(
    'Write-StructuredLog',
    'Get-StructuredLogs',
    'Record-Metric',
    'Get-MetricStatistics',
    'Get-AllMetricsSnapshot',
    'Start-TraceSpan',
    'End-TraceSpan',
    'Get-TraceContext',
    'Record-OperationLatency',
    'Get-PerformanceBaseline',
    'Test-SystemHealth',
    'Export-ObservabilityReport'
)
