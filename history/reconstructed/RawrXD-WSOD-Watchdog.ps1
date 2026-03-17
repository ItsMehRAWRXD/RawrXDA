# RawrXD-WSOD-Watchdog.ps1
# White Screen of Death Detection & Auto-Recovery System
# Integrates with IDE startup - logs everything, heals automatically

param(
    [string]$IDEName = "RawrXD-Win32IDE",
    [string]$BinaryPath = "D:\rawrxd\build\RawrXD-Win32IDE.exe",
    [string]$LogDir = "$env:LOCALAPPDATA\RawrXD\WSOD-Logs",
    [int]$CheckInterval = 500,  # ms
    [int]$WhiteThreshold = 95,   # % white pixels = WSOD
    [switch]$AutoHeal,
    [switch]$StartIDE
)

Add-Type -TypeDefinition @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.IO;
public class WSODWatcher {
    [DllImport("user32.dll")] static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr hWnd);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("gdi32.dll")] static extern uint GetPixel(IntPtr hdc, int nXPos, int nYPos);
    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr hWnd);
    [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")] static extern int GetWindowThreadProcessId(IntPtr hWnd, out int lpdwProcessId);
    
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    
    public static bool WindowExists() { return FindWindow(null, "RawrXD") != IntPtr.Zero || FindWindow("RawrXD_MainClass", null) != IntPtr.Zero; }
    
    public static WindowSnapshot CaptureWindow() {
        IntPtr hWnd = FindWindow(null, "RawrXD");
        if(hWnd == IntPtr.Zero) hWnd = FindWindow("RawrXD_MainClass", null);
        if(hWnd == IntPtr.Zero) return null;
        
        GetWindowRect(hWnd, out RECT rc);
        int width = rc.Right - rc.Left;
        int height = rc.Bottom - rc.Top;
        if(width < 100 || height < 100) return null;
        
        using(Bitmap bmp = new Bitmap(width, height)) {
            using(Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                PrintWindow(hWnd, hdc, 0);
                g.ReleaseHdc(hdc);
            }
            
            int whitePixels = 0;
            int totalPixels = width * height;
            int sampleStep = Math.Max(1, totalPixels / 1000); // Sample 1000 points
            
            for(int i = 0; i < totalPixels; i += sampleStep) {
                int x = (i % width);
                int y = (i / width);
                Color c = bmp.GetPixel(x, y);
                if(c.R > 240 && c.G > 240 && c.B > 240) whitePixels++;
            }
            
            return new WindowSnapshot {
                WhitePercentage = (double)whitePixels / (totalPixels / sampleStep) * 100,
                Width = width,
                Height = height,
                Timestamp = DateTime.Now,
                IsDead = (double)whitePixels / (totalPixels / sampleStep) * 100 > 95,
                Handle = hWnd
            };
        }
    }
    
    public static Process GetIDEProcess() {
        IntPtr hWnd = FindWindow(null, "RawrXD");
        if(hWnd == IntPtr.Zero) return null;
        GetWindowThreadProcessId(hWnd, out int pid);
        try { return Process.GetProcessById(pid); } catch { return null; }
    }
    
    public class WindowSnapshot {
        public double WhitePercentage;
        public int Width, Height;
        public DateTime Timestamp;
        public bool IsDead;
        public IntPtr Handle;
    }
}
"@ -ReferencedAssemblies System.Drawing

# Initialize Logging
$null = mkdir $LogDir -Force 2>$null
$SessionID = [Guid]::NewGuid().ToString().Substring(0,8)
$StartTime = Get-Date

function Write-WSODLog {
    param($Level, $Message, $Data)
    $entry = @{
        SessionID = $SessionID
        Timestamp = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fff")
        Level = $Level
        Message = $Message
        Data = $Data
        ProcessID = $PID
    } | ConvertTo-Json -Compress
    
    $entry | Out-File "$LogDir\wsod_log_$(Get-Date -Format 'yyyyMMdd').ndjson" -Append
    Write-Host "[$Level] $Message" -ForegroundColor $(if($Level -eq "ERROR"){"Red"}elseif($Level -eq "WARN"){"Yellow"}else{"Green"})
}

function Get-GDIDiagnostics {
    $proc = [WSODWatcher]::GetIDEProcess()
    if(!$proc) { return @{ Status = "NotRunning" } }
    
    return @{
        Status = "Running"
        ProcessID = $proc.Id
        WorkingSetMB = [math]::Round($proc.WorkingSet64 / 1MB, 2)
        GDI_Objects = $proc.HandleCount
        Threads = $proc.Threads.Count
        Uptime = (Get-Date) - $proc.StartTime
        CPU_Sec = $proc.TotalProcessorTime.TotalSeconds
    }
}

