#requires -Version 7.0

<#
.SYNOPSIS
    RawrXD 3.0 PRO - Agentic Integration Module
.DESCRIPTION
    Integrates AgenticEngine into the main IDE with streaming updates,
    security hardening, production observability, and failure detection
#>

# Import required modules
Import-Module -Name (Join-Path $PSScriptRoot "AgenticEngine.psm1") -Force -ErrorAction Stop
Import-Module -Name (Join-Path $PSScriptRoot "ProductionMonitoring.psm1") -Force -ErrorAction Stop
Import-Module -Name (Join-Path $PSScriptRoot "SecurityManager.psm1") -Force -ErrorAction Stop
Import-Module -Name (Join-Path $PSScriptRoot "ModelInvocationFailureDetector.psm1") -Force -ErrorAction Stop

# Global agentic engine instance
$script:AgenticEngine = $null
$script:SecurityAudit = $null
$script:MetricsCollector = $null
$script:AuditLogger = $null
$script:FailureDetector = $null

<#
.SYNOPSIS
    Write a startup log message
#>
function Write-StartupLog {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Message,
        
        [Parameter()]
        [ValidateSet("INFO", "SUCCESS", "WARNING", "ERROR", "DEBUG")]
        [string]$Level = "INFO"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $color = switch ($Level) {
        "SUCCESS" { "Green" }
        "ERROR" { "Red" }
        "WARNING" { "Yellow" }
        "DEBUG" { "Gray" }
        default { "Cyan" }
    }
    
    Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $color
}

<#
.SYNOPSIS
    Initialize agentic engine with all production features
#>
function Initialize-AgenticEngine {
    [CmdletBinding()]
    param()
    
    try {
        Write-StartupLog "Initializing RawrXD 3.0 PRO Agentic Engine" "INFO"
        
        $script:AgenticEngine = [AgenticEngine]::new()
        $script:MetricsCollector = [MetricsCollector]::new()
        $script:AuditLogger = [AuditLogger]::new()
        $script:FailureDetector = Get-FailureDetector
        
        Write-StartupLog "Agentic Engine initialized with $($script:AgenticEngine.Tools.Count) tools" "SUCCESS"
        Write-StartupLog "Failure Detector: Circuit breakers enabled" "SUCCESS"
        Write-StartupLog "Monitoring systems: Metrics, Audit, Security" "SUCCESS"
        
        # Start health monitoring
        Start-HealthMonitoring
        
        return $true
    }
    catch {
        Write-StartupLog "Failed to initialize agentic engine: $($_.Exception.Message)" "ERROR"
        return $false
    }
}


<#
.SYNOPSIS
    Execute agentic flow for user prompt with streaming updates
#>
function Invoke-AgenticFlow {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Prompt,
        
        [Parameter()]
        [string]$WorkingDirectory = [System.IO.Directory]::GetCurrentDirectory(),
        
        [Parameter()]
        [string[]]$OpenFiles = @(),
        
        [Parameter()]
        [scriptblock]$OnUpdate = {}
    )
    
    if (-not $script:AgenticEngine) {
        Initialize-AgenticEngine
    }
    
    try {
        $context = [AgentContext]@{
            WorkingDirectory = $WorkingDirectory
            OpenFiles = $OpenFiles
        }
        
        # Execute agentic flow
        $result = $script:AgenticEngine.ExecuteAgenticFlow($Prompt, $context)
        
        # Send updates via callback
        & $OnUpdate @{
            Status = "Completed"
            Response = $result.Response
            Success = $result.Success
            Metrics = $result.Metrics
        }
        
        return $result
    }
    catch {
        Write-StartupLog "Agentic flow failed: $($_.Exception.Message)" "ERROR"
        
        & $OnUpdate @{
            Status = "Error"
            Error = $_.Exception.Message
            Success = $false
        }
        
        return $null
    }
}

<#
.SYNOPSIS
    Safely execute external process using SecurityManager
    REMEDIATION FOR C-02: Command Injection Vulnerability
