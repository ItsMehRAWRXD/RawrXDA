# =============================================================================
# RawrXD Chat Subsystem E2E Smoke Test Coordinator
# =============================================================================
# Master orchestration script for smoke test scenarios
# Validates: Deferred Init | Subclass Trapping | IPC Frame Constraining | Live Host IPC
# =============================================================================

param(
    [string]$BinaryPath = "",
    [int]$Timeout = 120000,  # 2 minutes max for entire suite (stress extends automatically)
    [switch]$VerboseLogging = $false,
    [switch]$Stress,
    [Alias("Soak")]
    [switch]$SoakMode,
    [int]$DefaultIterations = 200,
    [int]$StressIterations = 2000
)

$ErrorActionPreference = "Stop"
$InformationPreference = "Continue"

$SmokeRoot = $PSScriptRoot
if ($env:GITHUB_WORKSPACE -and (Test-Path $env:GITHUB_WORKSPACE)) {
    $RepoRoot = $env:GITHUB_WORKSPACE
} else {
    $RepoRoot = (Resolve-Path (Join-Path $SmokeRoot "..\..")).Path
}
if (-not $BinaryPath) {
    $repoRoot = $RepoRoot
    foreach ($candidate in @(
            (Join-Path $repoRoot "build-win32\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja-final\bin\RawrXD-Win32IDE.exe"),
            "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe"
        )) {
        if (Test-Path $candidate) {
            $BinaryPath = $candidate
            break
        }
    }
    if (-not $BinaryPath) {
        $BinaryPath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe"
    }
}
$SmokeLogDir = Join-Path $SmokeRoot "logs"
if (-not (Test-Path $SmokeLogDir)) { New-Item -ItemType Directory -Path $SmokeLogDir -Force | Out-Null }

# =============================================================================
# UTILITY FUNCTIONS
# =============================================================================

function Log-Info {
    param([string]$Message)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss.fff')] [INFO]  $Message" -ForegroundColor Cyan
}

function Log-Success {
    param([string]$Message)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss.fff')] [✓]    $Message" -ForegroundColor Green
}

function Log-Warning {
    param([string]$Message)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss.fff')] [⚠]    $Message" -ForegroundColor Yellow
}

function Log-Error {
    param([string]$Message)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss.fff')] [✗]    $Message" -ForegroundColor Red
}

function Wait-FileReady {
    param(
        [string]$FilePath,
        [int]$MaxWaitMs = 30000
    )
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    while ($stopwatch.ElapsedMilliseconds -lt $MaxWaitMs) {
        if (Test-Path $FilePath) {
            try {
                [IO.File]::OpenRead($FilePath).Close()
                return $true
            } catch {
                # File locked, still being written
            }
        }
        Start-Sleep -Milliseconds 100
    }
    
    return $false
}

function Resolve-BuildLogPath {
    $candidates = @(
        "d:\build_smoke.log",
        "d:\smoke_build.log"
    )

    foreach ($p in $candidates) {
        if (Test-Path $p) {
            return $p
        }
    }

    return $null
}

function Assert-BuildLogHealthy {
    param(
        [string]$LogPath,
        [int]$MaxStaleMinutes = 15
    )

    if (-not (Test-Path $LogPath)) {
        throw "Build log missing: $LogPath"
    }

    $f = Get-Item $LogPath
    $age = [DateTime]::Now - $f.LastWriteTime
    if ($age.TotalMinutes -gt $MaxStaleMinutes) {
        throw "Build log stale: $LogPath (age=$([int]$age.TotalMinutes)m)"
    }

    $probe = "[SMOKE_PRECHECK] append probe $(Get-Date -Format s)"
    Add-Content -Path $LogPath -Value $probe
    $all = Get-Content -Path $LogPath
    $filtered = $all | Where-Object { $_ -ne $probe }
    Set-Content -Path $LogPath -Value $filtered
}

function Assert-BinSidecars {
    param([string]$BinDir)

    $required = @(
        "ggml.dll",
        "ggml-base.dll",
        "ggml-cpu.dll",
        "llama.dll"
    )

    $missing = @()
    foreach ($name in $required) {
        if (-not (Test-Path (Join-Path $BinDir $name))) {
            $missing += $name
        }
    }

    if ($missing.Count -gt 0) {
        throw "Missing sidecars in ${BinDir}: $($missing -join ', ')"
    }
}

