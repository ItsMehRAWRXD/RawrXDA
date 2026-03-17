# ============================================
# RawrXD-Browser.ps1 - Browser Module
# ============================================
# Contains WebView2 setup, browser & web tools, PS5.1 video browser functions,
# and DPI awareness functionality.
# ============================================

# ===============================
# WEBVIEW2 SETUP & COMPATIBILITY
# ===============================

function Test-WebView2Runtime {
  # Tests if WebView2 Runtime is installed on the system
  try {
    # Check registry for WebView2 installation
    $webView2RegPath = "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
    $webView2RegPath32 = "HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"

    if ((Test-Path $webView2RegPath) -or (Test-Path $webView2RegPath32)) {
      Write-EmergencyLog "✅ WebView2 Runtime found in registry" "SUCCESS"
      return $true
    }

    # Check for Edge WebView2 in common locations
    $webView2Paths = @(
      "$env:ProgramFiles\Microsoft\EdgeWebView\Application",
      "$env:ProgramFiles(x86)\Microsoft\EdgeWebView\Application",
      "$env:LocalAppData\Microsoft\EdgeWebView\Application"
    )

    foreach ($path in $webView2Paths) {
      if (Test-Path $path) {
        Write-EmergencyLog "✅ WebView2 Runtime found at: $path" "SUCCESS"
        return $true
      }
    }

    Write-EmergencyLog "WebView2 Runtime not found" "WARNING"
    return $false
  }
  catch {
    Write-EmergencyLog "Error checking WebView2: $($_.Exception.Message)" "WARNING"
    return $false
  }
}

function Initialize-WebView2ShimFallback {
  <#
    .SYNOPSIS
        Loads the WebView2Shim fallback module for environments without WebView2
    #>
  try {
    $shimPath = Join-Path $PSScriptRoot "WebView2Shim.ps1"

    if (Test-Path $shimPath) {
      Write-EmergencyLog "Loading WebView2Shim fallback from: $shimPath" "INFO"
      . $shimPath

      # Verify shim functions are available
      if (Get-Command Initialize-WebView2Shim -ErrorAction SilentlyContinue) {
        Write-EmergencyLog "✅ WebView2Shim loaded successfully" "SUCCESS"
        $script:UseWebView2FallbackAsBrowser = $true
        return $true
      }
    }
    else {
      Write-EmergencyLog "WebView2Shim.ps1 not found at: $shimPath" "WARNING"
    }

    return $false
  }
  catch {
    Write-EmergencyLog "Failed to load WebView2Shim: $($_.Exception.Message)" "ERROR"
    return $false
  }
}

# ===============================
# PS5.1 VIDEO BROWSER FUNCTIONS
# ===============================