#>
function Invoke-SafeProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ProcessName,
        
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        
        [Parameter()]
        [string]$WorkingDirectory = [System.IO.Directory]::GetCurrentDirectory()
    )
    
    Write-DevConsole "Executing process: $ProcessName with $($Arguments.Count) arguments" "DEBUG"
    
    # Use SecurityManager for safe execution
    $result = [SecurityManager]::SafeExecuteProcess($ProcessName, $Arguments, $WorkingDirectory)
    
    if ($result.Success) {
        Write-DevConsole "Process completed successfully" "SUCCESS"
    }
    else {
        Write-DevConsole "Process failed: $($result.Error)" "ERROR"
    }
    
    return $result
}

<#
.SYNOPSIS
    Execute Ollama inference with secure parameter passing and failure detection
    REPLACES: Unsafe string concatenation with safe ArgumentList
    INCLUDES: Circuit breaker, retry logic, and failure classification
#>
function Invoke-OllamaInference {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ModelPath,
        
        [Parameter(Mandatory)]
        [string]$Prompt,
        
        [Parameter()]
        [int]$MaxTokens = 2048,
        
        [Parameter()]
        [double]$Temperature = 0.7,
        
        [Parameter()]
        [double]$TopP = 0.9,
        
        [Parameter()]
        [string]$LlamaExecutable = "llama-cpp-server",
        
        [Parameter()]
        [string]$BackendType = "LlamaCpp",
        
        [Parameter()]
        [switch]$EnableRetry,
        
        [Parameter()]
        [int]$MaxRetries = 3
    )
    
    # Extract model name from path
    $modelName = [System.IO.Path]::GetFileNameWithoutExtension($ModelPath)
    
    # Validate inputs with SecurityManager
    if (-not [SecurityManager]::ValidatePath($ModelPath, [System.IO.Directory]::GetCurrentDirectory())) {
        throw "Invalid model path: $ModelPath"
    }
    
    # Build arguments as array (SECURE - prevents injection)
    $arguments = @(
        "-m", $ModelPath,
        "-n", $MaxTokens.ToString(),
        "-t", "8",
        "--temp", $Temperature.ToString(),
        "--top_p", $TopP.ToString(),
        "--no-display-prompt",
        "-p", $Prompt
    )
    
    # Define the action to execute
    $invokeAction = {
        Write-DevConsole "Invoking model with secure parameter passing" "INFO"
        $result = Invoke-SafeProcess -ProcessName $LlamaExecutable -Arguments $arguments
        
        if (-not $result.Success) {
            throw $result.Error
        }
        
        return $result
    }
    
    # Execute with failure detection
    $executionResult = Invoke-ModelWithFailureDetection `
        -ModelName $modelName `
        -BackendType $BackendType `
        -Action $invokeAction `
        -Context @{
            MaxTokens = $MaxTokens
            Temperature = $Temperature
            Timeout = 300000  # 5 min default
        } `
        -EnableRetry:$EnableRetry `
        -MaxRetries $MaxRetries
    
    if ($executionResult.Success) {
        $script:MetricsCollector.RecordSuccess($executionResult.Duration.TotalMilliseconds, "Ollama")
        $script:AuditLogger.LogIntent(@{
            Id = [Guid]::NewGuid()
            Primary = "Ollama Inference"
            Domain = "AIAssistance"
        })
        
        return @{
            Success = $true
            Output = $executionResult.Result.Output
            Duration = $executionResult.Duration
            RetryCount = $executionResult.RetryCount
        }
    }
    else {
        $script:MetricsCollector.RecordFailure("OllamaFailed", 0)
        Write-StartupLog "Ollama inference failed: $($executionResult.Error)" "ERROR"
        $script:AuditLogger.LogError([System.Exception]::new($executionResult.Error), $Prompt, @{
            ModelName = $modelName
            BackendType = $BackendType
            FailureType = $executionResult.FailureType
            CircuitState = $executionResult.CircuitState
        })
        
        return @{
            Success = $false
            Error = $executionResult.Error
            FailureType = $executionResult.FailureType
            CircuitState = $executionResult.CircuitState
            RetryCount = $executionResult.RetryCount
        }
    }
}

