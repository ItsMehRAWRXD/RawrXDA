# =============================================================================
# RawrXD v15.0-AGENTIC — Self-Healing Build Demo
# Valuation Defense Artifact: Break → Diagnose → Fix → Validate
# =============================================================================

param(
    [string]$BinaryPath = ""
)

$DemoStart = Get-Date
$LogFile = "self_healing_demo_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
$ErrorActionPreference = "Stop"

function Log($msg) {
    $ts = Get-Date -Format "HH:mm:ss.fff"
    "$ts | $msg" | Tee-Object -FilePath $LogFile -Append
}

function Select-HealingExecutable {
    if (-not [string]::IsNullOrWhiteSpace($BinaryPath)) {
        $resolved = $BinaryPath
        if (-not [System.IO.Path]::IsPathRooted($resolved)) {
            $resolved = Join-Path (Get-Location) $resolved
        }
        if (Test-Path $resolved) {
            return $resolved
        }
        Log "WARNING: Provided -BinaryPath not found: $resolved"
    }

    $candidates = @(
        "D:\rawrxd\build_prod\bin\RawrXD-AutoFixCLI.exe",
        "D:\rawrxd\bin\RawrXD-AutoFixCLI.exe",
        "D:\rawrxd\bin\RawrXD-AgenticIDE.exe",
        "D:\rawrxd\build_prod\bin\RawrXD-AgenticIDE.exe",
        "D:\rawrxd\bin\RawrXD_Sovereign.exe",
        "D:\rawrxd\bin\RawrXD_Sovereign2.exe"
    )

    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

function Ensure-HealingExecutable {
    $resolved = Select-HealingExecutable
    if ($resolved) {
        return $resolved
    }

    Log "No healing executable found. Building RawrXD-AutoFixCLI..."
    $BuildCliCmd = "cmake --build build_prod --config Release --target RawrXD-AutoFixCLI 2>&1"
    $BuildCliOut = Invoke-Expression $BuildCliCmd
    $BuildCliExit = $LASTEXITCODE
    Log "AutoFixCLI build exit code: $BuildCliExit"
    ($BuildCliOut | Select-Object -Last 40) | ForEach-Object { Log "AUTOFIX-BUILD> $_" }

    if ($BuildCliExit -ne 0) {
        return $null
    }

    return Select-HealingExecutable
}

function Ensure-BuildEnv {
    $msvcRoots = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717",
        "D:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
    )
    $sdkRoots = @(
        "C:\Program Files (x86)\Windows Kits\10",
        "D:\Program Files (x86)\Windows Kits\10"
    )

    $msvc = $msvcRoots | Where-Object { Test-Path $_ } | Select-Object -First 1
    $sdk = $sdkRoots | Where-Object { Test-Path $_ } | Select-Object -First 1
    $ver = "10.0.22621.0"

    if ($msvc -and $sdk) {
        $env:LIB = "$msvc\lib\x64;$msvc\lib\onecore\x64;$sdk\Lib\$ver\ucrt\x64;$sdk\Lib\$ver\um\x64"
        $env:INCLUDE = "$msvc\include;$sdk\Include\$ver\ucrt;$sdk\Include\$ver\shared;$sdk\Include\$ver\um"
        Log "Build env injected (MSVC+SDK): msvc=$msvc sdk=$sdk"
    } else {
        Log "WARNING: Could not fully resolve MSVC/SDK paths"
    }
}

Set-Location "D:\rawrxd"
Log "=== RAWRXD SELF-HEALING DEMO ==="
Log "Start: $DemoStart"

Ensure-BuildEnv

# Build a focused target with known clean baseline in this workspace.
$BuildCmd = "cmake --build build_prod --config Release --target test_autonomous_pipeline 2>&1"
Log "Build command: $BuildCmd"

# -----------------------------------------------------------------------------
# PHASE 0: BASELINE HEALTH CHECK
# -----------------------------------------------------------------------------
Log "[PHASE 0] Baseline check before controlled break..."
$BaselineBuild = cmd /c $BuildCmd 2>&1
$BaselineExit = $LASTEXITCODE
Log "Baseline exit code: $BaselineExit"
Log "Baseline errors: $(($BaselineBuild | Select-String "error|FAILED|undefined reference").Count)"

if ($BaselineExit -ne 0) {
    Log "FATAL: Baseline is not green. Demo cannot claim autonomous repair reliably."
    Log "Tail baseline output:"
    ($BaselineBuild | Select-Object -Last 40) | ForEach-Object { Log "BASELINE> $_" }
    exit 2
}

# -----------------------------------------------------------------------------
# PHASE 1: INTRODUCE CONTROLLED FAILURE
# -----------------------------------------------------------------------------
Log "[PHASE 1] Injecting intentional build failure..."

$TestFile = "tests\test_autonomous_pipeline.cpp"
$BackupFile = "$TestFile.demo_backup"
Copy-Item $TestFile $BackupFile -Force

$BrokenCode = @"

