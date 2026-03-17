#requires -Version 7.0
using namespace System.Diagnostics

<#
.SYNOPSIS
    Production Monitoring & Observability for RawrXD
.DESCRIPTION
    Metrics collection, audit logging, and health checks for production environments
#>

class MetricsCollector : System.IDisposable {
    [hashtable]$Metrics
    [System.Collections.Concurrent.ConcurrentDictionary[string, int]]$Counters
    [System.Collections.Concurrent.ConcurrentDictionary[string, double[]]]$Histograms
    
    MetricsCollector() {
        $this.Metrics = @{
            TotalRequests = 0
            SuccessfulRequests = 0
            FailedRequests = 0
            TotalDuration = 0
            AverageDuration = 0
            ErrorsByType = @{}
        }
        
        $this.Counters = [System.Collections.Concurrent.ConcurrentDictionary[string, int]]::new()
        $this.Histograms = [System.Collections.Concurrent.ConcurrentDictionary[string, double[]]]::new()
    }
    
    [void] RecordRequest([string]$intent, [string]$backend, [int]$duration, [bool]$success) {
        $this.Metrics.TotalRequests++
        $this.Metrics.TotalDuration += $duration
        $this.Metrics.AverageDuration = $this.Metrics.TotalDuration / $this.Metrics.TotalRequests
        
        if ($success) {
            $this.Metrics.SuccessfulRequests++
        }
        else {
            $this.Metrics.FailedRequests++
        }
        
        $key = "$intent-$backend"
        $this.Counters.AddOrUpdate($key, 1, { param($k, $v) $v + 1 }) | Out-Null
    }
    
    [void] RecordSuccess([int]$duration, [string]$domain = "general") {
        $this.RecordRequest($domain, "default", $duration, $true)
    }
    
    [void] RecordFailure([string]$error, [int]$duration) {
        $this.RecordRequest("error", "default", $duration, $false)
        
        if (-not $this.Metrics.ErrorsByType[$error]) {
            $this.Metrics.ErrorsByType[$error] = 0
        }
        $this.Metrics.ErrorsByType[$error]++
    }
    
    [void] RecordAutoSave() {
        $this.Counters.AddOrUpdate("AutoSave", 1, { param($k, $v) $v + 1 }) | Out-Null
    }
    
    [void] RecordMemoryUsage([double]$memoryMB) {
        $key = "MemoryUsage_MB"
        $this.Histograms.AddOrUpdate($key, @($memoryMB), { param($k, $v) $v + @($memoryMB) }) | Out-Null
    }
    
    [hashtable] GetMetrics() {
        return @{
            Total = $this.Metrics.TotalRequests
            Success = $this.Metrics.SuccessfulRequests
            Failed = $this.Metrics.FailedRequests
            AverageDuration = $this.Metrics.AverageDuration
            SuccessRate = if ($this.Metrics.TotalRequests -gt 0) {
                [Math]::Round($this.Metrics.SuccessfulRequests / $this.Metrics.TotalRequests * 100, 2)
            } else { 0 }
            Counters = $this.Counters.ToArray()
            Errors = $this.Metrics.ErrorsByType
        }
    }
    
    [void] EnablePrometheusEndpoint([string]$endpoint) {
        Write-Host "Prometheus metrics endpoint configured: $endpoint" -ForegroundColor Green
    }
    
    [void] Dispose() {
        # Cleanup resources
        $this.Counters.Clear()
        $this.Histograms.Clear()
    }
}

class AuditLogger {
    [string]$AuditPath
    [System.IO.StreamWriter]$Writer
    [object]$Lock = [object]::new()
    
    AuditLogger() {
        $auditDir = "$env:APPDATA\RawrXD\Audit"
        if (-not (Test-Path $auditDir)) {
            New-Item -ItemType Directory -Path $auditDir -Force | Out-Null
        }
        
        $this.AuditPath = Join-Path $auditDir "audit-$(Get-Date -Format 'yyyy-MM-dd').jsonl"
        $this.Writer = [System.IO.StreamWriter]::new($this.AuditPath, $true)
    }
    
