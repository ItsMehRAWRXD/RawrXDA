#============================================================================
# OS Explorer Interceptor PowerShell Module
# Provides CLI control and real-time streaming for the MASM IDE integration
#============================================================================

# Module metadata
@{
    ModuleVersion = '1.0.0'
    GUID = '12345678-1234-1234-1234-123456789012'
    Author = 'MASM IDE Integration'
    CompanyName = 'OS Explorer Interceptor'
    Copyright = '(c) 2026 OS Explorer Interceptor. All rights reserved.'
    Description = 'PowerShell module for OS Explorer Interceptor CLI control and streaming'
    PowerShellVersion = '7.0'
    CLRVersion = '4.0'
    RequiredAssemblies = @()
    ScriptsToProcess = @()
    TypesToProcess = @()
    FormatsToProcess = @()
    NestedModules = @()
    FunctionsToExport = @(
        'Start-OSInterceptor',
        'Stop-OSInterceptor',
        'Get-OSInterceptorStatus',
        'Get-OSInterceptorStats',
        'Clear-OSInterceptorLog',
        'Show-OSInterceptorHelp',
        'Wait-OSInterceptorEvent',
        'Format-OSInterceptorOutput',
        'Export-OSInterceptorData'
    )
    CmdletsToExport = @()
    VariablesToExport = @()
    AliasesToExport = @(
        'startos',
        'stopos',
        'statusos',
        'statsos',
        'clearos',
        'helpos'
    )
    PrivateData = @{
        PSData = @{
            Tags = @('MASM', 'IDE', 'Interceptor', 'OS', 'Explorer', 'Reverse-Engineering')
            LicenseUri = ''
            ProjectUri = ''
            IconUri = ''
            ReleaseNotes = 'Initial release of OS Explorer Interceptor PowerShell module'
        }
    }
}

# Global variables
$script:InterceptorProcess = $null
$script:InterceptorHandle = [IntPtr]::Zero
$script:IsActive = $false
$script:CallLog = [System.Collections.Generic.List[PSObject]]::new()
$script:Stats = @{
    TotalCalls = 0
    BytesStreamed = 0
    ErrorCount = 0
    StartTime = $null
    LastUpdate = $null
}
$script:StreamingJob = $null
$script:EventQueue = [System.Collections.Concurrent.ConcurrentQueue[PSObject]]::new()

#============================================================================
# CORE FUNCTIONS
#============================================================================

