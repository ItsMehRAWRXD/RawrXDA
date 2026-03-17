#requires -Version 7.0
using namespace System.Collections.Concurrent

<#
.SYNOPSIS
    Model Invocation Action Executioner Failure Detector
.DESCRIPTION
    Production-grade failure detection, circuit breaking, and recovery system
    for AI model invocations with comprehensive telemetry and alerting.
#>

# ============================================
# FAILURE DETECTOR CONFIGURATION
# ============================================

class FailureDetectorConfig {
    [int]$FailureThreshold = 5
    [int]$SuccessThreshold = 3
    [TimeSpan]$WindowDuration = [TimeSpan]::FromMinutes(5)
    [TimeSpan]$RecoveryTimeout = [TimeSpan]::FromMinutes(2)
    [TimeSpan]$HalfOpenRetryInterval = [TimeSpan]::FromSeconds(30)
    [double]$FailureRateThreshold = 0.5
    [int]$MinimumRequests = 10
    [bool]$EnableAutoRecovery = $true
    [bool]$EnableAlerts = $true
}

enum CircuitState {
    Closed      # Normal operation - requests flow through
    Open        # Failing - requests are rejected immediately
    HalfOpen    # Testing - limited requests allowed to test recovery
}

enum FailureType {
    Timeout
    ConnectionError
    ModelNotFound
    RateLimitExceeded
    InvalidResponse
    OutOfMemory
    GPUError
    TokenLimitExceeded
    AuthenticationError
    UnknownError
}

# ============================================
# FAILURE EVENT & METRICS
# ============================================

class FailureEvent {
    [Guid]$Id = [Guid]::NewGuid()
    [DateTime]$Timestamp = [DateTime]::UtcNow
    [string]$ModelName
    [string]$BackendType  # Ollama, LlamaCpp, OpenAI, etc.
    [FailureType]$FailureType
    [string]$ErrorMessage
    [TimeSpan]$Duration
    [hashtable]$Context = @{}
    [string]$StackTrace
    [bool]$Recovered = $false
    [string]$RecoveryAction
}

class InvocationMetrics {
    [int]$TotalRequests = 0
    [int]$SuccessfulRequests = 0
    [int]$FailedRequests = 0
    [int]$TimeoutCount = 0
    [int]$CircuitBreakerTrips = 0
    [double]$AverageLatencyMs = 0
    [double]$P95LatencyMs = 0
    [double]$P99LatencyMs = 0
    [DateTime]$LastSuccess
    [DateTime]$LastFailure
    
    [double] GetSuccessRate() {
        if ($this.TotalRequests -eq 0) { return 0 }
        return [Math]::Round($this.SuccessfulRequests / $this.TotalRequests * 100, 2)
    }
    
    [double] GetFailureRate() {
        if ($this.TotalRequests -eq 0) { return 0 }
        return [Math]::Round($this.FailedRequests / $this.TotalRequests * 100, 2)
    }
}

# ============================================
# MODEL CIRCUIT BREAKER
# ============================================

class ModelCircuitBreaker {
    [string]$ModelName
    [string]$BackendType
    [CircuitState]$State = [CircuitState]::Closed
    [FailureDetectorConfig]$Config
    [ConcurrentQueue[FailureEvent]]$FailureWindow
    [int]$ConsecutiveFailures = 0
    [int]$ConsecutiveSuccesses = 0
    [DateTime]$LastStateChange = [DateTime]::UtcNow
    [DateTime]$NextRetryTime = [DateTime]::MinValue
    [InvocationMetrics]$Metrics
    
    ModelCircuitBreaker([string]$modelName, [string]$backendType) {
        $this.ModelName = $modelName
        $this.BackendType = $backendType
        $this.Config = [FailureDetectorConfig]::new()
        $this.FailureWindow = [ConcurrentQueue[FailureEvent]]::new()
        $this.Metrics = [InvocationMetrics]::new()
    }
    
    [bool] AllowRequest() {
        switch ($this.State) {
            'Closed' {
                return $true
            }
            'Open' {
                if ([DateTime]::UtcNow -ge $this.NextRetryTime) {
                    $this.TransitionTo([CircuitState]::HalfOpen)
                    return $true
                }
                return $false
            }
            'HalfOpen' {
                return $true
            }
        }
        return $false
    }
    
