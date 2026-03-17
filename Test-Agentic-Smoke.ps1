#requires -Version 5.1
<#
.SYNOPSIS
    Full smoke test for agentic/autonomous features (command palette, autonomy, SubAgent).
.DESCRIPTION
    1. Code checks: verify wiring in Win32IDE_Commands, Win32IDE_SubAgent, Win32IDE, AgentCommands, Ship.
    2. Build: compile RawrXD-Win32IDE (or Ship) to ensure no compile/link errors.
    3. Manual checklist: print steps to verify in the running IDE.
.EXAMPLE
    .\Test-Agentic-Smoke.ps1
    .\Test-Agentic-Smoke.ps1 -SkipBuild
    .\Test-Agentic-Smoke.ps1 -LaunchIDE
#>
param(
    [switch]$SkipBuild,
    [switch]$LaunchIDE,
    [string]$ProjectRoot = $PSScriptRoot
)

$ErrorActionPreference = "Continue"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Test-CodeContains {
    param([string]$Path, [string[]]$Required, [string]$Label)
    if (-not (Test-Path $Path)) {
        Write-Host "  [FAIL] $Label - file missing: $Path" -ForegroundColor Red
        return $false
    }
    $content = Get-Content -Raw -Path $Path -ErrorAction SilentlyContinue
    $ok = $true
    foreach ($r in $Required) {
        if ($content -notmatch [regex]::Escape($r)) {
            Write-Host "  [FAIL] $Label - missing: $r" -ForegroundColor Red
            $ok = $false
        }
    }
    if ($ok) { Write-Host "  [PASS] $Label" -ForegroundColor Green }
    return $ok
}

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Agentic/Autonomous Features — Full Smoke Test" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$src = Join-Path $ProjectRoot "src"
$ship = Join-Path $ProjectRoot "Ship"
$passed = 0
$failed = 0

# ---------------------------------------------------------------------------
# Phase 1: Code wiring checks
# ---------------------------------------------------------------------------
Write-Host "[Phase 1] Code wiring checks" -ForegroundColor Yellow
Write-Host ""

# Command palette: Agent + SubAgent + Memory + Bounded Loop + Autonomy
$cmdPath = Join-Path $src "win32app\Win32IDE_Commands.cpp"
$cmdChecks = @(
    "IDM_SUBAGENT_CHAIN", "Agent: Execute Prompt Chain",
    "IDM_SUBAGENT_SWARM", "Agent: Execute HexMag Swarm",
    "IDM_SUBAGENT_TODO_LIST", "IDM_SUBAGENT_TODO_CLEAR", "IDM_SUBAGENT_STATUS",
    "IDM_AGENT_MEMORY_VIEW", "IDM_AGENT_MEMORY_CLEAR", "IDM_AGENT_MEMORY_EXPORT",
    "IDM_AGENT_BOUNDED_LOOP", "Agent: Bounded Agent Loop",
    "IDM_AUTONOMY_TOGGLE", "IDM_AUTONOMY_SET_GOAL", "IDM_AUTONOMY_STATUS", "IDM_AUTONOMY_MEMORY"
)
if (Test-CodeContains -Path $cmdPath -Required $cmdChecks -Label "Command registry (Agent/SubAgent/Memory/Autonomy)") { $passed++ } else { $failed++ }

# SubAgent: use GetSubAgentManager() from bridge, not m_subAgentManager
$subPath = Join-Path $src "win32app\Win32IDE_SubAgent.cpp"
$subChecks = @(
    "GetSubAgentManager()",
    "if (!m_agenticBridge) initializeAgenticBridge()",
    "SubAgentManager* mgr = m_agenticBridge"
)
if (Test-CodeContains -Path $subPath -Required $subChecks -Label "SubAgent handlers use AgenticBridge->GetSubAgentManager()") { $passed++ } else { $failed++ }

