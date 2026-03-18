param(
    [string]$GuiExe   = "d:\rawrxd\build_prod\bin\RawrXD-Win32IDE.exe",
    [string]$CliExe   = "d:\rawrxd\build_prod\bin\RawrXD-AutoFixCLI.exe",
    [string]$RepoRoot = "d:\rawrxd",
    [string]$OutFile  = "d:\rawrxd\reports\batch_8_14_runtime_validation.json"
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

function Invoke-ProcessWithTimeout {
    param(
        [int]$Id,
        [string]$Name,
        [string]$FilePath,
        [string]$ArgumentList,
        [int]$TimeoutSec = 45
    )

    if (-not (Test-Path $FilePath)) {
        $now = Get-Date
        return New-Result -Id $Id -Name $Name -Command "$FilePath $ArgumentList" -ExitCode -1 -Passed:$false `
            -Note "missing binary" -StartedAt $now -FinishedAt $now
    }

    $started = Get-Date
    try {
        $p = Start-Process -FilePath $FilePath -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden
    } catch {
        $finished = Get-Date
        return New-Result -Id $Id -Name $Name -Command "$FilePath $ArgumentList" -ExitCode -2 -Passed:$false `
            -Note $_.Exception.Message -StartedAt $started -FinishedAt $finished
    }

    $exited = $p.WaitForExit($TimeoutSec * 1000)
    $finished = Get-Date
    if (-not $exited) {
        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
        return New-Result -Id $Id -Name $Name -Command "$FilePath $ArgumentList" -ExitCode 9999 -Passed:$false `
            -Note "timeout ${TimeoutSec}s" -StartedAt $started -FinishedAt $finished
    }

    $exitCode = $p.ExitCode
    return New-Result -Id $Id -Name $Name -Command "$FilePath $ArgumentList" -ExitCode $exitCode -Passed:($exitCode -eq 0) `
        -Note "" -StartedAt $started -FinishedAt $finished
}

function Invoke-PwshScript {
    param(
        [int]$Id,
        [string]$Name,
        [string]$ScriptPath,
        [string[]]$ScriptArgs = @(),
        [int]$TimeoutSec = 90
    )

    if (-not (Test-Path $ScriptPath)) {
        $now = Get-Date
        return New-Result -Id $Id -Name $Name -Command "$ScriptPath $($ScriptArgs -join ' ')" -ExitCode -1 -Passed:$false `
            -Note "missing script" -StartedAt $now -FinishedAt $now
    }

    $pwsh = "pwsh.exe"
    if (-not (Get-Command $pwsh -ErrorAction SilentlyContinue)) {
        $pwsh = "powershell.exe"
    }

    $args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $ScriptArgs
    $started = Get-Date
    try {
        $p = Start-Process -FilePath $pwsh -ArgumentList $args -PassThru -WindowStyle Hidden -WorkingDirectory $RepoRoot
    } catch {
        $finished = Get-Date
        return New-Result -Id $Id -Name $Name -Command "$pwsh $($args -join ' ')" -ExitCode -2 -Passed:$false `
            -Note $_.Exception.Message -StartedAt $started -FinishedAt $finished
    }

    $exited = $p.WaitForExit($TimeoutSec * 1000)
    $finished = Get-Date
    if (-not $exited) {
        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
        return New-Result -Id $Id -Name $Name -Command "$pwsh $($args -join ' ')" -ExitCode 9999 -Passed:$false `
            -Note "timeout ${TimeoutSec}s" -StartedAt $started -FinishedAt $finished
    }

    $exitCode = $p.ExitCode
    return New-Result -Id $Id -Name $Name -Command "$pwsh $($args -join ' ')" -ExitCode $exitCode `
        -Passed:($exitCode -eq 0) -Note "" -StartedAt $started -FinishedAt $finished
}

$results = @()

# 8) Chat completion via CLI inference test
$results += Invoke-ProcessWithTimeout -Id 8 -Name "chat_completion_in_ide" -FilePath $CliExe `
    -ArgumentList '--test-inference "ping from batch 8-14"' -TimeoutSec 45

# 9) Terminal panel split behavior
$results += Invoke-ProcessWithTimeout -Id 9 -Name "terminal_panel_split" -FilePath $GuiExe `
    -ArgumentList "--test-terminal-split" -TimeoutSec 45

# 10) Auto-save timer wiring
$results += Invoke-ProcessWithTimeout -Id 10 -Name "auto_save_toggle" -FilePath $GuiExe `
    -ArgumentList "--test-autosave" -TimeoutSec 45

# 11) File open/save + dispatch sanity (Win32 selftest) — execute and capture exit
$results += Invoke-ProcessWithTimeout -Id 11 -Name "file_open_save_selftest" -FilePath $GuiExe `
    -ArgumentList "--selftest" -TimeoutSec 60

# 12) Agentic request flow (headless script)
$results += Invoke-PwshScript -Id 12 -Name "agentic_request_flow" -ScriptPath (Join-Path $RepoRoot "Test-Agentic-Headless.ps1") `
    -ScriptArgs @("-SmokeOnly") -TimeoutSec 30

# 13) 70B GGUF hotpatch (not implemented)
$now = Get-Date
$results += New-Result -Id 13 -Name "gguf_70b_hotpatch" -Command "not-implemented" -ExitCode -3 -Passed:$false `
    -Note "missing implementation" -StartedAt $now -FinishedAt $now

# 14) Layer eviction system (not implemented)
$now = Get-Date
$results += New-Result -Id 14 -Name "layer_eviction_system" -Command "not-implemented" -ExitCode -3 -Passed:$false `
    -Note "missing implementation" -StartedAt $now -FinishedAt $now

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
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $OutFile -Encoding UTF8
Write-Host "[batch-8-14] Saved report: $OutFile" -ForegroundColor Yellow

if ($summary.failed -gt 0) {
    exit 1
}
exit 0