// INTENTIONAL BREAK FOR DEMO — DO NOT COMMIT
void rawrxd_demo_break_function() {
    undefined_symbol_for_demo;
    syntax_error_here(
}
"@

Add-Content -Path $TestFile -Value $BrokenCode -Encoding ASCII
Log "Injected break into: $TestFile"

# -----------------------------------------------------------------------------
# PHASE 2: VERIFY BUILD FAILURE
# -----------------------------------------------------------------------------
Log "[PHASE 2] Verifying build fails without fix..."

$PreBuild = cmd /c $BuildCmd 2>&1
$PreExit = $LASTEXITCODE
$PreErrors = ($PreBuild | Select-String "error|FAILED|undefined reference").Count
Log "Pre-fix exit code: $PreExit"
Log "Pre-fix errors: $PreErrors"

if ($PreExit -eq 0) {
    Log "FATAL: Build succeeded when should fail — abort demo"
    Copy-Item $BackupFile $TestFile -Force
    Remove-Item $BackupFile -ErrorAction SilentlyContinue
    exit 1
}

# -----------------------------------------------------------------------------
# PHASE 3: TRIGGER AUTONOMOUS SELF-HEALING
# -----------------------------------------------------------------------------
Log "[PHASE 3] Triggering executeAutoFix()..."

$HealingExe = Ensure-HealingExecutable
$HealingStart = Get-Date
$HealingExit = -99
$HealingOutput = @()
$HealingLaunchAttempts = 0
$TelemetryPath = "healing_telemetry.json"
if (Test-Path $TelemetryPath) { Remove-Item $TelemetryPath -Force }

if (-not $HealingExe) {
    Log "WARNING: No healing executable found"
} else {
    Log "Healing executable: $HealingExe"

    if ($HealingExe -match "AutoFixCLI") {
        $MaxCliRetries = 3
        for ($cliAttempt = 1; $cliAttempt -le $MaxCliRetries; $cliAttempt++) {
            $HealingLaunchAttempts = $cliAttempt
            Log "AutoFixCLI attempt $cliAttempt/$MaxCliRetries"
            try {
                $HealingOutput = & $HealingExe `
                    --autofix `
                    --build-command $BuildCmd `
                    --max-attempts 3 `
                    --workspace-root "D:\rawrxd" `
                    --telemetry-out $TelemetryPath `
                    2>&1
                $HealingExit = $LASTEXITCODE
            } catch {
                Log "WARNING: AutoFixCLI attempt $cliAttempt failed: $($_.Exception.Message)"
                $HealingExit = -96
            }

            if ($HealingExit -eq 0) {
                Log "AutoFixCLI succeeded on attempt $cliAttempt"
                break
            }

            if ($cliAttempt -lt $MaxCliRetries) {
                $RetrySec = [Math]::Min(6, 2 * $cliAttempt)
                Log "AutoFixCLI failed (exit=$HealingExit). Retrying in ${RetrySec}s..."
                Start-Sleep -Seconds $RetrySec
            } else {
                Log "AutoFixCLI exhausted all $MaxCliRetries attempts (exit=$HealingExit)"
            }
        }
    } else {
        $args = @(
            "--autofix",
            "--build-command", $BuildCmd,
            "--max-attempts", "3",
            "--workspace-root", "D:\rawrxd",
            "--telemetry-out", $TelemetryPath
        )

        $stdout = "healing_stdout.log"
        $stderr = "healing_stderr.log"
        $HealingWaitSeconds = 5
        Remove-Item $stdout,$stderr -ErrorAction SilentlyContinue

        $MaxLaunchRetries = 3
        for ($launchAttempt = 1; $launchAttempt -le $MaxLaunchRetries; $launchAttempt++) {
            $HealingLaunchAttempts = $launchAttempt
            Log "Healing launch attempt $launchAttempt/$MaxLaunchRetries"

            Remove-Item $stdout,$stderr -ErrorAction SilentlyContinue

            $p = $null
            try {
                $p = Start-Process -FilePath $HealingExe -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
            } catch {
                Log "WARNING: Failed to launch healing executable: $($_.Exception.Message)"
                $HealingExit = -98
            }

            if ($p) {
                $timedOut = $false
                try {
                    Wait-Process -Id $p.Id -Timeout $HealingWaitSeconds -ErrorAction Stop
                } catch {
                    $timedOut = $true
                }

                if ($timedOut) {
                    Log "WARNING: Healing process timeout; terminating PID=$($p.Id)"
                    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
                    $HealingExit = -96
                } else {
                    try {
                        $HealingExit = $p.ExitCode
                    } catch {
                        $HealingExit = -97
                    }
                }
            }

            if (Test-Path $stdout) {
                $HealingOutput += (Get-Content $stdout | ForEach-Object { "[try $launchAttempt] $_" })
            }
            if (Test-Path $stderr) {
                $HealingOutput += (Get-Content $stderr | ForEach-Object { "[try $launchAttempt] $_" })
            }

            if ($HealingExit -eq 0) {
                break
            }

            if ($launchAttempt -lt $MaxLaunchRetries) {
                $SleepSec = [Math]::Min(6, 2 * $launchAttempt)
                Log "Healing attempt failed (exit=$HealingExit). Retrying in ${SleepSec}s..."
                Start-Sleep -Seconds $SleepSec
            }
        }
    }
}

$HealingDuration = (Get-Date) - $HealingStart

Log "Healing exit code: $HealingExit"
Log "Healing launch attempts: $HealingLaunchAttempts"
Log "Healing duration: $($HealingDuration.TotalSeconds.ToString('F2'))s"
if ($HealingOutput.Count -gt 0) {
    Log "Healing output lines: $($HealingOutput.Count)"
    ($HealingOutput | Select-Object -First 120) | ForEach-Object { Log "HEAL> $_" }
} else {
    Log "Healing output: <none>"
}

# -----------------------------------------------------------------------------
# PHASE 4: VERIFY CLEAN REBUILD
# -----------------------------------------------------------------------------
Log "[PHASE 4] Verifying post-healing build..."

$PostBuild = cmd /c $BuildCmd 2>&1
$PostExit = $LASTEXITCODE
$PostErrors = ($PostBuild | Select-String "error|FAILED|undefined reference").Count
Log "Post-fix exit code: $PostExit"
Log "Post-fix errors: $PostErrors"

if ($PostExit -ne 0) {
    Log "FAILURE: Build still broken after healing — inspect telemetry"
    ($PostBuild | Select-Object -Last 40) | ForEach-Object { Log "POST> $_" }
}

# -----------------------------------------------------------------------------
# PHASE 5: TELEMETRY ANALYSIS
# -----------------------------------------------------------------------------
Log "[PHASE 5] Analyzing telemetry..."

$Telemetry = $null
if (Test-Path $TelemetryPath) {
    try {
        $Telemetry = Get-Content $TelemetryPath -Raw | ConvertFrom-Json
        Log "Attempts consumed: $($Telemetry.attemptCount)"
        Log "Diagnostics generated: $($Telemetry.totalDiagnosticsGenerated)"
        Log "Diagnostics handled: $($Telemetry.totalDiagnosticsHandled)"
        Log "Fixes staged: $($Telemetry.totalFixesStaged)"
        Log "Final status: $($Telemetry.finalStatus)"
    } catch {
        Log "WARNING: Telemetry parse failed: $($_.Exception.Message)"
    }
} else {
    Log "WARNING: Telemetry file not produced"
}

# -----------------------------------------------------------------------------
# PHASE 6: ARTIFACT GENERATION
# -----------------------------------------------------------------------------
Log "[PHASE 6] Generating valuation artifacts..."

$DemoSummary = @{
    demo_timestamp = $DemoStart.ToString("o")
    duration_total_seconds = ([DateTime]::Now - $DemoStart).TotalSeconds
    phase_break_injected = $true
    phase_prefail_verified = ($PreExit -ne 0)
    phase_healing_triggered = ($HealingExit -eq 0 -or $HealingExit -eq 1)
    phase_postbuild_clean = ($PostExit -eq 0)
    healing_attempts_used = if ($Telemetry) { $Telemetry.attemptCount } else { $null }
    healing_diagnostics_initial = if ($Telemetry) { $Telemetry.totalDiagnosticsGenerated } else { $null }
    healing_fixes_applied = if ($Telemetry) { $Telemetry.totalFixesStaged } else { $null }
    healing_launch_attempts = $HealingLaunchAttempts
    build_pre_exit_code = $PreExit
    build_post_exit_code = $PostExit
    healing_executable = $HealingExe
    log_file = $LogFile
    telemetry_file = $TelemetryPath
    git_commit = (git rev-parse --short HEAD)
    rawrxd_version = "v15.0-AGENTIC"
} | ConvertTo-Json -Depth 5

$DemoSummary | Out-File "demo_summary.json" -Encoding utf8
Log "Artifact: demo_summary.json"

# -----------------------------------------------------------------------------
# PHASE 7: CLEANUP
# -----------------------------------------------------------------------------
Log "[PHASE 7] Cleaning demo injection..."
if (Test-Path $BackupFile) {
    Copy-Item $BackupFile $TestFile -Force
    Remove-Item $BackupFile -Force
    Log "Restored original file: $TestFile"
}

# -----------------------------------------------------------------------------
# COMPLETION
# -----------------------------------------------------------------------------
Log "=== DEMO COMPLETE ==="
Log "Duration: $(([DateTime]::Now - $DemoStart).ToString('mm\:ss\.fff'))"
Log "Status: $(if ($PostExit -eq 0) { 'SUCCESS — Self-healing validated' } else { 'PARTIAL — Review required' })"
Log "Artifacts: $LogFile, demo_summary.json, $TelemetryPath"

# Return code for CI/CD
exit $PostExit