    [void] RecordSuccess([TimeSpan]$duration) {
        $this.Metrics.TotalRequests++
        $this.Metrics.SuccessfulRequests++
        $this.Metrics.LastSuccess = [DateTime]::UtcNow
        $this.UpdateLatencyMetrics($duration.TotalMilliseconds)
        
        $this.ConsecutiveSuccesses++
        $this.ConsecutiveFailures = 0
        
        if ($this.State -eq [CircuitState]::HalfOpen) {
            if ($this.ConsecutiveSuccesses -ge $this.Config.SuccessThreshold) {
                $this.TransitionTo([CircuitState]::Closed)
            }
        }
    }
    
    [void] RecordFailure([FailureEvent]$failure) {
        $this.Metrics.TotalRequests++
        $this.Metrics.FailedRequests++
        $this.Metrics.LastFailure = [DateTime]::UtcNow
        
        if ($failure.FailureType -eq [FailureType]::Timeout) {
            $this.Metrics.TimeoutCount++
        }
        
        $this.ConsecutiveFailures++
        $this.ConsecutiveSuccesses = 0
        
        # Add to failure window
        $this.FailureWindow.Enqueue($failure)
        $this.CleanupOldFailures()
        
        # Check if circuit should trip
        switch ($this.State) {
            'Closed' {
                if ($this.ShouldTripCircuit()) {
                    $this.TransitionTo([CircuitState]::Open)
                }
            }
            'HalfOpen' {
                $this.TransitionTo([CircuitState]::Open)
            }
        }
    }
    
    [bool] ShouldTripCircuit() {
        # Trip if consecutive failures exceed threshold
        if ($this.ConsecutiveFailures -ge $this.Config.FailureThreshold) {
            return $true
        }
        
        # Trip if failure rate exceeds threshold (with minimum requests)
        if ($this.Metrics.TotalRequests -ge $this.Config.MinimumRequests) {
            $recentFailureRate = $this.CalculateRecentFailureRate()
            if ($recentFailureRate -ge $this.Config.FailureRateThreshold) {
                return $true
            }
        }
        
        return $false
    }
    
    [double] CalculateRecentFailureRate() {
        $windowStart = [DateTime]::UtcNow - $this.Config.WindowDuration
        $recentFailures = @($this.FailureWindow.ToArray() | Where-Object { $_.Timestamp -gt $windowStart })
        
        if ($recentFailures.Count -eq 0) { return 0 }
        
        return $recentFailures.Count / $this.Config.MinimumRequests
    }
    
    [void] TransitionTo([CircuitState]$newState) {
        $oldState = $this.State
        $this.State = $newState
        $this.LastStateChange = [DateTime]::UtcNow
        
        if ($newState -eq [CircuitState]::Open) {
            $this.NextRetryTime = [DateTime]::UtcNow + $this.Config.RecoveryTimeout
            $this.Metrics.CircuitBreakerTrips++
        }
        
        if ($newState -eq [CircuitState]::Closed) {
            $this.ConsecutiveFailures = 0
            $this.ConsecutiveSuccesses = 0
        }
        
        # Log state transition
        Write-Host "[$($this.ModelName)] Circuit breaker: $oldState -> $newState" -ForegroundColor $(
            if ($newState -eq 'Open') { 'Red' }
            elseif ($newState -eq 'Closed') { 'Green' }
            else { 'Yellow' }
        )
    }
    
    [void] CleanupOldFailures() {
        $windowStart = [DateTime]::UtcNow - $this.Config.WindowDuration
        $temp = [ConcurrentQueue[FailureEvent]]::new()
        
        foreach ($failure in $this.FailureWindow.ToArray()) {
            if ($failure.Timestamp -gt $windowStart) {
                $temp.Enqueue($failure)
            }
        }
        
        $this.FailureWindow = $temp
    }
    
    [void] UpdateLatencyMetrics([double]$latencyMs) {
        # Simple exponential moving average
        if ($this.Metrics.AverageLatencyMs -eq 0) {
            $this.Metrics.AverageLatencyMs = $latencyMs
        }
        else {
            $alpha = 0.1
            $this.Metrics.AverageLatencyMs = $alpha * $latencyMs + (1 - $alpha) * $this.Metrics.AverageLatencyMs
        }
    }
    
    [void] Reset() {
        $this.State = [CircuitState]::Closed
        $this.ConsecutiveFailures = 0
        $this.ConsecutiveSuccesses = 0
        $this.FailureWindow = [ConcurrentQueue[FailureEvent]]::new()
    }
}