# Autonomy: init bridge + autonomy on each handler
$idePath = Join-Path $src "win32app\Win32IDE.cpp"
$autonomyChecks = @(
    "onAutonomyStart()",
    "initializeAgenticBridge()",
    "initializeAutonomy()"
)
$ideContent = Get-Content -Raw -Path $idePath -ErrorAction SilentlyContinue
$hasAutonomyInit = ($ideContent -match "onAutonomyStart" -and $ideContent -match "initializeAgenticBridge\(\)" -and $ideContent -match "initializeAutonomy\(\)")
if ($hasAutonomyInit) {
    Write-Host "  [PASS] Autonomy handlers init bridge + autonomy" -ForegroundColor Green
    $passed++
} else {
    Write-Host "  [FAIL] Autonomy handlers init bridge + autonomy" -ForegroundColor Red
    $failed++
}

# Agent menu: Autonomy submenu + SubAgent items
$menuChecks = @(
    "IDM_SUBAGENT_CHAIN", "IDM_AUTONOMY_TOGGLE", "hAutonomyMenu"
)
if (Test-CodeContains -Path $idePath -Required $menuChecks -Label "Agent menu has SubAgent + Autonomy submenu") { $passed++ } else { $failed++ }

# Execute Command fallback when chat input unavailable
$agentPath = Join-Path $src "win32app\Win32IDE_AgentCommands.cpp"
$agentContent = Get-Content -Raw -Path $agentPath -ErrorAction SilentlyContinue
$hasExecFallback = ($agentContent -match "Fallback when invoked from command palette" -or $agentContent -match "Copilot Chat input not available") -and $agentContent -match "CreateWindowExA" -and $agentContent -match "Execute Command"
if ($hasExecFallback) {
    Write-Host "  [PASS] Agent Execute Command has fallback dialog" -ForegroundColor Green
    $passed++
} else {
    Write-Host "  [FAIL] Agent Execute Command fallback dialog" -ForegroundColor Red
    $failed++
}

# Start Agent Loop fallback when resource missing
$hasStartFallback = $agentContent -match "gotPrompt" -and $agentContent -match "Start Agent Loop"
if ($hasStartFallback) {
    Write-Host "  [PASS] Start Agent Loop has fallback dialog" -ForegroundColor Green
    $passed++
} else {
    Write-Host "  [FAIL] Start Agent Loop fallback dialog" -ForegroundColor Red
    $failed++
}

# Ship: palette has agentic commands and WM_COMMAND handlers
$shipPath = Join-Path $ship "RawrXD_Win32_IDE.cpp"
$shipPalette = @(
    "Agent: Start Loop", "4100",
    "Agent: Execute Command", "4101",
    "Agent: Bounded Agent Loop", "4120",
    "Autonomy: Toggle", "4150"
)
if (Test-CodeContains -Path $shipPath -Required $shipPalette -Label "Ship palette has Agent/Autonomy commands") { $passed++ } else { $failed++ }

$shipHandlers = "case 4100:", "case 4120:", "RunAutonomousMode()"
if (Test-CodeContains -Path $shipPath -Required $shipHandlers -Label "Ship WM_COMMAND handles 4100/4120") { $passed++ } else { $failed++ }

Write-Host ""

