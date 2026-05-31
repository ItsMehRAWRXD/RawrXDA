[CmdletBinding()]
param(
    [string]$ExePath = "d:/rawrxd/build_ninja/bin/RawrXD-Win32IDE.exe",
    [int]$DurationSeconds = 600,
    [int]$IntervalMs = 20,
    [string]$OutDir = "d:/rawrxd/reports/14day",
    [string]$Tag = "win32_message_churn_soak"
)

$ErrorActionPreference = "Stop"

# Force a single-instance baseline before soak so early exits are attributable
# to churn pressure, not instance handoff behavior.
Get-Process RawrXD-Win32IDE -ErrorAction SilentlyContinue | ForEach-Object {
    try {
        Stop-Process -Id $_.Id -Force -ErrorAction Stop
    } catch {
    }
}

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

Add-Type -Language CSharp -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeWin32 {
    public delegate bool EnumChildProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumChildProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, UIntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, UIntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool IsWindow(IntPtr hWnd);
}
"@

$WM_SIZE = 0x0005
$WM_PAINT = 0x000F
$WM_VSCROLL = 0x0115
$WM_MOUSEMOVE = 0x0200
$WM_GETTEXTLENGTH = 0x000E
$EM_SETSEL = 0x00B1
$EM_SCROLLCARET = 0x00B7
$SB_LINEDOWN = 1

function Find-EditorHwnd {
    param([IntPtr]$Main)

    $editor = [IntPtr]::Zero
    $callback = [NativeWin32+EnumChildProc]{
        param([IntPtr]$hwnd, [IntPtr]$lParam)
        $sb = New-Object System.Text.StringBuilder 256
        [void][NativeWin32]::GetClassName($hwnd, $sb, $sb.Capacity)
        $cn = $sb.ToString()
        if ($cn -like "RichEdit*" -or $cn -like "RICHEDIT*" -or $cn -like "Scintilla*" -or $cn -like "Edit*") {
            $script:editor = $hwnd
            return $false
        }
        return $true
    }
    [void][NativeWin32]::EnumChildWindows($Main, $callback, [IntPtr]::Zero)
    return $script:editor
}

$startTime = Get-Date
$proc = Start-Process -FilePath $ExePath -PassThru

for ($i = 0; $i -lt 150; $i++) {
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne 0) { break }
    Start-Sleep -Milliseconds 200
}
$proc.Refresh()
if ($proc.MainWindowHandle -eq 0) {
    throw "Main window handle not available for process $($proc.Id)"
}

$main = [IntPtr]$proc.MainWindowHandle
$editor = Find-EditorHwnd -Main $main

$events = 0
$sendOps = 0
$postOps = 0
$iterations = 0
$crashed = $false
$exitCode = $null
$exitSecond = $null
$rng = [System.Random]::new()
$deadline = (Get-Date).AddSeconds($DurationSeconds)

while ((Get-Date) -lt $deadline) {
    if ($proc.HasExited) {
        $crashed = $true
        $exitCode = $proc.ExitCode
        $exitSecond = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        break
    }

    if (-not [NativeWin32]::IsWindow($main)) {
        $crashed = $true
        $proc.Refresh()
        if ($proc.HasExited) {
            $exitCode = $proc.ExitCode
            $exitSecond = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        }
        break
    }

    $w = $rng.Next(900, 1800)
    $h = $rng.Next(600, 1200)
    $lParam = [IntPtr][int](($h -band 0xFFFF) -shl 16 -bor ($w -band 0xFFFF))

    [void][NativeWin32]::PostMessage($main, $WM_SIZE, [UIntPtr]::Zero, $lParam)
    [void][NativeWin32]::PostMessage($main, $WM_PAINT, [UIntPtr]::Zero, [IntPtr]::Zero)
    [void][NativeWin32]::PostMessage($main, $WM_MOUSEMOVE, [UIntPtr]::Zero, [IntPtr][int]((($rng.Next(10,500) -band 0xFFFF) -shl 16) -bor ($rng.Next(10,900) -band 0xFFFF)))
    $postOps += 3

    if ($editor -eq [IntPtr]::Zero -or -not [NativeWin32]::IsWindow($editor)) {
        $editor = Find-EditorHwnd -Main $main
    }

    if ($editor -ne [IntPtr]::Zero -and [NativeWin32]::IsWindow($editor)) {
        $lenPtr = [NativeWin32]::SendMessage($editor, $WM_GETTEXTLENGTH, [UIntPtr]::Zero, [IntPtr]::Zero)
        $len = [Math]::Max(0, $lenPtr.ToInt32())
        $pos = 0
        if ($len -gt 0) {
            $pos = $rng.Next(0, $len)
        }
        [void][NativeWin32]::SendMessage($editor, $EM_SETSEL, [UIntPtr]::new([uint32]$pos), [IntPtr][int]$pos)
        [void][NativeWin32]::SendMessage($editor, $EM_SCROLLCARET, [UIntPtr]::Zero, [IntPtr]::Zero)
        [void][NativeWin32]::PostMessage($editor, $WM_VSCROLL, [UIntPtr]::new([uint32]$SB_LINEDOWN), [IntPtr]::Zero)
        $sendOps += 2
        $postOps += 1
    }

    $events += 1
    $iterations += 1
    Start-Sleep -Milliseconds $IntervalMs
}

$endTime = Get-Date
$runSeconds = [Math]::Round(($endTime - $startTime).TotalSeconds, 2)

$appErrs = @()
try {
    $appErrs = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $startTime.AddSeconds(-5) } -ErrorAction Stop |
        Where-Object { ($_.Id -in 1000, 1001) -and $_.Message -match 'RawrXD-Win32IDE|RawrXD' }
} catch {
}

$recursionWarnings = @()
$searchRoots = @("d:/rawrxd/logs", [System.IO.Path]::GetTempPath())
$patterns = @("EditorSubclassProc", "re-entr", "reentrant", "recurs", "SendMessage")
foreach ($root in $searchRoots) {
    if (-not (Test-Path $root)) { continue }
    $files = Get-ChildItem -Path $root -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $startTime -and $_.Extension -in '.log', '.txt' }
    foreach ($f in $files) {
        try {
            $hits = Select-String -Path $f.FullName -Pattern $patterns -SimpleMatch -ErrorAction SilentlyContinue
            if ($hits) { $recursionWarnings += $hits }
        } catch {
        }
    }
}

$status = "PASS"
if ($crashed -or $appErrs.Count -gt 0) {
    $status = "FAIL"
}

if (-not $proc.HasExited) {
    try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch {}
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outJson = Join-Path $OutDir ("{0}_{1}.json" -f $Tag, $stamp)

$summary = [pscustomobject]@{
    generatedAt = (Get-Date).ToString("o")
    status = $status
    durationSecondsRequested = $DurationSeconds
    durationSecondsObserved = $runSeconds
    processId = $proc.Id
    crashed = $crashed
    exitCode = $exitCode
    exitSecond = $exitSecond
    eventBursts = $events
    postOps = $postOps
    sendOps = $sendOps
    applicationErrors = @($appErrs | Select-Object TimeCreated, Id, ProviderName)
    recursionWarningHits = $recursionWarnings.Count
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $outJson -Encoding UTF8

Write-Host ("SOAK_STATUS={0} duration={1}s bursts={2} sendOps={3} postOps={4} appErrors={5} recursionHits={6}" -f $status, $runSeconds, $events, $sendOps, $postOps, $appErrs.Count, $recursionWarnings.Count)
Write-Host ("SOAK_REPORT=" + $outJson)
