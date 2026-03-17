#Requires -Version 5.1
<#
.SYNOPSIS
    Full smoke test for RawrXD IDE: CLI, startup phases, agentic, no skips.
.DESCRIPTION
    Runs --help, --version, --selftest; checks ide_startup.log for _done (no _skipped).
    Run from repo root: pwsh -File .\SmokeTest-IDE-Full.ps1
#>

param(
    [string]$ExePath = "",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }

$Exe = if ($ExePath) { $ExePath } else {
    $candidates = @(
        (Join-Path $Root "$BuildDir\bin\RawrXD-Win32IDE.exe"),
        (Join-Path $Root "bin\RawrXD-Win32IDE.exe"),
        (Join-Path $Root "RawrXD-Win32IDE.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $c; break }
    }
}

if (-not $Exe -or -not (Test-Path $Exe)) {
    Write-Host "ERROR: RawrXD-Win32IDE.exe not found. Build first or pass -ExePath." -ForegroundColor Red
    exit 1
}

$ExeDir = Split-Path -Parent $Exe
$Pass = 0
$Fail = 0

function Test-Step {
    param([string]$Name, [scriptblock]$Run)
    try {
        $ok = & $Run
        if ($ok) { $script:Pass++; Write-Host "  [PASS] $Name" -ForegroundColor Green } else { $script:Fail++; Write-Host "  [FAIL] $Name" -ForegroundColor Red }
        return $ok
    } catch {
        $script:Fail++
        Write-Host "  [FAIL] $Name - $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

Write-Host ""
Write-Host "=== RawrXD IDE full smoke test ===" -ForegroundColor Cyan
Write-Host "  Exe: $Exe" -ForegroundColor Gray
Write-Host ""

Push-Location $ExeDir

# CLI: --help
Test-Step "CLI --help" {
    $p = Start-Process -FilePath $Exe -ArgumentList "--help" -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\rawrxd_help.txt" -RedirectStandardError "$env:TEMP\rawrxd_help_err.txt"
    if ($p.ExitCode -ne 0) { return $false }
    $out = Get-Content "$env:TEMP\rawrxd_help.txt" -Raw -ErrorAction SilentlyContinue
    $out -match "Usage|Options|RawrXD"
}

# CLI: --version
Test-Step "CLI --version" {
    $p = Start-Process -FilePath $Exe -ArgumentList "--version" -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\rawrxd_ver.txt" -RedirectStandardError "$env:TEMP\rawrxd_ver_err.txt"
    if ($p.ExitCode -ne 0) { return $false }
    $out = Get-Content "$env:TEMP\rawrxd_ver.txt" -Raw -ErrorAction SilentlyContinue
    $out -match "RawrXD|v\d"
}

# Selftest (quick exit, no message loop)
Test-Step "Selftest (--selftest)" {
    $p = Start-Process -FilePath $Exe -ArgumentList "--selftest" -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\rawrxd_selftest.txt" -RedirectStandardError "$env:TEMP\rawrxd_selftest_err.txt"
    $out = Get-Content "$env:TEMP\rawrxd_selftest.txt" -Raw -ErrorAction SilentlyContinue
    $out -match "result=PASS|PASS"
}

# After a normal run, ide_startup.log should have _done not _skipped (run GUI briefly then check log)
Test-Step "Startup log has enterprise_license_done (no skip)" {
    $logPath = Join-Path $ExeDir "ide_startup.log"
    if (Test-Path $logPath) { Remove-Item $logPath -Force }
    $proc = Start-Process -FilePath $Exe -PassThru -WorkingDirectory $ExeDir
    Start-Sleep -Seconds 4
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 1
    if (-not (Test-Path $logPath)) { return $false }
    $log = Get-Content $logPath -Raw
    $log -match "enterprise_license_done" -and $log -notmatch "enterprise_license_skipped"
}

Pop-Location

Write-Host ""
Write-Host "Result: $Pass passed, $Fail failed" -ForegroundColor $(if ($Fail -eq 0) { "Green" } else { "Yellow" })
if ($Fail -gt 0) { exit 1 }
exit 0
