#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Swarm Control Center - Launch, configure, and monitor agent swarms

.DESCRIPTION
    Unified swarm management interface with:
    - Real-time agent dashboard
    - Model assignment (BigDaddyG, Quantum, Cheetah, etc.)
    - Fine-tuning per-agent role configuration
    - Memory and context persistence
    - Performance metrics

.EXAMPLE
    .\swarm_control_center.ps1
    .\swarm_control_center.ps1 -LaunchPreset DeepResearch
#>

param(
    [string]$LaunchPreset = "",
    [switch]$Dashboard = $false,
    [switch]$Silent = $false
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

$script:SwarmRoot = "D:\lazy init ide"
$script:ConfigDir = Join-Path $SwarmRoot "logs/swarm_config"
$script:BeaconDir = Join-Path $SwarmRoot "logs/swarm_beacon"
$script:MemoryDir = Join-Path $SwarmRoot "logs/swarm_memory"
$script:ModelsConfigFile = Join-Path $ConfigDir "models.json"
$script:AgentPresetsFile = Join-Path $ConfigDir "agent_presets.json"
$script:SwarmStateFile = Join-Path $ConfigDir "swarm_state.json"

# Ensure directories exist
@($ConfigDir, $BeaconDir, $MemoryDir) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -Path $_ -ItemType Directory -Force | Out-Null }
}

# Load model sources module
$modulePath = Join-Path $SwarmRoot "scripts\model_sources.ps1"
if (Test-Path $modulePath) { . $modulePath }

# ═══════════════════════════════════════════════════════════════════════════════
# MODEL REGISTRY - Define available models and their characteristics
# ═══════════════════════════════════════════════════════════════════════════════

