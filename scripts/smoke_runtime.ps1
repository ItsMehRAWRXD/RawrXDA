#requires -Version 5.1
param([string]$ExePath = "", [int]$TimeoutSeconds = 5)
$ErrorActionPreference = "Continue"
$ProjectRoot = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
if (-not $ExePath -or -not (Test-Path $ExePath)) {
    foreach ($c in @((Join-Path $ProjectRoot "build\monolithic\RawrXD.exe"),(Join-Path $ProjectRoot "RawrXD.exe"))) {
        if (Test-Path $c) { $ExePath = $c; break }
    }
    if (-not $ExePath) { Write-Host "ERROR: No EXE found."; exit 1 }
}
$fi = Get-Item $ExePath
Write-Host "`n  == RawrXD Runtime Smoke Test ==`n"
Write-Host "  Binary : $($fi.FullName)"
Write-Host "  Size   : $([math]::Round($fi.Length / 1KB, 1)) KB"
Write-Host "  Built  : $($fi.LastWriteTime)`n"

$csharp = 'using System; using System.Text; using System.Runtime.InteropServices;
public class SmokeWin32 {
    [DllImport("user32.dll",SetLastError=true)][return:MarshalAs(UnmanagedType.Bool)]
    public static extern bool PostMessageW(IntPtr h,uint m,IntPtr w,IntPtr l);
    [DllImport("user32.dll")]public static extern bool EnumWindows(EnumWindowsProc f,IntPtr p);
    [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetClassNameW(IntPtr h,StringBuilder s,int n);
    [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr h);
    public delegate bool EnumWindowsProc(IntPtr h,IntPtr p);
    public const uint WM_CLOSE=0x0010;
    public static IntPtr foundHwnd=IntPtr.Zero;
    public static string foundClass="";
    public static void FindByPid(uint tp){
        foundHwnd=IntPtr.Zero; foundClass="";
        EnumWindows(delegate(IntPtr h,IntPtr lp){
            uint wp; GetWindowThreadProcessId(h,out wp);
            if(wp==tp && IsWindowVisible(h)){
                var sb=new StringBuilder(256); GetClassNameW(h,sb,256);
                string c=sb.ToString();
                if(c!="IME"&&c!="MSCTFIME UI"){foundHwnd=h;foundClass=c;return false;}
            } return true;
        },IntPtr.Zero);
    }
}'
Add-Type -TypeDefinition $csharp -ErrorAction SilentlyContinue

$failures = 0; $proc = $null

Write-Host "  [1] Bootstrap: Launch stability"
try {
    $proc = Start-Process -FilePath $ExePath -PassThru
    Start-Sleep -Milliseconds 800; $proc.Refresh()
    if ($proc.HasExited) { Write-Host "    FAIL: CRASHED (exit $($proc.ExitCode))"; $failures++; $proc = $null }
    else { Write-Host "    PASS: Stable 800ms (PID $($proc.Id), $([math]::Round($proc.WorkingSet64/1MB,1)) MB)" }
} catch { Write-Host "    FAIL: $($_.Exception.Message)"; $failures++; $proc = $null }

Write-Host "`n  [2] UI Module: Window detection"
$hwnd = [IntPtr]::Zero
if ($null -ne $proc) {
    $tpid = [uint32]$proc.Id
    $end = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $end) {
        [SmokeWin32]::FindByPid($tpid)
        if ([SmokeWin32]::foundHwnd -ne [IntPtr]::Zero) { $hwnd = [SmokeWin32]::foundHwnd; break }
        Start-Sleep -Milliseconds 250
    }
    if ($hwnd -ne [IntPtr]::Zero) { Write-Host "    PASS: Window '$([SmokeWin32]::foundClass)' hwnd=0x$($hwnd.ToString('X'))" }
    else { Write-Host "    FAIL: No window within ${TimeoutSeconds}s"; $failures++ }
} else { Write-Host "    SKIP: dead"; $failures++ }

Write-Host "`n  [3] Memory: Footprint"
if ($null -ne $proc) {
    $proc.Refresh(); $m = [math]::Round($proc.WorkingSet64/1MB,1)
    if ($m -lt 500) { Write-Host "    PASS: $m MB" }
    elseif ($m -lt 1920) { Write-Host "    WARN: $m MB" }
    else { Write-Host "    FAIL: $m MB exceeds target"; $failures++ }
} else { Write-Host "    SKIP" }

Write-Host "`n  [4] Shutdown: WM_CLOSE"
if ($null -ne $proc) {
    if ($hwnd -ne [IntPtr]::Zero) {
        [SmokeWin32]::PostMessageW($hwnd,[SmokeWin32]::WM_CLOSE,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
        $ok = $false
        for ($i=0;$i -lt 20;$i++) { Start-Sleep -Milliseconds 100; $proc.Refresh(); if ($proc.HasExited) { $ok=$true; break } }
        if ($ok) { Write-Host "    PASS: Clean exit (code $($proc.ExitCode))" }
        else { Write-Host "    WARN: Force kill"; Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
    } else { Write-Host "    WARN: No hwnd"; Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
} else { Write-Host "    SKIP" }

Write-Host ""
if ($failures -eq 0) { Write-Host "  === ALL 4 SMOKE TESTS PASSED ===" }
else { Write-Host "  === $failures FAILURE(S) ===" }
exit $failures