function Get-CrashLogSnapshot {
    param([string]$CrashLogPath)

    if (-not (Test-Path $CrashLogPath)) {
        return [PSCustomObject]@{
            Exists = $false
            LastWriteTime = $null
            Length = 0
        }
    }

    $f = Get-Item $CrashLogPath
    return [PSCustomObject]@{
        Exists = $true
        LastWriteTime = $f.LastWriteTime
        Length = $f.Length
    }
}

function Register-ScenarioResult {
    param(
        [System.Collections.Generic.List[object]]$Results,
        [string]$Name,
        [int]$ExitCode,
        [bool]$ScriptFound = $true
    )
    $passed = $ScriptFound -and ($ExitCode -eq 0)
    $Results.Add([PSCustomObject]@{
            Scenario    = $Name
            ExitCode    = $ExitCode
            Passed      = $passed
            ScriptFound = $ScriptFound
        }) | Out-Null
}

function Write-ScenarioAggregateSummary {
    param([System.Collections.Generic.List[object]]$Results)

    Log-Info ""
    Log-Info "────────────────────────────────────────────────────────────────"
    Log-Info "SCENARIO EXIT-CODE SUMMARY"
    Log-Info "────────────────────────────────────────────────────────────────"
    foreach ($row in $Results) {
        $status = if (-not $row.ScriptFound) { "SKIP (script missing)" }
        elseif ($row.Passed) { "PASS" }
        else { "FAIL" }
        $color = if ($row.Passed) { "Green" } elseif (-not $row.ScriptFound) { "Yellow" } else { "Red" }
        Write-Host ("  {0,-12} exit={1,3}  {2}" -f $row.Scenario, $row.ExitCode, $status) -ForegroundColor $color
    }
    $passedCount = @($Results | Where-Object { $_.Passed }).Count
    $total = $Results.Count
    Log-Info "Aggregate: $passedCount / $total scenarios passed"
}

function Emit-CrashEvidenceIfChanged {
    param(
        [string]$CrashLogPath,
        [object]$Before,
        [string]$ScenarioName,
        [int]$TailLines = 80
    )

    $after = Get-CrashLogSnapshot -CrashLogPath $CrashLogPath
    if (-not $after.Exists) {
        Log-Warning "[$ScenarioName] No crash log found at $CrashLogPath"
        return
    }

    $changed = $false
    if (-not $Before.Exists) {
        $changed = $true
    } elseif ($after.LastWriteTime -gt $Before.LastWriteTime) {
        $changed = $true
    } elseif ($after.Length -gt $Before.Length) {
        $changed = $true
    }

    if (-not $changed) {
        Log-Warning "[$ScenarioName] Crash log unchanged since pre-scenario snapshot"
        return
    }

    Log-Error "[$ScenarioName] Crash evidence detected in $CrashLogPath"
    Log-Info "[$ScenarioName] Crash log tail (last $TailLines lines):"
    $tail = Get-Content -Path $CrashLogPath -Tail $TailLines -ErrorAction SilentlyContinue
    foreach ($line in $tail) {
        Log-Info "[$ScenarioName][CRASH] $line"
    }
}

# =============================================================================
# MAIN ORCHESTRATION
# =============================================================================

$stressMode = $Stress -or $SoakMode
$targetSubclassIterations = if ($stressMode) { $StressIterations } else { $DefaultIterations }
if ($stressMode -and $Timeout -lt 600000) {
    $Timeout = 600000  # 10 minutes for soak (Scenario 2 @ 2000 iter + peer scenarios)
}

# Propagate binary + iteration budget to child harnesses / SmokeDiagnostics.
$env:RAWRXD_BINARY_PATH = $BinaryPath
$env:RAWRXD_SMOKE_ITERATIONS = [string]$targetSubclassIterations
if (-not $env:RAWRXD_SMOKE_GDI_TOLERANCE) {
  if ($stressMode) {
    # Scale with iteration budget (keyboard-only soak; no synthetic GetDC/SaveDC loop).
    $scaledGdi = [Math]::Max(14, [int][Math]::Ceiling($targetSubclassIterations / 40.0))
    $env:RAWRXD_SMOKE_GDI_TOLERANCE = [string]$scaledGdi
  } else {
    $env:RAWRXD_SMOKE_GDI_TOLERANCE = "10"
  }
}

