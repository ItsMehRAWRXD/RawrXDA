#Requires -Version 5.1
<#
.SYNOPSIS
    Full CLI + Win32 IDE manifestation tests — no simulation, every check runs for real.
.DESCRIPTION
    Discovers build binaries, runs all CLI contract modes against the unified/IDE binary,
    validates artifacts and log; then starts the Win32 IDE, verifies main window creation,
    and closes it. Fails the run if any single check fails (optional: SkipPersist/SkipElevated).
.NOTES
    Contract: CLI_CONTRACT_v1.1.md
    Do not simulate: every mode invokes the real binary; IDE test launches real process and window.
#>

[CmdletBinding()]
param(
    [string]$BuildDir = $null,
    [string]$WorkDir = $null,
    [switch]$SkipPersist,
    [switch]$SkipElevated,
    [switch]$CLIOnly,       # Run only CLI manifestation (no IDE launch)
    [switch]$IDEOnly,       # Run only Win32 IDE manifestation
    [int]$IDELaunchTimeoutSec = 30,
    [int]$LatencyThresholdMs = 5000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ─── Discovery (workspace-agnostic) ─────────────────────────────────────────
$script:RepoRoot = $null
if ($PSScriptRoot) {
    $script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $script:RepoRoot = (Get-Location).Path
}
if ($WorkDir) { $script:TestWorkDir = $WorkDir } else { $script:TestWorkDir = $script:RepoRoot }

$script:BuildDir = $BuildDir
if (-not $script:BuildDir) {
    if ($env:RAWRXD_BUILD_DIR) { $script:BuildDir = $env:RAWRXD_BUILD_DIR }
    else { $script:BuildDir = Join-Path $script:RepoRoot "build" }
}

function Find-Binary {
    param([string[]]$Names)
    foreach ($name in $Names) {
        $exe = Get-ChildItem -Path $script:BuildDir -Recurse -Filter "$name.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exe) { return $exe.FullName }
    }
    return $null
}

# Prefer contract binary name, then CMake output name
$script:CLIBinary = Find-Binary @("RawrXD_IDE_unified", "RawrXD-Win32IDE")
$script:IDEBinary = Find-Binary @("RawrXD-Win32IDE", "RawrXD_IDE_unified")
$script:RawrEngineBinary = Find-Binary @("RawrEngine")

# Counters and results
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0
$script:Results = @()

function Write-Step($num, $total, $msg) { Write-Host "[$num/$total] $msg" -ForegroundColor Yellow }
function Write-Pass($msg) { Write-Host "  [PASS] $msg" -ForegroundColor Green }
function Write-Fail($msg) { Write-Host "  [FAIL] $msg" -ForegroundColor Red; $script:FailedTests++ }
function Record-Pass($name, $detail) {
    $script:TotalTests++; $script:PassedTests++
    $script:Results += [PSCustomObject]@{ Name = $name; Status = "PASS"; Detail = $detail }
}
function Record-Fail($name, $detail) {
    $script:TotalTests++; $script:FailedTests++
    $script:Results += [PSCustomObject]@{ Name = $name; Status = "FAIL"; Detail = $detail }
}

# ─── CLI test helper: run one mode, assert exit + artifacts (no sim) ──────────
function Invoke-CLIMode {
    param(
        [string]$Name,
        [string]$Switch,
        [string[]]$ExtraArgs = @(),
        [int]$ExpectedExitCode = 0,
        [string[]]$ExpectedArtifacts = @(),
        [string[]]$OutputContains = @(),
        [bool]$Skip = $false,
        [string]$SkipReason = "",
        [int]$LatencyOverrideMs = 0
    )
    if (-not $script:CLIBinary) {
        Write-Host "  [SKIP] $Name — CLI binary not found" -ForegroundColor DarkYellow
        return
    }
    if ($Skip) {
        $script:TotalTests++
        $script:Results += [PSCustomObject]@{ Name = "cli-$Name"; Status = "SKIPPED"; Detail = $SkipReason }
        Write-Host "  [SKIP] $Name — $SkipReason" -ForegroundColor DarkYellow
        return
    }

    foreach ($a in $ExpectedArtifacts) { if (Test-Path $a) { Remove-Item $a -Force -ErrorAction SilentlyContinue } }
    $allArgs = @($Switch) + $ExtraArgs
    $argString = $allArgs -join " "
    $stdoutPath = Join-Path $env:TEMP "rawrxd_manifest_stdout.txt"
    $stderrPath = Join-Path $env:TEMP "rawrxd_manifest_stderr.txt"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $proc = Start-Process -FilePath $script:CLIBinary -ArgumentList $argString -WorkingDirectory $script:TestWorkDir `
            -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        $sw.Stop()
        $exitCode = $proc.ExitCode
        $stdout = ""
        if (Test-Path $stdoutPath) { $stdout = Get-Content $stdoutPath -Raw -ErrorAction SilentlyContinue }
        $maxMs = if ($LatencyOverrideMs -gt 0) { $LatencyOverrideMs } else { $LatencyThresholdMs }
        if ($sw.Elapsed.TotalMilliseconds -gt $maxMs) {
            Record-Fail "cli-$Name" "Latency $([int]$sw.Elapsed.TotalMilliseconds)ms > $maxMs ms"
            Write-Host "  [FAIL] $Name — Latency exceeded" -ForegroundColor Red
            return
        }
        if ($exitCode -ne $ExpectedExitCode) {
            Record-Fail "cli-$Name" "Exit code expected $ExpectedExitCode, got $exitCode"
            Write-Host "  [FAIL] $Name — Exit $exitCode (expected $ExpectedExitCode)" -ForegroundColor Red
            return
        }
        foreach ($art in $ExpectedArtifacts) {
            if (-not (Test-Path $art)) {
                Record-Fail "cli-$Name" "Missing artifact: $art"
                Write-Host "  [FAIL] $Name — Missing $art" -ForegroundColor Red
                return
            }
        }
        foreach ($sub in $OutputContains) {
            if ($stdout -notmatch [regex]::Escape($sub)) {
                Record-Fail "cli-$Name" "Output missing: $sub"
                Write-Host "  [FAIL] $Name — Output missing '$sub'" -ForegroundColor Red
                return
            }
        }
        Record-Pass "cli-$Name" "Exit $exitCode, artifacts OK"
        Write-Pass "$Name"
    } catch {
        Record-Fail "cli-$Name" $_.Exception.Message
        Write-Host "  [FAIL] $Name — $($_.Exception.Message)" -ForegroundColor Red
    }
}

# ─── Main ───────────────────────────────────────────────────────────────────
$script:PartTotal = 2
$script:PartNum = 0
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Full CLI + Win32 IDE Manifestation Tests (no simulation)" -ForegroundColor Cyan
Write-Host "  BuildDir: $script:BuildDir" -ForegroundColor Cyan
Write-Host "  WorkDir:  $script:TestWorkDir" -ForegroundColor Cyan
Write-Host "  CLI exe:  $(if($script:CLIBinary){$script:CLIBinary}else{'NOT FOUND'})" -ForegroundColor Cyan
Write-Host "  IDE exe:  $(if($script:IDEBinary){$script:IDEBinary}else{'NOT FOUND'})" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Push-Location $script:TestWorkDir

# ─── Part 1: Full CLI manifestation (every mode, real execution) ─────────────
if (-not $IDEOnly) {
    $script:PartNum++; Write-Step $script:PartNum $script:PartTotal "Part 1: CLI contract modes (real execution)"
    Write-Host "─── Part 1: CLI contract modes (real execution) ───" -ForegroundColor White
    if (-not $script:CLIBinary) {
        Record-Fail "cli-binary-present" "No CLI binary found under $script:BuildDir"
        Write-Host "  [FAIL] No RawrXD_IDE_unified.exe or RawrXD-Win32IDE.exe in build" -ForegroundColor Red
    } else {
        Invoke-CLIMode -Name "help" -Switch "--help" -OutputContains @("help")
        Invoke-CLIMode -Name "version" -Switch "--version" -OutputContains @("version")
        Invoke-CLIMode -Name "compile" -Switch "-compile" -ExpectedArtifacts @("trace_map.json")
        Invoke-CLIMode -Name "compile-c" -Switch "-c" -ExpectedArtifacts @("trace_map.json")
        Invoke-CLIMode -Name "compile-slash-c" -Switch "/c" -ExpectedArtifacts @("trace_map.json")
        Invoke-CLIMode -Name "encrypt" -Switch "-encrypt" -ExpectedArtifacts @("encrypted.bin")
        Invoke-CLIMode -Name "entropy" -Switch "-entropy"
        Invoke-CLIMode -Name "avscan" -Switch "-avscan"
        Invoke-CLIMode -Name "bbcov" -Switch "-bbcov" -ExpectedArtifacts @("bbcov_report.json")
        Invoke-CLIMode -Name "covfuse" -Switch "-covfuse" -ExpectedArtifacts @("covfusion_report.json")
        Invoke-CLIMode -Name "dyntrace" -Switch "-dyntrace"
        Invoke-CLIMode -Name "agenttrace" -Switch "-agenttrace"
        Invoke-CLIMode -Name "gapfuzz" -Switch "-gapfuzz"
        Invoke-CLIMode -Name "intelpt" -Switch "-intelpt"
        Invoke-CLIMode -Name "diffcov" -Switch "-diffcov"
        Invoke-CLIMode -Name "sideload" -Switch "-sideload"
        Invoke-CLIMode -Name "trace" -Switch "-trace" -ExpectedArtifacts @("trace_map.json")
        Invoke-CLIMode -Name "stubgen" -Switch "-stubgen"
        Invoke-CLIMode -Name "inject" -Switch "-inject"
        Invoke-CLIMode -Name "agent" -Switch "-agent" -LatencyOverrideMs 60000
        Invoke-CLIMode -Name "persist" -Switch "-persist" -Skip:$SkipPersist -SkipReason "Registry (use -SkipPersist:`$false to run)"
        Invoke-CLIMode -Name "uac" -Switch "-uac" -Skip:$SkipElevated -SkipReason "Elevation (use -SkipElevated:`$false to run)"

        # Log file
        $script:TotalTests++
        try {
            if (-not (Test-Path "rawrxd_ide.log")) { throw "rawrxd_ide.log not found" }
            $logContent = Get-Content "rawrxd_ide.log" -Raw
            if ($logContent -notmatch "\[INFO\]") { throw "Log missing [INFO]" }
            Record-Pass "cli-log-file" "rawrxd_ide.log present and structured"
            Write-Pass "log-file"
        } catch {
            Record-Fail "cli-log-file" $_.Exception.Message
            Write-Host "  [FAIL] log-file — $($_.Exception.Message)" -ForegroundColor Red
        }

        # Artifact integrity
        $script:TotalTests++
        try {
            if (-not (Test-Path "trace_map.json")) { throw "trace_map.json not found" }
            $c = Get-Content "trace_map.json" -Raw
            if ($c.Length -lt 10 -or $c -notmatch "trace_map") { throw "trace_map.json invalid" }
            Record-Pass "cli-trace-map-integrity" "trace_map.json valid"
            Write-Pass "trace-map-integrity"
        } catch {
            Record-Fail "cli-trace-map-integrity" $_.Exception.Message
            Write-Host "  [FAIL] trace-map-integrity — $($_.Exception.Message)" -ForegroundColor Red
        }

        $script:TotalTests++
        try {
            if (-not (Test-Path "bbcov_report.json")) { throw "bbcov_report.json not found" }
            $c = Get-Content "bbcov_report.json" -Raw
            if ($c -notmatch "basic_block_coverage") { throw "bbcov_report.json schema" }
            Record-Pass "cli-bbcov-integrity" "bbcov_report.json valid"
            Write-Pass "bbcov-integrity"
        } catch {
            Record-Fail "cli-bbcov-integrity" $_.Exception.Message
            Write-Host "  [FAIL] bbcov-integrity — $($_.Exception.Message)" -ForegroundColor Red
        }

        $script:TotalTests++
        try {
            if (-not (Test-Path "covfusion_report.json")) { throw "covfusion_report.json not found" }
            $c = Get-Content "covfusion_report.json" -Raw
            if ($c -notmatch "covfusion_report" -or $c -notmatch "schema_version") { throw "covfusion_report.json schema" }
            Record-Pass "cli-covfusion-integrity" "covfusion_report.json valid"
            Write-Pass "covfusion-integrity"
        } catch {
            Record-Fail "cli-covfusion-integrity" $_.Exception.Message
            Write-Host "  [FAIL] covfusion-integrity — $($_.Exception.Message)" -ForegroundColor Red
        }

        # schema_version must be 1 in both reports (contract)
        $script:TotalTests++
        try {
            $schemaFiles = @("bbcov_report.json", "covfusion_report.json")
            foreach ($sf in $schemaFiles) {
                if (Test-Path $sf) {
                    $content = Get-Content $sf -Raw
                    if ($content -notmatch '"schema_version"\s*:\s*1') { throw "$sf schema_version not 1" }
                }
            }
            Record-Pass "cli-schema-version-v1" "All reports schema_version:1"
            Write-Pass "schema-version-v1"
        } catch {
            Record-Fail "cli-schema-version-v1" $_.Exception.Message
            Write-Host "  [FAIL] schema-version-v1 — $($_.Exception.Message)" -ForegroundColor Red
        }

        # RVA-only: no absolute VAs in reports (ASLR safety)
        $script:TotalTests++
        try {
            foreach ($reportFile in @("bbcov_report.json", "covfusion_report.json")) {
                if (Test-Path $reportFile) {
                    $content = Get-Content $reportFile -Raw
                    if ($content -match '"0x[0-9A-Fa-f]{9,}"') { throw "$reportFile contains possible absolute VA" }
                }
            }
            Record-Pass "cli-rva-only-addresses" "No absolute VAs in reports"
            Write-Pass "rva-only-addresses"
        } catch {
            Record-Fail "cli-rva-only-addresses" $_.Exception.Message
            Write-Host "  [FAIL] rva-only-addresses — $($_.Exception.Message)" -ForegroundColor Red
        }

        # RawrEngine --help (headless binary) — real execution, no sim
        if ($script:RawrEngineBinary) {
            $script:TotalTests++
            try {
                $out = & $script:RawrEngineBinary --help 2>&1 | Out-String
                if ($out -notmatch "RawrXD|help|Engine") { throw "Help output missing expected text" }
                Record-Pass "rawrengine-help" "RawrEngine --help OK"
                Write-Pass "RawrEngine --help"
            } catch {
                Record-Fail "rawrengine-help" $_.Exception.Message
                Write-Host "  [FAIL] RawrEngine --help — $($_.Exception.Message)" -ForegroundColor Red
            }
        }
    }
    Write-Host ""
}

