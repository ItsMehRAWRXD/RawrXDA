<#
.SYNOPSIS
  Cross-platform-compatible orchestration script for BigDaddyG IDE.
  Runs under Windows PowerShell 5.1 and PowerShell Core 7+ without dependencies.

.DESCRIPTION
  Enterprise-grade model orchestrator launcher for BigDaddyG IDE.
  - Manages Ollama/Assembly/Python model server lifecycle
  - Coordinates context engine startup
  - Performs health checks with retry logic
  - Self-healing cleanup of stale processes
  - Structured logging compatible with log aggregators
  - Fully headless and idempotent

.NOTES
  Compatible with Windows PowerShell 5.1 and PowerShell 7+
  No external dependencies required
  Executes head-less (no GUI, no user prompts)
#>

# ========================
# CONFIGURATION PARAMETERS
# ========================
$Env:MODEL_PORT_OLLAMA   = if ($Env:MODEL_PORT_OLLAMA)   { [int]$Env:MODEL_PORT_OLLAMA   } else { 11438 }
$Env:MODEL_PORT_ASSEMBLY = if ($Env:MODEL_PORT_ASSEMBLY) { [int]$Env:MODEL_PORT_ASSEMBLY } else { 11441 }
$Env:CONTEXT_PORT        = if ($Env:CONTEXT_PORT)        { [int]$Env:CONTEXT_PORT        } else { 11439 }
$Env:MAX_ATTEMPTS        = if ($Env:MAX_ATTEMPTS)        { [int]$Env:MAX_ATTEMPTS        } else { 10 }
$Env:SLEEP_INTERVAL      = if ($Env:SLEEP_INTERVAL)      { [double]$Env:SLEEP_INTERVAL   } else { 0.5 }
$Env:CLEANUP_DELAY       = if ($Env:CLEANUP_DELAY)       { [double]$Env:CLEANUP_DELAY    } else { 1 }
$Env:HEALTH_CHECK_DELAY  = if ($Env:HEALTH_CHECK_DELAY)  { [double]$Env:HEALTH_CHECK_DELAY } else { 2 }
$ProjectRoot             = "D:\Security Research aka GitHub Repos\ProjectIDEAI"

# ===============
# LOGGING UTILITY
# ===============
function Write-Log {
    param(
        [ValidateSet('INFO','SUCCESS','WARNING','ERROR')]
        [string]$Level,
        [string]$Message
    )
    $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $logLine = "[{0}] [SYSTEM] [{1}] {2}" -f $timestamp, $Level, $Message
    Write-Host $logLine -ForegroundColor $(switch ($Level) {
        'SUCCESS' { 'Green' }
        'WARNING' { 'Yellow' }
        'ERROR'   { 'Red' }
        default   { 'Gray' }
    })
}

# ====================
# PORT VALIDATION LOGIC
# ====================
function Test-Port {
    param([int]$Port)
    try {
        # Try modern Get-NetTCPConnection (PowerShell 5.1+ with NetTCPIP module)
        $conn = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
        return [bool]($conn)
    } catch {
        # Fallback to netstat for compatibility
        $netstatOutput = netstat -ano | Select-String ":$Port\s"
        return [bool]$netstatOutput
    }
}

# Waits until a given port is active
function Wait-ServiceReady {
    param(
        [int]$Port,
        [string]$ServiceName
    )
    $attempt = 0
    Write-Log "INFO" "Waiting for $ServiceName on port $Port..."
    
    while ($attempt -lt $Env:MAX_ATTEMPTS) {
        if (Test-Port -Port $Port) {
            Write-Log "SUCCESS" "$ServiceName is responsive."
            return $true
        }
        Start-Sleep -Seconds $Env:SLEEP_INTERVAL
        $attempt++
    }
    
    Write-Log "ERROR" "$ServiceName failed to start after $($Env:MAX_ATTEMPTS) attempts."
    return $false
}

# ========================
# STALE PROCESS TERMINATION
# ========================
function Clear-StaleProcesses {
    Write-Log "INFO" "Clearing stale model or context processes..."
    
    $targets = @("node", "python", "ollama")
    
    foreach ($processName in $targets) {
        Get-Process -Name $processName -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                Write-Log "INFO" "Terminating stale process: $($_.ProcessName) (PID: $($_.Id))"
                Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
            } catch {
                # Silently continue if process already terminated
            }
        }
    }
    
    Start-Sleep -Seconds $Env:CLEANUP_DELAY
    Write-Log "SUCCESS" "Stale processes terminated."
}

# =============================
# PROJECT DIRECTORY NAVIGATION
# =============================
function Enter-ProjectRoot {
    if (Test-Path $ProjectRoot) {
        Set-Location $ProjectRoot
        Write-Log "SUCCESS" "Changed to project directory: $ProjectRoot"
    } else {
        Write-Log "ERROR" "Project directory not found: $ProjectRoot"
        exit 1
    }
}

# ============================
# MODEL SELECTION AND STARTUP
# ============================
function Select-Model {
    Write-Log "INFO" "Detecting available model servers..."
    
    # Check for Ollama
    if (Get-Command ollama -ErrorAction SilentlyContinue) {
        Write-Log "SUCCESS" "Ollama engine detected."
        return "ollama"
    }
    # Check for Python fallback
    elseif (Test-Path "models\your-custom-model\generate.py") {
        Write-Log "INFO" "Using Python fallback model."
        return "python"
    }
    # Check for Assembly fallback
    elseif (Test-Path "models\your-custom-model\generate.exe") {
        Write-Log "INFO" "Using Assembly fallback model."
        return "assembly"
    }
    else {
        Write-Log "ERROR" "No model backend found in /models path."
        Write-Log "ERROR" "Please install Ollama or provide a model in /models directory."
        exit 1
    }
}