<#
.SYNOPSIS
    Securely store and retrieve AI configuration credentials
    REMEDIATION FOR M-01: Insecure Credential Storage
#>
function Set-SecureAICredential {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ServiceName,
        
        [Parameter(Mandatory)]
        [PSCredential]$Credential
    )
    
    try {
        [SecurityManager]::StoreSecureCredential("RawrXD_$ServiceName", $Credential)
        Write-StartupLog "Credential stored securely for $ServiceName" "SUCCESS"
        
        $script:AuditLogger.LogSecurityEvent("CredentialStored", @{
            Service = $ServiceName
            Timestamp = [DateTime]::UtcNow
        })
    }
    catch {
        Write-StartupLog "Failed to store credential: $($_.Exception.Message)" "ERROR"
        throw
    }
}

function Get-SecureAICredential {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ServiceName
    )
    
    return [SecurityManager]::GetSecureCredential("RawrXD_$ServiceName")
}

<#
.SYNOPSIS
    Retrieve production metrics and health status
#>
function Get-AgenticMetrics {
    return $script:MetricsCollector.GetMetrics()
}

function Get-SystemHealth {
    $healthCheck = [HealthCheckService]::new()
    return $healthCheck.RunHealthChecks()
}

<#
.SYNOPSIS
    Start periodic health monitoring
#>
function Start-HealthMonitoring {
    $timer = [System.Timers.Timer]::new(60000)  # 60 seconds
    $timer.AutoReset = $true
    $timer.add_Elapsed({
        try {
            $health = Get-SystemHealth
            
            if ($health.OverallStatus -eq "Unhealthy") {
                Write-StartupLog "⚠️ System health degraded: $($health.OverallStatus)" "WARNING"
            }
            elseif ($health.OverallStatus -eq "Degraded") {
                Write-StartupLog "⚠️ System health degraded: $($health.OverallStatus)" "WARNING"
            }
        }
        catch { }
    })
    $timer.Start()
}

<#
.SYNOPSIS
    Graceful shutdown with security cleanup
#>
function Stop-AgenticEngine {
    [CmdletBinding()]
    param()
    
    try {
        Write-StartupLog "Shutting down RawrXD 3.0 PRO Agentic Engine" "INFO"
        
        if ($script:AgenticEngine) {
            $script:AgenticEngine.Dispose()
        }
        
        if ($script:MetricsCollector) {
            $metrics = $script:MetricsCollector.GetMetrics()
            Write-StartupLog "Final Metrics: Success=$($metrics.Success), Failed=$($metrics.Failed), SuccessRate=$($metrics.SuccessRate)%" "INFO"
            $script:MetricsCollector.Dispose()
        }
        
        # Reset all circuit breakers on shutdown
        if ($script:FailureDetector) {
            Reset-AllModelCircuitBreakers
        }
        
        Write-StartupLog "Agentic Engine shut down successfully" "SUCCESS"
    }
    catch {
        Write-StartupLog "Error during shutdown: $($_.Exception.Message)" "ERROR"
    }
}

# Export public functions
Export-ModuleMember -Function @(
    'Initialize-AgenticEngine',
    'Invoke-AgenticFlow',
    'Invoke-SafeProcess',
    'Invoke-OllamaInference',
    'Set-SecureAICredential',
    'Get-SecureAICredential',
    'Get-AgenticMetrics',
    'Get-SystemHealth',
    'Stop-AgenticEngine',
    # Failure Detection Functions
    'Invoke-ModelWithFailureDetection',
    'Get-ModelHealth',
    'Get-AllModelHealth',
    'Reset-ModelCircuitBreaker',
    'Reset-AllModelCircuitBreakers',
    'Get-FailureDetector'
)
