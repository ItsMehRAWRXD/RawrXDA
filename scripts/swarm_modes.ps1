#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Swarm Modes - Launch specialized agent swarms for different tasks

.DESCRIPTION
    Provides quick commands to launch swarms optimized for:
    - Planning: Architecture, design, and task breakdown
    - Thinking: Problem analysis and solution exploration
    - Asking: Interactive Q&A and clarification gathering
    - Research: Codebase exploration and documentation
    - Max: Maximum parallel agents, balanced depth
    - DeepResearch: Thorough analysis with chain-write persistence

.EXAMPLE
    .\swarm_modes.ps1 -Mode Planning -Agents 4
    .\swarm_modes.ps1 -Mode DeepResearch -Topic "GGUF loader optimization"
#>

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Planning','Thinking','Asking','Research','Max','DeepResearch','Build','Fix')]
    [string]$Mode,
    
    [int]$Agents = 4,
    [string]$RepoRoot = "D:\lazy init ide",
    [string]$Topic = "",
    [string]$MemoryFile = "",
    [switch]$SkipPrompt = $false,
    [switch]$Watch = $true
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# MEMORY SYSTEM - Persistent context across sessions
# ═══════════════════════════════════════════════════════════════════════════════

$memoryDir = Join-Path $RepoRoot "logs/swarm_memory"
if (-not (Test-Path $memoryDir)) {
    New-Item -Path $memoryDir -ItemType Directory -Force | Out-Null
}

$globalMemoryFile = Join-Path $memoryDir "global_memory.json"
$sessionMemoryFile = Join-Path $memoryDir "session_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"

function Get-SwarmMemory {
    if (Test-Path $globalMemoryFile) {
        try { return Get-Content $globalMemoryFile -Raw | ConvertFrom-Json -AsHashtable }
        catch { return @{} }
    }
    return @{
        CreatedAt = (Get-Date).ToString('o')
        Sessions = @()
        KeyFindings = @()
        Decisions = @()
        OpenQuestions = @()
        CodebaseKnowledge = @{}
        LastTopics = @()
    }
}

function Save-SwarmMemory {
    param([hashtable]$Memory)
    $Memory.LastUpdated = (Get-Date).ToString('o')
    $Memory | ConvertTo-Json -Depth 10 | Set-Content -Path $globalMemoryFile -Encoding UTF8
}

function Add-MemoryEntry {
    param(
        [string]$Category,  # KeyFindings, Decisions, OpenQuestions
        [string]$Entry,
        [string]$Source = "swarm"
    )
    $memory = Get-SwarmMemory
    if (-not $memory[$Category]) { $memory[$Category] = @() }
    $memory[$Category] += @{
        Timestamp = (Get-Date).ToString('o')
        Entry = $Entry
        Source = $Source
        Mode = $Mode
    }
    Save-SwarmMemory -Memory $memory
}

# Load existing memory
$swarmMemory = Get-SwarmMemory

# Add current topic to memory
if ($Topic) {
    if (-not $swarmMemory.LastTopics) { $swarmMemory.LastTopics = @() }
    $swarmMemory.LastTopics = @($Topic) + ($swarmMemory.LastTopics | Select-Object -First 9)
    Save-SwarmMemory -Memory $swarmMemory
}

# ═══════════════════════════════════════════════════════════════════════════════
# MODE-SPECIFIC CONTEXTS
# ═══════════════════════════════════════════════════════════════════════════════