function Invoke-Screenshot {
    param($Prefix)
    Add-Type -AssemblyName System.Windows.Forms
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap($screen.Width, $screen.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($screen.Location, [System.Drawing.Point]::Empty, $screen.Size)
    $path = "$LogDir\${Prefix}_$(Get-Date -Format 'HHmmss').png"
    $bitmap.Save($path)
    $graphics.Dispose()
    $bitmap.Dispose()
    return $path
}

function Invoke-EmergencyHeal {
    Write-WSODLog "HEAL" "Initiating auto-heal sequence"
    
    # Kill existing
    Get-Process | Where-Object {$_.ProcessName -like "*RawrXD*"} | Stop-Process -Force
    
    Start-Sleep 1
    
    # Clear shader caches (D2D corruption)
    Remove-Item "$env:LOCALAPPDATA\Microsoft\Windows\INetCache\*.d2d" -Force 2>$null
    
    # Restart with validation flags
    $proc = Start-Process $BinaryPath -ArgumentList "--validate-gpu","--software-render" -PassThru
    
    Write-WSODLog "HEAL" "Restarted with software rendering fallback" @{ PID = $proc.Id }
    
    # Wait and verify
    Start-Sleep 3
    return (Test-WSOD)
}

function Test-WSOD {
    $snapshot = [WSODWatcher]::CaptureWindow()
    if(!$snapshot) {
        Write-WSODLog "WARN" "Window not found for capture"
        return @{ IsDead = $true; Reason = "NoWindow" }
    }
    
    $diag = Get-GDIDiagnostics
    
    $result = @{
        IsDead = $snapshot.IsDead
        WhitePercent = [math]::Round($snapshot.WhitePercentage, 2)
        Resolution = "$($snapshot.Width)x$($snapshot.Height)"
        Diagnostics = $diag
        Threshold = $WhiteThreshold
    }
    
    if($snapshot.IsDead) {
        Write-WSODLog "ERROR" "WSOD DETECTED" $result
        Invoke-Screenshot "WSOD_DETECTED"
    } else {
        Write-WSODLog "INFO" "Screen healthy ($([math]::Round($snapshot.WhitePercentage,1))% white)" @{ GDI = $diag.GDI_Objects }
    }
    
    return $result
}

# MAIN WATCHDOG LOOP
Write-WSODLog "INFO" "WSOD Watchdog initialized" @{ Binary = $BinaryPath; Threshold = $WhiteThreshold }

if($StartIDE -and !(Test-Path $BinaryPath)) {
    Write-WSODLog "ERROR" "Binary not found: $BinaryPath"
    exit 1
}

if($StartIDE) {
    Write-WSODLog "INFO" "Starting IDE"
    Start-Process $BinaryPath
    Start-Sleep 2
}

# Startup validation
$bootCheck = Test-WSOD
if($bootCheck.IsDead -and $AutoHeal) {
    Write-WSODLog "WARN" "WSOD on startup, attempting heal"
    $healResult = Invoke-EmergencyHeal
    if($healResult.IsDead) {
        Write-WSODLog "FATAL" "Auto-heal failed, manual intervention required"
        exit 1
    }
}

# Continuous monitoring
$consecutiveDead = 0
while($true) {
    Start-Sleep -Milliseconds $CheckInterval
    
    $check = Test-WSOD
    
    if($check.IsDead) {
        $consecutiveDead++
        if($consecutiveDead -eq 3) {
            Write-WSODLog "ERROR" "WSOD persisted for 3 checks"
            Invoke-Screenshot "WSOD_PERSISTENT"
            
            if($AutoHeal) {
                $heal = Invoke-EmergencyHeal
                if($heal.IsDead) {
                    Write-WSODLog "FATAL" "Healing failed, entering manual mode"
                    Start-Process notepad "$LogDir\wsod_log_$(Get-Date -Format 'yyyyMMdd').ndjson"
                    break
                } else {
                    $consecutiveDead = 0
                }
            }
        }
    } else {
        if($consecutiveDead -gt 0) {
            Write-WSODLog "RECOVER" "Screen recovered after $consecutiveDead dead checks"
        }
        $consecutiveDead = 0
    }
    
    # Memory leak detection (GDI leak causes white screen)
    if($check.Diagnostics.GDI_Objects -gt 10000) {
        Write-WSODLog "WARN" "GDI Object leak detected" @{ GDI = $check.Diagnostics.GDI_Objects }
        if($AutoHeal) { Invoke-EmergencyHeal }
    }
}
