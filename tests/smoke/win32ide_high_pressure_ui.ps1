param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [int]$StartupTimeoutSec = 45,
    [int]$StressCycles = 40,
    [int]$PostSettleSec = 8,

    # Safe default command path through model subsystem (lists/refreshes models).
    # Override to a download command id in local labs if desired.
    [int]$ModelCommandId = 1036
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Executable not found: $ExePath"
}

$signature = @"
using System;
using System.Runtime.InteropServices;

public static class Win32Interop {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr hWnd, UInt32 msg, IntPtr wParam, IntPtr lParam);
}
"@

Add-Type -TypeDefinition $signature -Language CSharp

$WM_COMMAND = 0x0111
$SW_RESTORE = 9

# Command IDs from Win32IDE command registry.
$IDM_VIEW_FILE_EXPLORER = 2030
$IDM_LSP_START_ALL = 5058
$IDM_LSP_SHOW_DIAGNOSTICS = 5065
$IDM_LSP_SHOW_SYMBOL_INFO = 5068
$IDM_REFACTOR_SHOW_ALL = 11546

Write-Host "[stress] Launching: $ExePath"
$proc = Start-Process -FilePath $ExePath -PassThru

try {
    $deadline = (Get-Date).AddSeconds($StartupTimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $proc.Refresh()
        if ($proc.HasExited) {
            if ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) {
                Write-Host "[stress] INFO: Win32IDE exited during startup with non-crash code $($proc.ExitCode)"
                return
            }

            throw "Win32IDE exited early with code $($proc.ExitCode)"
        }

        if ($proc.MainWindowHandle -ne 0) {
            break
        }

        Start-Sleep -Milliseconds 200
    }

    $proc.Refresh()
    if ($proc.MainWindowHandle -eq 0) {
        throw "Timed out waiting for Win32IDE main window handle"
    }

    $hwnd = [IntPtr]$proc.MainWindowHandle

    [void][Win32Interop]::ShowWindowAsync($hwnd, $SW_RESTORE)
    [void][Win32Interop]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 500

    Write-Host "[stress] Sidebar open command"
    [void][Win32Interop]::PostMessage($hwnd, $WM_COMMAND, [IntPtr]$IDM_VIEW_FILE_EXPLORER, [IntPtr]::Zero)

    Write-Host "[stress] LSP start + diagnostic/refactor pressure loop ($StressCycles cycles)"
    [void][Win32Interop]::PostMessage($hwnd, $WM_COMMAND, [IntPtr]$IDM_LSP_START_ALL, [IntPtr]::Zero)

    for ($i = 0; $i -lt $StressCycles; $i++) {
        [void][Win32Interop]::PostMessage($hwnd, $WM_COMMAND, [IntPtr]$IDM_LSP_SHOW_DIAGNOSTICS, [IntPtr]::Zero)
        [void][Win32Interop]::PostMessage($hwnd, $WM_COMMAND, [IntPtr]$IDM_LSP_SHOW_SYMBOL_INFO, [IntPtr]::Zero)

        if (($i % 5) -eq 0) {
            [void][Win32Interop]::PostMessage($hwnd, $WM_COMMAND, [IntPtr]$IDM_REFACTOR_SHOW_ALL, [IntPtr]::Zero)
        }

        if (($i % 8) -eq 0) {
            # Exercises model command path (default is safe list/refresh lane).
            [void][Win32Interop]::PostMessage($hwnd, $WM_COMMAND, [IntPtr]$ModelCommandId, [IntPtr]::Zero)
        }

        Start-Sleep -Milliseconds 120
    }

    Write-Host "[stress] Settling for $PostSettleSec second(s)"
    Start-Sleep -Seconds $PostSettleSec

    $proc.Refresh()
    if ($proc.HasExited) {
        if ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) {
            Write-Host "[stress] PASS: process exited with non-crash code $($proc.ExitCode) under load"
            return
        }

        throw "Win32IDE crashed/exited during stress loop (exit code $($proc.ExitCode))"
    }

    Write-Host "[stress] PASS: process remained alive under concurrent command pressure"
}
finally {
    if ($null -ne $proc) {
        $proc.Refresh()
        if (-not $proc.HasExited) {
            [void]$proc.CloseMainWindow()
            if (-not $proc.WaitForExit(5000)) {
                $proc.Kill()
                [void]$proc.WaitForExit(3000)
            }
        }
    }
}