$modeContexts = @{
    'Planning' = @"
You are a RawrXD Planning Agent specializing in architecture and task decomposition.

ROLE: Break down complex tasks into actionable, ordered steps.

CAPABILITIES:
- Analyze requirements and identify dependencies
- Create implementation roadmaps with milestones
- Identify risks and blockers early
- Estimate complexity and effort
- Define clear acceptance criteria

OUTPUT FORMAT:
1. Summary of the goal
2. Prerequisites and dependencies
3. Ordered task list with estimates
4. Risk assessment
5. Success criteria

MEMORY CONTEXT:
Previous topics: $($swarmMemory.LastTopics -join ', ')
Key decisions: $($swarmMemory.Decisions | Select-Object -Last 5 | ForEach-Object { $_.Entry } | Out-String)
"@

    'Thinking' = @"
You are a RawrXD Thinking Agent specializing in deep problem analysis.

ROLE: Explore solution spaces, weigh tradeoffs, and reason through complex problems.

CAPABILITIES:
- Multi-perspective analysis (performance, maintainability, security)
- Tradeoff evaluation with pros/cons
- Edge case identification
- Alternative solution generation
- Root cause analysis

APPROACH:
1. Restate the problem clearly
2. List what we know vs. what we assume
3. Generate 3+ possible approaches
4. Evaluate each against constraints
5. Recommend with reasoning

MEMORY CONTEXT:
Open questions: $($swarmMemory.OpenQuestions | Select-Object -Last 5 | ForEach-Object { $_.Entry } | Out-String)
"@

    'Asking' = @"
You are a RawrXD Asking Agent specializing in clarification and information gathering.

ROLE: Identify gaps in requirements and formulate precise questions.

CAPABILITIES:
- Spot ambiguities in specifications
- Generate clarifying questions
- Prioritize questions by impact
- Synthesize answers into actionable items
- Track answered vs. unanswered questions

OUTPUT FORMAT:
1. What we understand so far
2. Critical questions (blocking)
3. Important questions (needed soon)
4. Nice-to-know questions
5. Suggested defaults if no answer

MEMORY CONTEXT:
Recent findings: $($swarmMemory.KeyFindings | Select-Object -Last 5 | ForEach-Object { $_.Entry } | Out-String)
"@

    'Research' = @"
You are a RawrXD Research Agent specializing in codebase exploration.

ROLE: Discover, document, and explain existing code patterns and architecture.

CAPABILITIES:
- Trace code paths and dependencies
- Document undocumented code
- Find usage patterns and examples
- Map architecture and data flow
- Identify technical debt

OUTPUT FORMAT:
1. Executive summary
2. Key components discovered
3. Dependencies and relationships
4. Code patterns observed
5. Recommendations

MEMORY CONTEXT:
Codebase knowledge: $($swarmMemory.CodebaseKnowledge.Keys -join ', ')
"@

    'Max' = @"
You are a RawrXD Max-Parallel Agent optimized for high-throughput work.

ROLE: Execute tasks rapidly across multiple parallel tracks.

CAPABILITIES:
- Parallel file processing
- Batch operations
- Quick fixes and refactoring
- Mass updates with consistency
- Progress tracking across agents

COORDINATION:
- Check beacon files before starting work on a file
- Update beacon immediately when claiming a file
- Release beacon when done or on error
- Never work on files another agent has claimed

MEMORY CONTEXT:
Last session: $($swarmMemory.Sessions | Select-Object -Last 1 | ForEach-Object { $_.Summary } | Out-String)
"@

    'DeepResearch' = @"
You are a RawrXD Deep Research Agent with persistent memory and chain-write capability.

ROLE: Conduct thorough, exhaustive analysis that may span multiple sessions.

CAPABILITIES:
- Multi-session persistent research
- Chain-write for long analyses
- Comprehensive documentation
- Cross-reference with codebase
- Build institutional knowledge

PERSISTENCE:
- Save findings incrementally to memory
- Resume from last checkpoint on restart
- Build on previous research sessions
- Maintain research trail for audit

CURRENT TOPIC: $Topic

MEMORY CONTEXT (FULL):
Key Findings:
$($swarmMemory.KeyFindings | ForEach-Object { "- [$($_.Timestamp)] $($_.Entry)" } | Out-String)

Decisions Made:
$($swarmMemory.Decisions | ForEach-Object { "- [$($_.Timestamp)] $($_.Entry)" } | Out-String)

Open Questions:
$($swarmMemory.OpenQuestions | ForEach-Object { "- [$($_.Timestamp)] $($_.Entry)" } | Out-String)
"@

    'Build' = @"
You are a RawrXD Build Agent specializing in compilation and linking.

ROLE: Ensure clean builds with zero errors and zero warnings.

CAPABILITIES:
- Parse and fix build errors
- Resolve linker issues
- Fix warning-as-error violations
- Manage include dependencies
- Deploy runtime dependencies

PRIORITY:
1. C/C++ compile errors
2. Linker errors (LNK*)
3. Warnings (treat as errors)
4. Qt MOC warnings
5. CMake configuration issues

MEMORY CONTEXT:
Recent build issues: $($swarmMemory.KeyFindings | Where-Object { $_.Source -eq 'build' } | Select-Object -Last 5 | ForEach-Object { $_.Entry } | Out-String)
"@

    'Fix' = @"
You are a RawrXD Fix Agent specializing in bug fixing and error resolution.

ROLE: Diagnose and fix issues with minimal side effects.

CAPABILITIES:
- Root cause analysis
- Minimal-change fixes
- Regression prevention
- Test coverage addition
- Documentation of fixes

APPROACH:
1. Reproduce the issue
2. Identify root cause
3. Design minimal fix
4. Verify fix works
5. Check for regressions
6. Document the fix

MEMORY CONTEXT:
Previous fixes: $($swarmMemory.Decisions | Where-Object { $_.Source -eq 'fix' } | Select-Object -Last 5 | ForEach-Object { $_.Entry } | Out-String)
"@
}