# ============================================
# FAILURE DETECTOR SERVICE
# ============================================

class ModelInvocationFailureDetector {
    [ConcurrentDictionary[string, ModelCircuitBreaker]]$CircuitBreakers
    [ConcurrentQueue[FailureEvent]]$GlobalFailureLog
    [FailureDetectorConfig]$DefaultConfig
    [hashtable]$RecoveryStrategies
    [bool]$IsRunning = $true
    
    ModelInvocationFailureDetector() {
        $this.CircuitBreakers = [ConcurrentDictionary[string, ModelCircuitBreaker]]::new()
        $this.GlobalFailureLog = [ConcurrentQueue[FailureEvent]]::new()
        $this.DefaultConfig = [FailureDetectorConfig]::new()
        $this.InitializeRecoveryStrategies()
    }
    
    [void] InitializeRecoveryStrategies() {
        $this.RecoveryStrategies = @{
            'Timeout' = {
                param($context)
                # Double timeout for next request
                $context.Timeout = [Math]::Min($context.Timeout * 2, 300000)
                return @{ Action = "IncreaseTimeout"; NewTimeout = $context.Timeout }
            }
            'ConnectionError' = {
                param($context)
                # Try alternative endpoint
                return @{ Action = "RetryWithBackoff"; BackoffMs = 5000 }
            }
            'RateLimitExceeded' = {
                param($context)
                # Wait and retry
                return @{ Action = "WaitAndRetry"; WaitMs = 60000 }
            }
            'OutOfMemory' = {
                param($context)
                # Reduce batch size or use smaller model
                return @{ Action = "ReduceLoad"; SuggestSmallerModel = $true }
            }
            'GPUError' = {
                param($context)
                # Fall back to CPU
                return @{ Action = "FallbackToCPU" }
            }
            'TokenLimitExceeded' = {
                param($context)
                # Truncate input
                return @{ Action = "TruncateInput"; MaxTokens = $context.MaxTokens / 2 }
            }
        }
    }
    
    [ModelCircuitBreaker] GetOrCreateCircuitBreaker([string]$modelName, [string]$backendType) {
        $key = "$backendType`:$modelName"
        
        return $this.CircuitBreakers.GetOrAdd($key, {
            param($k)
            [ModelCircuitBreaker]::new($modelName, $backendType)
        })
    }
    
    [hashtable] ExecuteWithFailureDetection([string]$modelName, [string]$backendType, [scriptblock]$action, [hashtable]$context = @{}) {
        $circuitBreaker = $this.GetOrCreateCircuitBreaker($modelName, $backendType)
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        
        # Check if request is allowed
        if (-not $circuitBreaker.AllowRequest()) {
            return @{
                Success = $false
                Error = "Circuit breaker is OPEN for $modelName"
                CircuitState = $circuitBreaker.State
                NextRetryTime = $circuitBreaker.NextRetryTime
                ShouldRetry = $false
            }
        }
        
        try {
            # Execute the action
            $result = & $action
            $stopwatch.Stop()
            
            # Record success
            $circuitBreaker.RecordSuccess($stopwatch.Elapsed)
            
            return @{
                Success = $true
                Result = $result
                Duration = $stopwatch.Elapsed
                CircuitState = $circuitBreaker.State
            }
        }
        catch {
            $stopwatch.Stop()
            
            # Classify failure
            $failureType = $this.ClassifyFailure($_.Exception)
            
            # Create failure event
            $failure = [FailureEvent]@{
                ModelName = $modelName
                BackendType = $backendType
                FailureType = $failureType
                ErrorMessage = $_.Exception.Message
                Duration = $stopwatch.Elapsed
                Context = $context
                StackTrace = $_.ScriptStackTrace
            }
            
            # Record failure
            $circuitBreaker.RecordFailure($failure)
            $this.GlobalFailureLog.Enqueue($failure)
            
            # Get recovery strategy
            $recovery = $this.GetRecoveryStrategy($failureType, $context)
            
            return @{
                Success = $false
                Error = $_.Exception.Message
                FailureType = $failureType
                Duration = $stopwatch.Elapsed
                CircuitState = $circuitBreaker.State
                Recovery = $recovery
                ShouldRetry = $recovery.Action -ne "None"
            }
        }
    }
    
