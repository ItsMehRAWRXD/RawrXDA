<#
.SYNOPSIS
    PowerShell 5.1 Browser Host for RawrXD IDE
    
.DESCRIPTION
    This script runs under Windows PowerShell 5.1 to provide full WebBrowser control
    functionality including video playback, YouTube watching, and legacy COM support.
    It communicates with the main IDE (which may run on PS7.5) via named pipes.
    
.NOTES
    Author: RawrXD Team
    Version: 1.0.0
    Requires: Windows PowerShell 5.1 (powershell.exe)
    
.PARAMETER PipeName
    The named pipe to use for IPC communication with the main IDE
    
.PARAMETER StartUrl
    Initial URL to navigate to
    
.PARAMETER Standalone
    Run as standalone browser window without IPC
#>

param(
    [string]$PipeName = "RawrXD-Browser-IPC",
    [string]$StartUrl = "about:blank",
    [switch]$Standalone,
    [switch]$Hidden
)

# Verify we're running under PowerShell 5.1
if ($PSVersionTable.PSVersion.Major -ge 6) {
    Write-Error "This script must run under Windows PowerShell 5.1 (powershell.exe), not PowerShell Core/7+"
    Write-Host "Attempting to relaunch under powershell.exe..."
    $args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $MyInvocation.MyCommand.Path)
    if ($PipeName) { $args += "-PipeName", $PipeName }
    if ($StartUrl) { $args += "-StartUrl", $StartUrl }
    if ($Standalone) { $args += "-Standalone" }
    if ($Hidden) { $args += "-Hidden" }
    Start-Process "powershell.exe" -ArgumentList $args
    exit
}

# Load Windows Forms
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ============================================================================
# BROWSER HOST FORM
# ============================================================================

$script:BrowserForm = $null
$script:WebBrowser = $null
$script:IpcServer = $null
$script:Running = $true