function Open-PS51VideoBrowser {
  <#
    .SYNOPSIS
        Opens a URL in a PowerShell 5.1 subprocess browser with full video support
    .DESCRIPTION
        Launches PS51-Browser-Host.ps1 under Windows PowerShell 5.1, which provides
        full WebBrowser control functionality including video playback that doesn't
        work properly in PowerShell 7.5 due to .NET 9 compatibility issues.
    .PARAMETER Url
        The URL to navigate to
    #>
  [CmdletBinding()]
  param(
    [Parameter(Mandatory = $false)]
    [string]$Url = "https://www.youtube.com"
  )

  try {
    $browserHostPath = Join-Path $PSScriptRoot "PS51-Browser-Host.ps1"

    if (-not (Test-Path $browserHostPath)) {
      Write-StartupLog "❌ PS51-Browser-Host.ps1 not found at: $browserHostPath" "ERROR"
      [System.Windows.Forms.MessageBox]::Show(
        "PS51-Browser-Host.ps1 not found.`n`nPlease ensure the file exists at:`n$browserHostPath",
        "Video Browser Error",
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error
      )
      return
    }

    # Smart URL handling
    if ($Url -and -not $Url.StartsWith("http://") -and -not $Url.StartsWith("https://") -and -not $Url.StartsWith("file://")) {
      if ($Url -match "^[\w\-]+(\.[\w\-]+)+") {
        $Url = "https://$Url"
      }
      else {
        $Url = "https://www.google.com/search?q=" + [System.Uri]::EscapeDataString($Url)
      }
    }

    Write-StartupLog "🎬 Launching PS5.1 Video Browser: $Url" "INFO"

    # Build arguments for Windows PowerShell 5.1
    $arguments = @(
      "-NoProfile",
      "-ExecutionPolicy", "Bypass",
      "-File", "`"$browserHostPath`"",
      "-StartUrl", "`"$Url`"",
      "-Standalone"  # Run without IPC for simplicity
    )

    # Launch under Windows PowerShell 5.1 (powershell.exe, NOT pwsh.exe)
    $processInfo = New-Object System.Diagnostics.ProcessStartInfo
    $processInfo.FileName = "powershell.exe"  # Windows PowerShell 5.1
    $processInfo.Arguments = $arguments -join " "
    $processInfo.UseShellExecute = $false
    $processInfo.CreateNoWindow = $false

    $process = [System.Diagnostics.Process]::Start($processInfo)

    Write-StartupLog "✅ PS5.1 Video Browser launched (PID: $($process.Id))" "SUCCESS"

    # Track the process
    $script:RuntimeInfo.PS51BrowserProcess = $process

  }
  catch {
    Write-StartupLog "❌ Failed to launch PS5.1 Video Browser: $($_.Exception.Message)" "ERROR"
    [System.Windows.Forms.MessageBox]::Show(
      "Failed to launch video browser:`n$($_.Exception.Message)",
      "Video Browser Error",
      [System.Windows.Forms.MessageBoxButtons]::OK,
      [System.Windows.Forms.MessageBoxIcon]::Error
    )
  }
}

function Close-PS51VideoBrowser {
  <#
    .SYNOPSIS
        Closes the PS5.1 video browser subprocess if running
    #>
  if ($script:RuntimeInfo.PS51BrowserProcess -and -not $script:RuntimeInfo.PS51BrowserProcess.HasExited) {
    try {
      Write-StartupLog "Closing PS5.1 Video Browser..." "INFO"
      $script:RuntimeInfo.PS51BrowserProcess.Kill()
      $script:RuntimeInfo.PS51BrowserProcess = $null
      Write-StartupLog "✅ PS5.1 Video Browser closed" "SUCCESS"
    }
    catch {
      Write-StartupLog "Warning: Could not close PS5.1 browser: $($_.Exception.Message)" "WARNING"
    }
  }
}

function Test-PS51VideoBrowserRunning {
  <#
    .SYNOPSIS
        Check if a PS5.1 video browser is currently running
    #>
  return ($script:RuntimeInfo.PS51BrowserProcess -and -not $script:RuntimeInfo.PS51BrowserProcess.HasExited)
}

# ===============================
# BROWSER NAVIGATION & WEB TOOLS
# ===============================