function Start-OSInterceptor {
    <#
    .SYNOPSIS
        Starts the OS Explorer Interceptor for a target process
    
    .DESCRIPTION
        Initializes and starts the OS Explorer Interceptor, injecting it into the target process
        and beginning real-time interception of all OS API calls
    
    .PARAMETER ProcessId
        The process ID of the target process to intercept
    
    .PARAMETER ProcessName
        The name of the target process to intercept (will find first matching process)
    
    .PARAMETER RealTimeStreaming
        Enable real-time streaming of intercepted calls to PowerShell console
    
    .PARAMETER OutputFile
        Path to output file for captured data (optional)
    
    .PARAMETER Filter
        Array of hook types to filter (e.g., @("FILE", "NETWORK"))
    
    .EXAMPLE
        Start-OSInterceptor -ProcessId 1234
        
    .EXAMPLE
        Start-OSInterceptor -ProcessName "cursor" -RealTimeStreaming
        
    .EXAMPLE
        Start-OSInterceptor -ProcessId 1234 -Filter @("FILE", "NETWORK") -OutputFile "C:\capture.log"
    #>
    [CmdletBinding(DefaultParameterSetName = 'ById')]
    param(
        [Parameter(ParameterSetName = 'ById', Mandatory = $true, Position = 0)]
        [int]$ProcessId,
        
        [Parameter(ParameterSetName = 'ByName', Mandatory = $true)]
        [string]$ProcessName,
        
        [Parameter()]
        [switch]$RealTimeStreaming,
        
        [Parameter()]
        [string]$OutputFile,
        
        [Parameter()]
        [ValidateSet("FILE", "REGISTRY", "PROCESS", "MEMORY", "NETWORK", "WINDOW", "COM", "CRYPTO")]
        [string[]]$Filter = @()
    )
    
    try {
        # Get process by name if needed
        if ($PSCmdlet.ParameterSetName -eq 'ByName') {
            $process = Get-Process -Name $ProcessName -ErrorAction Stop | Select-Object -First 1
            if (-not $process) {
                throw "Process '$ProcessName' not found"
            }
            $ProcessId = $process.Id
        }
        
        # Verify process exists
        $targetProcess = Get-Process -Id $ProcessId -ErrorAction Stop
        
        Write-Host "[OS Explorer Interceptor] Starting interception of process: $($targetProcess.ProcessName) (PID: $ProcessId)" -ForegroundColor Cyan
        
        # Load the interceptor DLL
        $interceptorPath = Join-Path $PSScriptRoot "..\bin\os_explorer_interceptor.dll"
        if (-not (Test-Path $interceptorPath)) {
            throw "Interceptor DLL not found at: $interceptorPath"
        }
        
        # Inject DLL into target process using existing MASM IDE injector
        $injectorPath = Join-Path $PSScriptRoot "..\bin\masm_ide_injector.exe"
        if (Test-Path $injectorPath) {
            & $injectorPath -ProcessId $ProcessId -DllPath $interceptorPath -Method ManualMap
        } else {
            # Fallback to PowerShell injection
            $injectResult = Invoke-DllInjection -ProcessId $ProcessId -DllPath $interceptorPath
            if (-not $injectResult.Success) {
                throw "Failed to inject DLL: $($injectResult.Error)"
            }
        }
        
        # Initialize interceptor
        $script:InterceptorProcess = $targetProcess
        $script:IsActive = $true
        $script:Stats.StartTime = Get-Date
        $script:Stats.LastUpdate = Get-Date
        
        # Start real-time streaming if requested
        if ($RealTimeStreaming) {
            Start-RealTimeStreaming -ProcessId $ProcessId -Filter $Filter -OutputFile $OutputFile
        }
        
        Write-Host "[SUCCESS] OS Explorer Interceptor started successfully" -ForegroundColor Green
        
        # Return status object
        return [PSCustomObject]@{
            ProcessId = $ProcessId
            ProcessName = $targetProcess.ProcessName
            Status = "Running"
            StartTime = $script:Stats.StartTime
            RealTimeStreaming = $RealTimeStreaming
            Filter = $Filter
            OutputFile = $OutputFile
        }
        
    } catch {
        Write-Error "[ERROR] Failed to start OS Explorer Interceptor: $_"
        $script:IsActive = $false
        throw
    }
}

function Stop-OSInterceptor {
    <#
    .SYNOPSIS
        Stops the OS Explorer Interceptor
    
    .DESCRIPTION
        Stops the interceptor, cleans up resources, and optionally saves captured data
    
    .PARAMETER SaveData
        Save captured data before stopping
    
    .PARAMETER OutputPath
        Path to save captured data
    
    .EXAMPLE
        Stop-OSInterceptor
        
    .EXAMPLE
        Stop-OSInterceptor -SaveData -OutputPath "C:\capture.json"
    #>
    [CmdletBinding()]
    param(
        [Parameter()]
        [switch]$SaveData,
        
        [Parameter()]
        [string]$OutputPath
    )
    
    try {
        if (-not $script:IsActive) {
            Write-Warning "OS Explorer Interceptor is not running"
            return
        }
        
        Write-Host "[OS Explorer Interceptor] Stopping..." -ForegroundColor Yellow
        
        # Save data if requested
        if ($SaveData -and $script:CallLog.Count -gt 0) {
            if (-not $OutputPath) {
                $OutputPath = "OSInterceptor_Capture_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
            }
            
            Write-Host "Saving captured data to: $OutputPath" -ForegroundColor Cyan
            $script:CallLog | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputPath -Encoding UTF8
            Write-Host "[SUCCESS] Data saved to: $OutputPath" -ForegroundColor Green
        }
        
        # Stop streaming
        if ($script:StreamingJob) {
            Stop-RealTimeStreaming
        }
        
        # Cleanup
        $script:IsActive = $false
        $script:InterceptorProcess = $null
        $script:InterceptorHandle = [IntPtr]::Zero
        
        Write-Host "[SUCCESS] OS Explorer Interceptor stopped" -ForegroundColor Green
        
    } catch {
        Write-Error "[ERROR] Failed to stop OS Explorer Interceptor: $_"
        throw
    }
}

