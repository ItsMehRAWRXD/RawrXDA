param(
    [ValidateRange(1, 14)]
    [int]$Day,
    [switch]$AllDays,
    [switch]$Strict,
    [switch]$Fast,
    [switch]$FastSmoke,
    [switch]$NoBuild,
    [switch]$NoTests,
    [string]$BuildDir = "",
    [string]$BaselineRef = "main",
    [ValidateRange(1, 500000)]
    [int]$MaxAddedLines = 10000,
    [string[]]$ScopePaths = @("src", "include", "tests", "scripts"),
    [switch]$SkipPostRunCheck,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

$reportsRoot = Join-Path $repoRoot "reports\14day"
if (-not (Test-Path -LiteralPath $reportsRoot)) {
    New-Item -Path $reportsRoot -ItemType Directory -Force | Out-Null
}

$gateJson = Join-Path $reportsRoot "under10k_gate.json"
$gateMd = Join-Path $reportsRoot "under10k_gate.md"
$orchestratorEntry = Join-Path $repoRoot "Run-14Day-ProductionFinishers.ps1"

function Require-Git {
    try {
        $null = & git --version
    } catch {
        throw "git is required to compute the line budget gate"
    }
}

function Resolve-MergeBase {
    param([string]$RefName)

    $base = (& git merge-base HEAD $RefName 2>$null)
    if (-not $base) {
        throw "Unable to resolve merge-base for ref '$RefName'. Verify the ref exists locally or fetch it first."
    }
    return ($base | Select-Object -First 1).Trim()
}

function Parse-NumStat {
    param([string[]]$Lines)

    $sum = 0
    foreach ($line in $Lines) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        $parts = $line -split "`t"
        if ($parts.Length -lt 3) {
            continue
        }

        $added = $parts[0]
        if ($added -eq "-" -or -not ($added -match '^\d+$')) {
            continue
        }

        $sum += [int]$added
    }

    return $sum
}

function Get-AddedLines {
    param(
        [string]$BaseSha,
        [string[]]$PathScope
    )

    $committedArgs = @("diff", "--numstat", "$BaseSha...HEAD", "--") + $PathScope
    $workingArgs = @("diff", "--numstat", "HEAD", "--") + $PathScope

    $committedRaw = & git @committedArgs
    $workingRaw = & git @workingArgs

    $committed = Parse-NumStat -Lines $committedRaw
    $working = Parse-NumStat -Lines $workingRaw

    return [ordered]@{
        committed = $committed
        working = $working
        total = ($committed + $working)
    }
}

function Write-GateArtifacts {
    param(
        $Snapshot,
        [int]$ExitCode,
        [string]$Result,
        [string]$Reason
    )

    $payload = [ordered]@{
        timestampUtc = (Get-Date).ToUniversalTime().ToString("o")
        baselineRef = $BaselineRef
        baselineMergeBase = $Snapshot.baseSha
        scopePaths = $ScopePaths
        maxAddedLines = $MaxAddedLines
        lines = [ordered]@{
            committed = $Snapshot.committed
            working = $Snapshot.working
            total = $Snapshot.total
            remainingBudget = ($MaxAddedLines - $Snapshot.total)
        }
        execution = [ordered]@{
            allDays = [bool]$AllDays
            day = if ($PSBoundParameters.ContainsKey("Day")) { $Day } else { $null }
            strict = [bool]$Strict
            fast = [bool]$Fast
            fastSmoke = [bool]$FastSmoke
            noBuild = [bool]$NoBuild
            noTests = [bool]$NoTests
            buildDir = $BuildDir
            dryRun = [bool]$DryRun
        }
        result = $Result
        reason = $Reason
        exitCode = $ExitCode
    }

    $payload | ConvertTo-Json -Depth 8 | Set-Content -Path $gateJson -Encoding UTF8

    $md = @()
    $md += "# 14-Day Under 10k Gate"
    $md += ""
    $md += "Result: **$Result**"
    $md += ""
    $md += "Reason: $Reason"
    $md += ""
    $md += "- Baseline ref: $BaselineRef"
    $md += "- Merge base: $($Snapshot.baseSha)"
    $md += "- Scope: $($ScopePaths -join ', ')"
    $md += "- Max added lines: $MaxAddedLines"
    $md += "- Added (committed): $($Snapshot.committed)"
    $md += "- Added (working): $($Snapshot.working)"
    $md += "- Added (total): $($Snapshot.total)"
    $md += "- Remaining budget: $($MaxAddedLines - $Snapshot.total)"
    $md += ""
    $md += "Execution:"
    $md += "- AllDays: $([bool]$AllDays)"
    $md += "- Day: $(if ($PSBoundParameters.ContainsKey('Day')) { $Day } else { 'n/a' })"
    $md += "- Strict: $([bool]$Strict)"
    $md += "- Fast: $([bool]$Fast)"
    $md += "- FastSmoke: $([bool]$FastSmoke)"
    $md += "- NoBuild: $([bool]$NoBuild)"
    $md += "- NoTests: $([bool]$NoTests)"
    $md += "- BuildDir: $(if ($BuildDir) { $BuildDir } else { 'auto' })"
    $md += "- DryRun: $([bool]$DryRun)"

    $md -join "`r`n" | Set-Content -Path $gateMd -Encoding UTF8
}

