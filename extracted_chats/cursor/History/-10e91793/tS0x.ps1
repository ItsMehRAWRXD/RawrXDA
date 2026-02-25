#Requires -Version 7
<#
.SYNOPSIS
    MITRE ATT&CK Defensive Agent Watcher
    
.DESCRIPTION
    Monitors orchestration events and emits agent-alert signals
    when MITRE ATT&CK techniques are detected.
    
    Glassquill visualizes these as animated sprites in the HUD.

.NOTES
    Zero dependencies
    Runs as a background watcher
#>

[CmdletBinding()]
param(
    [string]$AgentsPath = ".\mitre-agents.json",
    [string]$OutPipe = "\\.\pipe\GlassquillAgents"
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ============================================================================
# CONFIGURATION
# ============================================================================
$Global:EmotionVector = @{ confidence=0.95; hesitation=0.02; urgency=0.0 }
$Global:Agents = @()

# ============================================================================
# LOGGING
# ============================================================================
filter Write-Log {
    param([ValidateSet('INFO','SUCCESS','WARN','ERROR')][string]$Level='INFO')
    $ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $emo = ($Global:EmotionVector.GetEnumerator()|Sort-Object Name|ForEach-Object{"{0}={1:N2}" -f $_.Name,$_.Value}) -join ' '
    "[{0}] [MITRE] [{1}] {2} |{3}" -f $ts, $Level, $_, $emo | Out-Host
}

# ============================================================================
# LOAD MITRE AGENTS
# ============================================================================
function Load-MitreAgents {
    if (-not (Test-Path $AgentsPath)) {
        "❌ MITRE agents file not found: $AgentsPath" | Write-Log -Level ERROR
        return
    }
    
    try {
        $Global:Agents = Get-Content $AgentsPath | ConvertFrom-Json
        "✅ Loaded $($Global:Agents.Count) MITRE defensive agents" | Write-Log -Level SUCCESS
        
        foreach ($agent in $Global:Agents) {
            "  [$($agent.id)] $($agent.name)" | Write-Log -Level INFO
        }
    } catch {
        "Failed to load MITRE agents: $($_.Exception.Message)" | Write-Log -Level ERROR
    }
}

# ============================================================================
# DETECT TECHNIQUE
# ============================================================================
function Test-Technique {
    param(
        [string]$Command,
        [string]$Context = ""
    )
    
    $detected = @()
    
    foreach ($agent in $Global:Agents) {
        foreach ($pattern in $agent.watchFor) {
            if ($Command -match $pattern -or $Context -match $pattern) {
                $detected += $agent
                break
            }
        }
    }
    
    return $detected
}

# ============================================================================
# EMIT AGENT ALERT
# ============================================================================
function Send-AgentAlert {
    param(
        [object]$Agent,
        [string]$Message,
        [hashtable]$SpriteMove = @{ x=0.0; y=0.0; z=0.0 }
    )
    
    $alert = @{
        type = "agent-alert"
        technique = $Agent.id
        name = $Agent.name
        message = $Message
        sprite = $Agent.sprite
        position = $Agent.position
        color = $Agent.color
        move = $SpriteMove
        emotion = $Global:EmotionVector
        timestamp = Get-Date -Format o
    } | ConvertTo-Json -Compress
    
    # Output to console (Glassquill watches stdout)
    Write-Output $alert
    
    # Also send to named pipe if available
    if ($pipe -and $pipe.IsConnected) {
        try {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes("$alert`n")
            $pipe.Write($bytes, 0, $bytes.Length)
            $pipe.Flush()
        } catch {
            # Pipe broken, ignore
        }
    }
}

# ============================================================================
# WATCH ORCHESTRATION EVENTS
# ============================================================================
function Start-OrchestrationWatcher {
    "🔍 MITRE ATT&CK defensive watcher active" | Write-Log -Level INFO
    
    # Example: Monitor specific patterns
    $patterns = @(
        @{ command="Clear-StaleProcesses"; context="process cleanup"; urgency=0.3 },
        @{ command="Start-Process"; context="node server launch"; urgency=0.5 },
        @{ command="Invoke-RestMethod"; context="marketplace API"; urgency=0.2 }
    )
    
    foreach ($pattern in $patterns) {
        $detected = Test-Technique -Command $pattern.command -Context $pattern.context
        
        if ($detected.Count -gt 0) {
            foreach ($agent in $detected) {
                $Global:EmotionVector.urgency = $pattern.urgency
                
                $message = "$($pattern.command) invoked – monitoring for $($agent.name)"
                
                Send-AgentAlert -Agent $agent `
                                -Message $message `
                                -SpriteMove @{ 
                                    x = [Math]::Sin((Get-Date).Second / 10.0) * 0.5
                                    y = [Math]::Cos((Get-Date).Second / 10.0) * 0.5
                                    z = 0.0
                                }
                
                "⚠️ Technique detected: [$($agent.id)] $($agent.name)" | Write-Log -Level WARN
            }
        }
    }
}

# ============================================================================
# MAIN
# ============================================================================

try {
    Load-MitreAgents
    
    # Open named pipe for Glassquill
    try {
        $pipe = New-Object System.IO.Pipes.NamedPipeServerStream(
            "GlassquillAgents",
            [System.IO.Pipes.PipeDirection]::Out,
            1,
            [System.IO.Pipes.PipeTransmissionMode]::Byte,
            [System.IO.Pipes.PipeOptions]::Asynchronous
        )
        
        "Waiting for Glassquill agent layer to connect..." | Write-Log -Level INFO
        $pipe.WaitForConnection(5000) # 5 second timeout
        
        if ($pipe.IsConnected) {
            "✅ Glassquill agent layer connected" | Write-Log -Level SUCCESS
        }
    } catch {
        "⚠️ Named pipe not available - outputting to stdout only" | Write-Log -Level WARN
        $pipe = $null
    }
    
    # Start watching
    Start-OrchestrationWatcher
    
    # In production, this would be a continuous event loop
    # For now, we run once and demonstrate the pattern
    
    "🛡️ MITRE watcher cycle complete" | Write-Log -Level SUCCESS
    
} catch {
    "❌ MITRE watcher error: $($_.Exception.Message)" | Write-Log -Level ERROR
} finally {
    if ($pipe) {
        $pipe.Dispose()
    }
}

exit 0

