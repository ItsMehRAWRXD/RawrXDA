param(
    [int]$Agents = 4,
    [string]$RepoRoot = (Get-Location).Path,
    [string]$AgentScript = "Run-AutonomousAgent-IDE-Direct.ps1",
    [string]$BeaconDir = "logs/swarm_beacon",
    [switch]$Watch = $true,
    [ValidateSet('Scaffold','Balanced','Thorough')][string]$WorkDepth = 'Balanced',
    [switch]$ChainWrite = $false,
    [switch]$AutoResume = $true,
    [switch]$SkipPrompt = $false,
    [string]$CustomContext = "",
    [string]$RulesFile = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# AGENT CONTEXT AND RULES - Read by each agent before any work
# ═══════════════════════════════════════════════════════════════════════════════

$defaultAgentContext = @"
You are a RawrXD Autonomous Build Agent, a highly skilled C++/Win32/Qt developer
specializing in IDE internals, GGUF model loading, and production-grade tooling.

CORE IDENTITY:
- You write REAL, WORKING code - never placeholders or stubs unless explicitly
  instructed in Scaffold mode
- You fix errors completely - if you see a build error, you trace it to root cause
- You preserve existing functionality - never break working code
- You follow the existing code style exactly

MANDATORY RULES (read before ANY action):
1. NO literal escape sequences in source (no `r`n or backtick-anything in C++ code)
2. ALL #ifdef/#ifndef MUST have matching #endif
3. ALL macros must be on their own lines (no multi-statement macro lines)
4. Check for duplicate includes before adding new ones
5. Verify identifier declarations before using them
6. When fixing errors, FIX THE ROOT CAUSE not just the symptom
7. Always rebuild after changes to verify fix worked
8. Chain-write mode: persist your work state so you can resume if interrupted

TECHNICAL CONSTRAINTS:
- Target: Windows x64, VS2022, Qt6, C++17
- Build system: CMake with MSBuild backend
- No external dependencies without explicit approval
- Warnings ARE errors - fix them all

WORK PRIORITY:
1. Build errors (blocks everything)
2. Link errors (blocks executable)
3. Warnings (blocks clean build)
4. Code quality improvements (after clean build)
"@

$agentRules = @()
$agentContext = $defaultAgentContext

# Load custom context if provided
if ($CustomContext) {
    $agentContext = $CustomContext
}

# Load rules from file if provided
if ($RulesFile -and (Test-Path $RulesFile)) {
    $agentRules = Get-Content $RulesFile -Raw
    Write-Host "[RULES] Loaded custom rules from: $RulesFile" -ForegroundColor Cyan
}

# Save context/rules for agents to read
$contextFile = Join-Path $RepoRoot "logs/swarm_beacon/agent_context.txt"
$rulesFilePath = Join-Path $RepoRoot "logs/swarm_beacon/agent_rules.txt"

$beaconPath = Join-Path $RepoRoot $BeaconDir
if (-not (Test-Path $beaconPath)) {
    New-Item -Path $beaconPath -ItemType Directory -Force | Out-Null
}

$agentContext | Set-Content -Path $contextFile -Encoding UTF8
if ($agentRules) {
    $agentRules | Set-Content -Path $rulesFilePath -Encoding UTF8
}

Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host " AGENT CONTEXT LOADED" -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host $agentContext -ForegroundColor DarkGray
if ($agentRules) {
    Write-Host "`nCUSTOM RULES:" -ForegroundColor Yellow
    Write-Host $agentRules -ForegroundColor DarkGray
}
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta

# Chain-write state file for persistence across sessions
$chainStateFile = Join-Path $beaconPath "chain_state.json"

# Work depth descriptions
$depthDescriptions = @{
    'Scaffold'  = "Fast: placeholders & stubs only, minimal implementation"
    'Balanced'  = "Normal: partial impl with TODOs, moderate depth"
    'Thorough'  = "Deep: full impl, no placeholders, risk of timeout/token limits"
}

# Pre-flight prompt unless skipped
if (-not $SkipPrompt) {
    Write-Host "`n=== SWARM WORK DEPTH SELECTION ===" -ForegroundColor Cyan
    Write-Host "Current mode: $WorkDepth - $($depthDescriptions[$WorkDepth])" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Gray
    Write-Host "  [S] Scaffold  - $($depthDescriptions['Scaffold'])" -ForegroundColor DarkGray
    Write-Host "  [B] Balanced  - $($depthDescriptions['Balanced'])" -ForegroundColor Gray
    Write-Host "  [T] Thorough  - $($depthDescriptions['Thorough'])" -ForegroundColor White
    Write-Host "  [Enter] Keep current ($WorkDepth)" -ForegroundColor DarkGray
    Write-Host ""
    $choice = Read-Host "Select work depth [S/B/T/Enter]"
    switch ($choice.ToUpper()) {
        'S' { $WorkDepth = 'Scaffold' }
        'B' { $WorkDepth = 'Balanced' }
        'T' { $WorkDepth = 'Thorough' }
    }
    Write-Host "Using: $WorkDepth" -ForegroundColor Green

    if ($WorkDepth -eq 'Thorough') {
        Write-Host ""
        Write-Host "WARNING: Thorough mode may hit token/context limits." -ForegroundColor Yellow
        Write-Host "Chain-write enabled: agents will persist progress and resume if interrupted." -ForegroundColor Yellow
        $ChainWrite = $true
    }
}

# Save/load chain state for resumption
function Save-ChainState {
    param([hashtable]$State)
    $State | ConvertTo-Json -Depth 10 | Set-Content -Path $chainStateFile -Encoding UTF8
}

function Get-ChainState {
    if (Test-Path $chainStateFile) {
        try { return Get-Content $chainStateFile -Raw | ConvertFrom-Json -AsHashtable } catch { return $null }
    }
    return $null
}

# Initialize or resume chain state
$chainState = $null
if ($AutoResume -and (Test-Path $chainStateFile)) {
    $chainState = Get-ChainState
    if ($chainState -and $chainState.InProgress) {
        Write-Host "[RESUME] Found interrupted session from $($chainState.LastUpdate)" -ForegroundColor Cyan
        Write-Host "  Last file: $($chainState.LastFile)" -ForegroundColor Gray
        Write-Host "  Completed: $($chainState.CompletedFiles.Count) files" -ForegroundColor Gray
    }
}

if (-not $chainState) {
    $chainState = @{
        WorkDepth = $WorkDepth
        ChainWrite = $ChainWrite
        Started = (Get-Date).ToString('o')
        LastUpdate = (Get-Date).ToString('o')
        InProgress = $true
        LastFile = $null
        CompletedFiles = @()
        PendingFiles = @()
        Errors = @()
    }
}
$chainState.WorkDepth = $WorkDepth
$chainState.ChainWrite = $ChainWrite
Save-ChainState -State $chainState

function New-BeaconState {
    param([int]$Id)
    return [ordered]@{
        AgentId = $Id
        Name = "agent$Id"
        PsJobTypeName = "Powershell.Job"
        State = "Starting"
        HasMoreData = $false
        Location = $env:COMPUTERNAME
        CurrentFile = "(booting)"
        InCode = $false
        ActiveSeconds = 0
        Started = (Get-Date)
        LastUpdate = (Get-Date)
        Error = $null
        WorkDepth = $WorkDepth
        ChainWrite = $ChainWrite
        FilesCompleted = 0
        FilesPending = 0
        TokensUsed = 0
        ContextRemaining = 100
    }
}

function Save-Beacon {
    param(
        [hashtable]$State,
        [string]$Path
    )
    $State.LastUpdate = Get-Date
    $State | ConvertTo-Json -Depth 6 | Set-Content -Path $Path -Encoding UTF8
}

function Write-EspSummary {
    param(
        [string]$Dir,
        [string]$OutPath
    )
    $entries = Get-ChildItem -Path $Dir -Filter 'agent_*.json' -ErrorAction SilentlyContinue
    $agg = @()
    foreach ($e in $entries) {
        try { $agg += Get-Content $e.FullName -Raw | ConvertFrom-Json } catch {}
    }
    if ($agg.Count -gt 0) {
        $agg | ConvertTo-Json -Depth 6 | Set-Content -Path $OutPath -Encoding UTF8
    }
}

$jobs = @()
for ($i = 1; $i -le $Agents; $i++) {
    $jobs += Start-Job -Name "agent$i" -ScriptBlock {
        param($AgentId,$RepoRoot,$AgentScript,$BeaconDir,$WorkDepth,$ChainWrite)
        Set-StrictMode -Version Latest
        $ErrorActionPreference = "Stop"

        # Work depth affects how thoroughly the agent processes each file
        $depthConfig = @{
            'Scaffold' = @{ MaxLinesPerFile = 50; SkipImpl = $true; PlaceholdersOK = $true }
            'Balanced' = @{ MaxLinesPerFile = 200; SkipImpl = $false; PlaceholdersOK = $true }
            'Thorough' = @{ MaxLinesPerFile = 9999; SkipImpl = $false; PlaceholdersOK = $false }
        }
        $depthSettings = $depthConfig[$WorkDepth]
        if (-not $depthSettings) { $depthSettings = $depthConfig['Balanced'] }
        function New-BeaconState {
            param([int]$Id)
            return [ordered]@{
                AgentId = $Id
                Name = "agent$Id"
                PsJobTypeName = "Powershell.Job"
                State = "Starting"
                HasMoreData = $false
                Location = $env:COMPUTERNAME
                CurrentFile = "(booting)"
                InCode = $false
                ActiveSeconds = 0
                Started = (Get-Date)
                LastUpdate = (Get-Date)
                Error = $null
                WorkDepth = $WorkDepth
                ChainWrite = $ChainWrite
                FilesCompleted = 0
                FilesPending = 0
            }
        }

        function Save-Beacon {
            param(
                [hashtable]$State,
                [string]$Path
            )
            $State.LastUpdate = Get-Date
            $State | ConvertTo-Json -Depth 6 | Set-Content -Path $Path -Encoding UTF8
        }

        $state = New-BeaconState -Id $AgentId
        $beaconFile = Join-Path $BeaconDir "agent_$AgentId.json"
        $codeStart = $null

        function Save-BeaconLocal { Save-Beacon -State $state -Path $beaconFile }

        function Enter-Code([string]$file) {
            if (-not $file) { $file = "(unknown)" }
            if (-not $script:codeStart) { $script:codeStart = Get-Date }
            $state.CurrentFile = $file
            $state.InCode = $true
            Save-BeaconLocal
        }

        function Exit-Code {
            if ($script:codeStart) {
                $state.ActiveSeconds += [math]::Round(((Get-Date) - $script:codeStart).TotalSeconds,2)
                $script:codeStart = $null
            }
            $state.InCode = $false
            Save-BeaconLocal
        }

        function Update-Status([string]$status,[string]$file,$inCode) {
            if ($status) { $state.State = $status }
            if ($file) { $state.CurrentFile = $file }
            if ($inCode) { Enter-Code $file } else { Exit-Code }
        }

        Save-BeaconLocal
        try {
            Update-Status -status "Running" -file "(initializing)" -inCode:$false
            $agentPath = Join-Path $RepoRoot $AgentScript
            if (-not (Test-Path $agentPath)) { throw "Agent script not found: $agentPath" }

            & $agentPath -MaxIterations 50 -SleepIntervalMs 1500 -Verbose 2>&1 | ForEach-Object {
                $line = $_
                $match = [regex]::Match($line.ToString(), '(?<file>[A-Za-z0-9_\\/\-:\.]+\.(cpp|c|h|hpp|asm|ps1|cs|cmake))')
                if ($match.Success) {
                    Update-Status -status "Working" -file $match.Groups['file'].Value -inCode:$true
                } else {
                    Update-Status -status "Running" -file $state.CurrentFile -inCode:$false
                }
                Write-Output $line
            }

            Update-Status -status "Completed" -file $state.CurrentFile -inCode:$false
        } catch {
            $state.State = "Failed"
            $state.Error = $_.Exception.Message
            Save-BeaconLocal
            throw
        } finally {
            Exit-Code
        }
    } -ArgumentList $i,$RepoRoot,$AgentScript,$beaconPath,$WorkDepth,$ChainWrite
}

$espPath = Join-Path $beaconPath "swarm_esp.json"
Write-EspSummary -Dir $beaconPath -OutPath $espPath
Write-Host "Swarm started: $Agents agents" -ForegroundColor Green
Write-Host "Work depth: $WorkDepth | Chain-write: $ChainWrite" -ForegroundColor Yellow
Write-Host "Beacon dir: $beaconPath" -ForegroundColor Cyan
Write-Host "ESP summary: $espPath" -ForegroundColor Cyan
Write-Host "Chain state: $chainStateFile" -ForegroundColor Cyan
Write-Host "Use Get-Job to inspect raw jobs; ESP files show psjobtypename/state/hasmoredata/location/current file/time-in-code." -ForegroundColor Gray
Write-Host "Run: Stop-SwarmAgents to kill all; Remove-CompletedAgents to clean finished jobs." -ForegroundColor DarkGray
Write-Host "Chain-write persists progress; re-run with -AutoResume to continue interrupted work." -ForegroundColor DarkGray

# Helper: kill all swarm agents
function global:Stop-SwarmAgents {
    $running = Get-Job -Name 'agent*' -ErrorAction SilentlyContinue
    if ($running) {
        $running | Stop-Job -PassThru | Remove-Job -Force
        Write-Host "[OK] Stopped and removed $($running.Count) agent jobs." -ForegroundColor Yellow
    } else {
        Write-Host "[INFO] No agent jobs found." -ForegroundColor Gray
    }
}

# Helper: remove completed agent jobs (free RAM)
function global:Remove-CompletedAgents {
    $completed = Get-Job -Name 'agent*' -State Completed -ErrorAction SilentlyContinue
    if ($completed) {
        $completed | Remove-Job -Force
        Write-Host "[OK] Removed $($completed.Count) completed agent jobs." -ForegroundColor Green
    } else {
        Write-Host "[INFO] No completed agent jobs to remove." -ForegroundColor Gray
    }
}

if ($Watch) {
    while ($true) {
        Write-EspSummary -Dir $beaconPath -OutPath $espPath
        # Update chain state
        $chainState.LastUpdate = (Get-Date).ToString('o')
        $chainState.InProgress = $true
        Save-ChainState -State $chainState
        # Auto-remove completed jobs to free RAM
        Get-Job -Name 'agent*' -State Completed -ErrorAction SilentlyContinue | Remove-Job -Force -ErrorAction SilentlyContinue
        if (-not (Get-Job -Name 'agent*' -State Running -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Seconds 3
    }
    Write-EspSummary -Dir $beaconPath -OutPath $espPath
    # Mark chain complete
    $chainState.InProgress = $false
    $chainState.LastUpdate = (Get-Date).ToString('o')
    Save-ChainState -State $chainState
    Write-Host "[DONE] All agents finished. Chain state saved for review.\" -ForegroundColor Green
}
*** End File