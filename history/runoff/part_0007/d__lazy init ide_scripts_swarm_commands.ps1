#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick swarm commands - One-liners for common swarm operations

.DESCRIPTION
    Import this file to get quick swarm commands in your session:
    . .\scripts\swarm_commands.ps1

.EXAMPLE
    swarm-plan "Refactor GGUF loader"
    swarm-think "How to optimize token throughput"
    swarm-research "Find all uses of Win32IDE class"
    swarm-max
    swarm-deep "Analyze memory management patterns"
    swarm-build
    swarm-fix
    swarm-status
    swarm-kill
    swarm-clean
    swarm-memory
#>

$script:SwarmRoot = "D:\lazy init ide"
$script:SwarmModesScript = Join-Path $SwarmRoot "scripts\swarm_modes.ps1"
$script:SwarmBeaconScript = Join-Path $SwarmRoot "scripts\swarm_beacon_runner.ps1"

# ═══════════════════════════════════════════════════════════════════════════════
# QUICK SWARM LAUNCHERS
# ═══════════════════════════════════════════════════════════════════════════════

function global:swarm-plan {
    param([string]$Topic = "")
    Write-Host "🗺️  Launching PLANNING swarm..." -ForegroundColor Cyan
    & $script:SwarmModesScript -Mode Planning -Topic $Topic -SkipPrompt
}

function global:swarm-think {
    param([string]$Topic = "")
    Write-Host "🧠 Launching THINKING swarm..." -ForegroundColor Magenta
    & $script:SwarmModesScript -Mode Thinking -Topic $Topic -SkipPrompt
}

function global:swarm-ask {
    param([string]$Topic = "")
    Write-Host "❓ Launching ASKING swarm..." -ForegroundColor Yellow
    & $script:SwarmModesScript -Mode Asking -Topic $Topic -SkipPrompt
}

function global:swarm-research {
    param([string]$Topic = "")
    Write-Host "🔍 Launching RESEARCH swarm..." -ForegroundColor Green
    & $script:SwarmModesScript -Mode Research -Topic $Topic -SkipPrompt
}

function global:swarm-max {
    param([int]$Agents = 8)
    Write-Host "🚀 Launching MAX swarm ($Agents agents)..." -ForegroundColor Red
    & $script:SwarmModesScript -Mode Max -Agents $Agents -SkipPrompt
}

function global:swarm-deep {
    param([string]$Topic = "")
    Write-Host "🔬 Launching DEEP RESEARCH swarm..." -ForegroundColor Blue
    & $script:SwarmModesScript -Mode DeepResearch -Topic $Topic -SkipPrompt
}

function global:swarm-build {
    Write-Host "🔨 Launching BUILD swarm..." -ForegroundColor DarkYellow
    & $script:SwarmModesScript -Mode Build -SkipPrompt
}

function global:swarm-fix {
    param([string]$Topic = "")
    Write-Host "🔧 Launching FIX swarm..." -ForegroundColor DarkCyan
    & $script:SwarmModesScript -Mode Fix -Topic $Topic -SkipPrompt
}

# ═══════════════════════════════════════════════════════════════════════════════
# SWARM MANAGEMENT
# ═══════════════════════════════════════════════════════════════════════════════

function global:swarm-status {
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " SWARM STATUS" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    $jobs = Get-Job -Name 'agent*' -ErrorAction SilentlyContinue
    if (-not $jobs) {
        Write-Host "No active swarm agents." -ForegroundColor Gray
        return
    }
    
    $running = $jobs | Where-Object { $_.State -eq 'Running' }
    $completed = $jobs | Where-Object { $_.State -eq 'Completed' }
    $failed = $jobs | Where-Object { $_.State -eq 'Failed' }
    
    Write-Host "Running: $($running.Count)" -ForegroundColor Green
    Write-Host "Completed: $($completed.Count)" -ForegroundColor Gray
    Write-Host "Failed: $($failed.Count)" -ForegroundColor Red
    Write-Host ""
    
    # Show beacon status if available
    $beaconPath = Join-Path $script:SwarmRoot "logs/swarm_beacon"
    $espFile = Join-Path $beaconPath "swarm_esp.json"
    if (Test-Path $espFile) {
        try {
            $esp = Get-Content $espFile -Raw | ConvertFrom-Json
            Write-Host "Agent Details:" -ForegroundColor Yellow
            foreach ($agent in $esp) {
                $icon = switch ($agent.State) {
                    'Running' { '🟢' }
                    'Working' { '🔵' }
                    'Completed' { '⚪' }
                    'Failed' { '🔴' }
                    default { '⚫' }
                }
                $time = if ($agent.ActiveSeconds) { "$($agent.ActiveSeconds)s in code" } else { "" }
                Write-Host "  $icon Agent$($agent.AgentId): $($agent.State) | $($agent.CurrentFile) $time" -ForegroundColor DarkGray
            }
        } catch {}
    }
    
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    # Raw job table
    $jobs | Format-Table Id, Name, State, HasMoreData -AutoSize
}