    [FailureType] ClassifyFailure([Exception]$exception) {
        $message = $exception.Message.ToLower()
        
        if ($message -match 'timeout|timed out') {
            return [FailureType]::Timeout
        }
        elseif ($message -match 'connection|network|unreachable|refused') {
            return [FailureType]::ConnectionError
        }
        elseif ($message -match 'model not found|no such model') {
            return [FailureType]::ModelNotFound
        }
        elseif ($message -match 'rate limit|too many requests|429') {
            return [FailureType]::RateLimitExceeded
        }
        elseif ($message -match 'invalid response|parse error|json') {
            return [FailureType]::InvalidResponse
        }
        elseif ($message -match 'out of memory|oom|cuda memory') {
            return [FailureType]::OutOfMemory
        }
        elseif ($message -match 'gpu|cuda|rocm|device') {
            return [FailureType]::GPUError
        }
        elseif ($message -match 'token|context length|max.*token') {
            return [FailureType]::TokenLimitExceeded
        }
        elseif ($message -match 'auth|unauthorized|401|403') {
            return [FailureType]::AuthenticationError
        }
        
        return [FailureType]::UnknownError
    }
    
    [hashtable] GetRecoveryStrategy([FailureType]$failureType, [hashtable]$context) {
        $strategy = $this.RecoveryStrategies[$failureType.ToString()]
        
        if ($strategy) {
            return & $strategy $context
        }
        
        return @{ Action = "None" }
    }
    
    [hashtable] GetHealthStatus() {
        $allBreakers = $this.CircuitBreakers.Values
        $openCount = @($allBreakers | Where-Object { $_.State -eq 'Open' }).Count
        $halfOpenCount = @($allBreakers | Where-Object { $_.State -eq 'HalfOpen' }).Count
        $closedCount = @($allBreakers | Where-Object { $_.State -eq 'Closed' }).Count
        
        $totalFailures = $this.GlobalFailureLog.Count
        $recentFailures = @($this.GlobalFailureLog.ToArray() | 
            Where-Object { $_.Timestamp -gt ([DateTime]::UtcNow.AddMinutes(-5)) }).Count
        
        $overallHealth = if ($openCount -gt 0) { "Degraded" }
                        elseif ($halfOpenCount -gt 0) { "Recovering" }
                        else { "Healthy" }
        
        return @{
            OverallHealth = $overallHealth
            CircuitBreakers = @{
                Total = $allBreakers.Count
                Open = $openCount
                HalfOpen = $halfOpenCount
                Closed = $closedCount
            }
            Failures = @{
                Total = $totalFailures
                Last5Minutes = $recentFailures
            }
            Models = $allBreakers | ForEach-Object {
                @{
                    Name = $_.ModelName
                    Backend = $_.BackendType
                    State = $_.State.ToString()
                    SuccessRate = $_.Metrics.GetSuccessRate()
                    ConsecutiveFailures = $_.ConsecutiveFailures
                    LastFailure = $_.Metrics.LastFailure
                }
            }
        }
    }
    
    [FailureEvent[]] GetRecentFailures([int]$count = 10) {
        return $this.GlobalFailureLog.ToArray() | 
            Sort-Object Timestamp -Descending | 
            Select-Object -First $count
    }
    
    [void] ResetCircuitBreaker([string]$modelName, [string]$backendType) {
        $key = "$backendType`:$modelName"
        
        if ($this.CircuitBreakers.ContainsKey($key)) {
            $this.CircuitBreakers[$key].Reset()
            Write-Host "[$modelName] Circuit breaker manually reset" -ForegroundColor Green
        }
    }
    
    [void] ResetAllCircuitBreakers() {
        foreach ($breaker in $this.CircuitBreakers.Values) {
            $breaker.Reset()
        }
        Write-Host "All circuit breakers reset" -ForegroundColor Green
    }
}

# ============================================
# RETRY EXECUTOR WITH FAILURE DETECTION
# ============================================

class RetryExecutor {
    [ModelInvocationFailureDetector]$FailureDetector
    [int]$MaxRetries = 3
    [int]$BaseDelayMs = 1000
    [double]$BackoffMultiplier = 2.0
    [int]$MaxDelayMs = 30000
    
    RetryExecutor([ModelInvocationFailureDetector]$detector) {
        $this.FailureDetector = $detector
    }
    
