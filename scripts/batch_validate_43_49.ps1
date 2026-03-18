param(
    [string]$GuiExe   = "d:\rawrxd\build_prod\bin\RawrXD-Win32IDE.exe",
    [string]$CliExe   = "d:\rawrxd\build_prod\bin\RawrXD-AutoFixCLI.exe",
    [string]$RepoRoot = "d:\rawrxd",
    [string]$OutFile  = "d:\rawrxd\reports\batch_43_49_runtime_validation.json"
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
$results += New-Result -Id 43 -Name "transcendence_coordinator" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "no automation hook; needs runtime probe" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 44 -Name "vulkan_renderer" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "requires rendering test; not automated" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 45 -Name "os_explorer_interceptor" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs interception harness" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 46 -Name "mcp_hooks" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs MCP hook validation" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 47 -Name "iocp_file_watcher" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs IOCP watch test" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 48 -Name "ide_diagnostic_autohealer" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "needs auto-heal scenario test" -StartedAt $now -FinishedAt $now

$now = Get-Date
$results += New-Result -Id 49 -Name "consent_prompt" -Command "not-automated" -ExitCode -3 -Passed:$false `
    -Note "UI prompt; manual/automation hook needed" -StartedAt $now -FinishedAt $now

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
Write-Host "[batch-43-49] Saved report: $OutFile" -ForegroundColor Yellow

if ($summary.failed -gt 0) {
    exit 1
}
exit 0
