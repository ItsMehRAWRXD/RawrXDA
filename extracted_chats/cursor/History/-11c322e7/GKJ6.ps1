# BigDaddyG IDE - Before Prompt Submit Hook (PowerShell)
# Production-ready pre-prompt processing with security hardening
# Windows-native version with full platform integration

# ============================================================================
# CONFIGURATION
# ============================================================================

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Service ports
$MODEL_PORT_OLLAMA = $env:MODEL_PORT_OLLAMA ?? 11434
$MODEL_PORT_BIGDADDYG = $env:MODEL_PORT_BIGDADDYG ?? 11441
$ORCHESTRA_PORT = $env:ORCHESTRA_PORT ?? 3000
$CONTEXT_PORT = $env:CONTEXT_PORT ?? 11439

# Retry settings
$MAX_ATTEMPTS = $env:MAX_ATTEMPTS ?? 10
$SLEEP_INTERVAL = $env:SLEEP_INTERVAL ?? 0.5
$CLEANUP_DELAY = $env:CLEANUP_DELAY ?? 1
$HEALTH_CHECK_DELAY = $env:HEALTH_CHECK_DELAY ?? 2

# Security settings
$ENABLE_SHELL_INJECTION_CHECK = $env:ENABLE_SHELL_INJECTION_CHECK ?? $true
$ENABLE_SECRET_SCRUBBING = $env:ENABLE_SECRET_SCRUBBING ?? $true
$ENABLE_CONTEXT_ANALYSIS = $env:ENABLE_CONTEXT_ANALYSIS ?? $true

# ============================================================================
# LOGGING
# ============================================================================

function Write-Log {
    param(
        [string]$Level,
        [string]$Message
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] [BigDaddyG Hook] [$Level] $Message" -ForegroundColor $(
        switch ($Level) {
            "ERROR" { "Red" }
            "WARNING" { "Yellow" }
            "SUCCESS" { "Green" }
            "INFO" { "Cyan" }
            default { "White" }
        }
    )
}

# ============================================================================
# PORT CHECKING
# ============================================================================

function Test-Port {
    param([int]$Port)
    
    if ($Port -lt 1 -or $Port -gt 65535) {
        Write-Log "ERROR" "Invalid port number: $Port"
        return $false
    }
    
    try {
        $connection = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
        return $connection -ne $null
    } catch {
        return $false
    }
}

function Wait-ForService {
    param(
        [int]$Port,
        [string]$ServiceName = "service",
        [int]$MaxAttempts = $MAX_ATTEMPTS
    )
    
    Write-Log "INFO" "Waiting for $ServiceName on port $Port..."
    
    for ($attempt = 0; $attempt -lt $MaxAttempts; $attempt++) {
        if (Test-Port -Port $Port) {
            Write-Log "SUCCESS" "$ServiceName is ready on port $Port"
            return $true
        }
        Start-Sleep -Seconds $SLEEP_INTERVAL
    }
    
    Write-Log "ERROR" "$ServiceName failed to start on port $Port after $MaxAttempts attempts"
    return $false
}

# ============================================================================
# SECURITY: SHELL INJECTION DETECTION
# ============================================================================

function Test-ShellInjection {
    param([string]$Prompt)
    
    if (-not $ENABLE_SHELL_INJECTION_CHECK) {
        return $null
    }
    
    $dangerousPatterns = @(
        "&&", "||", ";", "|", "`",
        "$(" , "rm -rf /", "format",
        "dd if=", "> /dev/", "Remove-Item -Recurse -Force"
    )
    
    $warnings = @()
    
    foreach ($pattern in $dangerousPatterns) {
        if ($Prompt -match [regex]::Escape($pattern)) {
            Write-Log "WARNING" "Potentially dangerous pattern detected: $pattern"
            $warnings += "🛡️ SECURITY WARNING: Detected potentially dangerous pattern: $pattern"
        }
    }
    
    return $warnings
}

# ============================================================================
# SECURITY: SECRET SCRUBBING
# ============================================================================

