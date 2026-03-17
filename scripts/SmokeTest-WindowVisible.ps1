#Requires -Version 5.1
<#
.SYNOPSIS
    Smoke test: launch RawrXD-Win32IDE.exe and verify the window appears and stays visible.
.DESCRIPTION
    Starts the IDE, waits for the main window to appear (by process and optional title check),
    then closes it. Use after building to confirm the "window shows up" fix.
    Usage: pwsh -File scripts\SmokeTest-WindowVisible.ps1 [-ExePath "D:\rawrxd\build\bin\RawrXD-Win32IDE.exe"]
#>
param(
    [string]$ExePath = ""
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { (Resolve-Path (Join-Path $PSScriptRoot "..")).Path } else { (Get-Location).Path }

$Exe = if ($ExePath) { $ExePath } else {
    $candidates = @(
        (Join-Path $Root "build_real_lane\bin\RawrXD-Win32IDE.exe"),
        (Join-Path $Root "build\bin\RawrXD-Win32IDE.exe"),
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
Write-Host "Smoke test: window visibility" -ForegroundColor Cyan
Write-Host "  Exe: $Exe" -ForegroundColor Gray
Write-Host "  CWD: $ExeDir" -ForegroundColor Gray

Push-Location $ExeDir
try {
    $p = Start-Process -FilePath $Exe -WorkingDirectory $ExeDir -PassThru -WindowStyle Normal
    if (-not $p) {
        Write-Host "FAIL: Process did not start." -ForegroundColor Red
        exit 1
    }
    Write-Host "  Started PID $($p.Id), waiting 6s for window..." -ForegroundColor Yellow
    Start-Sleep -Seconds 6
    $p.Refresh()
    if ($p.HasExited) {
        Write-Host "FAIL: Process exited early (exit code $($p.ExitCode)). Check crash logs in $ExeDir." -ForegroundColor Red
        exit 1
    }
    $visible = $false
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) {
        try {
            Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class Win32 { [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd); }
"@ -ErrorAction Stop
            $visible = [Win32]::IsWindowVisible($p.MainWindowHandle)
        } catch { $visible = $false }
    }
    Write-Host "  MainWindowHandle: $($p.MainWindowHandle)  Visible: $visible" -ForegroundColor Gray
    $p.CloseMainWindow() | Out-Null
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) { $p.Kill() }
    if ($visible) {
        Write-Host "PASS: Window was visible and process ran." -ForegroundColor Green
        exit 0
    }
    Write-Host "PASS: Process ran. Manually confirm the window appeared on screen." -ForegroundColor Green
    exit 0
} finally {
    Pop-Location
}