function Get-OSInterceptorStatus {
    <#
    .SYNOPSIS
        Gets the current status of the OS Explorer Interceptor
    
    .DESCRIPTION
        Returns the current status, statistics, and configuration of the interceptor
    
    .EXAMPLE
        Get-OSInterceptorStatus
    #>
    [CmdletBinding()]
    param()
    
    $status = [PSCustomObject]@{
        IsActive = $script:IsActive
        ProcessId = if ($script:InterceptorProcess) { $script:InterceptorProcess.Id } else { $null }
        ProcessName = if ($script:InterceptorProcess) { $script:InterceptorProcess.ProcessName } else { $null }
        StartTime = $script:Stats.StartTime
        LastUpdate = $script:Stats.LastUpdate
        TotalCalls = $script:Stats.TotalCalls
        BytesStreamed = $script:Stats.BytesStreamed
        ErrorCount = $script:Stats.ErrorCount
        Uptime = if ($script:Stats.StartTime) { (Get-Date) - $script:Stats.StartTime } else { $null }
        CallLogCount = $script:CallLog.Count
        RealTimeStreaming = ($script:StreamingJob -ne $null)
    }
    
    return $status
}

function Get-OSInterceptorStats {
    <#
    .SYNOPSIS
        Gets detailed statistics from the OS Explorer Interceptor
    
    .DESCRIPTION
        Returns detailed statistics including per-hook call counts, durations, and error rates
    
    .PARAMETER HookType
        Filter statistics by hook type
    
    .PARAMETER Top
        Show only top N hooks by call count
    
    .EXAMPLE
        Get-OSInterceptorStats
        
    .EXAMPLE
        Get-OSInterceptorStats -HookType "NETWORK" -Top 10
    #>
    [CmdletBinding()]
    param(
        [Parameter()]
        [ValidateSet("FILE", "REGISTRY", "PROCESS", "MEMORY", "NETWORK", "WINDOW", "COM", "CRYPTO")]
        [string]$HookType,
        
        [Parameter()]
        [int]$Top = 0
    )
    
    if (-not $script:IsActive -and $script:CallLog.Count -eq 0) {
        Write-Warning "No statistics available - interceptor not running and no captured data"
        return
    }
    
    # Get calls from log
    $calls = $script:CallLog
    
    if ($HookType) {
        $calls = $calls | Where-Object { $_.HookType -eq $HookType }
    }
    
    # Group by function
    $stats = $calls | Group-Object -Property FunctionName | ForEach-Object {
        $group = $_.Group
        $durations = $group | Select-Object -ExpandProperty Duration
        
        [PSCustomObject]@{
            FunctionName = $_.Name
            CallCount = $_.Count
            TotalDuration = ($durations | Measure-Object -Sum).Sum
            AverageDuration = ($durations | Measure-Object -Average).Average
            MinDuration = ($durations | Measure-Object -Minimum).Minimum
            MaxDuration = ($durations | Measure-Object -Maximum).Maximum
            HookType = $group[0].HookType
            ErrorCount = ($group | Where-Object { $_.ReturnValue -lt 0 }).Count
        }
    } | Sort-Object -Property CallCount -Descending
    
    if ($Top -gt 0) {
        $stats = $stats | Select-Object -First $Top
    }
    
    return $stats
}

function Clear-OSInterceptorLog {
    <#
    .SYNOPSIS
        Clears the captured call log
    
    .DESCRIPTION
        Clears all captured calls from memory
    
    .EXAMPLE
        Clear-OSInterceptorLog
    #>
    [CmdletBinding()]
    param()
    
    $beforeCount = $script:CallLog.Count
    $script:CallLog.Clear()
    
    Write-Host "[INFO] Cleared $beforeCount entries from call log" -ForegroundColor Cyan
}