    [hashtable] ExecuteWithRetry([string]$modelName, [string]$backendType, [scriptblock]$action, [hashtable]$context = @{}) {
        $attempt = 0
        $lastResult = $null
        $delayMs = $this.BaseDelayMs
        
        while ($attempt -lt $this.MaxRetries) {
            $attempt++
            
            $result = $this.FailureDetector.ExecuteWithFailureDetection(
                $modelName, 
                $backendType, 
                $action, 
                $context
            )
            
            if ($result.Success) {
                return @{
                    Success = $true
                    Result = $result.Result
                    Attempts = $attempt
                    TotalDuration = $result.Duration
                }
            }
            
            $lastResult = $result
            
            # Check if we should retry
            if (-not $result.ShouldRetry) {
                break
            }
            
            # Apply recovery strategy delay
            if ($result.Recovery.WaitMs) {
                $delayMs = [Math]::Max($delayMs, $result.Recovery.WaitMs)
            }
            
            # Exponential backoff
            if ($attempt -lt $this.MaxRetries) {
                Write-Host "  Retry $attempt/$($this.MaxRetries) in $delayMs ms..." -ForegroundColor Yellow
                Start-Sleep -Milliseconds $delayMs
                $delayMs = [Math]::Min($delayMs * $this.BackoffMultiplier, $this.MaxDelayMs)
            }
        }
        
        return @{
            Success = $false
            Error = $lastResult.Error
            FailureType = $lastResult.FailureType
            Attempts = $attempt
            CircuitState = $lastResult.CircuitState
            Recovery = $lastResult.Recovery
        }
    }
}

# ============================================
# GLOBAL INSTANCE & HELPER FUNCTIONS
# ============================================

$script:GlobalFailureDetector = $null

function Get-FailureDetector {
    if (-not $script:GlobalFailureDetector) {
        $script:GlobalFailureDetector = [ModelInvocationFailureDetector]::new()
    }
    return $script:GlobalFailureDetector
}

function Invoke-ModelWithFailureDetection {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ModelName,
        
        [Parameter(Mandatory)]
        [string]$BackendType,
        
        [Parameter(Mandatory)]
        [scriptblock]$Action,
        
        [Parameter()]
        [hashtable]$Context = @{},
        
        [Parameter()]
        [switch]$EnableRetry,
        
        [Parameter()]
        [int]$MaxRetries = 3
    )
    
    $detector = Get-FailureDetector
    
    if ($EnableRetry) {
        $executor = [RetryExecutor]::new($detector)
        $executor.MaxRetries = $MaxRetries
        return $executor.ExecuteWithRetry($ModelName, $BackendType, $Action, $Context)
    }
    else {
        return $detector.ExecuteWithFailureDetection($ModelName, $BackendType, $Action, $Context)
    }
}

function Get-ModelHealth {
    [CmdletBinding()]
    param()
    
    $detector = Get-FailureDetector
    return $detector.GetHealthStatus()
}

function Get-RecentModelFailures {
    [CmdletBinding()]
    param(
        [Parameter()]
        [int]$Count = 10
    )
    
    $detector = Get-FailureDetector
    return $detector.GetRecentFailures($Count)
}

function Reset-ModelCircuitBreaker {
    [CmdletBinding()]
    param(
        [Parameter()]
        [string]$ModelName,
        
        [Parameter()]
        [string]$BackendType,
        
        [Parameter()]
        [switch]$All
    )
    
    $detector = Get-FailureDetector
    
    if ($All) {
        $detector.ResetAllCircuitBreakers()
    }
    elseif ($ModelName -and $BackendType) {
        $detector.ResetCircuitBreaker($ModelName, $BackendType)
    }
    else {
        Write-Warning "Specify -ModelName and -BackendType, or use -All"
    }
}

function Get-AllModelHealth {
    [CmdletBinding()]
    param()
    
    $detector = Get-FailureDetector
    $health = $detector.GetHealthStatus()
    return $health.Models
}

function Reset-AllModelCircuitBreakers {
    [CmdletBinding()]
    param()
    
    $detector = Get-FailureDetector
    $detector.ResetAllCircuitBreakers()
}

# Export functions
Export-ModuleMember -Function @(
    'Get-FailureDetector',
    'Invoke-ModelWithFailureDetection',
    'Get-ModelHealth',
    'Get-AllModelHealth',
    'Get-RecentModelFailures',
    'Reset-ModelCircuitBreaker',
    'Reset-AllModelCircuitBreakers'
)