Require-Git

if (-not (Test-Path -LiteralPath $orchestratorEntry)) {
    throw "Missing orchestrator entrypoint: $orchestratorEntry"
}

$baseSha = Resolve-MergeBase -RefName $BaselineRef
$before = Get-AddedLines -BaseSha $baseSha -PathScope $ScopePaths
$before["baseSha"] = $baseSha

Write-Host "[Under10k] Baseline merge-base: $baseSha" -ForegroundColor Cyan
Write-Host "[Under10k] Added lines before run: committed=$($before.committed), working=$($before.working), total=$($before.total), budget=$MaxAddedLines" -ForegroundColor Cyan

if ($before.total -gt $MaxAddedLines) {
    $msg = "Line budget exceeded before run ($($before.total) > $MaxAddedLines)."
    Write-Warning $msg
    Write-GateArtifacts -Snapshot $before -ExitCode 3 -Result "BLOCKED" -Reason $msg
    exit 3
}

if ($DryRun) {
    $msg = "Dry run complete. Gate pre-check passed."
    Write-Host "[Under10k] $msg" -ForegroundColor Green
    Write-GateArtifacts -Snapshot $before -ExitCode 0 -Result "PASS" -Reason $msg
    exit 0
}

$args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $orchestratorEntry)
if ($AllDays) {
    $args += "-AllDays"
} elseif ($PSBoundParameters.ContainsKey("Day")) {
    $args += @("-Day", $Day)
} else {
    $args += "-AllDays"
}

if ($Strict) { $args += "-Strict" }
if ($FastSmoke) { $args += "-FastSmoke" }

if ($Fast) {
    $args += "-NoBuild"
    $args += "-NoTests"
} else {
    if ($NoBuild) { $args += "-NoBuild" }
    if ($NoTests) { $args += "-NoTests" }
}

if ($BuildDir) {
    $args += @("-BuildDir", $BuildDir)
}

Write-Host "[Under10k] Running 14-day orchestrator with hard line-budget gate..." -ForegroundColor Cyan
& powershell @args
$runExit = $LASTEXITCODE

if ($SkipPostRunCheck) {
    $msg = "Run finished with exit code $runExit. Post-run budget check skipped by request."
    Write-Host "[Under10k] $msg" -ForegroundColor Yellow
    Write-GateArtifacts -Snapshot $before -ExitCode $runExit -Result (if ($runExit -eq 0) { "PASS" } else { "FAIL" }) -Reason $msg
    exit $runExit
}

$after = Get-AddedLines -BaseSha $baseSha -PathScope $ScopePaths
$after["baseSha"] = $baseSha

Write-Host "[Under10k] Added lines after run: committed=$($after.committed), working=$($after.working), total=$($after.total), budget=$MaxAddedLines" -ForegroundColor Cyan

if ($after.total -gt $MaxAddedLines) {
    $msg = "Line budget exceeded after run ($($after.total) > $MaxAddedLines)."
    Write-Warning $msg
    Write-GateArtifacts -Snapshot $after -ExitCode 4 -Result "BLOCKED" -Reason $msg
    exit 4
}

if ($runExit -ne 0) {
    $msg = "Orchestrator failed with exit code $runExit while staying within budget."
    Write-Warning $msg
    Write-GateArtifacts -Snapshot $after -ExitCode $runExit -Result "FAIL" -Reason $msg
    exit $runExit
}

$msg = "Full run passed and stayed within line budget."
Write-Host "[Under10k] $msg" -ForegroundColor Green
Write-GateArtifacts -Snapshot $after -ExitCode 0 -Result "PASS" -Reason $msg
exit 0