function global:swarm-kill {
    Write-Host "☠️  Killing all swarm agents..." -ForegroundColor Red
    $jobs = Get-Job -Name 'agent*' -ErrorAction SilentlyContinue
    if ($jobs) {
        $jobs | Stop-Job -PassThru | Remove-Job -Force
        Write-Host "Killed $($jobs.Count) agents." -ForegroundColor Yellow
    } else {
        Write-Host "No agents to kill." -ForegroundColor Gray
    }
}

function global:swarm-clean {
    Write-Host "🧹 Cleaning completed agents..." -ForegroundColor Green
    $completed = Get-Job -Name 'agent*' -State Completed -ErrorAction SilentlyContinue
    if ($completed) {
        $completed | Remove-Job -Force
        Write-Host "Removed $($completed.Count) completed agents." -ForegroundColor Green
    } else {
        Write-Host "No completed agents to clean." -ForegroundColor Gray
    }
}

function global:swarm-output {
    param([int]$AgentId = 1)
    Write-Host "📄 Output from agent$AgentId:" -ForegroundColor Cyan
    $job = Get-Job -Name "agent$AgentId" -ErrorAction SilentlyContinue
    if ($job) {
        Receive-Job $job -Keep | Select-Object -Last 50
    } else {
        Write-Host "Agent not found." -ForegroundColor Red
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MEMORY COMMANDS
# ═══════════════════════════════════════════════════════════════════════════════

function global:swarm-memory {
    $memFile = Join-Path $script:SwarmRoot "logs/swarm_memory/global_memory.json"
    if (-not (Test-Path $memFile)) {
        Write-Host "No swarm memory found." -ForegroundColor Gray
        return
    }
    
    $mem = Get-Content $memFile -Raw | ConvertFrom-Json
    
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " SWARM MEMORY" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "Created: $($mem.CreatedAt)" -ForegroundColor Gray
    Write-Host "Updated: $($mem.LastUpdated)" -ForegroundColor Gray
    Write-Host "Sessions: $($mem.Sessions.Count)" -ForegroundColor Gray
    Write-Host ""
    
    if ($mem.KeyFindings -and $mem.KeyFindings.Count -gt 0) {
        Write-Host "KEY FINDINGS ($($mem.KeyFindings.Count)):" -ForegroundColor Green
        $mem.KeyFindings | Select-Object -Last 10 | ForEach-Object {
            Write-Host "  [$($_.Mode)] $($_.Entry)" -ForegroundColor DarkGreen
        }
        Write-Host ""
    }
    
    if ($mem.Decisions -and $mem.Decisions.Count -gt 0) {
        Write-Host "DECISIONS ($($mem.Decisions.Count)):" -ForegroundColor Yellow
        $mem.Decisions | Select-Object -Last 10 | ForEach-Object {
            Write-Host "  [$($_.Mode)] $($_.Entry)" -ForegroundColor DarkYellow
        }
        Write-Host ""
    }
    
    if ($mem.OpenQuestions -and $mem.OpenQuestions.Count -gt 0) {
        Write-Host "OPEN QUESTIONS ($($mem.OpenQuestions.Count)):" -ForegroundColor Red
        $mem.OpenQuestions | Select-Object -Last 10 | ForEach-Object {
            Write-Host "  [$($_.Mode)] $($_.Entry)" -ForegroundColor DarkRed
        }
        Write-Host ""
    }
    
    if ($mem.LastTopics -and $mem.LastTopics.Count -gt 0) {
        Write-Host "RECENT TOPICS:" -ForegroundColor Magenta
        $mem.LastTopics | ForEach-Object { Write-Host "  - $_" -ForegroundColor DarkMagenta }
    }
    
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

function global:swarm-remember {
    param(
        [Parameter(Mandatory = $true)][string]$What,
        [ValidateSet('Finding','Decision','Question')][string]$Type = 'Finding'
    )
    
    $memFile = Join-Path $script:SwarmRoot "logs/swarm_memory/global_memory.json"
    $memDir = Split-Path $memFile -Parent
    if (-not (Test-Path $memDir)) {
        New-Item -Path $memDir -ItemType Directory -Force | Out-Null
    }
    
    $mem = @{ KeyFindings = @(); Decisions = @(); OpenQuestions = @(); Sessions = @(); LastTopics = @() }
    if (Test-Path $memFile) {
        try { $mem = Get-Content $memFile -Raw | ConvertFrom-Json -AsHashtable } catch {}
    }
    
    $entry = @{
        Timestamp = (Get-Date).ToString('o')
        Entry = $What
        Source = "manual"
        Mode = "user"
    }
    
    $category = switch ($Type) {
        'Finding' { 'KeyFindings' }
        'Decision' { 'Decisions' }
        'Question' { 'OpenQuestions' }
    }
    
    if (-not $mem[$category]) { $mem[$category] = @() }
    $mem[$category] += $entry
    $mem.LastUpdated = (Get-Date).ToString('o')
    
    $mem | ConvertTo-Json -Depth 10 | Set-Content -Path $memFile -Encoding UTF8
    Write-Host "💾 Remembered $Type`: $What" -ForegroundColor Green
}

function global:swarm-forget {
    param([switch]$Confirm)
    if (-not $Confirm) {
        Write-Host "Use: swarm-forget -Confirm to clear all memory" -ForegroundColor Yellow
        return
    }
    $memFile = Join-Path $script:SwarmRoot "logs/swarm_memory/global_memory.json"
    Remove-Item $memFile -Force -ErrorAction SilentlyContinue
    Write-Host "🗑️  Cleared all swarm memory." -ForegroundColor Red
}

# ═══════════════════════════════════════════════════════════════════════════════
# HELP
# ═══════════════════════════════════════════════════════════════════════════════

function global:swarm-help {
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " SWARM COMMANDS" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "CONTROL CENTER:" -ForegroundColor Magenta
    Write-Host "  swarm-center            🐝 Launch full control center dashboard" -ForegroundColor White
    Write-Host ""
    Write-Host "LAUNCH MODES:" -ForegroundColor Yellow
    Write-Host "  swarm-plan 'topic'      🗺️  Planning & architecture" -ForegroundColor White
    Write-Host "  swarm-think 'topic'     🧠 Deep problem analysis" -ForegroundColor White
    Write-Host "  swarm-ask 'topic'       ❓ Clarification & questions" -ForegroundColor White
    Write-Host "  swarm-research 'topic'  🔍 Codebase exploration" -ForegroundColor White
    Write-Host "  swarm-max [agents]      🚀 Maximum parallel (default 8)" -ForegroundColor White
    Write-Host "  swarm-deep 'topic'      🔬 Deep research with persistence" -ForegroundColor White
    Write-Host "  swarm-build             🔨 Build error fixing" -ForegroundColor White
    Write-Host "  swarm-fix 'topic'       🔧 Bug fixing" -ForegroundColor White
    Write-Host ""
    Write-Host "MANAGEMENT:" -ForegroundColor Yellow
    Write-Host "  swarm-status            📊 Show agent status" -ForegroundColor White
    Write-Host "  swarm-output [id]       📄 Show agent output" -ForegroundColor White
    Write-Host "  swarm-kill              ☠️  Kill all agents" -ForegroundColor White
    Write-Host "  swarm-clean             🧹 Remove completed agents" -ForegroundColor White
    Write-Host ""
    Write-Host "MEMORY:" -ForegroundColor Yellow
    Write-Host "  swarm-memory            💾 Show swarm memory" -ForegroundColor White
    Write-Host "  swarm-remember 'x' [-Type Finding|Decision|Question]" -ForegroundColor White
    Write-Host "  swarm-forget -Confirm   🗑️  Clear all memory" -ForegroundColor White
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

function global:swarm-center {
    Write-Host "🐝 Launching Swarm Control Center..." -ForegroundColor Cyan
    $centerScript = Join-Path $script:SwarmRoot "scripts\swarm_control_center.ps1"
    & $centerScript
}

# Show loaded message
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host " SWARM COMMANDS LOADED" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "Type 'swarm-help' for available commands" -ForegroundColor Gray
Write-Host ""
