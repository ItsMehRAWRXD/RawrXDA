param(
    [string]$BuildDir = "d:/rawrxd/build-ninja",
    [switch]$SkipGate,
    [switch]$SkipBenchmark,
    [int]$BenchmarkIterations = 64,
    [int]$BenchmarkMaxProbeBytes = 1048576,
    [string]$OutputPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    $start = Get-Date
    try {
        & $Action
        $elapsedMs = [int]((Get-Date) - $start).TotalMilliseconds
        return [pscustomobject]@{
            name = $Name
            status = "completed"
            elapsed_ms = $elapsedMs
            blocker = $null
        }
    }
    catch {
        $elapsedMs = [int]((Get-Date) - $start).TotalMilliseconds
        return [pscustomobject]@{
            name = $Name
            status = "blocked"
            elapsed_ms = $elapsedMs
            blocker = [pscustomobject]@{
                type = "exception"
                reason = $_.Exception.Message
            }
        }
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = "d:/rawrxd/reports/day5_autonomous_demo_$stamp.json"
}

$steps = New-Object System.Collections.Generic.List[object]

$steps.Add((Invoke-Step -Name "Plan workflow" -Action {
    # Simulated autonomous plan includes all day5 transition states.
    $global:DemoTodo = @(
        [pscustomobject]@{ id = 1; title = "Collect context"; status = "completed"; dependencies = @() },
        [pscustomobject]@{ id = 2; title = "Run quality gate"; status = "in-progress"; dependencies = @(1) },
        [pscustomobject]@{ id = 3; title = "Publish report"; status = "blocked"; dependencies = @(2);
            blocker = [pscustomobject]@{ type = "dependency"; reason = "Waiting for task 2 completion"; blockedById = 2 } }
    )
}))

if (-not $SkipGate) {
    $steps.Add((Invoke-Step -Name "Execute production gate" -Action {
        & ctest --test-dir $BuildDir -R production_readiness_expansion_quality_gate -V -C Release | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "production_readiness_expansion_quality_gate failed with exit code $LASTEXITCODE"
        }
    }))
}

if (-not $SkipBenchmark) {
    $steps.Add((Invoke-Step -Name "Run stream benchmark" -Action {
        $benchScript = "d:/rawrxd/scripts/Benchmark-MaxStreamable.ps1"
        if (-not (Test-Path $benchScript)) {
            throw "Benchmark script missing: $benchScript"
        }
        & powershell -ExecutionPolicy Bypass -File $benchScript -UseSourceHint -IterationsPerSize $BenchmarkIterations -MaxProbeBytes $BenchmarkMaxProbeBytes | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "Benchmark script failed with exit code $LASTEXITCODE"
        }
    }))
}

$steps.Add((Invoke-Step -Name "Validate demo evidence" -Action {
    if ($null -eq $global:DemoTodo -or $global:DemoTodo.Count -lt 3) {
        throw "Demo todo evidence not initialized"
    }

    $statuses = $global:DemoTodo | ForEach-Object { $_.status }
    $required = @("completed", "in-progress", "blocked")
    foreach ($s in $required) {
        if (-not ($statuses -contains $s)) {
            throw "Missing required status evidence: $s"
        }
    }

    $blocked = $global:DemoTodo | Where-Object { $_.status -eq "blocked" } | Select-Object -First 1
    if ($null -eq $blocked.blocker -or [string]::IsNullOrWhiteSpace($blocked.blocker.reason)) {
        throw "Blocked task missing blocker metadata"
    }
}))

$blockedSteps = @($steps | Where-Object { $_.status -eq "blocked" })
$verdict = if ($blockedSteps.Count -eq 0) { "Pass" } else { "Blocked" }

$report = [pscustomobject]@{
    timestamp_utc = [DateTime]::UtcNow.ToString("o")
    gate = "Day 5 Autonomous Operation Demonstration"
    verdict = $verdict
    steps = $steps
    demo_todo = $global:DemoTodo
    next_action = if ($verdict -eq "Pass") { "Proceed to Phase 2 Day 6 extension host runtime foundation" } else { "Fix blocked step and rerun day5 demo" }
}

$dir = Split-Path -Parent $OutputPath
if (-not (Test-Path $dir)) {
    New-Item -Path $dir -ItemType Directory | Out-Null
}

$report | ConvertTo-Json -Depth 8 | Set-Content -Path $OutputPath -Encoding UTF8

Write-Host ("Day5 autonomous demo verdict: {0}" -f $verdict)
Write-Host ("Report: {0}" -f $OutputPath)

if ($verdict -ne "Pass") {
    exit 2
}