# ═══════════════════════════════════════════════════════════════════════════════
# MODE CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

$modeConfig = @{
    'Planning'     = @{ Depth = 'Balanced'; ChainWrite = $false; Agents = [Math]::Min($Agents, 2) }
    'Thinking'     = @{ Depth = 'Thorough'; ChainWrite = $true;  Agents = [Math]::Min($Agents, 2) }
    'Asking'       = @{ Depth = 'Scaffold'; ChainWrite = $false; Agents = 1 }
    'Research'     = @{ Depth = 'Balanced'; ChainWrite = $false; Agents = $Agents }
    'Max'          = @{ Depth = 'Balanced'; ChainWrite = $false; Agents = [Math]::Max($Agents, 8) }
    'DeepResearch' = @{ Depth = 'Thorough'; ChainWrite = $true;  Agents = [Math]::Min($Agents, 2) }
    'Build'        = @{ Depth = 'Balanced'; ChainWrite = $false; Agents = 1 }
    'Fix'          = @{ Depth = 'Thorough'; ChainWrite = $true;  Agents = 1 }
}

$config = $modeConfig[$Mode]
$context = $modeContexts[$Mode]

# Record session start
$sessionInfo = @{
    Mode = $Mode
    Topic = $Topic
    StartedAt = (Get-Date).ToString('o')
    Agents = $config.Agents
    Summary = ""
}
$swarmMemory.Sessions += $sessionInfo
Save-SwarmMemory -Memory $swarmMemory

# ═══════════════════════════════════════════════════════════════════════════════
# LAUNCH SWARM
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host " SWARM MODE: $Mode" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Agents: $($config.Agents)" -ForegroundColor Yellow
Write-Host "Depth: $($config.Depth)" -ForegroundColor Yellow
Write-Host "Chain-Write: $($config.ChainWrite)" -ForegroundColor Yellow
if ($Topic) { Write-Host "Topic: $Topic" -ForegroundColor Yellow }
Write-Host ""
Write-Host "Context:" -ForegroundColor Gray
Write-Host $context -ForegroundColor DarkGray
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# Build arguments for beacon runner
$beaconArgs = @(
    "-Agents", $config.Agents,
    "-RepoRoot", "`"$RepoRoot`"",
    "-WorkDepth", $config.Depth,
    "-CustomContext", "`"$($context -replace '"', '\"')`"",
    "-SkipPrompt"
)