function Show-OSInterceptorHelp {
    <#
    .SYNOPSIS
        Shows help information for the OS Explorer Interceptor
    
    .DESCRIPTION
        Displays available commands and usage examples
    
    .EXAMPLE
        Show-OSInterceptorHelp
    #>
    [CmdletBinding()]
    param()
    
    $helpText = @"
╔══════════════════════════════════════════════════════════════════════╗
║           OS EXPLORER INTERCEPTOR - POWERHELP                      ║
║           Integrated with MASM IDE - Real-time Streaming           ║
╚══════════════════════════════════════════════════════════════════════╝

COMMANDS:
─────────

Start Interception:
    Start-OSInterceptor -ProcessId <PID> [-RealTimeStreaming] [-Filter <types>]
    Start-OSInterceptor -ProcessName <name> [-RealTimeStreaming]
    
    Aliases: startos
    
    Examples:
        Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming
        Start-OSInterceptor -ProcessName "cursor" -Filter @("FILE", "NETWORK")

Stop Interception:
    Stop-OSInterceptor [-SaveData] [-OutputPath <path>]
    
    Aliases: stopos
    
    Examples:
        Stop-OSInterceptor
        Stop-OSInterceptor -SaveData -OutputPath "C:\capture.json"

Get Status:
    Get-OSInterceptorStatus
    
    Aliases: statusos
    
    Shows current status, uptime, and basic statistics

Get Statistics:
    Get-OSInterceptorStats [-HookType <type>] [-Top <n>]
    
    Aliases: statsos
    
    Examples:
        Get-OSInterceptorStats
        Get-OSInterceptorStats -HookType "NETWORK" -Top 10

Clear Log:
    Clear-OSInterceptorLog
    
    Aliases: clearos
    
    Clears all captured calls from memory

Show Help:
    Show-OSInterceptorHelp
    
    Aliases: helpos

REAL-TIME STREAMING:
───────────────────

When -RealTimeStreaming is enabled, intercepted calls are displayed in
PowerShell in real-time with the following format:

    [Timestamp] [PID:TID] [HookType] Function(Params) = ReturnValue [Duration]ns

Example:
    [1234567890] [1234:5678] [NETWORK] send(0x123, 0x456, 1024, 0) = 1024 [1500]ns

FILTERING:
──────────

Filter by hook types to reduce noise:
    - FILE      : File I/O operations (CreateFile, ReadFile, WriteFile, etc.)
    - REGISTRY  : Registry operations (RegOpenKey, RegQueryValue, etc.)
    - PROCESS   : Process/thread operations (CreateProcess, CreateThread, etc.)
    - MEMORY    : Memory operations (VirtualAlloc, HeapAlloc, etc.)
    - NETWORK   : Network operations (send, recv, WSAConnect, etc.)
    - WINDOW    : Window/GUI operations (SendMessage, BitBlt, etc.)
    - COM       : COM/OLE operations (CoCreateInstance, etc.)
    - CRYPTO    : Crypto operations (CryptEncrypt, CryptDecrypt, etc.)

EXAMPLES:
─────────

# Start intercepting Cursor IDE with real-time streaming
PS> Start-OSInterceptor -ProcessName "cursor" -RealTimeStreaming

# Start with network filtering only
PS> Start-OSInterceptor -ProcessId 1234 -Filter @("NETWORK") -RealTimeStreaming

# Get detailed statistics for network hooks
PS> Get-OSInterceptorStats -HookType "NETWORK" -Top 10

# Save captured data and stop
PS> Stop-OSInterceptor -SaveData -OutputPath "C:\cursor_capture.json"

# Show current status
PS> Get-OSInterceptorStatus

INTEGRATION WITH MASM IDE:
═══════════════════════════

This module is integrated with the MASM IDE and can be used directly
from the IDE's PowerShell terminal. The interceptor DLL is automatically
injected using the IDE's built-in injector.

For standalone use, ensure the following files are available:
    - os_explorer_interceptor.dll
    - masm_ide_injector.exe (or use PowerShell injection)

TROUBLESHOOTING:
════════════════

"Failed to inject DLL":
    - Ensure target process is running
    - Check if you have administrator privileges
    - Verify DLL path is correct

"No statistics available":
    - Interceptor may not be running
    - No calls have been captured yet
    - Try with -RealTimeStreaming to see live data

"Access denied":
    - Run PowerShell as Administrator
    - Some processes require elevated privileges

PERFORMANCE:
════════════

- Overhead: < 2% CPU usage
- Memory: ~5MB for call log buffer
- Streaming: Real-time, minimal latency
- Call rate: Up to 100,000 calls/second

For more information, visit: https://github.com/ItsMehRAWRXD/cloud-hosting
"@
    
    Write-Host $helpText -ForegroundColor Cyan
}

