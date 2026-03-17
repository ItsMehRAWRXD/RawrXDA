<#
.SYNOPSIS
    Browser Bridge - IPC Client for PS5.1 Browser Host
    
.DESCRIPTION
    This module provides communication between the main RawrXD IDE (which may run
    on PowerShell 7.5) and the PS5.1 Browser Host subprocess that provides full
    WebBrowser functionality including video playback.
    
.NOTES
    Author: RawrXD Team
    Version: 1.0.0
#>

# ============================================================================
# BROWSER BRIDGE MODULE
# ============================================================================

$script:BrowserBridge = @{
    Process = $null
    PipeClient = $null
    PipeWriter = $null
    PipeReader = $null
    PipeName = "RawrXD-Browser-IPC-" + [guid]::NewGuid().ToString("N").Substring(0, 8)
    IsConnected = $false
    LastUrl = ""
    LastTitle = ""
    EventHandlers = @{
        Navigated = @()
        Loaded = @()
        Closed = @()
    }
}

function Initialize-BrowserBridge {
    <#
    .SYNOPSIS
        Initialize the browser bridge and launch PS5.1 browser host
    #>
    param(
        [string]$StartUrl = "https://www.google.com",
        [switch]$Standalone
    )
    
    $scriptPath = Join-Path $PSScriptRoot "PS51-Browser-Host.ps1"
    
    if (-not (Test-Path $scriptPath)) {
        Write-Error "PS51-Browser-Host.ps1 not found at: $scriptPath"
        return $false
    }
    
    try {
        # Build arguments for PS5.1 host
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", "`"$scriptPath`"",
            "-PipeName", $script:BrowserBridge.PipeName,
            "-StartUrl", "`"$StartUrl`""
        )
        
        if ($Standalone) {
            $arguments += "-Standalone"
        }
        
        Write-Host "[BrowserBridge] Launching PS5.1 Browser Host..." -ForegroundColor Cyan
        Write-Host "[BrowserBridge] Pipe: $($script:BrowserBridge.PipeName)" -ForegroundColor DarkGray
        
        # Launch PowerShell 5.1 (powershell.exe) subprocess
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = "powershell.exe"  # Windows PowerShell 5.1
        $startInfo.Arguments = $arguments -join " "
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $false
        
        $script:BrowserBridge.Process = [System.Diagnostics.Process]::Start($startInfo)
        
        if ($Standalone) {
            Write-Host "[BrowserBridge] Launched in standalone mode (no IPC)" -ForegroundColor Yellow
            return $true
        }
        
        # Wait a moment for the pipe server to start
        Start-Sleep -Milliseconds 500
        
        # Connect to the named pipe
        $connected = Connect-BrowserPipe -Timeout 5000
        
        if ($connected) {
            Write-Host "[BrowserBridge] Connected to PS5.1 Browser Host!" -ForegroundColor Green
            return $true
        } else {
            Write-Warning "[BrowserBridge] Could not connect to browser host pipe"
            return $false
        }
        
    } catch {
        Write-Error "[BrowserBridge] Failed to launch browser host: $_"
        return $false
    }
}

function Connect-BrowserPipe {
    param([int]$Timeout = 5000)
    
    try {
        $script:BrowserBridge.PipeClient = New-Object System.IO.Pipes.NamedPipeClientStream(
            ".",
            $script:BrowserBridge.PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            [System.IO.Pipes.PipeOptions]::Asynchronous
        )
        
        $script:BrowserBridge.PipeClient.Connect($Timeout)
        
        $script:BrowserBridge.PipeWriter = New-Object System.IO.StreamWriter($script:BrowserBridge.PipeClient)
        $script:BrowserBridge.PipeWriter.AutoFlush = $true
        $script:BrowserBridge.PipeReader = New-Object System.IO.StreamReader($script:BrowserBridge.PipeClient)
        
        $script:BrowserBridge.IsConnected = $true
        return $true
        
    } catch {
        Write-Warning "[BrowserBridge] Pipe connection failed: $_"
        return $false
    }
}

function Send-BrowserCommand {
    <#
    .SYNOPSIS
        Send a command to the PS5.1 browser host
    #>
    param(
        [Parameter(Mandatory)]
        [string]$Command,
        [hashtable]$Parameters = @{}
    )
    
    if (-not $script:BrowserBridge.IsConnected) {
        Write-Warning "[BrowserBridge] Not connected to browser host"
        return $null
    }
    
    try {
        $message = @{ command = $Command } + $Parameters
        $json = $message | ConvertTo-Json -Compress
        $script:BrowserBridge.PipeWriter.WriteLine($json)
        return $true
    } catch {
        Write-Warning "[BrowserBridge] Failed to send command: $_"
        $script:BrowserBridge.IsConnected = $false
        return $false
    }
}

function Receive-BrowserMessage {
    <#
    .SYNOPSIS
        Receive a message from the PS5.1 browser host (non-blocking)
    #>
    param([int]$TimeoutMs = 100)
    
    if (-not $script:BrowserBridge.IsConnected) { return $null }
    
    try {
        # Check if data is available
        $task = $script:BrowserBridge.PipeReader.ReadLineAsync()
        if ($task.Wait($TimeoutMs)) {
            $line = $task.Result
            if ($line) {
                return $line | ConvertFrom-Json
            }
        }
        return $null
    } catch {
        return $null
    }
}

# ============================================================================
# HIGH-LEVEL BROWSER COMMANDS
# ============================================================================

function Invoke-BrowserNavigate {
    <#
    .SYNOPSIS
        Navigate the browser to a URL
    #>
    param([Parameter(Mandatory)][string]$Url)
    
    # Smart URL handling
    if (-not $Url.StartsWith("http://") -and -not $Url.StartsWith("https://") -and -not $Url.StartsWith("file://")) {
        if ($Url -match "^[\w\-]+(\.[\w\-]+)+") {
            $Url = "https://$Url"
        } else {
            $Url = "https://www.google.com/search?q=" + [System.Uri]::EscapeDataString($Url)
        }
    }
    
    Send-BrowserCommand -Command "navigate" -Parameters @{ url = $Url }
}

function Invoke-BrowserBack {
    Send-BrowserCommand -Command "back"
}

function Invoke-BrowserForward {
    Send-BrowserCommand -Command "forward"
}

function Invoke-BrowserRefresh {
    Send-BrowserCommand -Command "refresh"
}

function Invoke-BrowserClose {
    Send-BrowserCommand -Command "close"
    Close-BrowserBridge
}

function Invoke-BrowserFocus {
    Send-BrowserCommand -Command "focus"
}

function Invoke-BrowserExecuteScript {
    param([Parameter(Mandatory)][string]$Script)
    Send-BrowserCommand -Command "executeScript" -Parameters @{ script = $Script }
}

function Get-BrowserUrl {
    Send-BrowserCommand -Command "getUrl"
    $response = Receive-BrowserMessage -TimeoutMs 1000
    if ($response -and $response.type -eq "url") {
        return $response.data.url
    }
    return $null
}

function Get-BrowserTitle {
    Send-BrowserCommand -Command "getTitle"
    $response = Receive-BrowserMessage -TimeoutMs 1000
    if ($response -and $response.type -eq "title") {
        return $response.data.title
    }
    return $null
}

function Test-BrowserHostRunning {
    <#
    .SYNOPSIS
        Check if the browser host process is still running
    #>
    if ($script:BrowserBridge.Process -and -not $script:BrowserBridge.Process.HasExited) {
        return $true
    }
    return $false
}

function Close-BrowserBridge {
    <#
    .SYNOPSIS
        Close the browser bridge and terminate the PS5.1 host
    #>
    Write-Host "[BrowserBridge] Closing browser bridge..." -ForegroundColor Yellow
    
    # Close pipe
    if ($script:BrowserBridge.PipeWriter) {
        try { $script:BrowserBridge.PipeWriter.Dispose() } catch { }
    }
    if ($script:BrowserBridge.PipeReader) {
        try { $script:BrowserBridge.PipeReader.Dispose() } catch { }
    }
    if ($script:BrowserBridge.PipeClient) {
        try { $script:BrowserBridge.PipeClient.Dispose() } catch { }
    }
    
    # Terminate process if still running
    if ($script:BrowserBridge.Process -and -not $script:BrowserBridge.Process.HasExited) {
        try {
            $script:BrowserBridge.Process.Kill()
        } catch { }
    }
    
    $script:BrowserBridge.IsConnected = $false
    $script:BrowserBridge.Process = $null
}

# ============================================================================
# STANDALONE LAUNCHER (For when PS5.1 native browser is all that's needed)
# ============================================================================

function Open-PS51Browser {
    <#
    .SYNOPSIS
        Open the PS5.1 browser in standalone mode (no IPC)
        This is useful when running on PS7.5 and you just want a working browser
    #>
    param(
        [string]$Url = "https://www.google.com"
    )
    
    $scriptPath = Join-Path $PSScriptRoot "PS51-Browser-Host.ps1"
    
    if (-not (Test-Path $scriptPath)) {
        Write-Error "PS51-Browser-Host.ps1 not found"
        return
    }
    
    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$scriptPath`"",
        "-StartUrl", "`"$Url`"",
        "-Standalone"
    )
    
    Write-Host "[PS51Browser] Opening standalone browser via PowerShell 5.1..." -ForegroundColor Cyan
    Start-Process "powershell.exe" -ArgumentList ($arguments -join " ")
}

# ============================================================================
# INTEGRATION HELPER
# ============================================================================

function Get-BrowserImplementation {
    <#
    .SYNOPSIS
        Determine the best browser implementation based on runtime
    .OUTPUTS
        Returns: "native" (use built-in WebBrowser), "webview2" (use WebView2), 
                 "ps51-bridge" (use PS5.1 subprocess), or "shim" (use WebView2Shim)
    #>
    
    $psVersion = $PSVersionTable.PSVersion.Major
    $isPS7Plus = $psVersion -ge 6
    
    # Check for WebView2
    $webview2Available = $false
    try {
        $webview2Path = [System.IO.Path]::Combine($env:ProgramFiles, "Microsoft", "EdgeWebView2", "Application")
        $webview2Available = Test-Path $webview2Path
    } catch { }
    
    if (-not $isPS7Plus) {
        # Running on PS5.1 - native WebBrowser is fully functional
        return "native"
    }
    
    # Running on PS7+ 
    # Check .NET version for ContextMenu compatibility
    $clrVersion = [System.Environment]::Version
    $isNet9Plus = $clrVersion.Major -ge 9
    
    if ($webview2Available -and -not $isNet9Plus) {
        # WebView2 available and no .NET 9 compatibility issues
        return "webview2"
    }
    
    # On .NET 9+ or no WebView2, use PS5.1 bridge for full video support
    return "ps51-bridge"
}

function Initialize-BestBrowser {
    <#
    .SYNOPSIS
        Initialize the best available browser implementation
    #>
    param([string]$StartUrl = "https://www.google.com")
    
    $impl = Get-BrowserImplementation
    Write-Host "[Browser] Selected implementation: $impl" -ForegroundColor Cyan
    
    switch ($impl) {
        "native" {
            # Return indicator that native WebBrowser should be used
            return @{ Type = "native"; Control = $null }
        }
        "webview2" {
            # Return indicator that WebView2 should be used
            return @{ Type = "webview2"; Control = $null }
        }
        "ps51-bridge" {
            # Launch PS5.1 browser subprocess
            $success = Initialize-BrowserBridge -StartUrl $StartUrl
            return @{ 
                Type = "ps51-bridge"
                Success = $success
                Bridge = $script:BrowserBridge
            }
        }
        "shim" {
            # Return indicator that WebView2Shim should be used
            return @{ Type = "shim"; Control = $null }
        }
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-BrowserBridge',
    'Send-BrowserCommand',
    'Receive-BrowserMessage',
    'Invoke-BrowserNavigate',
    'Invoke-BrowserBack',
    'Invoke-BrowserForward', 
    'Invoke-BrowserRefresh',
    'Invoke-BrowserClose',
    'Invoke-BrowserFocus',
    'Invoke-BrowserExecuteScript',
    'Get-BrowserUrl',
    'Get-BrowserTitle',
    'Test-BrowserHostRunning',
    'Close-BrowserBridge',
    'Open-PS51Browser',
    'Get-BrowserImplementation',
    'Initialize-BestBrowser'
)
