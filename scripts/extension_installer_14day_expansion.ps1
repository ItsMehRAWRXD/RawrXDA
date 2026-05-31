param(
    [ValidateRange(1,15)]
    [int]$Day = 1,
    [switch]$Live,
    [switch]$LiveInstall,
    [switch]$Strict,
    [switch]$SkipIntegrationGate,
    [switch]$TryBuildSmoke,
    [string]$BuildDir = "",
    [string]$ReportDir = "D:/rawrxd/contract_reports/extension-installer"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-BuildDir {
    param([string]$Prefer)

    if ($Prefer -and (Test-Path -LiteralPath (Join-Path $Prefer "CMakeCache.txt"))) {
        return (Resolve-Path -LiteralPath $Prefer).Path
    }

    foreach ($c in @(
            (Join-Path $repoRoot "build-win32"),
            (Join-Path $repoRoot "build-ninja"),
            (Join-Path $repoRoot "build-ninja-ctx2"),
            (Join-Path $repoRoot "build_smoke_auto"),
            (Join-Path $repoRoot "build"))) {
        $cache = Join-Path $c "CMakeCache.txt"
        if (Test-Path -LiteralPath $cache) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }

    return $null
}

function Find-ExtensionInstallerExe {
    param([string]$Bd)

    if (-not $Bd) {
        return $null
    }

    foreach ($p in @(
            (Join-Path $Bd "bin\Release\ExtensionInstallerSmoke.exe"),
            (Join-Path $Bd "bin\Debug\ExtensionInstallerSmoke.exe"),
            (Join-Path $Bd "bin\ExtensionInstallerSmoke.exe"),
            (Join-Path $Bd "ExtensionInstallerSmoke.exe"))) {
        if (Test-Path -LiteralPath $p) {
            return $p
        }
    }

    return $null
}

function Ensure-Dir([string]$Path) {
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Run-Smoke([string[]]$SmokeArgs, [string]$Label) {
    $exe = Find-ExtensionInstallerExe -Bd $script:ResolvedBuildDir
    if (-not $exe -and $TryBuildSmoke -and $script:ResolvedBuildDir) {
        Write-Host "[14D] Building ExtensionInstallerSmoke in $($script:ResolvedBuildDir)" -ForegroundColor Cyan
        & cmake --build $script:ResolvedBuildDir --target ExtensionInstallerSmoke --parallel
        if ($LASTEXITCODE -ne 0) {
            throw "cmake build ExtensionInstallerSmoke exit $LASTEXITCODE"
        }
        $exe = Find-ExtensionInstallerExe -Bd $script:ResolvedBuildDir
    }

    if (-not (Test-Path $exe)) {
        throw "Smoke test executable not found under build dir '$($script:ResolvedBuildDir)'; set -BuildDir, configure CMake tree, or pass -TryBuildSmoke"
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $out = Join-Path $ReportDir "${stamp}_${Label}.out.txt"
    $err = Join-Path $ReportDir "${stamp}_${Label}.err.txt"

    Write-Host "[14D] Running: $exe $($SmokeArgs -join ' ')"
    & $exe @SmokeArgs 1> $out 2> $err
    $exitCode = $LASTEXITCODE

    Write-Host "[14D] ExitCode=$exitCode"
    Write-Host "[14D] Out=$out"
    Write-Host "[14D] Err=$err"

    if ($Strict -and $exitCode -ne 0) {
        throw "Smoke run failed in strict mode: $Label"
    }

    return [pscustomobject]@{
        Label = $Label
        ExitCode = $exitCode
        OutFile = $out
        ErrFile = $err
    }
}

function Run-IntegrationGateMinimal([string]$Label) {
    $gateScript = Join-Path $repoRoot "scripts\Run-IntegrationGateMinimal.ps1"
    if (-not (Test-Path -LiteralPath $gateScript)) {
        throw "Integration gate script not found: $gateScript"
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $out = Join-Path $ReportDir "${stamp}_${Label}.out.txt"
    $err = Join-Path $ReportDir "${stamp}_${Label}.err.txt"

    $gateArgs = @("-NoProfile", "-File", $gateScript)
    if ($BuildDir) {
        $gateArgs += @("-BuildDir", $BuildDir)
    }
    $gateArgs += "-TryBuildExtensionInstaller"

    Write-Host "[15D] Running integration gate: pwsh $($gateArgs -join ' ')"
    & pwsh @gateArgs 1> $out 2> $err
    $exitCode = $LASTEXITCODE

    Write-Host "[15D] Integration gate ExitCode=$exitCode"
    Write-Host "[15D] Out=$out"
    Write-Host "[15D] Err=$err"

    if ($Strict -and $exitCode -ne 0) {
        throw "Integration gate failed in strict mode: $Label"
    }

    return [pscustomobject]@{
        Label = $Label
        ExitCode = $exitCode
        OutFile = $out
        ErrFile = $err
    }
}

Ensure-Dir $ReportDir

$script:ResolvedBuildDir = Resolve-BuildDir -Prefer $BuildDir

$results = @()

switch ($Day) {
    1 {
        # Baseline coherence and security in offline mode.
        $results += Run-Smoke -SmokeArgs @("--failure-inject", "--stress", "3") -Label "day01_baseline"
    }
    2 {
        # Marketplace metadata validation.
        $args = @("--live")
        if (-not $Live) { $args = @() }
        $results += Run-Smoke -SmokeArgs $args -Label "day02_marketplace"
    }
    3 {
        # Installer idempotency and pending-state stability.
        $results += Run-Smoke -SmokeArgs @("--stress", "10") -Label "day03_idempotency"
    }
    4 {
        # Failure recovery paths.
        $results += Run-Smoke -SmokeArgs @("--failure-inject", "--stress", "5") -Label "day04_failure_recovery"
    }
    5 {
        # Live install path for target language extensions.
        $args = @("--live-install")
        if ($Live) { $args += "--live" }
        $results += Run-Smoke -SmokeArgs $args -Label "day05_live_install"
    }
    6 {
        # Parallel stress medium.
        $args = @("--stress", "15")
        if ($Live) { $args = @("--stress-live", "10") }
        $results += Run-Smoke -SmokeArgs $args -Label "day06_parallel_medium"
    }
    7 {
        # Parallel stress high.
        $args = @("--stress", "25")
        if ($Live) { $args = @("--stress-live", "20") }
        $results += Run-Smoke -SmokeArgs $args -Label "day07_parallel_high"
    }
    8 {
        # Combined live + failure injection.
        $args = @("--failure-inject")
        if ($Live) { $args += "--live" }
        $results += Run-Smoke -SmokeArgs $args -Label "day08_combined"
    }
    9 {
        # Activation artifact assertions with live install.
        $args = @("--live-install")
        if ($Live) { $args += "--live" }
        $results += Run-Smoke -SmokeArgs $args -Label "day09_activation_artifacts"
    }
    10 {
        # Stress + failure in one pass.
        $args = @("--failure-inject", "--stress", "20")
        if ($Live) { $args = @("--failure-inject", "--stress-live", "15") }
        $results += Run-Smoke -SmokeArgs $args -Label "day10_stress_failure"
    }
    11 {
        # Re-run baseline to prove no regression drift.
        $results += Run-Smoke -SmokeArgs @("--failure-inject", "--stress", "3") -Label "day11_regression_baseline"
    }
    12 {
        # Live marketplace consistency pass.
        $args = @("--live")
        if (-not $Live) { $args = @() }
        $results += Run-Smoke -SmokeArgs $args -Label "day12_marketplace_consistency"
    }
    13 {
        # Pre-release gate.
        $args = @("--failure-inject", "--stress", "10")
        if ($Live) { $args = @("--live", "--failure-inject", "--stress-live", "10") }
        $results += Run-Smoke -SmokeArgs $args -Label "day13_release_gate"
    }
    14 {
        # Final production certification pass (matches docs: live + failure + stress-live 10; offline fallback without -Live).
        if ($Live) {
            $args = @("--live", "--failure-inject", "--stress-live", "10")
            if ($LiveInstall) { $args += "--live-install" }
        }
        else {
            $args = @("--failure-inject", "--stress", "10")
        }
        $results += Run-Smoke -SmokeArgs $args -Label "day14_certification"
    }
    15 {
        # Day 15: installer regression baseline + minimal system integration gate (IDE/agentic smoke chain).
        $results += Run-Smoke -SmokeArgs @("--failure-inject", "--stress", "3") -Label "day15_installer_regression_baseline"
        if (-not $SkipIntegrationGate) {
            $results += Run-IntegrationGateMinimal -Label "day15_integration_gate_minimal"
        }
    }
    default {
        throw "Unsupported day: $Day"
    }
}

$summaryPath = Join-Path $ReportDir ("summary_day{0:00}_{1}.txt" -f $Day, (Get-Date -Format "yyyyMMdd_HHmmss"))
$summary = @()
$summary += "RawrXD Extension Installer expansion (days 1-15; day 15 adds integration gate)"
$summary += "Day=$Day Live=$Live LiveInstall=$LiveInstall Strict=$Strict SkipIntegrationGate=$SkipIntegrationGate TryBuildSmoke=$TryBuildSmoke"
$summary += "ResolvedBuildDir=$script:ResolvedBuildDir"
$summary += ""
foreach ($r in $results) {
    $summary += ("Label={0}" -f $r.Label)
    $summary += ("ExitCode={0}" -f $r.ExitCode)
    $summary += ("OutFile={0}" -f $r.OutFile)
    $summary += ("ErrFile={0}" -f $r.ErrFile)
    $summary += ""
}
$summary | Set-Content -Path $summaryPath -Encoding UTF8

Write-Host "[14D] Summary: $summaryPath"
Write-Host "[14D] Completed day $Day"