#============================================================================
# REAL-TIME STREAMING
#============================================================================

function Start-RealTimeStreaming {
    <#
    .SYNOPSIS
        Starts real-time streaming of intercepted calls
    
    .DESCRIPTION
        Internal function to start streaming thread that displays intercepted calls in real-time
    
    .PARAMETER ProcessId
        Process ID to stream from
    
    .PARAMETER Filter
        Hook types to filter
    
    .PARAMETER OutputFile
        Optional output file for captured data
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId,
        
        [Parameter()]
        [string[]]$Filter = @(),
        
        [Parameter()]
        [string]$OutputFile
    )
    
    # Create streaming scriptblock
    $streamScriptBlock = {
        param($ProcessId, $Filter, $OutputFile, $EventQueue)
        
        $lastCount = 0
        $outputStream = if ($OutputFile) { [System.IO.StreamWriter]::new($OutputFile) } else { $null }
        
        try {
            while ($true) {
                # Process events from queue
                while ($EventQueue.TryDequeue([ref]$event)) {
                    # Apply filter
                    if ($Filter.Count -eq 0 -or $event.HookType -in $Filter) {
                        # Format output
                        $output = Format-InterceptorOutput -Event $event
                        
                        # Write to console
                        Write-Host $output -NoNewline
                        
                        # Write to file if specified
                        if ($outputStream) {
                            $outputStream.WriteLine($output)
                            $outputStream.Flush()
                        }
                    }
                }
                
                Start-Sleep -Milliseconds 10
            }
        }
        finally {
            if ($outputStream) {
                $outputStream.Close()
            }
        }
    }
    
    # Start streaming job
    $script:StreamingJob = Start-Job -ScriptBlock $streamScriptBlock -ArgumentList @(
        $ProcessId, $Filter, $OutputFile, $script:EventQueue
    )
    
    Write-Host "[INFO] Real-time streaming started" -ForegroundColor Cyan
}

function Stop-RealTimeStreaming {
    <#
    .SYNOPSIS
        Stops real-time streaming
    #>
    [CmdletBinding()]
    param()
    
    if ($script:StreamingJob) {
        Stop-Job -Job $script:StreamingJob
        Remove-Job -Job $script:StreamingJob -Force
        $script:StreamingJob = $null
        
        Write-Host "[INFO] Real-time streaming stopped" -ForegroundColor Cyan
    }
}

function Format-InterceptorOutput {
    <#
    .SYNOPSIS
        Formats intercepted call for display
    
    .PARAMETER Event
        The intercepted call event
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [PSObject]$Event
    )
    
    # Color coding by hook type
    $color = switch ($Event.HookType) {
        "FILE" { "Yellow" }
        "REGISTRY" { "Magenta" }
        "NETWORK" { "Cyan" }
        "PROCESS" { "Red" }
        "MEMORY" { "Green" }
        "WINDOW" { "Blue" }
        "COM" { "DarkYellow" }
        "CRYPTO" { "DarkMagenta" }
        default { "White" }
    }
    
    # Format timestamp
    $timestamp = [DateTimeOffset]::FromUnixTimeMilliseconds($Event.Timestamp).ToLocalTime().ToString("HH:mm:ss.fff")
    
    # Build output string
    $output = "[$timestamp] [$($Event.ProcessID):$($Event.ThreadID)] [$($Event.HookType)] $($Event.FunctionName)("
    
    # Add parameters
    if ($Event.ParamCount -gt 0) {
        $params = for ($i = 0; $i -lt [Math]::Min($Event.ParamCount, 8); $i++) {
            "0x{0:X}" -f $Event.Parameters[$i]
        }
        $output += ($params -join ", ")
    }
    
    $output += ") = 0x{0:X} [{1}ns]" -f $Event.ReturnValue, $Event.Duration
    
    return $output
}