function Start-ModelServer {
    param([string]$ModelType)
    
    Write-Log "INFO" "Starting model server type: $ModelType"

    switch ($ModelType) {
        "ollama" {
            $port = $Env:MODEL_PORT_OLLAMA
            $script = "server\Orchestra-Server.js"
            $logFile = "ollama-server.log"
            $executable = "node"
        }
        "assembly" {
            $port = $Env:MODEL_PORT_ASSEMBLY
            $script = "servers\bigdaddyg-model-server.js"
            $logFile = "assembly-server.log"
            $executable = "node"
        }
        "python" {
            $port = $Env:MODEL_PORT_OLLAMA
            $script = "servers\python-model-server.py"
            $logFile = "python-model.log"
            $executable = "python"
        }
        Default {
            Write-Log "ERROR" "Invalid model type: $ModelType"
            exit 1
        }
    }

    # Start the server process headless
    try {
        Start-Process -FilePath $executable `
                      -ArgumentList "`"$script`"" `
                      -RedirectStandardOutput $logFile `
                      -RedirectStandardError "$logFile.err" `
                      -WindowStyle Hidden `
                      -NoNewWindow:$false
        
        Start-Sleep -Seconds 1
        
        if (Wait-ServiceReady -Port $port -ServiceName "$ModelType model server") {
            Write-Log "SUCCESS" "Model server listening on port $port"
            return $port
        } else {
            Write-Log "ERROR" "Model server startup failed."
            return $null
        }
    } catch {
        Write-Log "ERROR" "Failed to start model server: $($_.Exception.Message)"
        return $null
    }
}

# ============================
# CONTEXT ENGINE MANAGEMENT
# ============================
function Start-ContextEngine {
    Write-Log "INFO" "Starting context engine..."
    
    $logFile = "context-engine.log"
    $script = "servers\context-engine.js"
    
    try {
        Start-Process -FilePath "node" `
                      -ArgumentList "`"$script`"" `
                      -RedirectStandardOutput $logFile `
                      -RedirectStandardError "$logFile.err" `
                      -WindowStyle Hidden `
                      -NoNewWindow:$false
        
        Start-Sleep -Seconds 1
        
        if (Wait-ServiceReady -Port $Env:CONTEXT_PORT -ServiceName "context engine") {
            Write-Log "SUCCESS" "Context engine listening on port $($Env:CONTEXT_PORT)"
        } else {
            Write-Log "WARNING" "Context engine not reachable after retries."
        }
    } catch {
        Write-Log "WARNING" "Context engine startup skipped: $($_.Exception.Message)"
    }
}

# =====================
# HEALTH CHECK VALIDATION
# =====================
function Perform-HealthChecks {
    Write-Log "INFO" "Performing health checks..."
    
    Start-Sleep -Seconds $Env:HEALTH_CHECK_DELAY
    
    # Check model service
    $modelHealthy = $false
    if (Test-Port -Port $Env:MODEL_PORT_OLLAMA) {
        Write-Log "SUCCESS" "Model service healthy on port $($Env:MODEL_PORT_OLLAMA)."
        $modelHealthy = $true
    } elseif (Test-Port -Port $Env:MODEL_PORT_ASSEMBLY) {
        Write-Log "SUCCESS" "Model service healthy on port $($Env:MODEL_PORT_ASSEMBLY)."
        $modelHealthy = $true
    } else {
        Write-Log "ERROR" "Model service unresponsive on all configured ports."
    }

    # Check context engine
    if (Test-Port -Port $Env:CONTEXT_PORT) {
        Write-Log "SUCCESS" "Context engine healthy on port $($Env:CONTEXT_PORT)."
    } else {
        Write-Log "WARNING" "Context engine not responding; continuing execution."
    }
    
    return $modelHealthy
}

# ======================
# MAIN EXECUTION SEQUENCE
# ======================
try {
    Write-Log "INFO" "BigDaddyG IDE - Orchestration Script Starting..."
    Write-Log "INFO" "Version: 1.0.0 - Regenerative Citadel Edition"
    Write-Log "INFO" "=============================================="
    
    # Step 1: Cleanup
    Clear-StaleProcesses
    
    # Step 2: Navigate to project
    Enter-ProjectRoot
    
    # Step 3: Detect and select model
    $model = Select-Model
    
    # Step 4: Start model server
    $modelPort = Start-ModelServer -ModelType $model
    
    if ($modelPort) {
        # Step 5: Start context engine (skip for Python model)
        if ($model -ne "python") {
            Start-ContextEngine
        }
        
        # Step 6: Perform health checks
        $healthy = Perform-HealthChecks
        
        if ($healthy) {
            Write-Log "INFO" "=============================================="
            Write-Log "SUCCESS" "All systems initialized successfully."
            Write-Log "INFO" "BigDaddyG IDE is ready."
            exit 0
        } else {
            Write-Log "ERROR" "Health checks failed."
            exit 1
        }
    } else {
        Write-Log "ERROR" "Startup sequence failed - model server did not start."
        exit 1
    }
    
} catch {
    Write-Log "ERROR" "Unexpected error during startup: $($_.Exception.Message)"
    Write-Log "ERROR" "Stack trace: $($_.ScriptStackTrace)"
    exit 1
}