function Invoke-SecretScrubbing {
    param([string]$Text)
    
    if (-not $ENABLE_SECRET_SCRUBBING) {
        return $Text
    }
    
    # Scrub secrets (PowerShell-safe regex)
    $scrubbed = $Text
    $scrubbed = $scrubbed -replace 'Bearer [A-Za-z0-9._-]{20,}', 'Bearer [REDACTED]'
    $scrubbed = $scrubbed -replace 'api[_-]?key[=:][A-Za-z0-9]{20,}', 'api_key=[REDACTED]'
    $scrubbed = $scrubbed -replace 'sk-[A-Za-z0-9]{48}', 'sk-[REDACTED]'
    $scrubbed = $scrubbed -replace 'ghp_[A-Za-z0-9]{36}', 'ghp_[REDACTED]'
    
    return $scrubbed
}

# ============================================================================
# CONTEXT ANALYSIS
# ============================================================================

function Get-PromptIntent {
    param([string]$Prompt)
    
    if (-not $ENABLE_CONTEXT_ANALYSIS) {
        return "general"
    }
    
    $intent = "general"
    
    if ($Prompt -match "(compile|build|make)") {
        $intent = "compilation"
        Write-Log "INFO" "Intent detected: Compilation task"
    } elseif ($Prompt -match "(create|generate|write).*file") {
        $intent = "file_creation"
        Write-Log "INFO" "Intent detected: File creation"
    } elseif ($Prompt -match "(fix|debug|error)") {
        $intent = "debugging"
        Write-Log "INFO" "Intent detected: Debugging"
    } elseif ($Prompt -match "(explain|what|how)") {
        $intent = "explanation"
        Write-Log "INFO" "Intent detected: Explanation request"
    } elseif ($Prompt -match "@[a-zA-Z0-9._-]+") {
        $intent = "file_reference"
        Write-Log "INFO" "Intent detected: File reference"
    }
    
    return $intent
}

# ============================================================================
# FILE REFERENCE EXTRACTION
# ============================================================================

function Get-FileReferences {
    param([string]$Prompt)
    
    $references = @()
    
    # Extract @filename patterns
    $matches = [regex]::Matches($Prompt, '@([a-zA-Z0-9._/-]+)')
    
    foreach ($match in $matches) {
        $filename = $match.Groups[1].Value
        
        if (Test-Path $filename) {
            $references += $filename
            Write-Log "INFO" "File reference found: $filename"
        } else {
            Write-Log "WARNING" "Referenced file not found: $filename"
        }
    }
    
    return $references
}

# ============================================================================
# CONTEXT INJECTION
# ============================================================================

function Add-ContextToPrompt {
    param(
        [string]$Prompt,
        [array]$References,
        [string]$Intent
    )
    
    $enhanced = $Prompt
    
    # Add file contents
    if ($References.Count -gt 0) {
        $enhanced += "`n`n--- Referenced Files ---`n"
        
        foreach ($file in $References) {
            if (Test-Path $file) {
                $enhanced += "`n--- $file ---`n"
                $enhanced += Get-Content $file -Raw
                $enhanced += "`n"
                Write-Log "INFO" "Injected file content: $file"
            }
        }
    }
    
    # Add intent-specific context
    switch ($Intent) {
        "compilation" {
            $enhanced += "`n`n[System: User wants to compile code. Provide compilation commands and error fixing.]"
        }
        "file_creation" {
            $enhanced += "`n`n[System: User wants to create files. Provide complete, working code.]"
        }
        "debugging" {
            $enhanced += "`n`n[System: User needs debugging help. Analyze for errors and provide fixes.]"
        }
        "explanation" {
            $enhanced += "`n`n[System: User wants an explanation. Be clear and detailed.]"
        }
    }
    
    return $enhanced
}

# ============================================================================
# MODEL SELECTION
# ============================================================================

function Select-OptimalModel {
    param([string]$Intent)
    
    $model = switch ($Intent) {
        "compilation" { "BigDaddyG:Code" }
        "file_creation" { "BigDaddyG:Code" }
        "debugging" { "BigDaddyG:Debug" }
        "explanation" { "BigDaddyG:Latest" }
        default { "BigDaddyG:Latest" }
    }
    
    Write-Log "INFO" "Selected model: $model"
    return $model
}

