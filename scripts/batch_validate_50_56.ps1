param(
    [string]$GuiExe   = "d:\rawrxd\build_prod\bin\RawrXD-Win32IDE.exe",
    [string]$CliExe   = "d:\rawrxd\build_prod\bin\RawrXD-AutoFixCLI.exe",
    [string]$RepoRoot = "d:\rawrxd",
    [string]$OutFile  = "d:\rawrxd\reports\batch_50_56_runtime_validation.json"
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
        durationMs    = [int](($FinishedAt - $StartedAt).TotalMilliseconds)
    }
}

$results = @()
$now = Get-Date
$results += New-Result -Id 50 -Name "autonomous_agent" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs agent activation test" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 51 -Name "chat_message_renderer" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "UI rendering; needs probe" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 52 -Name "tool_action_status" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs UI/status hook" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 53 -Name "chat_panel" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "UI; needs automation hook" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 54 -Name "perf_telemetry" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "telemetry endpoint check needed" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 55 -Name "update_signature" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "signing validation missing" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 56 -Name "plugin_signature" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "plugin signature verification hook needed" -StartedAt $now -FinishedAt $now

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
Write-Host "[batch-50-56] Saved report: $OutFile" -ForegroundColor Yellow

if ($summary.failed -gt 0) {
    exit 1
}
exit 0