# ---------------------------------------------------------------------------
# Phase 2: Build (optional)
# ---------------------------------------------------------------------------
if (-not $SkipBuild) {
    Write-Host "[Phase 2] Build RawrXD-Win32IDE" -ForegroundColor Yellow
    $buildDir = Join-Path $ProjectRoot "build"
    $slnPath = Join-Path $buildDir "RawrEngine.sln"
    $vcxprojPath = Join-Path $buildDir "RawrXD-Win32IDE.vcxproj"
    $needsConfigure = -not (Test-Path $buildDir) -or (-not (Test-Path $slnPath) -and -not (Test-Path $vcxprojPath))
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }
    if ($needsConfigure) {
        Write-Host "  Configuring CMake (Visual Studio 17 2022, x64)..." -ForegroundColor Gray
        Push-Location $buildDir
        try {
            $cmakeOut = & cmake $ProjectRoot -G "Visual Studio 17 2022" -A x64 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host "  [FAIL] CMake configure failed" -ForegroundColor Red
                $cmakeOut | Select-Object -Last 25 | ForEach-Object { Write-Host "    $_" }
                $failed++
            }
        } finally { Pop-Location }
    }
    if ((Test-Path $buildDir) -and ($LASTEXITCODE -eq 0 -or -not $needsConfigure)) {
        Push-Location $buildDir
        try {
            Write-Host "  Building target RawrXD-Win32IDE..." -ForegroundColor Gray
            $buildResult = & cmake --build . --config Release --target RawrXD-Win32IDE 2>&1
            $buildOk = $LASTEXITCODE -eq 0
            if ($buildOk) {
                Write-Host "  [PASS] Build succeeded" -ForegroundColor Green
                $passed++
            } else {
                Write-Host "  [FAIL] Build failed (exit code $LASTEXITCODE)" -ForegroundColor Red
                $buildResult | Select-Object -Last 35 | ForEach-Object { Write-Host "    $_" }
                $failed++
            }
        } finally { Pop-Location }
    }
    Write-Host ""
} else {
    Write-Host "[Phase 2] Build skipped (-SkipBuild)" -ForegroundColor Gray
    Write-Host ""
}

# ---------------------------------------------------------------------------
# Phase 3: Manual test checklist
# ---------------------------------------------------------------------------
Write-Host "[Phase 3] Manual test checklist (run IDE and verify)" -ForegroundColor Yellow
Write-Host ""
Write-Host @"
  Full Win32 IDE (RawrXD-Win32IDE.exe):
  ─────────────────────────────────────
  1. Launch IDE. Press Ctrl+Shift+P to open command palette.
  2. Type 'Agent' → confirm you see:
     • Agent: Start Agent Loop
     • Agent: Execute Command
     • Agent: Execute Prompt Chain
     • Agent: Execute HexMag Swarm
     • Agent: View Todo List / Clear Todo List / SubAgent Status
     • Agent: View Memory / Clear Memory / Export Memory
     • Agent: Bounded Agent Loop
  3. Type 'Autonomy' → confirm:
     • Autonomy: Toggle / Start / Stop / Set Goal / Show Status / Show Memory
  4. Run 'Agent: View Memory' → Output should show (empty or entries), no crash.
  5. Run 'Agent: SubAgent Status' → Output should show status or 'SubAgentManager not initialized' only if bridge failed to init (no crash).
  6. Run 'Autonomy: Set Goal' → Output should show 'Autonomy goal set' (bridge/autonomy init on first use).
  7. Run 'Agent: Execute Command' (with or without chat panel) → dialog appears; enter a short prompt and Execute → no crash.
  8. Run 'Agent: Start Agent Loop' → dialog appears (resource or fallback); cancel or enter task → no crash.
  9. Menu Agent → confirm submenus: Autonomy (Toggle, Start, Stop, Set Goal, Status, Memory), and items Chain, Swarm, Todo, Memory, Bounded Agent.

  Ship standalone (Ship\RawrXD_Win32_IDE.cpp build):
  ─────────────────────────────────────────────────
  10. If you build Ship IDE, Ctrl+Shift+P → type 'Agent' → confirm Agent: Start Loop, Execute Command, Bounded Agent Loop, etc. Run one → should run RunAutonomousMode() or show message.

"@ -ForegroundColor White
Write-Host ""

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Smoke test summary: $passed passed, $failed failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

if ($LaunchIDE) {
    $exe = Join-Path $ProjectRoot "build\bin\Release\RawrXD-Win32IDE.exe"
    if (Test-Path $exe) {
        Write-Host "Launching IDE: $exe" -ForegroundColor Green
        Start-Process -FilePath $exe -WorkingDirectory $ProjectRoot
    } else {
        $exe = Join-Path $ProjectRoot "build\Release\RawrXD-Win32IDE.exe"
        if (Test-Path $exe) {
            Write-Host "Launching IDE: $exe" -ForegroundColor Green
            Start-Process -FilePath $exe -WorkingDirectory $ProjectRoot
        } else {
            Write-Host "IDE exe not found. Build first without -SkipBuild." -ForegroundColor Yellow
        }
    }
}

exit $failed