$suiteStartUtc = [DateTime]::UtcNow
$ideLogPath = Join-Path $env:APPDATA "RawrXD\ide.log"

function Test-IdeLogBadAllocationSince {
    param([DateTime]$SinceUtc)
    if (-not (Test-Path $ideLogPath)) { return $false }
    $hits = Select-String -Path $ideLogPath -Pattern "bad allocation" -SimpleMatch -ErrorAction SilentlyContinue |
        Where-Object {
            if ($_.Line -match '^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})') {
                try {
                    $ts = [DateTime]::ParseExact($Matches[1], 'yyyy-MM-dd HH:mm:ss', $null)
                    return $ts.ToUniversalTime() -ge $SinceUtc
                } catch { return $true }
            }
            return $true
        }
    return ($null -ne $hits -and @($hits).Count -gt 0)
}

$scenarioResults = [System.Collections.Generic.List[object]]::new()

Log-Info "════════════════════════════════════════════════════════════════"
Log-Info "RawrXD Chat Subsystem E2E Smoke Test Suite"
Log-Info "════════════════════════════════════════════════════════════════"
Log-Info "Stress mode: $($stressMode.ToString().ToLower())"
Log-Info "Scenario 2 iteration target: $targetSubclassIterations"
Log-Info "Suite timeout budget: ${Timeout}ms"
Log-Info ""

$suiteFailed = $false

$buildLogPath = Resolve-BuildLogPath
if (-not $buildLogPath) {
    Log-Error "No known build log found (checked d:\build_smoke.log and d:\smoke_build.log)"
    exit 1
}

Log-Info "Build log resolved: $buildLogPath"
try {
    Assert-BuildLogHealthy -LogPath $buildLogPath
    Log-Success "Build log health check passed (append/read/write OK)"
} catch {
    $logErr = $_.Exception.Message
    if (($logErr -like "*Build log stale*") -and (Test-Path $BinaryPath)) {
        Log-Warning "Build log is stale but binary already exists; continuing smoke run"
    } else {
        Log-Error "Build log health check failed: $logErr"
        exit 1
    }
}

# Verify binary exists
if (-not (Test-Path $BinaryPath)) {
    Log-Error "Binary not found at: $BinaryPath"
    exit 1
}

Log-Success "Binary located: $BinaryPath"
$binarySize = (Get-Item $BinaryPath).Length / 1MB
Log-Info "Binary size: $([math]::Round($binarySize, 2)) MB"

$binaryDir = Split-Path -Path $BinaryPath -Parent
$crashLogPath = Join-Path $binaryDir "rawrxd_crash.log"
Log-Info "Crash log target: $crashLogPath"

try {
    Assert-BinSidecars -BinDir (Split-Path -Path $BinaryPath -Parent)
    Log-Success "Sidecar preflight passed (core runtime DLL set present)"
} catch {
    Log-Error "Sidecar preflight failed: $($_.Exception.Message)"
    exit 1
}

# Check timestamp
$binaryTime = (Get-Item $BinaryPath).LastWriteTime
$timeSinceBuild = [DateTime]::Now - $binaryTime
Log-Info "Build age: $([int]$timeSinceBuild.TotalSeconds) seconds"

Log-Info ""
Log-Info "────────────────────────────────────────────────────────────────"
Log-Info "SMOKE TEST SCENARIOS STAGED:"
Log-Info "────────────────────────────────────────────────────────────────"
Log-Info "  Scenario 1: Deferred Initialization Verification (WM_APP+1002)"
Log-Info "  Scenario 2: Subclass Keyboard Trapping & GDI Overlay ($targetSubclassIterations iterations)"
Log-Info "  Scenario 3: IPC Frame Constraining (>64KB Rejection)"
Log-Info "  Scenario 4: Live IDE <-> Extension Host IPC (production wire path)"
Log-Info "  Scenario 5: Copilot Chat Send/Receive (real HandleCopilotSend under smoke)"
Log-Info ""

# =============================================================================
# SCENARIO 1: DEFERRED INIT VERIFICATION
# =============================================================================

Log-Info "▶ SCENARIO 1: Deferred Initialization Verification"
Log-Info "  Action: Boot IDE and monitor createChatPanel() async timing"
Log-Info "  Expected: Core frame loads instantly, chat deferred to WM_APP+1002"
Log-Info ""

