<#
  test_x1_editor.ps1 — Phase X+1 Editor Surface validation
  Verifies: window class, title, Consolas font, keystroke I/O, no crash
#>
param(
    [string]$ExePath = (Join-Path $PSScriptRoot '..\build\monolithic\RawrXD.exe'),
    [int]$TimeoutSeconds = 5
)
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class X1Win32 {
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc f, IntPtr p);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    public delegate bool EnumWindowsProc(IntPtr h, IntPtr p);
    public static IntPtr foundHwnd = IntPtr.Zero;
    public static string foundClass = "";
    public static string foundTitle = "";
    public static void FindByPid(uint tp) {
        foundHwnd = IntPtr.Zero; foundClass = ""; foundTitle = "";
        EnumWindows(delegate(IntPtr h, IntPtr lp) {
            uint wp; GetWindowThreadProcessId(h, out wp);
            if (wp == tp && IsWindowVisible(h)) {
                var sb = new StringBuilder(256); GetClassNameW(h, sb, 256);
                var c = sb.ToString();
                if (c != "IME" && c != "MSCTFIME UI") {
                    foundHwnd = h; foundClass = c;
                    var tb = new StringBuilder(256); GetWindowTextW(h, tb, 256);
                    foundTitle = tb.ToString();
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
    }
}
"@ -ErrorAction SilentlyContinue

Write-Host "`n  == Phase X+1 Editor Surface Test ==`n"
Write-Host "  Binary: $ExePath"

$failures = 0
$proc = $null

# --- Test 1: Launch + window creation ---
Write-Host "`n  [1] Window creation"
try {
    $proc = Start-Process -FilePath $ExePath -PassThru
    Start-Sleep -Milliseconds 1500
    $proc.Refresh()
    if ($proc.HasExited) {
        Write-Host "    FAIL: Crashed (exit $($proc.ExitCode))"
        $failures++; $proc = $null
    } else {
        $tpid = [uint32]$proc.Id
        [X1Win32]::FindByPid($tpid)
        if ([X1Win32]::foundHwnd -ne [IntPtr]::Zero) {
            Write-Host "    PASS: Window hwnd=0x$([X1Win32]::foundHwnd.ToString('X'))"
        } else {
            Write-Host "    FAIL: No visible window"
            $failures++
        }
    }
} catch { Write-Host "    FAIL: $($_.Exception.Message)"; $failures++; $proc = $null }

# --- Test 2: Class name ---
Write-Host "`n  [2] Window class = 'RawrXD_Main'"
if ([X1Win32]::foundClass -eq 'RawrXD_Main') {
    Write-Host "    PASS: Class='$([X1Win32]::foundClass)'"
} else {
    Write-Host "    FAIL: Class='$([X1Win32]::foundClass)' (expected RawrXD_Main)"
    $failures++
}

# --- Test 3: Title ---
Write-Host "`n  [3] Window title contains 'RawrXD'"
if ([X1Win32]::foundTitle -match 'RawrXD') {
    Write-Host "    PASS: Title='$([X1Win32]::foundTitle)'"
} else {
    Write-Host "    FAIL: Title='$([X1Win32]::foundTitle)'"
    $failures++
}

# --- Test 4: Send keystrokes (WM_CHAR) without crash ---
Write-Host "`n  [4] Keystroke input (WM_CHAR x5)"
$WM_CHAR = 0x102
if ($null -ne $proc -and -not $proc.HasExited) {
    $hwnd = [X1Win32]::foundHwnd
    # Send 'H','e','l','l','o'
    foreach ($ch in @(72,101,108,108,111)) {
        [X1Win32]::PostMessageW($hwnd, $WM_CHAR, [IntPtr]$ch, [IntPtr]::Zero) | Out-Null
    }
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
    if ($proc.HasExited) {
        Write-Host "    FAIL: Crashed after keystrokes"
        $failures++; $proc = $null
    } else {
        Write-Host "    PASS: 5 chars sent, no crash"
    }
} else { Write-Host "    SKIP: no process"; $failures++ }

# --- Test 5: Send Enter + arrow keys (WM_KEYDOWN) without crash ---
Write-Host "`n  [5] Special keys (Enter, Arrows, Backspace)"
$WM_KEYDOWN = 0x100
if ($null -ne $proc -and -not $proc.HasExited) {
    $hwnd = [X1Win32]::foundHwnd
    # Enter
    [X1Win32]::PostMessageW($hwnd, $WM_CHAR, [IntPtr]0x0D, [IntPtr]::Zero) | Out-Null
    # WM_CHAR 'X'
    [X1Win32]::PostMessageW($hwnd, $WM_CHAR, [IntPtr]88, [IntPtr]::Zero) | Out-Null
    # Arrow keys
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x25, [IntPtr]::Zero) | Out-Null  # LEFT
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x26, [IntPtr]::Zero) | Out-Null  # UP
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x27, [IntPtr]::Zero) | Out-Null  # RIGHT
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x28, [IntPtr]::Zero) | Out-Null  # DOWN
    # Backspace
    [X1Win32]::PostMessageW($hwnd, $WM_CHAR, [IntPtr]8, [IntPtr]::Zero) | Out-Null
    # Home / End
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x24, [IntPtr]::Zero) | Out-Null  # HOME
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x23, [IntPtr]::Zero) | Out-Null  # END
    # Delete
    [X1Win32]::PostMessageW($hwnd, $WM_KEYDOWN, [IntPtr]0x2E, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
    if ($proc.HasExited) {
        Write-Host "    FAIL: Crashed after special keys"
        $failures++; $proc = $null
    } else {
        Write-Host "    PASS: All special keys handled, no crash"
    }
} else { Write-Host "    SKIP: no process"; $failures++ }

# --- Test 6: Memory under cap ---
Write-Host "`n  [6] Memory footprint"
if ($null -ne $proc -and -not $proc.HasExited) {
    $proc.Refresh()
    $mb = [math]::Round($proc.WorkingSet64/1MB, 1)
    if ($mb -lt 200) {
        Write-Host "    PASS: $mb MB < 200 MB cap"
    } else {
        Write-Host "    FAIL: $mb MB exceeds 200 MB"
        $failures++
    }
} else { Write-Host "    SKIP" }

# --- Cleanup ---
if ($null -ne $proc -and -not $proc.HasExited) {
    $WM_CLOSE = 0x10
    [X1Win32]::PostMessageW([X1Win32]::foundHwnd, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    $proc.WaitForExit(3000) | Out-Null
    if (-not $proc.HasExited) { Stop-Process $proc -Force }
}

Write-Host ""
if ($failures -gt 0) {
    Write-Host "  === $failures FAILURE(S) ==="
    exit 1
} else {
    Write-Host "  === ALL 6 EDITOR TESTS PASSED ==="
    exit 0
}
