#requires -Version 5.1
<#
.SYNOPSIS
    Optional Phase X+1 check: launch IDE, find main window, find Edit/RichEdit child, report.

.DESCRIPTION
    Standalone validation that the editor surface (text buffer) is present.
    Use after smoke_runtime.ps1 passes. Monolithic: standard Edit control.
    C++ IDE: RichEdit (RICHEDIT50W). Exits 0 if editor child found, 1 otherwise.

.PARAMETER ExePath
    Path to EXE. Auto-detected if not set.

.PARAMETER TimeoutSeconds
    Max seconds to wait for window (default 5).

.EXAMPLE
    .\validate_editor_surface.ps1
    .\validate_editor_surface.ps1 -ExePath ".\build\monolithic\RawrXD.exe"
#>
param(
    [string]$ExePath = "",
    [int]$TimeoutSeconds = 5
)

$ErrorActionPreference = "Stop"
$ProjectRoot = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
if (-not (Test-Path $ProjectRoot)) { $ProjectRoot = "D:\rawrxd" }

function Find-RawrXDExe {
    if ($ExePath -and (Test-Path $ExePath)) { return $ExePath }
    $candidates = @(
        (Join-Path $ProjectRoot "build\monolithic\RawrXD.exe"),
        (Join-Path $ProjectRoot "build\bin\RawrXD-Win32IDE.exe"),
        (Join-Path $ProjectRoot "RawrXD-Win32IDE.exe"),
        (Join-Path $ProjectRoot "RawrXD.exe")
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    throw "No RawrXD EXE found."
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class EditCheck {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowW(string lpClassName, string lpWindowName);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowExW(IntPtr hwndParent, IntPtr hwndChildAfter, string lpszClass, string lpszWindow);
}
"@ -ErrorAction SilentlyContinue

$exe = Find-RawrXDExe
$proc = Start-Process -FilePath $exe -PassThru -WindowStyle Normal
Start-Sleep -Milliseconds 800
if ($proc.HasExited) {
    Write-Host "[validate_editor_surface] Process exited during init (exit $($proc.ExitCode))" -ForegroundColor Red
    exit 2
}

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$main = [IntPtr]::Zero
$classes = @("RawrXD_Main", "RawrXD_Monolithic", "RawrXD_IDE_MainWindow")
while ([DateTime]::UtcNow -lt $deadline) {
    foreach ($cls in $classes) {
        $main = [EditCheck]::FindWindowW($cls, $null)
        if ($main -ne [IntPtr]::Zero) { break }
    }
    if ($main -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 200
}

if ($main -eq [IntPtr]::Zero) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "[validate_editor_surface] Main window not found within ${TimeoutSeconds}s" -ForegroundColor Red
    exit 1
}

$edit = [EditCheck]::FindWindowExW($main, [IntPtr]::Zero, "Edit", $null)
if ($edit -eq [IntPtr]::Zero) { $edit = [EditCheck]::FindWindowExW($main, [IntPtr]::Zero, "RICHEDIT50W", $null) }

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

if ($edit -ne [IntPtr]::Zero) {
    Write-Host "[validate_editor_surface] Editor surface present (child hwnd 0x$($edit.ToString('X')))" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[validate_editor_surface] No Edit/RichEdit child found under main window" -ForegroundColor Yellow
    exit 1
}
