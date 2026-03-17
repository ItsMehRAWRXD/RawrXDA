# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║ RAWRXD Sovereign Injection Script v1.2.0                                     ║
# ║ Purpose: Hot-inject thermal_dashboard.dll into running RawrXD IDE            ║
# ║ Author: RawrXD Team                                                          ║
# ║ License: Sovereign Tier - Internal Use Only                                  ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

<#
.SYNOPSIS
    Injects the thermal_dashboard.dll into the running RawrXD IDE process.

.DESCRIPTION
    This script uses Windows API to inject the thermal dashboard DLL into the
    IDE process, enabling real-time thermal monitoring without IDE restart.
    
    Supports two injection methods:
    1. Qt Plugin Loader (preferred) - Uses IPC to signal IDE to load plugin
    2. Remote Thread (fallback) - Direct DLL injection via CreateRemoteThread

.PARAMETER DllPath
    Path to the thermal_dashboard.dll file.

.PARAMETER IdeProcessName
    Name of the IDE process (default: RawrXD-IDE.exe or BigDaddyG-IDE.exe)

.PARAMETER Method
    Injection method: "plugin" (Qt-based) or "remote" (CreateRemoteThread)

.EXAMPLE
    .\Sovereign_Injection_Script.ps1
    .\Sovereign_Injection_Script.ps1 -Method plugin
    .\Sovereign_Injection_Script.ps1 -DllPath "C:\custom\path\thermal_dashboard.dll"
#>

[CmdletBinding()]
param (
    [Parameter(Position = 0)]
    [string]$DllPath = "D:\rawrxd\build\bin\thermal_dashboard.dll",
    
    [Parameter(Position = 1)]
    [string[]]$IdeProcessNames = @("RawrXD-IDE", "BigDaddyG-IDE", "rawrxd", "RawrXD"),
    
    [Parameter(Position = 2)]
    [ValidateSet("plugin", "remote", "auto")]
    [string]$Method = "auto"
)

# ═══════════════════════════════════════════════════════════════════════════════
# Configuration
# ═══════════════════════════════════════════════════════════════════════════════

$Script:CONFIG = @{
    PipeNameTemplate = "\\.\pipe\RawrXD_PluginLoader_{0}"
    IpcTimeout       = 5000  # ms
    RetryCount       = 3
    RetryDelay       = 1000  # ms
}

# ═══════════════════════════════════════════════════════════════════════════════
# Banner
# ═══════════════════════════════════════════════════════════════════════════════

