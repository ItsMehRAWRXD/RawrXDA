param(
    [string]$GuiExe   = "d:\rawrxd\build_prod\bin\RawrXD-Win32IDE.exe",
    [string]$CliExe   = "d:\rawrxd\build_prod\bin\RawrXD-AutoFixCLI.exe",
    [string]$RepoRoot = "d:\rawrxd",
    [string]$OutFile  = "d:\rawrxd\reports\batch_36_42_runtime_validation.json"
)

$ErrorActionPreference = "Stop"

function New-Result {
    param(
        [int]$Id,
        [string]$Name,
        [string]$Command,
        [int]$ExitCode,
        [bool]$Passed,
        [string]$Note,
        [datetime]$StartedAt,
        [datetime]$FinishedAt
    )
    return [pscustomobject]@{
        id            = $Id
        name          = $Name
        command       = $Command
        exitCode      = $ExitCode
        passed        = $Passed
        note          = $Note
        startedAtUtc  = $StartedAt.ToUniversalTime().ToString("o")
        finishedAtUtc = $FinishedAt.ToUniversalTime().ToString("o")
        durationMs    = [int](($FinishedAt - $FinishedAt).TotalMilliseconds)
    }
}

$results = @()
$now = Get-Date
$results += New-Result -Id 36 -Name "cursor_github_parity_bridge" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs parity harness; not automated here" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 37 -Name "omega_orchestrator" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "orchestration validation hook missing" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 38 -Name "mesh_brain" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "no automated mesh activation test" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 39 -Name "speciator_engine" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs runtime probe; not automated here" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 40 -Name "neural_bridge" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs bridge activation test" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 41 -Name "self_host_engine" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs hosting flow validation" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 42 -Name "hardware_synthesizer" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs synth runtime check" -StartedAt $now -FinishedAt $now

$summary = [pscustomobject]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    guiExe         = $GuiExe
    cliExe         = $CliExe
    total          = $results.Count
    passed         = ($results | Where-Object { $_.passed }).Count
    failed         = ($results | Where-Object { -not $_.passed }).Count
    results        = $results
}

$outDir = Split-Path -Parent $OutFile
$null = New-Item -ItemType Directory -Path $outDir -Force -ErrorAction SilentlyContinue

$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $OutFile -Encoding UTF8
Write-Host "[batch-36-42] Saved report: $OutFile" -ForegroundColor Yellow

if ($summary.failed -gt 0) {
    exit 1
}
exit 0