$scenario1Script = Join-Path $SmokeRoot "1_DeferredInitVerification.ps1"
if (Test-Path $scenario1Script) {
    $s1CrashBefore = Get-CrashLogSnapshot -CrashLogPath $crashLogPath
    Log-Info "Launching Scenario 1 harness..."
    & $scenario1Script -BinaryPath $BinaryPath -Verbose:$VerboseLogging
    $s1Exit = $LASTEXITCODE
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario1" -ExitCode $s1Exit
    if ($s1Exit -eq 0) {
        Log-Success "SCENARIO 1 PASSED ✓"
    } else {
        Log-Warning "SCENARIO 1 ISSUES DETECTED (exitcode=$s1Exit)"
        Emit-CrashEvidenceIfChanged -CrashLogPath $crashLogPath -Before $s1CrashBefore -ScenarioName "Scenario1"
        $suiteFailed = $true
    }
} else {
    Log-Warning "Scenario 1 harness not ready yet; will retry"
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario1" -ExitCode 1 -ScriptFound:$false
    $suiteFailed = $true
}

Log-Info ""

# =============================================================================
# SCENARIO 2: SUBCLASS KEYBOARD TRAPPING
# =============================================================================

Log-Info "▶ SCENARIO 2: Subclass Keyboard Trapping & GDI Overlay"
Log-Info "  Action: Send Tab key to chat input; verify ghost text acceptance"
Log-Info "  Expected: SaveDC/RestoreDC overlay; positive GDI accumulation <= tolerance (negative drift = OK)"
Log-Info ""

$scenario2Script = Join-Path $SmokeRoot "2_SubclassKeyboardTrapping.ps1"
if (Test-Path $scenario2Script) {
    $s2CrashBefore = Get-CrashLogSnapshot -CrashLogPath $crashLogPath
    $s2DurationMs = if ($stressMode) { 300000 } else { 30000 }
    Log-Info "Launching Scenario 2 harness ($targetSubclassIterations iterations, GDI tolerance=$($env:RAWRXD_SMOKE_GDI_TOLERANCE))..."
    & $scenario2Script -BinaryPath $BinaryPath -IterationCount $targetSubclassIterations -TestDurationMs $s2DurationMs -Verbose:$VerboseLogging
    $s2Exit = $LASTEXITCODE
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario2" -ExitCode $s2Exit
    if ($s2Exit -eq 0) {
        Log-Success "SCENARIO 2 PASSED ✓"
    } else {
        Log-Warning "SCENARIO 2 ISSUES DETECTED (exitcode=$s2Exit)"
        Emit-CrashEvidenceIfChanged -CrashLogPath $crashLogPath -Before $s2CrashBefore -ScenarioName "Scenario2"
        $suiteFailed = $true
    }
} else {
    Log-Warning "Scenario 2 harness not ready yet; will retry"
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario2" -ExitCode 1 -ScriptFound:$false
    $suiteFailed = $true
}

Log-Info ""

# =============================================================================
# SCENARIO 3: IPC FRAME CONSTRAINING
# =============================================================================

Log-Info "▶ SCENARIO 3: IPC Frame Constraining (>64KB Rejection)"
Log-Info "  Action: Attempt to send >64KB payload to CChatIpcDelegation"
Log-Info "  Expected: Security rejection [RAWR_IPC_SECURITY] + graceful fail"
Log-Info ""

$scenario3Script = Join-Path $SmokeRoot "3_IpcFrameConstraining.ps1"
if (Test-Path $scenario3Script) {
    Log-Info "Launching Scenario 3 harness..."
    & $scenario3Script -BinaryPath $BinaryPath -Verbose:$VerboseLogging
    $s3Exit = $LASTEXITCODE
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario3" -ExitCode $s3Exit
    if ($s3Exit -eq 0) {
        Log-Success "SCENARIO 3 PASSED ✓"
    } else {
        Log-Warning "SCENARIO 3 ISSUES DETECTED (exitcode=$s3Exit)"
        $suiteFailed = $true
    }
} else {
    Log-Warning "Scenario 3 harness not ready yet; will retry"
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario3" -ExitCode 1 -ScriptFound:$false
    $suiteFailed = $true
}

Log-Info ""

# =============================================================================
# SCENARIO 4: LIVE HOST IPC
# =============================================================================