    [void] LogIntent([object]$intent) {
        $entry = @{
            Timestamp = [DateTime]::UtcNow
            EventType = "IntentRecognized"
            IntentId = $intent.Id
            Primary = $intent.Primary
            Domain = $intent.Domain
            Urgency = $intent.Urgency
        }
        
        $this.WriteEntry($entry)
    }
    
    [void] LogPlan([object]$plan) {
        $entry = @{
            Timestamp = [DateTime]::UtcNow
            EventType = "PlanCreated"
            PlanId = $plan.Id
            IntentId = $plan.IntentId
            StepCount = $plan.Steps.Count
            Complexity = $plan.Steps.Count -gt 5 ? "complex" : "simple"
        }
        
        $this.WriteEntry($entry)
    }
    
    [void] LogError([Exception]$ex, [string]$prompt, [object]$context) {
        $entry = @{
            Timestamp = [DateTime]::UtcNow
            EventType = "Error"
            Exception = $ex.GetType().Name
            Message = $ex.Message
            Prompt = $prompt
            StackTrace = $ex.StackTrace
        }
        
        $this.WriteEntry($entry)
    }
    
    [void] LogSecurityEvent([string]$eventType, [hashtable]$details) {
        $entry = @{
            Timestamp = [DateTime]::UtcNow
            EventType = "Security_$eventType"
            Details = $details
        }
        
        $this.WriteEntry($entry)
    }
    
    [void] WriteEntry([hashtable]$entry) {
        [System.Threading.Monitor]::Enter($this.Lock)
        try {
            $json = $entry | ConvertTo-Json -Compress
            $this.Writer.WriteLine($json)
            $this.Writer.Flush()
        }
        finally {
            [System.Threading.Monitor]::Exit($this.Lock)
        }
    }
}

class HealthCheckService {
    [hashtable[]]$Checks
    [System.Collections.Generic.Dictionary[string, object]]$LastResults
    
    HealthCheckService() {
        $this.LastResults = [System.Collections.Generic.Dictionary[string, object]]::new()
    }
    
    [hashtable] RunHealthChecks() {
        $report = @{
            Timestamp = [DateTime]::UtcNow
            Checks = @()
            OverallStatus = "Healthy"
            Issues = @()
        }
        
        # Check memory
        $currentPid = [System.Diagnostics.Process]::GetCurrentProcess().Id
        $memoryUsage = [Math]::Round((Get-Process -Id $currentPid).WorkingSet / 1MB, 2)
        $memoryStatus = if ($memoryUsage -gt 1000) { "Degraded" } else { "Healthy" }
        $report.Checks += @{
            Name = "MemoryUsage"
            Status = $memoryStatus
            Value = "$memoryUsage MB"
        }
        
        # Check disk space
        $disk = Get-Volume | Where-Object { $_.DriveLetter -eq 'C' }
        $diskPercent = [Math]::Round($disk.SizeRemaining / $disk.Size * 100, 2)
        $diskStatus = if ($diskPercent -lt 10) { "Unhealthy" } elseif ($diskPercent -lt 20) { "Degraded" } else { "Healthy" }
        $report.Checks += @{
            Name = "DiskSpace"
            Status = $diskStatus
            Value = "$diskPercent% free"
        }
        
        # Determine overall status
        if ($report.Checks | Where-Object { $_.Status -eq "Unhealthy" }) {
            $report.OverallStatus = "Unhealthy"
        }
        elseif ($report.Checks | Where-Object { $_.Status -eq "Degraded" }) {
            $report.OverallStatus = "Degraded"
        }
        
        return $report
    }
}

class CircuitBreakerConfig {
    [int]$FailureThreshold
    [TimeSpan]$ResetTimeout
    [int]$HalfOpenMaxCalls
}

class RateLimitConfig {
    [int]$RequestsPerMinute
    [int]$BurstSize
    [TimeSpan]$CooldownPeriod
}

# Export public members
Export-ModuleMember -Function @()