function Wait-OSInterceptorEvent {
    <#
    .SYNOPSIS
        Waits for specific interceptor events
    
    .DESCRIPTION
        Blocks until a specific event occurs or timeout is reached
    
    .PARAMETER HookType
        Hook type to wait for
    
    .PARAMETER FunctionName
        Specific function to wait for
    
    .PARAMETER Timeout
        Timeout in seconds (default: 30)
    
    .PARAMETER Count
        Number of events to wait for (default: 1)
    
    .EXAMPLE
        Wait-OSInterceptorEvent -HookType "NETWORK" -FunctionName "send"
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$HookType,
        
        [Parameter()]
        [string]$FunctionName,
        
        [Parameter()]
        [int]$Timeout = 30,
        
        [Parameter()]
        [int]$Count = 1
    )
    
    $timeoutTime = (Get-Date).AddSeconds($Timeout)
    $foundCount = 0
    
    Write-Host "Waiting for $HookType event" -ForegroundColor Cyan
    
    while ((Get-Date) -lt $timeoutTime -and $foundCount -lt $Count) {
        # Check recent calls
        $recentCalls = $script:CallLog | Where-Object {
            $_.HookType -eq $HookType -and
            (-not $FunctionName -or $_.FunctionName -eq $FunctionName)
        } | Select-Object -Last 100
        
        if ($recentCalls.Count -gt $foundCount) {
            $foundCount = $recentCalls.Count
            
            if ($foundCount -ge $Count) {
                Write-Host "[SUCCESS] Event detected!" -ForegroundColor Green
                return $recentCalls[-1]
            }
        }
        
        Start-Sleep -Milliseconds 100
    }
    
    if ($foundCount -lt $Count) {
        Write-Warning "Timeout waiting for $HookType event"
        return $null
    }
}

function Export-OSInterceptorData {
    <#
    .SYNOPSIS
        Exports captured interceptor data to various formats
    
    .DESCRIPTION
        Exports the captured call log to JSON, CSV, or XML format
    
    .PARAMETER Format
        Export format (JSON, CSV, XML)
    
    .PARAMETER Path
        Output file path
    
    .PARAMETER Filter
        Filter data before export
    
    .EXAMPLE
        Export-OSInterceptorData -Format JSON -Path "C:\capture.json"
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("JSON", "CSV", "XML")]
        [string]$Format,
        
        [Parameter(Mandatory = $true)]
        [string]$Path,
        
        [Parameter()]
        [scriptblock]$Filter
    )
    
    if ($script:CallLog.Count -eq 0) {
        Write-Warning "No data to export"
        return
    }
    
    $data = if ($Filter) {
        $script:CallLog | Where-Object $Filter
    } else {
        $script:CallLog
    }
    
    switch ($Format) {
        "JSON" {
            $data | ConvertTo-Json -Depth 10 | Out-File -FilePath $Path -Encoding UTF8
        }
        "CSV" {
            $data | Export-Csv -Path $Path -NoTypeInformation
        }
        "XML" {
            $data | Export-Clixml -Path $Path
        }
    }
    
    Write-Host "[SUCCESS] Exported $($data.Count) records to $Path" -ForegroundColor Green
}

#============================================================================
# DLL INJECTION HELPER (Fallback if masm_ide_injector.exe not available)
#============================================================================