function Initialize-BrowserForm {
    $script:BrowserForm = New-Object System.Windows.Forms.Form
    $script:BrowserForm.Text = "RawrXD Browser (PS5.1 Host)"
    $script:BrowserForm.Size = New-Object System.Drawing.Size(1200, 800)
    $script:BrowserForm.StartPosition = "CenterScreen"
    $script:BrowserForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $script:BrowserForm.ForeColor = [System.Drawing.Color]::White
    
    # Navigation Panel
    $navPanel = New-Object System.Windows.Forms.Panel
    $navPanel.Dock = "Top"
    $navPanel.Height = 40
    $navPanel.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    
    # Back Button
    $btnBack = New-Object System.Windows.Forms.Button
    $btnBack.Text = "◀"
    $btnBack.Location = New-Object System.Drawing.Point(5, 5)
    $btnBack.Size = New-Object System.Drawing.Size(30, 28)
    $btnBack.FlatStyle = "Flat"
    $btnBack.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $btnBack.ForeColor = [System.Drawing.Color]::White
    $btnBack.Add_Click({ if ($script:WebBrowser.CanGoBack) { $script:WebBrowser.GoBack() } })
    $navPanel.Controls.Add($btnBack)
    
    # Forward Button
    $btnForward = New-Object System.Windows.Forms.Button
    $btnForward.Text = "▶"
    $btnForward.Location = New-Object System.Drawing.Point(40, 5)
    $btnForward.Size = New-Object System.Drawing.Size(30, 28)
    $btnForward.FlatStyle = "Flat"
    $btnForward.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $btnForward.ForeColor = [System.Drawing.Color]::White
    $btnForward.Add_Click({ if ($script:WebBrowser.CanGoForward) { $script:WebBrowser.GoForward() } })
    $navPanel.Controls.Add($btnForward)
    
    # Refresh Button
    $btnRefresh = New-Object System.Windows.Forms.Button
    $btnRefresh.Text = "⟳"
    $btnRefresh.Location = New-Object System.Drawing.Point(75, 5)
    $btnRefresh.Size = New-Object System.Drawing.Size(30, 28)
    $btnRefresh.FlatStyle = "Flat"
    $btnRefresh.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $btnRefresh.ForeColor = [System.Drawing.Color]::White
    $btnRefresh.Add_Click({ $script:WebBrowser.Refresh() })
    $navPanel.Controls.Add($btnRefresh)
    
    # Home Button
    $btnHome = New-Object System.Windows.Forms.Button
    $btnHome.Text = "🏠"
    $btnHome.Location = New-Object System.Drawing.Point(110, 5)
    $btnHome.Size = New-Object System.Drawing.Size(30, 28)
    $btnHome.FlatStyle = "Flat"
    $btnHome.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $btnHome.ForeColor = [System.Drawing.Color]::White
    $btnHome.Add_Click({ $script:WebBrowser.Navigate("https://www.google.com") })
    $navPanel.Controls.Add($btnHome)
    
    # URL TextBox
    $script:UrlBox = New-Object System.Windows.Forms.TextBox
    $script:UrlBox.Location = New-Object System.Drawing.Point(150, 8)
    $script:UrlBox.Size = New-Object System.Drawing.Size(800, 24)
    $script:UrlBox.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
    $script:UrlBox.ForeColor = [System.Drawing.Color]::White
    $script:UrlBox.BorderStyle = "FixedSingle"
    $script:UrlBox.Font = New-Object System.Drawing.Font("Consolas", 10)
    $script:UrlBox.Add_KeyDown({
        param($sender, $e)
        if ($e.KeyCode -eq "Enter") {
            $url = $script:UrlBox.Text
            if (-not $url.StartsWith("http://") -and -not $url.StartsWith("https://") -and -not $url.StartsWith("file://")) {
                if ($url -match "^[\w\-]+(\.[\w\-]+)+") {
                    $url = "https://$url"
                } else {
                    $url = "https://www.google.com/search?q=" + [System.Uri]::EscapeDataString($url)
                }
            }
            $script:WebBrowser.Navigate($url)
            $e.SuppressKeyPress = $true
        }
    })
    $navPanel.Controls.Add($script:UrlBox)
    
    # Go Button
    $btnGo = New-Object System.Windows.Forms.Button
    $btnGo.Text = "Go"
    $btnGo.Location = New-Object System.Drawing.Point(960, 5)
    $btnGo.Size = New-Object System.Drawing.Size(50, 28)
    $btnGo.FlatStyle = "Flat"
    $btnGo.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
    $btnGo.ForeColor = [System.Drawing.Color]::White
    $btnGo.Add_Click({
        $url = $script:UrlBox.Text
        if (-not $url.StartsWith("http://") -and -not $url.StartsWith("https://") -and -not $url.StartsWith("file://")) {
            if ($url -match "^[\w\-]+(\.[\w\-]+)+") {
                $url = "https://$url"
            } else {
                $url = "https://www.google.com/search?q=" + [System.Uri]::EscapeDataString($url)
            }
        }
        $script:WebBrowser.Navigate($url)
    })
    $navPanel.Controls.Add($btnGo)
    
    # YouTube Button
    $btnYouTube = New-Object System.Windows.Forms.Button
    $btnYouTube.Text = "▶ YouTube"
    $btnYouTube.Location = New-Object System.Drawing.Point(1020, 5)
    $btnYouTube.Size = New-Object System.Drawing.Size(80, 28)
    $btnYouTube.FlatStyle = "Flat"
    $btnYouTube.BackColor = [System.Drawing.Color]::FromArgb(255, 0, 0)
    $btnYouTube.ForeColor = [System.Drawing.Color]::White
    $btnYouTube.Add_Click({ $script:WebBrowser.Navigate("https://www.youtube.com") })
    $navPanel.Controls.Add($btnYouTube)
    
    # Status Label (for IPC status)
    $script:StatusLabel = New-Object System.Windows.Forms.Label
    $script:StatusLabel.Text = "PS5.1 Browser Host"
    $script:StatusLabel.Location = New-Object System.Drawing.Point(1110, 10)
    $script:StatusLabel.Size = New-Object System.Drawing.Size(80, 20)
    $script:StatusLabel.ForeColor = [System.Drawing.Color]::LimeGreen
    $script:StatusLabel.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    $navPanel.Controls.Add($script:StatusLabel)
    
    $script:BrowserForm.Controls.Add($navPanel)
    
    # WebBrowser Control (full functionality in PS5.1!)
    $script:WebBrowser = New-Object System.Windows.Forms.WebBrowser
    $script:WebBrowser.Dock = "Fill"
    $script:WebBrowser.ScriptErrorsSuppressed = $true
    $script:WebBrowser.IsWebBrowserContextMenuEnabled = $true
    $script:WebBrowser.AllowWebBrowserDrop = $true
    
    # Enable modern rendering
    try {
        $regPath = "HKCU:\Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION"
        if (-not (Test-Path $regPath)) {
            New-Item -Path $regPath -Force | Out-Null
        }
        $exeName = [System.IO.Path]::GetFileName([System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName)
        Set-ItemProperty -Path $regPath -Name $exeName -Value 11001 -Type DWord -ErrorAction SilentlyContinue
    } catch { }
    
    # Event handlers
    $script:WebBrowser.Add_Navigated({
        param($sender, $e)
        $script:UrlBox.Text = $sender.Url.ToString()
        Send-IpcMessage -Type "navigated" -Data @{ url = $sender.Url.ToString() }
    })
    
    $script:WebBrowser.Add_DocumentCompleted({
        param($sender, $e)
        $title = $sender.DocumentTitle
        if ($title) {
            $script:BrowserForm.Text = "RawrXD Browser - $title"
        }
        Send-IpcMessage -Type "loaded" -Data @{ url = $sender.Url.ToString(); title = $title }
    })
    
    $script:BrowserForm.Controls.Add($script:WebBrowser)
    
    # Resize handler for URL box
    $script:BrowserForm.Add_Resize({
        $script:UrlBox.Width = $script:BrowserForm.ClientSize.Width - 370
        $btnGo.Location = New-Object System.Drawing.Point(($script:UrlBox.Right + 10), 5)
        $btnYouTube.Location = New-Object System.Drawing.Point(($btnGo.Right + 10), 5)
        $script:StatusLabel.Location = New-Object System.Drawing.Point(($btnYouTube.Right + 10), 10)
    })
    
    # Form closing handler
    $script:BrowserForm.Add_FormClosing({
        param($sender, $e)
        $script:Running = $false
        Send-IpcMessage -Type "closed" -Data @{ }
    })
    
    # Navigate to start URL
    if ($StartUrl -and $StartUrl -ne "about:blank") {
        $script:WebBrowser.Navigate($StartUrl)
    } else {
        $script:WebBrowser.Navigate("https://www.google.com")
    }
}

# ============================================================================
# IPC COMMUNICATION (Named Pipes)
# ============================================================================

$script:PipeServer = $null
$script:PipeWriter = $null
$script:PipeReader = $null

function Initialize-IpcServer {
    if ($Standalone) { return }
    
    try {
        Add-Type -AssemblyName System.Core
        
        $script:PipeServer = New-Object System.IO.Pipes.NamedPipeServerStream(
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            1,
            [System.IO.Pipes.PipeTransmissionMode]::Message,
            [System.IO.Pipes.PipeOptions]::Asynchronous
        )
        
        $script:StatusLabel.Text = "Waiting..."
        $script:StatusLabel.ForeColor = [System.Drawing.Color]::Yellow
        
        # Start async wait for connection
        $asyncResult = $script:PipeServer.BeginWaitForConnection($null, $null)
        
        # Timer to check for connection
        $script:ConnectionTimer = New-Object System.Windows.Forms.Timer
        $script:ConnectionTimer.Interval = 100
        $script:ConnectionTimer.Add_Tick({
            if ($script:PipeServer.IsConnected) {
                $script:ConnectionTimer.Stop()
                $script:PipeWriter = New-Object System.IO.StreamWriter($script:PipeServer)
                $script:PipeWriter.AutoFlush = $true
                $script:PipeReader = New-Object System.IO.StreamReader($script:PipeServer)
                $script:StatusLabel.Text = "Connected"
                $script:StatusLabel.ForeColor = [System.Drawing.Color]::LimeGreen
                
                # Start message processing timer
                $script:MessageTimer = New-Object System.Windows.Forms.Timer
                $script:MessageTimer.Interval = 50
                $script:MessageTimer.Add_Tick({ Process-IpcMessages })
                $script:MessageTimer.Start()
            }
        })
        $script:ConnectionTimer.Start()
        
    } catch {
        Write-Host "IPC initialization failed: $_"
        $script:StatusLabel.Text = "Standalone"
        $script:StatusLabel.ForeColor = [System.Drawing.Color]::Orange
    }
}

function Send-IpcMessage {
    param(
        [string]$Type,
        [hashtable]$Data
    )
    
    if ($Standalone -or -not $script:PipeWriter) { return }
    
    try {
        $message = @{
            type = $Type
            data = $Data
            timestamp = (Get-Date).ToString("o")
        } | ConvertTo-Json -Compress
        
        $script:PipeWriter.WriteLine($message)
    } catch {
        # Connection lost
        $script:StatusLabel.Text = "Disconnected"
        $script:StatusLabel.ForeColor = [System.Drawing.Color]::Red
    }
}

function Process-IpcMessages {
    if (-not $script:PipeReader -or -not $script:PipeServer.IsConnected) { return }
    
    try {
        # Check if data is available (non-blocking)
        if ($script:PipeServer.IsConnected) {
            $line = $null
            
            # Use async read with timeout
            $task = $script:PipeReader.ReadLineAsync()
            if ($task.Wait(10)) {
                $line = $task.Result
            }
            
            if ($line) {
                $message = $line | ConvertFrom-Json
                Handle-IpcCommand -Message $message
            }
        }
    } catch {
        # Ignore read errors (no data available)
    }
}

function Handle-IpcCommand {
    param($Message)
    
    switch ($Message.command) {
        "navigate" {
            $script:WebBrowser.Navigate($Message.url)
        }
        "back" {
            if ($script:WebBrowser.CanGoBack) { $script:WebBrowser.GoBack() }
        }
        "forward" {
            if ($script:WebBrowser.CanGoForward) { $script:WebBrowser.GoForward() }
        }
        "refresh" {
            $script:WebBrowser.Refresh()
        }
        "getUrl" {
            Send-IpcMessage -Type "url" -Data @{ url = $script:WebBrowser.Url.ToString() }
        }
        "getTitle" {
            Send-IpcMessage -Type "title" -Data @{ title = $script:WebBrowser.DocumentTitle }
        }
        "close" {
            $script:BrowserForm.Close()
        }
        "focus" {
            $script:BrowserForm.Activate()
            $script:BrowserForm.BringToFront()
        }
        "resize" {
            $script:BrowserForm.Size = New-Object System.Drawing.Size($Message.width, $Message.height)
        }
        "move" {
            $script:BrowserForm.Location = New-Object System.Drawing.Point($Message.x, $Message.y)
        }
        "executeScript" {
            try {
                $result = $script:WebBrowser.Document.InvokeScript("eval", @($Message.script))
                Send-IpcMessage -Type "scriptResult" -Data @{ result = $result }
            } catch {
                Send-IpcMessage -Type "scriptError" -Data @{ error = $_.Exception.Message }
            }
        }
    }
}

# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

Write-Host "=============================================="
Write-Host "  RawrXD PS5.1 Browser Host"
Write-Host "=============================================="
Write-Host "PowerShell Version: $($PSVersionTable.PSVersion)"
Write-Host "CLR Version: $($PSVersionTable.CLRVersion)"
Write-Host "Pipe Name: $PipeName"
Write-Host "Start URL: $StartUrl"
Write-Host "Standalone Mode: $Standalone"
Write-Host ""

# Initialize browser form
Initialize-BrowserForm

# Initialize IPC server (unless standalone)
if (-not $Standalone) {
    Initialize-IpcServer
}

# Show form
if ($Hidden) {
    $script:BrowserForm.WindowState = "Minimized"
    $script:BrowserForm.ShowInTaskbar = $false
}

[System.Windows.Forms.Application]::EnableVisualStyles()
[System.Windows.Forms.Application]::Run($script:BrowserForm)

# Cleanup
if ($script:PipeServer) {
    try {
        $script:PipeServer.Dispose()
    } catch { }
}

Write-Host "Browser host closed."
