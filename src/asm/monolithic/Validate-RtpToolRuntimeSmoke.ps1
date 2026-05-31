#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,
    [string]$WorkspaceRoot,
    [ValidateRange(5, 600)]
    [int]$TimeoutSec = 45
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$exeFullPath = (Resolve-Path -LiteralPath $ExePath).Path
if (-not $WorkspaceRoot) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
}

$smokeFile = Join-Path $WorkspaceRoot "rawrxd_agent_smoke.txt"
$stdoutTmp = Join-Path $env:TEMP ("rawrxd_rtp_smoke_stdout_{0}.txt" -f [guid]::NewGuid().ToString("N"))

try {
    if (Test-Path -LiteralPath $smokeFile) {
        Remove-Item -LiteralPath $smokeFile -Force -ErrorAction SilentlyContinue
    }

    $p = Start-Process -FilePath $exeFullPath `
                       -ArgumentList @("--agent-prompt", "--prompt", "rtp smoke") `
                       -NoNewWindow `
                       -PassThru `
                       -RedirectStandardOutput $stdoutTmp

    Wait-Process -Id $p.Id -Timeout $TimeoutSec -ErrorAction SilentlyContinue
    if (-not $p.HasExited) {
        try {
            $p.Kill()
            $p.WaitForExit(2000) | Out-Null
        } catch {
            Write-Host "[rtp-smoke] warn: failed to terminate timed-out process id=$($p.Id): $($_.Exception.Message)" -ForegroundColor Yellow
        }
        throw "agent-prompt smoke timed out after ${TimeoutSec}s."
    }

    if ($p.ExitCode -ne 0) {
        throw "agent-prompt smoke failed (exit $($p.ExitCode))."
    }

    $stdoutLine = ""
    if (Test-Path -LiteralPath $stdoutTmp) {
        $stdoutLine = (Get-Content -LiteralPath $stdoutTmp -TotalCount 1 -ErrorAction SilentlyContinue)
        if ($null -eq $stdoutLine) { $stdoutLine = "" }
    }

    if ([string]::IsNullOrWhiteSpace($stdoutLine)) {
        throw "agent-prompt smoke produced empty stdout."
    }

    if (-not $stdoutLine.StartsWith("agent:")) {
        throw "agent-prompt envelope mismatch. Expected prefix 'agent:', got: $stdoutLine"
    }

    if (-not (Test-Path -LiteralPath $smokeFile)) {
        throw "agent smoke file missing: $smokeFile"
    }

    $smokeLine = (Get-Content -LiteralPath $smokeFile -TotalCount 1 -ErrorAction SilentlyContinue)
    if ($null -eq $smokeLine) { $smokeLine = "" }

    if ([string]::IsNullOrWhiteSpace($smokeLine)) {
        throw "agent smoke file is empty: $smokeFile"
    }

    if (-not $smokeLine.StartsWith("agent:")) {
        throw "agent smoke file envelope mismatch. Expected prefix 'agent:', got: $smokeLine"
    }

    Write-Host "[rtp-smoke] OK - runtime envelope validated." -ForegroundColor Green
    Write-Host "  stdout_head=$stdoutLine" -ForegroundColor DarkGray
    Write-Host "  smoke_head=$smokeLine" -ForegroundColor DarkGray
    exit 0
}
finally {
    if (Test-Path -LiteralPath $stdoutTmp) {
        Remove-Item -LiteralPath $stdoutTmp -Force -ErrorAction SilentlyContinue
    }
}