Log-Info "▶ SCENARIO 4: Live IDE <-> Host IPC Verification"
Log-Info "  Action: Native RawrXDIpcPing (or PS fallback) writes production-framed legacy packet"
Log-Info "  Expected: IDE ExtensionHostIpcBridge logs ExtensionEngine IPC ingest"
Log-Info ""

$scenario4Script = Join-Path $SmokeRoot "4_LiveExtensionIpc.ps1"
if (-not (Test-Path $scenario4Script)) {
    $scenario4Fallbacks = @(
        "4_LiveHostIpcValidation.ps1",
        "4_ExtensionHostBridge.ps1",
        "4_XProcIpcValidation.ps1"
    ) | ForEach-Object { Join-Path $SmokeRoot $_ }
    $scenario4Script = $scenario4Fallbacks | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if ($scenario4Script -and (Test-Path $scenario4Script)) {
    $s4CrashBefore = Get-CrashLogSnapshot -CrashLogPath $crashLogPath
    Log-Info "Launching Scenario 4 harness ($([IO.Path]::GetFileName($scenario4Script)))..."
    & $scenario4Script -BinaryPath $BinaryPath -RepoRoot $RepoRoot -FullSmoke -Verbose:$VerboseLogging
    $s4Exit = $LASTEXITCODE
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario4" -ExitCode $s4Exit
    if ($s4Exit -eq 0) {
        Log-Success "SCENARIO 4 PASSED ✓"
    } else {
        Log-Warning "SCENARIO 4 ISSUES DETECTED (exitcode=$s4Exit)"
        Emit-CrashEvidenceIfChanged -CrashLogPath $crashLogPath -Before $s4CrashBefore -ScenarioName "Scenario4"
        $suiteFailed = $true
    }
} else {
    Log-Warning "Scenario 4 harness not found; skipping"
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario4" -ExitCode 1 -ScriptFound:$false
    $suiteFailed = $true
}

# =============================================================================
# SCENARIO 5: COPILOT CHAT SEND / RECEIVE
# =============================================================================

Log-Info "▶ SCENARIO 5: Copilot Chat Send / Receive Interlock"
Log-Info "  Action: Unlock send under RAWRXD_SMOKE_DEFERRED_INIT; post smoke ping"
Log-Info "  Expected: Send enabled; HandleCopilotSend not blocked; chat surface updates or soft-pass without crash"
Log-Info ""

$scenario5Script = Join-Path $SmokeRoot "5_CopilotChatSendReceive.ps1"
if (Test-Path $scenario5Script) {
    $s5CrashBefore = Get-CrashLogSnapshot -CrashLogPath $crashLogPath
    Log-Info "Launching Scenario 5 harness..."
    & $scenario5Script -BinaryPath $BinaryPath -Verbose:$VerboseLogging
    $s5Exit = $LASTEXITCODE
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario5" -ExitCode $s5Exit
    if ($s5Exit -eq 0) {
        Log-Success "SCENARIO 5 PASSED ✓"
    } else {
        Log-Warning "SCENARIO 5 ISSUES DETECTED (exitcode=$s5Exit)"
        Emit-CrashEvidenceIfChanged -CrashLogPath $crashLogPath -Before $s5CrashBefore -ScenarioName "Scenario5"
        $suiteFailed = $true
    }
} else {
    Log-Warning "Scenario 5 harness not found; skipping"
    Register-ScenarioResult -Results $scenarioResults -Name "Scenario5" -ExitCode 1 -ScriptFound:$false
    $suiteFailed = $true
}

Log-Info ""

if (Test-IdeLogBadAllocationSince -SinceUtc $suiteStartUtc) {
    Log-Error "ide.log contains 'bad allocation' during this suite run ($ideLogPath)"
    $suiteFailed = $true
}

Log-Info ""
Log-Info "════════════════════════════════════════════════════════════════"
Log-Info "SMOKE TEST SUITE COMPLETE"
Log-Info "════════════════════════════════════════════════════════════════"
Write-ScenarioAggregateSummary -Results $scenarioResults
Log-Info ""
Log-Info "Review logs in $SmokeLogDir for detailed output"
if ($stressMode) {
    Log-Info "Stress soak: inspect scenario2_subclass.log for GDI floor (negative drift should flatten, not climb)"
}

Log-Info ""
if ($suiteFailed) {
    Log-Error "GLOBAL SMOKE SUITE RESULT: FAIL"
    exit 1
}
Log-Success "GLOBAL SMOKE SUITE RESULT: PASS"
exit 0