# ─── Part 2: Win32 IDE manifestation (real process, real window) ───────────────
if (-not $CLIOnly) {
    $script:PartNum++; Write-Step $script:PartNum $script:PartTotal "Part 2: Win32 IDE manifestation (real launch + window)"
    Write-Host "─── Part 2: Win32 IDE manifestation (real launch + window) ───" -ForegroundColor White
    if (-not $script:IDEBinary) {
        Record-Fail "ide-binary-present" "No IDE binary found under $script:BuildDir"
        Write-Host "  [FAIL] RawrXD-Win32IDE.exe not found" -ForegroundColor Red
    } else {
        $script:TotalTests++
        try {
            $proc = Start-Process -FilePath $script:IDEBinary -WorkingDirectory (Split-Path $script:IDEBinary) -PassThru
            $deadline = [DateTime]::UtcNow.AddSeconds($IDELaunchTimeoutSec)
            $windowFound = $false
            while ([DateTime]::UtcNow -lt $deadline) {
                Start-Sleep -Milliseconds 500
                try {
                    $proc.Refresh()
                    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $windowFound = $true; break }
                } catch { }
                if ($proc.HasExited) {
                    if ($proc.ExitCode -ne 0) { throw "IDE process exited with code $($proc.ExitCode) before window appeared" }
                    if (-not $windowFound) { throw "IDE exited before main window appeared (exit $($proc.ExitCode))" }
                    break
                }
            }
            if (-not $windowFound) {
                if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit(3000) }
                throw "Main window did not appear within ${IDELaunchTimeoutSec}s"
            }
            if (-not $proc.HasExited) {
                $proc.CloseMainWindow() | Out-Null
                if (-not $proc.WaitForExit(5000)) { $proc.Kill(); $proc.WaitForExit(2000) }
            }
            Record-Pass "ide-window-manifestation" "IDE launched, window verified, closed"
            Write-Pass "IDE process + main window manifested and closed"
        } catch {
            if ($proc -and -not $proc.HasExited) { try { $proc.Kill() } catch {} }
            Record-Fail "ide-window-manifestation" $_.Exception.Message
            Write-Host "  [FAIL] IDE manifestation — $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    Write-Host ""
}

Pop-Location

# ─── Summary ─────────────────────────────────────────────────────────────────
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RESULTS" -ForegroundColor Cyan
Write-Host "  Total: $($script:TotalTests)  Passed: $($script:PassedTests)  Failed: $($script:FailedTests)" -ForegroundColor $(if ($script:FailedTests -eq 0) { "Green" } else { "Red" })
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
$script:Results | Format-Table -Property Name, Status, Detail -AutoSize

if ($script:FailedTests -gt 0) { exit 1 }
exit 0