function Open-Browser {
  <#
    .SYNOPSIS
        Opens a URL in the embedded browser (WebView2-WPF, WebView2, PS51-Bridge, WebView2Shim, or legacy IE)
    .PARAMETER Url
        The URL to navigate to
    #>
  [CmdletBinding()]
  param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$url
  )
  if ($url) {
    if (-not $url.StartsWith("http://") -and -not $url.StartsWith("https://")) {
      $url = "https://" + $url
    }
    $browserUrlBox.Text = $url

    switch ($script:browserType) {
      "WebView2-WPF" {
        # WPF WebView2 via ElementHost (.NET 9+ compatible)
        if ($script:webBrowser -and $script:webBrowser._isWpf) {
          $script:webBrowser.Navigate($url)
          Write-StartupLog "WPF WebView2 navigating to: $url" "INFO"
        }
      }
      "PS51-Bridge" {
        # Open in PS5.1 subprocess browser for full video support
        Open-PS51VideoBrowser -Url $url
      }
      "WebView2" {
        if ($webBrowser.CoreWebView2) {
          $webBrowser.CoreWebView2.Navigate($url)
        }
      }
      "WebView2Shim" {
        # WebView2Shim navigates by opening in external browser
        if ($script:webBrowser -and $script:webBrowser.Navigate) {
          $script:webBrowser.Navigate($url)
        }
        else {
          # Fallback to Start-Process
          Start-Process $url
        }
        Write-StartupLog "WebView2Shim navigating to: $url" "INFO"
      }
      default {
        # Legacy WebBrowser control
        if ($webBrowser) {
          $webBrowser.Navigate($url)
        }
      }
    }
  }
}

# ===============================
# DPI AWARENESS FUNCTIONS
# ===============================

function Initialize-DPIAwareness {
  <#
    .SYNOPSIS
        Initializes DPI awareness for proper scaling on high-DPI displays
    #>
  try {
    Write-StartupLog "Initializing DPI awareness..." "INFO"

    # Set process DPI awareness (Windows 8.1+)
    if ([System.Environment]::OSVersion.Version -ge [version]"6.3") {
      try {
        # Try to set DPI awareness via Windows API
        $user32 = Add-Type -MemberDefinition @"
                    [DllImport("user32.dll")]
                    public static extern bool SetProcessDpiAwareness(int value);
                    [DllImport("user32.dll")]
                    public static extern bool SetProcessDpiAwarenessContext(int value);
"@ -Name "User32" -Namespace "Win32" -PassThru

        # Try SetProcessDpiAwarenessContext first (Windows 10 1703+)
        $DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
        $result = $user32::SetProcessDpiAwarenessContext($DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)

        if (-not $result) {
          # Fallback to SetProcessDpiAwareness
          $PROCESS_PER_MONITOR_DPI_AWARE = 2
          $result = $user32::SetProcessDpiAwareness($PROCESS_PER_MONITOR_DPI_AWARE)
        }

        if ($result) {
          Write-StartupLog "✅ DPI awareness set successfully" "SUCCESS"
          $script:DPIAwarenessEnabled = $true
        }
        else {
          Write-StartupLog "⚠️ DPI awareness API call failed" "WARNING"
        }
      }
      catch {
        Write-StartupLog "⚠️ DPI awareness not available: $($_.Exception.Message)" "WARNING"
      }
    }

    # Configure Windows Forms for DPI scaling
    try {
      [System.Windows.Forms.Application]::EnableVisualStyles()
      [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false)

      # Set high DPI mode for .NET Core/.NET 5+
      if ([System.Environment]::Version -ge [version]"5.0") {
        [System.Windows.Forms.Application]::SetHighDpiMode([System.Windows.Forms.HighDpiMode]::PerMonitorV2)
      }

      Write-StartupLog "✅ Windows Forms DPI scaling configured" "SUCCESS"
    }
    catch {
      Write-StartupLog "⚠️ Windows Forms DPI configuration failed: $($_.Exception.Message)" "WARNING"
    }

  }
  catch {
    Write-StartupLog "❌ DPI awareness initialization failed: $($_.Exception.Message)" "ERROR"
  }
}

function Get-DPIScaleFactor {
  <#
    .SYNOPSIS
        Gets the current DPI scale factor for the primary monitor
    #>
  try {
    $graphics = [System.Drawing.Graphics]::FromHwnd([System.IntPtr]::Zero)
    $dpiX = $graphics.DpiX
    $dpiY = $graphics.DpiY
    $graphics.Dispose()

    # Standard DPI is 96, so scale factor is current/96
    $scaleX = $dpiX / 96.0
    $scaleY = $dpiY / 96.0

    return @{
      ScaleX    = $scaleX
      ScaleY    = $scaleY
      Average   = ($scaleX + $scaleY) / 2
      IsHighDPI = ($scaleX -gt 1.25 -or $scaleY -gt 1.25)
    }
  }
  catch {
    Write-StartupLog "❌ Failed to get DPI scale factor: $($_.Exception.Message)" "ERROR"
    return @{
      ScaleX    = 1.0
      ScaleY    = 1.0
      Average   = 1.0
      IsHighDPI = $false
    }
  }
}