$defaultModels = @{
    "BigDaddyG" = @{
        Description = "Large 70B+ model for complex reasoning and architecture"
        Size = "74B (Q4_K quantized = ~42GB)"
        Speed = "15-25 tps"
        VRAM = "24GB+"
        BestFor = @("Architecture", "Deep Research", "Complex Planning")
        GGUFPath = ""  # Set your path
        ContextSize = 8192
        Layers = 80
        QuantType = "Q4_K_M"
    }
    "Quantum" = @{
        Description = "Fast 7B model for quick agent tasks"
        Size = "7B (Q4_K = ~4GB)"
        Speed = "70-100 tps"
        VRAM = "6GB"
        BestFor = @("Agents", "Quick Fixes", "Code Gen")
        GGUFPath = ""
        ContextSize = 4096
        Layers = 32
        QuantType = "Q4_K_M"
    }
    "Cheetah" = @{
        Description = "Ultra-fast 1.5B model for parallel swarm agents"
        Size = "1.5B (Q8_0 = ~1.6GB)"
        Speed = "150-200 tps"
        VRAM = "2GB"
        BestFor = @("Swarm Agents", "Parallel Tasks", "Scanning")
        GGUFPath = ""
        ContextSize = 2048
        Layers = 24
        QuantType = "Q8_0"
    }
    "Panther" = @{
        Description = "Balanced 13B model for general tasks"
        Size = "13B (Q4_K = ~8GB)"
        Speed = "50-70 tps"
        VRAM = "10GB"
        BestFor = @("General", "Code Review", "Documentation")
        GGUFPath = ""
        ContextSize = 4096
        Layers = 40
        QuantType = "Q4_K_M"
    }
    "Titan" = @{
        Description = "Massive 120B+ model for ultimate quality (zone-loaded)"
        Size = "120B (Q2_K = ~45GB, zone-loaded)"
        Speed = "5-10 tps"
        VRAM = "64GB (with zone swap)"
        BestFor = @("Final Review", "Critical Decisions", "Architecture Validation")
        GGUFPath = ""
        ContextSize = 16384
        Layers = 96
        QuantType = "Q2_K"
        ZoneLoaded = $true
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# AGENT PRESETS - Pre-configured agent roles
# ═══════════════════════════════════════════════════════════════════════════════

$defaultPresets = @{
    "Architect" = @{
        Model = "BigDaddyG"
        Context = "You are the Chief Architect Agent. Design systems, define interfaces, make structural decisions."
        MaxTokens = 4096
        Temperature = 0.3
        Role = "Planning & Architecture"
        Priority = 1
    }
    "Coder" = @{
        Model = "Quantum"
        Context = "You are a Coder Agent. Write clean, efficient, working code. No placeholders."
        MaxTokens = 2048
        Temperature = 0.2
        Role = "Code Generation"
        Priority = 2
    }
    "Scout" = @{
        Model = "Cheetah"
        Context = "You are a Scout Agent. Rapidly scan code, find patterns, report findings."
        MaxTokens = 512
        Temperature = 0.1
        Role = "Codebase Scanning"
        Priority = 3
    }
    "Reviewer" = @{
        Model = "Panther"
        Context = "You are a Code Reviewer Agent. Find bugs, suggest improvements, ensure quality."
        MaxTokens = 2048
        Temperature = 0.3
        Role = "Code Review"
        Priority = 2
    }
    "Fixer" = @{
        Model = "Quantum"
        Context = "You are a Fixer Agent. Fix bugs with minimal changes. Test your fixes."
        MaxTokens = 1024
        Temperature = 0.1
        Role = "Bug Fixing"
        Priority = 1
    }
    "Researcher" = @{
        Model = "BigDaddyG"
        Context = "You are a Research Agent. Conduct deep analysis, document findings, build knowledge."
        MaxTokens = 4096
        Temperature = 0.4
        Role = "Deep Research"
        Priority = 2
    }
    "Worker" = @{
        Model = "Cheetah"
        Context = "You are a Worker Agent. Execute tasks quickly and accurately. Report progress."
        MaxTokens = 512
        Temperature = 0.1
        Role = "Parallel Execution"
        Priority = 3
    }
    "Oracle" = @{
        Model = "Titan"
        Context = "You are the Oracle Agent. Provide final validation on critical decisions. Quality over speed."
        MaxTokens = 8192
        Temperature = 0.2
        Role = "Final Validation"
        Priority = 1
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# STATE MANAGEMENT
# ═══════════════════════════════════════════════════════════════════════════════

function Get-SwarmState {
    if (Test-Path $SwarmStateFile) {
        try { return Get-Content $SwarmStateFile -Raw | ConvertFrom-Json -AsHashtable }
        catch { }
    }
    return @{
        MaxAgents = 16
        ActiveAgents = 0
        AgentSlots = @{}
        ModelAssignments = @{}
        LaunchHistory = @()
        TotalTokensProcessed = 0
        TotalSessionTime = 0
        CreatedAt = (Get-Date).ToString('o')
    }
}

function Save-SwarmState {
    param([hashtable]$State)
    $State.LastUpdated = (Get-Date).ToString('o')
    $State | ConvertTo-Json -Depth 10 | Set-Content -Path $SwarmStateFile -Encoding UTF8
}

function Get-ModelsConfig {
    if (Test-Path $ModelsConfigFile) {
        try { return Get-Content $ModelsConfigFile -Raw | ConvertFrom-Json -AsHashtable }
        catch { }
    }
    # Save defaults and return
    $defaultModels | ConvertTo-Json -Depth 10 | Set-Content -Path $ModelsConfigFile -Encoding UTF8
    return $defaultModels
}

function Get-AgentPresets {
    if (Test-Path $AgentPresetsFile) {
        try { return Get-Content $AgentPresetsFile -Raw | ConvertFrom-Json -AsHashtable }
        catch { }
    }
    # Save defaults and return
    $defaultPresets | ConvertTo-Json -Depth 10 | Set-Content -Path $AgentPresetsFile -Encoding UTF8
    return $defaultPresets
}

# ═══════════════════════════════════════════════════════════════════════════════
# SYSTEM DETECTION
# ═══════════════════════════════════════════════════════════════════════════════

function Get-SystemCapabilities {
    $caps = @{
        TotalRAM = 0
        AvailableRAM = 0
        CPUCores = 0
        GPUName = ""
        GPUVRAM = 0
        MaxConcurrentAgents = 4
        RecommendedModel = "Quantum"
    }
    
    try {
        $os = Get-CimInstance Win32_OperatingSystem
        $caps.TotalRAM = [math]::Round($os.TotalVisibleMemorySize / 1MB, 1)
        $caps.AvailableRAM = [math]::Round($os.FreePhysicalMemory / 1MB, 1)
        
        $cpu = Get-CimInstance Win32_Processor
        $caps.CPUCores = ($cpu | Measure-Object -Property NumberOfCores -Sum).Sum
        
        # Try to detect GPU
        $gpu = Get-CimInstance Win32_VideoController | Where-Object { $_.Name -match 'NVIDIA|AMD|Intel' } | Select-Object -First 1
        if ($gpu) {
            $caps.GPUName = $gpu.Name
            $caps.GPUVRAM = [math]::Round($gpu.AdapterRAM / 1GB, 1)
        }
        
        # Calculate max agents based on resources
        $caps.MaxConcurrentAgents = [math]::Min(
            [math]::Floor($caps.AvailableRAM / 512),  # 512 per agent minimum
            $caps.CPUCores * 2,                      # 2 agents per core
            16                                        # Hard cap
        )
        
        # Recommend model based on VRAM
        if ($caps.GPUVRAM -ge 24) {
            $caps.RecommendedModel = "BigDaddyG"
        } elseif ($caps.GPUVRAM -ge 10) {
            $caps.RecommendedModel = "Panther"
        } elseif ($caps.GPUVRAM -ge 6) {
            $caps.RecommendedModel = "Quantum"
        } else {
            $caps.RecommendedModel = "Cheetah"
        }
    } catch { }
    
    return $caps
}

# ═══════════════════════════════════════════════════════════════════════════════
# DASHBOARD DISPLAY
# ═══════════════════════════════════════════════════════════════════════════════

function Show-SwarmDashboard {
    $state = Get-SwarmState
    $caps = Get-SystemCapabilities
    $models = Get-ModelsConfig
    $presets = Get-AgentPresets
    $jobs = Get-Job -Name 'agent*' -ErrorAction SilentlyContinue
    
    $running = ($jobs | Where-Object { $_.State -eq 'Running' }).Count
    $completed = ($jobs | Where-Object { $_.State -eq 'Completed' }).Count
    $failed = ($jobs | Where-Object { $_.State -eq 'Failed' }).Count
    
    Clear-Host
    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                     🐝 RAWRXD SWARM CONTROL CENTER 🐝                         ║" -ForegroundColor Cyan
    Write-Host "╠═══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    
    # Agent Status Bar
    $agentBar = ""
    for ($i = 0; $i -lt $caps.MaxConcurrentAgents; $i++) {
        if ($i -lt $running) { $agentBar += "🟢" }
        elseif ($i -lt ($running + $completed)) { $agentBar += "⚪" }
        elseif ($i -lt ($running + $completed + $failed)) { $agentBar += "🔴" }
        else { $agentBar += "⚫" }
    }
    
    $statusLine = "  AGENTS: $agentBar  [$running active / $($caps.MaxConcurrentAgents) max]"
    Write-Host "║$($statusLine.PadRight(79))║" -ForegroundColor Cyan
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    Write-Host "╠═══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    
    # System Resources
    Write-Host "║  SYSTEM RESOURCES                                                             ║" -ForegroundColor Yellow
    $ramLine = "    RAM: $($caps.AvailableRAM)GB free / $($caps.TotalRAM)GB total"
    Write-Host "║$($ramLine.PadRight(79))║" -ForegroundColor Gray
    $cpuLine = "    CPU: $($caps.CPUCores) cores"
    Write-Host "║$($cpuLine.PadRight(79))║" -ForegroundColor Gray
    $gpuLine = "    GPU: $($caps.GPUName) ($($caps.GPUVRAM)GB VRAM)"
    Write-Host "║$($gpuLine.PadRight(79))║" -ForegroundColor Gray
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    
    Write-Host "╠═══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    
    # Available Models
    Write-Host "║  AVAILABLE MODELS                                                             ║" -ForegroundColor Yellow
    foreach ($modelName in $models.Keys | Sort-Object) {
        $m = $models[$modelName]
        $icon = switch ($modelName) {
            "BigDaddyG" { "🦍" }
            "Quantum"   { "⚡" }
            "Cheetah"   { "🐆" }
            "Panther"   { "🐆" }
            "Titan"     { "🗿" }
            default     { "🤖" }
        }
        $rec = if ($modelName -eq $caps.RecommendedModel) { "★" } else { " " }
        $modelLine = "    $rec $icon $($modelName.PadRight(12)) $($m.Speed.PadRight(12)) $($m.BestFor[0])"
        Write-Host "║$($modelLine.PadRight(79))║" -ForegroundColor Gray
    }
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    
    Write-Host "╠═══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    
    # Agent Presets
    Write-Host "║  AGENT PRESETS                                                                ║" -ForegroundColor Yellow
    foreach ($presetName in $presets.Keys | Sort-Object { $presets[$_].Priority }) {
        $p = $presets[$presetName]
        $presetLine = "    [$($presetName.PadRight(10))] Model: $($p.Model.PadRight(10)) Role: $($p.Role)"
        Write-Host "║$($presetLine.PadRight(79))║" -ForegroundColor Gray
    }
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    
    Write-Host "╠═══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    
    # Active Agents Detail
    if ($jobs -and $jobs.Count -gt 0) {
        Write-Host "║  ACTIVE AGENTS                                                                ║" -ForegroundColor Yellow
        
        # Try to read beacon data
        $espFile = Join-Path $BeaconDir "swarm_esp.json"
        $beaconData = @{}
        if (Test-Path $espFile) {
            try {
                $esp = Get-Content $espFile -Raw | ConvertFrom-Json
                foreach ($a in $esp) { $beaconData["agent$($a.AgentId)"] = $a }
            } catch {}
        }
        
        foreach ($job in ($jobs | Where-Object { $_.State -eq 'Running' })) {
            $beacon = $beaconData[$job.Name]
            $file = if ($beacon) { $beacon.CurrentFile } else { "(unknown)" }
            $time = if ($beacon) { "$($beacon.ActiveSeconds)s" } else { "" }
            $agentLine = "    🟢 $($job.Name.PadRight(8)) | $($file.PadRight(30)) | $time"
            Write-Host "║$($agentLine.PadRight(79))║" -ForegroundColor Green
        }
        Write-Host "║                                                                               ║" -ForegroundColor Cyan
    }
    
    Write-Host "╠═══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
    Write-Host "║  COMMANDS                                                                     ║" -ForegroundColor Yellow
    Write-Host "║    [1] Launch Planning Swarm      [5] Launch Max Swarm (8 agents)            ║" -ForegroundColor White
    Write-Host "║    [2] Launch Thinking Swarm      [6] Launch Deep Research Swarm             ║" -ForegroundColor White
    Write-Host "║    [3] Launch Research Swarm      [7] Configure Models                       ║" -ForegroundColor White
    Write-Host "║    [4] Launch Build/Fix Swarm     [8] Create Custom Preset                   ║" -ForegroundColor White
    Write-Host "║    [9] Manage Model Sources       [M] Model/Agent Making Station 🏭          ║" -ForegroundColor White
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    Write-Host "║    [S] Status Refresh   [K] Kill All   [C] Clean Completed   [Q] Quit        ║" -ForegroundColor DarkGray
    Write-Host "║                                                                               ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

function Show-ModelConfig {
    $models = Get-ModelsConfig
    
    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                         MODEL CONFIGURATION                                   ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
    
    $i = 1
    $modelList = @()
    foreach ($name in $models.Keys | Sort-Object) {
        $m = $models[$name]
        $modelList += $name
        Write-Host "[$i] $name" -ForegroundColor Yellow
        Write-Host "    Size: $($m.Size)" -ForegroundColor Gray
        Write-Host "    Speed: $($m.Speed)" -ForegroundColor Gray
        Write-Host "    Best For: $($m.BestFor -join ', ')" -ForegroundColor Gray
        Write-Host "    GGUF: $(if ($m.GGUFPath) { $m.GGUFPath } else { '(not set)' })" -ForegroundColor $(if ($m.GGUFPath) { 'Green' } else { 'Red' })
        Write-Host ""
        $i++
    }
    
    Write-Host "Enter model number to configure, or [B] to go back: " -NoNewline -ForegroundColor Cyan
    $choice = Read-Host
    
    if ($choice -eq 'B' -or $choice -eq 'b') { return }
    
    $idx = [int]$choice - 1
    if ($idx -ge 0 -and $idx -lt $modelList.Count) {
        $modelName = $modelList[$idx]
        Write-Host ""
        Write-Host "Configuring $modelName" -ForegroundColor Yellow
        Write-Host "Current GGUF path: $($models[$modelName].GGUFPath)" -ForegroundColor Gray
        Write-Host "Enter new GGUF path (or Enter to keep current): " -NoNewline -ForegroundColor Cyan
        $newPath = Read-Host
        if ($newPath) {
            $models[$modelName].GGUFPath = $newPath
            $models | ConvertTo-Json -Depth 10 | Set-Content -Path $ModelsConfigFile -Encoding UTF8
            Write-Host "✓ Updated!" -ForegroundColor Green
        }
    }
}

function Show-CreatePreset {
    $models = Get-ModelsConfig
    $presets = Get-AgentPresets
    
    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                         CREATE CUSTOM AGENT PRESET                            ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
    
    Write-Host "Preset Name: " -NoNewline -ForegroundColor Cyan
    $name = Read-Host
    if (-not $name) { return }
    
    Write-Host ""
    Write-Host "Available Models:" -ForegroundColor Yellow
    $i = 1
    $modelList = @()
    foreach ($m in $models.Keys | Sort-Object) {
        $modelList += $m
        Write-Host "  [$i] $m - $($models[$m].Speed)" -ForegroundColor Gray
        $i++
    }
    Write-Host "Select model [1-$($modelList.Count)]: " -NoNewline -ForegroundColor Cyan
    $modelChoice = Read-Host
    $modelIdx = [int]$modelChoice - 1
    $selectedModel = if ($modelIdx -ge 0 -and $modelIdx -lt $modelList.Count) { $modelList[$modelIdx] } else { "Quantum" }
    
    Write-Host ""
    Write-Host "Role description (e.g., 'Code Review'): " -NoNewline -ForegroundColor Cyan
    $role = Read-Host
    if (-not $role) { $role = "Custom Agent" }
    
    Write-Host ""
    Write-Host "Agent context (what the agent should know/do):" -ForegroundColor Cyan
    Write-Host "(Press Enter twice to finish)" -ForegroundColor Gray
    $contextLines = @()
    while ($true) {
        $line = Read-Host
        if (-not $line -and $contextLines.Count -gt 0) { break }
        if ($line) { $contextLines += $line }
    }
    $context = $contextLines -join "`n"
    if (-not $context) { $context = "You are a $name agent. $role." }
    
    Write-Host ""
    Write-Host "Max tokens [2048]: " -NoNewline -ForegroundColor Cyan
    $maxTokens = Read-Host
    if (-not $maxTokens) { $maxTokens = 2048 }
    
    Write-Host "Temperature [0.2]: " -NoNewline -ForegroundColor Cyan
    $temp = Read-Host
    if (-not $temp) { $temp = 0.2 }
    
    # Save preset
    $presets[$name] = @{
        Model = $selectedModel
        Context = $context
        MaxTokens = [int]$maxTokens
        Temperature = [double]$temp
        Role = $role
        Priority = 2
        Custom = $true
    }
    
    $presets | ConvertTo-Json -Depth 10 | Set-Content -Path $AgentPresetsFile -Encoding UTF8
    
    Write-Host ""
    Write-Host "✓ Created preset: $name" -ForegroundColor Green
    Write-Host "  Model: $selectedModel" -ForegroundColor Gray
    Write-Host "  Role: $role" -ForegroundColor Gray
    Write-Host ""
    Read-Host "Press Enter to continue"
}

function Invoke-SwarmLaunch {
    param(
        [string]$Mode,
        [string]$Preset = "",
        [int]$AgentCount = 4
    )
    
    $modesScript = Join-Path $SwarmRoot "scripts\swarm_modes.ps1"
    
    Write-Host ""
    Write-Host "🚀 Launching $Mode swarm with $AgentCount agents..." -ForegroundColor Green
    
    $topic = ""
    Write-Host "Topic (optional, press Enter to skip): " -NoNewline -ForegroundColor Cyan
    $topic = Read-Host
    
    $args = @("-Mode", $Mode, "-Agents", $AgentCount, "-SkipPrompt")
    if ($topic) { $args += @("-Topic", "`"$topic`"") }
    
    & $modesScript @args
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN LOOP
# ═══════════════════════════════════════════════════════════════════════════════

if ($LaunchPreset) {
    # Direct launch mode
    Invoke-SwarmLaunch -Mode $LaunchPreset
    exit 0
}

# Interactive dashboard mode
while ($true) {
    Show-SwarmDashboard
    
    Write-Host ""
    Write-Host "Enter command: " -NoNewline -ForegroundColor Cyan
    $cmd = Read-Host
    
    switch ($cmd.ToUpper()) {
        '1' { Invoke-SwarmLaunch -Mode "Planning" -AgentCount 2 }
        '2' { Invoke-SwarmLaunch -Mode "Thinking" -AgentCount 2 }
        '3' { Invoke-SwarmLaunch -Mode "Research" -AgentCount 4 }
        '4' { Invoke-SwarmLaunch -Mode "Build" -AgentCount 1 }
        '5' { Invoke-SwarmLaunch -Mode "Max" -AgentCount 8 }
        '6' { Invoke-SwarmLaunch -Mode "DeepResearch" -AgentCount 2 }
        '7' { Show-ModelConfig }
        '8' { Show-CreatePreset }
        '9' { Show-ModelSources }
        'M' {
            $makingStationScript = Join-Path $SwarmRoot "scripts\model_agent_making_station.ps1"
            if (Test-Path $makingStationScript) {
                & $makingStationScript
            } else {
                Write-Host "Making Station not found at: $makingStationScript" -ForegroundColor Red
                Start-Sleep -Seconds 2
            }
        }
        'S' { continue }  # Refresh
        'K' {
            $jobs = Get-Job -Name 'agent*' -ErrorAction SilentlyContinue
            if ($jobs) { $jobs | Stop-Job -PassThru | Remove-Job -Force }
            Write-Host "☠️ Killed all agents" -ForegroundColor Red
            Start-Sleep -Seconds 1
        }
        'C' {
            $completed = Get-Job -Name 'agent*' -State Completed -ErrorAction SilentlyContinue
            if ($completed) { $completed | Remove-Job -Force }
            Write-Host "🧹 Cleaned completed agents" -ForegroundColor Green
            Start-Sleep -Seconds 1
        }
        'Q' { exit 0 }
        default { }
    }
}