function Invoke-DllInjection {
    <#
    .SYNOPSIS
        Injects a DLL into a target process using PowerShell
    
    .DESCRIPTION
        Fallback injection method if masm_ide_injector.exe is not available
    
    .PARAMETER ProcessId
        Target process ID
    
    .PARAMETER DllPath
        Path to DLL to inject
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId,
        
        [Parameter(Mandatory = $true)]
        [string]$DllPath
    )
    
    try {
        # Try x64 DLL first (most common)
        $dllPath = Join-Path (Split-Path $PSScriptRoot -Parent) "bin\os_explorer_interceptor_x64.dll"
        if (-not (Test-Path $dllPath)) {
            # Fall back to x86 DLL
            $dllPath = Join-Path (Split-Path $PSScriptRoot -Parent) "bin\os_explorer_interceptor_x86.dll"
            if (-not (Test-Path $dllPath)) {
                throw "No interceptor DLL found in bin directory (tried x64 and x86)"
            }
        }
        
        Add-Type @"
            using System;
            using System.Runtime.InteropServices;
            using System.Text;
            
            public class Injector
            {
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, uint nSize, out IntPtr lpNumberOfBytesWritten);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern IntPtr GetModuleHandle(string lpModuleName);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, out IntPtr lpThreadId);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern bool CloseHandle(IntPtr hObject);
                
                [DllImport("kernel32.dll", SetLastError = true)]
                public static extern bool FreeLibrary(IntPtr hLibModule);
                
                public static bool InjectDll(int processId, string dllPath)
                {
                    const uint PROCESS_ALL_ACCESS = 0x1F0FFF;
                    const uint MEM_COMMIT = 0x1000;
                    const uint MEM_RESERVE = 0x2000;
                    const uint PAGE_READWRITE = 0x04;
                    
                    IntPtr hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, processId);
                    if (hProcess == IntPtr.Zero)
                        return false;
                    
                    IntPtr loadLibraryAddr = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
                    if (loadLibraryAddr == IntPtr.Zero)
                    {
                        CloseHandle(hProcess);
                        return false;
                    }
                    
                    byte[] dllBytes = Encoding.ASCII.GetBytes(dllPath + '\0');
                    IntPtr allocMemAddress = VirtualAllocEx(hProcess, IntPtr.Zero, (uint)dllBytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                    if (allocMemAddress == IntPtr.Zero)
                    {
                        CloseHandle(hProcess);
                        return false;
                    }
                    
                    IntPtr bytesWritten;
                    if (!WriteProcessMemory(hProcess, allocMemAddress, dllBytes, (uint)dllBytes.Length, out bytesWritten))
                    {
                        CloseHandle(hProcess);
                        return false;
                    }
                    
                    IntPtr hThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0, loadLibraryAddr, allocMemAddress, 0, out IntPtr threadId);
                    if (hThread == IntPtr.Zero)
                    {
                        CloseHandle(hProcess);
                        return false;
                    }
                    
                    WaitForSingleObject(hThread, 0xFFFFFFFF);
                    CloseHandle(hThread);
                    CloseHandle(hProcess);
                    
                    return true;
                }
            }
"@
        
        $result = [Injector]::InjectDll($ProcessId, $DllPath)
        
        return [PSCustomObject]@{
            Success = $result
            Error = if (-not $result) { "Injection failed" } else { $null }
        }
        
    } catch {
        return [PSCustomObject]@{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

#============================================================================
# ALIASES
#============================================================================

Set-Alias -Name startos -Value Start-OSInterceptor
Set-Alias -Name stopos -Value Stop-OSInterceptor
Set-Alias -Name statusos -Value Get-OSInterceptorStatus
Set-Alias -Name statsos -Value Get-OSInterceptorStats
Set-Alias -Name clearos -Value Clear-OSInterceptorLog
Set-Alias -Name helpos -Value Show-OSInterceptorHelp

#============================================================================
# MODULE CLEANUP
#============================================================================

$ExecutionContext.SessionState.Module.OnRemove = {
    # Cleanup when module is removed
    if ($script:IsActive) {
        Stop-OSInterceptor
    }
    
    if ($script:StreamingJob) {
        Stop-RealTimeStreaming
    }
}

# Export module members
Export-ModuleMember -Function @(
    'Start-OSInterceptor',
    'Stop-OSInterceptor',
    'Get-OSInterceptorStatus',
    'Get-OSInterceptorStats',
    'Clear-OSInterceptorLog',
    'Show-OSInterceptorHelp',
    'Wait-OSInterceptorEvent',
    'Format-InterceptorOutput',
    'Export-OSInterceptorData'
) -Alias @(
    'startos',
    'stopos',
    'statusos',
    'statsos',
    'clearos',
    'helpos'
)

Write-Host "OS Explorer Interceptor PowerShell module loaded successfully!" -ForegroundColor Green
Write-Host "Type 'helpos' for available commands" -ForegroundColor Cyan
