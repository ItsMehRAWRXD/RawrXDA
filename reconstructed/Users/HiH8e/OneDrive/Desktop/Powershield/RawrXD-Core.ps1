<#
.SYNOPSIS
    RawrXD - AI-Powered Text Editor with Ollama Integration
.DESCRIPTION
    A comprehensive 3-pane text editor featuring:
    - File Explorer with syntax highlighting
    - AI Chat integration via Ollama
    - Embedded web browser (WebView2/IE fallback)
    - Integrated terminal
    - Git version control
#>
# ============================================
# ENHANCED STARTUP LOGGER SYSTEM
# ============================================
# Initialize startup log file path
$script:StartupLogFile = Join-Path $env:TEMP "RawrXD-Startup-$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
# Startup logger function - enhanced with emergency fallback
function Write-StartupLog {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    try {
        # Use emergency logging if available, otherwise create new entry
        if (Get-Command Write-EmergencyLog -ErrorAction SilentlyContinue) {
            Write-EmergencyLog $Message $Level
            return
        }
        # Fallback to basic logging
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $logEntry = "[$timestamp] [$Level] $Message"
        # Ensure log file path is set
        if (-not $script:StartupLogFile) {
            $script:StartupLogFile = Join-Path $env:TEMP "RawrXD-Startup-$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
        }
        # Ensure log directory exists
        if ($script:StartupLogFile -and -not (Test-Path $script:StartupLogFile)) {
            $logDir = Split-Path $script:StartupLogFile
            if ($logDir -and -not (Test-Path $logDir)) {
                New-Item -ItemType Directory -Path $logDir -Force | Out-Null
            }
        }
        # Write to log file
        if ($script:StartupLogFile) {
            Add-Content -Path $script:StartupLogFile -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        }
        # Also output to console for immediate feedback
        $color = switch ($Level) {
            "ERROR" { "Red" }
            "WARNING" { "Yellow" }
            "SUCCESS" { "Green" }
            "DEBUG" { "Gray" }
            default { "White" }
        }
        Write-Host $logEntry -ForegroundColor $color
    }
    catch {
        # Last resort - console output only
        Write-Host "[$Level] $Message" -ForegroundColor $(if ($Level -eq "ERROR") { "Red" }else { "Yellow" })
    }
}
# ============================================
# ADVANCED ERROR HANDLING & NOTIFICATION SYSTEM
# ============================================
# Error categories and severity levels
$script:ErrorCategories = @{
    Critical       = "CRITICAL"
    Security       = "SECURITY"
    Network        = "NETWORK"
    FileSystem     = "FILESYSTEM"
    UI             = "UI"
    Ollama         = "OLLAMA"
    Authentication = "AUTH"
    Performance    = "PERFORMANCE"
}
# Error notification settings
$script:ErrorNotificationConfig = @{
    EnableEmailNotifications = $false
    EmailRecipient           = "admin@company.com"
    SMTPServer               = "smtp.company.com"
    EnablePopupNotifications = $false
    EnableSoundNotifications = $true
    LogToEventLog            = $true
    MaxErrorsPerMinute       = 10
    EnableErrorReporting     = $true
}
# Error tracking and rate limiting
$script:ErrorTracker = @{
    ErrorCount       = 0
    LastErrorTime    = Get-Date
    ErrorHistory     = @()
    SuppressedErrors = @()
}
function Register-ErrorHandler {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ErrorMessage,
        [Parameter(Mandatory = $false)]
        [ValidateSet("CRITICAL", "SECURITY", "NETWORK", "FILESYSTEM", "UI", "OLLAMA", "AUTH", "PERFORMANCE")]
        [string]$ErrorCategory = "UI",
        [Parameter(Mandatory = $false)]
        [ValidateSet("LOW", "MEDIUM", "HIGH", "CRITICAL")]
        [string]$Severity = "MEDIUM",
        [Parameter(Mandatory = $false)]
        [string]$SourceFunction = "",
        [Parameter(Mandatory = $false)]
        [hashtable]$AdditionalData = @{},
        [Parameter(Mandatory = $false)]
        [bool]$ShowToUser = $true
    )
    $currentTime = Get-Date
    $timeSinceLastError = ($currentTime - $script:ErrorTracker.LastErrorTime).TotalMinutes
    # Rate limiting: prevent error spam
    if ($timeSinceLastError -lt 1) {
        $script:ErrorTracker.ErrorCount++
        if ($script:ErrorTracker.ErrorCount -gt $script:ErrorNotificationConfig.MaxErrorsPerMinute) {
            Write-StartupLog "Error rate limit exceeded, suppressing notifications" "WARNING"
            return
        }
    }
    else {
        $script:ErrorTracker.ErrorCount = 1
        $script:ErrorTracker.LastErrorTime = $currentTime
    }
    # Create detailed error record
    $errorRecord = @{
        Timestamp      = $currentTime.ToString("yyyy-MM-dd HH:mm:ss.fff")
        Message        = $ErrorMessage
        Category       = $ErrorCategory
        Severity       = $Severity
        SourceFunction = $SourceFunction
        SessionId      = $script:CurrentSession.SessionId
        ProcessId      = $PID
        UserContext    = [Environment]::UserName
        MachineName    = [Environment]::MachineName
        AdditionalData = $AdditionalData
        StackTrace     = (Get-PSCallStack | Select-Object -Skip 1 | ForEach-Object { "$($_.Command):$($_.ScriptLineNumber)" }) -join " -> "
    }
    # Add to error history
    $script:ErrorTracker.ErrorHistory += $errorRecord
    # Keep only last 100 errors to prevent memory issues
    if (@($script:ErrorTracker.ErrorHistory).Count -gt 100) {
        $script:ErrorTracker.ErrorHistory = $script:ErrorTracker.ErrorHistory | Select-Object -Last 100
    }
    # Log to startup log
    Write-StartupLog "[$ErrorCategory - $Severity] $ErrorMessage" "ERROR"
    if ($SourceFunction) {
        Write-StartupLog "  Source: $SourceFunction" "ERROR"
    }
    # Log to security log if available
    if (Get-Command Write-SecurityLog -ErrorAction SilentlyContinue) {
        Write-SecurityLog "Application error" "ERROR" "$ErrorCategory - $ErrorMessage"
    }
    # Log to Windows Event Log
    if ($script:ErrorNotificationConfig.LogToEventLog) {
        try {
            if (-not ([System.Diagnostics.EventLog]::SourceExists("RawrXD"))) {
                [System.Diagnostics.EventLog]::CreateEventSource("RawrXD", "Application")
            }
            $eventId = switch ($Severity) {
                "LOW" { 1001 }
                "MEDIUM" { 1002 }
                "HIGH" { 1003 }
                "CRITICAL" { 1004 }
                default { 1000 }
            }
            [System.Diagnostics.EventLog]::WriteEntry("RawrXD", "$ErrorCategory Error: $ErrorMessage`nSource: $SourceFunction", "Error", $eventId)
        }
        catch {
            Write-StartupLog "Failed to write to Event Log: $_" "WARNING"
        }
    }
    # Visual notification to user
    if ($ShowToUser -and $script:ErrorNotificationConfig.EnablePopupNotifications) {
        Show-ErrorNotification -ErrorRecord $errorRecord
    }
    # Sound notification
    if ($script:ErrorNotificationConfig.EnableSoundNotifications -and $Severity -in @("HIGH", "CRITICAL")) {
        try {
            [System.Media.SystemSounds]::Exclamation.Play()
        }
        catch { }
    }
    # Email notification for critical errors
    if ($script:ErrorNotificationConfig.EnableEmailNotifications -and $Severity -eq "CRITICAL") {
        Send-ErrorNotificationEmail -ErrorRecord $errorRecord
    }
    # Auto-recovery for specific error types
    Invoke-AutoRecovery -ErrorRecord $errorRecord
}
# ============================================
# RUNTIME COMPATIBILITY DETECTION SYSTEM
# ============================================
Write-EmergencyLog "═══════════════════════════════════════════════════════" "INFO"
Write-EmergencyLog "RUNTIME COMPATIBILITY DETECTION" "INFO"
Write-EmergencyLog "═══════════════════════════════════════════════════════" "INFO"
# Global runtime detection results - used throughout the application
$script:RuntimeInfo = @{
    PowerShellVersion         = $PSVersionTable.PSVersion
    PowerShellEdition         = if ($PSVersionTable.PSEdition) { $PSVersionTable.PSEdition } else { "Desktop" }
    DotNetVersion             = $null
    DotNetMajorVersion        = 0
    IsWindowsPowerShell       = $false
    IsPowerShellCore          = $false
    IsPowerShell7Plus         = $false
    IsNet9OrLater             = $false
    IsNet8OrEarlier           = $true
    ContextMenuAvailable      = $true       # System.Windows.Forms.ContextMenu deprecated in .NET 9
    ContextMenuStripAvailable = $true  # Always use this for .NET 9+
    WebView2Available         = $false
    WebView2ShimLoaded        = $false
    UseLegacyBrowser          = $false
    WinFormsAvailable         = $false
    DetectionComplete         = $false
    # PS5.1 Browser Bridge (Hybrid Mode) - Use PS5.1 subprocess for full WebBrowser video support
    PS51BrowserAvailable      = $false
    UsePS51BrowserBridge      = $false
    PS51BrowserProcess        = $null
    BrowserImplementation     = "unknown"  # native, webview2, ps51-bridge, shim
}
function Initialize-RuntimeDetection {
    # Comprehensive runtime detection for WebView2/WinForms compatibility
    try {
        Write-EmergencyLog "Starting runtime detection..." "INFO"
        # 1. PowerShell Version Detection
        $psVersion = $PSVersionTable.PSVersion
        # Handle different PSVersion formats (5.1 has Build, 7.x has Patch)
        # Use PSObject.Properties to safely check for property existence
        $buildOrPatch = "0"
        if ($psVersion.PSObject.Properties['Build'] -and $psVersion.Build) {
            $buildOrPatch = $psVersion.Build
        }
        elseif ($psVersion.PSObject.Properties['Patch'] -and $psVersion.Patch) {
            $buildOrPatch = $psVersion.Patch
        }
        Write-EmergencyLog "PowerShell Version: $($psVersion.Major).$($psVersion.Minor).$buildOrPatch" "INFO"
        $script:RuntimeInfo.PowerShellVersion = $psVersion
        $script:RuntimeInfo.IsWindowsPowerShell = ($psVersion.Major -eq 5)
        $script:RuntimeInfo.IsPowerShellCore = ($psVersion.Major -ge 6)
        $script:RuntimeInfo.IsPowerShell7Plus = ($psVersion.Major -ge 7)
        if ($script:RuntimeInfo.IsWindowsPowerShell) {
            Write-EmergencyLog "✅ Windows PowerShell 5.1 detected - Full compatibility mode" "SUCCESS"
        }
        elseif ($script:RuntimeInfo.IsPowerShell7Plus) {
            Write-EmergencyLog "⚠️ PowerShell $($psVersion.Major).$($psVersion.Minor) detected - Checking .NET compatibility" "WARNING"
        }
        # 2. .NET Runtime Version Detection
        try {
            $dotnetDescription = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
            $script:RuntimeInfo.DotNetVersion = $dotnetDescription
            Write-EmergencyLog ".NET Runtime: $dotnetDescription" "INFO"
            # Parse major version from description (e.g., ".NET 9.0.0" or ".NET Framework 4.8.9261.0")
            if ($dotnetDescription -match "\.NET\s+(\d+)") {
                $script:RuntimeInfo.DotNetMajorVersion = [int]$matches[1]
                Write-EmergencyLog ".NET Major Version: $($script:RuntimeInfo.DotNetMajorVersion)" "INFO"
                # .NET 9+ has breaking changes with System.Windows.Forms.ContextMenu
                if ($script:RuntimeInfo.DotNetMajorVersion -ge 9) {
                    $script:RuntimeInfo.IsNet9OrLater = $true
                    $script:RuntimeInfo.IsNet8OrEarlier = $false
                    $script:RuntimeInfo.ContextMenuAvailable = $false
                    Write-EmergencyLog "⚠️ .NET 9+ detected - System.Windows.Forms.ContextMenu NOT available" "WARNING"
                    Write-EmergencyLog "   Will use ContextMenuStrip for all context menus" "INFO"
                }
            }
            elseif ($dotnetDescription -match "\.NET Framework\s+(\d+)\.(\d+)") {
                # .NET Framework always supports ContextMenu
                $script:RuntimeInfo.DotNetMajorVersion = [int]$matches[1]
                $script:RuntimeInfo.IsNet8OrEarlier = $true
                $script:RuntimeInfo.ContextMenuAvailable = $true
                Write-EmergencyLog "✅ .NET Framework $($matches[1]).$($matches[2]) - Full ContextMenu support" "SUCCESS"
            }
        }
        catch {
            Write-EmergencyLog "Could not detect .NET version: $($_.Exception.Message)" "WARNING"
            # Assume conservative defaults
            if ($script:RuntimeInfo.IsPowerShell7Plus) {
                $script:RuntimeInfo.IsNet9OrLater = $true
                $script:RuntimeInfo.ContextMenuAvailable = $false
            }
        }
        # 3. WebView2 Runtime Detection
        $script:RuntimeInfo.WebView2Available = Test-WebView2Runtime
        # 4. PS5.1 Browser Bridge Detection (for hybrid mode on PS7+)
        $ps51BrowserHostPath = Join-Path $PSScriptRoot "PS51-Browser-Host.ps1"
        $browserBridgePath = Join-Path $PSScriptRoot "BrowserBridge.psm1"
        $script:RuntimeInfo.PS51BrowserAvailable = (Test-Path $ps51BrowserHostPath) -and (Test-Path $browserBridgePath)
        if ($script:RuntimeInfo.PS51BrowserAvailable) {
            Write-EmergencyLog "✅ PS5.1 Browser Bridge available for hybrid mode" "SUCCESS"
        }
        # 5. Determine Browser Strategy based on runtime
        if ($script:RuntimeInfo.IsWindowsPowerShell) {
            # Windows PowerShell 5.1 - Native WebBrowser has full video support
            $script:RuntimeInfo.BrowserImplementation = "native"
            $script:RuntimeInfo.UseLegacyBrowser = $false
            Write-EmergencyLog "✅ PS5.1 Native Mode - Full WebBrowser video support" "SUCCESS"
        }
        elseif ($script:RuntimeInfo.IsPowerShell7Plus -and $script:RuntimeInfo.PS51BrowserAvailable) {
            # PowerShell 7+ with PS5.1 bridge available - BEST OPTION for video
            $script:RuntimeInfo.BrowserImplementation = "ps51-bridge"
            $script:RuntimeInfo.UsePS51BrowserBridge = $true
            $script:RuntimeInfo.UseLegacyBrowser = $false
            Write-EmergencyLog "🎬 PS7+ Hybrid Mode - Using PS5.1 subprocess for full video support" "SUCCESS"
        }
        elseif ($script:RuntimeInfo.WebView2Available -and -not $script:RuntimeInfo.IsNet9OrLater) {
            # WebView2 available and no .NET 9 issues
            $script:RuntimeInfo.BrowserImplementation = "webview2"
            $script:RuntimeInfo.UseLegacyBrowser = $false
            Write-EmergencyLog "✅ WebView2 Mode - Modern browser engine available" "SUCCESS"
        }
        elseif ($script:RuntimeInfo.IsNet9OrLater) {
            # .NET 9+ without PS5.1 bridge - need WebView2Shim
            Write-EmergencyLog "⚠️ .NET 9+ detected - WebView2 WinForms may have ContextMenu issues" "WARNING"
            $script:RuntimeInfo.BrowserImplementation = "shim"
            $script:RuntimeInfo.UseLegacyBrowser = $true
        }
        else {
            # Fallback to legacy browser
            $script:RuntimeInfo.BrowserImplementation = "legacy"
            $script:RuntimeInfo.UseLegacyBrowser = $true
            Write-EmergencyLog "WebView2 Runtime not installed - Using legacy browser" "WARNING"
        }
        # 6. Load WebView2Shim as fallback if needed (but not if using PS5.1 bridge)
        if ($script:RuntimeInfo.UseLegacyBrowser -and -not $script:RuntimeInfo.UsePS51BrowserBridge) {
            $shimLoaded = Initialize-WebView2ShimFallback
            $script:RuntimeInfo.WebView2ShimLoaded = $shimLoaded
        }
        $script:RuntimeInfo.DetectionComplete = $true
        # Output summary
        Write-EmergencyLog "═══════════════════════════════════════════════════════" "INFO"
        Write-EmergencyLog "RUNTIME DETECTION SUMMARY:" "INFO"
        Write-EmergencyLog "  PowerShell: $($script:RuntimeInfo.PowerShellVersion) ($($script:RuntimeInfo.PowerShellEdition))" "INFO"
        Write-EmergencyLog "  .NET Runtime: $($script:RuntimeInfo.DotNetVersion)" "INFO"
        Write-EmergencyLog "  ContextMenu Available: $($script:RuntimeInfo.ContextMenuAvailable)" "INFO"
        Write-EmergencyLog "  WebView2 Available: $($script:RuntimeInfo.WebView2Available)" "INFO"
        Write-EmergencyLog "  PS5.1 Browser Bridge: $($script:RuntimeInfo.PS51BrowserAvailable)" "INFO"
        Write-EmergencyLog "  Browser Implementation: $($script:RuntimeInfo.BrowserImplementation)" "INFO"
        Write-EmergencyLog "  Use Legacy Browser: $($script:RuntimeInfo.UseLegacyBrowser)" "INFO"
        Write-EmergencyLog "═══════════════════════════════════════════════════════" "INFO"
        return $true
    }
    catch {
        Write-EmergencyLog "❌ Runtime detection failed: $($_.Exception.Message)" "CRITICAL"
        # Set safe defaults
        $script:RuntimeInfo.UseLegacyBrowser = $true
        $script:RuntimeInfo.ContextMenuAvailable = $false
        return $false
    }
}
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
# Initialize DotNetSwitchEnabled to false by default (will be set by switcher module if loaded)
$script:DotNetSwitchEnabled = $false
function Initialize-EditorDiagnosticsModule {
    <#
    .SYNOPSIS
        Loads the Editor Diagnostics module for health monitoring and auto-repair
    #>
    try {
        $editorDiagPath = Join-Path $PSScriptRoot "editor-diagnostics.ps1"
        if (Test-Path $editorDiagPath) {
            Write-StartupLog "Loading Editor Diagnostics from: $editorDiagPath" "INFO"
            # Dot-source the diagnostics module
            . $editorDiagPath -ErrorAction Stop
            # Initialize diagnostics system (this will start monitoring)
            if (Get-Command "Initialize-EditorDiagnosticsSystem" -ErrorAction SilentlyContinue) {
                Initialize-EditorDiagnosticsSystem -ErrorAction SilentlyContinue | Out-Null
                Write-StartupLog "✅ Editor Diagnostics module loaded successfully" "SUCCESS"
                return $true
            }
            else {
                Write-StartupLog "Initialize-EditorDiagnosticsSystem function not found" "WARNING"
                return $false
            }
        }
        else {
            Write-StartupLog "Editor Diagnostics module not found at: $editorDiagPath" "WARNING"
            return $false
        }
    }
    catch {
        Write-StartupLog "Failed to load Editor Diagnostics: $($_.Exception.Message)" "ERROR"
        return $false
    }
}
function Initialize-DotNetRuntimeSwitcherModule {
    <#
    .SYNOPSIS
        Loads the .NET Runtime Switcher module for testing features on different runtimes
    #>
    try {
        $switcherPath = Join-Path $PSScriptRoot "dotnet-runtime-switcher.ps1"
        if (Test-Path $switcherPath) {
            Write-StartupLog "Loading .NET Runtime Switcher from: $switcherPath" "INFO"
            # Dot-source the switcher module with error handling
            . $switcherPath -ErrorAction Stop
            # Initialize the switcher system
            if (Get-Command "Initialize-DotNetRuntimeSwitcher" -ErrorAction SilentlyContinue) {
                Initialize-DotNetRuntimeSwitcher -ErrorAction Stop
                # Mark as enabled if initialization succeeded
                $script:DotNetSwitchEnabled = $true
                Write-StartupLog "✅ .NET Runtime Switcher loaded successfully" "SUCCESS"
                return $true
            }
            else {
                Write-StartupLog "Initialize-DotNetRuntimeSwitcher function not found in module" "ERROR"
                $script:DotNetSwitchEnabled = $false
                return $false
            }
        }
        else {
            Write-StartupLog ".NET Runtime Switcher not found at: $switcherPath" "WARNING"
            $script:DotNetSwitchEnabled = $false
            return $false
        }
    }
    catch {
        Write-StartupLog "Failed to load .NET Runtime Switcher: $($_.Exception.Message)" "ERROR"
        $script:DotNetSwitchEnabled = $false
        return $false
    }
}
function Get-SafeContextMenu {
    <#
    .SYNOPSIS
        Returns a context menu object appropriate for the current .NET runtime
    .DESCRIPTION
        On .NET 9+, System.Windows.Forms.ContextMenu is deprecated/removed.
        This function always returns ContextMenuStrip which works on all versions.
    .PARAMETER Parent
        Optional parent control to attach the context menu to
    #>
    param(
        [System.Windows.Forms.Control]$Parent = $null
    )
    try {
        # Always use ContextMenuStrip - it's available on all .NET versions
        # and is the recommended modern alternative
        $contextMenu = New-Object System.Windows.Forms.ContextMenuStrip
        if ($Parent) {
            $Parent.ContextMenuStrip = $contextMenu
        }
        return $contextMenu
    }
    catch {
        Write-EmergencyLog "Failed to create context menu: $($_.Exception.Message)" "ERROR"
        return $null
    }
}
function Test-ControlCompatibility {
    <#
    .SYNOPSIS
        Tests if a specific WinForms control is available in the current runtime
    .PARAMETER ControlTypeName
        Full type name of the control to test
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ControlTypeName
    )
    try {
        $type = [Type]::GetType($ControlTypeName, $false, $true)
        if ($type) {
            return $true
        }
        # Try with assembly qualification
        $type = [System.Type]::GetType("$ControlTypeName, System.Windows.Forms", $false, $true)
        return ($null -ne $type)
    }
    catch {
        return $false
    }
}
function Show-RuntimeInfo {
    <#
    .SYNOPSIS
        Displays current runtime information for debugging
    #>
    $info = @"
═══════════════════════════════════════════════════════
       RAWRXD RUNTIME COMPATIBILITY INFORMATION
═══════════════════════════════════════════════════════
POWERSHELL:
  Version: $($script:RuntimeInfo.PowerShellVersion)
  Edition: $($script:RuntimeInfo.PowerShellEdition)
  Is Windows PowerShell 5.1: $($script:RuntimeInfo.IsWindowsPowerShell)
  Is PowerShell Core 6+: $($script:RuntimeInfo.IsPowerShellCore)
  Is PowerShell 7+: $($script:RuntimeInfo.IsPowerShell7Plus)
.NET RUNTIME:
  Full Description: $($script:RuntimeInfo.DotNetVersion)
  Major Version: $($script:RuntimeInfo.DotNetMajorVersion)
  Is .NET 9 or later: $($script:RuntimeInfo.IsNet9OrLater)
  Is .NET 8 or earlier: $($script:RuntimeInfo.IsNet8OrEarlier)
WINFORMS COMPATIBILITY:
  Windows Forms Available: $($script:RuntimeInfo.WinFormsAvailable)
  ContextMenu Available: $($script:RuntimeInfo.ContextMenuAvailable)
  ContextMenuStrip Available: $($script:RuntimeInfo.ContextMenuStripAvailable)
BROWSER:
  WebView2 Runtime Installed: $($script:RuntimeInfo.WebView2Available)
  WebView2Shim Loaded: $($script:RuntimeInfo.WebView2ShimLoaded)
  Using Legacy Browser: $($script:RuntimeInfo.UseLegacyBrowser)
  Current Browser Type: $($script:browserType)
  WebView2 Active: $($script:useWebView2)
DETECTION STATUS:
  Detection Complete: $($script:RuntimeInfo.DetectionComplete)
  .NET Compatible for WebView2: $($script:NetVersionCompatible)
═══════════════════════════════════════════════════════
RECOMMENDATIONS:
"@
    if ($script:RuntimeInfo.IsNet9OrLater) {
        $info += "`n• You're on .NET 9+. WebView2 WinForms may have issues.`n• Using WebView2Shim/legacy browser for compatibility.`n• For full WebView2 support, use Windows PowerShell 5.1."
    }
    elseif ($script:RuntimeInfo.IsPowerShell7Plus -and -not $script:useWebView2) {
        $info += "`n• PowerShell 7+ detected but WebView2 unavailable.`n• Install WebView2 Runtime for better browser experience.`n• Or use Windows PowerShell 5.1 for full compatibility."
    }
    elseif ($script:RuntimeInfo.IsWindowsPowerShell) {
        $info += "`n• Windows PowerShell 5.1 - Full compatibility mode.`n• All features should work correctly."
    }
    else {
        $info += "`n• Current configuration should work well."
    }
    $info += "`n═══════════════════════════════════════════════════════"
    return $info
}
function Get-RuntimeInfoObject {
    <#
    .SYNOPSIS
        Returns the runtime info hashtable for programmatic access
    #>
    return $script:RuntimeInfo
}
# Run runtime detection immediately
$runtimeDetectionSuccess = Initialize-RuntimeDetection
if (-not $runtimeDetectionSuccess) {
    Write-EmergencyLog "⚠️ Runtime detection had issues - proceeding with safe defaults" "WARNING"
}
# ============================================
# WINDOWS FORMS ASSEMBLY LOADING WITH ERROR HANDLING
# ============================================
Write-EmergencyLog "Initializing Windows Forms assemblies..." "INFO"
# Function to safely load assemblies with fallback options
function Initialize-WindowsForms {
    param()
    try {
        # Check PowerShell version (use cached runtime info)
        $psVersion = $script:RuntimeInfo.PowerShellVersion
        Write-EmergencyLog "PowerShell Version: $psVersion" "INFO"
        if ($script:RuntimeInfo.IsPowerShellCore) {
            Write-EmergencyLog "PowerShell Core/7+ detected - using Microsoft.WindowsDesktop.App" "INFO"
            # For PowerShell Core 6+, we need Microsoft.WindowsDesktop.App
            try {
                # Try to install Microsoft.WindowsDesktop.App if not available
                if (-not (Get-Module -ListAvailable -Name Microsoft.PowerShell.GraphicalTools -ErrorAction SilentlyContinue)) {
                    Write-EmergencyLog "Installing PowerShell GraphicalTools module..." "INFO"
                    Install-Module Microsoft.PowerShell.GraphicalTools -Force -Scope CurrentUser -ErrorAction SilentlyContinue
                }
            }
            catch {
                Write-EmergencyLog "Failed to install GraphicalTools module: $($_.Exception.Message)" "WARNING"
            }
        }
        else {
            Write-EmergencyLog "Windows PowerShell 5.1 detected - standard assembly loading" "INFO"
        }
        # Primary assembly loading with error handling
        $assemblies = @(
            'System.Windows.Forms',
            'System.Drawing',
            'System.Net.Http',
            'System.IO.Compression.FileSystem',
            'Microsoft.VisualBasic',
            'System.Security'
        )
        foreach ($assembly in $assemblies) {
            try {
                Add-Type -AssemblyName $assembly -ErrorAction Stop
                Write-EmergencyLog "✅ Loaded assembly: $assembly" "SUCCESS"
            }
            catch {
                Write-EmergencyLog "❌ Failed to load assembly $assembly`: $($_.Exception.Message)" "ERROR"
                # Try alternative loading methods
                try {
                    # Method 1: Try with full assembly name
                    [System.Reflection.Assembly]::LoadWithPartialName($assembly) | Out-Null
                    Write-EmergencyLog "✅ Loaded $assembly using LoadWithPartialName" "SUCCESS"
                }
                catch {
                    # Method 2: Try loading from GAC
                    try {
                        $fullName = switch ($assembly) {
                            'System.Windows.Forms' { 'System.Windows.Forms, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089' }
                            'System.Drawing' { 'System.Drawing, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a' }
                            default { $assembly }
                        }
                        [System.Reflection.Assembly]::Load($fullName) | Out-Null
                        Write-EmergencyLog "✅ Loaded $assembly using full assembly name" "SUCCESS"
                    }
                    catch {
                        Write-EmergencyLog "❌ All loading methods failed for $assembly" "CRITICAL"
                    }
                }
            }
        }
        # Test Windows Forms availability
        try {
            # Set application compatibility settings BEFORE creating any forms
            [System.Windows.Forms.Application]::EnableVisualStyles()
            try {
                [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false)
            }
            catch {
                # Silently ignore if already set - this is fine
                Write-EmergencyLog "⚠️ SetCompatibleTextRenderingDefault already configured" "WARNING"
            }
            Write-EmergencyLog "✅ Application compatibility settings applied" "SUCCESS"
            $testForm = New-Object System.Windows.Forms.Form -ErrorAction Stop
            $testForm.Dispose()
            Write-EmergencyLog "✅ Windows Forms is functional" "SUCCESS"
            $script:RuntimeInfo.WinFormsAvailable = $true
            return $true
        }
        catch {
            Write-EmergencyLog "❌ Windows Forms not functional: $($_.Exception.Message)" "CRITICAL"
            $script:RuntimeInfo.WinFormsAvailable = $false
            return $false
        }
    }
    catch {
        Write-EmergencyLog "❌ Critical error initializing Windows Forms: $($_.Exception.Message)" "CRITICAL"
        $script:RuntimeInfo.WinFormsAvailable = $false
        return $false
    }
}
# Initialize Windows Forms and store result
$script:WindowsFormsAvailable = Initialize-WindowsForms
if (-not $script:WindowsFormsAvailable) {
    Write-EmergencyLog "═══════════════════════════════════════════════════════" "CRITICAL"
    Write-EmergencyLog "CRITICAL ERROR: Windows Forms is not available!" "CRITICAL"
    Write-EmergencyLog "This can happen in PowerShell Core 6+ environments." "CRITICAL"
    Write-EmergencyLog "═══════════════════════════════════════════════════════" "CRITICAL"
    # Provide user-friendly error message
    $errorMessage = @"
🚨 WINDOWS FORMS NOT AVAILABLE
RawrXD requires Windows Forms to create the graphical interface.
SOLUTIONS:
1. Use Windows PowerShell 5.1 instead of PowerShell Core:
   - Run: powershell.exe (not pwsh.exe)
2. For PowerShell 7+, install Microsoft.WindowsDesktop.App:
   - Run: winget install Microsoft.DotNet.DesktopRuntime.8
3. Alternative: Use PowerShell ISE for guaranteed compatibility
4. Check if you're running in a restricted environment (like some CI/CD systems)
PowerShell Version: $($PSVersionTable.PSVersion)
Platform: $(if ($PSVersionTable.Platform) { $PSVersionTable.Platform } else { 'Windows' })
"@
    Write-Host $errorMessage -ForegroundColor Red
    # Try to continue in console-only mode
    Write-EmergencyLog "Attempting to continue in console-only mode..." "WARNING"
}
# ============================================
# CONSOLE-ONLY MODE (FALLBACK FOR NO WINDOWS FORMS)
# ============================================
function Start-ConsoleMode {
    param()
    try {
        Write-Host @"
╔════════════════════════════════════════════════════════════════╗
║                       🔧 RAWRXD CONSOLE MODE                  ║
║            AI-Powered Text Editor - Command Line Interface    ║
╚════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan
        Write-Host ""
        Write-Host "🚨 GUI Mode not available. Running in Console Mode." -ForegroundColor Yellow
        Write-Host "⚡ Core AI and agent functionality is still available!" -ForegroundColor Green
        Write-Host ""
        # Initialize core systems without GUI
        Write-Host "🔧 Initializing core systems..." -ForegroundColor White
        # Initialize AI/Ollama connection
        if (Test-NetConnection -ComputerName localhost -Port 11434 -InformationLevel Quiet -ErrorAction SilentlyContinue) {
            Write-Host "✅ Ollama service detected on localhost:11434" -ForegroundColor Green
            $script:ConsoleOllamaAvailable = $true
        }
        else {
            Write-Host "⚠️ Ollama service not detected" -ForegroundColor Yellow
            $script:ConsoleOllamaAvailable = $false
        }
        # Show available commands
        Show-ConsoleHelp
        # Start interactive console loop
        Start-ConsoleInteractiveMode
    }
    catch {
        Write-EmergencyLog "❌ Error in console mode: $($_.Exception.Message)" "ERROR"
        Write-Host "❌ Error starting console mode: $($_.Exception.Message)" -ForegroundColor Red
    }
}
function Show-ConsoleHelp {
    param()
    Write-Host @"
📋 AVAILABLE COMMANDS:
═══════════════════════════════════════════════════════════════════
🤖 AI COMMANDS:
   /ask <question>          - Ask AI a question (requires Ollama)
   /chat <message>          - Start AI chat conversation
   /models                  - List available AI models
   /status                  - Show AI service status
📁 FILE COMMANDS:
   /open <file>             - Open file for editing
   /save <file> <content>   - Save content to file
   /list [path]             - List files and directories
   /pwd                     - Show current directory
   /cd <path>               - Change directory
🔍 SEARCH & ANALYSIS:
   /search <term> [path]    - Search for text in files
   /analyze <file>          - Analyze file for insights
   /errors                  - Show error log dashboard
⚙️ SYSTEM COMMANDS:
   /settings                - Show current settings
   /logs                    - View system logs
   /help                    - Show this help message
   /exit                    - Exit RawrXD
═══════════════════════════════════════════════════════════════════
Type a command to get started, or /help for more information.
"@ -ForegroundColor Gray
}
function Start-ConsoleInteractiveMode {
    param()
    $script:ConsoleRunning = $true
    $script:ConsoleHistory = @()
    Write-Host ""
    Write-Host "🚀 Console mode ready! Type /help for commands or /exit to quit." -ForegroundColor Green
    Write-Host ""
    while ($script:ConsoleRunning) {
        try {
            # Show prompt
            Write-Host "RawrXD> " -NoNewline -ForegroundColor Cyan
            # Get user input
            $userInput = Read-Host
            if (-not [string]::IsNullOrWhiteSpace($userInput)) {
                $script:ConsoleHistory += $userInput
                Process-ConsoleCommand $userInput.Trim()
            }
        }
        catch {
            Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-EmergencyLog "Console command error: $($_.Exception.Message)" "ERROR"
        }
    }
}
function Process-ConsoleCommand {
    param([string]$Command)
    # Parse command and arguments
    $parts = $Command -split '\s+', 2
    $cmd = $parts[0].ToLower()
    $arguments = if ($parts.Length -gt 1) { $parts[1] } else { "" }
    switch ($cmd) {
        "/help" {
            Show-ConsoleHelp
        }
        "/exit" {
            Write-Host "👋 Goodbye!" -ForegroundColor Green
            $script:ConsoleRunning = $false
        }
        "/status" {
            Write-Host "📊 RAWRXD STATUS:" -ForegroundColor Cyan
            Write-Host "   PowerShell: $($PSVersionTable.PSVersion)" -ForegroundColor Gray
            Write-Host "   Platform: $($PSVersionTable.Platform)" -ForegroundColor Gray
            Write-Host "   Windows Forms: $(if ($script:WindowsFormsAvailable) { '✅ Available' } else { '❌ Not Available' })" -ForegroundColor Gray
            Write-Host "   Ollama: $(if ($script:ConsoleOllamaAvailable) { '✅ Available' } else { '❌ Not Available' })" -ForegroundColor Gray
            Write-Host "   Session ID: $($script:CurrentSession.SessionId)" -ForegroundColor Gray
        }
        "/ask" {
            if (-not $script:ConsoleOllamaAvailable) {
                Write-Host "❌ Ollama service not available. Please start Ollama first." -ForegroundColor Red
                return
            }
            if ([string]::IsNullOrWhiteSpace($arguments)) {
                Write-Host "❌ Please provide a question. Usage: /ask <your question>" -ForegroundColor Red
                return
            }
            Write-Host "🤖 Asking AI: $arguments" -ForegroundColor Yellow
            try {
                $response = Send-OllamaRequest $arguments $OllamaModel
                Write-Host "🤖 AI Response:" -ForegroundColor Green
                Write-Host $response -ForegroundColor White
            }
            catch {
                Write-Host "❌ Error getting AI response: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
        "/models" {
            if (-not $script:ConsoleOllamaAvailable) {
                Write-Host "❌ Ollama service not available" -ForegroundColor Red
                return
            }
            try {
                Write-Host "🧠 Available AI Models:" -ForegroundColor Cyan
                $models = Get-AvailableModels
                if ($models.Count -gt 0) {
                    foreach ($model in $models) {
                        $marker = if ($model -eq $OllamaModel) { "👉" } else { "  " }
                        Write-Host "$marker $model" -ForegroundColor Gray
                    }
                }
                else {
                    Write-Host "   No models found. Install models with: ollama pull <model>" -ForegroundColor Yellow
                }
            }
            catch {
                Write-Host "❌ Error listing models: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
        "/pwd" {
            Write-Host "📂 Current Directory: $(Get-Location)" -ForegroundColor Gray
        }
        "/list" {
            $path = if ([string]::IsNullOrWhiteSpace($arguments)) { Get-Location } else { $arguments }
            try {
                Write-Host "📁 Contents of: $path" -ForegroundColor Cyan
                Get-ChildItem $path | ForEach-Object {
                    $icon = if ($_.PSIsContainer) { "📁" } else { "📄" }
                    $size = if (-not $_.PSIsContainer) { " ($($_.Length) bytes)" } else { "" }
                    Write-Host "   $icon $($_.Name)$size" -ForegroundColor Gray
                }
            }
            catch {
                Write-Host "❌ Error listing directory: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
        "/errors" {
            try {
                $report = Get-AIErrorDashboard
                Write-Host $report -ForegroundColor Gray
            }
            catch {
                Write-Host "❌ Error generating error dashboard: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
        "/logs" {
            try {
                if (Test-Path $script:StartupLogFile) {
                    Write-Host "📋 Recent log entries:" -ForegroundColor Cyan
                    Get-Content $script:StartupLogFile -Tail 20 | ForEach-Object {
                        Write-Host "   $_" -ForegroundColor Gray
                    }
                }
                else {
                    Write-Host "❌ Log file not found: $script:StartupLogFile" -ForegroundColor Red
                }
            }
            catch {
                Write-Host "❌ Error reading logs: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
        "/settings" {
            Write-Host "⚙️ Current Settings:" -ForegroundColor Cyan
            Write-Host "   Ollama Model: $OllamaModel" -ForegroundColor Gray
            Write-Host "   Emergency Log: $script:EmergencyLogPath" -ForegroundColor Gray
            Write-Host "   Session Timeout: $($script:SecurityConfig.SessionTimeout) seconds" -ForegroundColor Gray
            Write-Host "   Debug Mode: $($global:settings.DebugMode)" -ForegroundColor Gray
        }
        default {
            if ($Command.StartsWith("/")) {
                Write-Host "❌ Unknown command: $cmd" -ForegroundColor Red
                Write-Host "   Type /help for available commands" -ForegroundColor Gray
            }
            else {
                # Treat as AI chat if Ollama is available
                if ($script:ConsoleOllamaAvailable) {
                    Write-Host "🤖 Chatting with AI..." -ForegroundColor Yellow
                    try {
                        $response = Send-OllamaRequest $Command $OllamaModel
                        Write-Host "🤖 AI: $response" -ForegroundColor Green
                    }
                    catch {
                        Write-Host "❌ AI Error: $($_.Exception.Message)" -ForegroundColor Red
                    }
                }
                else {
                    Write-Host "❌ Unknown command. Type /help for available commands" -ForegroundColor Red
                }
            }
        }
    }
    Write-Host ""  # Add spacing between commands
}
# ============================================
# SECURITY & STEALTH MODULE
# ============================================
# Enhanced encryption using AES256
Add-Type @"
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
public static class StealthCrypto {
    private static readonly byte[] DefaultKey = new byte[] {
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };
    public static string Encrypt(string data, byte[] key = null) {
        if (string.IsNullOrEmpty(data)) return data;
        key = key != null ? key : DefaultKey;
        using (var aes = Aes.Create()) {
            aes.Key = key;
            aes.GenerateIV();
            using (var encryptor = aes.CreateEncryptor())
            using (var ms = new MemoryStream())
            using (var cs = new CryptoStream(ms, encryptor, CryptoStreamMode.Write)) {
                var dataBytes = Encoding.UTF8.GetBytes(data);
                cs.Write(dataBytes, 0, dataBytes.Length);
                cs.FlushFinalBlock();
                var result = new byte[aes.IV.Length + ms.ToArray().Length];
                Array.Copy(aes.IV, 0, result, 0, aes.IV.Length);
                Array.Copy(ms.ToArray(), 0, result, aes.IV.Length, ms.ToArray().Length);
                return Convert.ToBase64String(result);
            }
        }
    }
    public static string Decrypt(string encryptedData, byte[] key = null) {
        if (string.IsNullOrEmpty(encryptedData)) return encryptedData;
        key = key != null ? key : DefaultKey;
        try {
            var encryptedBytes = Convert.FromBase64String(encryptedData);
            using (var aes = Aes.Create()) {
                aes.Key = key;
                var iv = new byte[16];
                var encrypted = new byte[encryptedBytes.Length - 16];
                Array.Copy(encryptedBytes, 0, iv, 0, 16);
                Array.Copy(encryptedBytes, 16, encrypted, 0, encrypted.Length);
                aes.IV = iv;
                using (var decryptor = aes.CreateDecryptor())
                using (var ms = new MemoryStream(encrypted))
                using (var cs = new CryptoStream(ms, decryptor, CryptoStreamMode.Read))
                using (var sr = new StreamReader(cs)) {
                    return sr.ReadToEnd();
                }
            }
        }
        catch {
            return encryptedData; // Return original if decryption fails
        }
    }
    public static string Hash(string data) {
        using (var sha256 = SHA256.Create()) {
            var hash = sha256.ComputeHash(Encoding.UTF8.GetBytes(data));
            return Convert.ToBase64String(hash);
        }
    }
    public static byte[] GenerateKey() {
        using (var rng = RandomNumberGenerator.Create()) {
            var key = new byte[32]; // 256-bit key
            rng.GetBytes(key);
            return key;
        }
    }
}
"@
# Global security configuration
$script:SecurityConfig = @{
    EncryptSensitiveData   = $true
    ValidateAllInputs      = $true
    SecureConnections      = $true
    StealthMode            = $false
    AuthenticationRequired = $false
    SessionTimeout         = 3600  # 1 hour
    MaxLoginAttempts       = 3
    LogSecurityEvents      = $true
    AntiForensics          = $false
    ProcessHiding          = $false
}
# Session management
$script:CurrentSession = @{
    UserId          = $null
    SessionId       = [System.Guid]::NewGuid().ToString()
    StartTime       = Get-Date
    LastActivity    = Get-Date
    IsAuthenticated = $false
    LoginAttempts   = 0
    SecurityLevel   = "Standard"
    EncryptionKey   = [StealthCrypto]::GenerateKey()
}
# Agentic command state management
$script:PendingDelete = $null
# Security event logging
$script:SecurityLog = @()
function Write-SecurityLog {
    param(
        [string]$EventName,
        [string]$Level = "INFO",
        [string]$Details = ""
    )
    if (-not $script:SecurityConfig.LogSecurityEvents) { return }
    $logEntry = @{
        Timestamp   = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        SessionId   = $script:CurrentSession.SessionId
        Event       = $EventName
        Level       = $Level
        Details     = $Details
        ProcessId   = $PID
        UserContext = [Environment]::UserName
    }
    $script:SecurityLog += $logEntry
    # Write to console if debug mode
    if ($script:DebugMode) {
        Write-Host "[$Level] Security: $EventName" -ForegroundColor $(
            switch ($Level) {
                "ERROR" { "Red" }
                "WARNING" { "Yellow" }
                "SUCCESS" { "Green" }
                default { "Cyan" }
            }
        )
    }
}
function Protect-SensitiveString {
    param([string]$Data)
    if (-not $script:SecurityConfig.EncryptSensitiveData -or [string]::IsNullOrEmpty($Data)) {
        return $Data
    }
    try {
        $encrypted = [StealthCrypto]::Encrypt($Data, $script:CurrentSession.EncryptionKey)
        Write-SecurityLog "Data encrypted" "DEBUG" "Length: $($Data.Length)"
        return $encrypted
    }
    catch {
        Write-SecurityLog "Encryption failed" "ERROR" $_.Exception.Message
        return $Data
    }
}
function Unprotect-SensitiveString {
    param([string]$EncryptedData)
    if (-not $script:SecurityConfig.EncryptSensitiveData -or [string]::IsNullOrEmpty($EncryptedData)) {
        return $EncryptedData
    }
    try {
        $decrypted = [StealthCrypto]::Decrypt($EncryptedData, $script:CurrentSession.EncryptionKey)
        Write-SecurityLog "Data decrypted" "DEBUG" "Success"
        return $decrypted
    }
    catch {
        Write-SecurityLog "Decryption failed" "ERROR" $_.Exception.Message
        return $EncryptedData
    }
}
function Test-InputSafety {
    param([string]$InputText, [string]$Type = "General")
    if (-not $script:SecurityConfig.ValidateAllInputs) { return $true }
    # Basic validation patterns
    $dangerousPatterns = @(
        '(?i)(script|javascript|vbscript):', # Script injection
        '(?i)<[^>]*on\w+\s*=', # Event handlers
        '(?i)(exec|eval|system|cmd|powershell|bash)', # Command execution
        '[;&|`$(){}[\]\\]', # Shell metacharacters (relaxed for normal text)
        '(?i)(select|insert|update|delete|drop|create|alter)\s+', # SQL injection
        '\.\./|\.\.\\', # Path traversal
        '(?i)(http|https|ftp|file)://' # URLs (may be suspicious in certain contexts)
    )
    foreach ($pattern in $dangerousPatterns) {
        if ($InputText -match $pattern) {
            Write-SecurityLog "Potentially dangerous input detected" "WARNING" "Type: $Type, Pattern: $pattern"
            return $false
        }
    }
    return $true
}
function Enable-StealthMode {
    param([bool]$Enable = $true)
    $script:SecurityConfig.StealthMode = $Enable
    if ($Enable) {
        Write-SecurityLog "Stealth mode enabled" "INFO"
        # Minimize resource footprint
        [System.GC]::Collect()
        [System.GC]::WaitForPendingFinalizers()
        # Hide from process list (basic obfuscation)
        if ($script:SecurityConfig.ProcessHiding) {
            try {
                $process = Get-Process -Id $PID
                $process.ProcessName = "svchost"  # This doesn't actually work but shows intent
            }
            catch { }
        }
        # Enable anti-forensics measures
        if ($script:SecurityConfig.AntiForensics) {
            # Clear PowerShell history
            if (Test-Path "$env:APPDATA\Microsoft\Windows\PowerShell\PSReadline\ConsoleHost_history.txt") {
                Clear-Content "$env:APPDATA\Microsoft\Windows\PowerShell\PSReadline\ConsoleHost_history.txt" -Force -ErrorAction SilentlyContinue
            }
        }
    }
    else {
        Write-SecurityLog "Stealth mode disabled" "INFO"
    }
}
function Test-SessionSecurity {
    $currentTime = Get-Date
    $sessionDuration = ($currentTime - $script:CurrentSession.StartTime).TotalSeconds
    # Check session timeout
    if ($script:SecurityConfig.AuthenticationRequired -and $sessionDuration -gt $script:SecurityConfig.SessionTimeout) {
        Write-SecurityLog "Session timeout exceeded" "WARNING" "Duration: $sessionDuration seconds"
        return $false
    }
    return $true
}
# ============================================
# MISSING CRITICAL FUNCTIONS - IMPLEMENTATION
# ============================================
function Write-ErrorLog {
    param(
        [Parameter(Mandatory = $true, Position = 0)]
        [Alias("Message")]
        [string]$ErrorMessage,
        [Parameter(Mandatory = $false, Position = 1)]
        [Alias("Category")]
        [ValidateSet("OPERATION", "SECURITY", "NETWORK", "FILE", "UI", "AI", "SYSTEM")]
        [string]$ErrorCategory = "SYSTEM",
        [Parameter(Mandatory = $false, Position = 2)]
        [ValidateSet("LOW", "MEDIUM", "HIGH", "CRITICAL")]
        [string]$Severity = "MEDIUM",
        [Parameter(Mandatory = $false, Position = 3)]
        [string]$SourceFunction = "",
        [Parameter(Mandatory = $false)]
        [hashtable]$AdditionalData = @{},
        [Parameter(Mandatory = $false)]
        [bool]$ShowToUser = $true,
        # Agentic AI Error Logging Parameters
        [Parameter(Mandatory = $false)]
        [bool]$IsAIRelated = $false,
        [Parameter(Mandatory = $false)]
        [string]$AgentContext = "",
        [Parameter(Mandatory = $false)]
        [string]$AIModel = "",
        [Parameter(Mandatory = $false)]
        [hashtable]$AIMetrics = @{}
    )
    try {
        # Enhanced error data with AI context
        $enhancedData = $AdditionalData.Clone()
        if ($IsAIRelated) {
            $enhancedData["IsAIRelated"] = $true
            $enhancedData["AgentContext"] = $AgentContext
            $enhancedData["AIModel"] = $AIModel
            $enhancedData["Timestamp"] = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
            if ($AIMetrics.Count -gt 0) {
                $enhancedData["AIMetrics"] = $AIMetrics
            }
        }
        # Use existing Write-StartupLog for immediate logging
        $logMessage = if ($IsAIRelated) {
            "[AI-$ErrorCategory - $Severity] $ErrorMessage"
        }
        else {
            "[$ErrorCategory - $Severity] $ErrorMessage"
        }
        Write-StartupLog $logMessage "ERROR"
        # Agentic AI specific logging
        if ($IsAIRelated) {
            Write-AgenticErrorLog -ErrorMessage $ErrorMessage -ErrorCategory $ErrorCategory -Severity $Severity -AgentContext $AgentContext -AIModel $AIModel -AIMetrics $AIMetrics
        }
        # Also call the comprehensive error reporting if available
        if (Get-Command Write-ErrorReport -ErrorAction SilentlyContinue) {
            Write-ErrorReport -ErrorMessage $ErrorMessage -ErrorCategory $ErrorCategory -Severity $Severity -SourceFunction $SourceFunction -AdditionalData $enhancedData -ShowToUser $ShowToUser
        }
        # Log to security system with AI context
        $securityContext = "Category: $ErrorCategory, Severity: $Severity, Source: $SourceFunction"
        if ($IsAIRelated) {
            $securityContext += ", AI_Context: $AgentContext, Model: $AIModel"
        }
        Write-SecurityLog "Error logged: $ErrorMessage" "ERROR" $securityContext
        # Real-time AI error notification to chat if available and AI-related
        if ($IsAIRelated -and $script:chatBox -and $ShowToUser) {
            $aiErrorNotification = "🤖 AI Agent Error [$Severity]: $ErrorMessage"
            if ($AgentContext) {
                $aiErrorNotification += "`nContext: $AgentContext"
            }
            if ($AIModel) {
                $aiErrorNotification += "`nModel: $AIModel"
            }
            $script:chatBox.AppendText("Agent > $aiErrorNotification`r`n`r`n")
        }
    }
    catch {
        # Fallback error logging
        Write-StartupLog "ERROR: Failed to log error - $($_.Exception.Message)" "ERROR"
        Write-Host "ERROR: Failed to log error - $($_.Exception.Message)" -ForegroundColor Red
    }
}
# ============================================
# ADVANCED TELEMETRY & INSIGHTS SYSTEM
# ============================================
# Telemetry configuration
$script:TelemetryConfig = @{
    EnableTelemetry        = $true
    EnableInsights         = $true
    RealTimeAnalysis       = $true
    PerformanceTracking    = $true
    UserBehaviorAnalysis   = $true
    NotificationThresholds = @{
        ErrorRate    = 0.05  # 5% error rate threshold
        ResponseTime = 5000  # 5 seconds
        MemoryUsage  = 512  # 512MB
        CPUUsage     = 80  # 80%
    }
    InsightsRetentionDays  = 30
    ExportPath             = Join-Path $env:TEMP "RawrXD_Insights"
}
# Global telemetry storage
$script:TelemetryData = @{
    SessionMetrics     = @{
        StartTime        = Get-Date
        EventCount       = 0
        ErrorCount       = 0
        WarningCount     = 0
        UserInteractions = 0
        AIRequests       = 0
        FileOperations   = 0
        NetworkRequests  = 0
    }
    PerformanceMetrics = @{
        MemoryUsage   = @()
        CPUUsage      = @()
        ResponseTimes = @()
        DiskIO        = @()
    }
    UserBehavior       = @{
        FeatureUsage       = @{}
        NavigationPatterns = @()
        ErrorPatterns      = @()
        SessionDuration    = @()
    }
    InsightsHistory    = @()
}
# Advanced insights function based on BigDaddyG's recommendation
function Update-Insights {
    param(
        [Parameter(Mandatory)]
        [string]$EventName,
        [Parameter(Mandatory)]
        [string]$EventData,
        [string]$EventCategory = "General",
        [hashtable]$Metadata = @{}
    )
    if (-not $script:TelemetryConfig.EnableInsights) { return }
    try {
        $timestamp = Get-Date
        $insight = @{
            Timestamp = $timestamp
            EventName = $EventName
            EventData = $EventData
            Category  = $EventCategory
            Metadata  = $Metadata
            SessionId = $script:CurrentSession.SessionId
        }
        # Store insight
        $script:TelemetryData.InsightsHistory += $insight
        $script:TelemetryData.SessionMetrics.EventCount++
        # Real-time analysis if enabled
        if ($script:TelemetryConfig.RealTimeAnalysis) {
            Analyze-RealTimeInsights -Insight $insight
        }
        # Log to console if debug mode
        if ($script:DebugMode) {
            Write-StartupLog "🔍 INSIGHT: [$EventCategory] $EventName - $EventData" "INFO"
        }
        # Performance tracking
        if ($script:TelemetryConfig.PerformanceTracking) {
            Update-PerformanceMetrics
        }
        # Check notification thresholds
        Check-InsightThresholds
        # Clean up old insights
        Cleanup-OldInsights
        # Send email notification if configured
        if ($script:ErrorNotificationConfig.EnableEmailNotifications -and $EventCategory -eq "ERROR") {
            Send-InsightEmailNotification -EventName $EventName -EventData $EventData -Category $EventCategory
        }
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to update insights: $($_.Exception.Message)" -ErrorCategory "TELEMETRY" -Severity "MEDIUM" -SourceFunction "Update-Insights"
    }
}
# Enhanced email notification for insights
function Send-InsightEmailNotification {
    param(
        [string]$EventName,
        [string]$EventData,
        [string]$Category
    )
    try {
        $emailConfig = $script:ErrorNotificationConfig.EmailSettings
        if (-not $emailConfig) { return }
        $subject = "RawrXD Application Insight: $Category - $EventName"
        $body = @"
RawrXD Application Insight Report
Event: $EventName
Category: $Category
Data: $EventData
Timestamp: $(Get-Date)
Session ID: $($script:CurrentSession.SessionId)
Machine: $env:COMPUTERNAME
User: $env:USERNAME
Performance Metrics:
- Memory Usage: $(if ($script:TelemetryData.PerformanceMetrics.MemoryUsage -and $script:TelemetryData.PerformanceMetrics.MemoryUsage.Count -gt 0) { $script:TelemetryData.PerformanceMetrics.MemoryUsage[-1].Value } else { "N/A" })MB
- Event Count: $($script:TelemetryData.SessionMetrics.EventCount)
- Error Count: $($script:TelemetryData.SessionMetrics.ErrorCount)
This notification was sent automatically by RawrXD's telemetry system.
"@
        $message = New-Object System.Net.Mail.MailMessage
        $message.From = $emailConfig.FromAddress
        $message.To.Add($emailConfig.ToAddress)
        $message.Subject = $subject
        $message.Body = $body
        $smtpClient = New-Object System.Net.Mail.SmtpClient($emailConfig.SmtpServer, $emailConfig.Port)
        if ($emailConfig.UseSSL) { $smtpClient.EnableSsl = $true }
        if ($emailConfig.Credentials) {
            $smtpClient.Credentials = $emailConfig.Credentials
        }
        $smtpClient.Send($message)
        Write-StartupLog "📧 Insight email notification sent successfully" "SUCCESS"
    }
    catch {
        Write-Warning "Failed to send insight email notification: $_"
    }
}
# Real-time insights analysis based on BigDaddyG's recommendation
function Analyze-RealTimeInsights {
    param($Insight)
    try {
        # Pattern detection
        $recentInsights = $script:TelemetryData.InsightsHistory | Where-Object {
            $_.Timestamp -gt (Get-Date).AddMinutes(-5)
        }
        # Error pattern detection
        if ($Insight.Category -eq "ERROR") {
            $script:TelemetryData.SessionMetrics.ErrorCount++
            $recentErrors = @($recentInsights | Where-Object { $_.Category -eq "ERROR" })
            $recentErrorCount = if ($recentErrors) { $recentErrors.Count } else { 0 }
            if ($recentErrorCount -gt 3) {
                Send-AlertNotification -Type "ErrorSpike" -Message "High error rate detected: $recentErrorCount errors in 5 minutes"
                # Track error patterns
                $script:TelemetryData.UserBehavior.ErrorPatterns += @{
                    Timestamp  = Get-Date
                    ErrorCount = $recentErrorCount
                    Pattern    = "ErrorSpike"
                }
            }
        }
        # Performance pattern detection
        if ($Insight.EventName -eq "SlowResponse") {
            $slowResponses = @($recentInsights | Where-Object { $_.EventName -eq "SlowResponse" })
            if ($slowResponses.Count -gt 2) {
                Send-AlertNotification -Type "PerformanceDegradation" -Message "Performance degradation detected: Multiple slow responses"
            }
        }
        # User behavior analysis
        if ($Insight.Category -eq "UserInteraction") {
            $script:TelemetryData.SessionMetrics.UserInteractions++
            Analyze-UserBehavior -Insight $Insight
        }
        # AI request tracking
        if ($Insight.Category -eq "AI") {
            $script:TelemetryData.SessionMetrics.AIRequests++
        }
        # File operation tracking
        if ($Insight.Category -eq "FileSystem") {
            $script:TelemetryData.SessionMetrics.FileOperations++
        }
        # Network request tracking
        if ($Insight.Category -eq "Network") {
            $script:TelemetryData.SessionMetrics.NetworkRequests++
        }
    }
    catch {
        Write-Warning "Real-time analysis failed: $_"
    }
}
# Performance metrics collection
function Update-PerformanceMetrics {
    try {
        $process = Get-Process -Id $PID -ErrorAction SilentlyContinue
        if ($process) {
            # Memory usage
            $memoryMB = [math]::Round($process.WorkingSet64 / 1MB, 2)
            $script:TelemetryData.PerformanceMetrics.MemoryUsage += @{
                Timestamp = Get-Date
                Value     = $memoryMB
            }
            # CPU usage (approximation)
            $cpuTime = $process.TotalProcessorTime.TotalMilliseconds
            $script:TelemetryData.PerformanceMetrics.CPUUsage += @{
                Timestamp = Get-Date
                Value     = $cpuTime
            }
            # Disk I/O (if available)
            try {
                $diskCounters = Get-Counter "\Process($($process.ProcessName)*)\IO Data Bytes/sec" -ErrorAction SilentlyContinue
                if ($diskCounters) {
                    $diskIO = $diskCounters.CounterSamples[0].CookedValue
                    $script:TelemetryData.PerformanceMetrics.DiskIO += @{
                        Timestamp = Get-Date
                        Value     = $diskIO
                    }
                }
            }
            catch {
                # Silent fail for disk I/O metrics
            }
            # Trim old metrics (keep last 100 entries)
            if ($script:TelemetryData.PerformanceMetrics.MemoryUsage -and $script:TelemetryData.PerformanceMetrics.MemoryUsage.Count -gt 100) {
                $script:TelemetryData.PerformanceMetrics.MemoryUsage = $script:TelemetryData.PerformanceMetrics.MemoryUsage[-100..-1]
                if ($script:TelemetryData.PerformanceMetrics.CPUUsage) {
                    $script:TelemetryData.PerformanceMetrics.CPUUsage = $script:TelemetryData.PerformanceMetrics.CPUUsage[-100..-1]
                }
                if ($script:TelemetryData.PerformanceMetrics.DiskIO -and $script:TelemetryData.PerformanceMetrics.DiskIO.Count -gt 100) {
                    $script:TelemetryData.PerformanceMetrics.DiskIO = $script:TelemetryData.PerformanceMetrics.DiskIO[-100..-1]
                }
            }
        }
    }
    catch {
        # Silent fail for performance metrics
    }
}
# Threshold checking and alerts
function Check-InsightThresholds {
    try {
        # Check if telemetry data and config are properly initialized
        if (-not $script:TelemetryConfig -or -not $script:TelemetryConfig.NotificationThresholds) {
            Write-StartupLog "Telemetry configuration not initialized, skipping threshold checks" "DEBUG"
            return
        }
        if (-not $script:TelemetryData -or -not $script:TelemetryData.PerformanceMetrics) {
            Write-StartupLog "Telemetry data not initialized, skipping threshold checks" "DEBUG"
            return
        }
        $thresholds = $script:TelemetryConfig.NotificationThresholds
        # Check memory usage with proper null checking
        if ($script:TelemetryData.PerformanceMetrics.MemoryUsage -and @($script:TelemetryData.PerformanceMetrics.MemoryUsage).Count -gt 0) {
            $latestMemory = $script:TelemetryData.PerformanceMetrics.MemoryUsage | Select-Object -Last 1
            if ($latestMemory -and $latestMemory.Value -gt $thresholds.MemoryUsage) {
                Send-AlertNotification -Type "MemoryUsage" -Message "High memory usage: $($latestMemory.Value)MB" -Severity "HIGH"
            }
        }
        # Check error rate with proper null checking
        if ($script:TelemetryData.InsightsHistory -and @($script:TelemetryData.InsightsHistory).Count -gt 0) {
            $recentInsights = @($script:TelemetryData.InsightsHistory | Where-Object {
                    $_.Timestamp -gt (Get-Date).AddMinutes(-10)
                })
            if ($recentInsights -and $recentInsights.Count -gt 0) {
                $errorInsights = @($recentInsights | Where-Object { $_.Category -eq "ERROR" })
                $errorCount = if ($errorInsights) { $errorInsights.Count } else { 0 }
                $errorRate = $errorCount / $recentInsights.Count
                if ($errorRate -gt $thresholds.ErrorRate) {
                    Send-AlertNotification -Type "ErrorRate" -Message "High error rate: $([math]::Round($errorRate * 100, 1))%" -Severity "HIGH"
                }
            }
        }
        # Check response times with proper null checking
        if ($script:TelemetryData.PerformanceMetrics.ResponseTimes -and @($script:TelemetryData.PerformanceMetrics.ResponseTimes).Count -gt 0) {
            $recentResponseTimes = @($script:TelemetryData.PerformanceMetrics.ResponseTimes | Where-Object {
                    $_.Timestamp -gt (Get-Date).AddMinutes(-5)
                })
            if ($recentResponseTimes -and $recentResponseTimes.Count -gt 0) {
                $avgResponseTime = ($recentResponseTimes | Measure-Object -Property Value -Average).Average
                if ($avgResponseTime -gt $thresholds.ResponseTime) {
                    Send-AlertNotification -Type "ResponseTime" -Message "Slow response time: $([math]::Round($avgResponseTime, 0))ms" -Severity "MEDIUM"
                }
            }
        }
    }
    catch {
        Write-StartupLog "Threshold checking failed: $($_.Exception.Message)" "DEBUG"
    }
}
# User behavior analysis
function Analyze-UserBehavior {
    param($Insight)
    try {
        # Track feature usage
        if ($Insight.Metadata.ContainsKey("Feature")) {
            $feature = $Insight.Metadata.Feature
            if (-not $script:TelemetryData.UserBehavior.FeatureUsage.ContainsKey($feature)) {
                $script:TelemetryData.UserBehavior.FeatureUsage[$feature] = 0
            }
            $script:TelemetryData.UserBehavior.FeatureUsage[$feature]++
        }
        # Track navigation patterns
        if ($Insight.EventName -eq "Navigation") {
            $script:TelemetryData.UserBehavior.NavigationPatterns += @{
                Timestamp = $Insight.Timestamp
                Target    = $Insight.EventData
                Source    = $Insight.Metadata.Source
            }
        }
        # Generate insights based on usage patterns
        if ($script:TelemetryData.UserBehavior.FeatureUsage -and @($script:TelemetryData.UserBehavior.FeatureUsage).Count -gt 0) {
            $mostUsed = ($script:TelemetryData.UserBehavior.FeatureUsage.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Key
            Update-Insights -EventName "PopularFeature" -EventData $mostUsed -EventCategory "Analytics" -Metadata @{Type = "FeatureAnalysis" }
        }
    }
    catch {
        Write-Verbose "User behavior analysis failed: $_"
    }
}
# Alert notification system
function Send-AlertNotification {
    param(
        [string]$Type,
        [string]$Message,
        [string]$Severity = "WARNING"
    )
    try {
        # Log alert through error handler system
        Register-ErrorHandler -ErrorMessage $Message -ErrorCategory "ALERT" -Severity $Severity -SourceFunction "TelemetrySystem"
        # Show desktop notification if possible
        if ($script:TelemetryConfig.EnableTelemetry) {
            Show-DesktopNotification -Title "RawrXD Alert" -Message $Message -Type $Type
        }
        # Log to security log for critical alerts
        if ($Severity -eq "CRITICAL") {
            Write-SecurityLog "Critical alert triggered" "ERROR" "$Type - $Message"
        }
    }
    catch {
        Write-Warning "Failed to send alert notification: $_"
    }
}
# Desktop notification function
function Show-DesktopNotification {
    param(
        [string]$Title,
        [string]$Message,
        [string]$Type = "Info"
    )
    try {
        # Use PowerShell's built-in notification if available
        if (Get-Module BurntToast -ListAvailable -ErrorAction SilentlyContinue) {
            Import-Module BurntToast
            New-BurntToastNotification -Text $Title, $Message
        }
        else {
            # Fallback to basic notification form
            $notificationForm = New-Object Windows.Forms.Form
            $notificationForm.Text = $Title
            $notificationForm.Size = New-Object Drawing.Size(350, 120)
            $notificationForm.StartPosition = "Manual"
            $notificationForm.Location = New-Object Drawing.Point(([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width - 350), 50)
            $notificationForm.TopMost = $true
            $notificationForm.FormBorderStyle = "FixedDialog"
            $notificationForm.MaximizeBox = $false
            $notificationForm.MinimizeBox = $false
            $notificationForm.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
            $notificationForm.ForeColor = [System.Drawing.Color]::White
            $label = New-Object Windows.Forms.Label
            $label.Text = $Message
            $label.Size = New-Object Drawing.Size(330, 80)
            $label.Location = New-Object Drawing.Point(10, 10)
            $label.TextAlign = "MiddleCenter"
            $label.BackColor = [System.Drawing.Color]::Transparent
            $label.ForeColor = [System.Drawing.Color]::White
            $notificationForm.Controls.Add($label)
            # Auto-close after 4 seconds
            $timer = New-Object Windows.Forms.Timer
            $timer.Interval = 4000
            $timer.Add_Tick({
                    $notificationForm.Close()
                    $timer.Dispose()
                })
            $timer.Start()
            $notificationForm.Show()
        }
    }
    catch {
        # Silent fail for notifications
        Write-Verbose "Notification failed: $_"
    }
}
# Insights cleanup
function Cleanup-OldInsights {
    try {
        $cutoffDate = (Get-Date).AddDays(-$script:TelemetryConfig.InsightsRetentionDays)
        $script:TelemetryData.InsightsHistory = $script:TelemetryData.InsightsHistory | Where-Object {
            $_.Timestamp -gt $cutoffDate
        }
    }
    catch {
        Write-Verbose "Insights cleanup failed: $_"
    }
}
# Export insights report
function Export-InsightsReport {
    param(
        [string]$OutputPath = $script:TelemetryConfig.ExportPath
    )
    $cursorToken = Enter-CursorWaitState -Reason "Telemetry:Export" -Style "Wait"
    try {
        if (-not (Test-Path $OutputPath)) {
            New-Item -Path $OutputPath -ItemType Directory -Force | Out-Null
        }
        $reportPath = Join-Path $OutputPath "RawrXD_Insights_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
        $report = @{
            GeneratedAt        = Get-Date
            SessionMetrics     = $script:TelemetryData.SessionMetrics
            PerformanceMetrics = $script:TelemetryData.PerformanceMetrics
            UserBehavior       = $script:TelemetryData.UserBehavior
            InsightsHistory    = $script:TelemetryData.InsightsHistory[-50..-1]  # Last 50 insights
            Configuration      = $script:TelemetryConfig
            SystemInfo         = @{
                PSVersion      = $PSVersionTable.PSVersion
                Platform       = [System.Environment]::OSVersion.Platform
                ProcessorCount = [System.Environment]::ProcessorCount
                MachineName    = $env:COMPUTERNAME
                UserName       = $env:USERNAME
            }
        }
        $report | ConvertTo-Json -Depth 10 | Out-File -FilePath $reportPath -Encoding UTF8
        Write-StartupLog "📊 Insights report exported: $reportPath" "SUCCESS"
        Update-Insights -EventName "ReportExported" -EventData $reportPath -EventCategory "Analytics"
        return $reportPath
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to export insights report: $($_.Exception.Message)" -ErrorCategory "TELEMETRY" -Severity "MEDIUM" -SourceFunction "Export-InsightsReport"
        return $null
    }
    finally {
        if ($cursorToken) {
            Exit-CursorWaitState -Token $cursorToken
        }
    }
}
function Invoke-SecureCleanup {
    Write-SecurityLog "Performing secure cleanup" "INFO"
    # Clear sensitive variables
    if (Get-Variable -Name "OllamaAPIKey" -ErrorAction SilentlyContinue) {
        Remove-Variable -Name "OllamaAPIKey" -Scope Global -Force -ErrorAction SilentlyContinue
    }
    # Clear clipboard if it contains sensitive data
    try {
        [System.Windows.Forms.Clipboard]::Clear()
    }
    catch { }
    # Force garbage collection
    [System.GC]::Collect()
    [System.GC]::WaitForPendingFinalizers()
    [System.GC]::Collect()
    Write-SecurityLog "Secure cleanup completed" "SUCCESS"
}
function Show-AuthenticationDialog {
    $authForm = New-Object System.Windows.Forms.Form
    $authForm.Text = "RawrXD Authentication"
    $authForm.Size = New-Object System.Drawing.Size(400, 300)
    $authForm.StartPosition = "CenterScreen"
    $authForm.FormBorderStyle = "FixedDialog"
    $authForm.MaximizeBox = $false
    $authForm.MinimizeBox = $false
    $authForm.TopMost = $true
    $authForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $authForm.ForeColor = [System.Drawing.Color]::White
    # Title label
    $titleLabel = New-Object System.Windows.Forms.Label
    $titleLabel.Text = "🔒 Secure Access Required"
    $titleLabel.Size = New-Object System.Drawing.Size(360, 30)
    $titleLabel.Location = New-Object System.Drawing.Point(20, 20)
    $titleLabel.Font = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Bold)
    $titleLabel.ForeColor = [System.Drawing.Color]::Cyan
    $authForm.Controls.Add($titleLabel)
    # Username
    $usernameLabel = New-Object System.Windows.Forms.Label
    $usernameLabel.Text = "Username:"
    $usernameLabel.Size = New-Object System.Drawing.Size(100, 20)
    $usernameLabel.Location = New-Object System.Drawing.Point(20, 70)
    $authForm.Controls.Add($usernameLabel)
    $usernameBox = New-Object System.Windows.Forms.TextBox
    $usernameBox.Size = New-Object System.Drawing.Size(250, 25)
    $usernameBox.Location = New-Object System.Drawing.Point(120, 67)
    $usernameBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $usernameBox.ForeColor = [System.Drawing.Color]::White
    $usernameBox.Text = "admin"  # Default username
    $authForm.Controls.Add($usernameBox)
    # Password
    $passwordLabel = New-Object System.Windows.Forms.Label
    $passwordLabel.Text = "Password:"
    $passwordLabel.Size = New-Object System.Drawing.Size(100, 20)
    $passwordLabel.Location = New-Object System.Drawing.Point(20, 110)
    $authForm.Controls.Add($passwordLabel)
    $passwordBox = New-Object System.Windows.Forms.TextBox
    $passwordBox.Size = New-Object System.Drawing.Size(250, 25)
    $passwordBox.Location = New-Object System.Drawing.Point(120, 107)
    $passwordBox.PasswordChar = '*'
    $passwordBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $passwordBox.ForeColor = [System.Drawing.Color]::White
    $authForm.Controls.Add($passwordBox)
    # Security options
    $optionsGroup = New-Object System.Windows.Forms.GroupBox
    $optionsGroup.Text = "Security Options"
    $optionsGroup.Size = New-Object System.Drawing.Size(350, 80)
    $optionsGroup.Location = New-Object System.Drawing.Point(20, 150)
    $optionsGroup.ForeColor = [System.Drawing.Color]::LightGray
    $authForm.Controls.Add($optionsGroup)
    $stealthCheck = New-Object System.Windows.Forms.CheckBox
    $stealthCheck.Text = "Enable Stealth Mode"
    $stealthCheck.Size = New-Object System.Drawing.Size(150, 20)
    $stealthCheck.Location = New-Object System.Drawing.Point(10, 25)
    $stealthCheck.ForeColor = [System.Drawing.Color]::LightGray
    $optionsGroup.Controls.Add($stealthCheck)
    $httpsCheck = New-Object System.Windows.Forms.CheckBox
    $httpsCheck.Text = "Force HTTPS"
    $httpsCheck.Size = New-Object System.Drawing.Size(150, 20)
    $httpsCheck.Location = New-Object System.Drawing.Point(180, 25)
    $httpsCheck.ForeColor = [System.Drawing.Color]::LightGray
    $optionsGroup.Controls.Add($httpsCheck)
    $encryptCheck = New-Object System.Windows.Forms.CheckBox
    $encryptCheck.Text = "Encrypt All Data"
    $encryptCheck.Size = New-Object System.Drawing.Size(150, 20)
    $encryptCheck.Location = New-Object System.Drawing.Point(10, 50)
    $encryptCheck.Checked = $true  # Default to encrypted
    $encryptCheck.ForeColor = [System.Drawing.Color]::LightGray
    $optionsGroup.Controls.Add($encryptCheck)
    # Buttons
    $buttonPanel = New-Object System.Windows.Forms.Panel
    $buttonPanel.Size = New-Object System.Drawing.Size(350, 40)
    $buttonPanel.Location = New-Object System.Drawing.Point(20, 240)
    $authForm.Controls.Add($buttonPanel)
    $loginBtn = New-Object System.Windows.Forms.Button
    $loginBtn.Text = "Login"
    $loginBtn.Size = New-Object System.Drawing.Size(75, 30)
    $loginBtn.Location = New-Object System.Drawing.Point(190, 5)
    $loginBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $loginBtn.ForeColor = [System.Drawing.Color]::White
    $loginBtn.FlatStyle = "Flat"
    $buttonPanel.Controls.Add($loginBtn)
    $cancelBtn = New-Object System.Windows.Forms.Button
    $cancelBtn.Text = "Cancel"
    $cancelBtn.Size = New-Object System.Drawing.Size(75, 30)
    $cancelBtn.Location = New-Object System.Drawing.Point(275, 5)
    $cancelBtn.BackColor = [System.Drawing.Color]::FromArgb(120, 120, 120)
    $cancelBtn.ForeColor = [System.Drawing.Color]::White
    $cancelBtn.FlatStyle = "Flat"
    $buttonPanel.Controls.Add($cancelBtn)
    # Event handlers
    $script:authResult = $false
    $loginBtn.Add_Click({
            $username = $usernameBox.Text.Trim()
            $password = $passwordBox.Text
            # Simple authentication (in real scenario, use proper credential storage)
            $validCredentials = @{
                "admin" = "RawrXD2024!"
                "user"  = "secure123"
                "guest" = "guest"
            }
            if ($username -and $validCredentials.ContainsKey($username) -and $validCredentials[$username] -eq $password) {
                $script:CurrentSession.UserId = $username
                $script:SecurityConfig.StealthMode = $stealthCheck.Checked
                $script:UseHTTPS = $httpsCheck.Checked
                $script:SecurityConfig.EncryptSensitiveData = $encryptCheck.Checked
                if ($script:UseHTTPS) {
                    $script:OllamaAPIEndpoint = $OllamaSecureEndpoint
                }
                Write-SecurityLog "User '$username' authenticated successfully" "SUCCESS" "Options: Stealth=$($stealthCheck.Checked), HTTPS=$($httpsCheck.Checked), Encrypt=$($encryptCheck.Checked)"
                $script:authResult = $true
                $authForm.DialogResult = "OK"
                $authForm.Close()
            }
            else {
                $script:CurrentSession.LoginAttempts++
                Write-SecurityLog "Authentication failed for user '$username'" "ERROR" "Attempts: $($script:CurrentSession.LoginAttempts)"
                if ($script:CurrentSession.LoginAttempts -ge $script:SecurityConfig.MaxLoginAttempts) {
                    Write-StartupLog "Maximum login attempts exceeded. Application will exit." "CRITICAL"; Write-DevConsole "SECURITY: Maximum login attempts exceeded" "ERROR"
                    $authForm.DialogResult = "Cancel"
                    $authForm.Close()
                    return
                }
                Write-DevConsole "Invalid credentials. Please try again." "WARNING"
                $passwordBox.Clear()
                $passwordBox.Focus()
            }
        })
    $cancelBtn.Add_Click({
            Write-SecurityLog "Authentication cancelled by user" "WARNING"
            $authForm.DialogResult = "Cancel"
            $authForm.Close()
        })
    # Enter key for login
    $authForm.Add_KeyDown({
            if ($_.KeyCode -eq "Enter") {
                $loginBtn.PerformClick()
            }
            elseif ($_.KeyCode -eq "Escape") {
                $cancelBtn.PerformClick()
            }
        })
    $authForm.AcceptButton = $loginBtn
    $authForm.CancelButton = $cancelBtn
    # Show dialog
    $passwordBox.Focus()
    $result = $authForm.ShowDialog()
    return ($result -eq "OK" -and $script:authResult)
}
function Show-SecuritySettings {
    $settingsForm = New-Object System.Windows.Forms.Form
    $settingsForm.Text = "Security Settings"
    $settingsForm.Size = New-Object System.Drawing.Size(500, 600)
    $settingsForm.StartPosition = "CenterScreen"
    $settingsForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $settingsForm.ForeColor = [System.Drawing.Color]::White
    $settingsForm.FormBorderStyle = "FixedDialog"
    $settingsForm.MaximizeBox = $false
    $settingsForm.MinimizeBox = $false
    # Settings controls
    $y = 20
    foreach ($setting in $script:SecurityConfig.Keys) {
        $label = New-Object System.Windows.Forms.Label
        $label.Text = $setting + ":"
        $label.Size = New-Object System.Drawing.Size(200, 20)
        $label.Location = New-Object System.Drawing.Point(20, $y)
        $settingsForm.Controls.Add($label)
        if ($script:SecurityConfig[$setting] -is [bool]) {
            $checkbox = New-Object System.Windows.Forms.CheckBox
            $checkbox.Checked = $script:SecurityConfig[$setting]
            $checkbox.Size = New-Object System.Drawing.Size(20, 20)
            $checkbox.Location = New-Object System.Drawing.Point(230, $y)
            $checkbox.Tag = $setting
            $settingsForm.Controls.Add($checkbox)
        }
        elseif ($script:SecurityConfig[$setting] -is [int]) {
            $numericUpDown = New-Object System.Windows.Forms.NumericUpDown
            $numericUpDown.Size = New-Object System.Drawing.Size(100, 20)
            $numericUpDown.Location = New-Object System.Drawing.Point(230, $y)
            # Set appropriate min/max values based on the specific setting
            switch ($setting) {
                "SessionTimeout" {
                    $numericUpDown.Minimum = 60      # 1 minute minimum
                    $numericUpDown.Maximum = 86400   # 24 hours maximum
                }
                "MaxLoginAttempts" {
                    $numericUpDown.Minimum = 1       # At least 1 attempt
                    $numericUpDown.Maximum = 100     # Maximum 100 attempts
                }
                "MaxErrorsPerMinute" {
                    $numericUpDown.Minimum = 1       # At least 1 error per minute
                    $numericUpDown.Maximum = 1000    # Maximum 1000 errors per minute
                }
                "MaxFileSize" {
                    $numericUpDown.Minimum = 1       # 1 byte minimum
                    $numericUpDown.Maximum = 1073741824  # 1GB maximum (in bytes)
                }
                default {
                    # Generic safe range for unknown integer settings
                    $numericUpDown.Minimum = 0
                    $numericUpDown.Maximum = 999999
                }
            }
            # Clamp value into valid range before assigning to avoid ArgumentOutOfRangeException
            $intValue = [int]$script:SecurityConfig[$setting]
            if ($intValue -lt [int]$numericUpDown.Minimum) { $intValue = [int]$numericUpDown.Minimum }
            if ($intValue -gt [int]$numericUpDown.Maximum) { $intValue = [int]$numericUpDown.Maximum }
            $numericUpDown.Value = $intValue
            $numericUpDown.Tag = $setting
            $settingsForm.Controls.Add($numericUpDown)
        }
        $y += 35
    }
    # Save button
    $saveBtn = New-Object System.Windows.Forms.Button
    $saveBtn.Text = "Save Settings"
    $saveBtn.Size = New-Object System.Drawing.Size(100, 30)
    $saveBtn.Location = New-Object System.Drawing.Point(20, $y + 20)
    $saveBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $saveBtn.ForeColor = [System.Drawing.Color]::White
    $saveBtn.FlatStyle = "Flat"
    $settingsForm.Controls.Add($saveBtn)
    # Cancel button
    $cancelBtn = New-Object System.Windows.Forms.Button
    $cancelBtn.Text = "Cancel"
    $cancelBtn.Size = New-Object System.Drawing.Size(80, 30)
    $cancelBtn.Location = New-Object System.Drawing.Point(130, $y + 20)
    $cancelBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $cancelBtn.ForeColor = [System.Drawing.Color]::White
    $cancelBtn.FlatStyle = "Flat"
    $cancelBtn.Add_Click({ $settingsForm.Close() })
    $settingsForm.Controls.Add($cancelBtn)
    $saveBtn.Add_Click({
            foreach ($control in $settingsForm.Controls) {
                if ($control.Tag -and $script:SecurityConfig.ContainsKey($control.Tag)) {
                    if ($control -is [System.Windows.Forms.CheckBox]) {
                        $script:SecurityConfig[$control.Tag] = $control.Checked
                    }
                    elseif ($control -is [System.Windows.Forms.NumericUpDown]) {
                        $script:SecurityConfig[$control.Tag] = $control.Value
                    }
                }
            }
            # Save to file
            $configDir = Join-Path $env:APPDATA "RawrXD"
            if (-not (Test-Path $configDir)) {
                New-Item -ItemType Directory -Path $configDir -Force | Out-Null
            }
            $configPath = Join-Path $configDir "security.json"
            $script:SecurityConfig | ConvertTo-Json | Set-Content $configPath
            Write-SecurityLog "Security settings updated" "SUCCESS"
            $settingsForm.Close()
        })
    $settingsForm.ShowDialog()
}
# ============================================
# Environment Awareness
# ============================================
function Get-EnvironmentInfo {
    $env = @{
        OS                = if ($PSVersionTable.PSObject.Properties["OS"]) { $PSVersionTable.OS } else { [System.Environment]::OSVersion.VersionString }
        Platform          = if ($PSVersionTable.PSObject.Properties["Platform"]) { $PSVersionTable.Platform } else { "Win32NT" }
        PowerShellVersion = $PSVersionTable.PSVersion
        Shell             = "PowerShell"
        Python            = $null
        Node              = $null
        Java              = $null
        DotNet            = $null
    }
    # Detect Python
    try {
        $pythonVersion = python --version 2>&1
        $env.Python = $pythonVersion
    }
    catch {}
    # Detect Node
    try {
        $nodeVersion = node --version 2>&1
        $env.Node = $nodeVersion
    }
    catch {}
    # Detect Java
    try {
        $javaVersion = java -version 2>&1 | Select-Object -First 1
        $env.Java = $javaVersion
    }
    catch {}
    # Detect .NET
    try {
        $dotnetVersion = dotnet --version 2>&1
        $env.DotNet = $dotnetVersion
    }
    catch {}
    $global:agentContext.Environment = $env
    return $env
}
# ============================================
# Telemetry & Logging System
# ============================================
function Write-AgentLog {
    param(
        [string]$Level,
        [string]$Message,
        [hashtable]$Data = @{}
    )
    # Implementation placeholder - function body should be added here
}

# ===============================
# CUSTOMIZATION FUNCTIONS
# ===============================
function Apply-Theme {
    param(
        [string]$ThemeName
    )
    try {
        Write-DevConsole "Applying $ThemeName theme..." "INFO"
        switch ($ThemeName) {
            "Stealth-Cheetah" {
                # Stealth-Cheetah: Professional dark theme with amber accents for stealth operations
                $bgColor = [System.Drawing.Color]::FromArgb(18, 18, 18)          # Deep black background
                $fgColor = [System.Drawing.Color]::FromArgb(220, 220, 220)       # Light gray text
                $panelColor = [System.Drawing.Color]::FromArgb(25, 25, 25)       # Slightly lighter panels
                $textColor = [System.Drawing.Color]::FromArgb(255, 191, 0)       # Amber/cheetah accent color
                Write-DevConsole "🐆 Stealth-Cheetah theme activated - Maximum stealth mode" "SUCCESS"
            }
            "Dark" {
                $bgColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
                $fgColor = [System.Drawing.Color]::White
                $panelColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
                $textColor = [System.Drawing.Color]::White
            }
            "Light" {
                $bgColor = [System.Drawing.Color]::White
                $fgColor = [System.Drawing.Color]::Black
                $panelColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
                $textColor = [System.Drawing.Color]::Black
            }
            default {
                # Default to Stealth-Cheetah
                $bgColor = [System.Drawing.Color]::FromArgb(18, 18, 18)
                $fgColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
                $panelColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
                $textColor = [System.Drawing.Color]::FromArgb(255, 191, 0)
                Write-DevConsole "🐆 Defaulting to Stealth-Cheetah theme" "INFO"
            }
        }
        # Apply to main form
        $form.BackColor = $bgColor
        $form.ForeColor = $fgColor
        # Apply to panels - use correct splitter panel references
        try {
            if ($mainSplitter.Panel1) { $mainSplitter.Panel1.BackColor = $panelColor }
            if ($mainSplitter.Panel2) { $mainSplitter.Panel2.BackColor = $panelColor }
            if ($leftSplitter.Panel1) { $leftSplitter.Panel1.BackColor = $panelColor }
            if ($leftSplitter.Panel2) { $leftSplitter.Panel2.BackColor = $panelColor }
            if ($leftPanel) { $leftPanel.BackColor = $panelColor }
            if ($explorerContainer) { $explorerContainer.BackColor = $panelColor }
            if ($explorerToolbar) { $explorerToolbar.BackColor = $panelColor }
        }
        catch {
            Write-DevConsole "Panel theming partial: $_" "WARNING"
        }
        # Apply to chat boxes
        try {
            if ($script:chatTabs) {
                foreach ($session in $script:chatTabs.Values) {
                    if ($session.ChatBox) {
                        $session.ChatBox.BackColor = $bgColor
                        $session.ChatBox.ForeColor = $textColor
                    }
                    if ($session.InputBox) {
                        $session.InputBox.BackColor = $bgColor
                        $session.InputBox.ForeColor = $textColor
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Chat theming partial: $_" "WARNING"
        }
        # Apply to text editor
        try {
            if ($script:editor) {
                $script:editor.BackColor = $bgColor
                $script:editor.ForeColor = $textColor
            }
        }
        catch {
            Write-DevConsole "Editor theming partial: $_" "WARNING"
        }
        # Save theme preference
        $script:CurrentTheme = $ThemeName
        Save-CustomizationSettings
        Write-DevConsole "✅ $ThemeName theme applied successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying theme: $_" "ERROR"
    }
}
function Apply-FontSize {
    param(
        [int]$Size
    )
    try {
        Write-DevConsole "Applying font size: ${Size}pt..." "INFO"
        $newFont = New-Object System.Drawing.Font("Segoe UI", $Size)
        # Apply to main form elements
        $form.Font = $newFont
        # Apply to chat boxes
        try {
            if ($script:chatTabs) {
                foreach ($session in $script:chatTabs.Values) {
                    if ($session.ChatBox) {
                        $session.ChatBox.Font = New-Object System.Drawing.Font("Consolas", $Size)
                    }
                    if ($session.InputBox) {
                        $session.InputBox.Font = New-Object System.Drawing.Font("Consolas", $Size)
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Chat font update partial: $_" "WARNING"
        }
        # Apply to text editor
        try {
            if ($script:editor) {
                $script:editor.Font = New-Object System.Drawing.Font("Consolas", $Size)
            }
        }
        catch {
            Write-DevConsole "Editor font update partial: $_" "WARNING"
        }
        # Save font preference
        $script:CurrentFontSize = $Size
        Save-CustomizationSettings
        Write-DevConsole "✅ Font size set to ${Size}pt successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying font size: $_" "ERROR"
    }
}
function Apply-UIScaling {
    param(
        [double]$Scale
    )
    try {
        Write-DevConsole "Applying UI scaling: $($Scale * 100)%..." "INFO"
        # Calculate scaled dimensions
        $baseWidth = 1200
        $baseHeight = 800
        $scaledWidth = [int]($baseWidth * $Scale)
        $scaledHeight = [int]($baseHeight * $Scale)
        # Apply to main form
        $form.Size = New-Object System.Drawing.Size($scaledWidth, $scaledHeight)
        # Scale panels proportionally
        try {
            if ($mainSplitter) { $mainSplitter.SplitterDistance = [int](300 * $Scale) }
            # Note: StatusPanel doesn't exist, might be referring to a toolbar - skipping for now
        }
        catch {
            Write-DevConsole "Panel scaling partial: $_" "WARNING"
        }
        # Save scaling preference
        $script:CurrentUIScale = $Scale
        Save-CustomizationSettings
        Write-DevConsole "✅ UI scaling set to $($Scale * 100)% successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying UI scaling: $_" "ERROR"
    }
}
function Update-FontMenuChecks {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$SelectedItem,
        [System.Windows.Forms.ToolStripMenuItem[]]$AllItems
    )
    foreach ($item in $AllItems) {
        $item.Checked = ($item -eq $SelectedItem)
    }
}
function Update-ScaleMenuChecks {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$SelectedItem,
        [System.Windows.Forms.ToolStripMenuItem[]]$AllItems
    )
    foreach ($item in $AllItems) {
        $item.Checked = ($item -eq $SelectedItem)
    }
}
function Show-CustomThemeBuilder {
    $themeForm = New-Object System.Windows.Forms.Form
    $themeForm.Text = "Custom Theme Builder"
    $themeForm.Size = New-Object System.Drawing.Size(500, 400)
    $themeForm.StartPosition = "CenterScreen"
    $themeForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::FixedDialog
    $themeForm.MaximizeBox = $false
    # Background Color
    $bgLabel = New-Object System.Windows.Forms.Label
    $bgLabel.Text = "Background Color:"
    $bgLabel.Location = New-Object System.Drawing.Point(20, 30)
    $bgLabel.Size = New-Object System.Drawing.Size(120, 20)
    $themeForm.Controls.Add($bgLabel)
    $bgButton = New-Object System.Windows.Forms.Button
    $bgButton.Text = "Select Color"
    $bgButton.Location = New-Object System.Drawing.Point(150, 25)
    $bgButton.Size = New-Object System.Drawing.Size(100, 30)
    $bgButton.BackColor = [System.Drawing.Color]::White
    $themeForm.Controls.Add($bgButton)
    $bgButton.Add_Click({
            $colorDialog = New-Object System.Windows.Forms.ColorDialog
            if ($colorDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $bgButton.BackColor = $colorDialog.Color
            }
        })
    # Text Color
    $textLabel = New-Object System.Windows.Forms.Label
    $textLabel.Text = "Text Color:"
    $textLabel.Location = New-Object System.Drawing.Point(20, 80)
    $textLabel.Size = New-Object System.Drawing.Size(120, 20)
    $themeForm.Controls.Add($textLabel)
    $textButton = New-Object System.Windows.Forms.Button
    $textButton.Text = "Select Color"
    $textButton.Location = New-Object System.Drawing.Point(150, 75)
    $textButton.Size = New-Object System.Drawing.Size(100, 30)
    $textButton.BackColor = [System.Drawing.Color]::Black
    $textButton.ForeColor = [System.Drawing.Color]::White
    $themeForm.Controls.Add($textButton)
    $textButton.Add_Click({
            $colorDialog = New-Object System.Windows.Forms.ColorDialog
            if ($colorDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $textButton.BackColor = $colorDialog.Color
                $textButton.ForeColor = if ($colorDialog.Color.GetBrightness() -gt 0.5) { [System.Drawing.Color]::Black } else { [System.Drawing.Color]::White }
            }
        })
    # Panel Color
    $panelLabel = New-Object System.Windows.Forms.Label
    $panelLabel.Text = "Panel Color:"
    $panelLabel.Location = New-Object System.Drawing.Point(20, 130)
    $panelLabel.Size = New-Object System.Drawing.Size(120, 20)
    $themeForm.Controls.Add($panelLabel)
    $panelButton = New-Object System.Windows.Forms.Button
    $panelButton.Text = "Select Color"
    $panelButton.Location = New-Object System.Drawing.Point(150, 125)
    $panelButton.Size = New-Object System.Drawing.Size(100, 30)
    $panelButton.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
    $themeForm.Controls.Add($panelButton)
    $panelButton.Add_Click({
            $colorDialog = New-Object System.Windows.Forms.ColorDialog
            if ($colorDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $panelButton.BackColor = $colorDialog.Color
            }
        })
    # Preview Panel
    $previewPanel = New-Object System.Windows.Forms.Panel
    $previewPanel.Location = New-Object System.Drawing.Point(300, 25)
    $previewPanel.Size = New-Object System.Drawing.Size(150, 200)
    $previewPanel.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
    $previewPanel.BackColor = $bgButton.BackColor
    $themeForm.Controls.Add($previewPanel)
    $previewLabel = New-Object System.Windows.Forms.Label
    $previewLabel.Text = "Preview Text"
    $previewLabel.Location = New-Object System.Drawing.Point(10, 10)
    $previewLabel.Size = New-Object System.Drawing.Size(130, 20)
    $previewLabel.BackColor = $textButton.BackColor
    $previewLabel.ForeColor = $textButton.ForeColor
    $previewPanel.Controls.Add($previewLabel)
    # Apply Button
    $applyButton = New-Object System.Windows.Forms.Button
    $applyButton.Text = "Apply Theme"
    $applyButton.Location = New-Object System.Drawing.Point(200, 300)
    $applyButton.Size = New-Object System.Drawing.Size(100, 35)
    $themeForm.Controls.Add($applyButton)
    $applyButton.Add_Click({
            Apply-CustomTheme -BackColor $bgButton.BackColor -TextColor $textButton.BackColor -PanelColor $panelButton.BackColor
            $themeForm.Close()
        })
    $themeForm.ShowDialog()
}
function Apply-CustomTheme {
    param(
        [System.Drawing.Color]$BackColor,
        [System.Drawing.Color]$TextColor,
        [System.Drawing.Color]$PanelColor
    )
    try {
        Write-DevConsole "Applying custom theme..." "INFO"
        # Apply to main form
        $form.BackColor = $BackColor
        $form.ForeColor = $TextColor
        # Apply to panels - use correct splitter panel references
        try {
            if ($mainSplitter.Panel1) { $mainSplitter.Panel1.BackColor = $PanelColor }
            if ($mainSplitter.Panel2) { $mainSplitter.Panel2.BackColor = $PanelColor }
            if ($leftSplitter.Panel1) { $leftSplitter.Panel1.BackColor = $PanelColor }
            if ($leftSplitter.Panel2) { $leftSplitter.Panel2.BackColor = $PanelColor }
            if ($leftPanel) { $leftPanel.BackColor = $PanelColor }
            if ($explorerContainer) { $explorerContainer.BackColor = $PanelColor }
            if ($explorerToolbar) { $explorerToolbar.BackColor = $PanelColor }
        }
        catch {
            Write-DevConsole "Panel theming partial: $_" "WARNING"
        }
        # Apply to chat boxes
        try {
            if ($script:chatTabs) {
                foreach ($session in $script:chatTabs.Values) {
                    if ($session.ChatBox) {
                        $session.ChatBox.BackColor = $BackColor
                        $session.ChatBox.ForeColor = $TextColor
                    }
                    if ($session.InputBox) {
                        $session.InputBox.BackColor = $BackColor
                        $session.InputBox.ForeColor = $TextColor
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Chat custom theming partial: $_" "WARNING"
        }
        # Apply to text editor
        try {
            if ($script:editor) {
                $script:editor.BackColor = $BackColor
                $script:editor.ForeColor = $TextColor
            }
        }
        catch {
            Write-DevConsole "Editor custom theming partial: $_" "WARNING"
        }
        # Save custom theme
        $script:CustomTheme = @{
            BackColor  = $BackColor
            TextColor  = $TextColor
            PanelColor = $PanelColor
        }
        $script:CurrentTheme = "Custom"
        Save-CustomizationSettings
        Write-DevConsole "✅ Custom theme applied successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying custom theme: $_" "ERROR"
    }
}
function Reset-UILayout {
    try {
        Write-DevConsole "Resetting UI layout to defaults..." "INFO"
        # Reset form size
        $form.Size = New-Object System.Drawing.Size(1200, 800)
        $form.StartPosition = "CenterScreen"
        # Reset panel sizes - use splitter distance instead of non-existent panels
        try {
            if ($mainSplitter) { $mainSplitter.SplitterDistance = 300 }
            # Note: StatusPanel doesn't exist in current structure
        }
        catch {
            Write-DevConsole "Panel reset partial: $_" "WARNING"
        }
        # Reset splitter position
        if ($mainSplitter) { $mainSplitter.SplitterDistance = 300 }
        # Reset theme to light
        Apply-Theme "Light"
        # Reset font size to 14pt
        Apply-FontSize 14
        # Reset UI scaling to 100%
        Apply-UIScaling 1.0
        Write-DevConsole "✅ UI layout reset to defaults successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error resetting UI layout: $_" "ERROR"
    }
}
function Save-UILayout {
    try {
        $layoutData = @{
            FormSize          = @{
                Width  = $form.Width
                Height = $form.Height
            }
            FormPosition      = @{
                X = $form.Location.X
                Y = $form.Location.Y
            }
            LeftPanelWidth    = if ($mainSplitter) { $mainSplitter.SplitterDistance } else { 300 }
            StatusPanelHeight = 30  # Default value as status panel doesn't exist
            SplitterDistance  = if ($mainSplitter) { $mainSplitter.SplitterDistance } else { 300 }
            Theme             = $script:CurrentTheme
            FontSize          = $script:CurrentFontSize
            UIScale           = $script:CurrentUIScale
        }
        $layoutPath = Join-Path $env:USERPROFILE "RawrXD_Layout.json"
        $layoutData | ConvertTo-Json -Depth 3 | Set-Content -Path $layoutPath
        Write-DevConsole "✅ UI layout saved to: $layoutPath" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error saving UI layout: $_" "ERROR"
    }
}
function Load-UILayout {
    try {
        $layoutPath = Join-Path $env:USERPROFILE "RawrXD_Layout.json"
        if (Test-Path $layoutPath) {
            $layoutData = Get-Content -Path $layoutPath | ConvertFrom-Json
            # Apply saved layout
            $form.Size = New-Object System.Drawing.Size($layoutData.FormSize.Width, $layoutData.FormSize.Height)
            $form.Location = New-Object System.Drawing.Point($layoutData.FormPosition.X, $layoutData.FormPosition.Y)
            # Apply splitter distance instead of non-existent panel properties
            try {
                if ($mainSplitter -and $layoutData.SplitterDistance) {
                    $mainSplitter.SplitterDistance = $layoutData.SplitterDistance
                }
            }
            catch {
                Write-DevConsole "Splitter restore partial: $_" "WARNING"
            }
            # Apply saved customization settings
            if ($layoutData.Theme) { Apply-Theme $layoutData.Theme }
            if ($layoutData.FontSize) { Apply-FontSize $layoutData.FontSize }
            if ($layoutData.UIScale) { Apply-UIScaling $layoutData.UIScale }
            Write-DevConsole "✅ UI layout loaded successfully" "SUCCESS"
        }
        else {
            Write-DevConsole "⚠️ No saved layout found, using defaults" "WARNING"
        }
    }
    catch {
        Write-DevConsole "❌ Error loading UI layout: $_" "ERROR"
    }
}
function Save-CustomizationSettings {
    try {
        $settings = @{
            Theme       = $script:CurrentTheme
            FontSize    = $script:CurrentFontSize
            UIScale     = $script:CurrentUIScale
            CustomTheme = $script:CustomTheme
        }
        $settingsPath = Join-Path $env:USERPROFILE "RawrXD_Customization.json"
        $settings | ConvertTo-Json -Depth 3 | Set-Content -Path $settingsPath
    }
    catch {
        Write-DevConsole "❌ Error saving customization settings: $_" "ERROR"
    }
}
function Load-CustomizationSettings {
    try {
        $settingsPath = Join-Path $env:USERPROFILE "RawrXD_Customization.json"
        if (Test-Path $settingsPath) {
            $settings = Get-Content -Path $settingsPath | ConvertFrom-Json
            $script:CurrentTheme = if ($settings.Theme) { $settings.Theme } else { "Stealth-Cheetah" }
            $script:CurrentFontSize = if ($settings.FontSize) { $settings.FontSize } else { 14 }
            $script:CurrentUIScale = if ($settings.UIScale) { $settings.UIScale } else { 1.0 }
            $script:CustomTheme = $settings.CustomTheme
            # Apply loaded settings
            if ($script:CurrentTheme -ne "Stealth-Cheetah") {
                Apply-Theme $script:CurrentTheme
            }
            else {
                # Apply default Stealth-Cheetah theme
                Apply-Theme "Stealth-Cheetah"
            }
            if ($script:CurrentFontSize -ne 14) {
                Apply-FontSize $script:CurrentFontSize
            }
            if ($script:CurrentUIScale -ne 1.0) {
                Apply-UIScaling $script:CurrentUIScale
            }
        }
        else {
            # No settings file exists, apply default Stealth-Cheetah theme
            Apply-Theme "Stealth-Cheetah"
            Write-DevConsole "🐆 Applied default Stealth-Cheetah theme" "INFO"
        }
    }
    catch {
        Write-DevConsole "❌ Error loading customization settings: $_" "ERROR"
        # Fallback to Stealth-Cheetah on error
        Apply-Theme "Stealth-Cheetah"
    }
}
# ============================================
# AGENTIC AI ERROR DASHBOARD
# ============================================
function Get-AIErrorDashboard {
    param(
        [int]$DaysBack = 7,
        [switch]$IncludeSuccessMetrics
    )
    try {
        # Load AI error statistics
        $statsFile = Join-Path $script:EmergencyLogPath "ai_error_stats.json"
        $aiLogPath = Join-Path $script:EmergencyLogPath "AI_Errors"
        $dashboard = @"
═══════════════════════════════════════════════════════════
🤖 AI AGENT ERROR DASHBOARD - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
═══════════════════════════════════════════════════════════
"@
        # Check if stats file exists
        if (Test-Path $statsFile) {
            $stats = Get-Content $statsFile -Raw | ConvertFrom-Json
            $dashboard += @"
📊 OVERALL STATISTICS (Since Start):
   Total AI Errors: $($stats.TotalErrors)
   Last Updated: $($stats.LastUpdated)
📈 ERRORS BY CATEGORY:
"@
            if ($stats.ErrorsByCategory) {
                foreach ($category in $stats.ErrorsByCategory.PSObject.Properties) {
                    $dashboard += "`n   $($category.Name): $($category.Value)"
                }
            }
            else {
                $dashboard += "`n   No category data available"
            }
            $dashboard += @"
🔥 ERRORS BY SEVERITY:
"@
            if ($stats.ErrorsBySeverity) {
                foreach ($severity in $stats.ErrorsBySeverity.PSObject.Properties) {
                    $severity_icon = switch ($severity.Name) {
                        "CRITICAL" { "🔴" }
                        "HIGH" { "🟡" }
                        "MEDIUM" { "🟠" }
                        "LOW" { "🟢" }
                        default { "⚪" }
                    }
                    $dashboard += "`n   $severity_icon $($severity.Name): $($severity.Value)"
                }
            }
            else {
                $dashboard += "`n   No severity data available"
            }
            $dashboard += @"
🤖 ERRORS BY MODEL:
"@
            if ($stats.ErrorsByModel) {
                foreach ($model in $stats.ErrorsByModel.PSObject.Properties) {
                    $dashboard += "`n   🧠 $($model.Name): $($model.Value)"
                }
            }
            else {
                $dashboard += "`n   No model data available"
            }
        }
        else {
            $dashboard += @"
📊 OVERALL STATISTICS:
   No error statistics available yet
   Stats file: $statsFile
"@
        }
        # Recent error files analysis
        $dashboard += @"
📁 RECENT ERROR LOGS (Last $DaysBack days):
"@
        if (Test-Path $aiLogPath) {
            $cutoffDate = (Get-Date).AddDays(-$DaysBack)
            $recentLogs = Get-ChildItem "$aiLogPath\ai_errors_*.log" | Where-Object { $_.LastWriteTime -ge $cutoffDate } | Sort-Object LastWriteTime -Descending
            if ($recentLogs) {
                foreach ($log in $recentLogs) {
                    $fileDate = $log.LastWriteTime.ToString("yyyy-MM-dd HH:mm")
                    $fileSize = [Math]::Round($log.Length / 1KB, 1)
                    $dashboard += "`n   📄 $($log.Name) - $fileDate - ${fileSize}KB"
                }
            }
            else {
                $dashboard += "`n   ✅ No recent error logs found"
            }
        }
        else {
            $dashboard += "`n   📁 AI error log directory not created yet"
        }
        # System health indicators
        $dashboard += @"
🏥 SYSTEM HEALTH:
   Current Session: $($script:CurrentSession.SessionId)
   Session Start: $($script:CurrentSession.StartTime.ToString("yyyy-MM-dd HH:mm:ss"))
   Last Activity: $($script:CurrentSession.LastActivity.ToString("yyyy-MM-dd HH:mm:ss"))
   Agent Mode: $(if ($global:AgentMode) { "🟢 ACTIVE" } else { "🔴 INACTIVE" })
   Ollama Connection: $(if (Test-NetConnection -ComputerName localhost -Port 11434 -InformationLevel Quiet) { "🟢 ONLINE" } else { "🔴 OFFLINE" })
💡 QUICK ACTIONS:
   /ai-errors          - Show this dashboard
   /clear-ai-errors    - Clear error statistics
   /ai-logs           - View recent error details
   /agent-status      - Check agent system status
═══════════════════════════════════════════════════════════
"@
        return $dashboard
    }
    catch {
        return @"
❌ Error generating AI Error Dashboard: $($_.Exception.Message)
Basic Info:
- Emergency Log Path: $script:EmergencyLogPath
- Current Session: $($script:CurrentSession.SessionId)
- Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
"@
    }
}
function Clear-AIErrorStatistics {
    try {
        $statsFile = Join-Path $script:EmergencyLogPath "ai_error_stats.json"
        $aiLogPath = Join-Path $script:EmergencyLogPath "AI_Errors"
        # Reset statistics file
        if (Test-Path $statsFile) {
            Remove-Item $statsFile -Force
        }
        # Archive old error logs (don't delete, just move to archive)
        if (Test-Path $aiLogPath) {
            $archivePath = Join-Path $aiLogPath "archive_$(Get-Date -Format 'yyyy-MM-dd_HH-mm-ss')"
            New-Item -ItemType Directory -Path $archivePath -Force | Out-Null
            Get-ChildItem "$aiLogPath\ai_errors_*.log" | ForEach-Object {
                Move-Item $_.FullName -Destination $archivePath
            }
        }
        Write-StartupLog "AI error statistics cleared and logs archived" "INFO"
        return "✅ AI error statistics cleared and logs archived to $archivePath"
    }
    catch {
        Write-StartupLog "Failed to clear AI error statistics: $($_.Exception.Message)" "ERROR"
        return "❌ Failed to clear AI error statistics: $($_.Exception.Message)"
    }
}
# ============================================
# Initialize customization variables
$script:CurrentTheme = "Stealth-Cheetah"  # Default to stealth-cheetah theme
$script:CurrentFontSize = 14
$script:CurrentUIScale = 1.0
$script:CustomTheme = $null
# Initialize error statistics
$script:ErrorStats = @{
    TotalErrors       = 0
    CriticalErrors    = 0
    SecurityErrors    = 0
    NetworkErrors     = 0
    FilesystemErrors  = 0
    UIErrors          = 0
    OllamaErrors      = 0
    AuthErrors        = 0
    PerformanceErrors = 0
    AutoRecoveryCount = 0
}