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
    .\Test-Agentic-Smoke.ps1 -BuildDir build_smoke_vs -Generator "Visual Studio 17 2022" -Arch x64 -Configuration Release -CleanBuild
#>
param(
    [switch]$SkipBuild,
    [switch]$LaunchIDE,
    [string]$ProjectRoot = $PSScriptRoot,
    [string]$BuildDir = "build_smoke_auto",
    [string]$Generator = "Auto",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$Target = "RawrXD-Win32IDE",
    [switch]$CleanBuild,
    [int]$BuildRetryCount = 3,
    [int]$MaxParallel = 1
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

function Get-CMakeCacheValue {
    param([string]$BuildDirectory, [string]$VarName)
    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path $cachePath)) { return $null }
    foreach ($line in Get-Content -Path $cachePath -ErrorAction SilentlyContinue) {
        if ($line -like "$VarName*") {
            $parts = $line -split "=", 2
            if ($parts.Count -eq 2) {
                return $parts[1].Trim()
            }
            return ""
        }
    }
    return $null
}

function Invoke-LoggedProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$LogPath
    )
    $errPath = "$LogPath.err"
    if (Test-Path $LogPath) { Remove-Item $LogPath -Force -ErrorAction SilentlyContinue }
    if (Test-Path $errPath) { Remove-Item $errPath -Force -ErrorAction SilentlyContinue }

    $p = Start-Process -FilePath $FilePath `
        -ArgumentList $Arguments `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $LogPath `
        -RedirectStandardError $errPath

    if (Test-Path $errPath) {
        Get-Content -Path $errPath -ErrorAction SilentlyContinue | Add-Content -Path $LogPath
        Remove-Item $errPath -Force -ErrorAction SilentlyContinue
    }
    return $p.ExitCode
}

function Get-LogTail {
    param([string]$Path, [int]$LineCount = 25)
    if (-not (Test-Path $Path)) { return @() }
    return Get-Content -Path $Path -Tail $LineCount -ErrorAction SilentlyContinue
}

function Remove-LockedObjFromLog {
    param([string]$BuildDirectory, [string]$LogPath)
    if (-not (Test-Path $LogPath)) { return $false }
    $content = Get-Content -Path $LogPath -Raw -ErrorAction SilentlyContinue
    if (-not $content) { return $false }
    $m = [regex]::Match($content, "Cannot open compiler generated file: '([^']+\.obj)': Permission denied")
    if (-not $m.Success) { return $false }
    $objPath = $m.Groups[1].Value
    if (-not [System.IO.Path]::IsPathRooted($objPath)) {
        $objPath = Join-Path $BuildDirectory $objPath
    }
    if (Test-Path $objPath) {
        Remove-Item -Path $objPath -Force -ErrorAction SilentlyContinue
    }
    return $true
}

function Get-ActiveBuildProcessesForPath {
    param([string]$BuildDirectory)
    if ([string]::IsNullOrWhiteSpace($BuildDirectory)) { return @() }
    $normalized = [System.IO.Path]::GetFullPath($BuildDirectory).TrimEnd("\\/")
    try {
        return @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
            $_.Name -in @("cmake.exe", "nmake.exe", "cl.exe", "link.exe", "rc.exe", "mt.exe") -and
            $_.CommandLine -and
            $_.CommandLine.IndexOf($normalized, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        })
    } catch {
        return @()
    }
}

function Stop-ActiveBuildProcessesForPath {
    param([string]$BuildDirectory, [int]$WaitSeconds = 5)
    $procs = @(Get-ActiveBuildProcessesForPath -BuildDirectory $BuildDirectory)
    if ($procs.Count -eq 0) { return $false }

    foreach ($proc in $procs) {
        try {
            Stop-Process -Id $proc.ProcessId -Force -ErrorAction SilentlyContinue
        } catch {
        }
    }

    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $WaitSeconds))
    do {
        Start-Sleep -Milliseconds 250
        $remaining = @(Get-ActiveBuildProcessesForPath -BuildDirectory $BuildDirectory)
    } while ($remaining.Count -gt 0 -and (Get-Date) -lt $deadline)

    return $true
}

function Test-IsMultiConfigGenerator {
    param([string]$GeneratorName)
    $multi = @(
        "Visual Studio 17 2022",
        "Visual Studio 16 2019",
        "Visual Studio 15 2017",
        "Ninja Multi-Config",
        "Xcode"
    )
    return $multi -contains $GeneratorName
}

function Get-GeneratorSafeBuildDir {
    param([string]$Root, [string]$RequestedDir, [string]$RequestedGenerator)
    $requestedFull = if ([System.IO.Path]::IsPathRooted($RequestedDir)) { $RequestedDir } else { Join-Path $Root $RequestedDir }
    $cacheGen = Get-CMakeCacheValue -BuildDirectory $requestedFull -VarName "CMAKE_GENERATOR"
    if ([string]::IsNullOrWhiteSpace($cacheGen) -or $cacheGen -eq $RequestedGenerator) {
        return $requestedFull
    }

    $safeSuffix = ($RequestedGenerator -replace "[^A-Za-z0-9]+", "_").Trim("_").ToLowerInvariant()
    if ([string]::IsNullOrWhiteSpace($safeSuffix)) { $safeSuffix = "cmake" }
    $fallback = Join-Path $Root ("build_smoke_{0}" -f $safeSuffix)
    return $fallback
}

function Get-AvailableCMakeGenerators {
    $help = & cmake --help 2>&1
    if ($LASTEXITCODE -ne 0) { return @() }
    $generators = @()
    foreach ($line in $help) {
        if ($line -match "^\s*\*\s+(.+?)\s*=\s*Generate") {
            $generators += $Matches[1].Trim()
        }
    }
    return $generators
}

function Get-GeneratorCandidates {
    param([string]$PreferredGenerator)
    if ($PreferredGenerator -and $PreferredGenerator -ne "Auto") {
        return @($PreferredGenerator)
    }
    $available = Get-AvailableCMakeGenerators
    $ordered = @(
        "Ninja",
        "NMake Makefiles",
        "Visual Studio 17 2022",
        "Visual Studio 16 2019",
        "Ninja Multi-Config"
    )
    $candidates = @()
    foreach ($g in $ordered) {
        if ($available -contains $g) { $candidates += $g }
    }
    return $candidates
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
    $generatorCandidates = Get-GeneratorCandidates -PreferredGenerator $Generator
    if ($generatorCandidates.Count -eq 0) {
        Write-Host "  [FAIL] No usable CMake generator found." -ForegroundColor Red
        $failed++
    } else {
        $buildPass = $false
        foreach ($candidateGenerator in $generatorCandidates) {
            $buildDirResolved = Get-GeneratorSafeBuildDir -Root $ProjectRoot -RequestedDir $BuildDir -RequestedGenerator $candidateGenerator
            if (-not (Test-Path $buildDirResolved)) { New-Item -ItemType Directory -Path $buildDirResolved -Force | Out-Null }

            $staleBuildProcs = @(Get-ActiveBuildProcessesForPath -BuildDirectory $buildDirResolved)
            if ($staleBuildProcs.Count -gt 0) {
                Write-Host "  Found $($staleBuildProcs.Count) stale build process(es) for $buildDirResolved; stopping them before configure/build..." -ForegroundColor Yellow
                [void](Stop-ActiveBuildProcessesForPath -BuildDirectory $buildDirResolved)
            }

            if ($CleanBuild -and (Test-Path (Join-Path $buildDirResolved "CMakeCache.txt"))) {
                Write-Host "  Clean build requested: removing cached CMake files in $buildDirResolved..." -ForegroundColor Gray
                Remove-Item -Path (Join-Path $buildDirResolved "CMakeCache.txt") -Force -ErrorAction SilentlyContinue
                Remove-Item -Path (Join-Path $buildDirResolved "CMakeFiles") -Recurse -Force -ErrorAction SilentlyContinue
            }

            $cacheGenBefore = Get-CMakeCacheValue -BuildDirectory $buildDirResolved -VarName "CMAKE_GENERATOR"
            if ($cacheGenBefore -and $cacheGenBefore -ne $candidateGenerator) {
                Write-Host "  Generator mismatch in $buildDirResolved (`"$cacheGenBefore`" vs `"$candidateGenerator`"), trying next..." -ForegroundColor Yellow
                continue
            }

            $logDir = Join-Path $ProjectRoot "logs"
            if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
            $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
            $safeGen = ($candidateGenerator -replace "[^A-Za-z0-9]+", "_").Trim("_").ToLowerInvariant()
            $cfgLog = Join-Path $logDir ("smoke_configure_{0}_{1}.log" -f $safeGen, $stamp)
            $bldLog = Join-Path $logDir ("smoke_build_{0}_{1}.log" -f $safeGen, $stamp)

            $needsConfigure = -not (Test-Path (Join-Path $buildDirResolved "CMakeCache.txt"))
            $cmakeConfigureOk = $true
            if ($needsConfigure) {
                Write-Host "  Configuring CMake ($candidateGenerator) in $buildDirResolved..." -ForegroundColor Gray
                $cfgArgs = @("-S", $ProjectRoot, "-B", $buildDirResolved, "-G", $candidateGenerator)
                if (Test-IsMultiConfigGenerator -GeneratorName $candidateGenerator) {
                    $cfgArgs += @("-A", $Arch)
                }
                $cfgExit = Invoke-LoggedProcess -FilePath "cmake" -Arguments $cfgArgs -LogPath $cfgLog
                if ($cfgExit -ne 0) {
                    Write-Host "  Configure failed for generator $candidateGenerator, trying next..." -ForegroundColor Yellow
                    Get-LogTail -Path $cfgLog -LineCount 10 | ForEach-Object { Write-Host "    $_" }
                    $cmakeConfigureOk = $false
                }
            }

            if ((Test-Path $buildDirResolved) -and $cmakeConfigureOk) {
                $cacheGenAfter = Get-CMakeCacheValue -BuildDirectory $buildDirResolved -VarName "CMAKE_GENERATOR"
                if ([string]::IsNullOrWhiteSpace($cacheGenAfter)) { $cacheGenAfter = $candidateGenerator }
                $isMultiConfig = Test-IsMultiConfigGenerator -GeneratorName $cacheGenAfter
                Write-Host "  Building target $Target (generator: $cacheGenAfter)..." -ForegroundColor Gray
                $buildArgs = @("--build", $buildDirResolved, "--target", $Target)
                if ($isMultiConfig) { $buildArgs += @("--config", $Configuration) }
                if ($MaxParallel -gt 0) { $buildArgs += @("--parallel", $MaxParallel) }
                $bldExit = 1
                for ($attempt = 1; $attempt -le [Math]::Max(1, $BuildRetryCount); $attempt++) {
                    $bldExit = Invoke-LoggedProcess -FilePath "cmake" -Arguments $buildArgs -LogPath $bldLog
                    if ($bldExit -eq 0) { break }
                    $recovered = Remove-LockedObjFromLog -BuildDirectory $buildDirResolved -LogPath $bldLog
                    if ($recovered -and $attempt -lt $BuildRetryCount) {
                        Write-Host "  Build hit locked obj file; stopping stale build tools and retrying attempt $($attempt + 1)/$BuildRetryCount..." -ForegroundColor Yellow
                        [void](Stop-ActiveBuildProcessesForPath -BuildDirectory $buildDirResolved)
                        Start-Sleep -Seconds 2
                        continue
                    }
                    break
                }

                if ($bldExit -eq 0) {
                    Write-Host "  [PASS] Build succeeded ($buildDirResolved)" -ForegroundColor Green
                    $passed++
                    $buildPass = $true
                    $Generator = $cacheGenAfter
                    $BuildDir = $buildDirResolved
                    break
                } else {
                    [void](Stop-ActiveBuildProcessesForPath -BuildDirectory $buildDirResolved)
                    Write-Host "  Build failed for generator $cacheGenAfter, trying next..." -ForegroundColor Yellow
                    Get-LogTail -Path $bldLog -LineCount 20 | ForEach-Object { Write-Host "    $_" }
                    $knownError = Select-String -Path $bldLog -Pattern "error C|fatal error|U1077|LNK[0-9]+|Permission denied" -Quiet -ErrorAction SilentlyContinue
                    if (-not $knownError) {
                        Write-Host "    No explicit compiler/linker error text found; inspect full log: $bldLog" -ForegroundColor Yellow
                    }
                }
            }
        }

        if (-not $buildPass) {
            Write-Host "  [FAIL] Build failed for all candidate generators." -ForegroundColor Red
            $failed++
        }
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
    $launchBuildDir = Get-GeneratorSafeBuildDir -Root $ProjectRoot -RequestedDir $BuildDir -RequestedGenerator $Generator
    $launchCandidates = @(
        (Join-Path $launchBuildDir ("bin\{0}\RawrXD-Win32IDE.exe" -f $Configuration)),
        (Join-Path $launchBuildDir ("{0}\RawrXD-Win32IDE.exe" -f $Configuration)),
        (Join-Path $launchBuildDir "bin\RawrXD-Win32IDE.exe"),
        (Join-Path $launchBuildDir "RawrXD-Win32IDE.exe")
    )
    $exe = $launchCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($exe) {
        Write-Host "Launching IDE: $exe" -ForegroundColor Green
        Start-Process -FilePath $exe -WorkingDirectory $ProjectRoot
    } else {
        Write-Host "IDE exe not found in $launchBuildDir. Build first without -SkipBuild." -ForegroundColor Yellow
    }
}

exit $failed