if ($config.ChainWrite) { $beaconArgs += "-ChainWrite" }
if ($Watch) { $beaconArgs += "-Watch" }

$beaconScript = Join-Path $RepoRoot "scripts\swarm_beacon_runner.ps1"

Write-Host "Launching swarm..." -ForegroundColor Green
$cmd = "pwsh -NoProfile -File `"$beaconScript`" $($beaconArgs -join ' ')"
Write-Host $cmd -ForegroundColor DarkGray

Invoke-Expression $cmd

# ═══════════════════════════════════════════════════════════════════════════════
# HELPER FUNCTIONS (exported to global scope)
# ═══════════════════════════════════════════════════════════════════════════════

function global:Add-SwarmFinding {
    param([string]$Finding)
    Add-MemoryEntry -Category "KeyFindings" -Entry $Finding -Source $Mode.ToLower()
    Write-Host "[MEMORY] Added finding: $Finding" -ForegroundColor Green
}

function global:Add-SwarmDecision {
    param([string]$Decision)
    Add-MemoryEntry -Category "Decisions" -Entry $Decision -Source $Mode.ToLower()
    Write-Host "[MEMORY] Added decision: $Decision" -ForegroundColor Green
}

function global:Add-SwarmQuestion {
    param([string]$Question)
    Add-MemoryEntry -Category "OpenQuestions" -Entry $Question -Source $Mode.ToLower()
    Write-Host "[MEMORY] Added question: $Question" -ForegroundColor Yellow
}

function global:Get-SwarmMemorySummary {
    $mem = Get-SwarmMemory
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " SWARM MEMORY SUMMARY" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "Sessions: $($mem.Sessions.Count)" -ForegroundColor Gray
    Write-Host "Key Findings: $($mem.KeyFindings.Count)" -ForegroundColor Green
    Write-Host "Decisions: $($mem.Decisions.Count)" -ForegroundColor Yellow
    Write-Host "Open Questions: $($mem.OpenQuestions.Count)" -ForegroundColor Red
    Write-Host ""
    Write-Host "Last 5 Findings:" -ForegroundColor Green
    $mem.KeyFindings | Select-Object -Last 5 | ForEach-Object { Write-Host "  - $($_.Entry)" -ForegroundColor DarkGreen }
    Write-Host ""
    Write-Host "Last 5 Decisions:" -ForegroundColor Yellow
    $mem.Decisions | Select-Object -Last 5 | ForEach-Object { Write-Host "  - $($_.Entry)" -ForegroundColor DarkYellow }
    Write-Host ""
    Write-Host "Open Questions:" -ForegroundColor Red
    $mem.OpenQuestions | Select-Object -Last 5 | ForEach-Object { Write-Host "  - $($_.Entry)" -ForegroundColor DarkRed }
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

function global:Clear-SwarmMemory {
    param([switch]$Confirm)
    if (-not $Confirm) {
        Write-Host "Use -Confirm to actually clear memory" -ForegroundColor Yellow
        return
    }
    Remove-Item $globalMemoryFile -Force -ErrorAction SilentlyContinue
    Write-Host "[MEMORY] Cleared all swarm memory" -ForegroundColor Red
}

Write-Host ""
Write-Host "Memory commands available:" -ForegroundColor Gray
Write-Host "  Add-SwarmFinding 'your finding'" -ForegroundColor DarkGray
Write-Host "  Add-SwarmDecision 'your decision'" -ForegroundColor DarkGray
Write-Host "  Add-SwarmQuestion 'your question'" -ForegroundColor DarkGray
Write-Host "  Get-SwarmMemorySummary" -ForegroundColor DarkGray
Write-Host "  Clear-SwarmMemory -Confirm" -ForegroundColor DarkGray
