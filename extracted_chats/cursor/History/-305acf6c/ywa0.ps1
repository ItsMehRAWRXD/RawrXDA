#Requires -Version 7
<#
.SYNOPSIS
    /resurrect - One-Click Re-Hydration of Last-Known-Good Universe
    
.DESCRIPTION
    Cinematic fast-path to restore BigDaddyG IDE to the last successful state
    without re-running the full orchestration hook.
    
    Reads the orchestration ledger, finds the last successful state,
    verifies health, and either resumes or falls back to full boot.

.NOTES
    USB and cloud portable
    Zero external dependencies
#>

[CmdletBinding()]
param(
    [switch]$Force,     # Force full boot even if warm state exists
    [switch]$Cinematic  # Show cinematic transition effects
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ============================================================================
# CONFIGURATION
# ============================================================================
$LEDGER_PATH = "$env:USERPROFILE\.bigdaddyg\orchestration-ledger.jsonl"
$PROJECT_ROOT = "D:\Security Research aka GitHub Repos\ProjectIDEAI"
$Global:EmotionVector = @{ confidence=0.0; hesitation=0.95; urgency=0.8 }

# ============================================================================
# LOGGING WITH EMOTION
# ============================================================================
filter Write-Log {
    param([ValidateSet('INFO','SUCCESS','WARN','ERROR')][string]$Level='INFO')
    $ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $emo = ($Global:EmotionVector.GetEnumerator()|Sort-Object Name|ForEach-Object{"{0}={1:N2}" -f $_.Name,$_.Value}) -join ' '
    "[{0}] [RESURRECT] [{1}] {2} |{3}" -f $ts, $Level, $_, $emo | Out-Host
}

# ============================================================================
# PORT CHECKER
# ============================================================================
function Test-PortInUse {
    param([int]$Port)
    if ($Port -lt 1 -or $Port -gt 65535){ return $false }
    try {
        return [bool](Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue)
    } catch {
        return [bool](netstat -ano | Select-String ":$Port\s")
    }
}

# ============================================================================
# CINEMATIC EFFECTS
# ============================================================================
function Show-CinematicTransition {
    param([string]$Message)
    
    if (-not $Cinematic) { return }
    
    # ASCII art fade-in
    $frames = @(
        "█▓▒░ RESURRECT ░▒▓█",
        "██▓▒░ RESURRECT ░▒▓██",
        "███▓▒░ RESURRECT ░▒▓███"
    )
    
    foreach ($frame in $frames) {
        Clear-Host
        Write-Host "`n`n`n"
        Write-Host "    $frame" -ForegroundColor Cyan
        Write-Host "`n    $Message" -ForegroundColor Gray
        Start-Sleep -Milliseconds 200
    }
    
    Start-Sleep -Milliseconds 500
}

# ============================================================================
# MAIN RESURRECTION SEQUENCE
# ============================================================================

try {
    # Cinematic intro
    if ($Cinematic) {
        Show-CinematicTransition "Scanning ledger for last known good state..."
    }
    
    "🔄 BigDaddyG IDE - Resurrection Protocol" | Write-Log -Level INFO
    "=============================================" | Write-Log -Level INFO
    
    # Check if forced full boot
    if ($Force) {
        "Force flag detected - skipping warm start" | Write-Log -Level WARN
        $Global:EmotionVector.urgency = 1.0
        throw "FORCE_FULL_BOOT"
    }
    
    # Check if ledger exists
    if (-not (Test-Path $LEDGER_PATH)) {
        "No ledger found - first boot required" | Write-Log -Level INFO
        $Global:EmotionVector.confidence = 0.0
        throw "NO_LEDGER"
    }
    
    # Read last line from ledger
    $Global:EmotionVector.confidence = 0.3
    $Global:EmotionVector.hesitation = 0.6
    
    "Reading orchestration ledger..." | Write-Log -Level INFO
    $lastLine = Get-Content $LEDGER_PATH -Tail 1
    
    if (-not $lastLine) {
        "Ledger is empty" | Write-Log -Level WARN
        throw "EMPTY_LEDGER"
    }
    
    # Parse ledger entry
    $parts = $lastLine -split '\|', 4
    if ($parts.Length -lt 4) {
        "Invalid ledger format" | Write-Log -Level ERROR
        throw "CORRUPT_LEDGER"
    }
    
    $timestamp = $parts[0]
    $prevHash = $parts[1]
    $currentHash = $parts[2]
    $payload = $parts[3] | ConvertFrom-Json
    
    "Last state: $($payload.s)" | Write-Log -Level INFO
    "  Timestamp:  $timestamp" | Write-Log -Level INFO
    "  Hash:       $($currentHash.Substring(0,32))..." | Write-Log -Level INFO
    "  Result:     $($payload.r)" | Write-Log -Level INFO
    "  Confidence: $($payload.e.confidence)" | Write-Log -Level INFO
    
    # Check if last state was successful
    if ($payload.r -ne 0) {
        "Last state was not successful (result=$($payload.r))" | Write-Log -Level WARN
        $Global:EmotionVector.hesitation = 0.9
        throw "LAST_STATE_FAILED"
    }
    
    # Check if state is too old (> 24 hours)
    $stateAge = (Get-Date) - [DateTime]::Parse($timestamp)
    if ($stateAge.TotalHours -gt 24) {
        "State is too old ($([Math]::Round($stateAge.TotalHours, 1)) hours)" | Write-Log -Level WARN
        throw "STATE_TOO_OLD"
    }
    
    # Restore environment from ledger
    "Restoring environment from ledger..." | Write-Log -Level INFO
    $env:MODEL_PORT_OLLAMA   = $payload.p.MODEL_PORT_OLLAMA
    $env:MODEL_PORT_ASSEMBLY = $payload.p.MODEL_PORT_ASSEMBLY
    $env:CONTEXT_PORT        = $payload.p.CONTEXT_PORT
    
    $Global:EmotionVector.confidence = 0.6
    $Global:EmotionVector.hesitation = 0.3
    
    # Cinematic health check
    if ($Cinematic) {
        Show-CinematicTransition "Probing for signs of life..."
    }
    
    # Quick health check
    "Performing health checks..." | Write-Log -Level INFO
    
    $modelHealthy = $false
    $contextHealthy = $false
    
    # Check model ports
    if (Test-PortInUse -Port $env:MODEL_PORT_OLLAMA) {
        "✅ Model service alive on port $env:MODEL_PORT_OLLAMA" | Write-Log -Level SUCCESS
        $modelHealthy = $true
    } elseif (Test-PortInUse -Port $env:MODEL_PORT_ASSEMBLY) {
        "✅ Model service alive on port $env:MODEL_PORT_ASSEMBLY" | Write-Log -Level SUCCESS
        $modelHealthy = $true
    }
    
    # Check context engine
    if (Test-PortInUse -Port $env:CONTEXT_PORT) {
        "✅ Context engine alive on port $env:CONTEXT_PORT" | Write-Log -Level SUCCESS
        $contextHealthy = $true
    }
    
    if ($modelHealthy) {
        $Global:EmotionVector.confidence = 0.97
        $Global:EmotionVector.hesitation = 0.01
        $Global:EmotionVector.urgency = 0.0
        
        # Cinematic success
        if ($Cinematic) {
            Show-CinematicTransition "Universe is alive!"
        }
        
        "=============================================" | Write-Log -Level SUCCESS
        "🌟 WARM UNIVERSE RESURRECTED" | Write-Log -Level SUCCESS
        "=============================================" | Write-Log -Level SUCCESS
        "  State:      $($payload.s)" | Write-Log -Level INFO
        "  Age:        $([Math]::Round($stateAge.TotalMinutes, 1)) minutes" | Write-Log -Level INFO
        "  Model:      Port $($env:MODEL_PORT_OLLAMA)" | Write-Log -Level INFO
        "  Context:    $(if ($contextHealthy) { "Port $env:CONTEXT_PORT" } else { 'Offline' })" | Write-Log -Level INFO
        "  Confidence: $($Global:EmotionVector.confidence)" | Write-Log -Level INFO
        "=============================================" | Write-Log -Level SUCCESS
        
        exit 0
        
    } else {
        "Services not responding - resurrection failed" | Write-Log -Level WARN
        $Global:EmotionVector.confidence = 0.0
        $Global:EmotionVector.urgency = 0.95
        throw "SERVICES_OFFLINE"
    }
    
} catch {
    # Resurrection failed - fall back to full boot
    $reason = $_.Exception.Message
    
    "❌ Resurrection failed: $reason" | Write-Log -Level ERROR
    "Falling back to full orchestration boot..." | Write-Log -Level WARN
    
    $Global:EmotionVector.confidence = 0.0
    $Global:EmotionVector.urgency = 1.0
    
    # Change to project directory
    if (Test-Path $PROJECT_ROOT) {
        Set-Location $PROJECT_ROOT
    }
    
    # Run full boot sequence
    $hookPath = Join-Path $PROJECT_ROOT "hooks\beforePromptSubmit.ps1"
    
    if (Test-Path $hookPath) {
        "Executing full boot: $hookPath" | Write-Log -Level INFO
        & $hookPath
        exit $LASTEXITCODE
    } else {
        "❌ Boot hook not found: $hookPath" | Write-Log -Level ERROR
        exit 1
    }
}