# ============================================================================
# HEALTH CHECKS
# ============================================================================

function Test-OrchestraHealth {
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:$ORCHESTRA_PORT/health" -TimeoutSec 2 -ErrorAction Stop
        return $response.StatusCode -eq 200
    } catch {
        return $false
    }
}

function Test-BigDaddyGHealth {
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:$MODEL_PORT_BIGDADDYG/api/health" -TimeoutSec 2 -ErrorAction Stop
        return $response.StatusCode -eq 200
    } catch {
        return $false
    }
}

# ============================================================================
# STARTUP CHECKS
# ============================================================================

function Start-RequiredServices {
    Write-Log "INFO" "Checking BigDaddyG services..."
    
    # Check Orchestra server
    if (-not (Test-Port -Port $ORCHESTRA_PORT)) {
        Write-Log "WARNING" "Orchestra server not running on port $ORCHESTRA_PORT"
        Write-Log "INFO" "Attempting to start Orchestra server..."
        
        if (Test-Path "server\Orchestra-Server.js") {
            Start-Process node -ArgumentList "server\Orchestra-Server.js" -WindowStyle Hidden
            Start-Sleep -Seconds $HEALTH_CHECK_DELAY
        }
    }
    
    # Verify health
    if (Test-OrchestraHealth) {
        Write-Log "SUCCESS" "Orchestra server healthy"
    } else {
        Write-Log "WARNING" "Orchestra server health check failed"
    }
    
    if (Test-BigDaddyGHealth) {
        Write-Log "SUCCESS" "BigDaddyG model healthy"
    } else {
        Write-Log "INFO" "BigDaddyG model will initialize on first use"
    }
}

# ============================================================================
# PROMPT ENHANCEMENT
# ============================================================================

function Invoke-PromptEnhancement {
    param([string]$OriginalPrompt)
    
    Write-Log "INFO" "Processing prompt..."
    
    # 1. Security check
    $securityWarnings = Test-ShellInjection -Prompt $OriginalPrompt
    
    # 2. Scrub secrets
    $scrubbedPrompt = Invoke-SecretScrubbing -Text $OriginalPrompt
    
    # 3. Analyze context
    $intent = Get-PromptIntent -Prompt $scrubbedPrompt
    
    # 4. Extract file references
    $references = Get-FileReferences -Prompt $scrubbedPrompt
    
    # 5. Inject context
    $enhancedPrompt = Add-ContextToPrompt -Prompt $scrubbedPrompt -References $references -Intent $intent
    
    # 6. Select optimal model
    $selectedModel = Select-OptimalModel -Intent $intent
    
    # 7. Build final prompt with metadata
    $finalPrompt = @"
[BigDaddyG IDE - Enhanced Prompt]
[Intent: $intent]
[Model: $selectedModel]
[Safety: BALANCED]
[Context: $($enhancedPrompt.Length) chars]
[Referenced Files: $($references.Count)]

"@
    
    if ($securityWarnings) {
        $finalPrompt += ($securityWarnings -join "`n") + "`n`n"
    }
    
    $finalPrompt += $enhancedPrompt
    
    Write-Output $finalPrompt
    Write-Log "SUCCESS" "Prompt enhanced and ready"
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

function Main {
    Write-Log "INFO" "╔════════════════════════════════════════════╗"
    Write-Log "INFO" "║  BigDaddyG IDE - Before Prompt Submit    ║"
    Write-Log "INFO" "╚════════════════════════════════════════════╝"
    
    # Ensure services running
    Start-RequiredServices
    
    # Read prompt from pipeline
    $prompt = @($input) -join "`n"
    
    # Enhance the prompt
    $enhanced = Invoke-PromptEnhancement -OriginalPrompt $prompt
    
    # Output enhanced prompt
    Write-Output $enhanced
    
    Write-Log "INFO" "Hook execution complete"
}

# ============================================================================
# RUN
# ============================================================================

Main