function Show-Banner {
    Write-Host ""
    Write-Host "  ╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "  ║  🔥 RAWRXD SOVEREIGN THERMAL INJECTION v1.2.0 🔥          ║" -ForegroundColor Cyan
    Write-Host "  ║     Hot-injectable Thermal Dashboard for BigDaddyG        ║" -ForegroundColor DarkCyan
    Write-Host "  ╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

# ═══════════════════════════════════════════════════════════════════════════════
# Process Discovery
# ═══════════════════════════════════════════════════════════════════════════════

function Find-IdeProcess {
    param([string[]]$ProcessNames)
    
    Write-Host "🔍 Searching for IDE process..." -ForegroundColor Yellow
    
    foreach ($name in $ProcessNames) {
        $proc = Get-Process -Name $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($proc) {
            Write-Host "  ✅ Found: $($proc.ProcessName) (PID: $($proc.Id))" -ForegroundColor Green
            return $proc
        }
    }
    
    # Fallback: search by window title
    $procs = Get-Process | Where-Object { $_.MainWindowTitle -like "*RawrXD*" -or $_.MainWindowTitle -like "*BigDaddyG*" }
    if ($procs) {
        $proc = $procs | Select-Object -First 1
        Write-Host "  ✅ Found by window title: $($proc.ProcessName) (PID: $($proc.Id))" -ForegroundColor Green
        return $proc
    }
    
    return $null
}

# ═══════════════════════════════════════════════════════════════════════════════
# Method 1: Qt Plugin Loader via Named Pipe IPC
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-PluginInjection {
    param(
        [string]$DllPath,
        [int]$ProcessId
    )
    
    Write-Host "📡 Attempting Qt Plugin Loader injection via IPC..." -ForegroundColor Cyan
    
    $pipeName = $Script:CONFIG.PipeNameTemplate -f $ProcessId
    
    try {
        # Check if pipe exists
        if (-not (Test-Path $pipeName)) {
            Write-Host "  ⚠️  Named pipe not found. IDE may not have plugin loader active." -ForegroundColor Yellow
            return $false
        }
        
        # Connect to named pipe
        $pipeClient = New-Object System.IO.Pipes.NamedPipeClientStream(".", "RawrXD_PluginLoader_$ProcessId", [System.IO.Pipes.PipeDirection]::InOut)
        $pipeClient.Connect($Script:CONFIG.IpcTimeout)
        
        $writer = New-Object System.IO.StreamWriter($pipeClient)
        $reader = New-Object System.IO.StreamReader($pipeClient)
        
        # Send load command
        $command = @{
            action = "LOAD_PLUGIN"
            path   = $DllPath
            type   = "thermal_dashboard"
        } | ConvertTo-Json -Compress
        
        $writer.WriteLine($command)
        $writer.Flush()
        
        # Wait for response
        $response = $reader.ReadLine()
        $result = $response | ConvertFrom-Json
        
        $pipeClient.Close()
        
        if ($result.success) {
            Write-Host "  ✅ Plugin loaded successfully via IPC!" -ForegroundColor Green
            return $true
        } else {
            Write-Host "  ❌ Plugin load failed: $($result.error)" -ForegroundColor Red
            return $false
        }
    }
    catch {
        Write-Host "  ⚠️  IPC injection failed: $_" -ForegroundColor Yellow
        return $false
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# Method 2: Remote Thread DLL Injection (Fallback)
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-RemoteThreadInjection {
    param(
        [string]$DllPath,
        [int]$ProcessId
    )
    
    Write-Host "💉 Attempting Remote Thread injection..." -ForegroundColor Cyan
    
    $injectorCode = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class SovereignInjector {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress,
        uint dwSize, uint flAllocationType, uint flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress,
        byte[] buffer, uint size, out UIntPtr written);

    [DllImport("kernel32.dll")]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes,
        uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool VirtualFreeEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint dwFreeType);

    private const uint PROCESS_ALL_ACCESS = 0x1F0FFF;
    private const uint MEM_COMMIT = 0x1000;
    private const uint MEM_RESERVE = 0x2000;
    private const uint MEM_RELEASE = 0x8000;
    private const uint PAGE_READWRITE = 0x04;
    private const uint INFINITE = 0xFFFFFFFF;

    public static bool Inject(string dllPath, int pid) {
        IntPtr hProcess = IntPtr.Zero;
        IntPtr remoteAddr = IntPtr.Zero;
        IntPtr hThread = IntPtr.Zero;

        try {
            // Open target process
            hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
            if (hProcess == IntPtr.Zero) {
                Console.WriteLine("[-] OpenProcess failed: " + Marshal.GetLastWin32Error());
                return false;
            }
            Console.WriteLine("[+] Process opened successfully");

            // Allocate memory in target process
            byte[] dllPathBytes = Encoding.Unicode.GetBytes(dllPath + "\0");
            uint allocSize = (uint)dllPathBytes.Length;
            
            remoteAddr = VirtualAllocEx(hProcess, IntPtr.Zero, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remoteAddr == IntPtr.Zero) {
                Console.WriteLine("[-] VirtualAllocEx failed: " + Marshal.GetLastWin32Error());
                return false;
            }
            Console.WriteLine("[+] Memory allocated at: 0x" + remoteAddr.ToString("X"));

            // Write DLL path to target process
            UIntPtr bytesWritten;
            if (!WriteProcessMemory(hProcess, remoteAddr, dllPathBytes, allocSize, out bytesWritten)) {
                Console.WriteLine("[-] WriteProcessMemory failed: " + Marshal.GetLastWin32Error());
                return false;
            }
            Console.WriteLine("[+] DLL path written to target process");

            // Get LoadLibraryW address
            IntPtr kernel32 = GetModuleHandle("kernel32.dll");
            IntPtr loadLibAddr = GetProcAddress(kernel32, "LoadLibraryW");
            if (loadLibAddr == IntPtr.Zero) {
                Console.WriteLine("[-] GetProcAddress(LoadLibraryW) failed");
                return false;
            }
            Console.WriteLine("[+] LoadLibraryW at: 0x" + loadLibAddr.ToString("X"));

            // Create remote thread
            hThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0, loadLibAddr, remoteAddr, 0, IntPtr.Zero);
            if (hThread == IntPtr.Zero) {
                Console.WriteLine("[-] CreateRemoteThread failed: " + Marshal.GetLastWin32Error());
                return false;
            }
            Console.WriteLine("[+] Remote thread created");

            // Wait for thread completion
            WaitForSingleObject(hThread, 10000);  // 10 second timeout
            Console.WriteLine("[+] Remote thread completed");

            return true;
        }
        finally {
            // Cleanup
            if (remoteAddr != IntPtr.Zero && hProcess != IntPtr.Zero) {
                VirtualFreeEx(hProcess, remoteAddr, 0, MEM_RELEASE);
            }
            if (hThread != IntPtr.Zero) CloseHandle(hThread);
            if (hProcess != IntPtr.Zero) CloseHandle(hProcess);
        }
    }
}
"@

    try {
        # Add the injector type
        Add-Type -TypeDefinition $injectorCode -Language CSharp -ErrorAction Stop
        
        # Perform injection
        $result = [SovereignInjector]::Inject($DllPath, $ProcessId)
        
        if ($result) {
            Write-Host "  ✅ Remote thread injection successful!" -ForegroundColor Green
            return $true
        } else {
            Write-Host "  ❌ Remote thread injection failed" -ForegroundColor Red
            return $false
        }
    }
    catch {
        Write-Host "  ❌ Injection error: $_" -ForegroundColor Red
        return $false
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# Main Injection Logic
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-ThermalDashboardInjection {
    param(
        [string]$DllPath,
        [string]$Method,
        [string[]]$ProcessNames
    )
    
    Show-Banner
    
    # Validate DLL exists
    if (-not (Test-Path $DllPath)) {
        Write-Host "❌ DLL not found: $DllPath" -ForegroundColor Red
        Write-Host "   Run the build first to generate thermal_dashboard.dll" -ForegroundColor Yellow
        return $false
    }
    
    $dllInfo = Get-Item $DllPath
    Write-Host "📦 DLL: $($dllInfo.Name) ($([math]::Round($dllInfo.Length / 1KB, 1)) KB)" -ForegroundColor Gray
    Write-Host "   Path: $DllPath" -ForegroundColor DarkGray
    Write-Host ""
    
    # Find IDE process
    $process = Find-IdeProcess -ProcessNames $ProcessNames
    if (-not $process) {
        Write-Host ""
        Write-Host "❌ No IDE process found. Please launch the RawrXD IDE first." -ForegroundColor Red
        Write-Host "   Looking for: $($ProcessNames -join ', ')" -ForegroundColor Yellow
        return $false
    }
    
    Write-Host ""
    
    # Determine injection method
    $success = $false
    
    switch ($Method) {
        "plugin" {
            $success = Invoke-PluginInjection -DllPath $DllPath -ProcessId $process.Id
        }
        "remote" {
            $success = Invoke-RemoteThreadInjection -DllPath $DllPath -ProcessId $process.Id
        }
        "auto" {
            # Try plugin method first, fall back to remote thread
            Write-Host "🔄 Auto mode: Trying Plugin Loader first..." -ForegroundColor Cyan
            $success = Invoke-PluginInjection -DllPath $DllPath -ProcessId $process.Id
            
            if (-not $success) {
                Write-Host ""
                Write-Host "🔄 Falling back to Remote Thread injection..." -ForegroundColor Yellow
                $success = Invoke-RemoteThreadInjection -DllPath $DllPath -ProcessId $process.Id
            }
        }
    }
    
    Write-Host ""
    
    if ($success) {
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Green
        Write-Host " ✅ THERMAL DASHBOARD INJECTION COMPLETE!" -ForegroundColor Green
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Green
        Write-Host ""
        Write-Host " 🌡️  The Sovereign Thermal HUD should now be visible in the IDE" -ForegroundColor Cyan
        Write-Host " 📊  Real-time NVMe/GPU/CPU monitoring active" -ForegroundColor Cyan
        Write-Host " 🔥  Burst mode controls available" -ForegroundColor Cyan
        Write-Host ""
    } else {
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Red
        Write-Host " ❌ INJECTION FAILED" -ForegroundColor Red
        Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Red
        Write-Host ""
        Write-Host " Possible causes:" -ForegroundColor Yellow
        Write-Host "   • IDE not running or wrong process name" -ForegroundColor Gray
        Write-Host "   • Antivirus blocking injection" -ForegroundColor Gray
        Write-Host "   • Insufficient privileges (try running as Admin)" -ForegroundColor Gray
        Write-Host "   • DLL architecture mismatch (32-bit vs 64-bit)" -ForegroundColor Gray
        Write-Host ""
    }
    
    return $success
}

# ═══════════════════════════════════════════════════════════════════════════════
# Entry Point
# ═══════════════════════════════════════════════════════════════════════════════

$result = Invoke-ThermalDashboardInjection -DllPath $DllPath -Method $Method -ProcessNames $IdeProcessNames
exit $(if ($result) { 0 } else { 1 })