function Scale-ControlForDPI {
  <#
    .SYNOPSIS
        Scales a Windows Forms control for the current DPI setting
    .PARAMETER Control
        The control to scale
    .PARAMETER BaseDPI
        The base DPI the control was designed for (default: 96)
    #>
  param(
    [Parameter(Mandatory = $true)]
    [System.Windows.Forms.Control]$Control,
    [int]$BaseDPI = 96
  )

  try {
    $dpiInfo = Get-DPIScaleFactor
    $scaleFactor = $dpiInfo.Average

    if ($scaleFactor -ne 1.0) {
      # Scale size
      $Control.Width = [int]($Control.Width * $scaleFactor)
      $Control.Height = [int]($Control.Height * $scaleFactor)

      # Scale font if it exists
      if ($Control.Font) {
        $currentSize = $Control.Font.Size
        $newSize = $currentSize * $scaleFactor
        $Control.Font = New-Object System.Drawing.Font($Control.Font.FontFamily, $newSize, $Control.Font.Style)
      }

      # Scale location
      $Control.Left = [int]($Control.Left * $scaleFactor)
      $Control.Top = [int]($Control.Top * $scaleFactor)

      Write-StartupLog "✅ Control scaled for DPI: $($Control.GetType().Name)" "DEBUG"
    }
  }
  catch {
    Write-StartupLog "⚠️ Failed to scale control for DPI: $($_.Exception.Message)" "WARNING"
  }
}

function Initialize-BrowserDPIAwareness {
  <#
    .SYNOPSIS
        Initializes browser-specific DPI awareness and scaling
    #>
  try {
    Write-StartupLog "Initializing browser DPI awareness..." "INFO"

    $dpiInfo = Get-DPIScaleFactor

    if ($dpiInfo.IsHighDPI) {
      Write-StartupLog "High DPI detected (Scale: $($dpiInfo.Average.ToString('F2'))x)" "INFO"

      # Configure WebView2 for high DPI if available
      if ($script:webBrowser -and $script:webBrowser.CoreWebView2) {
        try {
          # Set zoom factor based on DPI
          $zoomFactor = $dpiInfo.Average
          $script:webBrowser.CoreWebView2.ExecuteScriptAsync("document.body.style.zoom = '$zoomFactor';") | Out-Null
          Write-StartupLog "✅ WebView2 zoom set to ${zoomFactor}x for DPI" "SUCCESS"
        }
        catch {
          Write-StartupLog "⚠️ WebView2 DPI scaling failed: $($_.Exception.Message)" "WARNING"
        }
      }

      # Scale browser-related controls
      if ($browserUrlBox) { Scale-ControlForDPI -Control $browserUrlBox }
      if ($browserGoBtn) { Scale-ControlForDPI -Control $browserGoBtn }
      if ($browserBackBtn) { Scale-ControlForDPI -Control $browserBackBtn }
      if ($browserForwardBtn) { Scale-ControlForDPI -Control $browserForwardBtn }
      if ($browserRefreshBtn) { Scale-ControlForDPI -Control $browserRefreshBtn }
    }
    else {
      Write-StartupLog "Standard DPI detected, no scaling needed" "INFO"
    }

    Write-StartupLog "✅ Browser DPI awareness initialized" "SUCCESS"
  }
  catch {
    Write-StartupLog "❌ Browser DPI awareness failed: $($_.Exception.Message)" "ERROR"
  }
}