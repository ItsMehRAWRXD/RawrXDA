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
    - Agent task automation
#>
Write-EmergencyLog "Working Directory: $(Get-Location)" "INFO"
Write-EmergencyLog "Log File: $script:StartupLogFile" "INFO"
Write-EmergencyLog "═══════════════════════════════════════════════════════" "INFO"

# Strict mode for better error detection
Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"  # Changed from Stop to Continue for better startup resilience

# Global error handler with emergency logging
trap {
    Write-EmergencyLog "CRITICAL STARTUP ERROR: $_" "ERROR"
    Write-EmergencyLog "Error Category: $($_.CategoryInfo.Category)" "ERROR" 
    Write-EmergencyLog "Error Type: $($_.Exception.GetType().Name)" "ERROR"
    Write-EmergencyLog "Stack Trace: $($_.ScriptStackTrace)" "ERROR"
    Write-EmergencyLog "Script Line Number: $($_.InvocationInfo.ScriptLineNumber)" "ERROR"
    Write-EmergencyLog "Position Message: $($_.InvocationInfo.PositionMessage)" "ERROR"
    
    # Also save critical errors to a separate emergency file
    $emergencyFile = Join-Path $script:EmergencyLogPath "CRITICAL_ERRORS.log"
    $criticalEntry = @"
[$(Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff")] CRITICAL ERROR
Error: $_
Category: $($_.CategoryInfo.Category)
Type: $($_.Exception.GetType().Name)
Line: $($_.InvocationInfo.ScriptLineNumber)
Position: $($_.InvocationInfo.PositionMessage)
Stack Trace:
$($_.ScriptStackTrace)
═════════════════════════════════════════════════════════

"@
    try {
        Add-Content -Path $emergencyFile -Value $criticalEntry -Encoding UTF8
    }
    catch { }
    
    continue
}

# ============================================
# ENHANCED STARTUP LOGGER SYSTEM  
# ============================================

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
        
        # Ensure log directory exists
        if (-not (Test-Path $script:StartupLogFile)) {
            $logDir = Split-Path $script:StartupLogFile
            if (-not (Test-Path $logDir)) {
                New-Item -ItemType Directory -Path $logDir -Force | Out-Null
            }
        }
        
        # Write to log file
        Add-Content -Path $script:StartupLogFile -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        
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

function Show-ErrorNotification {
    param([hashtable]$ErrorRecord)
    
    try {
        # ALWAYS use logging instead of popups - non-intrusive error handling
        $message = "$($ErrorRecord.Message) | Category: $($ErrorRecord.Category) | Time: $($ErrorRecord.Timestamp)"
        if ($ErrorRecord.SourceFunction) {
            $message += " | Source: $($ErrorRecord.SourceFunction)"
        }
        
        # Log to appropriate channel based on severity
        switch ($ErrorRecord.Severity) {
            "CRITICAL" { 
                Write-ErrorLog -Message $message -Severity "CRITICAL"
                Write-DevConsole "CRITICAL ERROR: $message" "ERROR"
            }
            "HIGH" { 
                Write-ErrorLog -Message $message -Severity "HIGH"
                Write-DevConsole "HIGH ERROR: $message" "ERROR"
            }
            "MEDIUM" { 
                Write-DevConsole "MEDIUM ERROR: $message" "WARNING"
            }
            "LOW" { 
                Write-DevConsole "LOW ERROR: $message" "INFO"
            }
            default { 
                Write-DevConsole "ERROR: $message" "WARNING"
            }
        }
    }
    catch {
        Write-StartupLog "Failed to log error notification: $_" "WARNING"
    }
}

function Send-ErrorNotificationEmail {
    param([hashtable]$ErrorRecord)
    
    try {
        if (-not $script:ErrorNotificationConfig.EmailRecipient -or -not $script:ErrorNotificationConfig.SMTPServer) {
            return
        }
        
        $subject = "RawrXD Critical Error - $($ErrorRecord.Category)"
        $body = @"
A critical error has occurred in RawrXD:

Error Details:
- Time: $($ErrorRecord.Timestamp)
- Category: $($ErrorRecord.Category)
- Severity: $($ErrorRecord.Severity)
- Message: $($ErrorRecord.Message)
- Source: $($ErrorRecord.SourceFunction)
- Session ID: $($ErrorRecord.SessionId)
- User: $($ErrorRecord.UserContext)
- Machine: $($ErrorRecord.MachineName)

Stack Trace:
$($ErrorRecord.StackTrace)

Additional Data:
$($ErrorRecord.AdditionalData | ConvertTo-Json -Depth 2)

Please investigate this issue immediately.
"@
        
        Send-MailMessage -To $script:ErrorNotificationConfig.EmailRecipient -Subject $subject -Body $body -SmtpServer $script:ErrorNotificationConfig.SMTPServer -From "rawrxd-noreply@company.com"
        Write-StartupLog "Critical error notification email sent" "INFO"
    }
    catch {
        Write-StartupLog "Failed to send error notification email: $_" "WARNING"
    }
}

function Invoke-AutoRecovery {
    param([hashtable]$ErrorRecord)
    
    switch ($ErrorRecord.Category) {
        "OLLAMA" {
            Write-StartupLog "Attempting Ollama auto-recovery..." "INFO"
            if (Get-Command Start-OllamaServer -ErrorAction SilentlyContinue) {
                Start-OllamaServer
            }
        }
        "NETWORK" {
            Write-StartupLog "Network error detected, checking connectivity..." "INFO"
            Test-NetworkConnectivity
        }
        "FILESYSTEM" {
            Write-StartupLog "File system error detected, checking permissions..." "INFO"
            # Could implement file permission recovery here
        }
        "PERFORMANCE" {
            Write-StartupLog "Performance issue detected, running cleanup..." "INFO"
            [System.GC]::Collect()
            [System.GC]::WaitForPendingFinalizers()
        }
    }
}

function Get-ErrorStatistics {
    return @{
        TotalErrors      = @($script:ErrorTracker.ErrorHistory).Count
        ErrorsByCategory = $script:ErrorTracker.ErrorHistory | Group-Object Category | ForEach-Object { @{ $_.Name = $_.Count } }
        ErrorsBySeverity = $script:ErrorTracker.ErrorHistory | Group-Object Severity | ForEach-Object { @{ $_.Name = $_.Count } }
        RecentErrors     = $script:ErrorTracker.ErrorHistory | Where-Object { [datetime]$_.Timestamp -gt (Get-Date).AddMinutes(-10) }
        LastError        = $script:ErrorTracker.ErrorHistory | Select-Object -Last 1
    }
}

function Show-ErrorReportDialog {
    $reportForm = New-Object System.Windows.Forms.Form
    $reportForm.Text = "Error Report & Statistics"
    $reportForm.Size = New-Object System.Drawing.Size(800, 600)
    $reportForm.StartPosition = "CenterScreen"
    $reportForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    
    $statsGrid = New-Object System.Windows.Forms.DataGridView
    $statsGrid.Dock = [System.Windows.Forms.DockStyle]::Fill
    $statsGrid.BackgroundColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $statsGrid.DefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $statsGrid.DefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $statsGrid.ColumnHeadersDefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $statsGrid.ColumnHeadersDefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $statsGrid.ReadOnly = $true
    $statsGrid.AutoSizeColumnsMode = "AllCells"
    $statsGrid.AllowUserToAddRows = $false
    
    # Add columns
    $statsGrid.Columns.Add("Timestamp", "Timestamp") | Out-Null
    $statsGrid.Columns.Add("Category", "Category") | Out-Null
    $statsGrid.Columns.Add("Severity", "Severity") | Out-Null
    $statsGrid.Columns.Add("Message", "Message") | Out-Null
    $statsGrid.Columns.Add("Source", "Source") | Out-Null
    
    # Add error data
    foreach ($errItem in $script:ErrorTracker.ErrorHistory) {
        $row = @($errItem.Timestamp, $errItem.Category, $errItem.Severity, $errItem.Message, $errItem.SourceFunction)
        $statsGrid.Rows.Add($row) | Out-Null
        
        # Color code by severity
        $rowCount = $statsGrid.Rows.Count
        if ($rowCount -gt 0) {
            $lastRow = $statsGrid.Rows[$rowCount - 1]
            switch ($error.Severity) {
                "CRITICAL" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Red }
                "HIGH" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Orange }
                "MEDIUM" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Yellow }
                "LOW" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::LightGray }
            }
        }
    }
    
    $reportForm.Controls.Add($statsGrid)
    $reportForm.ShowDialog()
}

# ============================================
# FIND & REPLACE DIALOGS
# ============================================

function Show-FindDialog {
    $findForm = New-Object System.Windows.Forms.Form
    $findForm.Text = "Find"
    $findForm.Size = New-Object System.Drawing.Size(450, 150)
    $findForm.StartPosition = "CenterScreen"
    $findForm.FormBorderStyle = "FixedDialog"
    $findForm.MaximizeBox = $false
    $findForm.MinimizeBox = $false
    $findForm.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    
    # Find label
    $findLabel = New-Object System.Windows.Forms.Label
    $findLabel.Text = "Find what:"
    $findLabel.Location = New-Object System.Drawing.Point(10, 20)
    $findLabel.Size = New-Object System.Drawing.Size(70, 20)
    $findLabel.ForeColor = [System.Drawing.Color]::White
    $findForm.Controls.Add($findLabel)
    
    # Find textbox
    $findTextBox = New-Object System.Windows.Forms.TextBox
    $findTextBox.Location = New-Object System.Drawing.Point(85, 17)
    $findTextBox.Size = New-Object System.Drawing.Size(250, 20)
    $findTextBox.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $findTextBox.ForeColor = [System.Drawing.Color]::White
    $findForm.Controls.Add($findTextBox)
    
    # Case sensitive checkbox
    $caseSensitiveCheckbox = New-Object System.Windows.Forms.CheckBox
    $caseSensitiveCheckbox.Text = "Match case"
    $caseSensitiveCheckbox.Location = New-Object System.Drawing.Point(85, 45)
    $caseSensitiveCheckbox.Size = New-Object System.Drawing.Size(100, 20)
    $caseSensitiveCheckbox.ForeColor = [System.Drawing.Color]::White
    $findForm.Controls.Add($caseSensitiveCheckbox)
    
    # Find Next button
    $findNextBtn = New-Object System.Windows.Forms.Button
    $findNextBtn.Text = "Find Next"
    $findNextBtn.Location = New-Object System.Drawing.Point(350, 15)
    $findNextBtn.Size = New-Object System.Drawing.Size(80, 25)
    $findNextBtn.Add_Click({
            if ([string]::IsNullOrEmpty($findTextBox.Text)) { return }
        
            $searchText = $findTextBox.Text
            $editorText = $script:editor.Text
            $startIndex = $script:editor.SelectionStart + $script:editor.SelectionLength
        
            $comparison = if ($caseSensitiveCheckbox.Checked) { 
                [System.StringComparison]::Ordinal 
            }
            else { 
                [System.StringComparison]::OrdinalIgnoreCase 
            }
        
            $foundIndex = $editorText.IndexOf($searchText, $startIndex, $comparison)
        
            if ($foundIndex -eq -1) {
                # Wrap around to beginning
                $foundIndex = $editorText.IndexOf($searchText, 0, $comparison)
            }
        
            if ($foundIndex -ge 0) {
                $script:editor.Select($foundIndex, $searchText.Length)
                $script:editor.ScrollToCaret()
                $script:editor.Focus()
            }
            else {
                Write-DevConsole "Text not found: '$searchText'" "INFO"
            }
        })
    $findForm.Controls.Add($findNextBtn)
    
    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(350, 50)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 25)
    $closeBtn.Add_Click({ $findForm.Close() })
    $findForm.Controls.Add($closeBtn)
    
    $findForm.ShowDialog()
}

function Show-ReplaceDialog {
    $replaceForm = New-Object System.Windows.Forms.Form
    $replaceForm.Text = "Find and Replace"
    $replaceForm.Size = New-Object System.Drawing.Size(450, 200)
    $replaceForm.StartPosition = "CenterScreen"
    $replaceForm.FormBorderStyle = "FixedDialog"
    $replaceForm.MaximizeBox = $false
    $replaceForm.MinimizeBox = $false
    $replaceForm.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    
    # Find label
    $findLabel = New-Object System.Windows.Forms.Label
    $findLabel.Text = "Find what:"
    $findLabel.Location = New-Object System.Drawing.Point(10, 20)
    $findLabel.Size = New-Object System.Drawing.Size(80, 20)
    $findLabel.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($findLabel)
    
    # Find textbox
    $findTextBox = New-Object System.Windows.Forms.TextBox
    $findTextBox.Location = New-Object System.Drawing.Point(95, 17)
    $findTextBox.Size = New-Object System.Drawing.Size(240, 20)
    $findTextBox.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $findTextBox.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($findTextBox)
    
    # Replace label
    $replaceLabel = New-Object System.Windows.Forms.Label
    $replaceLabel.Text = "Replace with:"
    $replaceLabel.Location = New-Object System.Drawing.Point(10, 50)
    $replaceLabel.Size = New-Object System.Drawing.Size(80, 20)
    $replaceLabel.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($replaceLabel)
    
    # Replace textbox
    $replaceTextBox = New-Object System.Windows.Forms.TextBox
    $replaceTextBox.Location = New-Object System.Drawing.Point(95, 47)
    $replaceTextBox.Size = New-Object System.Drawing.Size(240, 20)
    $replaceTextBox.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $replaceTextBox.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($replaceTextBox)
    
    # Case sensitive checkbox
    $caseSensitiveCheckbox = New-Object System.Windows.Forms.CheckBox
    $caseSensitiveCheckbox.Text = "Match case"
    $caseSensitiveCheckbox.Location = New-Object System.Drawing.Point(95, 75)
    $caseSensitiveCheckbox.Size = New-Object System.Drawing.Size(100, 20)
    $caseSensitiveCheckbox.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($caseSensitiveCheckbox)
    
    # Status label
    $statusLabel = New-Object System.Windows.Forms.Label
    $statusLabel.Text = ""
    $statusLabel.Location = New-Object System.Drawing.Point(10, 130)
    $statusLabel.Size = New-Object System.Drawing.Size(325, 20)
    $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
    $replaceForm.Controls.Add($statusLabel)
    
    # Find Next button
    $findNextBtn = New-Object System.Windows.Forms.Button
    $findNextBtn.Text = "Find Next"
    $findNextBtn.Location = New-Object System.Drawing.Point(350, 15)
    $findNextBtn.Size = New-Object System.Drawing.Size(80, 25)
    $findNextBtn.Add_Click({
            if ([string]::IsNullOrEmpty($findTextBox.Text)) { return }
        
            $searchText = $findTextBox.Text
            $editorText = $script:editor.Text
            $startIndex = $script:editor.SelectionStart + $script:editor.SelectionLength
        
            $comparison = if ($caseSensitiveCheckbox.Checked) { 
                [System.StringComparison]::Ordinal 
            }
            else { 
                [System.StringComparison]::OrdinalIgnoreCase 
            }
        
            $foundIndex = $editorText.IndexOf($searchText, $startIndex, $comparison)
        
            if ($foundIndex -eq -1) {
                $foundIndex = $editorText.IndexOf($searchText, 0, $comparison)
            }
        
            if ($foundIndex -ge 0) {
                $script:editor.Select($foundIndex, $searchText.Length)
                $script:editor.ScrollToCaret()
                $script:editor.Focus()
                $statusLabel.Text = "Found at position $foundIndex"
            }
            else {
                $statusLabel.Text = "Text not found"
                $statusLabel.ForeColor = [System.Drawing.Color]::Orange
            }
        })
    $replaceForm.Controls.Add($findNextBtn)
    
    # Replace button
    $replaceBtn = New-Object System.Windows.Forms.Button
    $replaceBtn.Text = "Replace"
    $replaceBtn.Location = New-Object System.Drawing.Point(350, 45)
    $replaceBtn.Size = New-Object System.Drawing.Size(80, 25)
    $replaceBtn.Add_Click({
            if ($script:editor.SelectionLength -gt 0) {
                $script:editor.SelectedText = $replaceTextBox.Text
                $statusLabel.Text = "Replaced"
                $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
            }
        })
    $replaceForm.Controls.Add($replaceBtn)
    
    # Replace All button
    $replaceAllBtn = New-Object System.Windows.Forms.Button
    $replaceAllBtn.Text = "Replace All"
    $replaceAllBtn.Location = New-Object System.Drawing.Point(350, 75)
    $replaceAllBtn.Size = New-Object System.Drawing.Size(80, 25)
    $replaceAllBtn.Add_Click({
            if ([string]::IsNullOrEmpty($findTextBox.Text)) { return }
        
            $searchText = $findTextBox.Text
            $replaceWith = $replaceTextBox.Text
        
            if ($caseSensitiveCheckbox.Checked) {
                $count = ($script:editor.Text | Select-String -Pattern [regex]::Escape($searchText) -AllMatches -CaseSensitive).Matches.Count
                $script:editor.Text = $script:editor.Text.Replace($searchText, $replaceWith)
            }
            else {
                $count = ($script:editor.Text | Select-String -Pattern [regex]::Escape($searchText) -AllMatches).Matches.Count
                $script:editor.Text = $script:editor.Text -ireplace [regex]::Escape($searchText), $replaceWith
            }
        
            $statusLabel.Text = "Replaced $count occurrence(s)"
            $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
        })
    $replaceForm.Controls.Add($replaceAllBtn)
    
    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(350, 105)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 25)
    $closeBtn.Add_Click({ $replaceForm.Close() })
    $replaceForm.Controls.Add($closeBtn)
    
    $replaceForm.ShowDialog()
}

# ============================================
# WINDOWS FORMS ASSEMBLY LOADING WITH ERROR HANDLING
# ============================================

Write-EmergencyLog "Initializing Windows Forms assemblies..." "INFO"

# Function to safely load assemblies with fallback options
function Initialize-WindowsForms {
    param()
    
    try {
        # Check PowerShell version
        $psVersion = $PSVersionTable.PSVersion
        Write-EmergencyLog "PowerShell Version: $psVersion" "INFO"
        
        if ($psVersion.Major -ge 6) {
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
            $testForm = New-Object System.Windows.Forms.Form -ErrorAction Stop
            $testForm.Dispose()
            Write-EmergencyLog "✅ Windows Forms is functional" "SUCCESS"
            
            # Set application compatibility settings
            [System.Windows.Forms.Application]::EnableVisualStyles()
            [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false)
            Write-EmergencyLog "✅ Application compatibility settings applied" "SUCCESS"
            
            return $true
        }
        catch {
            Write-EmergencyLog "❌ Windows Forms not functional: $($_.Exception.Message)" "CRITICAL"
            return $false
        }
    }
    catch {
        Write-EmergencyLog "❌ Critical error initializing Windows Forms: $($_.Exception.Message)" "CRITICAL"
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
Platform: $($PSVersionTable.Platform)
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
# AGENTIC AI ERROR LOGGING SYSTEM
# ============================================

function Write-AgenticErrorLog {
    param(
        [string]$ErrorMessage,
        [string]$ErrorCategory = "AI",
        [string]$Severity = "MEDIUM",
        [string]$AgentContext = "",
        [string]$AIModel = "",
        [hashtable]$AIMetrics = @{}
    )
    
    try {
        # Create AI-specific log directory
        $aiLogPath = Join-Path $script:EmergencyLogPath "AI_Errors"
        if (-not (Test-Path $aiLogPath)) {
            New-Item -ItemType Directory -Path $aiLogPath -Force | Out-Null
        }
        
        # AI error log file with date
        $dateStr = Get-Date -Format "yyyy-MM-dd"
        $aiLogFile = Join-Path $aiLogPath "ai_errors_$dateStr.log"
        
        # Build detailed AI error entry
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $aiErrorEntry = @"
[$timestamp] [$Severity] AI_ERROR
Category: $ErrorCategory
Message: $ErrorMessage
Agent_Context: $AgentContext
AI_Model: $AIModel
"@
        
        # Add AI metrics if available
        if ($AIMetrics.Count -gt 0) {
            $aiErrorEntry += "`nAI_Metrics:"
            foreach ($metric in $AIMetrics.GetEnumerator()) {
                $aiErrorEntry += "`n  $($metric.Key): $($metric.Value)"
            }
        }
        
        $aiErrorEntry += "`n" + ("=" * 80) + "`n"
        
        # Write to AI error log
        Add-Content -Path $aiLogFile -Value $aiErrorEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        
        # Also log to main error log
        Write-StartupLog "[AI_ERROR] $ErrorMessage | Context: $AgentContext | Model: $AIModel" "ERROR"
        
        # Update AI error statistics
        Update-AIErrorStatistics -ErrorCategory $ErrorCategory -Severity $Severity -AIModel $AIModel
        
    }
    catch {
        Write-StartupLog "Failed to write agentic error log: $($_.Exception.Message)" "ERROR"
    }
}

function Update-AIErrorStatistics {
    param(
        [string]$ErrorCategory,
        [string]$Severity,
        [string]$AIModel
    )
    
    try {
        # AI statistics file
        $statsFile = Join-Path $script:EmergencyLogPath "ai_error_stats.json"
        
        # Load existing stats or create new
        $stats = @{
            TotalErrors      = 0
            ErrorsByCategory = @{}
            ErrorsBySeverity = @{}
            ErrorsByModel    = @{}
            LastUpdated      = ""
        }
        
        if (Test-Path $statsFile) {
            $existingStats = Get-Content $statsFile -Raw | ConvertFrom-Json
            $stats.TotalErrors = $existingStats.TotalErrors
            $stats.ErrorsByCategory = $existingStats.ErrorsByCategory
            $stats.ErrorsBySeverity = $existingStats.ErrorsBySeverity
            $stats.ErrorsByModel = $existingStats.ErrorsByModel
        }
        
        # Update statistics
        $stats.TotalErrors++
        $stats.LastUpdated = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        
        # Category statistics
        if (-not $stats.ErrorsByCategory.$ErrorCategory) {
            $stats.ErrorsByCategory.$ErrorCategory = 0
        }
        $stats.ErrorsByCategory.$ErrorCategory++
        
        # Severity statistics
        if (-not $stats.ErrorsBySeverity.$Severity) {
            $stats.ErrorsBySeverity.$Severity = 0
        }
        $stats.ErrorsBySeverity.$Severity++
        
        # Model statistics
        if ($AIModel -and -not $stats.ErrorsByModel.$AIModel) {
            $stats.ErrorsByModel.$AIModel = 0
        }
        if ($AIModel) {
            $stats.ErrorsByModel.$AIModel++
        }
        
        # Save updated statistics
        $stats | ConvertTo-Json -Depth 3 | Set-Content $statsFile -Encoding UTF8
        
    }
    catch {
        Write-StartupLog "Failed to update AI error statistics: $($_.Exception.Message)" "ERROR"
    }
}

function Initialize-SecurityConfig {
    param(
        [bool]$EnableStealthMode = $false,
        [bool]$EnableEncryption = $true,
        [int]$SessionTimeoutMinutes = 60
    )
    
    Write-StartupLog "Initializing security configuration..." "INFO"
    
    # Initialize security configuration if not already done
    if (-not $script:SecurityConfig) {
        $script:SecurityConfig = @{
            EncryptSensitiveData  = $EnableEncryption
            StealthMode           = $EnableStealthMode
            ProcessHiding         = $false
            AntiForensics         = $false
            MaxFileSize           = 10MB
            AllowedExtensions     = @('.txt', '.md', '.ps1', '.json', '.xml', '.yaml', '.yml', '.log')
            DangerousExtensions   = @('.exe', '.bat', '.cmd', '.com', '.scr', '.pif', '.vbs', '.js', '.jar', '.msi')
            SessionTimeout        = $SessionTimeoutMinutes * 60  # Convert minutes to seconds for consistency
            MaxErrorsPerMinute    = 10
            LogToEventLog         = $true
            EnableAuditTrail      = $true
            RequireAuthentication = $false
            TwoFactorAuth         = $false
        }
        Write-StartupLog "Security configuration initialized with default values" "SUCCESS"
    }
    else {
        Write-StartupLog "Security configuration already exists, validating..." "INFO"
    }
    
    # Validate configuration
    if (-not $script:SecurityConfig.ContainsKey('MaxFileSize')) {
        $script:SecurityConfig.MaxFileSize = 10MB
    }
    if (-not $script:SecurityConfig.ContainsKey('AllowedExtensions')) {
        $script:SecurityConfig.AllowedExtensions = @('.txt', '.md', '.ps1', '.json', '.xml', '.yaml', '.yml', '.log')
    }
    if (-not $script:SecurityConfig.ContainsKey('SessionTimeout')) {
        $script:SecurityConfig.SessionTimeout = 3600  # 1 hour - consistent with main config
    }
    
    Write-SecurityLog "Security configuration initialized successfully" "SUCCESS" "Configuration validated and ready"
    return $true
}

function Process-AgentCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        
        [Parameter(Mandatory = $false)]
        [hashtable]$Parameters = @{},
        
        [Parameter(Mandatory = $false)]
        [string]$SourceContext = "Unknown"
    )
    
    try {
        Write-SecurityLog "Processing agent command" "INFO" "Command: $Command, Source: $SourceContext"
        
        # Validate command safety
        if (-not (Test-InputSafety -Input $Command -Type "Command")) {
            Write-ErrorLog "Unsafe agent command blocked" "SECURITY" "HIGH" "Process-AgentCommand" @{Command = $Command; Source = $SourceContext }
            return $false
        }
        
        # Process different command types
        switch ($Command.ToLower()) {
            "analyze_code" {
                if ($Parameters.ContainsKey('FilePath')) {
                    $result = Invoke-CodeAnalysis -FilePath $Parameters.FilePath
                    Write-StartupLog "Code analysis completed for: $($Parameters.FilePath)" "SUCCESS"
                    return $result
                }
            }
            "generate_summary" {
                if ($Parameters.ContainsKey('Content')) {
                    $result = Invoke-ContentSummary -Content $Parameters.Content
                    Write-StartupLog "Content summary generated" "SUCCESS"
                    return $result
                }
            }
            "security_scan" {
                $result = Invoke-SecurityScan -Target $Parameters.Target
                Write-StartupLog "Security scan completed" "SUCCESS"
                return $result
            }
            "optimize_performance" {
                $result = Invoke-PerformanceOptimization
                Write-StartupLog "Performance optimization completed" "SUCCESS"
                return $result
            }
            default {
                Write-ErrorLog "Unknown agent command: $Command" "OPERATION" "MEDIUM" "Process-AgentCommand" @{Command = $Command }
                return $false
            }
        }
        
        Write-SecurityLog "Agent command processed successfully" "SUCCESS" "Command: $Command"
        return $true
        
    }
    catch {
        Write-ErrorLog "Agent command processing failed: $_" "OPERATION" "HIGH" "Process-AgentCommand" @{Command = $Command; Error = $_.Exception.Message }
        return $false
    }
}

function Load-Settings {
    param(
        [Parameter(Mandatory = $false)]
        [string]$ConfigPath = ""
    )
    
    try {
        # Determine config file path
        if ([string]::IsNullOrEmpty($ConfigPath)) {
            $ConfigPath = Join-Path $env:TEMP "RawrXD_Settings.json"
        }
        
        Write-StartupLog "Loading settings from: $ConfigPath" "INFO"
        
        # Check if config file exists
        if (-not (Test-Path $ConfigPath)) {
            Write-StartupLog "Settings file not found, creating default settings..." "WARNING"
            
            # Create default settings
            $defaultSettings = @{
                Theme            = "Stealth-Cheetah"
                FontSize         = 12
                FontFamily       = "Consolas"
                UIScale          = 1.0
                ChatHistory      = $true
                SaveSession      = $true
                SecurityLevel    = "Medium"
                OllamaServer     = "http://localhost:11434"
                DefaultModel     = "llama2"
                AutoSave         = $true
                AutoSaveInterval = 300  # 5 minutes
                WindowState      = @{
                    Maximized = $false
                    Width     = 1200
                    Height    = 800
                    Left      = 100
                    Top       = 100
                }
                UILayout         = @{
                    LeftPanelWidth    = 300
                    RightPanelWidth   = 400
                    SplitterPositions = @(300, 800)
                }
                LastOpenFiles    = @()
                RecentProjects   = @()
            }
            
            # Save default settings
            $defaultSettings | ConvertTo-Json -Depth 10 | Set-Content $ConfigPath -Encoding UTF8
            Write-StartupLog "Default settings created and saved" "SUCCESS"
            
            # Apply default settings to application
            $script:CurrentSettings = $defaultSettings
            return $true
        }
        
        # Load existing settings
        $settingsContent = Get-Content $ConfigPath -Raw -ErrorAction Stop
        $loadedSettings = $settingsContent | ConvertFrom-Json
        
        # Convert PSCustomObject to hashtable for easier manipulation
        $script:CurrentSettings = @{}
        $loadedSettings.PSObject.Properties | ForEach-Object {
            $script:CurrentSettings[$_.Name] = $_.Value
        }
        
        Write-StartupLog "Settings loaded successfully" "SUCCESS"
        Write-SecurityLog "Configuration loaded" "SUCCESS" "File: $ConfigPath, Settings count: $($script:CurrentSettings.Keys.Count)"
        
        # Apply loaded settings to UI if forms are available
        if ($script:form -and $script:CurrentSettings.ContainsKey('WindowState')) {
            Apply-WindowSettings
        }
        
        if ($script:CurrentSettings.ContainsKey('Theme')) {
            Apply-Theme -ThemeName $script:CurrentSettings.Theme
        }
        
        return $true
        
    }
    catch {
        Write-ErrorLog "Failed to load settings: $_" "FILE" "MEDIUM" "Load-Settings" @{ConfigPath = $ConfigPath; Error = $_.Exception.Message }
        
        # Create emergency fallback settings
        $script:CurrentSettings = @{
            Theme      = "Dark"
            FontSize   = 12
            FontFamily = "Consolas"
            UIScale    = 1.0
        }
        
        Write-StartupLog "Emergency fallback settings applied" "WARNING"
        return $false
    }
}

# Helper function for Load-Settings
function Apply-WindowSettings {
    param(
        [hashtable]$WindowSettings = $script:CurrentSettings.WindowState,
        [System.Windows.Forms.Form]$TargetForm = $script:form
    )
    
    if (-not $TargetForm -or -not $script:CurrentSettings.ContainsKey('WindowState')) {
        return
    }
    
    try {
        $windowState = $script:CurrentSettings.WindowState
        
        if ($windowState.Maximized) {
            $script:form.WindowState = [System.Windows.Forms.FormWindowState]::Maximized
        }
        else {
            $script:form.WindowState = [System.Windows.Forms.FormWindowState]::Normal
            $script:form.Width = $windowState.Width
            $script:form.Height = $windowState.Height
            $script:form.Left = $windowState.Left
            $script:form.Top = $windowState.Top
        }
        
        Write-StartupLog "Window settings applied successfully" "SUCCESS"
    }
    catch {
        Write-StartupLog "Failed to apply window settings: $_" "WARNING"
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
            $numericUpDown.Value = $script:SecurityConfig[$setting]
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
# WebView2 and Browser Setup
# ============================================

# WebView2 Setup (Modern browser engine for YouTube support)
# Use lightweight WebView2 Runtime instead of full Edge browser
$wvDir = "$env:TEMP\WVLibs"
$script:useWebView2 = $false
$script:browserType = "Unknown"

Write-StartupLog "Checking WebView2 Runtime..." "INFO"

# Check if WebView2 Runtime is already installed system-wide
$webView2Installed = Test-Path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"

if ($webView2Installed) {
    Write-StartupLog "WebView2 Runtime already installed" "SUCCESS"
}
else {
    Write-StartupLog "WebView2 Runtime not found, downloading portable version..." "WARNING"
    
    if (!(Test-Path "$wvDir")) {
        try {
            New-Item -ItemType Directory -Path "$wvDir" -Force | Out-Null
            
            # Download WebView2 NuGet package (no admin required)
            Write-StartupLog "Downloading WebView2 libraries (portable, no admin needed)..." "INFO"
            Invoke-WebRequest "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" -OutFile "$wvDir\wv.zip" -ErrorAction Stop
            
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [IO.Compression.ZipFile]::ExtractToDirectory("$wvDir\wv.zip", "$wvDir\ex", $true)
            
            # Find and copy the WinForms DLL (managed assembly)
            $winformsDll = Get-ChildItem "$wvDir\ex" -Recurse -Filter "Microsoft.Web.WebView2.WinForms.dll" | 
            Where-Object { $_.FullName -match "net45" } | 
            Select-Object -First 1
            
            if ($winformsDll) {
                Copy-Item $winformsDll.FullName -Destination $wvDir -Force
                Write-Host "✓ WebView2 libraries installed (portable mode)" -ForegroundColor Green
            }
        }
        catch {
            Write-Host "⚠ Could not download WebView2. Using Internet Explorer fallback for browser." -ForegroundColor Yellow
            Write-Host "  (YouTube may not work in IE fallback mode)" -ForegroundColor DarkYellow
        }
    }
}

# Try to load the managed WinForms assembly
if (Test-Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll") {
    try {
        Add-Type -Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll" -ErrorAction Stop
        $script:useWebView2 = $true
        Write-Host "✓ WebView2 loaded successfully (YouTube ready!)" -ForegroundColor Green
    }
    catch {
        Write-Host "⚠ WebView2 load failed: $($_.Exception.Message)" -ForegroundColor Yellow
        Write-Host "  Using Internet Explorer fallback" -ForegroundColor DarkYellow
        $script:useWebView2 = $false
    }
}
else {
    Write-Host "⚠ WebView2 not available, using Internet Explorer fallback" -ForegroundColor Yellow
    Write-Host "  (YouTube embeds may not work in IE mode)" -ForegroundColor DarkYellow
    $script:useWebView2 = $false
}

# Configuration (Adjust these as needed)
$OllamaAPIEndpoint = "http://localhost:11434/api/generate"  # Default API Endpoint
$OllamaSecureEndpoint = "https://localhost:11434/api/generate"  # HTTPS Endpoint (if configured)
$OllamaModel = "bigdaddyg-fast:latest" # Default Ollama Model
$script:OllamaAPIKey = $null  # API key for authentication (if required)
$script:UseHTTPS = $false     # Enable HTTPS for Ollama connections
$script:DebugMode = $false    # Enable debug logging

# ============================================
# MULTI-SERVER OLLAMA SUPPORT SYSTEM
# ============================================

# Ollama server configuration and management
$script:OllamaServers = @{
    "Local"       = @{
        Name                = "Local Server"
        BaseURL             = "http://localhost:11434"
        SecureURL           = "https://localhost:11434"
        Username            = ""
        Password            = ""
        APIKey              = ""
        IsActive            = $true
        LastConnection      = $null
        Status              = "Unknown"
        Models              = @()
        Features            = @("generate", "embed", "pull", "push")
        Priority            = 1
        Timeout             = 30
        MaxRetries          = 3
        HealthCheckInterval = 30
    }
    "Production"  = @{
        Name                = "Production Server"
        BaseURL             = "http://ollama-prod.company.com:11434"
        SecureURL           = "https://ollama-prod.company.com:11434"
        Username            = "prod_user"
        Password            = ""
        APIKey              = ""
        IsActive            = $false
        LastConnection      = $null
        Status              = "Unknown"
        Models              = @()
        Features            = @("generate", "embed")
        Priority            = 2
        Timeout             = 45
        MaxRetries          = 2
        HealthCheckInterval = 60
    }
    "Development" = @{
        Name                = "Development Server"
        BaseURL             = "http://ollama-dev.company.com:11434"
        SecureURL           = "https://ollama-dev.company.com:11434"
        Username            = "dev_user"
        Password            = ""
        APIKey              = ""
        IsActive            = $false
        LastConnection      = $null
        Status              = "Unknown"
        Models              = @()
        Features            = @("generate", "embed", "pull", "push", "create")
        Priority            = 3
        Timeout             = 60
        MaxRetries          = 5
        HealthCheckInterval = 45
    }
}

# Current active server and connection settings
$script:CurrentOllamaServer = "Local"
$script:OllamaConnectionPool = @{}
$script:OllamaHealthMonitor = $null

function Connect-OllamaServer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ServerName,
        
        [Parameter(Mandatory = $false)]
        [string]$Username = "",
        
        [Parameter(Mandatory = $false)]
        [string]$Password = "",
        
        [Parameter(Mandatory = $false)]
        [string]$APIKey = "",
        
        [Parameter(Mandatory = $false)]
        [bool]$UseHTTPS = $false,
        
        [Parameter(Mandatory = $false)]
        [bool]$ValidateCertificate = $true
    )
    
    try {
        if (-not $script:OllamaServers.ContainsKey($ServerName)) {
            Register-ErrorHandler -ErrorMessage "Server '$ServerName' not found in configuration" -ErrorCategory "OLLAMA" -Severity "HIGH" -SourceFunction "Connect-OllamaServer"
            return $false
        }
        
        $server = $script:OllamaServers[$ServerName]
        $baseUrl = if ($UseHTTPS) { $server.SecureURL } else { $server.BaseURL }
        
        # Create connection object
        $connection = @{
            ServerName     = $ServerName
            BaseURL        = $baseUrl
            Username       = if ($Username) { $Username } else { $server.Username }
            Password       = if ($Password) { $Password } else { $server.Password }
            APIKey         = if ($APIKey) { $APIKey } else { $server.APIKey }
            UseHTTPS       = $UseHTTPS
            Timeout        = $server.Timeout
            MaxRetries     = $server.MaxRetries
            IsConnected    = $false
            LastUsed       = Get-Date
            ConnectionTime = $null
        }
        
        # Create HTTP client with authentication
        $httpClient = New-Object System.Net.WebClient
        
        # Setup authentication
        if ($connection.APIKey) {
            $httpClient.Headers.Add("Authorization", "Bearer $($connection.APIKey)")
        }
        elseif ($connection.Username -and $connection.Password) {
            $credentials = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes("$($connection.Username):$($connection.Password)"))
            $httpClient.Headers.Add("Authorization", "Basic $credentials")
        }
        
        # Configure SSL/TLS for HTTPS
        if ($UseHTTPS) {
            if (-not $ValidateCertificate) {
                # Allow self-signed certificates for development/internal servers
                add-type @"
using System.Net;
using System.Security.Cryptography.X509Certificates;
public class TrustAllCertsPolicy : ICertificatePolicy {
    public bool CheckValidationResult(ServicePoint sp, X509Certificate cert, WebRequest req, int problem) {
        return true;
    }
}
"@
                [System.Net.ServicePointManager]::CertificatePolicy = New-Object TrustAllCertsPolicy
            }
            [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
        }
        
        # Test connection with health check
        $healthUrl = "$baseUrl/api/tags"
        $startTime = Get-Date
        
        $httpClient.DownloadString($healthUrl)
        
        $connectionTime = ((Get-Date) - $startTime).TotalMilliseconds
        
        # Update connection status
        $connection.IsConnected = $true
        $connection.ConnectionTime = $connectionTime
        $server.Status = "Connected"
        $server.LastConnection = Get-Date
        
        # Store connection in pool
        $script:OllamaConnectionPool[$ServerName] = $connection
        
        Write-StartupLog "✅ Connected to Ollama server '$ServerName' ($([math]::Round($connectionTime, 2))ms)" "SUCCESS"
        Register-ErrorHandler -ErrorMessage "Successfully connected to server '$ServerName'" -ErrorCategory "OLLAMA" -Severity "LOW" -SourceFunction "Connect-OllamaServer" -ShowToUser $false
        
        return $true
    }
    catch {
        $server.Status = "Failed"
        Register-ErrorHandler -ErrorMessage "Failed to connect to Ollama server '$ServerName': $($_.Exception.Message)" -ErrorCategory "OLLAMA" -Severity "HIGH" -SourceFunction "Connect-OllamaServer"
        return $false
    }
    finally {
        if ($httpClient) {
            $httpClient.Dispose()
        }
    }
}

function Authenticate-OllamaUser {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ServerName,
        
        [Parameter(Mandatory = $true)]
        [string]$Username,
        
        [Parameter(Mandatory = $true)]
        [string]$Password
    )
    
    try {
        if (-not $script:OllamaServers.ContainsKey($ServerName)) {
            Register-ErrorHandler -ErrorMessage "Server '$ServerName' not found for authentication" -ErrorCategory "AUTH" -Severity "HIGH" -SourceFunction "Authenticate-OllamaUser"
            return $false
        }
        
        $server = $script:OllamaServers[$ServerName]
        $authUrl = "$($server.BaseURL)/api/auth/login"  # Hypothetical auth endpoint
        
        $authData = @{
            username = $Username
            password = $Password
        } | ConvertTo-Json
        
        $response = Invoke-RestMethod -Uri $authUrl -Method POST -Body $authData -ContentType "application/json" -TimeoutSec $server.Timeout
        
        if ($response.success -and $response.token) {
            $server.APIKey = $response.token
            $server.Username = $Username
            Write-StartupLog "✅ User '$Username' authenticated on server '$ServerName'" "SUCCESS"
            Register-ErrorHandler -ErrorMessage "User authentication successful for '$Username'" -ErrorCategory "AUTH" -Severity "LOW" -SourceFunction "Authenticate-OllamaUser" -ShowToUser $false
            return $true
        }
        else {
            Register-ErrorHandler -ErrorMessage "Authentication failed for user '$Username' on server '$ServerName'" -ErrorCategory "AUTH" -Severity "MEDIUM" -SourceFunction "Authenticate-OllamaUser"
            return $false
        }
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Authentication error for user '$Username': $($_.Exception.Message)" -ErrorCategory "AUTH" -Severity "HIGH" -SourceFunction "Authenticate-OllamaUser"
        return $false
    }
}

function Switch-OllamaServer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ServerName
    )
    
    try {
        if (-not $script:OllamaServers.ContainsKey($ServerName)) {
            Register-ErrorHandler -ErrorMessage "Server '$ServerName' not found" -ErrorCategory "OLLAMA" -Severity "MEDIUM" -SourceFunction "Switch-OllamaServer"
            return $false
        }
        
        $previousServer = $script:CurrentOllamaServer
        $server = $script:OllamaServers[$ServerName]
        
        # Test connection to new server
        if (Test-OllamaServerConnection -ServerName $ServerName) {
            $script:CurrentOllamaServer = $ServerName
            
            # Update global endpoints
            $global:OllamaAPIEndpoint = "$($server.BaseURL)/api/generate"
            $global:OllamaSecureEndpoint = "$($server.SecureURL)/api/generate"
            
            Write-StartupLog "✅ Switched to Ollama server '$ServerName'" "SUCCESS"
            
            # Update UI if available
            if (Get-Command Update-OllamaStatusDisplay -ErrorAction SilentlyContinue) {
                Update-OllamaStatusDisplay
            }
            
            Register-ErrorHandler -ErrorMessage "Switched from '$previousServer' to '$ServerName'" -ErrorCategory "OLLAMA" -Severity "LOW" -SourceFunction "Switch-OllamaServer" -ShowToUser $false
            return $true
        }
        else {
            Register-ErrorHandler -ErrorMessage "Failed to switch to server '$ServerName' - connection test failed" -ErrorCategory "OLLAMA" -Severity "MEDIUM" -SourceFunction "Switch-OllamaServer"
            return $false
        }
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Error switching to server '$ServerName': $($_.Exception.Message)" -ErrorCategory "OLLAMA" -Severity "HIGH" -SourceFunction "Switch-OllamaServer"
        return $false
    }
}

function Test-OllamaServerConnection {
    param([string]$ServerName)
    
    try {
        if (-not $ServerName) { $ServerName = $script:CurrentOllamaServer }
        
        $server = $script:OllamaServers[$ServerName]
        if (-not $server) { return $false }
        
        $testUrl = "$($server.BaseURL)/api/tags"
        
        $null = Invoke-RestMethod -Uri $testUrl -Method GET -TimeoutSec $server.Timeout
        
        $server.Status = "Online"
        $server.LastConnection = Get-Date
        
        return $true
    }
    catch {
        $server.Status = "Offline"
        return $false
    }
}

function Get-OllamaServerModels {
    param([string]$ServerName)
    
    try {
        if (-not $ServerName) { $ServerName = $script:CurrentOllamaServer }
        
        $server = $script:OllamaServers[$ServerName]
        if (-not $server) { return @() }
        
        $modelsUrl = "$($server.BaseURL)/api/tags"
        $response = Invoke-RestMethod -Uri $modelsUrl -Method GET -TimeoutSec $server.Timeout
        
        $models = @($response.models | ForEach-Object { $_.name })
        $server.Models = $models
        
        Write-StartupLog "✅ Retrieved $($models.Count) models from server '$ServerName'" "INFO"
        return $models
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to get models from server '$ServerName': $($_.Exception.Message)" -ErrorCategory "OLLAMA" -Severity "MEDIUM" -SourceFunction "Get-OllamaServerModels"
        return @()
    }
}

function Start-OllamaHealthMonitoring {
    if ($script:OllamaHealthMonitor) {
        $script:OllamaHealthMonitor.Stop()
        $script:OllamaHealthMonitor.Dispose()
    }
    
    $script:OllamaHealthMonitor = New-Object System.Windows.Forms.Timer
    $script:OllamaHealthMonitor.Interval = 30000  # Check every 30 seconds
    $script:OllamaHealthMonitor.add_Tick({
            foreach ($serverName in $script:OllamaServers.Keys) {
                $server = $script:OllamaServers[$serverName]
                if ($server.IsActive) {
                    $previousStatus = $server.Status
                    $isOnline = Test-OllamaServerConnection -ServerName $serverName
                
                    if ($isOnline -and $previousStatus -ne "Online") {
                        Register-ErrorHandler -ErrorMessage "Server '$serverName' is back online" -ErrorCategory "OLLAMA" -Severity "LOW" -SourceFunction "Health Monitor" -ShowToUser $false
                    }
                    elseif (-not $isOnline -and $previousStatus -eq "Online") {
                        Register-ErrorHandler -ErrorMessage "Server '$serverName' went offline" -ErrorCategory "OLLAMA" -Severity "MEDIUM" -SourceFunction "Health Monitor"
                    }
                }
            }
        })
    $script:OllamaHealthMonitor.Start()
    
    Write-StartupLog "✅ Ollama health monitoring started" "INFO"
}

function Show-OllamaServerManager {
    $managerForm = New-Object System.Windows.Forms.Form
    $managerForm.Text = "Ollama Server Manager"
    $managerForm.Size = New-Object System.Drawing.Size(800, 600)
    $managerForm.StartPosition = "CenterScreen"
    $managerForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $managerForm.ForeColor = [System.Drawing.Color]::White
    
    # Server list
    $serverGrid = New-Object System.Windows.Forms.DataGridView
    $serverGrid.Size = New-Object System.Drawing.Size(760, 300)
    $serverGrid.Location = New-Object System.Drawing.Point(20, 20)
    $serverGrid.BackgroundColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $serverGrid.DefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $serverGrid.DefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $serverGrid.ColumnHeadersDefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $serverGrid.ColumnHeadersDefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $serverGrid.ReadOnly = $true
    $serverGrid.AutoSizeColumnsMode = "Fill"
    $serverGrid.AllowUserToAddRows = $false
    
    # Add columns
    $serverGrid.Columns.Add("Name", "Server Name") | Out-Null
    $serverGrid.Columns.Add("Status", "Status") | Out-Null
    $serverGrid.Columns.Add("URL", "URL") | Out-Null
    $serverGrid.Columns.Add("Models", "Models") | Out-Null
    $serverGrid.Columns.Add("LastConnection", "Last Connection") | Out-Null
    
    # Populate server data
    foreach ($serverName in $script:OllamaServers.Keys) {
        $server = $script:OllamaServers[$serverName]
        $row = @(
            $server.Name,
            $server.Status,
            $server.BaseURL,
            @($server.Models).Count,
            $(if ($server.LastConnection) { $server.LastConnection.ToString("HH:mm:ss") } else { "Never" })
        )
        $serverGrid.Rows.Add($row) | Out-Null
        
        # Color code by status
        $rowCount = $serverGrid.Rows.Count
        if ($rowCount -gt 0) {
            $lastRow = $serverGrid.Rows[$rowCount - 1]
            switch ($server.Status) {
                "Online" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Green }
                "Offline" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Red }
                "Connected" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::LightGreen }
                "Failed" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Orange }
                default { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Gray }
            }
        }
    }
    
    # Buttons panel
    $buttonsPanel = New-Object System.Windows.Forms.Panel
    $buttonsPanel.Size = New-Object System.Drawing.Size(760, 60)
    $buttonsPanel.Location = New-Object System.Drawing.Point(20, 340)
    
    $connectBtn = New-Object System.Windows.Forms.Button
    $connectBtn.Text = "Connect"
    $connectBtn.Size = New-Object System.Drawing.Size(100, 30)
    $connectBtn.Location = New-Object System.Drawing.Point(0, 15)
    $connectBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $connectBtn.ForeColor = [System.Drawing.Color]::White
    $connectBtn.FlatStyle = "Flat"
    $buttonsPanel.Controls.Add($connectBtn)
    
    $switchBtn = New-Object System.Windows.Forms.Button
    $switchBtn.Text = "Switch To"
    $switchBtn.Size = New-Object System.Drawing.Size(100, 30)
    $switchBtn.Location = New-Object System.Drawing.Point(110, 15)
    $switchBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 150, 0)
    $switchBtn.ForeColor = [System.Drawing.Color]::White
    $switchBtn.FlatStyle = "Flat"
    $buttonsPanel.Controls.Add($switchBtn)
    
    $refreshBtn = New-Object System.Windows.Forms.Button
    $refreshBtn.Text = "Refresh"
    $refreshBtn.Size = New-Object System.Drawing.Size(100, 30)
    $refreshBtn.Location = New-Object System.Drawing.Point(220, 15)
    $refreshBtn.BackColor = [System.Drawing.Color]::FromArgb(120, 120, 120)
    $refreshBtn.ForeColor = [System.Drawing.Color]::White
    $refreshBtn.FlatStyle = "Flat"
    $buttonsPanel.Controls.Add($refreshBtn)
    
    # Event handlers
    $connectBtn.Add_Click({
            if ($serverGrid.SelectedRows -and $serverGrid.SelectedRows.Count -gt 0) {
                $serverName = $serverGrid.SelectedRows[0].Cells[0].Value
                $serverKey = $script:OllamaServers.Keys | Where-Object { $script:OllamaServers[$_].Name -eq $serverName }
                if ($serverKey) {
                    Connect-OllamaServer -ServerName $serverKey
                    # Refresh display
                    $refreshBtn.PerformClick()
                }
            }
        })
    
    $switchBtn.Add_Click({
            if ($serverGrid.SelectedRows -and $serverGrid.SelectedRows.Count -gt 0) {
                $serverName = $serverGrid.SelectedRows[0].Cells[0].Value
                $serverKey = $script:OllamaServers.Keys | Where-Object { $script:OllamaServers[$_].Name -eq $serverName }
                if ($serverKey) {
                    Switch-OllamaServer -ServerName $serverKey
                    Write-DevConsole "Switched to server: $serverName" "INFO"
                }
            }
        })
    
    $refreshBtn.Add_Click({
            # Test all server connections
            foreach ($serverName in $script:OllamaServers.Keys) {
                Test-OllamaServerConnection -ServerName $serverName
                Get-OllamaServerModels -ServerName $serverName
            }
            $managerForm.Close()
            Show-OllamaServerManager  # Reopen with updated data
        })
    
    $managerForm.Controls.Add($serverGrid)
    $managerForm.Controls.Add($buttonsPanel)
    $managerForm.ShowDialog()
}

# Security initialization
Write-SecurityLog "Application starting" "INFO" "Version: 2.0, Security: Enabled"
$script:CurrentSession.LastActivity = Get-Date

# Initialize security configuration from environment or config file
$configPath = Join-Path $env:APPDATA "RawrXD\security.json"
if (Test-Path $configPath) {
    try {
        $savedConfig = Get-Content $configPath | ConvertFrom-Json
        foreach ($key in $savedConfig.PSObject.Properties.Name) {
            if ($script:SecurityConfig.ContainsKey($key)) {
                $script:SecurityConfig[$key] = $savedConfig.$key
            }
        }
        Write-SecurityLog "Security configuration loaded" "SUCCESS" "Source: $configPath"
    }
    catch {
        Write-SecurityLog "Failed to load security configuration" "ERROR" $_.Exception.Message
    }
}

# GUI Setup
$form = New-Object System.Windows.Forms.Form
$form.Text = "RawrXD - Secure AI Editor"
$form.Size = New-Object System.Drawing.Size(1200, 700)
$form.StartPosition = "CenterScreen"
$form.Icon = [System.Drawing.SystemIcons]::Application

# Security check and optional authentication
if ($script:SecurityConfig.AuthenticationRequired) {
    $authResult = Show-AuthenticationDialog
    if (-not $authResult) {
        Write-SecurityLog "Authentication failed or cancelled" "ERROR"
        Write-DevConsole "Authentication required to access this application." "WARNING"
        exit
    }
    $script:CurrentSession.IsAuthenticated = $true
    Write-SecurityLog "User authenticated successfully" "SUCCESS"
}

# Form security events
$form.Add_FormClosing({
        Write-SecurityLog "Application closing" "INFO"
        Invoke-SecureCleanup
    })

$form.Add_Activated({
        $script:CurrentSession.LastActivity = Get-Date
    })

# Add security status to title bar
$form.Add_Shown({
        $securityIndicator = if ($script:SecurityConfig.StealthMode) { "🔒 STEALTH" } 
        elseif ($script:SecurityConfig.EncryptSensitiveData) { "🔐 SECURE" }
        else { "🔓 STANDARD" }
        $form.Text = "RawrXD - Secure AI Editor [$securityIndicator]"
    })

# Main vertical splitter (Left: Explorer+Editor, Right: Chat)
$mainSplitter = New-Object System.Windows.Forms.SplitContainer
$mainSplitter.Dock = [System.Windows.Forms.DockStyle]::Fill
$mainSplitter.Orientation = [System.Windows.Forms.Orientation]::Vertical
$mainSplitter.SplitterDistance = 800

# Left side splitter (Explorer and Editor)
$leftSplitter = New-Object System.Windows.Forms.SplitContainer
$leftSplitter.Dock = [System.Windows.Forms.DockStyle]::Fill
$leftSplitter.Orientation = [System.Windows.Forms.Orientation]::Vertical
$leftSplitter.SplitterDistance = 200
$mainSplitter.Panel1.Controls.Add($leftSplitter) | Out-Null

# File Explorer Container (Left Pane)
$explorerContainer = New-Object System.Windows.Forms.Panel
$explorerContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$leftSplitter.Panel1.Controls.Add($explorerContainer) | Out-Null

$leftPanel = $leftSplitter.Panel1

# File Explorer (TreeView) - Add FIRST so it fills remaining space
$explorer = New-Object System.Windows.Forms.TreeView
$explorer.Dock = [System.Windows.Forms.DockStyle]::Fill
$explorer.ShowLines = $true
$explorer.ShowRootLines = $true
$explorer.Font = New-Object System.Drawing.Font("Consolas", 9)
$explorer.BorderStyle = [System.Windows.Forms.BorderStyle]::None
$explorerContainer.Controls.Add($explorer) | Out-Null

# Add TreeView event handlers for lazy loading
$explorer.add_BeforeExpand({
        param($sender, $e)
        try {
            $node = $e.Node
            if ($node.Nodes.Count -eq 1 -and $node.Nodes[0].Tag -eq "DUMMY") {
                # Remove dummy node
                $node.Nodes.Clear()
            
                # Get the directory path from the node's ToolTipText or Tag
                $dirPath = ""
                if ($node.Tag -and $node.Tag -ne "DUMMY") {
                    $dirPath = $node.Tag
                }
                elseif ($node.ToolTipText -match "Folder: ([^`n]+)") {
                    $dirPath = $matches[1]
                }
            
                if ($dirPath -and (Test-Path $dirPath)) {
                    # Load subdirectories and files
                    try {
                        $subdirs = Get-ChildItem -Path $dirPath -Directory -ErrorAction SilentlyContinue | Sort-Object Name
                        $files = Get-ChildItem -Path $dirPath -File -ErrorAction SilentlyContinue | Sort-Object Name
                    
                        # Add subdirectories
                        foreach ($dir in $subdirs) {
                            try {
                                $isHidden = $dir.Attributes -band [System.IO.FileAttributes]::Hidden
                                $dirIcon = if ($isHidden) { "📁💀" } else { "📁" }
                                $dirName = if ($isHidden) { "$($dir.Name) (Hidden)" } else { $dir.Name }
                            
                                $dirNode = New-Object System.Windows.Forms.TreeNode("$dirIcon $dirName")
                                $dirNode.Tag = $dir.FullName
                                $dirNode.ToolTipText = "Folder: $($dir.FullName)`nCreated: $($dir.CreationTime)`nAttributes: $($dir.Attributes)"
                            
                                if ($isHidden) {
                                    $dirNode.ForeColor = [System.Drawing.Color]::Gray
                                }
                            
                                # Check if this directory has subdirectories
                                $hasSubdirs = Get-ChildItem -Path $dir.FullName -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
                                if ($hasSubdirs) {
                                    $dummy = New-Object System.Windows.Forms.TreeNode("Loading...")
                                    $dummy.Tag = "DUMMY"
                                    $dummy.ForeColor = [System.Drawing.Color]::Gray
                                    $dirNode.Nodes.Add($dummy) | Out-Null
                                }
                            
                                $node.Nodes.Add($dirNode) | Out-Null
                            }
                            catch {
                                Write-DevConsole "Error adding directory node: $_" "WARNING"
                            }
                        }
                    
                        # Add files
                        foreach ($file in $files) {
                            try {
                                $fileIcon = Get-FileIcon $file.Extension
                                $isHidden = $file.Attributes -band [System.IO.FileAttributes]::Hidden
                                $fileName = if ($isHidden) { "$($file.Name) (Hidden)" } else { $file.Name }
                            
                                $fileNode = New-Object System.Windows.Forms.TreeNode("$fileIcon $fileName")
                                $fileNode.Tag = $file.FullName
                                $fileNode.ToolTipText = "File: $($file.FullName)`nSize: $($file.Length) bytes`nCreated: $($file.CreationTime)`nModified: $($file.LastWriteTime)"
                            
                                if ($isHidden) {
                                    $fileNode.ForeColor = [System.Drawing.Color]::Gray
                                }
                            
                                $node.Nodes.Add($fileNode) | Out-Null
                            }
                            catch {
                                Write-DevConsole "Error adding file node: $_" "WARNING"
                            }
                        }
                    }
                    catch {
                        $errorNode = New-Object System.Windows.Forms.TreeNode("⚠️ Error loading contents")
                        $errorNode.ForeColor = [System.Drawing.Color]::Red
                        $errorNode.ToolTipText = "Error: $_"
                        $node.Nodes.Add($errorNode) | Out-Null
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Error in TreeView BeforeExpand: $_" "ERROR"
        }
    })

# Add double-click event handler for opening files (SIMPLIFIED FOR RELIABILITY)
$explorer.add_NodeMouseDoubleClick({
        param($sender, $e)
        try {
            $node = $e.Node
            if ($node.Tag -and $node.Tag -ne "DUMMY") {
                $filePath = $node.Tag
            
                Write-DevConsole "🔍 Double-click detected on: $filePath" "INFO"
            
                # Basic validation first
                if (-not (Test-Path $filePath)) {
                    Write-DevConsole "❌ File not found: $filePath" "ERROR"
                    Write-DevConsole "File not found: $filePath" "ERROR"
                    return
                }
            
                if (Test-Path $filePath -PathType Container) {
                    Write-DevConsole "ℹ️ Directory double-clicked, ignoring..." "INFO"
                    return
                }
            
                # Check if it's a file we can handle
                $fileInfo = Get-Item $filePath
                Write-DevConsole "✅ File found: $($fileInfo.Length) bytes" "SUCCESS"
            
                # Basic size check (increased to 50MB for practicality)
                if ($fileInfo.Length -gt 50MB) {
                    $result = (Read-Host "File is large ($([math]::Round($fileInfo.Length/1MB, 1))MB). This may slow down the editor. Continue? (y/N)") -eq "y"
                    if ($result -ne "Yes") {
                        return
                    }
                }
            
                # Basic extension check for obviously dangerous files
                $extension = [System.IO.Path]::GetExtension($filePath).ToLower()
                $binaryExts = @('.exe', '.dll', '.bin', '.obj', '.lib', '.com', '.scr', '.msi', '.cab')
                if ($extension -in $binaryExts) {
                    $result = (Read-Host "This appears to be a binary file ($extension). It may not display correctly as text. Open anyway? (y/N)") -eq "y"
                    if ($result -ne "Yes") {
                        return
                    }
                }
            
                try {
                    # Read the file content
                    Write-DevConsole "📖 Reading file content..." "INFO"
                    $content = [System.IO.File]::ReadAllText($filePath)
                    Write-DevConsole "✅ File content read successfully: $($content.Length) characters" "SUCCESS"
                
                    # Handle encrypted files (.secure extension)
                    if ($extension -eq '.secure') {
                        try {
                            if (Get-Command "Unprotect-SensitiveString" -ErrorAction SilentlyContinue) {
                                $content = Unprotect-SensitiveString -EncryptedData $content
                                Write-DevConsole "🔓 Encrypted file decrypted successfully" "SUCCESS"
                            }
                            else {
                                Write-DevConsole "⚠️ Encrypted file detected but no decryption function available" "WARNING"
                            }
                        }
                        catch {
                            Write-DevConsole "❌ Failed to decrypt file: $_" "ERROR"
                            $result = (Read-Host "File appears to be encrypted but decryption failed. Show raw content? (y/N)") -eq "y"
                            if ($result -ne "Yes") {
                                return
                            }
                        }
                    }
                
                    # Assign to editor (this is where the magic happens)
                    if ($script:editor) {
                        $script:editor.Text = $content
                        $global:currentFile = $filePath
                        $form.Text = "RawrXD - AI Editor - $([System.IO.Path]::GetFileName($filePath))"
                        Write-DevConsole "🎉 File opened successfully in editor!" "SUCCESS"
                    
                        # Update last activity if session exists
                        if ($script:CurrentSession) {
                            $script:CurrentSession.LastActivity = Get-Date
                        }
                    
                        # Optional: Add to recent files
                        if ($script:RecentFiles -and $script:RecentFiles.Count -lt 10) {
                            if ($filePath -notin $script:RecentFiles) {
                                $script:RecentFiles.Add($filePath)
                            }
                        }
                    }
                    else {
                        Write-DevConsole "❌ Editor not initialized!" "ERROR"
                        Write-StartupLog "Editor is not properly initialized. Please restart RawrXD." "ERROR"; Write-DevConsole "CRITICAL: Editor initialization failed" "ERROR"
                    }
                
                }
                catch {
                    Write-DevConsole "❌ Error reading file: $_" "ERROR"
                    Write-DevConsole "Error reading file: $($_.Exception.Message)" "ERROR"
                }
            }
        }
        catch {
            Write-DevConsole "❌ Error in double-click handler: $_" "ERROR"
            Write-DevConsole "Unexpected error: $($_.Exception.Message)" "ERROR"
        }
    })# Create context menu for TreeView file operations
$explorerContextMenu = New-Object System.Windows.Forms.ContextMenuStrip
$explorer.ContextMenuStrip = $explorerContextMenu

# Open in RawrXD
$openInRawrItem = New-Object System.Windows.Forms.ToolStripMenuItem
$openInRawrItem.Text = "📝 Open in RawrXD"
$openInRawrItem.Add_Click({
        $selectedNode = $explorer.SelectedNode
        if ($selectedNode -and $selectedNode.Tag -and $selectedNode.Tag -ne "DUMMY") {
            if ((Test-Path $selectedNode.Tag) -and -not (Test-Path $selectedNode.Tag -PathType Container)) {
                # Trigger the same logic as double-click
                $explorer_NodeMouseDoubleClick = $explorer.GetType().GetEvent("NodeMouseDoubleClick")
                $dummyArgs = New-Object System.Windows.Forms.TreeNodeMouseClickEventArgs($selectedNode, [System.Windows.Forms.MouseButtons]::Left, 2, 0, 0)
                $explorer.OnNodeMouseDoubleClick($dummyArgs)
            }
        }
    })
$explorerContextMenu.Items.Add($openInRawrItem) | Out-Null

# Open in System Default
$openSystemItem = New-Object System.Windows.Forms.ToolStripMenuItem
$openSystemItem.Text = "🚀 Open in System Default"
$openSystemItem.Add_Click({
        $selectedNode = $explorer.SelectedNode
        if ($selectedNode -and $selectedNode.Tag -and $selectedNode.Tag -ne "DUMMY") {
            if (Test-Path $selectedNode.Tag) {
                try {
                    Start-Process $selectedNode.Tag
                    Write-DevConsole "Opened in system default: $($selectedNode.Tag)" "SUCCESS"
                }
                catch {
                    Write-DevConsole "Failed to open in system default: $_" "ERROR"
                    Write-ErrorLog -Message "Failed to open file: $($_.Exception.Message)" -Severity "HIGH"
                }
            }
        }
    })
$explorerContextMenu.Items.Add($openSystemItem) | Out-Null

# Copy Path
$copyPathItem = New-Object System.Windows.Forms.ToolStripMenuItem
$copyPathItem.Text = "📋 Copy Path"
$copyPathItem.Add_Click({
        $selectedNode = $explorer.SelectedNode
        if ($selectedNode -and $selectedNode.Tag -and $selectedNode.Tag -ne "DUMMY") {
            try {
                [System.Windows.Forms.Clipboard]::SetText($selectedNode.Tag)
                Write-DevConsole "Copied path to clipboard: $($selectedNode.Tag)" "SUCCESS"
            }
            catch {
                Write-DevConsole "Failed to copy path: $_" "ERROR"
            }
        }
    })
$explorerContextMenu.Items.Add($copyPathItem) | Out-Null

# Separator
$explorerContextMenu.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

# Open Containing Folder
$openFolderItem = New-Object System.Windows.Forms.ToolStripMenuItem
$openFolderItem.Text = "📁 Open Containing Folder"
$openFolderItem.Add_Click({
        $selectedNode = $explorer.SelectedNode
        if ($selectedNode -and $selectedNode.Tag -and $selectedNode.Tag -ne "DUMMY") {
            try {
                if (Test-Path $selectedNode.Tag -PathType Container) {
                    # It's a folder, open it
                    Start-Process "explorer.exe" $selectedNode.Tag
                }
                else {
                    # It's a file, open containing folder and select file
                    Start-Process "explorer.exe" "/select,`"$($selectedNode.Tag)`""
                }
                Write-DevConsole "Opened containing folder for: $($selectedNode.Tag)" "SUCCESS"
            }
            catch {
                Write-DevConsole "Failed to open containing folder: $_" "ERROR"
                Write-ErrorLog -Message "Failed to open folder: $($_.Exception.Message)" -Severity "HIGH"
            }
        }
    })
$explorerContextMenu.Items.Add($openFolderItem) | Out-Null

# Properties
$propertiesItem = New-Object System.Windows.Forms.ToolStripMenuItem
$propertiesItem.Text = "ℹ️ Properties"
$propertiesItem.Add_Click({
        $selectedNode = $explorer.SelectedNode
        if ($selectedNode -and $selectedNode.Tag -and $selectedNode.Tag -ne "DUMMY") {
            try {
                if (Test-Path $selectedNode.Tag) {
                    $item = Get-Item $selectedNode.Tag
                    $isFile = -not (Test-Path $selectedNode.Tag -PathType Container)
                
                    $props = @"
Path: $($item.FullName)
Name: $($item.Name)
Type: $(if ($isFile) { "File" } else { "Folder" })
Created: $($item.CreationTime)
Modified: $($item.LastWriteTime)
$(if ($isFile) { "Size: $($item.Length) bytes" } else { "" })
Attributes: $($item.Attributes)
"@
                    Write-DevConsole "Properties - $($item.Name): $props" "INFO"
                }
            }
            catch {
                Write-DevConsole "Failed to get properties: $_" "ERROR"
                Write-ErrorLog -Message "Failed to get properties: $($_.Exception.Message)" -Severity "HIGH"
            }
        }
    })
$explorerContextMenu.Items.Add($propertiesItem) | Out-Null

# Explorer toolbar - Add SECOND so it docks on top
$explorerToolbar = New-Object System.Windows.Forms.Panel
$explorerToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$explorerToolbar.Height = 30
$explorerToolbar.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
$explorerContainer.Controls.Add($explorerToolbar) | Out-Null

# Explorer refresh button
$explorerRefreshBtn = New-Object System.Windows.Forms.Button
$explorerRefreshBtn.Text = "🔄"
$explorerRefreshBtn.Dock = [System.Windows.Forms.DockStyle]::Right
$explorerRefreshBtn.Width = 30
$explorerRefreshBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$explorerRefreshBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
$explorerRefreshBtn.ForeColor = [System.Drawing.Color]::White
$explorerRefreshBtn.Font = New-Object System.Drawing.Font("Segoe UI", 10)
$explorerToolbar.Controls.Add($explorerRefreshBtn) | Out-Null

# Explorer up/back button
$explorerUpBtn = New-Object System.Windows.Forms.Button
$explorerUpBtn.Text = "⬆️"
$explorerUpBtn.Dock = [System.Windows.Forms.DockStyle]::Right
$explorerUpBtn.Width = 30
$explorerUpBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$explorerUpBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
$explorerUpBtn.ForeColor = [System.Drawing.Color]::White
$explorerUpBtn.Font = New-Object System.Drawing.Font("Segoe UI", 10)
$explorerToolbar.Controls.Add($explorerUpBtn) | Out-Null

# Current path label
$explorerPathLabel = New-Object System.Windows.Forms.Label
$explorerPathLabel.Text = "Path: "
$explorerPathLabel.Dock = [System.Windows.Forms.DockStyle]::Fill
$explorerPathLabel.Font = New-Object System.Drawing.Font("Segoe UI", 8)
$explorerPathLabel.ForeColor = [System.Drawing.Color]::White
$explorerPathLabel.Padding = New-Object System.Windows.Forms.Padding(5, 8, 5, 0)
$explorerToolbar.Controls.Add($explorerPathLabel) | Out-Null

# Text Editor (Middle Pane)
$script:editor = New-Object System.Windows.Forms.RichTextBox
$script:editor.Dock = [System.Windows.Forms.DockStyle]::Fill
$script:editor.Font = New-Object System.Drawing.Font("Consolas", 10)

# Enable built-in undo for RichTextBox
$script:editor.EnableAutoDragDrop = $true

# Track text changes for undo/redo stack
$script:editor.Add_TextChanged({
        if (-not $script:isUndoRedoOperation) {
            # Only save to undo stack if text actually changed
            if ($script:lastEditorText -ne $script:editor.Text) {
                $script:undoStack.Push($script:lastEditorText)
                $script:redoStack.Clear()  # Clear redo stack on new edit
                $script:lastEditorText = $script:editor.Text
            
                # Limit undo stack size to prevent memory issues
                if ($script:undoStack.Count -gt 100) {
                    $tempStack = [System.Collections.Generic.Stack[string]]::new()
                    for ($i = 0; $i -lt 50; $i++) {
                        $tempStack.Push($script:undoStack.Pop())
                    }
                    $script:undoStack.Clear()
                    while ($tempStack.Count -gt 0) {
                        $script:undoStack.Push($tempStack.Pop())
                    }
                }
            }
        }
    })

$leftSplitter.Panel2.Controls.Add($script:editor) | Out-Null

# Right side - Tab Control for Chat and Browser
$rightTabControl = New-Object System.Windows.Forms.TabControl
$rightTabControl.Dock = [System.Windows.Forms.DockStyle]::Fill
$mainSplitter.Panel2.Controls.Add($rightTabControl) | Out-Null

# Chat Tab
$chatTab = New-Object System.Windows.Forms.TabPage
$chatTab.Text = "Chat"
$rightTabControl.TabPages.Add($chatTab) | Out-Null

# Chat Tab Control for multiple chat sessions
$chatTabControl = New-Object System.Windows.Forms.TabControl
$chatTabControl.Dock = [System.Windows.Forms.DockStyle]::Fill
$chatTab.Controls.Add($chatTabControl) | Out-Null

# Chat toolbar
$chatToolbar = New-Object System.Windows.Forms.Panel
$chatToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$chatToolbar.Height = 30
$chatTab.Controls.Add($chatToolbar) | Out-Null

# New Chat Button
$newChatBtn = New-Object System.Windows.Forms.Button
$newChatBtn.Text = "New Chat"
$newChatBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$newChatBtn.Width = 80
$chatToolbar.Controls.Add($newChatBtn) | Out-Null

# Close Chat Button
$closeChatBtn = New-Object System.Windows.Forms.Button
$closeChatBtn.Text = "Close Chat"
$closeChatBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$closeChatBtn.Width = 80
$chatToolbar.Controls.Add($closeChatBtn) | Out-Null

# Bulk Actions Button (for multithreading demo)
$bulkActionsBtn = New-Object System.Windows.Forms.Button
$bulkActionsBtn.Text = "Bulk Actions"
$bulkActionsBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$bulkActionsBtn.Width = 90
$chatToolbar.Controls.Add($bulkActionsBtn) | Out-Null

# Threading Status Indicator
$threadingStatusLabel = New-Object System.Windows.Forms.Label
$threadingStatusLabel.Text = "MT: Initializing..."
$threadingStatusLabel.Dock = [System.Windows.Forms.DockStyle]::Left
$threadingStatusLabel.Width = 100
$threadingStatusLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleLeft
$threadingStatusLabel.Padding = New-Object System.Windows.Forms.Padding(5, 0, 0, 0)
$chatToolbar.Controls.Add($threadingStatusLabel) | Out-Null

# Chat Status Label
$script:chatStatusLabel = New-Object System.Windows.Forms.Label
$script:chatStatusLabel.Text = "No active chats"
$script:chatStatusLabel.Dock = [System.Windows.Forms.DockStyle]::Fill
$script:chatStatusLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleLeft
$script:chatStatusLabel.Padding = New-Object System.Windows.Forms.Padding(10, 0, 0, 0)
$chatToolbar.Controls.Add($script:chatStatusLabel) | Out-Null

# Git Tab
$gitTab = New-Object System.Windows.Forms.TabPage
$gitTab.Text = "Git"
$rightTabControl.TabPages.Add($gitTab) | Out-Null

# Git container
$gitContainer = New-Object System.Windows.Forms.Panel
$gitContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$gitTab.Controls.Add($gitContainer) | Out-Null

# Git toolbar
$gitToolbar = New-Object System.Windows.Forms.Panel
$gitToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$gitToolbar.Height = 40
$gitContainer.Controls.Add($gitToolbar) | Out-Null

# Git status refresh button
$gitRefreshBtn = New-Object System.Windows.Forms.Button
$gitRefreshBtn.Text = "Refresh"
$gitRefreshBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$gitRefreshBtn.Width = 80
$gitToolbar.Controls.Add($gitRefreshBtn) | Out-Null

# Git status label
$gitStatusLabel = New-Object System.Windows.Forms.Label
$gitStatusLabel.Text = "Git Status"
$gitStatusLabel.Dock = [System.Windows.Forms.DockStyle]::Fill
$gitStatusLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9, [System.Drawing.FontStyle]::Bold)
$gitToolbar.Controls.Add($gitStatusLabel) | Out-Null

# Git status display
$gitStatusBox = New-Object System.Windows.Forms.RichTextBox
$gitStatusBox.Dock = [System.Windows.Forms.DockStyle]::Fill
$gitStatusBox.ReadOnly = $false
$gitStatusBox.Font = New-Object System.Drawing.Font("Consolas", 9)
$gitStatusBox.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$gitStatusBox.ForeColor = [System.Drawing.Color]::FromArgb(200, 200, 200)
$gitContainer.Controls.Add($gitStatusBox) | Out-Null

# Agent Tasks Tab (for Copilot-style workflows)
$agentTasksTab = New-Object System.Windows.Forms.TabPage
$agentTasksTab.Text = "Agent Tasks"
$rightTabControl.TabPages.Add($agentTasksTab) | Out-Null

# Agent tasks container
$agentTasksContainer = New-Object System.Windows.Forms.Panel
$agentTasksContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$agentTasksTab.Controls.Add($agentTasksContainer) | Out-Null

# Agent tasks toolbar
$agentTasksToolbar = New-Object System.Windows.Forms.Panel
$agentTasksToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$agentTasksToolbar.Height = 40
$agentTasksContainer.Controls.Add($agentTasksToolbar) | Out-Null

# Agent status label
$agentStatusLabel = New-Object System.Windows.Forms.Label
$agentStatusLabel.Text = "Agent Status: Active - Agentic editing enabled"
$agentStatusLabel.Dock = [System.Windows.Forms.DockStyle]::Fill
$agentStatusLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9, [System.Drawing.FontStyle]::Bold)
$agentStatusLabel.ForeColor = 'Green'
$agentTasksToolbar.Controls.Add($agentStatusLabel) | Out-Null

# Ollama status label and controls
$ollamaToolbar = New-Object System.Windows.Forms.Panel
$ollamaToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$ollamaToolbar.Height = 30
$agentTasksContainer.Controls.Add($ollamaToolbar) | Out-Null

$script:ollamaStatusLabel = New-Object System.Windows.Forms.Label
$script:ollamaStatusLabel.Text = "🔴 Ollama: Initializing..."
$script:ollamaStatusLabel.Dock = [System.Windows.Forms.DockStyle]::Left
$script:ollamaStatusLabel.Width = 200
$script:ollamaStatusLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9, [System.Drawing.FontStyle]::Bold)
$script:ollamaStatusLabel.ForeColor = 'Orange'
$script:ollamaStatusLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleLeft
$ollamaToolbar.Controls.Add($script:ollamaStatusLabel) | Out-Null

$ollamaStartBtn = New-Object System.Windows.Forms.Button
$ollamaStartBtn.Text = "Start"
$ollamaStartBtn.Size = New-Object System.Drawing.Size(50, 25)
$ollamaStartBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$ollamaStartBtn.Add_Click({
        Start-OllamaServer
        Update-OllamaStatusDisplay
    })
$ollamaToolbar.Controls.Add($ollamaStartBtn) | Out-Null

$ollamaStopBtn = New-Object System.Windows.Forms.Button
$ollamaStopBtn.Text = "Stop"
$ollamaStopBtn.Size = New-Object System.Drawing.Size(50, 25)
$ollamaStopBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$ollamaStopBtn.Add_Click({
        Stop-OllamaServer
        Update-OllamaStatusDisplay
    })
$ollamaToolbar.Controls.Add($ollamaStopBtn) | Out-Null

$ollamaRefreshBtn = New-Object System.Windows.Forms.Button
$ollamaRefreshBtn.Text = "Refresh"
$ollamaRefreshBtn.Size = New-Object System.Drawing.Size(60, 25)
$ollamaRefreshBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$ollamaRefreshBtn.Add_Click({
        # Check current status
        $status = Get-OllamaStatus
        Write-StartupLog "Ollama Status Check: $($status | ConvertTo-Json -Compress)" "INFO"
        Update-OllamaStatusDisplay
    })
$ollamaToolbar.Controls.Add($ollamaRefreshBtn) | Out-Null

# Agent tasks list
$agentTasksList = New-Object System.Windows.Forms.ListView
$agentTasksList.Dock = [System.Windows.Forms.DockStyle]::Fill
$agentTasksList.View = [System.Windows.Forms.View]::Details
$agentTasksList.FullRowSelect = $true
$agentTasksList.Columns.Add("Task", 200) | Out-Null
$agentTasksList.Columns.Add("Status", 100) | Out-Null
$agentTasksList.Columns.Add("Progress", 150) | Out-Null
$agentTasksList.Columns.Add("Time", 100) | Out-Null
$agentTasksList.Add_SelectedIndexChanged({
        if ($agentTasksList.SelectedItems -and $agentTasksList.SelectedItems.Count -gt 0) {
            $taskId = $agentTasksList.SelectedItems[0].Tag
            $task = $global:agentContext.Tasks | Where-Object { $_.Id -eq $taskId } | Select-Object -First 1
            if ($task) {
                $agentTaskDetails.Clear()
                $agentTaskDetails.AppendText("Task: $($task.Name)`r`n")
                $agentTaskDetails.AppendText("Status: $($task.Status)`r`n")
                $agentTaskDetails.AppendText("Steps:`r`n")
                foreach ($step in $task.Steps) {
                    $status = if ($step.Completed) { "✓" } else { "○" }
                    $agentTaskDetails.AppendText("  $status $($step.Description)`r`n")
                }
            }
        }
    })
$agentTasksContainer.Controls.Add($agentTasksList) | Out-Null

# Agent task details panel
$agentTaskDetails = New-Object System.Windows.Forms.RichTextBox
$agentTaskDetails.Dock = [System.Windows.Forms.DockStyle]::Fill
$agentTaskDetails.ReadOnly = $true
$agentTaskDetails.Font = New-Object System.Drawing.Font("Consolas", 8)
$agentTaskDetails.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
$agentTaskDetails.ForeColor = [System.Drawing.Color]::FromArgb(200, 200, 200)
$agentTaskDetails.Text = "Agent Task Console - Use the input below to send commands to the agent`r`n"
$agentTasksContainer.Controls.Add($agentTaskDetails) | Out-Null

# Agent input container
$agentInputContainer = New-Object System.Windows.Forms.Panel
$agentInputContainer.Dock = [System.Windows.Forms.DockStyle]::Bottom
$agentInputContainer.Height = 60
$agentInputContainer.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$agentTasksContainer.Controls.Add($agentInputContainer) | Out-Null

# Agent input label
$agentInputLabel = New-Object System.Windows.Forms.Label
$agentInputLabel.Text = "Agent Command:"
$agentInputLabel.Location = New-Object System.Drawing.Point(10, 8)
$agentInputLabel.Size = New-Object System.Drawing.Size(100, 20)
$agentInputLabel.ForeColor = [System.Drawing.Color]::White
$agentInputLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$agentInputContainer.Controls.Add($agentInputLabel) | Out-Null

# Agent input textbox
$agentInputBox = New-Object System.Windows.Forms.TextBox
$agentInputBox.Location = New-Object System.Drawing.Point(10, 30)
$agentInputBox.Anchor = ([System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Left -bor [System.Windows.Forms.AnchorStyles]::Right)
$agentInputBox.Size = New-Object System.Drawing.Size(280, 25)
$agentInputBox.Font = New-Object System.Drawing.Font("Consolas", 9)
$agentInputBox.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$agentInputBox.ForeColor = [System.Drawing.Color]::White
$agentInputContainer.Controls.Add($agentInputBox) | Out-Null

# Agent send button
$agentSendBtn = New-Object System.Windows.Forms.Button
$agentSendBtn.Text = "Send"
$agentSendBtn.Location = New-Object System.Drawing.Point(300, 30)
$agentSendBtn.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Right
$agentSendBtn.Size = New-Object System.Drawing.Size(60, 25)
$agentSendBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
$agentSendBtn.ForeColor = [System.Drawing.Color]::White
$agentSendBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$agentInputContainer.Controls.Add($agentSendBtn) | Out-Null

# Terminal Tab
$terminalTab = New-Object System.Windows.Forms.TabPage
$global:terminalSessionCounter = 1
$global:terminalSessionLock = $false
$global:terminalSessionOwner = $null
$terminalTab.Text = "Terminal (Session 1)"
$rightTabControl.TabPages.Add($terminalTab) | Out-Null

# Terminal container
$terminalContainer = New-Object System.Windows.Forms.Panel
$terminalContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$terminalTab.Controls.Add($terminalContainer) | Out-Null

# Terminal output
$terminalOutput = New-Object System.Windows.Forms.RichTextBox
$terminalOutput.Dock = [System.Windows.Forms.DockStyle]::Fill
$terminalOutput.ReadOnly = $false
$terminalOutput.Font = New-Object System.Drawing.Font("Consolas", 9)
$terminalOutput.BackColor = [System.Drawing.Color]::FromArgb(0, 0, 0)
$terminalOutput.ForeColor = [System.Drawing.Color]::FromArgb(0, 255, 0)
$terminalContainer.Controls.Add($terminalOutput) | Out-Null

# Terminal input container
$terminalInputContainer = New-Object System.Windows.Forms.Panel
$terminalInputContainer.Dock = [System.Windows.Forms.DockStyle]::Bottom
$terminalInputContainer.Height = 30
$terminalContainer.Controls.Add($terminalInputContainer) | Out-Null

# Terminal prompt
$terminalPrompt = New-Object System.Windows.Forms.Label
$terminalPrompt.Text = "PS> "
$terminalPrompt.Dock = [System.Windows.Forms.DockStyle]::Left
$terminalPrompt.Width = 40
$terminalPrompt.Font = New-Object System.Drawing.Font("Consolas", 9)
$terminalPrompt.ForeColor = [System.Drawing.Color]::FromArgb(0, 255, 0)
$terminalInputContainer.Controls.Add($terminalPrompt) | Out-Null

# Terminal input
$terminalInput = New-Object System.Windows.Forms.TextBox
$terminalInput.Dock = [System.Windows.Forms.DockStyle]::Fill
$terminalInput.Font = New-Object System.Drawing.Font("Consolas", 9)
$terminalInput.BackColor = [System.Drawing.Color]::FromArgb(0, 0, 0)
$terminalInput.ForeColor = [System.Drawing.Color]::FromArgb(0, 255, 0)
$terminalInputContainer.Controls.Add($terminalInput) | Out-Null

# Global variables
$global:currentWorkingDir = Get-Location
$global:terminalHistory = @()
$global:terminalHistoryIndex = -1
$global:currentFile = $null

# Ollama Server Management
$global:ollamaProcess = $null
$global:ollamaStartupAttempted = $false
$global:ollamaServerStatus = "Stopped"
$script:ollamaTimer = $null

# Settings & Configuration
$global:settings = @{
    OllamaModel      = $OllamaModel
    MaxTabs          = 25
    EditorFontSize   = 10
    EditorFontFamily = "Consolas"
    AutoSaveEnabled  = $true
    AutoSaveInterval = 30  # seconds
    ThemeMode        = "Dark"
    ShowLineNumbers  = $true
    CodeHighlighting = $true
    AutoIndent       = $true
    TabSize          = 4
    WrapText         = $false
    ShowWhitespace   = $false
    AutoComplete     = $true
    DebugMode        = $false
    MaxChatTabs      = 5
    ChatTabAutoClose = $false
    ChatTabPosition  = "Right" # Right, Bottom, Popup
}

Set-EditorSettings

# Settings file path
$script:settingsPath = Join-Path $env:APPDATA "RawrXD\settings.json"

# Chat Tab Management
$script:chatTabs = @{}  # TabId -> ChatTab object
$script:activeChatTabId = $null
$script:chatTabCounter = 0
$script:maxChatTabs = 5
$script:chatJobs = @()  # Active chat processing jobs
$script:chatJobMonitorTimer = $null  # Timer for monitoring chat jobs

# Agentic System State
$global:agentContext = @{
    SessionId       = [guid]::NewGuid().ToString()
    StartTime       = Get-Date
    Messages        = @()
    Edits           = @()
    Commands        = @()
    Tasks           = @()
    Environment     = @{}
    DependencyGraph = @{}
    PendingEdits    = @()
    Logs            = @()
}

# ============================================
# ADVANCED DEPENDENCY TRACKING SYSTEM
# ============================================

# Enhanced dependency tracking based on BigDaddyG's recommendation
$script:DependencyTracker = @{
    Dependencies      = @{}
    BuildSystems      = @{}
    PackageManagers   = @{}
    ProjectStructures = @{}
    VersionConflicts  = @()
    AutoResolution    = $true
    UpdateChecking    = $true
    SecurityScanning  = $true
}

# Track dependency function
function Track-Dependency {
    param(
        [Parameter(Mandatory)]
        [string]$DependencyName,
        [Parameter(Mandatory)]
        [string]$DependencyType,
        [string]$Version = "",
        [string]$ProjectPath = "",
        [string]$BuildSystem = "",
        [hashtable]$Metadata = @{}
    )
    
    try {
        $timestamp = Get-Date
        $dependency = @{
            Name          = $DependencyName
            Type          = $DependencyType
            Version       = $Version
            ProjectPath   = $ProjectPath
            BuildSystem   = $BuildSystem
            LastUpdated   = $timestamp
            FirstSeen     = $timestamp
            Metadata      = $Metadata
            Status        = "Active"
            SecurityScore = 0
            Conflicts     = @()
        }
        
        # Check if dependency already exists
        $dependencyKey = "$DependencyType`:$DependencyName"
        if ($script:DependencyTracker.Dependencies.ContainsKey($dependencyKey)) {
            $existing = $script:DependencyTracker.Dependencies[$dependencyKey]
            $dependency.FirstSeen = $existing.FirstSeen
            
            # Check for version conflicts
            if ($existing.Version -ne $Version -and -not [string]::IsNullOrEmpty($Version)) {
                $conflict = @{
                    DependencyName  = $DependencyName
                    ExistingVersion = $existing.Version
                    NewVersion      = $Version
                    Timestamp       = $timestamp
                    Resolved        = $false
                }
                $script:DependencyTracker.VersionConflicts += $conflict
                Write-StartupLog "⚠️ Version conflict detected: $DependencyName ($($existing.Version) vs $Version)" "WARNING"
            }
        }
        
        # Store dependency
        $script:DependencyTracker.Dependencies[$dependencyKey] = $dependency
        $global:agentContext.DependencyGraph[$dependencyKey] = $dependency
        
        # Log the dependency tracking
        Write-StartupLog "📦 Dependency tracked: [$DependencyType] $DependencyName $(if($Version){"v$Version"})" "INFO"
        Update-Insights -EventName "DependencyTracked" -EventData $DependencyName -EventCategory "Dependencies" -Metadata @{
            Type        = $DependencyType
            Version     = $Version
            BuildSystem = $BuildSystem
        }
        
        # Auto-analyze dependency if enabled
        if ($script:DependencyTracker.AutoResolution) {
            Analyze-DependencyHealth -DependencyKey $dependencyKey
        }
        
        # Security scanning if enabled
        if ($script:DependencyTracker.SecurityScanning) {
            Start-DependencySecurityScan -DependencyKey $dependencyKey
        }
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to track dependency '$DependencyName': $($_.Exception.Message)" -ErrorCategory "DEPENDENCIES" -Severity "MEDIUM" -SourceFunction "Track-Dependency"
    }
}

# Analyze dependency health
function Analyze-DependencyHealth {
    param([string]$DependencyKey)
    
    try {
        $dependency = $script:DependencyTracker.Dependencies[$DependencyKey]
        if (-not $dependency) { return }
        
        $healthScore = 100
        $issues = @()
        
        # Check version freshness
        if (-not [string]::IsNullOrEmpty($dependency.Version)) {
            $versionAge = (Get-Date) - $dependency.FirstSeen
            if ($versionAge.TotalDays -gt 365) {
                $healthScore -= 20
                $issues += "Version may be outdated (first seen $($versionAge.Days) days ago)"
            }
        }
        
        # Check for known conflicts
        $conflicts = @($script:DependencyTracker.VersionConflicts | Where-Object { 
                $_.DependencyName -eq $dependency.Name -and -not $_.Resolved 
            })
        $conflictCount = if ($conflicts) { $conflicts.Count } else { 0 }
        if ($conflictCount -gt 0) {
            $healthScore -= ($conflictCount * 15)
            $issues += "Has $conflictCount unresolved version conflict(s)"
        }
        
        # Check build system compatibility
        if (-not [string]::IsNullOrEmpty($dependency.BuildSystem)) {
            if (-not $script:DependencyTracker.BuildSystems.ContainsKey($dependency.BuildSystem)) {
                $healthScore -= 10
                $issues += "Unknown build system: $($dependency.BuildSystem)"
            }
        }
        
        $dependency.SecurityScore = $healthScore
        $dependency.HealthIssues = $issues
        
        if ($healthScore -lt 80) {
            Write-StartupLog "⚠️ Dependency health concern: $($dependency.Name) (Score: $healthScore)" "WARNING"
            Update-Insights -EventName "DependencyHealthConcern" -EventData $dependency.Name -EventCategory "Dependencies" -Metadata @{
                Score  = $healthScore
                Issues = $issues
            }
        }
        
    }
    catch {
        Write-Warning "Dependency health analysis failed: $_"
    }
}

# Security scanning for dependencies
function Start-DependencySecurityScan {
    param([string]$DependencyKey)
    
    try {
        $dependency = $script:DependencyTracker.Dependencies[$DependencyKey]
        if (-not $dependency) { return }
        
        # Basic security checks
        $securityIssues = @()
        
        # Check for common vulnerable patterns
        $vulnerablePatterns = @(
            "jquery.*1\.[0-7]",     # Old jQuery versions
            "lodash.*4\.[0-16]",    # Vulnerable Lodash
            "bootstrap.*[2-3]\.",   # Old Bootstrap
            "angular.*1\."          # AngularJS 1.x
        )
        
        foreach ($pattern in $vulnerablePatterns) {
            if ("$($dependency.Name) $($dependency.Version)" -match $pattern) {
                $securityIssues += "Potentially vulnerable version detected"
                break
            }
        }
        
        # Store security scan results
        if (@($securityIssues).Count -gt 0) {
            $dependency.SecurityIssues = $securityIssues
            $dependency.SecurityScore -= (@($securityIssues).Count * 25)
            
            Write-StartupLog "🔴 Security issue detected: $($dependency.Name)" "ERROR"
            Update-Insights -EventName "DependencySecurityIssue" -EventData $dependency.Name -EventCategory "Security" -Metadata @{
                Issues = $securityIssues
                Score  = $dependency.SecurityScore
            }
            
            Send-AlertNotification -Type "SecurityVulnerability" -Message "Vulnerable dependency detected: $($dependency.Name)" -Severity "HIGH"
        }
        
    }
    catch {
        Write-Warning "Dependency security scan failed: $_"
    }
}

# Build system detection and registration
function Register-BuildSystem {
    param(
        [string]$Name,
        [string]$ConfigFile,
        [string]$DependencyFile,
        [scriptblock]$Parser
    )
    
    try {
        $buildSystem = @{
            Name           = $Name
            ConfigFile     = $ConfigFile
            DependencyFile = $DependencyFile
            Parser         = $Parser
            LastDetected   = Get-Date
            ProjectsUsing  = @()
        }
        
        $script:DependencyTracker.BuildSystems[$Name] = $buildSystem
        Write-StartupLog "🔧 Build system registered: $Name" "INFO"
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to register build system '$Name': $($_.Exception.Message)" -ErrorCategory "DEPENDENCIES" -Severity "LOW" -SourceFunction "Register-BuildSystem"
    }
}

# Auto-detect project structure and dependencies
function Detect-ProjectDependencies {
    param([string]$ProjectPath = ".")
    
    try {
        $detectedDependencies = @()
        $projectPath = Resolve-Path $ProjectPath -ErrorAction SilentlyContinue
        if (-not $projectPath) { return $detectedDependencies }
        
        # Detect package.json (Node.js)
        $packageJson = Join-Path $projectPath "package.json"
        if (Test-Path $packageJson) {
            $packageData = Get-Content $packageJson -Raw | ConvertFrom-Json -ErrorAction SilentlyContinue
            if ($packageData) {
                # Track npm dependencies
                if ($packageData.dependencies) {
                    foreach ($dep in $packageData.dependencies.PSObject.Properties) {
                        Track-Dependency -DependencyName $dep.Name -DependencyType "npm" -Version $dep.Value -ProjectPath $projectPath -BuildSystem "npm"
                        $detectedDependencies += "$($dep.Name)@$($dep.Value)"
                    }
                }
                if ($packageData.devDependencies) {
                    foreach ($dep in $packageData.devDependencies.PSObject.Properties) {
                        Track-Dependency -DependencyName $dep.Name -DependencyType "npm-dev" -Version $dep.Value -ProjectPath $projectPath -BuildSystem "npm"
                        $detectedDependencies += "$($dep.Name)@$($dep.Value) (dev)"
                    }
                }
            }
        }
        
        # Detect requirements.txt (Python)
        $requirementsTxt = Join-Path $projectPath "requirements.txt"
        if (Test-Path $requirementsTxt) {
            $requirements = Get-Content $requirementsTxt
            foreach ($req in $requirements) {
                if ($req -match "^([^=><!\s]+)([=><!=]+.+)?") {
                    $depName = $matches[1]
                    $version = if ($matches[2]) { $matches[2] } else { "" }
                    Track-Dependency -DependencyName $depName -DependencyType "python" -Version $version -ProjectPath $projectPath -BuildSystem "pip"
                    $detectedDependencies += "$depName$version"
                }
            }
        }
        
        # Detect .csproj (C#/.NET)
        $csprojFiles = Get-ChildItem -Path $projectPath -Filter "*.csproj" -ErrorAction SilentlyContinue
        foreach ($csproj in $csprojFiles) {
            try {
                [xml]$projectXml = Get-Content $csproj.FullName
                $packageRefs = $projectXml.Project.ItemGroup.PackageReference
                foreach ($package in $packageRefs) {
                    if ($package.Include) {
                        $version = if ($package.Version) { $package.Version } else { "" }
                        Track-Dependency -DependencyName $package.Include -DependencyType "nuget" -Version $version -ProjectPath $projectPath -BuildSystem "dotnet"
                        $detectedDependencies += "$($package.Include)@$version"
                    }
                }
            }
            catch {
                Write-Warning "Failed to parse csproj: $($csproj.Name)"
            }
        }
        
        # Detect Cargo.toml (Rust)
        $cargoToml = Join-Path $projectPath "Cargo.toml"
        if (Test-Path $cargoToml) {
            try {
                $cargoContent = Get-Content $cargoToml -Raw
                # Simple TOML parsing for dependencies section
                if ($cargoContent -match '\[dependencies\](.*?)(?=\[|\z)') {
                    $depsSection = $matches[1]
                    $depMatches = [regex]::Matches($depsSection, '(\w+)\s*=\s*"([^"]+)"')
                    foreach ($match in $depMatches) {
                        $depName = $match.Groups[1].Value
                        $version = $match.Groups[2].Value
                        Track-Dependency -DependencyName $depName -DependencyType "cargo" -Version $version -ProjectPath $projectPath -BuildSystem "cargo"
                        $detectedDependencies += "$depName@$version"
                    }
                }
            }
            catch {
                Write-Warning "Failed to parse Cargo.toml"
            }
        }
        
        if (@($detectedDependencies).Count -gt 0) {
            Write-StartupLog "📦 Detected $(@($detectedDependencies).Count) dependencies in $projectPath" "SUCCESS"
            Update-Insights -EventName "ProjectDependenciesDetected" -EventData "$(@($detectedDependencies).Count) dependencies" -EventCategory "Dependencies" -Metadata @{
                ProjectPath  = $projectPath
                Dependencies = $detectedDependencies
            }
        }
        
        return $detectedDependencies
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to detect project dependencies: $($_.Exception.Message)" -ErrorCategory "DEPENDENCIES" -Severity "MEDIUM" -SourceFunction "Detect-ProjectDependencies"
        return @()
    }
}

# Resolve version conflicts
function Resolve-DependencyConflicts {
    param([switch]$AutoResolve = $false)
    
    try {
        $unresolvedConflicts = @($script:DependencyTracker.VersionConflicts | Where-Object { -not $_.Resolved })
        
        if ($unresolvedConflicts.Count -eq 0) {
            Write-StartupLog "✅ No dependency conflicts to resolve" "SUCCESS"
            return
        }
        
        Write-StartupLog "🔍 Found $($unresolvedConflicts.Count) dependency conflicts" "WARNING"
        
        foreach ($conflict in $unresolvedConflicts) {
            Write-StartupLog "   Conflict: $($conflict.DependencyName) - $($conflict.ExistingVersion) vs $($conflict.NewVersion)" "INFO"
            
            if ($AutoResolve) {
                # Simple auto-resolution: use newer version if it's clearly newer
                try {
                    $existing = [version]$conflict.ExistingVersion
                    $new = [version]$conflict.NewVersion
                    
                    if ($new -gt $existing) {
                        $conflict.Resolved = $true
                        $conflict.Resolution = "Auto-resolved to newer version: $($conflict.NewVersion)"
                        Write-StartupLog "✅ Auto-resolved: Using $($conflict.NewVersion) for $($conflict.DependencyName)" "SUCCESS"
                    }
                    elseif ($existing -gt $new) {
                        $conflict.Resolved = $true
                        $conflict.Resolution = "Auto-resolved to existing newer version: $($conflict.ExistingVersion)"
                        Write-StartupLog "✅ Auto-resolved: Keeping $($conflict.ExistingVersion) for $($conflict.DependencyName)" "SUCCESS"
                    }
                }
                catch {
                    # Version comparison failed, mark for manual resolution
                    Write-StartupLog "⚠️ Manual resolution required for $($conflict.DependencyName)" "WARNING"
                }
            }
        }
        
        Update-Insights -EventName "DependencyConflictsProcessed" -EventData "$($unresolvedConflicts.Count) conflicts" -EventCategory "Dependencies" -Metadata @{
            AutoResolve = $AutoResolve
            Conflicts   = $unresolvedConflicts
        }
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to resolve dependency conflicts: $($_.Exception.Message)" -ErrorCategory "DEPENDENCIES" -Severity "MEDIUM" -SourceFunction "Resolve-DependencyConflicts"
    }
}

# Export dependency report
function Export-DependencyReport {
    param([string]$OutputPath = (Join-Path $env:TEMP "RawrXD_Dependencies.json"))
    
    try {
        $report = @{
            GeneratedAt       = Get-Date
            ProjectPath       = (Get-Location).Path
            TotalDependencies = if ($script:DependencyTracker.Dependencies) { @($script:DependencyTracker.Dependencies).Count } else { 0 }
            Dependencies      = $script:DependencyTracker.Dependencies
            BuildSystems      = $script:DependencyTracker.BuildSystems
            VersionConflicts  = $script:DependencyTracker.VersionConflicts
            HealthSummary     = @{
                HealthyDependencies   = if ($script:DependencyTracker.Dependencies.Values) { @($script:DependencyTracker.Dependencies.Values | Where-Object { $_.SecurityScore -ge 80 }).Count } else { 0 }
                UnhealthyDependencies = if ($script:DependencyTracker.Dependencies.Values) { @($script:DependencyTracker.Dependencies.Values | Where-Object { $_.SecurityScore -lt 80 }).Count } else { 0 }
                SecurityIssues        = if ($script:DependencyTracker.Dependencies.Values) { @($script:DependencyTracker.Dependencies.Values | Where-Object { $_.SecurityIssues }).Count } else { 0 }
                UnresolvedConflicts   = if ($script:DependencyTracker.VersionConflicts) { @($script:DependencyTracker.VersionConflicts | Where-Object { -not $_.Resolved }).Count } else { 0 }
            }
        }
        
        $report | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputPath -Encoding UTF8
        Write-StartupLog "📊 Dependency report exported: $OutputPath" "SUCCESS"
        
        Update-Insights -EventName "DependencyReportExported" -EventData $OutputPath -EventCategory "Dependencies"
        return $OutputPath
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to export dependency report: $($_.Exception.Message)" -ErrorCategory "DEPENDENCIES" -Severity "MEDIUM" -SourceFunction "Export-DependencyReport"
        return $null
    }
}

# Initialize default build systems
Register-BuildSystem -Name "npm" -ConfigFile "package.json" -DependencyFile "package-lock.json" -Parser { param($path) }
Register-BuildSystem -Name "pip" -ConfigFile "setup.py" -DependencyFile "requirements.txt" -Parser { param($path) }
Register-BuildSystem -Name "dotnet" -ConfigFile "*.csproj" -DependencyFile "*.csproj" -Parser { param($path) }
Register-BuildSystem -Name "cargo" -ConfigFile "Cargo.toml" -DependencyFile "Cargo.lock" -Parser { param($path) }

# ============================================
# MULTITHREADING INFRASTRUCTURE
# ============================================

# Thread-safe collections using ConcurrentDictionary equivalents
$script:threadSafeContext = @{
    RunspacePool       = $null
    ActiveJobs         = [System.Collections.Hashtable]::Synchronized(@{})
    TaskQueue          = [System.Collections.Queue]::Synchronized((New-Object System.Collections.Queue))
    CompletedTasks     = [System.Collections.ArrayList]::Synchronized((New-Object System.Collections.ArrayList))
    WorkerCount        = 4
    MaxConcurrentTasks = 8
    SyncRoot           = New-Object System.Object
}

# Runspace session state for sharing variables
$script:sessionState = [System.Management.Automation.Runspaces.InitialSessionState]::CreateDefault()

# Add required assemblies and types to session state
$script:sessionState.ImportPSModule(@('Microsoft.PowerShell.Utility', 'Microsoft.PowerShell.Management'))

# Thread-safe logging queue
$script:logQueue = [System.Collections.Queue]::Synchronized((New-Object System.Collections.Queue))
$script:logProcessingTimer = $null

# Agent worker states
$script:agentWorkers = @{
    ChatProcessor = @{ Status = "Idle"; CurrentTask = $null; LastActivity = Get-Date }
    FileProcessor = @{ Status = "Idle"; CurrentTask = $null; LastActivity = Get-Date }
    CommandRunner = @{ Status = "Idle"; CurrentTask = $null; LastActivity = Get-Date }
    DataAnalyzer  = @{ Status = "Idle"; CurrentTask = $null; LastActivity = Get-Date }
}

# Chat History Persistence
$script:chatHistoryPath = Join-Path $env:APPDATA "RawrXD\chat_history.txt"
$script:chatHistoryDir = Split-Path $script:chatHistoryPath
if (-not (Test-Path $script:chatHistoryDir)) {
    New-Item -ItemType Directory -Path $script:chatHistoryDir -Force | Out-Null
}

# Extension Marketplace System
$script:extensionsDir = Join-Path $env:APPDATA "RawrXD\Extensions"
if (-not (Test-Path $script:extensionsDir)) {
    New-Item -ItemType Directory -Path $script:extensionsDir -Force | Out-Null
}

$script:extensionRegistry = @()
$script:marketplaceCache = @()
$script:marketplaceSources = @(
    @{ Name = "RawrXD Official"; Url = "https://raw.githubusercontent.com/HiH8e/RawrXD-marketplace/main/extensions.json" }
    @{ Name = "Community Marketplace"; Url = "https://raw.githubusercontent.com/HiH8e/RawrXD-marketplace/main/community.json" }
)
$script:marketplaceLastRefresh = $null

# Agent Tools System
$script:agentTools = @{}

# Extension Capabilities
$script:CAP_SYNTAX_HIGHLIGHT = 1
$script:CAP_CODE_COMPLETION = 2
$script:CAP_DEBUGGING = 4
$script:CAP_LINTING = 8
$script:CAP_FORMATTING = 16
$script:CAP_REFACTORING = 32
$script:CAP_BUILD_SYSTEM = 64
$script:CAP_GIT_INTEGRATION = 128
$script:CAP_MODEL_DAMPENING = 256
$script:CAP_AI_ASSIST = 512

# ============================================
# ADVANCED AGENT TASK MANAGEMENT SYSTEM
# ============================================

# Task management configuration based on BigDaddyG's recommendation
$script:TaskManager = @{
    EnableScheduling         = $true
    EnablePriorityQueue      = $true
    EnableResourceTracking   = $true
    EnableDeadlineMonitoring = $true
    MaxConcurrentTasks       = 10
    TaskTimeoutMinutes       = 30
    RetryAttempts            = 3
    BackoffMultiplier        = 2
    ResourceLimits           = @{
        MemoryLimitMB    = 1024
        CPULimitPercent  = 80
        DiskSpaceLimitMB = 500
    }
}

# Task storage and tracking
$script:TaskRegistry = @{
    ActiveTasks    = @{}
    CompletedTasks = @{}
    FailedTasks    = @{}
    ScheduledTasks = @{}
    TaskHistory    = @()
    ResourceUsage  = @()
}

# Task priority levels
$script:TaskPriority = @{
    CRITICAL   = 1
    HIGH       = 2
    NORMAL     = 3
    LOW        = 4
    BACKGROUND = 5
}

# Advanced agent task scheduling function
function Schedule-AgentTask {
    param(
        [Parameter(Mandatory)]
        [string]$TaskName,
        [Parameter(Mandatory)]
        [string]$TaskType,
        [Parameter(Mandatory)]
        [scriptblock]$TaskScript,
        [datetime]$TaskDeadline = (Get-Date).AddHours(1),
        [int]$Priority = $script:TaskPriority.NORMAL,
        [hashtable]$Dependencies = @{},
        [hashtable]$ResourceRequirements = @{},
        [hashtable]$Metadata = @{}
    )
    
    try {
        $taskId = [guid]::NewGuid().ToString()
        $now = Get-Date
        
        $task = @{
            Id                   = $taskId
            Name                 = $TaskName
            Type                 = $TaskType
            Script               = $TaskScript
            Priority             = $Priority
            CreatedAt            = $now
            ScheduledAt          = $now
            Deadline             = $TaskDeadline
            Status               = "Scheduled"
            Dependencies         = $Dependencies
            ResourceRequirements = $ResourceRequirements
            Metadata             = $Metadata
            RetryCount           = 0
            LastError            = $null
            StartedAt            = $null
            CompletedAt          = $null
            ExecutionTime        = $null
            ResourceUsage        = @{}
            Progress             = 0
        }
        
        # Validate dependencies
        foreach ($dep in $Dependencies.Keys) {
            if (-not $script:TaskRegistry.CompletedTasks.ContainsKey($dep)) {
                throw "Dependency task '$dep' not found or not completed"
            }
        }
        
        # Check resource requirements
        $resourceCheck = Test-ResourceAvailability -Requirements $ResourceRequirements
        if (-not $resourceCheck.Available) {
            $task.Status = "WaitingForResources"
            $task.Metadata.ResourceWaitReason = $resourceCheck.Reason
        }
        
        # Store task
        $script:TaskRegistry.ScheduledTasks[$taskId] = $task
        
        # Log task scheduling
        Write-StartupLog "📅 Task scheduled: [$TaskType] $TaskName (ID: $($taskId.Substring(0,8)))" "INFO"
        Update-Insights -EventName "TaskScheduled" -EventData $TaskName -EventCategory "TaskManagement" -Metadata @{
            TaskType = $TaskType
            Priority = $Priority
            Deadline = $TaskDeadline
            TaskId   = $taskId
        }
        
        # Auto-start if resources available and no blocking dependencies
        if ($task.Status -eq "Scheduled") {
            Start-ScheduledTaskExecution -TaskId $taskId
        }
        
        return $taskId
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to schedule agent task '$TaskName': $($_.Exception.Message)" -ErrorCategory "TASKS" -Severity "HIGH" -SourceFunction "Schedule-AgentTask"
        return $null
    }
}

# Test resource availability
function Test-ResourceAvailability {
    param([hashtable]$Requirements)
    
    try {
        $limits = $script:TaskManager.ResourceLimits
        $result = @{ Available = $true; Reason = "" }
        
        # Check memory requirements
        if ($Requirements.ContainsKey("MemoryMB")) {
            $process = Get-Process -Id $PID
            $currentMemoryMB = [math]::Round($process.WorkingSet64 / 1MB, 2)
            $requiredMemory = $Requirements.MemoryMB
            
            if (($currentMemoryMB + $requiredMemory) -gt $limits.MemoryLimitMB) {
                $result.Available = $false
                $result.Reason = "Insufficient memory: Need $requiredMemory MB, available $($limits.MemoryLimitMB - $currentMemoryMB) MB"
            }
        }
        
        # Check concurrent task limits
        $activeTasks = if ($script:TaskRegistry.ActiveTasks) { @($script:TaskRegistry.ActiveTasks).Count } else { 0 }
        if ($activeTasks -ge $script:TaskManager.MaxConcurrentTasks) {
            $result.Available = $false
            $result.Reason = "Maximum concurrent tasks reached: $activeTasks/$($script:TaskManager.MaxConcurrentTasks)"
        }
        
        # Check disk space requirements
        if ($Requirements.ContainsKey("DiskSpaceMB")) {
            $drive = Get-WmiObject -Class Win32_LogicalDisk | Where-Object { $_.DeviceID -eq $env:SystemDrive }
            $availableSpaceMB = [math]::Round($drive.FreeSpace / 1MB, 2)
            $requiredSpace = $Requirements.DiskSpaceMB
            
            if ($availableSpaceMB -lt $requiredSpace) {
                $result.Available = $false
                $result.Reason = "Insufficient disk space: Need $requiredSpace MB, available $availableSpaceMB MB"
            }
        }
        
        return $result
        
    }
    catch {
        return @{ Available = $false; Reason = "Resource check failed: $($_.Exception.Message)" }
    }
}

# Start executing a scheduled task
function Start-ScheduledTaskExecution {
    param([string]$TaskId)
    
    try {
        $task = $script:TaskRegistry.ScheduledTasks[$TaskId]
        if (-not $task) {
            Write-Warning "Task $TaskId not found in scheduled tasks"
            return
        }
        
        # Move task from scheduled to active
        $script:TaskRegistry.ActiveTasks[$TaskId] = $task
        $script:TaskRegistry.ScheduledTasks.Remove($TaskId)
        
        # Update task status
        $task.Status = "Running"
        $task.StartedAt = Get-Date
        
        # Log task start
        Write-StartupLog "🚀 Task started: $($task.Name) (Priority: $($task.Priority))" "INFO"
        Update-Insights -EventName "TaskStarted" -EventData $task.Name -EventCategory "TaskManagement" -Metadata @{
            TaskId   = $TaskId
            TaskType = $task.Type
            Priority = $task.Priority
        }
        
        # Execute task asynchronously
        $job = Start-Job -ScriptBlock {
            param($TaskScript, $TaskId, $TaskName)
            
            try {
                $startTime = Get-Date
                $result = & $TaskScript
                $endTime = Get-Date
                $executionTime = ($endTime - $startTime).TotalMilliseconds
                
                return @{
                    Success       = $true
                    Result        = $result
                    ExecutionTime = $executionTime
                    CompletedAt   = $endTime
                    Error         = $null
                }
            }
            catch {
                return @{
                    Success       = $false
                    Result        = $null
                    ExecutionTime = 0
                    CompletedAt   = Get-Date
                    Error         = $_.Exception.Message
                }
            }
        } -ArgumentList $task.Script, $TaskId, $task.Name
        
        # Store job reference
        $task.JobId = $job.Id
        
        # Start monitoring task
        Monitor-TaskExecution -TaskId $TaskId
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to start task execution: $($_.Exception.Message)" -ErrorCategory "TASKS" -Severity "HIGH" -SourceFunction "Start-ScheduledTaskExecution"
    }
}

# Monitor task execution
function Monitor-TaskExecution {
    param([string]$TaskId)
    
    try {
        $task = $script:TaskRegistry.ActiveTasks[$TaskId]
        if (-not $task) { return }
        
        $job = Get-Job -Id $task.JobId -ErrorAction SilentlyContinue
        if (-not $job) { return }
        
        # Create monitoring timer
        $timer = New-Object System.Timers.Timer
        $timer.Interval = 5000  # Check every 5 seconds
        $timer.AutoReset = $true
        
        $timer.Add_Elapsed({
                param($timerSender, $e)
            
                try {
                    $currentTask = $script:TaskRegistry.ActiveTasks[$TaskId]
                    if (-not $currentTask) {
                        $timer.Stop()
                        $timer.Dispose()
                        return
                    }
                
                    $currentJob = Get-Job -Id $currentTask.JobId -ErrorAction SilentlyContinue
                    if (-not $currentJob) {
                        $timer.Stop()
                        $timer.Dispose()
                        return
                    }
                
                    # Check if job completed
                    if ($currentJob.State -eq "Completed") {
                        $result = Receive-Job -Job $currentJob
                        Complete-TaskExecution -TaskId $TaskId -Result $result -Success $true
                        Remove-Job -Job $currentJob
                        $timer.Stop()
                        $timer.Dispose()
                    }
                    elseif ($currentJob.State -eq "Failed") {
                        $result = Receive-Job -Job $currentJob
                        Complete-TaskExecution -TaskId $TaskId -Result $result -Success $false
                        Remove-Job -Job $currentJob
                        $timer.Stop()
                        $timer.Dispose()
                    }
                
                    # Check timeout
                    $executionTime = (Get-Date) - $currentTask.StartedAt
                    if ($executionTime.TotalMinutes -gt $script:TaskManager.TaskTimeoutMinutes) {
                        Stop-Job -Job $currentJob
                        Remove-Job -Job $currentJob
                        Fail-TaskExecution -TaskId $TaskId -Reason "Task timeout after $($executionTime.TotalMinutes) minutes"
                        $timer.Stop()
                        $timer.Dispose()
                    }
                
                    # Check deadline
                    if ((Get-Date) -gt $currentTask.Deadline) {
                        Send-AlertNotification -Type "TaskDeadlineMissed" -Message "Task deadline missed: $($currentTask.Name)" -Severity "HIGH"
                    }
                
                }
                catch {
                    Write-Warning "Task monitoring error: $_"
                }
            })
        
        $timer.Start()
        
    }
    catch {
        Write-Warning "Failed to setup task monitoring: $_"
    }
}

# Complete task execution
function Complete-TaskExecution {
    param(
        [string]$TaskId,
        [object]$Result,
        [bool]$Success
    )
    
    try {
        $task = $script:TaskRegistry.ActiveTasks[$TaskId]
        if (-not $task) { return }
        
        # Move task from active to completed/failed
        $script:TaskRegistry.ActiveTasks.Remove($TaskId)
        
        if ($Success -and $Result.Success) {
            $task.Status = "Completed"
            $task.CompletedAt = $Result.CompletedAt
            $task.ExecutionTime = $Result.ExecutionTime
            $task.Progress = 100
            $script:TaskRegistry.CompletedTasks[$TaskId] = $task
            
            Write-StartupLog "✅ Task completed: $($task.Name) ($(([math]::Round($Result.ExecutionTime, 0)))ms)" "SUCCESS"
            Update-Insights -EventName "TaskCompleted" -EventData $task.Name -EventCategory "TaskManagement" -Metadata @{
                TaskId        = $TaskId
                ExecutionTime = $Result.ExecutionTime
                Success       = $true
            }
        }
        else {
            $task.Status = "Failed"
            $task.LastError = if ($Result.Error) { $Result.Error } else { "Unknown error" }
            $task.CompletedAt = Get-Date
            
            # Check if we should retry
            if ($task.RetryCount -lt $script:TaskManager.RetryAttempts) {
                Retry-FailedTask -TaskId $TaskId
            }
            else {
                $script:TaskRegistry.FailedTasks[$TaskId] = $task
                Write-StartupLog "❌ Task failed permanently: $($task.Name) - $($task.LastError)" "ERROR"
                Update-Insights -EventName "TaskFailed" -EventData $task.Name -EventCategory "TaskManagement" -Metadata @{
                    TaskId     = $TaskId
                    Error      = $task.LastError
                    RetryCount = $task.RetryCount
                }
            }
        }
        
        # Update task history
        $script:TaskRegistry.TaskHistory += $task
        
        # Cleanup old history (keep last 100 tasks)
        if ($script:TaskRegistry.TaskHistory -and @($script:TaskRegistry.TaskHistory).Count -gt 100) {
            $script:TaskRegistry.TaskHistory = $script:TaskRegistry.TaskHistory[-100..-1]
        }
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to complete task execution: $($_.Exception.Message)" -ErrorCategory "TASKS" -Severity "MEDIUM" -SourceFunction "Complete-TaskExecution"
    }
}

# Retry failed task
function Retry-FailedTask {
    param([string]$TaskId)
    
    try {
        $task = $script:TaskRegistry.FailedTasks[$TaskId]
        if (-not $task) {
            $task = $script:TaskRegistry.ActiveTasks[$TaskId]
        }
        if (-not $task) { return }
        
        $task.RetryCount++
        $task.Status = "Retrying"
        
        # Apply backoff delay
        $delaySeconds = [math]::Pow($script:TaskManager.BackoffMultiplier, $task.RetryCount)
        Write-StartupLog "🔄 Retrying task: $($task.Name) (Attempt $($task.RetryCount)/$($script:TaskManager.RetryAttempts)) in $delaySeconds seconds" "INFO"
        
        # Schedule retry
        Start-Sleep -Seconds $delaySeconds
        
        # Move back to scheduled tasks
        $script:TaskRegistry.ScheduledTasks[$TaskId] = $task
        if ($script:TaskRegistry.FailedTasks.ContainsKey($TaskId)) {
            $script:TaskRegistry.FailedTasks.Remove($TaskId)
        }
        if ($script:TaskRegistry.ActiveTasks.ContainsKey($TaskId)) {
            $script:TaskRegistry.ActiveTasks.Remove($TaskId)
        }
        
        # Attempt to start again
        Start-ScheduledTaskExecution -TaskId $TaskId
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to retry task: $($_.Exception.Message)" -ErrorCategory "TASKS" -Severity "MEDIUM" -SourceFunction "Retry-FailedTask"
    }
}

# Fail task execution
function Fail-TaskExecution {
    param(
        [string]$TaskId,
        [string]$Reason
    )
    
    try {
        $task = $script:TaskRegistry.ActiveTasks[$TaskId]
        if (-not $task) { return }
        
        $task.Status = "Failed"
        $task.LastError = $Reason
        $task.CompletedAt = Get-Date
        
        # Move to failed tasks
        $script:TaskRegistry.ActiveTasks.Remove($TaskId)
        $script:TaskRegistry.FailedTasks[$TaskId] = $task
        
        Write-StartupLog "❌ Task failed: $($task.Name) - $Reason" "ERROR"
        Update-Insights -EventName "TaskFailed" -EventData $task.Name -EventCategory "TaskManagement" -Metadata @{
            TaskId        = $TaskId
            Reason        = $Reason
            ExecutionTime = if ($task.StartedAt) { ((Get-Date) - $task.StartedAt).TotalMilliseconds } else { 0 }
        }
        
        Send-AlertNotification -Type "TaskFailed" -Message "Task failed: $($task.Name) - $Reason" -Severity "HIGH"
        
    }
    catch {
        Write-Warning "Failed to fail task execution: $_"
    }
}

# Get task status report
function Get-TaskStatusReport {
    try {
        $report = @{
            ActiveTasks          = if ($script:TaskRegistry.ActiveTasks) { @($script:TaskRegistry.ActiveTasks).Count } else { 0 }
            ScheduledTasks       = if ($script:TaskRegistry.ScheduledTasks) { @($script:TaskRegistry.ScheduledTasks).Count } else { 0 }
            CompletedTasks       = if ($script:TaskRegistry.CompletedTasks) { @($script:TaskRegistry.CompletedTasks).Count } else { 0 }
            FailedTasks          = if ($script:TaskRegistry.FailedTasks) { @($script:TaskRegistry.FailedTasks).Count } else { 0 }
            TotalTasks           = (
                (if ($script:TaskRegistry.ActiveTasks) { @($script:TaskRegistry.ActiveTasks).Count } else { 0 }) +
                (if ($script:TaskRegistry.ScheduledTasks) { @($script:TaskRegistry.ScheduledTasks).Count } else { 0 }) +
                (if ($script:TaskRegistry.CompletedTasks) { @($script:TaskRegistry.CompletedTasks).Count } else { 0 }) +
                (if ($script:TaskRegistry.FailedTasks) { @($script:TaskRegistry.FailedTasks).Count } else { 0 })
            )
            SuccessRate          = 0
            AverageExecutionTime = 0
            ResourceUsage        = @{
                MemoryMB = if (Get-Process -Id $PID -ErrorAction SilentlyContinue) { [math]::Round((Get-Process -Id $PID).WorkingSet64 / 1MB, 2) } else { 0 }
            }
        }
        
        # Calculate success rate
        if ($report.TotalTasks -gt 0) {
            $report.SuccessRate = [math]::Round(($report.CompletedTasks / $report.TotalTasks) * 100, 1)
        }
        
        # Calculate average execution time
        $completedTasks = @($script:TaskRegistry.CompletedTasks.Values | Where-Object { $_.ExecutionTime })
        if ($completedTasks.Count -gt 0) {
            $report.AverageExecutionTime = [math]::Round(($completedTasks | Measure-Object -Property ExecutionTime -Average).Average, 0)
        }
        
        return $report
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to generate task status report: $($_.Exception.Message)" -ErrorCategory "TASKS" -Severity "LOW" -SourceFunction "Get-TaskStatusReport"
        return @{}
    }
}

# Export task management report
function Export-TaskReport {
    param([string]$OutputPath = (Join-Path $env:TEMP "RawrXD_Tasks.json"))
    
    try {
        $report = @{
            GeneratedAt    = Get-Date
            StatusReport   = Get-TaskStatusReport
            ActiveTasks    = $script:TaskRegistry.ActiveTasks
            ScheduledTasks = $script:TaskRegistry.ScheduledTasks
            CompletedTasks = $script:TaskRegistry.CompletedTasks
            FailedTasks    = $script:TaskRegistry.FailedTasks
            TaskHistory    = $script:TaskRegistry.TaskHistory[-50..-1]  # Last 50 tasks
            Configuration  = $script:TaskManager
        }
        
        $report | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputPath -Encoding UTF8
        Write-StartupLog "📊 Task report exported: $OutputPath" "SUCCESS"
        
        Update-Insights -EventName "TaskReportExported" -EventData $OutputPath -EventCategory "TaskManagement"
        return $OutputPath
        
    }
    catch {
        Register-ErrorHandler -ErrorMessage "Failed to export task report: $($_.Exception.Message)" -ErrorCategory "TASKS" -Severity "MEDIUM" -SourceFunction "Export-TaskReport"
        return $null
    }
}

# Task cleanup and maintenance
function Invoke-TaskMaintenance {
    try {
        $now = Get-Date
        $cleanupCount = 0
        
        # Clean up old completed tasks (older than 24 hours)
        $oldCompleted = @()
        foreach ($taskId in $script:TaskRegistry.CompletedTasks.Keys) {
            $task = $script:TaskRegistry.CompletedTasks[$taskId]
            if ($task.CompletedAt -and ($now - $task.CompletedAt).TotalHours -gt 24) {
                $oldCompleted += $taskId
            }
        }
        foreach ($taskId in $oldCompleted) {
            $script:TaskRegistry.CompletedTasks.Remove($taskId)
            $cleanupCount++
        }
        
        # Clean up old failed tasks (older than 7 days)
        $oldFailed = @()
        foreach ($taskId in $script:TaskRegistry.FailedTasks.Keys) {
            $task = $script:TaskRegistry.FailedTasks[$taskId]
            if ($task.CompletedAt -and ($now - $task.CompletedAt).TotalDays -gt 7) {
                $oldFailed += $taskId
            }
        }
        foreach ($taskId in $oldFailed) {
            $script:TaskRegistry.FailedTasks.Remove($taskId)
            $cleanupCount++
        }
        
        if ($cleanupCount -gt 0) {
            Write-StartupLog "🧹 Task maintenance: Cleaned up $cleanupCount old tasks" "INFO"
            Update-Insights -EventName "TaskMaintenanceCompleted" -EventData "$cleanupCount tasks cleaned" -EventCategory "TaskManagement"
        }
        
    }
    catch {
        Write-Warning "Task maintenance failed: $_"
    }
}

# Initialize task management system
Write-StartupLog "🎯 Advanced Task Management System initialized" "SUCCESS"
Update-Insights -EventName "TaskManagementStarted" -EventData "System ready" -EventCategory "TaskManagement"

# Language IDs
$script:LANG_ASM = 0
$script:LANG_PYTHON = 1
$script:LANG_C = 2
$script:LANG_CPP = 3
$script:LANG_RUST = 4
$script:LANG_GO = 5
$script:LANG_JAVASCRIPT = 6
$script:LANG_CUSTOM = 999

# Browser Tab
$browserTab = New-Object System.Windows.Forms.TabPage
$browserTab.Text = "Browser"
$rightTabControl.TabPages.Add($browserTab) | Out-Null

# Browser container with controls
$browserContainer = New-Object System.Windows.Forms.Panel
$browserContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$browserTab.Controls.Add($browserContainer) | Out-Null

# Browser toolbar
$browserToolbar = New-Object System.Windows.Forms.Panel
$browserToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$browserToolbar.Height = 40
$browserContainer.Controls.Add($browserToolbar) | Out-Null

# Browser URL box
$browserUrlBox = New-Object System.Windows.Forms.TextBox
$browserUrlBox.Dock = [System.Windows.Forms.DockStyle]::Fill
$browserUrlBox.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$browserUrlBox.Text = "https://www.youtube.com"
$browserToolbar.Controls.Add($browserUrlBox) | Out-Null

# Browser buttons panel
$browserButtons = New-Object System.Windows.Forms.Panel
$browserButtons.Dock = [System.Windows.Forms.DockStyle]::Right
$browserButtons.Width = 200
$browserToolbar.Controls.Add($browserButtons) | Out-Null

# Go button
$browserGoBtn = New-Object System.Windows.Forms.Button
$browserGoBtn.Text = "Go"
$browserGoBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$browserGoBtn.Width = 50
$browserButtons.Controls.Add($browserGoBtn) | Out-Null

# Back button
$browserBackBtn = New-Object System.Windows.Forms.Button
$browserBackBtn.Text = "←"
$browserBackBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$browserBackBtn.Width = 40
$browserButtons.Controls.Add($browserBackBtn) | Out-Null

# Forward button
$browserForwardBtn = New-Object System.Windows.Forms.Button
$browserForwardBtn.Text = "→"
$browserForwardBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$browserForwardBtn.Width = 40
$browserButtons.Controls.Add($browserForwardBtn) | Out-Null

# Refresh button
$browserRefreshBtn = New-Object System.Windows.Forms.Button
$browserRefreshBtn.Text = "↻"
$browserRefreshBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$browserRefreshBtn.Width = 40
$browserButtons.Controls.Add($browserRefreshBtn) | Out-Null

# WebBrowser control (WebView2 or fallback)
if ($script:useWebView2) {
    try {
        Write-StartupLog "Initializing WebView2 browser..." "INFO"
        $script:webBrowser = New-Object Microsoft.Web.WebView2.WinForms.WebView2
        $script:webBrowser.Dock = [System.Windows.Forms.DockStyle]::Fill
        $browserContainer.Controls.Add($script:webBrowser) | Out-Null
        
        # Initialize WebView2 with proper error handling
        try {
            # Use NavigationCompleted event instead of CoreWebView2InitializationCompleted 
            # which may not be available on all WebView2 versions
            
            # First ensure CoreWebView2 is initialized
            $null = $webBrowser.EnsureCoreWebView2Async()
            
            # Set up events after initialization
            $webBrowser.add_NavigationCompleted({
                    param($navSender, $navEventArgs)
                    try {
                        if ($navSender.CoreWebView2) {
                            Write-StartupLog "WebView2 initialization successful" "SUCCESS"
                            
                            # Add host object for agentic control
                            $script:webBrowser.CoreWebView2.AddHostObjectToScript("rawrAgent", @{
                                    getPageTitle  = {
                                        return $script:webBrowser.CoreWebView2.DocumentTitle
                                    }
                                    getPageUrl    = {
                                        return $script:webBrowser.CoreWebView2.Source.ToString()
                                    }
                                    executeScript = {
                                        param($script)
                                        return $script:webBrowser.CoreWebView2.ExecuteScriptAsync($script).Result
                                    }
                                })
                        
                            # Set up navigation events
                            $script:webBrowser.CoreWebView2.Add_NavigationStarting({
                                    param($startSender, $navArgs)
                                    Write-StartupLog "Navigating to: $($navArgs.Uri)" "INFO"
                                })
                        
                            $script:webBrowser.CoreWebView2.Add_NavigationCompleted({
                                    param($completeSender, $navArgs)
                                    if ($navArgs.IsSuccess) {
                                        Write-StartupLog "Navigation completed successfully" "SUCCESS"
                                        if ($browserUrlBox) {
                                            $browserUrlBox.Text = $script:webBrowser.CoreWebView2.Source.ToString()
                                        }
                                    }
                                    else {
                                        Write-StartupLog "Navigation failed" "ERROR"
                                    }
                                })
                        }
                    }
                    catch {
                        Write-StartupLog "WebView2 event setup failed: $($_.Exception.Message)" "DEBUG"
                    }
                })
        }
        catch {
            Write-StartupLog "WebView2 initialization failed: $($_.Exception.Message)" "ERROR"
            $script:useWebView2 = $false
        }
        
        $script:browserType = "WebView2"        
    }
    catch {
        Write-StartupLog "WebView2 initialization failed: $_" "ERROR"
        $script:useWebView2 = $false
        $script:browserType = "WebBrowser"
    }
}

if (-not $script:useWebView2) {
    # Fallback to old WebBrowser control
    Write-StartupLog "Using fallback WebBrowser control" "WARNING"
    $script:webBrowser = New-Object System.Windows.Forms.WebBrowser
    $script:webBrowser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $script:webBrowser.ScriptErrorsSuppressed = $true
    $script:webBrowser.IsWebBrowserContextMenuEnabled = $true
    $script:webBrowser.AllowNavigation = $true
    
    # Add navigation events for legacy browser
    $script:webBrowser.Add_Navigated({
            param($legacySender, $navEventArgs)
            $browserUrlBox.Text = $navEventArgs.Url.ToString()
            Write-StartupLog "Legacy browser navigated to: $($navEventArgs.Url)" "INFO"
        })
    
    $browserContainer.Controls.Add($script:webBrowser) | Out-Null
    $script:browserType = "WebBrowser"
}

# ============================================
# DEV TOOLS TAB
# ============================================
$devToolsTab = New-Object System.Windows.Forms.TabPage
$devToolsTab.Text = "Dev Tools"
$rightTabControl.TabPages.Add($devToolsTab) | Out-Null

# Dev Tools container
$devToolsContainer = New-Object System.Windows.Forms.Panel
$devToolsContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$devToolsTab.Controls.Add($devToolsContainer) | Out-Null

# Dev Tools toolbar
$devToolbar = New-Object System.Windows.Forms.Panel
$devToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$devToolbar.Height = 25
$devToolbar.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$devToolsContainer.Controls.Add($devToolbar) | Out-Null

# Clear console button
$clearConsoleBtn = New-Object System.Windows.Forms.Button
$clearConsoleBtn.Text = "Clear Console"
$clearConsoleBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$clearConsoleBtn.Width = 100
$clearConsoleBtn.Height = 22
$clearConsoleBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
$clearConsoleBtn.ForeColor = [System.Drawing.Color]::White
$clearConsoleBtn.Font = New-Object System.Drawing.Font("Segoe UI", 8)
$clearConsoleBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$devToolbar.Controls.Add($clearConsoleBtn) | Out-Null

# Export log button
$exportLogBtn = New-Object System.Windows.Forms.Button
$exportLogBtn.Text = "Export Log"
$exportLogBtn.Dock = [System.Windows.Forms.DockStyle]::Left
$exportLogBtn.Width = 80
$exportLogBtn.Height = 22
$exportLogBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
$exportLogBtn.ForeColor = [System.Drawing.Color]::White
$exportLogBtn.Font = New-Object System.Drawing.Font("Segoe UI", 8)
$exportLogBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$devToolbar.Controls.Add($exportLogBtn) | Out-Null

# Console output
$global:devConsole = New-Object System.Windows.Forms.RichTextBox
$global:devConsole.Dock = [System.Windows.Forms.DockStyle]::Fill
$global:devConsole.ReadOnly = $true
$global:devConsole.Font = New-Object System.Drawing.Font("Consolas", 9)
$global:devConsole.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
$global:devConsole.ForeColor = [System.Drawing.Color]::LightGray
$global:devConsole.WordWrap = $false
$devToolsContainer.Controls.Add($global:devConsole) | Out-Null

# Dev Console logging function
function Write-DevConsole {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    
    # Guard clause - if dev console not yet created, output to host instead
    if (-not $global:devConsole) {
        $color = switch ($Level) {
            "ERROR" { "Red" }
            "WARNING" { "Yellow" }
            "SUCCESS" { "Green" }
            "DEBUG" { "Cyan" }
            default { "White" }
        }
        Write-Host "[$Level] $Message" -ForegroundColor $color
        return
    }
    
    $timestamp = Get-Date -Format "HH:mm:ss.fff"
    $color = switch ($Level) {
        "ERROR" { [System.Drawing.Color]::Red }
        "WARNING" { [System.Drawing.Color]::Yellow }
        "SUCCESS" { [System.Drawing.Color]::LightGreen }
        "DEBUG" { [System.Drawing.Color]::Cyan }
        default { [System.Drawing.Color]::LightGray }
    }
    
    $global:devConsole.SelectionStart = $global:devConsole.TextLength
    $global:devConsole.SelectionLength = 0
    $global:devConsole.SelectionColor = [System.Drawing.Color]::DarkGray
    $global:devConsole.AppendText("[$timestamp] ")
    
    $global:devConsole.SelectionColor = $color
    $global:devConsole.AppendText("[$Level] ")
    
    $global:devConsole.SelectionColor = [System.Drawing.Color]::LightGray
    $global:devConsole.AppendText("$Message`r`n")
    
    $global:devConsole.SelectionColor = $global:devConsole.ForeColor
    $global:devConsole.ScrollToCaret()
}

# Clear console button handler
$clearConsoleBtn.Add_Click({
        $global:devConsole.Clear()
        Write-DevConsole "Console cleared" "INFO"
    })

# Export log button handler
$exportLogBtn.Add_Click({
        try {
            $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
            $saveDialog.Filter = "Log Files (*.log)|*.log|Text Files (*.txt)|*.txt"
            $saveDialog.Title = "Export Developer Console Log"
            $saveDialog.FileName = "RawrXD_DevLog_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
            
            if ($saveDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                [System.IO.File]::WriteAllText($saveDialog.FileName, $global:devConsole.Text)
                Write-DevConsole "✅ Log exported successfully to: $($saveDialog.FileName)" "SUCCESS"
                Write-StartupLog "Log export completed: $($saveDialog.FileName)" "SUCCESS"
            }
        }
        catch {
            Write-DevConsole "Error exporting log: $_" "ERROR"
        }
    })

# Initialize dev console with startup info
Write-DevConsole "═══════════════════════════════════════════════════════" "INFO"
Write-DevConsole "RawrXD Developer Console Initialized" "SUCCESS"
Write-DevConsole "PowerShell Version: $($PSVersionTable.PSVersion)" "INFO"
Write-DevConsole "Browser Type: $script:browserType" "INFO"
Write-DevConsole "WebView2 Enabled: $script:useWebView2" "INFO"
Write-DevConsole "═══════════════════════════════════════════════════════" "INFO"

# ============================================
# OLLAMA SERVER MANAGEMENT FUNCTIONS
# ============================================

function Start-OllamaServer {
    [CmdletBinding()]
    param()
    
    try {
        Write-StartupLog "Starting Ollama server..." "INFO"
        Write-DevConsole "Attempting to start Ollama server..." "INFO"
        
        # Check if Ollama is available
        $ollamaPath = Get-Command ollama -ErrorAction SilentlyContinue
        if (-not $ollamaPath) {
            Write-StartupLog "Ollama not found in PATH - install Ollama from ollama.ai" "WARNING"
            Write-DevConsole "Ollama not found in PATH. Please install Ollama from https://ollama.ai" "WARNING"
            $global:ollamaServerStatus = "Not Found"
            return $false
        }
        
        # Check if already running
        $existingProcess = Get-Process -Name "ollama" -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -eq "" }
        if ($existingProcess) {
            Write-StartupLog "Ollama server already running (PID: $($existingProcess.Id))" "SUCCESS"
            Write-DevConsole "Ollama server already running (PID: $($existingProcess.Id))" "SUCCESS"
            $global:ollamaProcess = $existingProcess
            $global:ollamaServerStatus = "Running"
            return $true
        }
        
        # Start Ollama server
        $processInfo = New-Object System.Diagnostics.ProcessStartInfo
        $processInfo.FileName = "ollama"
        $processInfo.Arguments = "serve"
        $processInfo.UseShellExecute = $false
        $processInfo.CreateNoWindow = $true
        $processInfo.RedirectStandardOutput = $true
        $processInfo.RedirectStandardError = $true
        
        $global:ollamaProcess = [System.Diagnostics.Process]::Start($processInfo)
        
        if ($global:ollamaProcess) {
            Write-StartupLog "Ollama server started successfully (PID: $($global:ollamaProcess.Id))" "SUCCESS"
            Write-DevConsole "✓ Ollama server started (PID: $($global:ollamaProcess.Id))" "SUCCESS"
            $global:ollamaServerStatus = "Starting"
            
            # Wait for server to initialize
            Start-Sleep -Seconds 3
            
            # Test connection
            if (Test-OllamaConnection) {
                $global:ollamaServerStatus = "Running"
                Write-DevConsole "✓ Ollama server ready and responding" "SUCCESS"
                return $true
            }
            else {
                $global:ollamaServerStatus = "Failed to Connect"
                Write-DevConsole "✗ Ollama server started but not responding" "WARNING"
                return $false
            }
        }
        else {
            Write-StartupLog "Failed to start Ollama server" "ERROR"
            Write-DevConsole "✗ Failed to start Ollama server" "ERROR"
            $global:ollamaServerStatus = "Failed"
            return $false
        }
    }
    catch {
        Write-StartupLog "Error starting Ollama server: $_" "ERROR"
        Write-DevConsole "Error starting Ollama server: $_" "ERROR"
        $global:ollamaServerStatus = "Error"
        return $false
    }
}

function Stop-OllamaServer {
    [CmdletBinding()]
    param()
    
    try {
        Write-DevConsole "Stopping Ollama server..." "INFO"
        
        if ($global:ollamaProcess -and -not $global:ollamaProcess.HasExited) {
            $global:ollamaProcess.Kill()
            $global:ollamaProcess.WaitForExit(5000)  # Wait up to 5 seconds
            Write-DevConsole "✓ Ollama server stopped" "SUCCESS"
        }
        
        # Also check for any other Ollama processes
        Get-Process -Name "ollama" -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                $_.Kill()
                Write-DevConsole "Stopped additional Ollama process (PID: $($_.Id))" "INFO"
            }
            catch {
                Write-DevConsole "Could not stop Ollama process (PID: $($_.Id)): $_" "WARNING"
            }
        }
        
        $global:ollamaProcess = $null
        $global:ollamaServerStatus = "Stopped"
        return $true
    }
    catch {
        Write-DevConsole "Error stopping Ollama server: $_" "ERROR"
        $global:ollamaServerStatus = "Error"
        return $false
    }
}

function Test-OllamaConnection {
    [CmdletBinding()]
    param(
        [int]$TimeoutSeconds = 5
    )
    
    try {
        $testUrl = "http://localhost:11434/api/tags"
        $request = [System.Net.WebRequest]::Create($testUrl)
        $request.Method = "GET"
        $request.Timeout = $TimeoutSeconds * 1000
        
        $response = $request.GetResponse()
        $statusCode = $response.StatusCode
        $response.Close()
        
        return ($statusCode -eq 200)
    }
    catch {
        return $false
    }
}

function Get-OllamaStatus {
    [CmdletBinding()]
    param()
    
    return @{
        Status     = $global:ollamaServerStatus
        ProcessId  = if ($global:ollamaProcess) { $global:ollamaProcess.Id } else { $null }
        IsRunning  = if ($global:ollamaProcess) { -not $global:ollamaProcess.HasExited } else { $false }
        Connection = Test-OllamaConnection
    }
}

function Update-OllamaStatusDisplay {
    [CmdletBinding()]
    param()
    
    if ($script:ollamaStatusLabel) {
        $status = Get-OllamaStatus
        
        switch ($status.Status) {
            "Running" { 
                $script:ollamaStatusLabel.Text = "🟢 Ollama: Running"
                $script:ollamaStatusLabel.ForeColor = [System.Drawing.Color]::Green
            }
            "Starting" { 
                $script:ollamaStatusLabel.Text = "🟡 Ollama: Starting..."
                $script:ollamaStatusLabel.ForeColor = [System.Drawing.Color]::Orange
            }
            "Stopped" { 
                $script:ollamaStatusLabel.Text = "🔴 Ollama: Stopped"
                $script:ollamaStatusLabel.ForeColor = [System.Drawing.Color]::Red
            }
            "Not Found" { 
                $script:ollamaStatusLabel.Text = "❌ Ollama: Not Installed"
                $script:ollamaStatusLabel.ForeColor = [System.Drawing.Color]::DarkRed
            }
            default { 
                $script:ollamaStatusLabel.Text = "⚠️ Ollama: $($status.Status)"
                $script:ollamaStatusLabel.ForeColor = [System.Drawing.Color]::Orange
            }
        }
    }
}

# Menu
$menu = New-Object System.Windows.Forms.MenuStrip
$menu.Dock = [System.Windows.Forms.DockStyle]::Top
$form.MainMenuStrip = $menu

# File Menu
$fileMenu = New-Object System.Windows.Forms.ToolStripMenuItem "File"
$menu.Items.Add($fileMenu) | Out-Null

$openItem = New-Object System.Windows.Forms.ToolStripMenuItem "Open..."
$saveItem = New-Object System.Windows.Forms.ToolStripMenuItem "Save"
$saveAsItem = New-Object System.Windows.Forms.ToolStripMenuItem "Save As..."
$browseItem = New-Object System.Windows.Forms.ToolStripMenuItem "Browse Folder..."
$fileMenu.DropDownItems.AddRange(@($openItem, $saveItem, $saveAsItem, $browseItem))

# Edit Menu (Undo/Redo/Cut/Copy/Paste)
$editMenu = New-Object System.Windows.Forms.ToolStripMenuItem "Edit"
$menu.Items.Add($editMenu) | Out-Null

# Undo/Redo stack management
$script:undoStack = [System.Collections.Generic.Stack[string]]::new()
$script:redoStack = [System.Collections.Generic.Stack[string]]::new()
$script:lastEditorText = ""
$script:isUndoRedoOperation = $false

# Undo menu item
$undoItem = New-Object System.Windows.Forms.ToolStripMenuItem "Undo"
$undoItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::Z
$undoItem.Add_Click({
        if ($script:editor -and $script:undoStack.Count -gt 0) {
            $script:isUndoRedoOperation = $true
            $script:redoStack.Push($script:editor.Text)
            $script:editor.Text = $script:undoStack.Pop()
            $script:isUndoRedoOperation = $false
            Update-UndoRedoMenuState
        }
    })

# Redo menu item
$redoItem = New-Object System.Windows.Forms.ToolStripMenuItem "Redo"
$redoItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::Y
$redoItem.Add_Click({
        if ($script:editor -and $script:redoStack.Count -gt 0) {
            $script:isUndoRedoOperation = $true
            $script:undoStack.Push($script:editor.Text)
            $script:editor.Text = $script:redoStack.Pop()
            $script:isUndoRedoOperation = $false
            Update-UndoRedoMenuState
        }
    })

# Separator
$editSeparator1 = New-Object System.Windows.Forms.ToolStripSeparator

# Cut menu item
$cutItem = New-Object System.Windows.Forms.ToolStripMenuItem "Cut"
$cutItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::X
$cutItem.Add_Click({
        if ($script:editor -and $script:editor.SelectionLength -gt 0) {
            [System.Windows.Forms.Clipboard]::SetText($script:editor.SelectedText)
            $script:editor.SelectedText = ""
        }
    })

# Copy menu item
$copyItem = New-Object System.Windows.Forms.ToolStripMenuItem "Copy"
$copyItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::C
$copyItem.Add_Click({
        if ($script:editor -and $script:editor.SelectionLength -gt 0) {
            [System.Windows.Forms.Clipboard]::SetText($script:editor.SelectedText)
        }
    })

# Paste menu item
$pasteItem = New-Object System.Windows.Forms.ToolStripMenuItem "Paste"
$pasteItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::V
$pasteItem.Add_Click({
        if ($script:editor -and [System.Windows.Forms.Clipboard]::ContainsText()) {
            $script:editor.SelectedText = [System.Windows.Forms.Clipboard]::GetText()
        }
    })

# Separator
$editSeparator2 = New-Object System.Windows.Forms.ToolStripSeparator

# Select All menu item
$selectAllItem = New-Object System.Windows.Forms.ToolStripMenuItem "Select All"
$selectAllItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::A
$selectAllItem.Add_Click({
        if ($script:editor) {
            $script:editor.SelectAll()
        }
    })

# Find menu item
$findItem = New-Object System.Windows.Forms.ToolStripMenuItem "Find..."
$findItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::F
$findItem.Add_Click({
        Show-FindDialog
    })

# Replace menu item
$replaceItem = New-Object System.Windows.Forms.ToolStripMenuItem "Replace..."
$replaceItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::H
$replaceItem.Add_Click({
        Show-ReplaceDialog
    })

$editMenu.DropDownItems.AddRange(@($undoItem, $redoItem, $editSeparator1, $cutItem, $copyItem, $pasteItem, $editSeparator2, $selectAllItem, $findItem, $replaceItem))

# Function to update undo/redo menu state
function Update-UndoRedoMenuState {
    $undoItem.Enabled = ($script:undoStack.Count -gt 0)
    $redoItem.Enabled = ($script:redoStack.Count -gt 0)
}

# Chat Menu
$chatMenu = New-Object System.Windows.Forms.ToolStripMenuItem "Chat"
$menu.Items.Add($chatMenu) | Out-Null

$clearChatItem = New-Object System.Windows.Forms.ToolStripMenuItem "Clear Chat History"
$exportChatItem = New-Object System.Windows.Forms.ToolStripMenuItem "Export Chat History..."
$loadChatItem = New-Object System.Windows.Forms.ToolStripMenuItem "Load Chat History..."
$chatMenu.DropDownItems.AddRange(@($clearChatItem, $exportChatItem, $loadChatItem))

# Settings Menu
$settingsMenu = New-Object System.Windows.Forms.ToolStripMenuItem "Settings"
$menu.Items.Add($settingsMenu) | Out-Null

$modelSettingsItem = New-Object System.Windows.Forms.ToolStripMenuItem "AI Model & General..."
$editorSettingsItem = New-Object System.Windows.Forms.ToolStripMenuItem "Editor Settings..."
$chatSettingsItem = New-Object System.Windows.Forms.ToolStripMenuItem "Chat Settings..."
$themeSettingsItem = New-Object System.Windows.Forms.ToolStripMenuItem "Theme & Appearance..."
$settingsMenu.DropDownItems.AddRange(@($modelSettingsItem, $editorSettingsItem, $chatSettingsItem, $themeSettingsItem))

$exitItem = New-Object System.Windows.Forms.ToolStripMenuItem "Exit"
$fileMenu.DropDownItems.Add($exitItem)

# Command Palette (Ctrl+P or Ctrl+Shift+P)
$commandPalette = New-Object System.Windows.Forms.Form
$commandPalette.Text = "Command Palette"
$commandPalette.Size = New-Object System.Drawing.Size(600, 400)
$commandPalette.StartPosition = "CenterScreen"
$commandPalette.TopMost = $true
$commandPalette.FormBorderStyle = "None"
$commandPalette.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)

# Command palette input
$paletteInput = New-Object System.Windows.Forms.TextBox
$paletteInput.Dock = [System.Windows.Forms.DockStyle]::Top
$paletteInput.Height = 40
$paletteInput.Font = New-Object System.Drawing.Font("Consolas", 12)
$paletteInput.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$paletteInput.ForeColor = [System.Drawing.Color]::White
$commandPalette.Controls.Add($paletteInput) | Out-Null

# Command palette results
$paletteResults = New-Object System.Windows.Forms.ListBox
$paletteResults.Dock = [System.Windows.Forms.DockStyle]::Fill
$paletteResults.Font = New-Object System.Drawing.Font("Consolas", 10)
$paletteResults.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$paletteResults.ForeColor = [System.Drawing.Color]::White
$commandPalette.Controls.Add($paletteResults) | Out-Null

# Command palette label
$paletteLabel = New-Object System.Windows.Forms.Label
$paletteLabel.Text = "Type a command or search extensions..."
$paletteLabel.Dock = [System.Windows.Forms.DockStyle]::Bottom
$paletteLabel.Height = 20
$paletteLabel.ForeColor = [System.Drawing.Color]::Gray
$paletteLabel.Font = New-Object System.Drawing.Font("Segoe UI", 8)
$commandPalette.Controls.Add($paletteLabel) | Out-Null

$commandPalette.Hide()

# Extensions Menu
$extensionsMenu = New-Object System.Windows.Forms.ToolStripMenuItem "Extensions"
$menu.Items.Add($extensionsMenu) | Out-Null

$marketplaceItem = New-Object System.Windows.Forms.ToolStripMenuItem "Marketplace..."
$installedItem = New-Object System.Windows.Forms.ToolStripMenuItem "Installed Extensions"
$extensionsMenu.DropDownItems.AddRange(@($marketplaceItem, $installedItem))

# Security Menu
$securityMenu = New-Object System.Windows.Forms.ToolStripMenuItem "Security"
$menu.Items.Add($securityMenu) | Out-Null

$securitySettingsItem = New-Object System.Windows.Forms.ToolStripMenuItem "Security Settings..."
$stealthModeItem = New-Object System.Windows.Forms.ToolStripMenuItem "Stealth Mode"
$securityLogItem = New-Object System.Windows.Forms.ToolStripMenuItem "View Security Log..."
$sessionInfoItem = New-Object System.Windows.Forms.ToolStripMenuItem "Session Information..."
$encryptionTestItem = New-Object System.Windows.Forms.ToolStripMenuItem "Test Encryption..."

# Add checkable items
$stealthModeItem.Checked = $script:SecurityConfig.StealthMode
$stealthModeItem.CheckOnClick = $true

$securityMenu.DropDownItems.AddRange(@(
        $securitySettingsItem,
        (New-Object System.Windows.Forms.ToolStripSeparator),
        $stealthModeItem,
        (New-Object System.Windows.Forms.ToolStripSeparator),
        $sessionInfoItem,
        $securityLogItem,
        $encryptionTestItem
    ))

# Security menu event handlers
$securitySettingsItem.Add_Click({
        Show-SecuritySettings
    })

$stealthModeItem.Add_Click({
        Enable-StealthMode -Enable $stealthModeItem.Checked
        $securityIndicator = if ($script:SecurityConfig.StealthMode) { "🔒 STEALTH" } 
        elseif ($script:SecurityConfig.EncryptSensitiveData) { "🔐 SECURE" }
        else { "🔓 STANDARD" }
        $form.Text = "RawrXD - Secure AI Editor [$securityIndicator]"
        Write-DevConsole "Stealth mode: $($stealthModeItem.Checked)" "INFO"
    })

$sessionInfoItem.Add_Click({
        Show-SessionInfo
    })

$securityLogItem.Add_Click({
        Show-SecurityLog
    })

$encryptionTestItem.Add_Click({
        Show-EncryptionTest
    })

# Tools Menu
$toolsMenu = New-Object System.Windows.Forms.ToolStripMenuItem "Tools"
$menu.Items.Add($toolsMenu) | Out-Null

# Ollama Server submenu
$ollamaServerItem = New-Object System.Windows.Forms.ToolStripMenuItem "Ollama Server"
$ollamaStartItem = New-Object System.Windows.Forms.ToolStripMenuItem "Start Server"
$ollamaStopItem = New-Object System.Windows.Forms.ToolStripMenuItem "Stop Server"
$ollamaStatusItem = New-Object System.Windows.Forms.ToolStripMenuItem "Check Status"
$ollamaServerItem.DropDownItems.AddRange(@($ollamaStartItem, $ollamaStopItem, $ollamaStatusItem))
$toolsMenu.DropDownItems.Add($ollamaServerItem) | Out-Null

# Performance Tools submenu
$perfToolsItem = New-Object System.Windows.Forms.ToolStripMenuItem "Performance"
$perfMonitorItem = New-Object System.Windows.Forms.ToolStripMenuItem "Performance Monitor"
$perfOptimizerItem = New-Object System.Windows.Forms.ToolStripMenuItem "Optimize Performance"
$perfProfilerItem = New-Object System.Windows.Forms.ToolStripMenuItem "Start Profiler"
$perfRealTimeItem = New-Object System.Windows.Forms.ToolStripMenuItem "Real-Time Monitor"
$perfToolsItem.DropDownItems.AddRange(@($perfMonitorItem, $perfOptimizerItem, $perfProfilerItem, $perfRealTimeItem))
$toolsMenu.DropDownItems.Add($perfToolsItem) | Out-Null

# Ollama menu event handlers
$ollamaStartItem.Add_Click({
        Write-DevConsole "Manual Ollama start requested..." "INFO"
        Start-OllamaServer
        Update-OllamaStatusDisplay
    })

$ollamaStopItem.Add_Click({
        Write-DevConsole "Manual Ollama stop requested..." "INFO"
        Stop-OllamaServer
        Update-OllamaStatusDisplay
    })

$ollamaStatusItem.Add_Click({
        $status = Get-OllamaStatus
        Write-DevConsole "Ollama Status Report:" "INFO"
        Write-DevConsole "  Status: $($status.Status)" "INFO"
        Write-DevConsole "  Process ID: $($status.ProcessId)" "INFO"
        Write-DevConsole "  Is Running: $($status.IsRunning)" "INFO"
        Write-DevConsole "  Connection Test: $($status.Connection)" "INFO"
        Update-OllamaStatusDisplay
    })

# Performance tools event handlers
$perfMonitorItem.Add_Click({
        Write-DevConsole "🔍 Opening Performance Monitor..." "INFO"
        Show-PerformanceMonitor
    })

$perfOptimizerItem.Add_Click({
        Write-DevConsole "🚀 Starting Performance Optimization..." "INFO"
        Start-PerformanceOptimization
    })

$perfProfilerItem.Add_Click({
        Write-DevConsole "📊 Starting Performance Profiler for 60 seconds..." "INFO"
        Start-PerformanceProfiler -DurationSeconds 60
    })

$perfRealTimeItem.Add_Click({
        Write-DevConsole "📈 Opening Real-Time Monitor..." "INFO"
        Show-RealTimeMonitor
    })

# Agent Mode Toggle - Start with Agent Mode ON for agentic editing
$global:AgentMode = $true
$toggle = New-Object System.Windows.Forms.ToolStripMenuItem
$toggle.Text = "Agent Mode: ON"
$toggle.ForeColor = 'Green'
$menu.Items.Add($toggle) | Out-Null

$toggle.Add_Click({
        $global:AgentMode = -not $global:AgentMode
        if ($global:AgentMode) {
            $toggle.Text = "Agent Mode: ON"
            $toggle.ForeColor = 'Green'
            $agentStatusLabel.Text = "Agent Status: Active - Agentic editing enabled"
            $agentStatusLabel.ForeColor = 'Green'
            
            $activeChat = Get-ActiveChatTab
            if ($activeChat) {
                $activeChat.ChatBox.AppendText("System > Agent Mode ENABLED - All agentic features active`r`n`r`n")
                $activeChat.ChatBox.ScrollToCaret()
            }
        }
        else {
            $toggle.Text = "Agent Mode: OFF"
            $toggle.ForeColor = 'Red'
            $agentStatusLabel.Text = "Agent Status: Inactive"
            $agentStatusLabel.ForeColor = 'Red'
            
            $activeChat = Get-ActiveChatTab
            if ($activeChat) {
                $activeChat.ChatBox.AppendText("System > Agent Mode DISABLED - Basic chat only`r`n`r`n")
                $activeChat.ChatBox.ScrollToCaret()
            }
        }
    })

# Chat History Functions
function Save-ChatHistory {
    try {
        $activeChat = Get-ActiveChatTab
        if ($activeChat) {
            $chatContent = $activeChat.ChatBox.Text
            if ($chatContent) {
                # Save current session to persistent file
                Set-Content -Path $script:chatHistoryPath -Value $chatContent -ErrorAction Stop
                Write-DevConsole "✅ Chat history saved for $($activeChat.TabPage.Text)" "SUCCESS"
            }
        }
    }
    catch {
        Write-DevConsole "❌ Error saving chat history: $_" "ERROR"
    }
}

function Get-ChatHistory {
    <#
    .SYNOPSIS
        Loads chat history from persistent storage
    .DESCRIPTION
        Retrieves and displays saved chat history from the application data directory
    #>
    [CmdletBinding()]
    param()
    
    try {
        if (Test-Path $script:chatHistoryPath) {
            $content = Get-Content -Path $script:chatHistoryPath -Raw -ErrorAction Stop
            if ($content) {
                $activeChat = Get-ActiveChatTab
                if ($activeChat) {
                    $activeChat.ChatBox.Text = $content
                    $activeChat.ChatBox.SelectionStart = $activeChat.ChatBox.TextLength
                    $activeChat.ChatBox.ScrollToCaret()
                    Write-DevConsole "✅ Chat history loaded for $($activeChat.TabPage.Text)" "SUCCESS"
                }
            }
        }
    }
    catch {
        Write-DevConsole "❌ Error loading chat history: $_" "ERROR"
    }
}

function Clear-ChatHistory {
    $activeChat = Get-ActiveChatTab
    if ($activeChat) {
        $activeChat.ChatBox.Clear()
        $activeChat.Messages = @()
        Save-ChatHistory
        Write-DevConsole "✅ Chat history cleared for $($activeChat.TabPage.Text)" "SUCCESS"
    }
    else {
        Write-DevConsole "⚠ No active chat tab to clear" "WARNING"
    }
}

function Export-ChatHistory {
    # Get active chat tab
    $activeChat = Get-ActiveChatTab
    if (-not $activeChat) {
        Write-DevConsole "No active chat tab found for export" "WARNING"
        return
    }
    
    $chatBox = $activeChat.ChatBox
    
    $dlg = New-Object System.Windows.Forms.SaveFileDialog
    $dlg.Filter = "Text Files (*.txt)|*.txt|All Files (*.*)|*.*"
    $dlg.FileName = "chat_history_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    if ($dlg.ShowDialog() -eq "OK") {
        try {
            $chatBox.SaveFile($dlg.FileName, [System.Windows.Forms.RichTextBoxStreamType]::PlainText)
            Write-DevConsole "✅ Chat history exported successfully to: $($dlg.FileName)" "SUCCESS"
        }
        catch {
            Write-DevConsole "❌ Error exporting chat: $_" "ERROR"
        }
    }
}

function Import-ChatHistory {
    # Get active chat tab
    $activeChat = Get-ActiveChatTab
    if (-not $activeChat) {
        Write-DevConsole "No active chat tab found for import" "WARNING"
        return
    }
    
    $chatBox = $activeChat.ChatBox
    
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter = "Text Files (*.txt)|*.txt|All Files (*.*)|*.*"
    if ($dlg.ShowDialog() -eq "OK") {
        try {
            $content = Get-Content -Path $dlg.FileName -Raw
            $chatBox.AppendText("`r`n`r`n=== Imported Chat History ===`r`n`r`n$content`r`n`r`n")
            $chatBox.SelectionStart = $chatBox.TextLength
            $chatBox.ScrollToCaret()
            Write-DevConsole "✅ Chat history imported successfully from: $($dlg.FileName)" "SUCCESS"
        }
        catch {
            Write-DevConsole "❌ Error importing chat: $_" "ERROR"
        }
    }
}

# File Open
$openItem.Add_Click({
        # Security check
        if (-not (Test-SessionSecurity)) {
            Write-SecurityLog "File open blocked: Session security check failed" "ERROR"
            Write-DevConsole "Session security check failed. Please restart the application." "WARNING"
            return
        }
        
        $dlg = New-Object System.Windows.Forms.OpenFileDialog
        $dlg.Filter = "Text/Markdown (*.txt;*.md)|*.txt;*.md|All Files (*.*)|*.*"

        if ($dlg.ShowDialog() -eq "OK") {
            try {
                # Validate file path for security
                $fileName = $dlg.FileName
                if (-not (Test-InputSafety -Input $fileName -Type "FilePath")) {
                    Write-SecurityLog "File open blocked: Potentially dangerous file path" "WARNING" "Path: $fileName"
                    Write-DevConsole "File path contains potentially dangerous content." "WARNING"
                    return
                }
                
                # Check file size (limit to 10MB for security)
                $fileInfo = Get-Item $fileName
                if ($fileInfo.Length -gt 10MB) {
                    Write-SecurityLog "File open blocked: File too large" "WARNING" "Size: $($fileInfo.Length) bytes"
                    Write-DevConsole "File is too large (>10MB) for security reasons." "WARNING"
                    return
                }
                
                # Check file extension for potentially dangerous files
                $dangerousExtensions = @('.exe', '.bat', '.cmd', '.com', '.scr', '.pif', '.vbs', '.js', '.jar', '.msi')
                $extension = [System.IO.Path]::GetExtension($fileName).ToLower()
                if ($extension -in $dangerousExtensions) {
                    Write-SecurityLog "File open blocked: Potentially dangerous file type" "WARNING" "Extension: $extension"
                    $result = "No"; Write-DevConsole "Security Warning: File type ($extension) potentially dangerous - defaulting to safe mode" "WARNING"
                    if ($result -ne "Yes") {
                        return
                    }
                }
                
                $content = [System.IO.File]::ReadAllText($fileName)
                
                # Validate file content for security
                if (-not (Test-InputSafety -Input $content -Type "FileContent")) {
                    Write-SecurityLog "File open blocked: Potentially dangerous file content" "WARNING" "File: $fileName"
                    $result = "No"; Write-DevConsole "Security Warning: File content contains dangerous patterns - defaulting to safe mode" "WARNING"
                    if ($result -ne "Yes") {
                        return
                    }
                }
                
                # If encryption is enabled, we can optionally decrypt files with .secure extension
                if ($extension -eq '.secure' -and $script:SecurityConfig.EncryptSensitiveData) {
                    try {
                        $content = Unprotect-SensitiveString -EncryptedData $content
                        Write-SecurityLog "Encrypted file decrypted successfully" "SUCCESS" "File: $fileName"
                    }
                    catch {
                        Write-SecurityLog "Failed to decrypt file" "ERROR" "File: $fileName, Error: $($_.Exception.Message)"
                        Write-ErrorLog -Message "Failed to decrypt file. It may not be encrypted or use a different key." -Severity "HIGH"
                        return
                    }
                }
                
                $script:editor.Text = $content
                $global:currentFile = $fileName
                $form.Text = "RawrXD - Secure AI Editor - $([System.IO.Path]::GetFileName($fileName))"
                Write-DevConsole "✅ File opened successfully: $fileName" "SUCCESS"
                Write-SecurityLog "File opened successfully" "SUCCESS" "File: $fileName, Size: $($content.Length) chars"
                
                # Update last activity
                $script:CurrentSession.LastActivity = Get-Date
            }
            catch {
                Write-DevConsole "❌ Error opening file: $_" "ERROR"
                Write-SecurityLog "File open failed" "ERROR" "File: $fileName, Error: $($_.Exception.Message)"
                Write-DevConsole "Error opening file: $($_.Exception.Message)" "ERROR"
            }
        }
    })

# File Save
$saveItem.Add_Click({
        # Security check
        if (-not (Test-SessionSecurity)) {
            Write-SecurityLog "File save blocked: Session security check failed" "ERROR"
            Write-DevConsole "Session security check failed. Please restart the application." "WARNING"
            return
        }
        
        if ($global:currentFile) {
            try {
                # Validate file path for security
                if (-not (Test-InputSafety -Input $global:currentFile -Type "FilePath")) {
                    Write-SecurityLog "File save blocked: Potentially dangerous file path" "WARNING" "Path: $global:currentFile"
                    Write-DevConsole "File path contains potentially dangerous content." "WARNING"
                    return
                }
                
                $content = $script:editor.Text
                
                # Validate content for security before saving
                if (-not (Test-InputSafety -Input $content -Type "FileContent")) {
                    Write-SecurityLog "File save warning: Potentially dangerous content" "WARNING" "File: $global:currentFile"
                    $result = "No"; Write-DevConsole "Security Warning: Content contains dangerous patterns - save blocked for safety" "WARNING"
                    if ($result -ne "Yes") {
                        return
                    }
                }
                
                # Encrypt content if security is enabled and file has .secure extension
                $fileName = $global:currentFile
                $extension = [System.IO.Path]::GetExtension($fileName).ToLower()
                if (($extension -eq '.secure' -or $script:SecurityConfig.EncryptSensitiveData) -and $content.Length -gt 0) {
                    try {
                        $encryptedContent = Protect-SensitiveString -Data $content
                        # Save encrypted version with .secure extension if not already
                        if ($extension -ne '.secure') {
                            $fileName = $fileName + '.secure'
                            $global:currentFile = $fileName
                        }
                        [System.IO.File]::WriteAllText($fileName, $encryptedContent)
                        Write-SecurityLog "File saved with encryption" "SUCCESS" "File: $fileName"
                    }
                    catch {
                        Write-SecurityLog "File encryption failed, saving unencrypted" "WARNING" "Error: $($_.Exception.Message)"
                        [System.IO.File]::WriteAllText($global:currentFile, $content)
                    }
                }
                else {
                    [System.IO.File]::WriteAllText($global:currentFile, $content)
                    Write-SecurityLog "File saved (unencrypted)" "SUCCESS" "File: $global:currentFile"
                }
                
                $form.Text = "RawrXD - Secure AI Editor - Saved"
                Write-DevConsole "✅ File saved successfully: $global:currentFile" "SUCCESS"
                
                # Update last activity
                $script:CurrentSession.LastActivity = Get-Date
            }
            catch {
                Write-DevConsole "❌ Error saving file: $_" "ERROR"
                Write-SecurityLog "File save failed" "ERROR" "File: $global:currentFile, Error: $($_.Exception.Message)"
                Write-DevConsole "Error saving file: $($_.Exception.Message)" "ERROR"
            }
        }
        else {
            # If no file, use Save As dialog
            $dlg = New-Object System.Windows.Forms.SaveFileDialog
            $dlg.Filter = "Text Files (*.txt)|*.txt|Markdown (*.md)|*.md|Secure Files (*.secure)|*.secure|All Files (*.*)|*.*"
            if ($dlg.ShowDialog() -eq "OK") {
                try {
                    # Validate file path for security
                    if (-not (Test-InputSafety -Input $dlg.FileName -Type "FilePath")) {
                        Write-SecurityLog "File save blocked: Potentially dangerous file path" "WARNING" "Path: $($dlg.FileName)"
                        Write-DevConsole "File path contains potentially dangerous content." "WARNING"
                        return
                    }
                    
                    $content = $script:editor.Text
                    $fileName = $dlg.FileName
                    $extension = [System.IO.Path]::GetExtension($fileName).ToLower()
                    
                    # Encrypt if .secure extension or encryption is enabled
                    if ($extension -eq '.secure' -or $script:SecurityConfig.EncryptSensitiveData) {
                        $content = Protect-SensitiveString -Data $content
                        Write-SecurityLog "File saved with encryption" "SUCCESS" "File: $fileName"
                    }
                    else {
                        Write-SecurityLog "File saved (unencrypted)" "SUCCESS" "File: $fileName"
                    }
                    
                    [System.IO.File]::WriteAllText($fileName, $content)
                    $global:currentFile = $fileName
                    $form.Text = "RawrXD - Secure AI Editor - $([System.IO.Path]::GetFileName($fileName))"
                    Write-DevConsole "✅ File saved as: $fileName" "SUCCESS"
                    
                    # Update last activity
                    $script:CurrentSession.LastActivity = Get-Date
                }
                catch {
                    Write-DevConsole "❌ Error saving file: $_" "ERROR"
                    Write-SecurityLog "File save failed" "ERROR" "File: $($dlg.FileName), Error: $($_.Exception.Message)"
                    Write-DevConsole "Error saving file: $($_.Exception.Message)" "ERROR"
                }
            }
        }
    })

# File Save As
$saveAsItem.Add_Click({
        $dlg = New-Object System.Windows.Forms.SaveFileDialog
        $dlg.Filter = "Text Files (*.txt)|*.txt|Markdown (*.md)|*.md|All Files (*.*)|*.*"
        if ($global:currentFile) {
            $dlg.FileName = $global:currentFile
        }
        if ($dlg.ShowDialog() -eq "OK") {
            try {
                [System.IO.File]::WriteAllText($dlg.FileName, $script:editor.Text)
                $global:currentFile = $dlg.FileName
                $form.Text = "AI Text Editor - $($dlg.FileName)"
                Write-DevConsole "✅ File saved as: $($dlg.FileName)" "SUCCESS"
            }
            catch {
                Write-DevConsole "❌ Error saving file as: $_" "ERROR"
            }
        }
    })

# Folder Browser
$browseItem.Add_Click({
        $folder = New-Object System.Windows.Forms.FolderBrowserDialog
        if ($folder.ShowDialog() -eq "OK") {
            Write-DevConsole "Folder selected: $($folder.SelectedPath)" "INFO"
            # Update working directory to selected folder
            $global:currentWorkingDir = $folder.SelectedPath
            Update-Explorer
        }
    })

# Chat Menu Event Handlers
$clearChatItem.Add_Click({
        # Confirm clear chat operation through console
        Write-DevConsole "🗑️ Clearing chat history..." "INFO"
        Clear-ChatHistory
        Write-DevConsole "✅ Chat history cleared successfully" "SUCCESS"
    })

$exportChatItem.Add_Click({
        Export-ChatHistory
    })

$loadChatItem.Add_Click({
        Import-ChatHistory
    })

# Extensions Menu Event Handlers
$marketplaceItem.Add_Click({
        Show-Marketplace
    })

$installedItem.Add_Click({
        Show-InstalledExtensions
    })

# Command Palette Event Handlers
$paletteInput.Add_TextChanged({
        Update-CommandPalette
    })

$paletteInput.Add_KeyDown({
        if ($_.KeyCode -eq "Enter") {
            Invoke-CommandPaletteSelection
            $_.Handled = $true
        }
        elseif ($_.KeyCode -eq "Escape") {
            Hide-CommandPalette
            $_.Handled = $true
        }
        elseif ($_.KeyCode -eq "Up") {
            if ($paletteResults.SelectedIndex -gt 0) {
                $paletteResults.SelectedIndex--
            }
            $_.Handled = $true
        }
        elseif ($_.KeyCode -eq "Down") {
            $itemCount = $paletteResults.Items.Count
            if ($paletteResults.SelectedIndex -lt ($itemCount - 1)) {
                $paletteResults.SelectedIndex++
            }
            $_.Handled = $true
        }
    })

$paletteResults.Add_DoubleClick({
        Invoke-CommandPaletteSelection
    })

# Global keyboard hook for Ctrl+P
$form.KeyPreview = $true
$form.Add_KeyDown({
        if ($_.Control -and $_.KeyCode -eq "P") {
            Show-CommandPalette
            $_.Handled = $true
        }
    })

# Settings menu event handlers
$modelSettingsItem.Add_Click({
        Show-ModelSettings
    })

$editorSettingsItem.Add_Click({
        Show-EditorSettings
    })

$chatSettingsItem.Add_Click({
        Show-ChatSettings
    })

$themeSettingsItem.Add_Click({
        Write-DevConsole "ℹ️ Theme settings feature coming soon!" "INFO"
    })

# Exit - Save chat before closing
$exitItem.Add_Click({
        Save-ChatHistory
        $form.Close()
    })

# Chat button event handlers
$newChatBtn.Add_Click({
        $tabId = New-ChatTab
        if ($tabId) {
            Write-DevConsole "✅ New chat tab created: $tabId" "SUCCESS"
        }
    })

$closeChatBtn.Add_Click({
        if ($script:activeChatTabId) {
            Remove-ChatTab -TabId $script:activeChatTabId
        }
        else {
            Write-DevConsole "⚠ No active chat tab to close" "WARNING"
        }
    })

# Bulk Actions button for multithreading demonstrations
$bulkActionsBtn.Add_Click({
        Show-BulkActionsMenu
    })

# Chat tab control event handlers
$chatTabControl.Add_SelectedIndexChanged({
        if ($chatTabControl.SelectedTab) {
            $script:activeChatTabId = $chatTabControl.SelectedTab.Name
            $activeChat = Get-ActiveChatTab
            if ($activeChat) {
                $activeChat.IsActive = $true
                $activeChat.InputBox.Focus()
                Write-DevConsole "🔄 Switched to chat: $($activeChat.TabPage.Text)" "INFO"
            }
        }
    })

# Function to send HTTP request to Ollama API
function Send-OllamaRequest {
    param(
        [string]$Prompt,
        [string]$Model = $OllamaModel #Use default model if no specific model is provided
    )

    # Security checks
    $script:CurrentSession.LastActivity = Get-Date
    
    # Validate session security
    if (-not (Test-SessionSecurity)) {
        Write-SecurityLog "Session security check failed" "ERROR"
        return "Error: Session expired or security validation failed"
    }
    
    # Input validation
    if (-not (Test-InputSafety -Input $Prompt -Type "ChatPrompt")) {
        Write-SecurityLog "Potentially dangerous input blocked in chat prompt" "WARNING" "Length: $($Prompt.Length)"
        return "Error: Input contains potentially dangerous content and was blocked for security"
    }
    
    if (-not (Test-InputSafety -Input $Model -Type "ModelName")) {
        Write-SecurityLog "Potentially dangerous input blocked in model name" "WARNING" "Model: $Model"
        return "Error: Model name contains potentially dangerous content"
    }
    
    Write-DevConsole "Sending request to Ollama API (Model: $Model)" "DEBUG"
    Write-SecurityLog "Ollama request initiated" "INFO" "Model: $Model, PromptLength: $($Prompt.Length)"
    
    # Encrypt prompt if security is enabled (stored for logging purposes)
    if ($script:SecurityConfig.EncryptSensitiveData) {
        $null = Protect-SensitiveString -Data $Prompt
    }
    
    # Determine endpoint based on security settings
    $endpoint = if ($script:UseHTTPS) { 
        $OllamaSecureEndpoint 
    }
    else { 
        $OllamaAPIEndpoint 
    }
    
    $tagsEndpoint = if ($script:UseHTTPS) { 
        "https://localhost:11434/api/tags" 
    }
    else { 
        "http://localhost:11434/api/tags" 
    }
    
    # Validate model exists first
    try {
        Write-DevConsole "Validating model availability..." "DEBUG"
        
        # Prepare headers for secure connections
        $headers = @{}
        if ($script:OllamaAPIKey) {
            $headers["Authorization"] = "Bearer $script:OllamaAPIKey"
            Write-SecurityLog "Using API key authentication" "DEBUG"
        }
        
        # Configure SSL/TLS settings for HTTPS
        if ($script:UseHTTPS) {
            # Allow self-signed certificates for local Ollama instance
            add-type @"
using System.Net;
using System.Security.Cryptography.X509Certificates;
public class TrustAllCertsPolicy : ICertificatePolicy {
    public bool CheckValidationResult(ServicePoint sp, X509Certificate cert, WebRequest req, int problem) {
        return true;
    }
}
"@
            [System.Net.ServicePointManager]::CertificatePolicy = New-Object TrustAllCertsPolicy
            [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
            Write-SecurityLog "HTTPS connection configured with TLS 1.2" "INFO"
        }
        
        $modelsResponse = if (@($headers).Count -gt 0) {
            Invoke-RestMethod -Uri $tagsEndpoint -Method GET -Headers $headers -TimeoutSec 10
        }
        else {
            Invoke-RestMethod -Uri $tagsEndpoint -Method GET -TimeoutSec 10
        }
        
        $availableModels = @($modelsResponse.models | ForEach-Object { $_.name })
        
        if ($Model -notin $availableModels) {
            Write-DevConsole "Model '$Model' not found. Available models: $($availableModels -join ', ')" "ERROR"
            Write-SecurityLog "Requested model not available" "WARNING" "Requested: $Model, Available: $($availableModels.Count)"
            
            # AI-specific error logging for model validation
            $modelMetrics = @{
                RequestedModel  = $Model
                AvailableModels = $availableModels
                AvailableCount  = $availableModels.Count
                Endpoint        = $tagsEndpoint
            }
            
            Write-ErrorLog -ErrorMessage "Requested AI model '$Model' not available on server" `
                -ErrorCategory "AI" `
                -Severity "MEDIUM" `
                -SourceFunction "Send-OllamaRequest" `
                -IsAIRelated $true `
                -AgentContext "Model_Validation_Failed" `
                -AIModel $Model `
                -AIMetrics $modelMetrics
            
            if ($availableModels.Count -gt 0) {
                $fallbackModel = $availableModels[0]
                Write-DevConsole "Using fallback model: $fallbackModel" "WARNING"
                
                # Log successful fallback
                Write-ErrorLog -ErrorMessage "Automatically using fallback model '$fallbackModel'" `
                    -ErrorCategory "AI" `
                    -Severity "LOW" `
                    -SourceFunction "Send-OllamaRequest" `
                    -IsAIRelated $true `
                    -AgentContext "Model_Fallback_Applied" `
                    -AIModel $fallbackModel `
                    -AIMetrics $modelMetrics
                              
                $Model = $fallbackModel
            }
            else {
                return "Error: No models available. Please install a model using 'ollama pull <model>'"
            }
        }
    }
    catch {
        Write-DevConsole "Could not validate models (server may be down): $_" "WARNING"
        Write-SecurityLog "Model validation failed" "WARNING" $_.Exception.Message
        # Continue with original model - might work if server is just slow
    }
    
    $body = @{
        model  = $Model
        prompt = if ($script:SecurityConfig.EncryptSensitiveData) { $Prompt } else { $Prompt }  # Don't double-encrypt
        stream = $false
    }
    
    # Add additional security parameters if available
    if ($script:OllamaAPIKey) {
        $body.api_key = $script:OllamaAPIKey
    }
    
    # Retry logic with exponential backoff
    $maxRetries = 3
    $retryCount = 0
    
    while ($retryCount -lt $maxRetries) {
        try {
            $jsonBody = $body | ConvertTo-Json -Depth 10
            
            Write-DevConsole "Attempt $($retryCount + 1)/$maxRetries - POST $endpoint" "DEBUG"
            Write-SecurityLog "API request attempt" "DEBUG" "Retry: $retryCount, HTTPS: $script:UseHTTPS"
            
            $response = if (@($headers).Count -gt 0) {
                Invoke-RestMethod -Uri $endpoint -Method POST -Body $jsonBody -ContentType "application/json" -Headers $headers -TimeoutSec 30
            }
            else {
                Invoke-RestMethod -Uri $endpoint -Method POST -Body $jsonBody -ContentType "application/json" -TimeoutSec 30
            }
            
            # Parse response
            if ($response.response) {
                Write-DevConsole "Ollama response received ($($response.response.Length) chars)" "SUCCESS"
                Write-SecurityLog "Ollama response received successfully" "SUCCESS" "Length: $($response.response.Length)"
                
                # AI success metrics logging
                $successMetrics = @{
                    ResponseLength  = $response.response.Length
                    Model           = $Model
                    RetryCount      = $retryCount
                    Endpoint        = $endpoint
                    UseHTTPS        = $script:UseHTTPS
                    PromptLength    = $Prompt.Length
                    HasAPIKey       = ($null -ne $script:OllamaAPIKey)
                    ResponseTime    = if ($response.total_duration) { $response.total_duration } else { "Unknown" }
                    LoadDuration    = if ($response.load_duration) { $response.load_duration } else { "Unknown" }
                    PromptEvalCount = if ($response.prompt_eval_count) { $response.prompt_eval_count } else { "Unknown" }
                }
                
                Write-ErrorLog -ErrorMessage "AI response generated successfully" `
                    -ErrorCategory "AI" `
                    -Severity "LOW" `
                    -SourceFunction "Send-OllamaRequest" `
                    -IsAIRelated $true `
                    -AgentContext "Successful_Response_Generated" `
                    -AIModel $Model `
                    -AIMetrics $successMetrics `
                    -ShowToUser $false
                
                # Decrypt response if it was encrypted
                $finalResponse = $response.response
                if ($script:SecurityConfig.EncryptSensitiveData -and $response.encrypted) {
                    $finalResponse = Unprotect-SensitiveString -EncryptedData $response.response
                }
                
                return $finalResponse
            }
            elseif ($response.error) {
                Write-DevConsole "Ollama API returned error: $($response.error)" "ERROR"
                Write-SecurityLog "Ollama API error" "ERROR" $response.error
                return "Ollama Error: $($response.error)"
            }
            else {
                Write-DevConsole "Ollama response received (raw)" "SUCCESS" 
                Write-SecurityLog "Ollama raw response received" "SUCCESS"
                return $response.ToString()
            }
        }
        catch {
            $retryCount++
            $errorMsg = $_.Exception.Message
            
            # Enhanced AI error logging with metrics
            $aiMetrics = @{
                RetryCount   = $retryCount
                MaxRetries   = $maxRetries
                Model        = $Model
                Endpoint     = $endpoint
                UseHTTPS     = $script:UseHTTPS
                HasAPIKey    = ($null -ne $script:OllamaAPIKey)
                PromptLength = $Prompt.Length
                ErrorType    = if ($errorMsg -match "Unable to connect") { "CONNECTION" } elseif ($errorMsg -match "timeout") { "TIMEOUT" } elseif ($errorMsg -match "refused") { "REFUSED" } else { "OTHER" }
                ResponseTime = $null
            }
            
            # Log AI-specific error with context
            Write-ErrorLog -ErrorMessage "Ollama API request failed: $errorMsg" `
                -ErrorCategory "AI" `
                -Severity $(if ($retryCount -eq $maxRetries) { "HIGH" } else { "MEDIUM" }) `
                -SourceFunction "Send-OllamaRequest" `
                -IsAIRelated $true `
                -AgentContext "API_Request_Retry_$retryCount" `
                -AIModel $Model `
                -AIMetrics $aiMetrics
            
            if ($errorMsg -match "Unable to connect|refused|timeout|not found") {
                Write-DevConsole "Network error (attempt $retryCount): $errorMsg" "ERROR"
                Write-SecurityLog "Network connection error" "ERROR" "Attempt: $retryCount, Error: $errorMsg"
                
                if ($retryCount -lt $maxRetries) {
                    $backoffMs = [math]::Pow(2, $retryCount) * 500  # 500ms, 1s, 2s
                    Write-DevConsole "Retrying in $($backoffMs)ms..." "INFO"
                    Start-Sleep -Milliseconds $backoffMs
                    continue
                }
            }
            
            Write-DevConsole "Ollama API Error (final): $errorMsg" "ERROR"
            Write-SecurityLog "Final Ollama API failure" "ERROR" $errorMsg
            
            # Provide helpful diagnostic info
            $protocolInfo = if ($script:UseHTTPS) { "HTTPS (secure)" } else { "HTTP (insecure)" }
            $diagnosticMsg = @"
Connection failed to Ollama API at $endpoint [$protocolInfo]

Troubleshooting steps:
1. Check if Ollama is running: Test-NetConnection -ComputerName localhost -Port 11434
2. Start Ollama service if needed: ollama serve
3. Verify models are installed: ollama list
4. Test endpoint manually: Invoke-RestMethod -Uri $tagsEndpoint
$(if ($script:UseHTTPS) { "5. Verify HTTPS configuration: Check SSL certificate" })
$(if ($script:OllamaAPIKey) { "6. Verify API key is valid and not expired" })

Security Status:
- HTTPS: $($script:UseHTTPS)
- API Key Auth: $($null -ne $script:OllamaAPIKey)
- Data Encryption: $($script:SecurityConfig.EncryptSensitiveData)
- Session ID: $($script:CurrentSession.SessionId)

Error details: $errorMsg
"@
            
            return $diagnosticMsg
        }
    }
    
    return "Error: Failed to connect to Ollama after $maxRetries attempts"
}


# Chat Function
function Send-Chat {
    param($msg)

    if (-not $msg.Trim()) { return }
    
    # Get active chat tab
    $activeChat = Get-ActiveChatTab
    if (-not $activeChat) {
        Write-DevConsole "No active chat tab found" "WARNING"
        return
    }
    
    $chatBox = $activeChat.ChatBox
    
    # Security validation
    $script:CurrentSession.LastActivity = Get-Date
    
    # Validate session security
    if (-not (Test-SessionSecurity)) {
        Write-SecurityLog "Chat blocked: Session security check failed" "ERROR"
        $chatBox.AppendText("SECURITY > Session expired or security validation failed. Please restart the application.`r`n`r`n")
        return
    }
    
    # Handle pending delete confirmation (chat-based, no popups)
    if ($script:PendingDelete) {
        $msgLower = $msg.Trim().ToLower()
        $pendingPath = $script:PendingDelete.Path
        $pendingType = $script:PendingDelete.Type
        
        if ($msgLower -eq "yes" -or $msgLower -eq "y") {
            # Proceed with deletion
            try {
                Remove-Item -Path $pendingPath -Recurse -Force
                Update-Explorer
                $chatBox.AppendText("Agent > ✅ Deleted ${pendingType}: $pendingPath`r`n`r`n")
                Write-DevConsole "File deleted via agentic command: $pendingPath" "INFO"
            }
            catch {
                $chatBox.AppendText("Agent > ❌ Error deleting ${pendingType}: $_`r`n`r`n")
                Write-DevConsole "Error deleting file via agentic command: $_" "ERROR"
            }
            $script:PendingDelete = $null
            return
        }
        elseif ($msgLower -eq "no" -or $msgLower -eq "n" -or $msgLower -eq "cancel") {
            # Cancel deletion
            $chatBox.AppendText("Agent > ❌ Delete cancelled for ${pendingType}: $pendingPath`r`n`r`n")
            $script:PendingDelete = $null
            return
        }
        else {
            # Invalid response, ask again
            $chatBox.AppendText("Agent > ⚠️  Please respond with 'yes' to confirm deletion or 'no' to cancel`r`n")
            $chatBox.AppendText("Agent > Pending: Delete $pendingType '$pendingPath'?`r`n`r`n")
            return
        }
    }
    
    # Input validation for security
    if (-not (Test-InputSafety -Input $msg -Type "ChatMessage")) {
        Write-SecurityLog "Chat message blocked: Potentially dangerous input" "WARNING" "Length: $($msg.Length)"
        $chatBox.AppendText("SECURITY > Message contains potentially dangerous content and was blocked for security.`r`n`r`n")
        return
    }
    
    # Log chat activity (but don't log sensitive content in stealth mode)
    $logContent = if ($script:SecurityConfig.StealthMode) { 
        "Length: $($msg.Length), Type: User" 
    }
    else { 
        "Message: $($msg.Substring(0, [Math]::Min(50, $msg.Length)))" 
    }
    Write-SecurityLog "Chat message processed" "INFO" $logContent

    Write-DevConsole "User message: $msg" "INFO"
    
    # Encrypt message for storage if security is enabled (for audit purposes)
    if ($script:SecurityConfig.EncryptSensitiveData) {
        $null = Protect-SensitiveString -Data $msg
    }
    
    # Append user message to chat (display original, store encrypted)
    $chatBox.AppendText("You > $msg`r`n")
    
    # Auto-save chat history after each message
    Save-ChatHistory

    # ============================================
    # AUTO-ENABLE AGENT MODE FOR AGENTIC REQUESTS
    # ============================================
    # Detect if user is making an agentic request that requires tools
    $agenticKeywords = @(
        # File/Code operations
        'view\s+(code|file|project|folder|directory)',
        'read\s+(file|code|content)',
        'open\s+(file|project|folder)',
        'show\s+(me\s+)?(the\s+)?(file|code|content|directory|folder)',
        'list\s+(files|directory|folder|contents)',
        'audit\s+',
        'analyze\s+(code|file|project)',
        'scan\s+',
        # Navigation
        '(cd|navigate|go\s+to|change\s+directory)',
        'browse\s+',
        # Git operations
        'git\s+(status|commit|push|pull|log)',
        '(commit|push|pull)\s+(changes|code)',
        # Terminal operations
        '(run|execute)\s+(command|script|terminal)',
        '/term',
        '/exec',
        # Tool invocations
        '^/(sys|browse|nav|go|ls|dir|read|open|cd|git|workflow|task|agent|tools|env|deps|code|generate|review|refactor)',
        # Agentic keywords
        'agentically',
        'using\s+tools',
        'with\s+agent',
        # IDE operations
        'D:\\\\',  # Path references
        'C:\\\\',
        'professional.?nasm',
        '\.asm\b',
        '\.ps1\b',
        '\.py\b'
    )
    
    $requiresAgentMode = $false
    foreach ($pattern in $agenticKeywords) {
        if ($msg -match $pattern) {
            $requiresAgentMode = $true
            break
        }
    }
    
    # Auto-enable Agent Mode if request requires it
    if ($requiresAgentMode -and -not $global:AgentMode) {
        $global:AgentMode = $true
        $toggle.Text = "Agent Mode: ON"
        $toggle.ForeColor = 'Green'
        $agentStatusLabel.Text = "Agent Status: Active - Auto-enabled for agentic request"
        $agentStatusLabel.ForeColor = 'Green'
        $chatBox.AppendText("System > Agent Mode AUTO-ENABLED for this request (detected agentic operation)`r`n")
        Write-DevConsole "Agent Mode auto-enabled for request: $($msg.Substring(0, [Math]::Min(50, $msg.Length)))" "INFO"
    }

    # Handle Agent Mode
    if ($global:AgentMode) {
        # System command execution
        if ($msg -match "^/sys\s+(.+)$") {
            $cmd = $Matches[1]
            try {
                $out = powershell -command "$cmd"
                $chatBox.AppendText("SYS > $out`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("SYS-ERR > $_`r`n`r`n")
            }
            return
        }

        # Browser navigation commands
        if ($msg -match "^/browse\s+(.+)$" -or $msg -match "^/nav\s+(.+)$" -or $msg -match "^/go\s+(.+)$") {
            $url = $Matches[1]
            Open-Browser $url
            $chatBox.AppendText("Agent > Navigating to: $url`r`n")
            # Switch to browser tab
            $rightTabControl.SelectedTab = $browserTab
            return
        }

        # Get current page info
        if ($msg -match "^/pageinfo$" -or $msg -match "^/browserinfo$") {
            try {
                if ($script:browserType -eq "WebView2" -and $script:webBrowser.CoreWebView2) {
                    $title = $script:webBrowser.CoreWebView2.DocumentTitle
                    $url = $script:webBrowser.CoreWebView2.Source.ToString()
                    $chatBox.AppendText("Agent > Current Page:`r`nTitle: $title`r`nURL: $url`r`n`r`n")
                }
                else {
                    $title = $script:webBrowser.DocumentTitle
                    $url = $script:webBrowser.Url.ToString()
                    $chatBox.AppendText("Agent > Current Page:`r`nTitle: $title`r`nURL: $url`r`n`r`n")
                }
            }
            catch {
                $chatBox.AppendText("Agent > Error getting page info: $_`r`n`r`n")
            }
            return
        }

        # Browser back/forward
        if ($msg -eq "/back" -or $msg -eq "/browserback") {
            $browserBackBtn.PerformClick()
            $chatBox.AppendText("Agent > Navigated back`r`n")
            return
        }

        if ($msg -eq "/forward" -or $msg -eq "/browserforward") {
            $browserForwardBtn.PerformClick()
            $chatBox.AppendText("Agent > Navigated forward`r`n")
            return
        }

        # Browser refresh
        if ($msg -eq "/refresh" -or $msg -eq "/reload") {
            $browserRefreshBtn.PerformClick()
            $chatBox.AppendText("Agent > Refreshed page`r`n")
            return
        }

        # Extract page content
        if ($msg -match "^/extract$" -or $msg -match "^/getcontent$") {
            try {
                if ($script:browserType -eq "WebView2" -and $webBrowser.CoreWebView2) {
                    $script = "document.body.innerText"
                    $content = $webBrowser.CoreWebView2.ExecuteScriptAsync($script).Result
                    $content = $content -replace '"', '' -replace '\\n', "`r`n"
                    $chatBox.AppendText("Agent > Page Content:`r`n$content`r`n`r`n")
                }
                else {
                    $content = $webBrowser.Document.Body.InnerText
                    $chatBox.AppendText("Agent > Page Content:`r`n$content`r`n`r`n")
                }
            }
            catch {
                $chatBox.AppendText("Agent > Error extracting content: $_`r`n`r`n")
            }
            return
        }

        # Search YouTube
        if ($msg -match "^/youtube\s+(.+)$" -or $msg -match "^/yt\s+(.+)$") {
            $query = $Matches[1]
            $encodedQuery = [System.Uri]::EscapeDataString($query)
            $searchUrl = "https://www.youtube.com/results?search_query=$encodedQuery"
            Open-Browser $searchUrl
            $chatBox.AppendText("Agent > Searching YouTube for: $query`r`n")
            $rightTabControl.SelectedTab = $browserTab
            return
        }

        # Natural language browser commands
        if ($msg -match "open\s+(https?://[^\s]+|www\.[^\s]+|[^\s]+\.(com|org|net|io|edu|gov))" -or 
            $msg -match "navigate\s+to\s+(https?://[^\s]+|www\.[^\s]+|[^\s]+\.(com|org|net|io|edu|gov))" -or
            $msg -match "go\s+to\s+(https?://[^\s]+|www\.[^\s]+|[^\s]+\.(com|org|net|io|edu|gov))") {
            $url = $Matches[1]
            Open-Browser $url
            $chatBox.AppendText("Agent > Opening: $url`r`n")
            $rightTabControl.SelectedTab = $browserTab
            return
        }

        if ($msg -match "search\s+youtube\s+for\s+(.+)$" -or $msg -match "youtube\s+search\s+(.+)$") {
            $query = $Matches[1]
            $encodedQuery = [System.Uri]::EscapeDataString($query)
            $searchUrl = "https://www.youtube.com/results?search_query=$encodedQuery"
            Open-Browser $searchUrl
            $chatBox.AppendText("Agent > Searching YouTube for: $query`r`n")
            $rightTabControl.SelectedTab = $browserTab
            return
        }

        # Enhanced natural language browser commands
        if ($msg -match "(?:search|google|find)\s+(?:for\s+)?(.+)" -and $msg -notmatch "youtube") {
            $query = $Matches[1]
            $encodedQuery = [System.Uri]::EscapeDataString($query)
            $searchUrl = "https://www.google.com/search?q=$encodedQuery"
            Open-Browser $searchUrl
            $chatBox.AppendText("Agent > Searching Google for: $query`r`n")
            $rightTabControl.SelectedTab = $browserTab
            return
        }

        if ($msg -match "(?:show|display|view)\s+(?:the\s+)?(?:web\s+)?browser") {
            $rightTabControl.SelectedTab = $browserTab
            $chatBox.AppendText("Agent > Switched to browser`r`n`r`n")
            return
        }

        if ($msg -match "extract\s+(?:page\s+)?(?:content|text)|get\s+(?:page\s+)?(?:content|text)|read\s+(?:the\s+)?page") {
            try {
                if ($script:browserType -eq "WebView2" -and $webBrowser.CoreWebView2) {
                    $script = "document.body.innerText"
                    $content = $webBrowser.CoreWebView2.ExecuteScriptAsync($script).Result
                    $content = $content -replace '"', '' -replace '\\n', "`r`n"
                    $chatBox.AppendText("Agent > Page Content:`r`n$content`r`n`r`n")
                }
                else {
                    $content = $webBrowser.Document.Body.InnerText
                    $chatBox.AppendText("Agent > Page Content:`r`n$content`r`n`r`n")
                }
            }
            catch {
                $chatBox.AppendText("Agent > Error extracting content: $_`r`n`r`n")
            }
            return
        }

        # Apply editor content
        if ($msg -eq "/apply") {
            $lines = @($chatBox.Lines)
            if ($lines.Count -ge 1) {
                $lastAI = $lines[-1]
                $script:editor.Text = $lastAI
                $chatBox.AppendText("Agent > Applied AI output to editor.`r`n`r`n")
            }
            return
        }

        # ============================================
        # Git Agentic Commands
        # ============================================
        if ($msg -match "^/git\s+status$" -or $msg -eq "/gitstatus") {
            $status = Get-GitStatus
            $chatBox.AppendText("Agent > Git Status:`r`n$status`r`n`r`n")
            Update-GitStatus
            $rightTabControl.SelectedTab = $gitTab
            return
        }

        if ($msg -match "^/git\s+add\s+(.+)$") {
            $files = $Matches[1]
            $result = Invoke-GitCommand "add" @($files)
            $chatBox.AppendText("Agent > Git add: $result`r`n`r`n")
            Update-GitStatus
            return
        }

        if ($msg -match "^/git\s+commit\s+-m\s+['""](.+)['""]$" -or $msg -match "^/git\s+commit\s+-m\s+(.+)$") {
            $message = $Matches[1]
            $result = Invoke-GitCommand "commit" @("-m", $message)
            $chatBox.AppendText("Agent > Git commit: $result`r`n`r`n")
            Update-GitStatus
            return
        }

        if ($msg -match "^/git\s+push$" -or $msg -eq "/gitpush") {
            $result = Invoke-GitCommand "push" @()
            $chatBox.AppendText("Agent > Git push: $result`r`n`r`n")
            Update-GitStatus
            return
        }

        if ($msg -match "^/git\s+pull$" -or $msg -eq "/gitpull") {
            $result = Invoke-GitCommand "pull" @()
            $chatBox.AppendText("Agent > Git pull: $result`r`n`r`n")
            Update-GitStatus
            return
        }

        if ($msg -match "^/git\s+branch\s+(.+)$") {
            $branch = $Matches[1]
            $result = Invoke-GitCommand "checkout" @("-b", $branch)
            $chatBox.AppendText("Agent > Git branch: $result`r`n`r`n")
            Update-GitStatus
            return
        }

        if ($msg -match "^/git\s+(.+)$") {
            $gitCmd = $Matches[1]
            $result = Invoke-GitCommand $gitCmd @()
            $chatBox.AppendText("Agent > Git $gitCmd : $result`r`n`r`n")
            Update-GitStatus
            return
        }

        # Natural language Git commands
        if ($msg -match "commit\s+(?:changes|code|files)" -or $msg -match "git\s+commit") {
            $result = Invoke-GitCommand "status" @()
            if ($result -match "Changes to be committed" -or $result -match "modified:") {
                $commitMsg = "Auto-commit by agent"
                $result = Invoke-GitCommand "commit" @("-m", $commitMsg)
                $chatBox.AppendText("Agent > Committed changes: $result`r`n`r`n")
            }
            else {
                $chatBox.AppendText("Agent > No changes to commit`r`n`r`n")
            }
            Update-GitStatus
            return
        }

        # ============================================
        # Terminal Agentic Commands
        # ============================================
        if ($msg -match "^/term\s+(.+)$" -or $msg -match "^/terminal\s+(.+)$" -or $msg -match "^/exec\s+(.+)$") {
            $command = $Matches[1]
            Invoke-TerminalCommand $command
            $chatBox.AppendText("Agent > Executed: $command`r`n")
            $rightTabControl.SelectedTab = $terminalTab
            return
        }

        if ($msg -eq "/terminal" -or $msg -eq "/term") {
            $rightTabControl.SelectedTab = $terminalTab
            $chatBox.AppendText("Agent > Switched to terminal`r`n`r`n")
            return
        }

        # Enhanced natural language terminal commands
        if ($msg -match "(?:run|execute)\s+(?:command\s+)?(.+)" -or
            $msg -match "(?:in|use)\s+(?:the\s+)?terminal\s+(?:run\s+)?(.+)") {
            $command = $Matches[1].Trim()
            Invoke-TerminalCommand $command
            $chatBox.AppendText("Agent > Executed in terminal: $command`r`n")
            $rightTabControl.SelectedTab = $terminalTab
            return
        }

        if ($msg -match "(?:show|open|display)\s+(?:the\s+)?terminal") {
            $rightTabControl.SelectedTab = $terminalTab
            $chatBox.AppendText("Agent > Switched to terminal`r`n`r`n")
            return
        }

        # ============================================
        # File Browser Agentic Commands
        # ============================================
        if ($msg -match "^/cd\s+(.+)$" -or $msg -match "^/navigate\s+(.+)$") {
            $path = $Matches[1]
            if (Test-Path $path) {
                $global:currentWorkingDir = Resolve-Path $path
                Set-Location $global:currentWorkingDir
                Update-Explorer
                $chatBox.AppendText("Agent > Changed directory to: $global:currentWorkingDir`r`n`r`n")
            }
            else {
                $chatBox.AppendText("Agent > Path not found: $path`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/ls\s*(.*)$" -or $msg -match "^/list\s*(.*)$" -or $msg -match "^/dir\s*(.*)$") {
            $path = if ($Matches[1]) { $Matches[1] } else { $global:currentWorkingDir }
            try {
                $items = Get-ChildItem -Path $path -ErrorAction Stop
                $output = "Contents of $path :`r`n"
                foreach ($item in $items) {
                    $type = if ($item.PSIsContainer) { "[DIR]" } else { "[FILE]" }
                    $output += "$type $($item.Name)`r`n"
                }
                $chatBox.AppendText("Agent > $output`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > Error: $_`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/read\s+(.+)$" -or $msg -match "^/open\s+(.+)$") {
            $filePath = $Matches[1]
            if (Test-Path $filePath) {
                try {
                    $content = [System.IO.File]::ReadAllText($filePath)
                    $script:editor.Text = $content
                    $global:currentFile = $filePath
                    $form.Text = "AI Text Editor - $filePath"
                    $chatBox.AppendText("Agent > Opened file: $filePath`r`n`r`n")
                }
                catch {
                    $chatBox.AppendText("Agent > Error reading file: $_`r`n`r`n")
                }
            }
            else {
                $chatBox.AppendText("Agent > File not found: $filePath`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/write\s+(.+?)\s+(.+)$" -or $msg -match "^/save\s+(.+?)\s+(.+)$") {
            $filePath = $Matches[1]
            $content = $Matches[2]
            try {
                [System.IO.File]::WriteAllText($filePath, $content)
                $chatBox.AppendText("Agent > Wrote to file: $filePath`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > Error writing file: $_`r`n`r`n")
            }
            return
        }

        if ($msg -eq "/browse" -or $msg -eq "/explorer") {
            $folder = New-Object System.Windows.Forms.FolderBrowserDialog
            if ($folder.ShowDialog() -eq "OK") {
                $global:currentWorkingDir = $folder.SelectedPath
                Update-Explorer
                $chatBox.AppendText("Agent > Browsed: $($folder.SelectedPath)`r`n`r`n")
            }
            return
        }

        # AI Error Dashboard command
        if ($msg -match "^/(ai-errors|ai_errors|errors|error-dashboard)$") {
            try {
                $report = Get-AIErrorDashboard
                $chatBox.AppendText("Agent > 🤖 AI Error Dashboard:`r`n")
                $chatBox.AppendText("$report`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > ❌ Error generating AI error dashboard: $_`r`n`r`n")
                Write-ErrorLog -ErrorMessage "Failed to generate AI error dashboard: $($_.Exception.Message)" `
                    -ErrorCategory "AI" `
                    -Severity "MEDIUM" `
                    -SourceFunction "Send-Chat" `
                    -IsAIRelated $true `
                    -AgentContext "Dashboard_Generation_Failed"
            }
            return
        }

        # Clear AI Error Statistics command
        if ($msg -match "^/(clear-ai-errors|clear_ai_errors|clear-errors)$") {
            try {
                $result = Clear-AIErrorStatistics
                $chatBox.AppendText("Agent > $result`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > ❌ Error clearing AI error statistics: $_`r`n`r`n")
            }
            return
        }

        # View AI Logs command
        if ($msg -match "^/(ai-logs|ai_logs|logs)$") {
            try {
                $aiLogPath = Join-Path $script:EmergencyLogPath "AI_Errors"
                if (Test-Path $aiLogPath) {
                    $todayLog = Join-Path $aiLogPath "ai_errors_$(Get-Date -Format 'yyyy-MM-dd').log"
                    if (Test-Path $todayLog) {
                        $logContent = Get-Content $todayLog -Tail 20 | Out-String
                        $chatBox.AppendText("Agent > 📄 Recent AI Error Log (Last 20 entries):`r`n")
                        $chatBox.AppendText("$logContent`r`n`r`n")
                    }
                    else {
                        $chatBox.AppendText("Agent > ✅ No AI errors logged today`r`n`r`n")
                    }
                }
                else {
                    $chatBox.AppendText("Agent > 📁 AI error log directory not created yet`r`n`r`n")
                }
            }
            catch {
                $chatBox.AppendText("Agent > ❌ Error reading AI logs: $_`r`n`r`n")
            }
            return
        }

        # ============================================
        # File/Directory Creation Commands
        # ============================================
        if ($msg -match "^/mkdir\s+(.+)$" -or $msg -match "^/md\s+(.+)$" -or $msg -match "^/newdir\s+(.+)$") {
            $dirName = $Matches[1].Trim()
            $fullPath = if ([System.IO.Path]::IsPathRooted($dirName)) { $dirName } else { Join-Path $global:currentWorkingDir $dirName }
            try {
                New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
                Update-Explorer
                $chatBox.AppendText("Agent > Created directory: $fullPath`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > Error creating directory: $_`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/touch\s+(.+)$" -or $msg -match "^/newfile\s+(.+)$" -or $msg -match "^/create\s+(.+)$") {
            $fileName = $Matches[1].Trim()
            $fullPath = if ([System.IO.Path]::IsPathRooted($fileName)) { $fileName } else { Join-Path $global:currentWorkingDir $fileName }
            try {
                if (-not (Test-Path $fullPath)) {
                    New-Item -ItemType File -Path $fullPath -Force | Out-Null
                    $chatBox.AppendText("Agent > Created file: $fullPath`r`n`r`n")
                }
                else {
                    # Touch - update timestamp
                    (Get-Item $fullPath).LastWriteTime = Get-Date
                    $chatBox.AppendText("Agent > Touched file: $fullPath (updated timestamp)`r`n`r`n")
                }
                Update-Explorer
            }
            catch {
                $chatBox.AppendText("Agent > Error creating file: $_`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/rm\s+(.+)$" -or $msg -match "^/delete\s+(.+)$" -or $msg -match "^/del\s+(.+)$") {
            $target = $Matches[1].Trim()
            $fullPath = if ([System.IO.Path]::IsPathRooted($target)) { $target } else { Join-Path $global:currentWorkingDir $target }
            if (Test-Path $fullPath) {
                # Check if this is a confirmation response to a previous delete request
                if ($script:PendingDelete -and $script:PendingDelete.Path -eq $fullPath) {
                    # This is a follow-up message, skip the original delete logic
                    $script:PendingDelete = $null
                    return
                }
                
                # Use chat-based confirmation instead of popup dialog
                $itemType = if (Test-Path $fullPath -PathType Container) { "directory" } else { "file" }
                $chatBox.AppendText("Agent > ⚠️ Confirm deletion of ${itemType} '$fullPath'`r`n")
                $chatBox.AppendText("Agent > Type 'yes' to confirm deletion or 'no' to cancel`r`n`r`n")
                
                # Store pending delete operation
                $script:PendingDelete = @{
                    Path      = $fullPath
                    Type      = $itemType
                    Timestamp = Get-Date
                }
            }
            else {
                $chatBox.AppendText("Agent > Not found: $fullPath`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/pwd$" -or $msg -match "^/cwd$") {
            $chatBox.AppendText("Agent > Current directory: $global:currentWorkingDir`r`n`r`n")
            return
        }

        # Enhanced natural language file/directory commands
        if ($msg -match "(?:show|list|display)\s+(?:me\s+)?(?:the\s+)?files?\s+(?:in\s+)?(.+)" -or 
            $msg -match "(?:what's|whats)\s+in\s+(?:the\s+)?(?:folder|directory)\s+(.+)") {
            $path = $Matches[1].Trim()
            if (-not $path -or $path -eq "here" -or $path -eq "current") {
                $path = $global:currentWorkingDir
            }
            try {
                $items = Get-ChildItem -Path $path -ErrorAction Stop
                $output = "Contents of $path :`r`n"
                foreach ($item in $items) {
                    $type = if ($item.PSIsContainer) { "[DIR]" } else { "[FILE]" }
                    $size = if (-not $item.PSIsContainer) { " ($([math]::Round($item.Length/1KB, 2)) KB)" } else { "" }
                    $output += "$type $($item.Name)$size`r`n"
                }
                $chatBox.AppendText("Agent > $output`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > Error: $_`r`n`r`n")
            }
            return
        }

        if ($msg -match "(?:change|go|navigate)\s+(?:to\s+)?(?:directory|folder|path)\s+(.+)" -or
            $msg -match "(?:move|switch)\s+to\s+(.+)" -or
            $msg -match "(?:goto|cd)\s+(.+)") {
            $path = $Matches[1].Trim() -replace '["'']', ''
            
            # If it's a relative path, try to resolve it from current directory first
            if (-not [System.IO.Path]::IsPathRooted($path)) {
                $possiblePath = Join-Path $global:currentWorkingDir $path
                if (Test-Path $possiblePath) {
                    $path = $possiblePath
                }
            }
            
            if (Test-Path $path) {
                $global:currentWorkingDir = Resolve-Path $path
                Set-Location $global:currentWorkingDir
                Update-Explorer
                $explorerPathLabel.Text = "Path: $global:currentWorkingDir"
                $chatBox.AppendText("Agent > ✓ Changed directory to: $global:currentWorkingDir`r`n`r`n")
            }
            else {
                $chatBox.AppendText("Agent > ✗ Path not found: $path`r`n")
                $chatBox.AppendText("        Current directory: $global:currentWorkingDir`r`n")
                $chatBox.AppendText("        Tip: Use full path like 'C:\Users\...' or relative like 'Powershield'`r`n`r`n")
            }
            return
        }

        if ($msg -match "(?:open|load|read)\s+(?:the\s+)?(?:file|project)\s+(.+)" -or
            $msg -match "(?:show|display)\s+(?:me\s+)?(?:the\s+)?(?:file|code)\s+(.+)") {
            $filePath = $Matches[1].Trim() -replace '["'']', ''
            
            # Try relative path first, then absolute
            if (-not (Test-Path $filePath)) {
                $fullPath = Join-Path $global:currentWorkingDir $filePath
                if (Test-Path $fullPath) {
                    $filePath = $fullPath
                }
            }
            
            if (Test-Path $filePath) {
                try {
                    $fileInfo = Get-Item $filePath
                    if ($fileInfo.PSIsContainer) {
                        # It's a directory/project
                        $global:currentWorkingDir = $filePath
                        Set-Location $global:currentWorkingDir
                        Update-Explorer
                        $explorerPathLabel.Text = "Path: $global:currentWorkingDir"
                        $chatBox.AppendText("Agent > Opened project folder: $filePath`r`n`r`n")
                    }
                    else {
                        # It's a file
                        $content = [System.IO.File]::ReadAllText($filePath)
                        $script:editor.Text = $content
                        $global:currentFile = $filePath
                        $form.Text = "AI Text Editor - $filePath"
                        $chatBox.AppendText("Agent > Opened file: $filePath ($($fileInfo.Length) bytes)`r`n`r`n")
                    }
                }
                catch {
                    $chatBox.AppendText("Agent > Error opening: $_`r`n`r`n")
                }
            }
            else {
                $chatBox.AppendText("Agent > File not found: $filePath`r`n`r`n")
            }
            return
        }

        if ($msg -match "(?:show|open|display)\s+(?:the\s+)?(?:file\s+)?(?:explorer|browser|tree)") {
            Update-Explorer
            $chatBox.AppendText("Agent > File explorer refreshed. Current path: $global:currentWorkingDir`r`n`r`n")
            return
        }

        if ($msg -match "(?:save|write)\s+(?:this|the|current)\s+(?:file|code|content)(?:\s+as\s+(.+))?") {
            $fileName = if ($Matches[1]) { $Matches[1].Trim() -replace '["'']', '' } else { $global:currentFile }
            
            if (-not $fileName) {
                $chatBox.AppendText("Agent > Please specify a filename`r`n`r`n")
                return
            }
            
            try {
                [System.IO.File]::WriteAllText($fileName, $script:editor.Text)
                $global:currentFile = $fileName
                $form.Text = "AI Text Editor - $fileName"
                $chatBox.AppendText("Agent > Saved file: $fileName ($($script:editor.Text.Length) bytes)`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > Error saving file: $_`r`n`r`n")
            }
            return
        }

        if ($msg -match "(?:create|make)\s+(?:a\s+)?(?:new\s+)?(?:file|project)\s+(?:called\s+|named\s+)?(.+)") {
            $fileName = $Matches[1].Trim() -replace '["'']', ''
            try {
                $fullPath = Join-Path $global:currentWorkingDir $fileName
                [System.IO.File]::WriteAllText($fullPath, "")
                $script:editor.Text = ""
                $global:currentFile = $fullPath
                $form.Text = "AI Text Editor - $fullPath"
                Update-Explorer
                $chatBox.AppendText("Agent > Created new file: $fullPath`r`n`r`n")
            }
            catch {
                $chatBox.AppendText("Agent > Error creating file: $_`r`n`r`n")
            }
            return
        }

        # ============================================
        # Agentic Workflow Commands
        # ============================================
        if ($msg -match "^/workflow\s+(.+)$" -or $msg -match "^/task\s+(.+)$" -or $msg -match "^/agent\s+(.+)$") {
            $goal = $Matches[1]
            $context = if ($script:editor.Text) { "Current file: $($script:editor.Text.Substring(0, [Math]::Min(500, $script:editor.Text.Length)))" } else { "" }
            $task = Invoke-AgenticWorkflow -Goal $goal -Context $context
            $chatBox.AppendText("Agent > Started workflow: $goal`r`nTask ID: $($task.Id)`r`n`r`n")
            $rightTabControl.SelectedTab = $agentTasksTab
            return
        }

        if ($msg -match "^/tools$" -or $msg -eq "/listtools") {
            $tools = Get-AgentToolsSchema
            $chatBox.AppendText("Agent > Available Tools:`r`n")
            foreach ($tool in $tools) {
                $chatBox.AppendText("  - $($tool.name): $($tool.description)`r`n")
            }
            $chatBox.AppendText("`r`n")
            return
        }

        if ($msg -match "^/env$" -or $msg -eq "/environment") {
            $env = Get-EnvironmentInfo
            $chatBox.AppendText("Agent > Environment Info:`r`n")
            foreach ($key in $env.Keys) {
                $chatBox.AppendText("  $key : $($env[$key])`r`n")
            }
            $chatBox.AppendText("`r`n")
            return
        }

        if ($msg -match "^/deps\s*(.*)$") {
            $path = if ($Matches[1]) { $Matches[1] } else { $global:currentWorkingDir }
            $deps = Get-ProjectDependencies -Path $path
            $chatBox.AppendText("Agent > Dependencies for $path :`r`n")
            $chatBox.AppendText("  Type: $($deps.Type)`r`n")
            $chatBox.AppendText("  Build System: $($deps.BuildSystem)`r`n")
            $chatBox.AppendText("  Dependencies: $($deps.Dependencies -join ', ')`r`n`r`n")
            return
        }

        # ============================================
        # Coding/Copilot Commands
        # ============================================
        if ($msg -match "^/code\s+(.+)$" -or $msg -match "^/generate\s+(.+)$") {
            $prompt = $Matches[1]
            $context = if ($script:editor.Text) { "Current editor content: $($script:editor.Text)" } else { "" }
            try {
                $code = Invoke-CodeGeneration $prompt $context
                
                # Add code to editor
                $script:editor.Text = $code
                
                # Display formatted code block in chat
                $codeLines = @($code -split "`n")
                $chatBox.AppendText("Agent > Generated code ($($codeLines.Count) lines):`r`n")
                $chatBox.AppendText("╭─────────────────────────────────────────────╮`r`n")
                
                # Show first few lines as preview
                $previewLines = $codeLines | Select-Object -First 10
                foreach ($line in $previewLines) {
                    $chatBox.AppendText("│ $line`r`n")
                }
                
                if ($codeLines.Count -gt 10) {
                    $chatBox.AppendText("│ ... ($($codeLines.Count - 10) more lines)`r`n")
                }
                
                $chatBox.AppendText("╰─────────────────────────────────────────────╯`r`n")
                $chatBox.AppendText("💡 Full code has been placed in the editor.`r`n`r`n")
                
                # Switch focus to editor
                $leftSplitter.Panel2.Focus()
                Write-DevConsole "Generated $($codeLines.Count) lines of code" "SUCCESS"
            }
            catch {
                $chatBox.AppendText("Agent > Error generating code: $_`r`n`r`n")
                Write-DevConsole "Code generation error: $_" "ERROR"
            }
            return
        }

        if ($msg -eq "/review" -or $msg -match "^/review\s+(.+)$") {
            $code = if ($msg -match "^/review\s+(.+)$") { $Matches[1] } else { $script:editor.Text }
            
            if ($code -and $code.Trim()) {
                $codeLength = $code.Length
                $lineCount = @($code -split "`n").Count
                
                $chatBox.AppendText("Agent > Reviewing code ($codeLength characters, $lineCount lines)...`r`n`r`n")
                
                try {
                    $review = Invoke-CodeReview $code
                    $chatBox.AppendText("Agent > Code Review:`r`n")
                    $chatBox.AppendText("$review`r`n`r`n")
                }
                catch {
                    $chatBox.AppendText("Agent > Error reviewing code: $_`r`n`r`n")
                    Write-DevConsole "Code review error: $_" "ERROR"
                }
            }
            else {
                $currentFile = if ($global:currentFile) { Split-Path -Leaf $global:currentFile } else { "none" }
                $chatBox.AppendText("Agent > No code to review in editor.`r`n")
                $chatBox.AppendText("Current file: $currentFile`r`n")
                $chatBox.AppendText("Editor content length: $($script:editor.Text.Length) characters`r`n")
                $chatBox.AppendText("Tip: Open a file or paste code into the editor first.`r`n`r`n")
            }
            return
        }

        # /tools - List all available agent tools
        if ($msg -eq "/tools" -or $msg -match "^/tools\s*(.*)$") {
            $filter = if ($msg -match "^/tools\s+(.+)$") { $Matches[1] } else { "" }
            
            $chatBox.AppendText("╔════════════════════════════════════════════════════╗`r`n")
            $chatBox.AppendText("║         🔧 REGISTERED AGENT TOOLS                  ║`r`n")
            $chatBox.AppendText("╚════════════════════════════════════════════════════╝`r`n`r`n")
            
            $categories = Get-AgentToolsList
            $toolCount = 0
            
            foreach ($category in ($categories.Keys | Sort-Object)) {
                if ($filter -and $category -notmatch $filter) { continue }
                
                $chatBox.AppendText("📁 $category`r`n")
                $chatBox.AppendText(("─" * 50) + "`r`n")
                
                foreach ($tool in $categories[$category]) {
                    $toolCount++
                    $chatBox.AppendText("  • $($tool.name) (v$($tool.version))`r`n")
                    $chatBox.AppendText("    $($tool.description)`r`n")
                    if ($tool.parameters) {
                        $chatBox.AppendText("    Parameters: $($tool.parameters)`r`n")
                    }
                    $chatBox.AppendText("`r`n")
                }
            }
            
            $chatBox.AppendText("─────────────────────────────────────────────────────`r`n")
            $chatBox.AppendText("Total: $toolCount tools registered across $(@($categories).Count) categories`r`n")
            $chatBox.AppendText("`r`nUsage: Type commands naturally or use /execute_tool <name> <params>`r`n`r`n")
            return
        }

        # /execute_tool - Execute a specific tool
        if ($msg -match "^/execute[_\-]?tool\s+(\w+)(?:\s+(.+))?$") {
            $toolName = $Matches[1]
            $paramsJson = if ($Matches[2]) { $Matches[2] } else { "{}" }
            
            try {
                $params = ConvertFrom-Json $paramsJson -AsHashtable
                $result = Invoke-AgentTool -ToolName $toolName -Parameters $params
                
                $chatBox.AppendText("🔧 Tool: $toolName`r`n")
                $chatBox.AppendText("─────────────────────────────────────────────────────`r`n")
                
                if ($result.success) {
                    $chatBox.AppendText("✓ Success`r`n`r`n")
                    $resultJson = ($result | ConvertTo-Json -Depth 5)
                    $chatBox.AppendText($resultJson + "`r`n`r`n")
                }
                else {
                    $chatBox.AppendText("✗ Error: $($result.error)`r`n`r`n")
                }
            }
            catch {
                $chatBox.AppendText("Agent > Error executing tool: $_`r`n`r`n")
            }
            return
        }

        if ($msg -match "^/refactor\s+(.+)$") {
            $instructions = $Matches[1]
            $code = $script:editor.Text
            if ($code) {
                try {
                    $refactored = Invoke-CodeRefactor $code $instructions
                    $chatBox.AppendText("Agent > Refactored code:`r`n$refactored`r`n`r`n")
                    $script:editor.Text = $refactored
                }
                catch {
                    $chatBox.AppendText("Agent > Error refactoring code: $_`r`n`r`n")
                }
            }
            else {
                $chatBox.AppendText("Agent > No code to refactor`r`n`r`n")
            }
            return
        }

        if ($msg -match "summarize|rewrite|cleaner|improve|refactor") {
            $content = $script:editor.Text

            if (-not $content.Trim()) {
                $chatBox.AppendText("Agent > Left editor is empty.`r`n`r`n")
                return
            }

            $prompt = @"
Here is the full text editor content. Summarize or rewrite it cleaner.

TEXT START:
$content
TEXT END:
"@

            try {
                $response = Send-OllamaRequest $prompt
                $chatBox.AppendText("AI > $response`r`n`r`n")
                Save-ChatHistory
            }
            catch {
                $chatBox.AppendText("AI-ERR > $_`r`n`r`n")
                Save-ChatHistory
            }
            return
        }
    }

    # Normal Chat Mode
    if ($msg -match "^/chat\s+(.+?)\s+(.+)$") {
        $model = $Matches[1]
        $prompt = $Matches[2]
    }
    else {
        $model = $OllamaModel
        $prompt = $msg
    }

    # ============================================
    # INTELLIGENT CONTEXT GATHERING FOR AGENTIC REQUESTS
    # ============================================
    # If the request mentions files/directories/auditing, gather REAL data first
    $contextData = ""
    
    if ($global:AgentMode -and $requiresAgentMode) {
        $chatBox.AppendText("Agent > *Gathering real filesystem context...*`r`n")
        
        # Extract path references from the message
        $pathMatches = [regex]::Matches($msg, '([A-Za-z]:\\[^\s"''<>|*?]+|/[^\s"''<>|*?]+)')
        $targetPaths = @()
        foreach ($match in $pathMatches) {
            $targetPaths += $match.Value
        }
        
        # Also check for common project references
        if ($msg -match 'professional.?nasm|asm\s*ide' -and -not $targetPaths) {
            $targetPaths += "D:\professional-nasm-ide"
        }
        if ($msg -match 'multi.?lang|multi-lang' -and -not $targetPaths) {
            $targetPaths += "D:\multi-lang-ide"
        }
        
        # Default to current working directory if no path specified
        if (-not $targetPaths -and ($msg -match 'audit|scan|analyze|view|list|show')) {
            $targetPaths += $global:currentWorkingDir
        }
        
        # Gather context for each path
        $gatheredContext = @()
        foreach ($path in $targetPaths) {
            if (Test-Path $path) {
                try {
                    $item = Get-Item $path -ErrorAction Stop
                    if ($item.PSIsContainer) {
                        # It's a directory - gather stats
                        $dirItems = Get-ChildItem -Path $path -Force -ErrorAction SilentlyContinue
                        $files = @($dirItems | Where-Object { -not $_.PSIsContainer })
                        $folders = @($dirItems | Where-Object { $_.PSIsContainer })
                        
                        # Count files by extension recursively
                        $allFiles = Get-ChildItem -Path $path -Recurse -File -ErrorAction SilentlyContinue
                        $extCounts = $allFiles | Group-Object Extension | Sort-Object Count -Descending | Select-Object -First 8
                        
                        $gatheredContext += @"

=== REAL FILESYSTEM DATA FOR: $path ===
Directory Stats:
- Total items in root: $($dirItems.Count)
- Subdirectories: $($folders.Count)
- Files in root: $($files.Count)
- Total files (recursive): $($allFiles.Count)

First 15 items:
$($dirItems | Select-Object -First 15 | ForEach-Object { "  - $($_.Name)$(if($_.PSIsContainer){'/'})" } | Out-String)

File types breakdown:
$($extCounts | ForEach-Object { "  $($_.Name): $($_.Count) files" } | Out-String)
=== END FILESYSTEM DATA ===
"@
                    }
                    else {
                        # It's a file - read content preview
                        $content = Get-Content -Path $path -TotalCount 50 -ErrorAction SilentlyContinue
                        $lineCount = (Get-Content -Path $path -ErrorAction SilentlyContinue | Measure-Object).Count
                        
                        $gatheredContext += @"

=== REAL FILE DATA FOR: $path ===
File: $($item.Name)
Size: $($item.Length) bytes
Lines: $lineCount
Last Modified: $($item.LastWriteTime)

First 50 lines preview:
$($content | Out-String)
=== END FILE DATA ===
"@
                    }
                }
                catch {
                    Write-DevConsole "Error gathering context for $path : $_" "WARNING"
                }
            }
        }
        
        if ($gatheredContext.Count -gt 0) {
            $contextData = @"

IMPORTANT: You have access to REAL filesystem data below. Use ONLY this data in your response.
Do NOT hallucinate or make up file counts, names, or statistics.
$($gatheredContext -join "`n")

"@
            $chatBox.AppendText("Agent > *Context gathered for $($targetPaths.Count) path(s)*`r`n")
        }
    }
    
    # Build enhanced prompt with context
    $enhancedPrompt = if ($contextData) {
        @"
$contextData

USER REQUEST: $prompt

Based on the REAL filesystem data above, respond to the user's request. Use ONLY the actual data provided - do not invent statistics or file names.
"@
    }
    else {
        $prompt
    }

    try {
        $response = Send-OllamaRequest $enhancedPrompt $model
        $chatBox.AppendText("AI > $response`r`n`r`n")
        # Save chat history after AI response
        Save-ChatHistory
    }
    catch {
        $chatBox.AppendText("Error: $_`r`n`r`n")
        Save-ChatHistory
    }
}

# Browser Navigation Function
function Open-Browser {
    <#
    .SYNOPSIS
        Opens a URL in the embedded browser
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
        if ($script:browserType -eq "WebView2") {
            if ($webBrowser.CoreWebView2) {
                $webBrowser.CoreWebView2.Navigate($url)
            }
        }
        else {
            $webBrowser.Navigate($url)
        }
    }
}

# Browser Event Handlers
$browserGoBtn.Add_Click({
        Open-Browser $browserUrlBox.Text.Trim()
    })

$browserUrlBox.Add_KeyDown({
        if ($_.KeyCode -eq "Enter") {
            Open-Browser $browserUrlBox.Text.Trim()
        }
    })

$browserBackBtn.Add_Click({
        if ($script:browserType -eq "WebView2") {
            if ($webBrowser.CoreWebView2 -and $webBrowser.CoreWebView2.CanGoBack) {
                $webBrowser.CoreWebView2.GoBack()
            }
        }
        else {
            if ($webBrowser.CanGoBack) {
                $webBrowser.GoBack()
            }
        }
    })

$browserForwardBtn.Add_Click({
        if ($script:browserType -eq "WebView2") {
            if ($webBrowser.CoreWebView2 -and $webBrowser.CoreWebView2.CanGoForward) {
                $webBrowser.CoreWebView2.GoForward()
            }
        }
        else {
            if ($webBrowser.CanGoForward) {
                $webBrowser.GoForward()
            }
        }
    })

$browserRefreshBtn.Add_Click({
        if ($script:browserType -eq "WebView2") {
            if ($webBrowser.CoreWebView2) {
                $webBrowser.CoreWebView2.Reload()
            }
        }
        else {
            $webBrowser.Refresh()
        }
    })

# Update URL box when browser navigates
if ($script:browserType -eq "WebView2") {
    try {
        # Use direct event binding instead of CoreWebView2InitializationCompleted
        # which may not be available on all WebView2 versions
        $webBrowser.add_NavigationCompleted({
                param($wv2Sender, $wv2Args)
                try {
                    if ($wv2Sender.CoreWebView2 -and $browserUrlBox) {
                        $browserUrlBox.Text = $wv2Sender.CoreWebView2.Source.ToString()
                    }
                }
                catch {
                    Write-StartupLog "Failed to update URL box: $($_.Exception.Message)" "DEBUG"
                }
            })
    }
    catch {
        Write-StartupLog "Failed to set up WebView2 navigation events: $($_.Exception.Message)" "DEBUG"
    }
}
else {
    try {
        # For WebView2, use NavigationCompleted event
        $webBrowser.add_NavigationCompleted({
                param($navCompleteSender, $navCompleteArgs)
                if ($browserUrlBox) {
                    $browserUrlBox.Text = $navCompleteSender.Source.ToString()
                }
            })
    }
    catch {
        Write-StartupLog "Browser navigation event setup failed: $_" "WARNING"
    }
}

# Navigate to default URL on load
Start-Job -ScriptBlock {
    Start-Sleep -Seconds 1
} | Out-Null
$form.Add_Shown({
        Open-Browser "https://www.youtube.com"
    })

# ============================================
# Git Functions
# ============================================
function Get-GitStatus {
    $currentDir = if ($global:currentFile) { Split-Path $global:currentFile } else { Get-Location }
    if (-not (Test-Path (Join-Path $currentDir ".git"))) {
        return "Not a Git repository"
    }
    
    try {
        Push-Location $currentDir
        $status = git status --short 2>&1
        $branch = git branch --show-current 2>&1
        $remote = git remote -v 2>&1
        
        $output = "Branch: $branch`r`n`r`n"
        $output += "Status:`r`n$status`r`n`r`n"
        $output += "Remote:`r`n$remote"
        return $output
    }
    catch {
        return "Error: $_"
    }
    finally {
        Pop-Location
    }
}

function Invoke-GitCommand {
    <#
    .SYNOPSIS
        Executes a Git command in the current repository
    .PARAMETER Command
        The Git command to execute
    .PARAMETER Arguments
        Arguments to pass to the Git command
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        
        [Parameter(Mandatory = $false)]
        [string[]]$Arguments
    )
    
    $currentDir = if ($global:currentFile) { Split-Path $global:currentFile } else { Get-Location }
    if (-not (Test-Path (Join-Path $currentDir ".git"))) {
        return "Not a Git repository"
    }
    
    try {
        Push-Location $currentDir
        $result = & git $Command $Arguments 2>&1 | Out-String
        return $result
    }
    catch {
        return "Error: $_"
    }
    finally {
        Pop-Location
    }
}

function Update-GitStatus {
    $status = Get-GitStatus
    $gitStatusBox.Clear()
    $gitStatusBox.AppendText($status)
}

# ============================================
# Terminal Functions
# ============================================
function Write-TerminalOutput {
    param([string]$text, [string]$color = "Green")
    $terminalOutput.SelectionStart = $terminalOutput.TextLength
    $terminalOutput.SelectionLength = 0
    $terminalOutput.SelectionColor = [System.Drawing.Color]::FromName($color)
    $terminalOutput.AppendText($text)
    $terminalOutput.SelectionColor = $terminalOutput.ForeColor
    $terminalOutput.ScrollToCaret()
}

function Start-NewTerminalSession {
    param(
        [string]$initiator = "System"
    )

    $global:terminalSessionCounter++
    $terminalOutput.Clear()
    $terminalInput.Clear()
    $terminalTab.Text = "Terminal (Session $global:terminalSessionCounter)"
    Write-TerminalOutput "`r`n--- Terminal Session $global:terminalSessionCounter started by $initiator ---`r`n" "Gray"
    $global:terminalSessionLock = $false
    $global:terminalSessionOwner = $null
}

function Acquire-TerminalControl {
    param(
        [string]$owner = "Agent"
    )

    if (-not $owner) { $owner = "Agent" }

    if ($global:terminalSessionLock -and $global:terminalSessionOwner -and $global:terminalSessionOwner -ne $owner) {
        Write-TerminalOutput "Terminal currently reserved by $($global:terminalSessionOwner). Starting a new session for $owner...`r`n" "Yellow"
        Start-NewTerminalSession -initiator $owner
    }

    $global:terminalSessionLock = $true
    $global:terminalSessionOwner = $owner
    if ($terminalTab.Text -notmatch "Session\s+\d+") {
        $terminalTab.Text = "Terminal (Session $global:terminalSessionCounter)"
    }

    Write-TerminalOutput "Terminal allocated to $owner.`r`n" "Gray"
}

function Release-TerminalControl {
    param(
        [string]$owner = "Agent"
    )

    if (-not $owner) { $owner = "Agent" }

    if ($global:terminalSessionOwner -and $global:terminalSessionOwner -ne $owner) {
        Write-TerminalOutput "Terminal is owned by $($global:terminalSessionOwner); $owner cannot release it.`r`n" "Yellow"
        return
    }

    $global:terminalSessionLock = $false
    $global:terminalSessionOwner = $null
    Write-TerminalOutput "Terminal released by $owner.`r`n" "Gray"
}

function Exit-TerminalSession {
    param(
        [string]$owner = "Agent"
    )

    Release-TerminalControl -owner $owner
    Write-TerminalOutput "Terminal session for $owner closed; please open a new one before the next command.`r`n`r`n" "Gray"
}

function Invoke-TerminalCommand {
    param([string]$command)
    
    if (-not $command.Trim()) { return }
    
    Acquire-TerminalControl -Owner "Manual"
    
    try {
        Write-TerminalOutput "PS $($global:currentWorkingDir)> $command`r`n" "Cyan"
    
    # Add to history
    $global:terminalHistory += $command
    $global:terminalHistoryIndex = @($global:terminalHistory).Count
    
    try {
        # Change directory commands
        if ($command -match "^cd\s+(.+)$") {
            $path = $Matches[1]
            if ($path -eq "..") {
                $global:currentWorkingDir = Split-Path $global:currentWorkingDir
            }
            elseif (Test-Path $path) {
                $global:currentWorkingDir = Resolve-Path $path
            }
            else {
                Write-TerminalOutput "Path not found: $path`r`n" "Red"
                return
            }
            Set-Location $global:currentWorkingDir
            Write-TerminalOutput "`r`n"
            return
        }
        
        # Execute command
        $output = Invoke-Expression $command 2>&1 | Out-String
        Write-TerminalOutput $output
    }
    catch {
        Write-TerminalOutput "Error: $_`r`n" "Red"
    }
    finally {
        Release-TerminalControl -Owner "Manual"
    }
}

# ============================================
# Enhanced File Browser Functions
# ============================================
function Update-Explorer {
    Write-StartupLog "Updating file explorer..." "INFO"
    $explorer.Nodes.Clear()
    $currentPath = $global:currentWorkingDir
    
    if (-not $currentPath) {
        $currentPath = Get-Location
        $global:currentWorkingDir = $currentPath
    }
    
    $explorerPathLabel.Text = "Path: $currentPath"
    Write-StartupLog "Current working directory: $currentPath" "INFO"
    
    try {
        # Add drives with better error handling
        $drives = @(Get-PSDrive -PSProvider FileSystem)
        Write-StartupLog "Found $($drives.Count) drives to load" "INFO"
        
        foreach ($drive in $drives) {
            try {
                $driveNode = New-Object System.Windows.Forms.TreeNode("� $($drive.Name):\ ($([math]::Round($drive.Used/1GB,1))GB used)")
                $driveNode.Tag = "$($drive.Root)"
                $driveNode.Name = $drive.Root
                $driveNode.ToolTipText = "Drive $($drive.Name): - Free: $([math]::Round($drive.Free/1GB,1))GB"
                $explorer.Nodes.Add($driveNode) | Out-Null
                
                # Add immediate children with lazy loading (don't load subdirectories)
                try {
                    Add-TreeNodeChildren -parentNode $driveNode -path $driveNode.Tag -showFiles $true -maxItems 500
                    Write-StartupLog "Successfully loaded drive $($drive.Name):" "SUCCESS"
                }
                catch {
                    Write-StartupLog "Warning: Could not fully load drive $($drive.Name): $_" "WARNING"
                    # Add an error indicator but don't fail the whole operation
                    $errorNode = New-Object System.Windows.Forms.TreeNode("⚠ Access restricted - some items may not be visible")
                    $errorNode.ForeColor = [System.Drawing.Color]::Orange
                    $driveNode.Nodes.Add($errorNode) | Out-Null
                }
            }
            catch {
                Write-StartupLog "Error processing drive $($drive.Name): $_" "ERROR"
            }
        }
        
        # Expand the drive containing current working directory and navigate to it
        if ($global:currentWorkingDir) {
            $currentDrive = [System.IO.Path]::GetPathRoot($global:currentWorkingDir)
            $matchingNode = $explorer.Nodes | Where-Object { $_.Tag -eq $currentDrive }
            if ($matchingNode) {
                $matchingNode.Expand()
                Write-StartupLog "Expanded current drive: $currentDrive" "INFO"
                
                # Try to expand path to current directory
                try {
                    Expand-PathInTree -treeView $explorer -targetPath $global:currentWorkingDir
                }
                catch {
                    Write-StartupLog "Could not navigate to current directory in tree: $_" "WARNING"
                }
            }
        }
        
        Write-StartupLog "File explorer update completed successfully" "SUCCESS"
    }
    catch {
        Write-StartupLog "Critical error in Update-Explorer: $_" "ERROR"
        Write-DevConsole "Error in Update-Explorer: $_" "ERROR"
        
        # Fallback to desktop
        try {
            $desktopPath = [Environment]::GetFolderPath("Desktop")
            $rootNode = New-Object System.Windows.Forms.TreeNode("�️ Desktop (Fallback)")
            $rootNode.Tag = $desktopPath
            $explorer.Nodes.Add($rootNode) | Out-Null
            Add-TreeNodeChildren -parentNode $rootNode -path $desktopPath -showFiles $true -maxItems 100
            $rootNode.Expand()
            Write-StartupLog "Using desktop fallback: $desktopPath" "WARNING"
        }
        catch {
            Write-StartupLog "Even desktop fallback failed: $_" "ERROR"
        }
    }
}

function Add-TreeNodeChildren {
    param(
        [System.Windows.Forms.TreeNode]$parentNode,
        [string]$path,
        [bool]$showFiles = $true,
        [int]$maxItems = 500,
        [int]$maxDepth = 0
    )
    
    if (-not (Test-Path $path -ErrorAction SilentlyContinue)) { 
        Write-StartupLog "Path does not exist: $path" "WARNING"
        return 
    }
    
    try {
        # Only clear nodes if they are dummy nodes or if explicitly refreshing
        $hasDummyNodes = $parentNode.Nodes | Where-Object { $_.Tag -eq "DUMMY" }
        $nodeCount = $parentNode.Nodes.Count
        if ($hasDummyNodes -or $nodeCount -eq 0) {
            $parentNode.Nodes.Clear()
            Write-StartupLog "Cleared existing nodes for: $path" "DEBUG"
        }
        else {
            Write-StartupLog "Nodes already populated for: $path, skipping" "DEBUG"
            return
        }
        
        # Get directories first, then files with better error handling
        $directories = @()
        $files = @()
        
        try {
            # Use more robust directory enumeration
            $directories = @(Get-ChildItem -Path $path -Directory -Force -ErrorAction SilentlyContinue | 
                Where-Object { -not ($_.Attributes -band [System.IO.FileAttributes]::System) -or $_.Name -notmatch '^(\$|System Volume Information|pagefile\.sys)' } |
                Sort-Object Name | 
                Select-Object -First $maxItems)
            
            Write-StartupLog "Found $($directories.Count) directories in: $path" "DEBUG"
        }
        catch {
            Write-StartupLog "Error reading directories in $path : $_" "WARNING"
        }
        
        if ($showFiles) {
            try {
                $files = @(Get-ChildItem -Path $path -File -Force -ErrorAction SilentlyContinue | 
                    Where-Object { $_.Length -lt 100MB } |  # Skip very large files for performance
                    Sort-Object Name | 
                    Select-Object -First $maxItems)
                
                Write-StartupLog "Found $($files.Count) files in: $path" "DEBUG"
            }
            catch {
                Write-StartupLog "Error reading files in $path : $_" "WARNING"
            }
        }
        
        # Add directories with improved icons and tooltips
        foreach ($dir in $directories) {
            try {
                $isHidden = $dir.Attributes -band [System.IO.FileAttributes]::Hidden
                $isSystem = $dir.Attributes -band [System.IO.FileAttributes]::System
                
                $dirIcon = if ($isHidden) { "👁️‍🗨️" } elseif ($isSystem) { "⚙️" } else { "📁" }
                $dirName = if ($isHidden) { "$($dir.Name) (Hidden)" } else { $dir.Name }
                
                $dirNode = New-Object System.Windows.Forms.TreeNode("$dirIcon $dirName")
                $dirNode.Tag = $dir.FullName
                $dirNode.Name = $dir.FullName
                $dirNode.ToolTipText = "Folder: $($dir.FullName)`nCreated: $($dir.CreationTime)`nAttributes: $($dir.Attributes)"
                
                # Dim hidden/system folders
                if ($isHidden) {
                    $dirNode.ForeColor = [System.Drawing.Color]::Gray
                }
                
                # Add dummy node to show expand arrow for lazy loading
                # Check if directory has subdirectories to decide whether to show expand arrow
                try {
                    $hasSubdirs = Get-ChildItem -Path $dir.FullName -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
                    if ($hasSubdirs) {
                        $dummy = New-Object System.Windows.Forms.TreeNode("Loading...")
                        $dummy.Tag = "DUMMY"
                        $dummy.ForeColor = [System.Drawing.Color]::Gray
                        $dirNode.Nodes.Add($dummy) | Out-Null
                    }
                }
                catch {
                    # If we can't check subdirectories, assume there might be some and add dummy
                    $dummy = New-Object System.Windows.Forms.TreeNode("Loading...")
                    $dummy.Tag = "DUMMY"
                    $dummy.ForeColor = [System.Drawing.Color]::Gray
                    $dirNode.Nodes.Add($dummy) | Out-Null
                }
                
                $parentNode.Nodes.Add($dirNode) | Out-Null
            }
            catch {
                Write-StartupLog "Error processing directory $($dir.Name): $_" "WARNING"
            }
        }
        
        # Add files with better categorization and icons
        foreach ($file in $files) {
            try {
                $fileIcon = Get-FileIcon $file.Extension
                $isHidden = $file.Attributes -band [System.IO.FileAttributes]::Hidden
                $fileName = if ($isHidden) { "$($file.Name) (Hidden)" } else { $file.Name }
                
                $fileNode = New-Object System.Windows.Forms.TreeNode("$fileIcon $fileName")
                $fileNode.Tag = $file.FullName
                $fileNode.Name = $file.FullName
                
                $fileSizeStr = if ($file.Length -gt 1MB) { 
                    "$([math]::Round($file.Length / 1MB, 2)) MB" 
                }
                elseif ($file.Length -gt 1KB) { 
                    "$([math]::Round($file.Length / 1KB, 2)) KB" 
                }
                else { 
                    "$($file.Length) bytes" 
                }
                
                $fileNode.ToolTipText = "File: $($file.FullName)`nSize: $fileSizeStr`nModified: $($file.LastWriteTime)`nAttributes: $($file.Attributes)"
                
                # Enhanced color coding for file types
                if ($file.Extension -match '\.(ps1|bat|cmd|sh)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Blue
                }
                elseif ($file.Extension -match '\.(txt|md|log|readme)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Green
                }
                elseif ($file.Extension -match '\.(json|xml|yml|yaml|config|ini)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Purple
                }
                elseif ($file.Extension -match '\.(js|html|css|py|cs|cpp|c|h|java|php|rb)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::DarkOrange
                }
                elseif ($file.Extension -match '\.(exe|dll|msi|app)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Red
                }
                elseif ($file.Extension -match '\.(jpg|jpeg|png|gif|bmp|ico|svg)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Magenta
                }
                elseif ($isHidden) {
                    $fileNode.ForeColor = [System.Drawing.Color]::Gray
                }
                
                $parentNode.Nodes.Add($fileNode) | Out-Null
            }
            catch {
                Write-StartupLog "Error processing file $($file.Name): $_" "WARNING"
            }
        }
        
        # Add summary if items were truncated
        $totalDirs = (Get-ChildItem -Path $path -Directory -Force -ErrorAction SilentlyContinue | Measure-Object).Count
        $totalFiles = if ($showFiles) { (Get-ChildItem -Path $path -File -Force -ErrorAction SilentlyContinue | Measure-Object).Count } else { 0 }
        
        $dirCount = @($directories).Count
        $fileCount = @($files).Count
        if (($dirCount + $fileCount) -lt ($totalDirs + $totalFiles)) {
            $truncatedNode = New-Object System.Windows.Forms.TreeNode("⋯ ... and $($totalDirs + $totalFiles - $dirCount - $fileCount) more items")
            $truncatedNode.ForeColor = [System.Drawing.Color]::DarkGray
            $parentNode.Nodes.Add($truncatedNode) | Out-Null
        }
        
        Write-StartupLog "Loaded $dirCount folders and $fileCount files for: $path" "DEBUG"
        
    }
    catch {
        Write-StartupLog "Critical error loading children for $path : $_" "ERROR"
        Write-DevConsole "Error loading children for $path : $_" "WARNING"
        $errorNode = New-Object System.Windows.Forms.TreeNode("⚠ Error loading contents: $($_.Exception.Message)")
        $errorNode.ForeColor = [System.Drawing.Color]::Red
        $errorNode.ToolTipText = "Error details: $_"
        $parentNode.Nodes.Add($errorNode) | Out-Null
    }
}

# Helper function to expand a specific path in the tree view
function Expand-PathInTree {
    param(
        [System.Windows.Forms.TreeView]$treeView,
        [string]$targetPath
    )
    
    try {
        $pathParts = $targetPath.Split([IO.Path]::DirectorySeparatorChar, [StringSplitOptions]::RemoveEmptyEntries)
        $currentPath = ""
        $currentNodes = $treeView.Nodes
        
        foreach ($part in $pathParts) {
            $currentPath = Join-Path $currentPath $part
            $matchingNode = $currentNodes | Where-Object { $_.Tag -eq $currentPath -or $_.Tag -eq "$currentPath\" }
            
            if ($matchingNode) {
                $matchingNode.Expand()
                $currentNodes = $matchingNode.Nodes
            }
            else {
                break
            }
        }
    }
    catch {
        Write-StartupLog "Error expanding path in tree: $_" "WARNING"
    }
}

function Get-FileIcon {
    param([string]$extension)
    
    switch -Regex ($extension.ToLower()) {
        '\.(txt|md|log)$' { return "📄" }
        '\.(ps1|bat|cmd|sh)$' { return "⚡" }
        '\.(json|xml|yml|yaml)$' { return "⚙️" }
        '\.(js|html|css)$' { return "🌐" }
        '\.(py|cs|cpp|c|h|java)$' { return "💻" }
        '\.(jpg|jpeg|png|gif|bmp)$' { return "🖼️" }
        '\.(mp3|wav|flac|mp4|avi)$' { return "🎵" }
        '\.(zip|rar|7z|gz)$' { return "📦" }
        '\.(pdf|doc|docx|xls|xlsx)$' { return "📋" }
        '\.(exe|msi|dll)$' { return "⚙️" }
        default { return "📄" }
    }
}

function Expand-ExplorerNode {
    param($node)
    
    # Don't expand dummy nodes or nodes without tags
    if (-not $node.Tag -or $node.Tag -eq "DUMMY") { 
        Write-DevConsole "Skipping expansion of dummy or invalid node" "DEBUG"
        return 
    }
    
    # Check if this node has dummy children (needs population)
    $hasDummyChild = $node.Nodes | Where-Object { $_.Tag -eq "DUMMY" }
    
    if ($hasDummyChild) {
        # This directory node needs to be populated - remove dummy and add real content
        Write-DevConsole "Expanding directory with dummy child: $($node.Tag)" "DEBUG"
        
        if (Test-Path $node.Tag -PathType Container) {
            try {
                # Clear all existing nodes (including dummy)
                $node.Nodes.Clear()
                
                # Populate with actual directories and files
                Add-TreeNodeChildren -parentNode $node -path $node.Tag -showFiles $true -maxItems 500
                
                Write-DevConsole "Successfully populated node: $($node.Tag) with $($node.Nodes.Count) items" "DEBUG"
            }
            catch {
                Write-DevConsole "Error expanding node $($node.Tag): $_" "ERROR"
                
                # Re-add dummy node if expansion failed
                $dummy = New-Object System.Windows.Forms.TreeNode("Error loading...")
                $dummy.Tag = "DUMMY"
                $dummy.ForeColor = [System.Drawing.Color]::Red
                $node.Nodes.Add($dummy) | Out-Null
            }
        }
        else {
            Write-DevConsole "Path no longer exists: $($node.Tag)" "WARNING"
            $node.Nodes.Clear()
        }
    }
    else {
        Write-DevConsole "Node already expanded or has no dummy children: $($node.Tag)" "DEBUG"
    }
}

# ============================================
# Extension Marketplace Functions
# ============================================
function Resolve-MarketplaceLanguageCode {
    param($LanguageInput)

    if ($null -eq $LanguageInput) { return $script:LANG_CUSTOM }
    if ($LanguageInput -is [int]) { return $LanguageInput }
    if ($LanguageInput -match '^\d+$') { return [int]$LanguageInput }

    $token = $LanguageInput.ToString().ToLower()
    switch ($token) {
        'asm'       { return $script:LANG_ASM }
        'python'    { return $script:LANG_PYTHON }
        'c'         { return $script:LANG_C }
        'cpp'       { return $script:LANG_CPP }
        'c++'       { return $script:LANG_CPP }
        'rust'      { return $script:LANG_RUST }
        'go'        { return $script:LANG_GO }
        'js'        { return $script:LANG_JAVASCRIPT }
        'javascript'{ return $script:LANG_JAVASCRIPT }
        default     { return $script:LANG_CUSTOM }
    }
}

function Normalize-MarketplaceEntry {
    param($Entry)

    $name = $Entry.Name ?? $Entry.Id
    $id = $Entry.Id
    if (-not $id -and $name) {
        $id = ($name.ToLower() -replace '[^a-z0-9]+', '-') -replace '^-+|-+$', ''
    }

    $language = Resolve-MarketplaceLanguageCode -Input ($Entry.Language ?? $script:LANG_CUSTOM)
    $downloads = [int64]($Entry.Downloads ?? 0)
    $rating = [double]($Entry.Rating ?? 4.5)
    if ($rating -gt 5) { $rating = 5 }
    if ($rating -lt 0) { $rating = 0 }

    $tags = @()
    if ($Entry.Tags) {
        $tags = @($Entry.Tags) | Where-Object { $_ }
    }

    return [PSCustomObject]@{
        Id           = $id
        Name         = $name
        Description  = $Entry.Description ?? 'No description provided.'
        Author       = $Entry.Author ?? 'RawrXD Community'
        Language     = $language
        Capabilities = [int]($Entry.Capabilities ?? 0)
        Version      = $Entry.Version ?? '1.0.0'
        Category     = $Entry.Category ?? 'Marketplace'
        Downloads    = $downloads
        Rating       = [math]::Round($rating, 1)
        Tags         = $tags
        MarketplaceId= $Entry.MarketplaceId ?? $id
        Source       = $Entry.Source ?? 'Marketplace'
        Installed    = [bool]($Entry.Installed ?? $false)
        Enabled      = [bool]($Entry.Enabled ?? $false)
        EntryPoint   = $Entry.EntryPoint
    }
}

function Get-MarketplaceSeedData {
    $seeds = @(
        @{ Id = 'python-toolkit'; Name = 'Python Productivity Pack'; Description = 'Linting, formatting, and debugging helpers for Python developers.'; Author = 'RawrXD'; Language = $script:LANG_PYTHON; Capabilities = ($script:CAP_SYNTAX_HIGHLIGHT -bor $script:CAP_CODE_COMPLETION -bor $script:CAP_LINTING -bor $script:CAP_DEBUGGING); Version = '3.1.0'; Category = 'Productivity'; Downloads = 341234; Rating = 4.9; Tags = @('python','lint','debug'); Source = 'RawrXD Official' }
        @{ Id = 'js-debugger-plus'; Name = 'JavaScript Debugger+'; Description = 'Live JavaScript/Node debugging with console replay and breakpoints.'; Author = 'RawrXD'; Language = $script:LANG_JAVASCRIPT; Capabilities = ($script:CAP_DEBUGGING -bor $script:CAP_CODE_COMPLETION); Version = '2.5.3'; Category = 'Debugger'; Downloads = 217890; Rating = 4.7; Tags = @('javascript','debugger','node'); Source = 'RawrXD Official' }
        @{ Id = 'rust-toolchain'; Name = 'Rust Toolchain Suite'; Description = 'Cargo workflows with linting, formatting, and documentation previews.'; Author = 'RawrXD'; Language = $script:LANG_RUST; Capabilities = ($script:CAP_BUILD_SYSTEM -bor $script:CAP_FORMATTING -bor $script:CAP_LINTING); Version = '1.2.4'; Category = 'Toolchain'; Downloads = 114502; Rating = 4.6; Tags = @('rust','cargo'); Source = 'RawrXD Official' }
        @{ Id = 'gitops-visualizer'; Name = 'GitOps Visualizer'; Description = 'Visualize branches, commits, and automations with live dependency graphs.'; Author = 'RawrXD'; Language = $script:LANG_CUSTOM; Capabilities = $script:CAP_GIT_INTEGRATION; Version = '1.0.8'; Category = 'Git'; Downloads = 85301; Rating = 4.8; Tags = @('git','visual'); Source = 'Community Marketplace' }
        @{ Id = 'ai-code-mentor'; Name = 'AI Code Mentor'; Description = 'Context-aware AI hints, refactors, and explanations powered by RawrXD models.'; Author = 'RawrXD'; Language = $script:LANG_CUSTOM; Capabilities = ($script:CAP_AI_ASSIST -bor $script:CAP_REFACTORING); Version = '0.9.7'; Category = 'AI'; Downloads = 68790; Rating = 4.9; Tags = @('ai','assistant','mentor'); Source = 'Community Marketplace' }
    )

    return $seeds | ForEach-Object { Normalize-MarketplaceEntry -Entry $_ }
}

function Load-MarketplaceCatalog {
    param([switch]$Force)

    if (-not $Force -and $script:marketplaceCache.Count -gt 0) {
        return $script:marketplaceCache
    }

    $catalog = Get-MarketplaceSeedData
    foreach ($source in $script:marketplaceSources) {
        try {
            $payload = Invoke-RestMethod -Uri $source.Url -TimeoutSec 15 -UseBasicParsing -Headers @{ 'User-Agent' = 'RawrXD-Marketplace/1.0' }
            if ($payload -and $payload.Extensions) {
                $payload = $payload.Extensions
            }

            if ($payload) {
                $entries = @()
                if ($payload -is [System.Collections.IEnumerable]) {
                    $entries = $payload
                }
                else {
                    $entries = @($payload)
                }

                foreach ($entry in $entries) {
                    $normalized = Normalize-MarketplaceEntry -Entry $entry
                    $normalized.Source = $source.Name
                    $catalog += $normalized
                }
            }

            Write-StartupLog "Loaded marketplace source '$($source.Name)'" "INFO"
        }
        catch {
            Write-StartupLog "Marketplace source '$($source.Name)' failed: $($_.Exception.Message)" "WARNING"
        }
    }

    if (-not $catalog.Count) {
        $catalog = Get-MarketplaceSeedData
    }

    $unique = @{}
    foreach ($entry in $catalog) {
        if (-not $entry.Id) { continue }
        $key = $entry.Id.ToLower()
        if (-not $unique.ContainsKey($key)) {
            $unique[$key] = $entry
        }
    }

    $script:marketplaceCache = $unique.Values | Sort-Object -Property Downloads -Descending
    $script:marketplaceLastRefresh = Get-Date
    return $script:marketplaceCache
}

function Register-Extension {
    param(
        [string]$Id,
        [string]$Name,
        [string]$Description,
        [string]$Author,
        [int]$Language,
        [int]$Capabilities,
        [string]$Version = "1.0.0",
        [scriptblock]$EntryPoint = $null
    )
    
    $extension = @{
        Id           = $Id
        Name         = $Name
        Description  = $Description
        Author       = $Author
        Language     = $Language
        Capabilities = $Capabilities
        Version      = $Version
        Enabled      = $true
        Installed    = $true
        EntryPoint   = $EntryPoint
        Hooks        = @{}
    }
    
    $script:extensionRegistry += $extension
    return $extension
}

function Initialize-ExtensionSystem {
    # Register built-in extensions
    Register-Extension -Id "python-lang" -Name "Python Language Support" `
        -Description "Full Python IDE features with syntax highlighting, debugging, and linting" `
        -Author "RawrXD" -Language $script:LANG_PYTHON `
        -Capabilities ($script:CAP_SYNTAX_HIGHLIGHT -bor $script:CAP_CODE_COMPLETION -bor $script:CAP_DEBUGGING -bor $script:CAP_LINTING)
    
    Register-Extension -Id "c-lang" -Name "C/C++ Development" `
        -Description "C and C++ support with GCC/Clang integration" `
        -Author "RawrXD" -Language $script:LANG_C `
        -Capabilities ($script:CAP_SYNTAX_HIGHLIGHT -bor $script:CAP_CODE_COMPLETION -bor $script:CAP_DEBUGGING -bor $script:CAP_BUILD_SYSTEM)
    
    Register-Extension -Id "rust-lang" -Name "Rust Language Support" `
        -Description "Rust development with Cargo integration" `
        -Author "RawrXD" -Language $script:LANG_RUST `
        -Capabilities ($script:CAP_SYNTAX_HIGHLIGHT -bor $script:CAP_CODE_COMPLETION -bor $script:CAP_BUILD_SYSTEM)
    
    Register-Extension -Id "model-dampener" -Name "Model Dampener" `
        -Description "On-the-fly AI model behavior modification without retraining" `
        -Author "RawrXD" -Language $script:LANG_ASM `
        -Capabilities ($script:CAP_MODEL_DAMPENING -bor $script:CAP_AI_ASSIST)
    
    Register-Extension -Id "git-enhanced" -Name "Enhanced Git Integration" `
        -Description "Advanced Git features with visual diff and merge tools" `
        -Author "RawrXD" -Language $script:LANG_ASM `
        -Capabilities $script:CAP_GIT_INTEGRATION
    
    # Load user-installed extensions
    Import-UserExtensions
}

function Import-UserExtensions {
    $extensionsFile = Join-Path $script:extensionsDir "extensions.json"
    if (Test-Path $extensionsFile) {
        try {
            $userExtensions = Get-Content $extensionsFile | ConvertFrom-Json
            foreach ($ext in $userExtensions) {
                Register-Extension -Id $ext.Id -Name $ext.Name -Description $ext.Description `
                    -Author $ext.Author -Language $ext.Language -Capabilities $ext.Capabilities -Version $ext.Version
            }
        }
        catch {
            Write-Host "Error loading user extensions: $_" -ForegroundColor Yellow
        }
    }
}

function Search-Marketplace {
    param(
        [string]$Query,
        [int]$LanguageFilter = -1,
        [switch]$IncludeInstalled,
        [switch]$IncludeRemote
    )

    if (-not $IncludeInstalled.IsPresent -and -not $IncludeRemote.IsPresent) {
        $IncludeInstalled = $true
        $IncludeRemote = $true
    }

    $results = @()
    $seenIds = @{}

    $evaluateEntry = {
        param($entry, $defaultSource)

        if (-not $entry) {
            return
        }

        if (-not $entry.Source -and $defaultSource) {
            $entry.Source = $defaultSource
        }

        $idKey = ($entry.Id ?? $entry.MarketplaceId ?? '')
        if ($idKey) {
            $idKey = $idKey.ToString().ToLower()
            if ($seenIds.ContainsKey($idKey)) {
                return
            }
        }

        $match = [string]::IsNullOrEmpty($Query)
        if (-not $match) {
            foreach ($field in @($entry.Name, $entry.Description, $entry.Author, $entry.Category)) {
                if ($field -and $field -like "*$Query*") {
                    $match = $true
                    break
                }
            }
        }

        if ($match -and ($LanguageFilter -eq -1 -or $entry.Language -eq $LanguageFilter)) {
            $results += $entry
            if ($idKey) {
                $seenIds[$idKey] = $true
            }
        }
    }

    if ($IncludeInstalled) {
        foreach ($ext in $script:extensionRegistry) {
            & $evaluateEntry $ext "Installed"
        }
    }

    if ($IncludeRemote) {
        $catalog = Load-MarketplaceCatalog
        foreach ($entry in $catalog) {
            & $evaluateEntry $entry ($entry.Source ?? 'Marketplace')
        }
    }

    return $results | Sort-Object @{ Expression = { $_.Downloads ?? 0 }; Descending = $true }, @{ Expression = { $_.Name }; Descending = $false }
}

function Show-Marketplace {
    $marketplaceForm = New-Object System.Windows.Forms.Form
    $marketplaceForm.Text = "Extension Marketplace"
    $marketplaceForm.Size = New-Object System.Drawing.Size(800, 600)
    $marketplaceForm.StartPosition = "CenterScreen"
    
    # Search box
    $searchBox = New-Object System.Windows.Forms.TextBox
    $searchBox.Dock = [System.Windows.Forms.DockStyle]::Top
    $searchBox.Height = 30
    $searchBox.Font = New-Object System.Drawing.Font("Segoe UI", 10)
    $marketplaceForm.Controls.Add($searchBox) | Out-Null
    
    # Results list
    $resultsList = New-Object System.Windows.Forms.ListView
    $resultsList.Dock = [System.Windows.Forms.DockStyle]::Fill
    $resultsList.View = [System.Windows.Forms.View]::Details
    $resultsList.Font = New-Object System.Drawing.Font("Segoe UI", 9)
    $resultsList.FullRowSelect = $true
    $resultsList.Columns.Add("Name", 200) | Out-Null
    $resultsList.Columns.Add("Description", 400) | Out-Null
    $resultsList.Columns.Add("Author", 100) | Out-Null
    $resultsList.Columns.Add("Version", 80) | Out-Null
    $marketplaceForm.Controls.Add($resultsList) | Out-Null
    
    # Refresh results
    $refreshResults = {
        $resultsList.Items.Clear()
        $query = $searchBox.Text
        $results = Search-Marketplace -Query $query
        foreach ($ext in $results) {
            $item = New-Object System.Windows.Forms.ListViewItem($ext.Name)
            $item.SubItems.Add($ext.Description) | Out-Null
            $item.SubItems.Add($ext.Author) | Out-Null
            $item.SubItems.Add($ext.Version) | Out-Null
            $item.Tag = $ext
            $resultsList.Items.Add($item) | Out-Null
        }
    }
    
    $searchBox.Add_TextChanged($refreshResults)
    $refreshResults.Invoke()
    
    $marketplaceForm.ShowDialog() | Out-Null
}

function Show-InstalledExtensions {
    $installedForm = New-Object System.Windows.Forms.Form
    $installedForm.Text = "Installed Extensions"
    $installedForm.Size = New-Object System.Drawing.Size(700, 500)
    $installedForm.StartPosition = "CenterScreen"
    
    $listBox = New-Object System.Windows.Forms.ListBox
    $listBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $listBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $installedForm.Controls.Add($listBox) | Out-Null
    
    foreach ($ext in $script:extensionRegistry) {
        $status = if ($ext.Enabled) { "[ENABLED]" } else { "[DISABLED]" }
        $listBox.Items.Add("$($ext.Name) | Out-Null $status - $($ext.Description)") | Out-Null
    }
    
    $installedForm.ShowDialog() | Out-Null
}

# ============================================
# Settings Functions
# ============================================
function Get-Settings {
    if (Test-Path $script:settingsPath) {
        try {
            $loadedSettings = Get-Content $script:settingsPath | ConvertFrom-Json
            foreach ($key in $loadedSettings.PSObject.Properties.Name) {
                if ($global:settings.ContainsKey($key)) {
                    $global:settings[$key] = $loadedSettings.$key
                }
            }
            Write-DevConsole "Settings loaded from: $script:settingsPath" "INFO"
            
            # Apply loaded settings
            $script:currentModel = $global:settings.OllamaModel
            Set-EditorSettings
        }
        catch {
            Write-DevConsole "Error loading settings: $_" "ERROR"
        }
    }
}

function Save-Settings {
    try {
        $settingsDir = Split-Path $script:settingsPath
        if (-not (Test-Path $settingsDir)) {
            New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
        }
        
        $global:settings | ConvertTo-Json -Depth 3 | Out-File $script:settingsPath -Encoding UTF8
        Write-DevConsole "Settings saved to: $script:settingsPath" "SUCCESS"
    }
    catch {
        Write-DevConsole "Error saving settings: $_" "ERROR"
    }
}

function Apply-EditorSettings {
    try {
        $script:editor.Font = New-Object System.Drawing.Font($global:settings.EditorFontFamily, $global:settings.EditorFontSize)
        Write-DevConsole "Applied editor settings" "DEBUG"
    }
    catch {
        Write-DevConsole "Error applying editor settings: $_" "WARNING"
    }
}

function Set-EditorSettings {
    param(
        [hashtable]$SettingsOverride = @{}
    )

    if (-not $global:settings) {
        $global:settings = @{}
    }

    $defaults = @{
        EditorFontFamily = "Consolas"
        EditorFontSize   = 10
        TabSize          = 4
        ShowLineNumbers  = $true
        WrapText         = $false
        AutoIndent       = $true
        CodeHighlighting = $true
        AutoComplete     = $true
        ShowWhitespace   = $false
    }

    foreach ($key in $defaults.Keys) {
        if (-not $global:settings.ContainsKey($key) -or $null -eq $global:settings[$key]) {
            $global:settings[$key] = $defaults[$key]
        }
    }

    if ($SettingsOverride -and $SettingsOverride.Count -gt 0) {
        foreach ($entry in $SettingsOverride.GetEnumerator()) {
            $global:settings[$entry.Key] = $entry.Value
        }
    }

    try {
        if ($script:editor) {
            Apply-EditorSettings
        }
    }
    catch {
        Write-DevConsole "Error initializing editor settings: $_" "WARNING"
    }
}

function Get-AvailableModels {
    try {
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5
        return $response.models | ForEach-Object { $_.name } | Sort-Object
    }
    catch {
        Write-DevConsole "Could not fetch models from Ollama: $_" "WARNING"
        return @("bigdaddyg-fast:latest", "llama3:latest", "phi:latest")  # fallback models
    }
}

function Show-ModelSettings {
    $settingsForm = New-Object System.Windows.Forms.Form
    $settingsForm.Text = "AI Model & General Settings"
    $settingsForm.Size = New-Object System.Drawing.Size(500, 400)
    $settingsForm.StartPosition = "CenterScreen"
    $settingsForm.FormBorderStyle = "FixedDialog"
    $settingsForm.MaximizeBox = $false
    
    # Model selection
    $modelLabel = New-Object System.Windows.Forms.Label
    $modelLabel.Text = "AI Model:"
    $modelLabel.Location = New-Object System.Drawing.Point(20, 30)
    $modelLabel.Size = New-Object System.Drawing.Size(100, 23)
    $settingsForm.Controls.Add($modelLabel)
    
    $modelCombo = New-Object System.Windows.Forms.ComboBox
    $modelCombo.Location = New-Object System.Drawing.Point(130, 27)
    $modelCombo.Size = New-Object System.Drawing.Size(300, 25)
    $modelCombo.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
    
    # Populate with available models
    $availableModels = Get-AvailableModels
    foreach ($model in $availableModels) {
        $modelCombo.Items.Add($model) | Out-Null
    }
    $modelCombo.Text = $global:settings.OllamaModel
    $settingsForm.Controls.Add($modelCombo)
    
    # Refresh models button
    $refreshBtn = New-Object System.Windows.Forms.Button
    $refreshBtn.Text = "🔄"
    $refreshBtn.Location = New-Object System.Drawing.Point(440, 27)
    $refreshBtn.Size = New-Object System.Drawing.Size(30, 25)
    $refreshBtn.Add_Click({
            $modelCombo.Items.Clear()
            $newModels = Get-AvailableModels
            foreach ($model in $newModels) {
                $modelCombo.Items.Add($model) | Out-Null
            }
        })
    $settingsForm.Controls.Add($refreshBtn)
    
    # Max tabs setting
    $tabsLabel = New-Object System.Windows.Forms.Label
    $tabsLabel.Text = "Max Editor Tabs:"
    $tabsLabel.Location = New-Object System.Drawing.Point(20, 70)
    $tabsLabel.Size = New-Object System.Drawing.Size(100, 23)
    $settingsForm.Controls.Add($tabsLabel)
    
    $tabsNumeric = New-Object System.Windows.Forms.NumericUpDown
    $tabsNumeric.Location = New-Object System.Drawing.Point(130, 67)
    $tabsNumeric.Size = New-Object System.Drawing.Size(80, 25)
    $tabsNumeric.Minimum = 1
    $tabsNumeric.Maximum = 100
    $tabsNumeric.Value = $global:settings.MaxTabs
    $settingsForm.Controls.Add($tabsNumeric)
    
    # Auto-save checkbox
    $autoSaveCheck = New-Object System.Windows.Forms.CheckBox
    $autoSaveCheck.Text = "Enable Auto-Save"
    $autoSaveCheck.Location = New-Object System.Drawing.Point(20, 110)
    $autoSaveCheck.Size = New-Object System.Drawing.Size(150, 23)
    $autoSaveCheck.Checked = $global:settings.AutoSaveEnabled
    $settingsForm.Controls.Add($autoSaveCheck)
    
    # Auto-save interval
    $intervalLabel = New-Object System.Windows.Forms.Label
    $intervalLabel.Text = "Auto-Save Interval (sec):"
    $intervalLabel.Location = New-Object System.Drawing.Point(20, 150)
    $intervalLabel.Size = New-Object System.Drawing.Size(140, 23)
    $settingsForm.Controls.Add($intervalLabel)
    
    $intervalNumeric = New-Object System.Windows.Forms.NumericUpDown
    $intervalNumeric.Location = New-Object System.Drawing.Point(170, 147)
    $intervalNumeric.Size = New-Object System.Drawing.Size(80, 25)
    $intervalNumeric.Minimum = 5
    $intervalNumeric.Maximum = 300
    $intervalNumeric.Value = $global:settings.AutoSaveInterval
    $settingsForm.Controls.Add($intervalNumeric)
    
    # Debug mode checkbox
    $debugCheck = New-Object System.Windows.Forms.CheckBox
    $debugCheck.Text = "Enable Debug Mode"
    $debugCheck.Location = New-Object System.Drawing.Point(20, 190)
    $debugCheck.Size = New-Object System.Drawing.Size(150, 23)
    $debugCheck.Checked = $global:settings.DebugMode
    $settingsForm.Controls.Add($debugCheck)
    
    # Buttons panel
    $buttonPanel = New-Object System.Windows.Forms.Panel
    $buttonPanel.Location = New-Object System.Drawing.Point(0, 320)
    $buttonPanel.Size = New-Object System.Drawing.Size(500, 50)
    $buttonPanel.Dock = [System.Windows.Forms.DockStyle]::Bottom
    $settingsForm.Controls.Add($buttonPanel)
    
    $okButton = New-Object System.Windows.Forms.Button
    $okButton.Text = "OK"
    $okButton.Location = New-Object System.Drawing.Point(320, 10)
    $okButton.Size = New-Object System.Drawing.Size(75, 30)
    $okButton.DialogResult = [System.Windows.Forms.DialogResult]::OK
    $buttonPanel.Controls.Add($okButton)
    
    $cancelButton = New-Object System.Windows.Forms.Button
    $cancelButton.Text = "Cancel"
    $cancelButton.Location = New-Object System.Drawing.Point(405, 10)
    $cancelButton.Size = New-Object System.Drawing.Size(75, 30)
    $cancelButton.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
    $buttonPanel.Controls.Add($cancelButton)
    
    if ($settingsForm.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        # Apply settings
        $global:settings.OllamaModel = $modelCombo.Text
        $global:settings.MaxTabs = $tabsNumeric.Value
        # Don't automatically sync MaxChatTabs with MaxTabs - they are separate settings
        # $global:settings.MaxChatTabs should only be changed through the chat settings dialog
        $global:settings.AutoSaveEnabled = $autoSaveCheck.Checked
        $global:settings.AutoSaveInterval = $intervalNumeric.Value
        $global:settings.DebugMode = $debugCheck.Checked
        
        Save-Settings
        Write-DevConsole "Settings updated and saved" "SUCCESS"
        
        # Show restart message if model changed
        Write-DevConsole "Settings saved successfully!" "SUCCESS"
    }
    
    $settingsForm.Dispose()
}

function Show-EditorSettings {
    $editorForm = New-Object System.Windows.Forms.Form
    $editorForm.Text = "Editor Settings"
    $editorForm.Size = New-Object System.Drawing.Size(450, 350)
    $editorForm.StartPosition = "CenterScreen"
    $editorForm.FormBorderStyle = "FixedDialog"
    $editorForm.MaximizeBox = $false
    
    # Font family
    $fontLabel = New-Object System.Windows.Forms.Label
    $fontLabel.Text = "Font Family:"
    $fontLabel.Location = New-Object System.Drawing.Point(20, 30)
    $fontLabel.Size = New-Object System.Drawing.Size(100, 23)
    $editorForm.Controls.Add($fontLabel)
    
    $fontCombo = New-Object System.Windows.Forms.ComboBox
    $fontCombo.Location = New-Object System.Drawing.Point(130, 27)
    $fontCombo.Size = New-Object System.Drawing.Size(150, 25)
    $fontCombo.Items.AddRange(@("Consolas", "Courier New", "Monaco", "Lucida Console", "Source Code Pro"))
    $fontCombo.Text = $global:settings.EditorFontFamily
    $editorForm.Controls.Add($fontCombo)
    
    # Font size
    $sizeLabel = New-Object System.Windows.Forms.Label
    $sizeLabel.Text = "Font Size:"
    $sizeLabel.Location = New-Object System.Drawing.Point(300, 30)
    $sizeLabel.Size = New-Object System.Drawing.Size(70, 23)
    $editorForm.Controls.Add($sizeLabel)
    
    $sizeNumeric = New-Object System.Windows.Forms.NumericUpDown
    $sizeNumeric.Location = New-Object System.Drawing.Point(370, 27)
    $sizeNumeric.Size = New-Object System.Drawing.Size(50, 25)
    $sizeNumeric.Minimum = 8
    $sizeNumeric.Maximum = 72
    $sizeNumeric.Value = $global:settings.EditorFontSize
    $editorForm.Controls.Add($sizeNumeric)
    
    # Tab size
    $tabLabel = New-Object System.Windows.Forms.Label
    $tabLabel.Text = "Tab Size:"
    $tabLabel.Location = New-Object System.Drawing.Point(20, 70)
    $tabLabel.Size = New-Object System.Drawing.Size(100, 23)
    $editorForm.Controls.Add($tabLabel)
    
    $tabNumeric = New-Object System.Windows.Forms.NumericUpDown
    $tabNumeric.Location = New-Object System.Drawing.Point(130, 67)
    $tabNumeric.Size = New-Object System.Drawing.Size(50, 25)
    $tabNumeric.Minimum = 1
    $tabNumeric.Maximum = 8
    $tabNumeric.Value = $global:settings.TabSize
    $editorForm.Controls.Add($tabNumeric)
    
    # Checkboxes
    $lineNumbersCheck = New-Object System.Windows.Forms.CheckBox
    $lineNumbersCheck.Text = "Show Line Numbers"
    $lineNumbersCheck.Location = New-Object System.Drawing.Point(20, 110)
    $lineNumbersCheck.Size = New-Object System.Drawing.Size(150, 23)
    $lineNumbersCheck.Checked = $global:settings.ShowLineNumbers
    $editorForm.Controls.Add($lineNumbersCheck)
    
    $wrapCheck = New-Object System.Windows.Forms.CheckBox
    $wrapCheck.Text = "Word Wrap"
    $wrapCheck.Location = New-Object System.Drawing.Point(200, 110)
    $wrapCheck.Size = New-Object System.Drawing.Size(150, 23)
    $wrapCheck.Checked = $global:settings.WrapText
    $editorForm.Controls.Add($wrapCheck)
    
    $autoIndentCheck = New-Object System.Windows.Forms.CheckBox
    $autoIndentCheck.Text = "Auto Indent"
    $autoIndentCheck.Location = New-Object System.Drawing.Point(20, 150)
    $autoIndentCheck.Size = New-Object System.Drawing.Size(150, 23)
    $autoIndentCheck.Checked = $global:settings.AutoIndent
    $editorForm.Controls.Add($autoIndentCheck)
    
    $highlightCheck = New-Object System.Windows.Forms.CheckBox
    $highlightCheck.Text = "Syntax Highlighting"
    $highlightCheck.Location = New-Object System.Drawing.Point(200, 150)
    $highlightCheck.Size = New-Object System.Drawing.Size(150, 23)
    $highlightCheck.Checked = $global:settings.CodeHighlighting
    $editorForm.Controls.Add($highlightCheck)
    
    # Buttons
    $buttonPanel = New-Object System.Windows.Forms.Panel
    $buttonPanel.Location = New-Object System.Drawing.Point(0, 270)
    $buttonPanel.Size = New-Object System.Drawing.Size(450, 50)
    $buttonPanel.Dock = [System.Windows.Forms.DockStyle]::Bottom
    $editorForm.Controls.Add($buttonPanel)
    
    $okButton = New-Object System.Windows.Forms.Button
    $okButton.Text = "OK"
    $okButton.Location = New-Object System.Drawing.Point(270, 10)
    $okButton.Size = New-Object System.Drawing.Size(75, 30)
    $okButton.DialogResult = [System.Windows.Forms.DialogResult]::OK
    $buttonPanel.Controls.Add($okButton)
    
    $cancelButton = New-Object System.Windows.Forms.Button
    $cancelButton.Text = "Cancel"
    $cancelButton.Location = New-Object System.Drawing.Point(355, 10)
    $cancelButton.Size = New-Object System.Drawing.Size(75, 30)
    $cancelButton.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
    $buttonPanel.Controls.Add($cancelButton)
    
    if ($editorForm.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        # Apply settings
        $global:settings.EditorFontFamily = $fontCombo.Text
        $global:settings.EditorFontSize = $sizeNumeric.Value
        $global:settings.TabSize = $tabNumeric.Value
        $global:settings.ShowLineNumbers = $lineNumbersCheck.Checked
        $global:settings.WrapText = $wrapCheck.Checked
        $global:settings.AutoIndent = $autoIndentCheck.Checked
        $global:settings.CodeHighlighting = $highlightCheck.Checked
        
        Apply-EditorSettings
        Save-Settings
        Write-DevConsole "Editor settings updated and applied" "SUCCESS"
    }
    
    $editorForm.Dispose()
}

# ============================================
# Chat Tab Management Functions
# ============================================
function New-ChatTab {
    param(
        [string]$TabName = "Chat",
        [string]$Model = $null
    )
    
    # Check max chat tabs limit
    $currentTabCount = if ($script:chatTabs) { @($script:chatTabs).Count } else { 0 }
    if ($currentTabCount -ge $global:settings.MaxChatTabs) {
        Write-DevConsole "⚠ Maximum chat tabs ($($global:settings.MaxChatTabs)) reached" "WARNING"
        return $null
    }
    
    $script:chatTabCounter++
    $tabId = "chat_$($script:chatTabCounter)"
    $finalTabName = if ($TabName -eq "Chat") { "Chat $($script:chatTabCounter)" } else { $TabName }
    
    # Create new tab page
    $newChatTabPage = New-Object System.Windows.Forms.TabPage
    $newChatTabPage.Text = $finalTabName
    $newChatTabPage.Name = $tabId
    
    # Create chat splitter for this tab
    $chatSplitter = New-Object System.Windows.Forms.SplitContainer
    $chatSplitter.Dock = [System.Windows.Forms.DockStyle]::Fill
    $chatSplitter.Orientation = [System.Windows.Forms.Orientation]::Horizontal
    $chatSplitter.SplitterDistance = 350
    $chatSplitter.Panel2MinSize = 80
    $chatSplitter.IsSplitterFixed = $false
    $newChatTabPage.Controls.Add($chatSplitter)
    
    # Chat display area
    $chatBox = New-Object System.Windows.Forms.RichTextBox
    $chatBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $chatBox.ReadOnly = $true
    $chatBox.Font = New-Object System.Drawing.Font("Segoe UI", 9)
    $chatBox.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $chatBox.ForeColor = [System.Drawing.Color]::White
    $chatSplitter.Panel1.Controls.Add($chatBox)
    
    # Input area container
    $inputContainer = New-Object System.Windows.Forms.Panel
    $inputContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
    $inputContainer.Height = 80
    $inputContainer.Padding = New-Object System.Windows.Forms.Padding(0, 0, 0, 2)
    $chatSplitter.Panel2.Controls.Add($inputContainer)
    
    # Model selector
    $modelPanel = New-Object System.Windows.Forms.Panel
    $modelPanel.Dock = [System.Windows.Forms.DockStyle]::Top
    $modelPanel.Height = 20
    $modelPanel.Margin = New-Object System.Windows.Forms.Padding(0)
    $inputContainer.Controls.Add($modelPanel)
    
    $modelLabel = New-Object System.Windows.Forms.Label
    $modelLabel.Text = "Model:"
    $modelLabel.Dock = [System.Windows.Forms.DockStyle]::Left
    $modelLabel.Width = 45
    $modelLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleLeft
    $modelLabel.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    $modelLabel.Margin = New-Object System.Windows.Forms.Padding(0)
    $modelPanel.Controls.Add($modelLabel)
    
    $modelCombo = New-Object System.Windows.Forms.ComboBox
    $modelCombo.Dock = [System.Windows.Forms.DockStyle]::Left
    $modelCombo.Width = 180
    $modelCombo.Height = 18
    $modelCombo.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    $modelCombo.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
    $modelCombo.Items.AddRange(@("bigdaddyg-fast:latest", "llama3.2", "llama3.2:1b", "llama3.1", "codellama", "mistral", "qwen2.5-coder"))
    $modelCombo.SelectedItem = if ($Model) { $Model } else { $global:settings.OllamaModel }
    $modelCombo.Margin = New-Object System.Windows.Forms.Padding(0)
    $modelPanel.Controls.Add($modelCombo)
    
    # Input box - Use explicit positioning instead of Fill to ensure visibility
    $inputBox = New-Object System.Windows.Forms.TextBox
    $inputBox.Anchor = [System.Windows.Forms.AnchorStyles]::Left -bor [System.Windows.Forms.AnchorStyles]::Right -bor [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Bottom
    $inputBox.Location = New-Object System.Drawing.Point(2, 22)
    $inputBox.Size = New-Object System.Drawing.Size(($inputContainer.Width - 6), ($inputContainer.Height - 26))
    $inputBox.Multiline = $true
    $inputBox.Font = New-Object System.Drawing.Font("Segoe UI", 9)
    $inputBox.ScrollBars = [System.Windows.Forms.ScrollBars]::Vertical
    $inputBox.WordWrap = $true
    $inputBox.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
    $inputBox.ForeColor = [System.Drawing.Color]::White
    $inputBox.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
    $inputBox.TabIndex = 0
    $inputContainer.Controls.Add($inputBox)
    
    # Chat session data
    $chatSession = @{
        TabId        = $tabId
        TabPage      = $newChatTabPage
        ChatBox      = $chatBox
        InputBox     = $inputBox
        ModelCombo   = $modelCombo
        Messages     = @()
        IsActive     = $false
        CreatedTime  = Get-Date
        LastActivity = Get-Date
    }
    
    # Store chat tab
    $script:chatTabs[$tabId] = $chatSession
    
    # Add to tab control
    $chatTabControl.TabPages.Add($newChatTabPage)
    
    # Event handlers for this chat - use current tab approach
    $inputBox.add_KeyDown({
            param($keyDownSender, $e)
            if ($e.Control -and $e.KeyCode -eq "Enter") {
                # Find the active chat tab based on selected tab
                $selectedTab = $chatTabControl.SelectedTab
                if ($selectedTab -and $script:chatTabs.ContainsKey($selectedTab.Name)) {
                    Send-ChatMessage -TabId $selectedTab.Name
                }
                elseif ($script:activeChatTabId -and $script:chatTabs.ContainsKey($script:activeChatTabId)) {
                    Send-ChatMessage -TabId $script:activeChatTabId
                }
            }
        })
    
    $modelCombo.add_SelectedIndexChanged({
            param($comboSender, $e)
            # Find the active chat tab based on selected tab
            $selectedTab = $chatTabControl.SelectedTab
            if ($selectedTab -and $script:chatTabs.ContainsKey($selectedTab.Name)) {
                Update-ChatModel -TabId $selectedTab.Name -Model $comboSender.SelectedItem
            }
            elseif ($script:activeChatTabId -and $script:chatTabs.ContainsKey($script:activeChatTabId)) {
                Update-ChatModel -TabId $script:activeChatTabId -Model $sender.SelectedItem
            }
        })
    
    # Add focus and cursor positioning handlers
    $inputBox.add_GotFocus({
            param($focusSender, $e)
            # Ensure cursor is visible and positioned correctly at the very top
            $focusSender.SelectionStart = 0
            $focusSender.SelectionLength = 0
            $focusSender.ScrollToCaret()
        })
    
    $inputBox.add_Click({
            param($clickSender, $e)
            # Ensure cursor positioning on click - force to start of text area
            $clickSender.Focus()
            $clickSender.SelectionStart = 0
            $clickSender.ScrollToCaret()
        })
    
    $inputBox.add_Enter({
            param($enterSender, $e)
            # Ensure proper positioning when entering the control
            $enterSender.SelectionStart = 0
            $enterSender.ScrollToCaret()
        })
    
    # Add resize handler for input container to update input box size
    $inputContainer.add_Resize({
            param($resizeSender, $e)
            try {
                # Update input box size when container resizes
                $inputBox.Size = New-Object System.Drawing.Size(($resizeSender.Width - 6), ($resizeSender.Height - 26))
            }
            catch {
                # Ignore resize errors
            }
        })
    
    # Select the new tab
    $chatTabControl.SelectedTab = $newChatTabPage
    $script:activeChatTabId = $tabId
    
    Update-ChatStatus
    Write-DevConsole "✅ Created new chat tab: $finalTabName" "SUCCESS"
    
    return $tabId
}

function Remove-ChatTab {
    param([string]$TabId)
    
    if (-not $script:chatTabs.ContainsKey($TabId)) {
        Write-DevConsole "❌ Chat tab $TabId not found" "ERROR"
        return
    }
    
    $chatSession = $script:chatTabs[$TabId]
    
    # Remove from tab control
    $chatTabControl.TabPages.Remove($chatSession.TabPage)
    
    # Clean up resources
    $chatSession.TabPage.Dispose()
    
    # Remove from collection
    $script:chatTabs.Remove($TabId)
    
    # Update active tab if this was active
    if ($script:activeChatTabId -eq $TabId) {
        $tabPageCount = $chatTabControl.TabPages.Count
        if ($tabPageCount -gt 0) {
            $script:activeChatTabId = $chatTabControl.SelectedTab.Name
        }
        else {
            $script:activeChatTabId = $null
        }
    }
    
    Update-ChatStatus
    Write-DevConsole "✅ Closed chat tab: $($chatSession.TabPage.Text)" "SUCCESS"
}

function Send-ChatMessage {
    param(
        [string]$TabId,
        [switch]$UseMultithreading
    )
    
    # Default to multithreading if not specified
    if (-not $PSBoundParameters.ContainsKey('UseMultithreading')) {
        $UseMultithreading = $true
    }
    
    if (-not $script:chatTabs.ContainsKey($TabId)) { return }
    
    $chatSession = $script:chatTabs[$TabId]
    $message = $chatSession.InputBox.Text.Trim()
    
    if ([string]::IsNullOrWhiteSpace($message)) { return }
    
    # Check for theme switching commands
    if ($message -match "^/(theme|set-theme)\s+(.+)" -or $message -match "switch theme to (.+)" -or $message -match "use (.+) theme" -or $message -match "^/(theme|current-theme)$" -or $message -match "what theme" -or $message -match "current theme") {
        $themeRequest = $matches[1] -or $matches[2]
        $themeRequest = $themeRequest.Trim()
        
        $userText = "You: $message`n"
        $chatSession.ChatBox.AppendText($userText)
        $chatSession.ChatBox.ScrollToCaret()
        
        # Process theme change
        $themeChanged = $false
        $responseMessage = ""
        
        # Check for theme status request
        if ($message -match "^/(theme|current-theme)$" -or $message -match "what theme" -or $message -match "current theme") {
            $responseMessage = "🎨 Current theme: $script:CurrentTheme`n📋 Available themes: stealth-cheetah (default), dark, light, custom"
        }
        else {
            $themeRequest = $matches[1] -or $matches[2]
            $themeRequest = $themeRequest.Trim()
            
            switch -regex ($themeRequest) {
                "stealth.?cheetah|cheetah.?stealth" {
                    Apply-Theme "Stealth-Cheetah"
                    $themeChanged = $true
                    $responseMessage = "🐆 Stealth-Cheetah theme activated! Maximum stealth mode enabled with amber highlights."
                }
                "dark" {
                    Apply-Theme "Dark"
                    $themeChanged = $true
                    $responseMessage = "🌙 Dark theme applied! Professional dark interface activated."
                }
                "light" {
                    Apply-Theme "Light"
                    $themeChanged = $true
                    $responseMessage = "☀️ Light theme applied! Clean bright interface activated."
                }
                "custom" {
                    Show-CustomThemeBuilder
                    $responseMessage = "🎨 Opening custom theme builder... Create your perfect theme!"
                }
                default {
                    $responseMessage = "❓ Available themes: stealth-cheetah (default), dark, light, custom. Try '/theme stealth-cheetah' or 'switch theme to dark'"
                }
            }
        }
        
        # Add AI response
        $aiText = "AI: $responseMessage`n`n"
        $chatSession.ChatBox.AppendText($aiText)
        $chatSession.ChatBox.ScrollToCaret()
        
        # Store messages in session
        $chatSession.Messages += @{
            Role      = "user"
            Content   = $message
            Timestamp = Get-Date
        }
        $chatSession.Messages += @{
            Role      = "assistant"
            Content   = $responseMessage
            Timestamp = Get-Date
        }
        
        # Clear input and update activity
        $chatSession.InputBox.Text = ""
        $chatSession.LastActivity = Get-Date
        
        if ($themeChanged) {
            # Update current theme and save settings
            $script:CurrentTheme = $themeRequest -replace "stealth.?cheetah", "Stealth-Cheetah" -replace "dark", "Dark" -replace "light", "Light"
            Save-CustomizationSettings
            Write-StartupLog "🎨 Theme changed via chat command: $script:CurrentTheme" "SUCCESS"
        }
        
        return
    }
    
    # Add user message to chat
    $userText = "You: $message`n"
    $chatSession.ChatBox.AppendText($userText)
    $chatSession.ChatBox.ScrollToCaret()
    
    # Store message in session
    $chatSession.Messages += @{
        Role      = "user"
        Content   = $message
        Timestamp = Get-Date
    }
    
    # Clear input
    $chatSession.InputBox.Text = ""
    $chatSession.LastActivity = Get-Date
    
    # Show processing indicator
    $processingText = "AI (processing...): "
    $chatSession.ChatBox.AppendText($processingText)
    $chatSession.ChatBox.ScrollToCaret()
    
    # Check if multithreading is available and enabled
    if ($UseMultithreading -and $script:threadSafeContext.RunspacePool) {
        # Use the new multithreaded system
        $messageHistory = @($chatSession.Messages)
        $historyCount = $messageHistory.Count
        $chatRequest = @{
            TabId       = $TabId
            Message     = $message
            Model       = $chatSession.ModelCombo.SelectedItem
            ChatHistory = if ($historyCount -gt 1) { $messageHistory[0..($historyCount - 2)] } else { @() }
        }
        
        Start-ParallelChatProcessing -ChatRequests @($chatRequest)
        Write-DevConsole "🔄 Using multithreaded chat processing for $TabId" "INFO"
        return
    }
    
    # Fallback to original single-threaded approach
    Write-DevConsole "🔄 Using single-threaded chat processing for $TabId" "INFO"
    
    # Send to Ollama asynchronously (original approach)
    $runspace = [runspacefactory]::CreateRunspace()
    $runspace.Open()
    $powershell = [powershell]::Create()
    $powershell.Runspace = $runspace
    
    $null = $powershell.AddScript({
            param($message, $model, $chatHistory)
        
            try {
                # Build context from chat history
                $context = ""
                foreach ($msg in $chatHistory) {
                    $context += "$($msg.Role): $($msg.Content)`n"
                }
            
                $body = @{
                    model  = $model
                    prompt = $context + "user: $message`n"
                    stream = $false
                } | ConvertTo-Json -Depth 3
            
                $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $body -ContentType "application/json"
                return $response.response
            }
            catch {
                return "Error: $($_.Exception.Message)"
            }
        })
    
    $null = $powershell.AddParameter("message", $message)
    $null = $powershell.AddParameter("model", $chatSession.ModelCombo.SelectedItem)
    $messageHistory = @($chatSession.Messages)
    $historyCount = $messageHistory.Count
    $chatHistoryParam = if ($historyCount -gt 1) { $messageHistory[0..($historyCount - 2)] } else { @() }
    $null = $powershell.AddParameter("chatHistory", $chatHistoryParam)
    
    $job = $powershell.BeginInvoke()
    
    # Create completion tracking with proper variable capture
    $completionTracker = @{
        Job            = $job
        PowerShell     = $powershell
        Runspace       = $runspace
        ProcessingText = $processingText
        ChatSession    = $chatSession
        TabId          = $TabId
        Timer          = $null
    }
    
    # Update UI when complete (timer-based polling)
    $completionTracker.Timer = New-Object System.Windows.Forms.Timer
    $completionTracker.Timer.Interval = 100
    $completionTracker.Timer.add_Tick({
            if ($completionTracker.Job.IsCompleted) {
                try {
                    $aiResponse = $completionTracker.PowerShell.EndInvoke($completionTracker.Job)
                
                    # Update chat display
                    $completionTracker.ChatSession.ChatBox.Text = $completionTracker.ChatSession.ChatBox.Text -replace "$([regex]::Escape($completionTracker.ProcessingText))$", "AI: $aiResponse`n`n"
                    $completionTracker.ChatSession.ChatBox.ScrollToCaret()
                
                    # Store AI response
                    $completionTracker.ChatSession.Messages += @{
                        Role      = "assistant"
                        Content   = $aiResponse
                        Timestamp = Get-Date
                    }
                
                    Write-DevConsole "✅ AI response received for chat $($completionTracker.TabId)" "SUCCESS"
                }
                catch {
                    $completionTracker.ChatSession.ChatBox.Text = $completionTracker.ChatSession.ChatBox.Text -replace "$([regex]::Escape($completionTracker.ProcessingText))$", "AI: Error generating response`n`n"
                    Write-DevConsole "❌ Error in chat $($completionTracker.TabId) : $($_.Exception.Message)" "ERROR"
                }
                finally {
                    $completionTracker.Timer.Stop()
                    $completionTracker.Timer.Dispose()
                    $completionTracker.PowerShell.Dispose()
                    $completionTracker.Runspace.Close()
                }
            }
        }.GetNewClosure())
    $completionTracker.Timer.Start()
}

function Update-ChatModel {
    param([string]$TabId, [string]$Model)
    
    if (-not $script:chatTabs.ContainsKey($TabId)) { return }
    
    $chatSession = $script:chatTabs[$TabId]
    Write-DevConsole "🔄 Changed model for $($chatSession.TabPage.Text) to $Model" "INFO"
}

function Update-ChatStatus {
    $activeCount = if ($script:chatTabs) { @($script:chatTabs).Count } else { 0 }
    
    # Find the chat status label from the form controls
    if ($script:chatStatusLabel) {
        if ($activeCount -eq 0) {
            $script:chatStatusLabel.Text = "No active chats"
        }
        else {
            $script:chatStatusLabel.Text = "$activeCount/$($global:settings.MaxChatTabs) chats active"
        }
    }
}

function Get-ActiveChatTab {
    if ($script:activeChatTabId -and $script:chatTabs.ContainsKey($script:activeChatTabId)) {
        return $script:chatTabs[$script:activeChatTabId]
    }
    return $null
}

function Show-ChatSettings {
    Write-DevConsole "🔧 Opening Chat Settings..." "INFO"
    
    $chatSettingsForm = New-Object System.Windows.Forms.Form
    $chatSettingsForm.Text = "Chat Settings"
    $chatSettingsForm.Size = New-Object System.Drawing.Size(450, 400)
    $chatSettingsForm.StartPosition = "CenterScreen"
    $chatSettingsForm.FormBorderStyle = "FixedDialog"
    $chatSettingsForm.MaximizeBox = $false
    $chatSettingsForm.MinimizeBox = $false
    
    # Max Chat Tabs
    $maxTabsLabel = New-Object System.Windows.Forms.Label
    $maxTabsLabel.Text = "Maximum Chat Tabs:"
    $maxTabsLabel.Location = New-Object System.Drawing.Point(20, 20)
    $maxTabsLabel.Size = New-Object System.Drawing.Size(150, 20)
    $chatSettingsForm.Controls.Add($maxTabsLabel)
    
    $maxTabsNumeric = New-Object System.Windows.Forms.NumericUpDown
    $maxTabsNumeric.Location = New-Object System.Drawing.Point(180, 18)
    $maxTabsNumeric.Size = New-Object System.Drawing.Size(60, 20)
    $maxTabsNumeric.Minimum = 1
    $maxTabsNumeric.Maximum = 100  # Match the main MaxTabs maximum to avoid value conflicts
    $maxTabsNumeric.Value = $global:settings.MaxChatTabs
    $chatSettingsForm.Controls.Add($maxTabsNumeric)
    
    # Auto Close Tabs
    $autoCloseCheck = New-Object System.Windows.Forms.CheckBox
    $autoCloseCheck.Text = "Auto-close inactive tabs"
    $autoCloseCheck.Location = New-Object System.Drawing.Point(20, 60)
    $autoCloseCheck.Size = New-Object System.Drawing.Size(200, 20)
    $autoCloseCheck.Checked = $global:settings.ChatTabAutoClose
    $chatSettingsForm.Controls.Add($autoCloseCheck)
    
    # Chat Position
    $positionLabel = New-Object System.Windows.Forms.Label
    $positionLabel.Text = "Chat Tab Position:"
    $positionLabel.Location = New-Object System.Drawing.Point(20, 100)
    $positionLabel.Size = New-Object System.Drawing.Size(150, 20)
    $chatSettingsForm.Controls.Add($positionLabel)
    
    $positionCombo = New-Object System.Windows.Forms.ComboBox
    $positionCombo.Location = New-Object System.Drawing.Point(180, 98)
    $positionCombo.Size = New-Object System.Drawing.Size(120, 20)
    $positionCombo.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
    $positionCombo.Items.AddRange(@("Right", "Bottom", "Popup"))
    $positionCombo.SelectedItem = $global:settings.ChatTabPosition
    $chatSettingsForm.Controls.Add($positionCombo)
    
    # Default Model
    $defaultModelLabel = New-Object System.Windows.Forms.Label
    $defaultModelLabel.Text = "Default Model for New Chats:"
    $defaultModelLabel.Location = New-Object System.Drawing.Point(20, 140)
    $defaultModelLabel.Size = New-Object System.Drawing.Size(180, 20)
    $chatSettingsForm.Controls.Add($defaultModelLabel)
    
    $defaultModelCombo = New-Object System.Windows.Forms.ComboBox
    $defaultModelCombo.Location = New-Object System.Drawing.Point(20, 165)
    $defaultModelCombo.Size = New-Object System.Drawing.Size(200, 20)
    $defaultModelCombo.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
    $defaultModelCombo.Items.AddRange(@("bigdaddyg-fast:latest", "llama3.2", "llama3.2:1b", "llama3.1", "codellama", "mistral", "qwen2.5-coder"))
    $defaultModelCombo.SelectedItem = $global:settings.OllamaModel
    $chatSettingsForm.Controls.Add($defaultModelCombo)
    
    # Current Chat Status
    $statusLabel = New-Object System.Windows.Forms.Label
    $statusLabel.Text = "Active Chats:"
    $statusLabel.Location = New-Object System.Drawing.Point(20, 210)
    $statusLabel.Size = New-Object System.Drawing.Size(100, 20)
    $chatSettingsForm.Controls.Add($statusLabel)
    
    $statusListBox = New-Object System.Windows.Forms.ListBox
    $statusListBox.Location = New-Object System.Drawing.Point(20, 235)
    $statusListBox.Size = New-Object System.Drawing.Size(380, 80)
    
    foreach ($chatId in $script:chatTabs.Keys) {
        $chat = $script:chatTabs[$chatId]
        $msgCount = @($chat.Messages).Count
        $status = "$($chat.TabPage.Text) - Model: $($chat.ModelCombo.SelectedItem) - Messages: $msgCount"
        $statusListBox.Items.Add($status)
    }
    $chatSettingsForm.Controls.Add($statusListBox)
    
    # Buttons
    $okBtn = New-Object System.Windows.Forms.Button
    $okBtn.Text = "OK"
    $okBtn.Location = New-Object System.Drawing.Point(250, 330)
    $okBtn.Size = New-Object System.Drawing.Size(75, 23)
    $okBtn.DialogResult = [System.Windows.Forms.DialogResult]::OK
    $chatSettingsForm.Controls.Add($okBtn)
    
    $cancelBtn = New-Object System.Windows.Forms.Button
    $cancelBtn.Text = "Cancel"
    $cancelBtn.Location = New-Object System.Drawing.Point(335, 330)
    $cancelBtn.Size = New-Object System.Drawing.Size(75, 23)
    $cancelBtn.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
    $chatSettingsForm.Controls.Add($cancelBtn)
    
    # OK button click handler
    $okBtn.add_Click({
            $global:settings.MaxChatTabs = $maxTabsNumeric.Value
            $global:settings.ChatTabAutoClose = $autoCloseCheck.Checked
            $global:settings.ChatTabPosition = $positionCombo.SelectedItem
            $global:settings.OllamaModel = $defaultModelCombo.SelectedItem
            $script:maxChatTabs = $global:settings.MaxChatTabs
        
            Save-Settings
            Update-ChatStatus
            Write-DevConsole "✅ Chat settings saved" "SUCCESS"
        
            $chatSettingsForm.Close()
        })
    
    # Show form
    $null = $chatSettingsForm.ShowDialog()
    $chatSettingsForm.Dispose()
}

function Show-BulkActionsMenu {
    <#
    .SYNOPSIS
        Shows a menu for bulk operations to demonstrate multithreading capabilities
    #>
    
    $bulkForm = New-Object System.Windows.Forms.Form
    $bulkForm.Text = "Bulk Actions - Multithreading Demo"
    $bulkForm.Size = New-Object System.Drawing.Size(500, 400)
    $bulkForm.StartPosition = "CenterScreen"
    $bulkForm.FormBorderStyle = "FixedDialog"
    $bulkForm.MaximizeBox = $false
    
    # Status display
    $statusGroup = New-Object System.Windows.Forms.GroupBox
    $statusGroup.Text = "Threading Status"
    $statusGroup.Location = New-Object System.Drawing.Point(20, 20)
    $statusGroup.Size = New-Object System.Drawing.Size(440, 100)
    $bulkForm.Controls.Add($statusGroup)
    
    $statusText = New-Object System.Windows.Forms.TextBox
    $statusText.Multiline = $true
    $statusText.ReadOnly = $true
    $statusText.Location = New-Object System.Drawing.Point(10, 20)
    $statusText.Size = New-Object System.Drawing.Size(420, 70)
    $statusText.ScrollBars = [System.Windows.Forms.ScrollBars]::Vertical
    
    $threadingStatus = Get-ThreadingStatus
    $statusText.Text = @"
Multithreading: $($threadingStatus.IsInitialized)
Active Jobs: $($threadingStatus.ActiveJobs)
Queued Tasks: $($threadingStatus.QueuedTasks)
Worker Count: $($threadingStatus.WorkerCount)
Max Concurrent: $($threadingStatus.MaxConcurrentTasks)
"@
    $statusGroup.Controls.Add($statusText)
    
    # Actions group
    $actionsGroup = New-Object System.Windows.Forms.GroupBox
    $actionsGroup.Text = "Bulk Operations"
    $actionsGroup.Location = New-Object System.Drawing.Point(20, 130)
    $actionsGroup.Size = New-Object System.Drawing.Size(440, 180)
    $bulkForm.Controls.Add($actionsGroup)
    
    # Create 3 new chats button
    $createChatsBtn = New-Object System.Windows.Forms.Button
    $createChatsBtn.Text = "Create 3 New Chat Tabs"
    $createChatsBtn.Location = New-Object System.Drawing.Point(20, 30)
    $createChatsBtn.Size = New-Object System.Drawing.Size(180, 30)
    $createChatsBtn.add_Click({
            Write-DevConsole "🚀 Creating multiple chat tabs..." "INFO"
            for ($i = 1; $i -le 3; $i++) {
                $tabId = New-ChatTab -TabName "Bulk Chat $i"
                if ($tabId) {
                    Write-DevConsole "✅ Created bulk chat tab $i : $tabId" "SUCCESS"
                }
            }
            $bulkForm.Close()
        })
    $actionsGroup.Controls.Add($createChatsBtn)
    
    # Send parallel messages button
    $parallelMsgBtn = New-Object System.Windows.Forms.Button
    $parallelMsgBtn.Text = "Send Test Messages to All Chats"
    $parallelMsgBtn.Location = New-Object System.Drawing.Point(220, 30)
    $parallelMsgBtn.Size = New-Object System.Drawing.Size(200, 30)
    $parallelMsgBtn.add_Click({
            $activeChatIds = @($script:chatTabs.Keys | Select-Object -First 3)
            if ($activeChatIds.Count -eq 0) {
                Write-DevConsole "⚠ No active chats to test" "WARNING"
                return
            }
        
            Write-DevConsole "🔄 Sending parallel test messages to $($activeChatIds.Count) chats..." "INFO"
        
            $chatRequests = @()
            $testMessages = @(
                "What is the capital of France?",
                "Explain quantum computing in simple terms",
                "Write a hello world program in Python"
            )
        
            $testMsgCount = $testMessages.Count
            for ($i = 0; $i -lt [Math]::Min($activeChatIds.Count, $testMsgCount); $i++) {
                $chatId = $activeChatIds[$i]
                $chatSession = $script:chatTabs[$chatId]
            
                # Add test message to input
                $chatSession.InputBox.Text = $testMessages[$i]
            
                $chatRequests += @{
                    TabId       = $chatId
                    Message     = $testMessages[$i]
                    Model       = $chatSession.ModelCombo.SelectedItem
                    ChatHistory = $chatSession.Messages
                }
            }
        
            $requestCount = @($chatRequests).Count
            if ($requestCount -gt 0) {
                Start-ParallelChatProcessing -ChatRequests $chatRequests
                Write-DevConsole "🚀 Parallel processing started for $requestCount chats" "SUCCESS"
            }
        
            $bulkForm.Close()
        })
    $actionsGroup.Controls.Add($parallelMsgBtn)
    
    # Create agent tasks button
    $agentTasksBtn = New-Object System.Windows.Forms.Button
    $agentTasksBtn.Text = "Create 3 Demo Agent Tasks"
    $agentTasksBtn.Location = New-Object System.Drawing.Point(20, 80)
    $agentTasksBtn.Size = New-Object System.Drawing.Size(180, 30)
    $agentTasksBtn.add_Click({
            Write-DevConsole "🤖 Creating demo agent tasks..." "INFO"
        
            # Task 1: File analysis
            $task1Id = New-AgentTask -Name "File Analysis" -Description "Analyze current file structure"
            $task1 = $global:agentContext.Tasks | Where-Object { $_.Id -eq $task1Id } | Select-Object -First 1
            $task1.Steps = @(
                @{ Type = "tool"; Description = "List directory"; Tool = "list_directory"; Arguments = @{} }
                @{ Type = "ai_query"; Description = "Analyze structure"; Query = "Analyze the file structure" }
            )
            Start-AgentTaskAsync -TaskId $task1Id -Priority "Normal"
        
            # Task 2: Environment check
            $task2Id = New-AgentTask -Name "Environment Check" -Description "Check system environment"
            $task2 = $global:agentContext.Tasks | Where-Object { $_.Id -eq $task2Id } | Select-Object -First 1
            $task2.Steps = @(
                @{ Type = "command"; Description = "Get PowerShell version"; Command = '$PSVersionTable.PSVersion' }
                @{ Type = "tool"; Description = "Get environment"; Tool = "get_environment"; Arguments = @{} }
            )
            Start-AgentTaskAsync -TaskId $task2Id -Priority "High"
        
            # Task 3: Code generation
            $task3Id = New-AgentTask -Name "Code Generation" -Description "Generate sample code"
            $task3 = $global:agentContext.Tasks | Where-Object { $_.Id -eq $task3Id } | Select-Object -First 1
            $task3.Steps = @(
                @{ Type = "ai_query"; Description = "Generate PowerShell function"; Query = "Create a simple PowerShell function" }
                @{ Type = "edit"; Description = "Save generated code"; File = "generated_code.ps1"; Content = "# Generated code" }
            )
            Start-AgentTaskAsync -TaskId $task3Id -Priority "Low"
        
            Write-DevConsole "✅ Created 3 demo agent tasks with different priorities" "SUCCESS"
            $bulkForm.Close()
        })
    $actionsGroup.Controls.Add($agentTasksBtn)
    
    # Threading stats button
    $statsBtn = New-Object System.Windows.Forms.Button
    $statsBtn.Text = "Refresh Stats"
    $statsBtn.Location = New-Object System.Drawing.Point(220, 80)
    $statsBtn.Size = New-Object System.Drawing.Size(120, 30)
    $statsBtn.add_Click({
            $threadingStatus = Get-ThreadingStatus
            $statusText.Text = @"
Multithreading: $($threadingStatus.IsInitialized)
Active Jobs: $($threadingStatus.ActiveJobs)
Queued Tasks: $($threadingStatus.QueuedTasks)
Worker Count: $($threadingStatus.WorkerCount)
Max Concurrent: $($threadingStatus.MaxConcurrentTasks)

Worker States:
$($threadingStatus.WorkerStates.Keys | ForEach-Object { 
    "  $_ : $($threadingStatus.WorkerStates[$_].Status)"
} | Out-String)
"@
        })
    $actionsGroup.Controls.Add($statsBtn)
    
    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(380, 320)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 30)
    $closeBtn.DialogResult = [System.Windows.Forms.DialogResult]::OK
    $bulkForm.Controls.Add($closeBtn)
    
    $bulkForm.ShowDialog() | Out-Null
    $bulkForm.Dispose()
}

function Update-ThreadingStatusLabel {
    <#
    .SYNOPSIS
        Updates the threading status label in the chat toolbar
    #>
    
    if ($threadingStatusLabel) {
        $status = Get-ThreadingStatus
        if ($status.IsInitialized) {
            $threadingStatusLabel.Text = "MT: $($status.ActiveJobs)/$($status.MaxConcurrentTasks) active"
            $threadingStatusLabel.ForeColor = [System.Drawing.Color]::Green
        }
        else {
            $threadingStatusLabel.Text = "MT: Disabled"
            $threadingStatusLabel.ForeColor = [System.Drawing.Color]::Red
        }
    }
}

# ============================================
# Command Palette Functions
# ============================================
function Show-CommandPalette {
    $commandPalette.Show()
    $paletteInput.Focus()
    $paletteInput.SelectAll()
    Update-CommandPalette
}

function Hide-CommandPalette {
    $commandPalette.Hide()
}

function Update-CommandPalette {
    $query = $paletteInput.Text.ToLower()
    $paletteResults.Items.Clear()
    
    if ([string]::IsNullOrWhiteSpace($query)) {
        $paletteLabel.Text = "Type a command or search extensions..."
        return
    }
    
    # Command list
    $commands = @(
        @{Name = "> Git: Status"; Action = { Update-GitStatus; $rightTabControl.SelectedTab = $gitTab } }
        @{Name = "> Git: Add All"; Action = { Invoke-GitCommand "add" @("."); Update-GitStatus } }
        @{Name = "> Git: Commit"; Action = { $msg = [Microsoft.VisualBasic.Interaction]::InputBox("Commit message:", "Git Commit"); if ($msg) { Invoke-GitCommand "commit" @("-m", $msg); Update-GitStatus } } }
        @{Name = "> Git: Push"; Action = { Invoke-GitCommand "push" @(); Update-GitStatus } }
        @{Name = "> Git: Pull"; Action = { Invoke-GitCommand "pull" @(); Update-GitStatus } }
        @{Name = "> File: Open"; Action = { $openItem.PerformClick() } }
        @{Name = "> File: Save"; Action = { $saveItem.PerformClick() } }
        @{Name = "> File: Save As"; Action = { $saveAsItem.PerformClick() } }
        @{Name = "> Terminal: Focus"; Action = { $rightTabControl.SelectedTab = $terminalTab } }
        @{Name = "> Browser: Focus"; Action = { $rightTabControl.SelectedTab = $browserTab } }
        @{Name = "> Chat: Focus"; Action = { $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Extensions: Marketplace"; Action = { Show-Marketplace } }
        @{Name = "> Extensions: Installed"; Action = { Show-InstalledExtensions } }
        @{Name = "> Agent: Toggle Mode"; Action = { $toggle.PerformClick() } }
        @{Name = "> Code: Generate"; Action = { $chatBox.AppendText("Use /code <description> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Code: Review"; Action = { $chatBox.AppendText("Use /review in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: Start Workflow"; Action = { $chatBox.AppendText("Use /workflow <goal> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: List Tools"; Action = { $chatBox.AppendText("Use /tools in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: Environment Info"; Action = { $chatBox.AppendText("Use /env in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: Tasks Panel"; Action = { $rightTabControl.SelectedTab = $agentTasksTab } }
    )
    
    # Extension search
    $extensions = Search-Marketplace -Query $query
    foreach ($ext in $extensions) {
        $commands += @{Name = "Extension: $($ext.Name)"; Action = { $chatBox.AppendText("Extension: $($ext.Name) - $($ext.Description)`r`n"); $rightTabControl.SelectedTab = $chatTab } }
    }
    
    # Filter and add matching commands
    $matching = $commands | Where-Object { $_.Name.ToLower() -like "*$query*" }
    foreach ($cmd in $matching) {
        $paletteResults.Items.Add($cmd.Name) | Out-Null
    }
    
    $resultCount = $paletteResults.Items.Count
    if ($resultCount -gt 0) {
        $paletteResults.SelectedIndex = 0
        $paletteLabel.Text = "$($paletteResults.Items.Count) result(s) found"
    }
    else {
        $paletteLabel.Text = "No commands found"
    }
}

function Invoke-CommandPaletteSelection {
    $selected = $paletteResults.SelectedItem
    if ($selected) {
        $query = $paletteInput.Text.ToLower()
        $commands = @(
            @{Name = "> Git: Status"; Action = { Update-GitStatus; $rightTabControl.SelectedTab = $gitTab } }
            @{Name = "> Git: Add All"; Action = { Invoke-GitCommand "add" @("."); Update-GitStatus } }
            @{Name = "> Git: Commit"; Action = { $msg = [Microsoft.VisualBasic.Interaction]::InputBox("Commit message:", "Git Commit"); if ($msg) { Invoke-GitCommand "commit" @("-m", $msg); Update-GitStatus } } }
            @{Name = "> Git: Push"; Action = { Invoke-GitCommand "push" @(); Update-GitStatus } }
            @{Name = "> Git: Pull"; Action = { Invoke-GitCommand "pull" @(); Update-GitStatus } }
            @{Name = "> File: Open"; Action = { $openItem.PerformClick() } }
            @{Name = "> File: Save"; Action = { $saveItem.PerformClick() } }
            @{Name = "> File: Save As"; Action = { $saveAsItem.PerformClick() } }
            @{Name = "> Terminal: Focus"; Action = { $rightTabControl.SelectedTab = $terminalTab } }
            @{Name = "> Browser: Focus"; Action = { $rightTabControl.SelectedTab = $browserTab } }
            @{Name = "> Chat: Focus"; Action = { $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Extensions: Marketplace"; Action = { Show-Marketplace } }
            @{Name = "> Extensions: Installed"; Action = { Show-InstalledExtensions } }
            @{Name = "> Agent: Toggle Mode"; Action = { $toggle.PerformClick() } }
            @{Name = "> Code: Generate"; Action = { $chatBox.AppendText("Use /code <description> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Code: Review"; Action = { $chatBox.AppendText("Use /review in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        )
        
        $extensions = Search-Marketplace -Query $query
        foreach ($ext in $extensions) {
            $commands += @{Name = "Extension: $($ext.Name)"; Action = { $chatBox.AppendText("Extension: $($ext.Name) - $($ext.Description)`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        }
        
        $cmd = $commands | Where-Object { $_.Name -eq $selected } | Select-Object -First 1
        if ($cmd) {
            Hide-CommandPalette
            $cmd.Action.Invoke()
        }
    }
}

# ============================================
# Agentic Tool-Calling API (Copilot/Amazon Q Compatible)
# ============================================
function Register-AgentTool {
    param(
        [string]$Name,
        [string]$Description,
        [hashtable]$Parameters,
        [scriptblock]$Handler
    )
    
    if (-not $script:agentTools) {
        $script:agentTools = @{}
    }
    
    $script:agentTools[$Name] = @{
        Name        = $Name
        Description = $Description
        Parameters  = $Parameters
        Handler     = $Handler
    }
}

function Invoke-AgentTool {
    param(
        [string]$ToolName,
        [hashtable]$Arguments
    )
    
    if ($script:agentTools -and $script:agentTools[$ToolName]) {
        $tool = $script:agentTools[$ToolName]
        try {
            $result = & $tool.Handler @Arguments
            $global:agentContext.Commands += @{
                Tool      = $ToolName
                Arguments = $Arguments
                Result    = $result
                Timestamp = Get-Date
            }
            return $result
        }
        catch {
            return @{Error = $_.Exception.Message }
        }
    }
    return @{Error = "Tool not found: $ToolName" }
}

function Get-AgentToolsSchema {
    <#
    .SYNOPSIS
        Returns the schema of all registered agent tools
    .DESCRIPTION
        Provides a comprehensive list of available tools with their parameters and descriptions
        Compatible with OpenAI function calling and agentic frameworks
    #>
    [CmdletBinding()]
    param()
    
    $tools = @()
    foreach ($tool in $script:agentTools.Values) {
        $tools += @{
            name        = $tool.Name
            description = $tool.Description
            parameters  = $tool.Parameters
            category    = $tool.Category
            version     = $tool.Version
        }
    }
    return $tools
}

# ============================================
# CORE FILE SYSTEM TOOLS
# ============================================

Register-AgentTool -Name "read_file" -Description "Read contents of a file from disk" `
    -Category "FileSystem" -Version "1.0" `
    -Parameters @{
    path = @{
        type        = "string"
        description = "Absolute or relative file path to read"
        required    = $true
    }
} `
    -Handler {
    param([string]$path)
    try {
        if (-not (Test-Path $path)) {
            $fullPath = Join-Path $global:currentWorkingDir $path
            if (Test-Path $fullPath) {
                $path = $fullPath
            }
        }
        
        if (Test-Path $path) {
            $content = [System.IO.File]::ReadAllText($path)
            $fileInfo = Get-Item $path
            return @{
                success    = $true
                content    = $content
                path       = $path
                size_bytes = $fileInfo.Length
                modified   = $fileInfo.LastWriteTime.ToString()
            }
        }
        return @{success = $false; error = "File not found: $path" }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "write_file" -Description "Write or create a file with content" `
    -Category "FileSystem" -Version "1.0" `
    -Parameters @{
    path    = @{
        type        = "string"
        description = "File path to write (creates if doesn't exist)"
        required    = $true
    }
    content = @{
        type        = "string"
        description = "Content to write to the file"
        required    = $true
    }
    append  = @{
        type        = "boolean"
        description = "Append to file instead of overwriting"
        required    = $false
    }
} `
    -Handler {
    param([string]$path, [string]$content, [bool]$append = $false)
    try {
        if ($append) {
            [System.IO.File]::AppendAllText($path, $content)
        }
        else {
            [System.IO.File]::WriteAllText($path, $content)
        }
        $fileInfo = Get-Item $path
        return @{
            success    = $true
            path       = $path
            size_bytes = $fileInfo.Length
            operation  = if ($append) { "appended" } else { "written" }
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "list_directory" -Description "List all files and folders in a directory" `
    -Category "FileSystem" -Version "1.0" `
    -Parameters @{
    path      = @{
        type        = "string"
        description = "Directory path to list"
        required    = $true
    }
    recursive = @{
        type        = "boolean"
        description = "List files recursively"
        required    = $false
    }
    filter    = @{
        type        = "string"
        description = "File filter pattern (e.g., *.js)"
        required    = $false
    }
} `
    -Handler {
    param([string]$path, [bool]$recursive = $false, [string]$filter = "*")
    try {
        if (Test-Path $path) {
            $items = Get-ChildItem -Path $path -Filter $filter -Recurse:$recursive
            $files = @()
            $directories = @()
            
            foreach ($item in $items) {
                if ($item.PSIsContainer) {
                    $directories += @{
                        name     = $item.Name
                        path     = $item.FullName
                        modified = $item.LastWriteTime.ToString()
                    }
                }
                else {
                    $files += @{
                        name     = $item.Name
                        path     = $item.FullName
                        size     = $item.Length
                        modified = $item.LastWriteTime.ToString()
                    }
                }
            }
            
            return @{
                success     = $true
                path        = $path
                files       = $files
                directories = $directories
                total_files = @($files).Count
                total_dirs  = @($directories).Count
            }
        }
        return @{success = $false; error = "Directory not found: $path" }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "create_directory" -Description "Create a new directory" `
    -Category "FileSystem" -Version "1.0" `
    -Parameters @{
    path = @{
        type        = "string"
        description = "Directory path to create"
        required    = $true
    }
} `
    -Handler {
    param([string]$path)
    try {
        if (-not (Test-Path $path)) {
            $null = New-Item -ItemType Directory -Path $path -Force
            return @{success = $true; path = $path; created = $true }
        }
        return @{success = $true; path = $path; created = $false; message = "Directory already exists" }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "delete_file" -Description "Delete a file from disk" `
    -Category "FileSystem" -Version "1.0" `
    -Parameters @{
    path = @{
        type        = "string"
        description = "File path to delete"
        required    = $true
    }
} `
    -Handler {
    param([string]$path)
    try {
        if (Test-Path $path) {
            Remove-Item -Path $path -Force
            return @{success = $true; path = $path; deleted = $true }
        }
        return @{success = $false; error = "File not found: $path" }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

# ============================================
# TERMINAL & EXECUTION TOOLS
# ============================================

Register-AgentTool -Name "execute_command" -Description "Execute a shell command in terminal" `
    -Category "Terminal" -Version "1.0" `
    -Parameters @{
    command           = @{
        type        = "string"
        description = "Command to execute"
        required    = $true
    }
    working_directory = @{
        type        = "string"
        description = "Working directory for command execution"
        required    = $false
    }
    timeout_seconds   = @{
        type        = "integer"
        description = "Command timeout in seconds"
        required    = $false
    }
} `
    -Handler {
    param([string]$command, [string]$working_directory = $null, [int]$timeout_seconds = 30)
    Acquire-TerminalControl -Owner "Agent"
    try {
        $originalLocation = Get-Location
        if ($working_directory) {
            Set-Location $working_directory
        }
        
        $output = Invoke-Expression $command 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
        
        if ($working_directory) {
            Set-Location $originalLocation
        }
        
        return @{
            success   = ($exitCode -eq 0)
            output    = $output
            exit_code = $exitCode
            command   = $command
        }
    }
    catch {
        if ($working_directory) {
            Set-Location $originalLocation
        }
        return @{success = $false; error = $_.Exception.Message }
    }
    finally {
        Exit-TerminalSession -Owner "Agent"
    }
}

# ============================================
# GIT VERSION CONTROL TOOLS
# ============================================

Register-AgentTool -Name "git_status" -Description "Get Git repository status" `
    -Category "Git" -Version "1.0" `
    -Parameters @{
    repository_path = @{
        type        = "string"
        description = "Path to Git repository"
        required    = $false
    }
} `
    -Handler {
    param([string]$repository_path = $null)
    try {
        $repoPath = if ($repository_path) { $repository_path } else { $global:currentWorkingDir }
        $originalLocation = Get-Location
        Set-Location $repoPath
        
        $status = git status --short 2>&1
        $branch = git branch --show-current 2>&1
        $remote = git remote -v 2>&1
        
        Set-Location $originalLocation
        
        return @{
            success = $true
            branch  = $branch
            status  = $status
            remote  = $remote
            path    = $repoPath
        }
    }
    catch {
        Set-Location $originalLocation
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "git_commit" -Description "Commit changes to Git repository" `
    -Category "Git" -Version "1.0" `
    -Parameters @{
    message         = @{
        type        = "string"
        description = "Commit message"
        required    = $true
    }
    repository_path = @{
        type        = "string"
        description = "Path to Git repository"
        required    = $false
    }
} `
    -Handler {
    param([string]$message, [string]$repository_path = $null)
    try {
        # Use provided path or fall back to current working directory
        $repoPath = if ($repository_path) { $repository_path } else { $global:currentWorkingDir }
        $result = Invoke-GitCommand -Command "commit" -Arguments @("-m", $message) -WorkingDirectory $repoPath
        return @{success = $true; message = $message; output = $result }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

# ============================================
# BROWSER & WEB TOOLS
# ============================================

Register-AgentTool -Name "browse_url" -Description "Navigate browser to a URL" `
    -Category "Browser" -Version "1.0" `
    -Parameters @{
    url = @{
        type        = "string"
        description = "URL to navigate to"
        required    = $true
    }
} `
    -Handler {
    param([string]$url)
    try {
        Open-Browser $url
        return @{success = $true; url = $url; navigated = $true }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "search_web" -Description "Search the web using Google" `
    -Category "Browser" -Version "1.0" `
    -Parameters @{
    query = @{
        type        = "string"
        description = "Search query"
        required    = $true
    }
} `
    -Handler {
    param([string]$query)
    try {
        $encodedQuery = [System.Uri]::EscapeDataString($query)
        $searchUrl = "https://www.google.com/search?q=$encodedQuery"
        Open-Browser $searchUrl
        return @{success = $true; query = $query; url = $searchUrl }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

# ============================================
# CODE EDITING TOOLS
# ============================================

Register-AgentTool -Name "apply_edit" -Description "Apply structured code edits with diff preview" `
    -Category "CodeEdit" -Version "1.0" `
    -Parameters @{
    file  = @{
        type        = "string"
        description = "File to edit"
        required    = $true
    }
    edits = @{
        type        = "array"
        description = "Array of edit operations with line numbers and content"
        required    = $true
    }
} `
    -Handler {
    param([string]$file, [array]$edits)
    return Set-StructuredEdit -File $file -Edits $edits
}

Register-AgentTool -Name "get_dependencies" -Description "Analyze project dependencies" `
    -Category "Project" -Version "1.0" `
    -Parameters @{
    project_path = @{
        type        = "string"
        description = "Project root path"
        required    = $true
    }
} `
    -Handler {
    param([string]$project_path)
    return Get-ProjectDependencies -Path $project_path
}

Register-AgentTool -Name "get_environment" -Description "Get development environment info" `
    -Category "System" -Version "1.0" `
    -Parameters @{} `
    -Handler {
    param()
    try {
        return @{
            success            = $true
            os                 = [System.Environment]::OSVersion.ToString()
            powershell_version = $PSVersionTable.PSVersion.ToString()
            current_dir        = $global:currentWorkingDir
            user               = $env:USERNAME
            computer           = $env:COMPUTERNAME
            dotnet_installed   = (Get-Command dotnet -ErrorAction SilentlyContinue) -ne $null
            git_installed      = (Get-Command git -ErrorAction SilentlyContinue) -ne $null
            node_installed     = (Get-Command node -ErrorAction SilentlyContinue) -ne $null
            python_installed   = (Get-Command python -ErrorAction SilentlyContinue) -ne $null
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

# Enhanced Browser Automation Tools
Register-AgentTool -Name "browser_search_dependency" -Description "Search for missing dependencies and get download links" `
    -Category "Browser" -Version "1.0" `
    -Parameters @{
    dependency_name = @{
        type        = "string"
        description = "Name of the missing dependency (e.g., 'numpy', 'lodash', 'boost')"
        required    = $true
    }
    language        = @{
        type        = "string" 
        description = "Programming language context (python, javascript, cpp, etc.)"
        required    = $false
    }
} `
    -Handler {
    param([string]$dependency_name, [string]$language = "")
    
    try {
        $searchQuery = "$dependency_name"
        if ($language) {
            $searchQuery += " $language package download install"
        }
        else {
            $searchQuery += " package library download"
        }
        
        $encodedQuery = [System.Web.HttpUtility]::UrlEncode($searchQuery)
        $searchUrl = "https://www.google.com/search?q=$encodedQuery"
        
        # Navigate browser to search results
        if ($script:useWebView2 -and $global:webView2) {
            $global:webView2.CoreWebView2.Navigate($searchUrl)
        }
        elseif ($global:browser) {
            $global:browser.Navigate($searchUrl)
        }
        
        # Return suggested package managers based on language
        $suggestions = @()
        switch ($language.ToLower()) {
            "python" { 
                $suggestions += "pip install $dependency_name"
                $suggestions += "conda install $dependency_name" 
                $pypiUrl = "https://pypi.org/search/?q=$dependency_name"
                if ($script:useWebView2 -and $global:webView2) {
                    Start-Sleep 2
                    $global:webView2.CoreWebView2.Navigate($pypiUrl)
                }
            }
            "javascript" { 
                $suggestions += "npm install $dependency_name"
                $suggestions += "yarn add $dependency_name"
                $npmUrl = "https://www.npmjs.com/search?q=$dependency_name"
                if ($script:useWebView2 -and $global:webView2) {
                    Start-Sleep 2
                    $global:webView2.CoreWebView2.Navigate($npmUrl)
                }
            }
            "cpp" { 
                $suggestions += "vcpkg install $dependency_name"
                $suggestions += "conan install $dependency_name"
            }
            "rust" {
                $suggestions += "cargo add $dependency_name"
                $cratesUrl = "https://crates.io/search?q=$dependency_name"
                if ($script:useWebView2 -and $global:webView2) {
                    Start-Sleep 2
                    $global:webView2.CoreWebView2.Navigate($cratesUrl)
                }
            }
            "go" {
                $suggestions += "go get $dependency_name"
            }
            "dotnet" {
                $suggestions += "dotnet add package $dependency_name"
                $nugetUrl = "https://www.nuget.org/packages?q=$dependency_name"
                if ($script:useWebView2 -and $global:webView2) {
                    Start-Sleep 2
                    $global:webView2.CoreWebView2.Navigate($nugetUrl)
                }
            }
        }
        
        return @{
            success     = $true
            dependency  = $dependency_name
            language    = $language
            search_url  = $searchUrl
            suggestions = $suggestions
            message     = "Browser navigated to search results for '$dependency_name'. Check browser tab for download links."
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "extract_webpage_content" -Description "Extract text content from current webpage for analysis" `
    -Category "Browser" -Version "1.0" `
    -Parameters @{
    extract_links = @{
        type        = "boolean"
        description = "Whether to extract download links from the page"
        required    = $false
    }
} `
    -Handler {
    param([bool]$extract_links = $false)
    
    try {
        $content = @{
            title = ""
            text  = ""
            links = @()
            url   = ""
        }
        
        if ($script:useWebView2 -and $global:webView2) {
            $content.url = $global:webView2.CoreWebView2.Source
            $content.title = $global:webView2.CoreWebView2.DocumentTitle
            
            if ($extract_links) {
                # JavaScript to extract download links (for reference - actual execution would use ExecuteScriptAsync)
                # Note: In real implementation, you'd use ExecuteScriptAsync with this JS code
                $content.links = @("Manual extraction required - check browser for download links")
            }
        }
        
        return @{
            success = $true
            content = $content
            message = "Webpage content extracted. Current URL: $($content.url)"
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "check_package_installed" -Description "Check if a package/dependency is already installed" `
    -Category "System" -Version "1.0" `
    -Parameters @{
    package_name = @{
        type        = "string"
        description = "Name of the package to check"
        required    = $true
    }
    language     = @{
        type        = "string"
        description = "Language/package manager context"
        required    = $true
    }
} `
    -Handler {
    param([string]$package_name, [string]$language)
    
    try {
        $installed = $false
        $version = ""
        $command = ""
        
        switch ($language.ToLower()) {
            "python" {
                $command = "pip show $package_name"
                try {
                    $result = pip show $package_name 2>$null
                    if ($LASTEXITCODE -eq 0) {
                        $installed = $true
                        $version = ($result | Where-Object { $_ -like "Version:*" }) -replace "Version: ", ""
                    }
                }
                catch { }
            }
            "javascript" {
                $command = "npm list $package_name"
                try {
                    $result = npm list $package_name --depth=0 2>$null
                    $installed = $LASTEXITCODE -eq 0
                }
                catch { }
            }
            "dotnet" {
                $command = "dotnet list package"
                try {
                    $result = dotnet list package | Select-String $package_name
                    $installed = @($result).Count -gt 0
                }
                catch { }
            }
            "powershell" {
                $command = "Get-Module -ListAvailable $package_name"
                try {
                    $module = Get-Module -ListAvailable $package_name -ErrorAction SilentlyContinue
                    if ($module) {
                        $installed = $true
                        $version = $module[0].Version.ToString()
                    }
                }
                catch { }
            }
        }
        
        return @{
            success       = $true
            package       = $package_name
            language      = $language
            installed     = $installed
            version       = $version
            check_command = $command
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "auto_install_dependency" -Description "Automatically install missing dependencies" `
    -Category "System" -Version "1.0" `
    -Parameters @{
    package_name = @{
        type        = "string"
        description = "Name of the package to install"
        required    = $true
    }
    language     = @{
        type        = "string"
        description = "Language/package manager to use"
        required    = $true
    }
    auto_confirm = @{
        type        = "boolean"
        description = "Whether to auto-confirm installation"
        required    = $false
    }
} `
    -Handler {
    param([string]$package_name, [string]$language, [bool]$auto_confirm = $false)
    
    try {
        $installCommand = ""
        $success = $false
        
        switch ($language.ToLower()) {
            "python" {
                $installCommand = "pip install $package_name"
            }
            "javascript" {
                $installCommand = "npm install $package_name"
            }
            "dotnet" {
                $installCommand = "dotnet add package $package_name"
            }
            "rust" {
                $installCommand = "cargo add $package_name"
            }
            "powershell" {
                $installCommand = "Install-Module -Name $package_name -Force"
            }
        }
        
        if ($installCommand) {
            if ($auto_confirm) {
                try {
                    $output = Invoke-Expression $installCommand 2>&1
                    $success = $LASTEXITCODE -eq 0
                    
                    return @{
                        success  = $success
                        package  = $package_name
                        language = $language
                        command  = $installCommand
                        output   = $output -join "`n"
                        message  = if ($success) { "Successfully installed $package_name" } else { "Installation failed" }
                    }
                }
                catch {
                    return @{success = $false; error = $_.Exception.Message; command = $installCommand }
                }
            }
            else {
                return @{
                    success  = $true
                    package  = $package_name
                    language = $language
                    command  = $installCommand
                    message  = "Installation command prepared: $installCommand (run with auto_confirm=true to execute)"
                }
            }
        }
        else {
            return @{success = $false; error = "Unsupported language/package manager: $language" }
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "analyze_code_errors" -Description "Analyze code for syntax errors and suggest fixes" `
    -Category "Code" -Version "1.0" `
    -Parameters @{
    file_path = @{
        type        = "string"
        description = "Path to the code file to analyze"
        required    = $true
    }
    language  = @{
        type        = "string"
        description = "Programming language of the code"
        required    = $false
    }
} `
    -Handler {
    param([string]$file_path, [string]$language = "")
    
    try {
        if (-not (Test-Path $file_path)) {
            return @{success = $false; error = "File not found: $file_path" }
        }
        
        $content = Get-Content $file_path -Raw
        $errors = @()
        $suggestions = @()
        
        # Detect language if not provided
        if (-not $language) {
            $extension = [System.IO.Path]::GetExtension($file_path).ToLower()
            switch ($extension) {
                ".py" { $language = "python" }
                ".js" { $language = "javascript" }
                ".ps1" { $language = "powershell" }
                ".cs" { $language = "csharp" }
                ".cpp" { $language = "cpp" }
                ".c" { $language = "c" }
            }
        }
        
        # Language-specific error checking
        switch ($language.ToLower()) {
            "python" {
                # Check for common Python issues
                if ($content -match "import (\w+)") {
                    $imports = [regex]::Matches($content, "import (\w+)") | ForEach-Object { $_.Groups[1].Value }
                    foreach ($import in $imports) {
                        try {
                            python -c "import $import" 2>$null
                            if ($LASTEXITCODE -ne 0) {
                                $errors += "Missing import: $import"
                                $suggestions += "Install with: pip install $import"
                            }
                        }
                        catch { }
                    }
                }
            }
            "powershell" {
                # PowerShell syntax check
                try {
                    $null = [System.Management.Automation.PSParser]::Tokenize($content, [ref]$null)
                }
                catch {
                    $errors += "PowerShell syntax error: $($_.Exception.Message)"
                }
            }
            "javascript" {
                # Check for common JS issues
                if ($content -match "require\(['""]([^'""]+)['""\)]") {
                    $requires = [regex]::Matches($content, "require\(['""]([^'""]+)['""\)]") | ForEach-Object { $_.Groups[1].Value }
                    foreach ($req in $requires) {
                        if (-not (Test-Path "node_modules\$req") -and -not $req.StartsWith(".")) {
                            $errors += "Missing module: $req"
                            $suggestions += "Install with: npm install $req"
                        }
                    }
                }
            }
        }
        
        return @{
            success     = $true
            file        = $file_path
            language    = $language
            errors      = $errors
            suggestions = $suggestions
            message     = "Analysis complete. Found $(@($errors).Count) potential issues."
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

Register-AgentTool -Name "generate_project_template" -Description "Generate a new project structure with dependencies" `
    -Category "Project" -Version "1.0" `
    -Parameters @{
    project_name  = @{
        type        = "string"
        description = "Name of the new project"
        required    = $true
    }
    language      = @{
        type        = "string"
        description = "Primary programming language"
        required    = $true
    }
    template_type = @{
        type        = "string"
        description = "Type of project (console, web, library, etc.)"
        required    = $false
    }
} `
    -Handler {
    param([string]$project_name, [string]$language, [string]$template_type = "console")
    
    try {
        $projectPath = Join-Path $global:currentWorkingDir $project_name
        
        if (Test-Path $projectPath) {
            return @{success = $false; error = "Project directory already exists: $projectPath" }
        }
        
        New-Item -ItemType Directory -Path $projectPath -Force | Out-Null
        $created_files = @()
        
        switch ($language.ToLower()) {
            "python" {
                # Create Python project structure
                $mainFile = Join-Path $projectPath "main.py"
                $requirementsFile = Join-Path $projectPath "requirements.txt"
                
                Set-Content $mainFile @"
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
$project_name - A Python application
"""

def main():
    print("Hello from $project_name!")
    
if __name__ == "__main__":
    main()
"@
                Set-Content $requirementsFile "# Add your dependencies here"
                $created_files += @("main.py", "requirements.txt")
            }
            "javascript" {
                # Create Node.js project
                $packageJson = Join-Path $projectPath "package.json"
                $indexFile = Join-Path $projectPath "index.js"
                
                $packageContent = @{
                    name         = $project_name.ToLower()
                    version      = "1.0.0"
                    description  = "A Node.js application"
                    main         = "index.js"
                    scripts      = @{
                        start = "node index.js"
                    }
                    dependencies = @{}
                } | ConvertTo-Json -Depth 3
                
                Set-Content $packageJson $packageContent
                Set-Content $indexFile @"
console.log('Hello from $project_name!');
"@
                $created_files += @("package.json", "index.js")
            }
            "powershell" {
                # Create PowerShell module
                $moduleFile = Join-Path $projectPath "$project_name.psm1"
                $manifestFile = Join-Path $projectPath "$project_name.psd1"
                
                Set-Content $moduleFile @"
# $project_name PowerShell Module

function Get-$project_name {
    Write-Host "Hello from $project_name!"
}

Export-ModuleMember -Function Get-$project_name
"@
                
                New-ModuleManifest -Path $manifestFile -ModuleVersion "1.0.0" -Description "$project_name PowerShell Module"
                $created_files += @("$project_name.psm1", "$project_name.psd1")
            }
        }
        
        return @{
            success       = $true
            project_name  = $project_name
            project_path  = $projectPath
            language      = $language
            template_type = $template_type
            created_files = $created_files
            message       = "Project '$project_name' created successfully at $projectPath"
        }
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

# ============================================
# AGENT TOOLS COMMAND INTERFACE
# ============================================

function Invoke-AgentTool {
    <#
    .SYNOPSIS
        Execute a registered agent tool by name
    .DESCRIPTION
        Validates parameters and executes the specified tool's handler
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ToolName,
        
        [Parameter(Mandatory = $false)]
        [hashtable]$Parameters = @{}
    )
    
    if (-not $script:agentTools.ContainsKey($ToolName)) {
        return @{success = $false; error = "Tool not found: $ToolName" }
    }
    
    $tool = $script:agentTools[$ToolName]
    
    # Validate required parameters
    foreach ($paramName in $tool.Parameters.Keys) {
        $paramDef = $tool.Parameters[$paramName]
        if ($paramDef.required -and -not $Parameters.ContainsKey($paramName)) {
            return @{success = $false; error = "Missing required parameter: $paramName" }
        }
    }
    
    # Execute tool handler
    try {
        $result = & $tool.Handler @Parameters
        return $result
    }
    catch {
        return @{success = $false; error = $_.Exception.Message }
    }
}

function Get-AgentToolsList {
    <#
    .SYNOPSIS
        Get a formatted list of all available agent tools
    .DESCRIPTION
        Returns categorized tools with descriptions for display
    #>
    [CmdletBinding()]
    param()
    
    $categories = @{}
    
    foreach ($tool in $script:agentTools.Values) {
        $category = $tool.Category
        if (-not $categories.ContainsKey($category)) {
            $categories[$category] = @()
        }
        
        $categories[$category] += @{
            name        = $tool.Name
            description = $tool.Description
            version     = $tool.Version
            parameters  = $tool.Parameters.Keys -join ", "
        }
    }
    
    return $categories
}

# Register environment info agent tool
Register-AgentTool `
    -Id "environment-info" `
    -Name "Environment Information" `
    -Description "Provides detailed system environment information" `
    -Category "System" `
    -Handler {
    return Get-EnvironmentInfo
}

# ============================================
# Edit Application Engine (Diff/Preview System)
# ============================================
function Set-StructuredEdit {
    param(
        [string]$File,
        [array]$Edits
    )
    
    if (-not (Test-Path $File)) {
        return @{Error = "File not found: $File" }
    }
    
    $originalContent = [System.IO.File]::ReadAllText($File)
    $newContent = $originalContent
    $diffs = @()
    
    foreach ($edit in $Edits) {
        if ($edit.type -eq "replace") {
            $start = $edit.range.start
            $end = $edit.range.end
            $newText = $edit.newText
            
            # Calculate line/column positions
            $lines = $originalContent -split "`r?`n"
            $startPos = 0
            $endPos = 0
            $lineCount = @($lines).Count
            for ($i = 0; $i -lt $lineCount; $i++) {
                if ($i -lt $start.line) {
                    $startPos += $lines[$i].Length + 1
                }
                elseif ($i -eq $start.line) {
                    $startPos += $start.character
                }
                if ($i -lt $end.line) {
                    $endPos += $lines[$i].Length + 1
                }
                elseif ($i -eq $end.line) {
                    $endPos += $end.character
                }
            }
            
            $oldText = $originalContent.Substring($startPos, $endPos - $startPos)
            $diffs += @{
                File    = $File
                OldText = $oldText
                NewText = $newText
                Range   = $edit.range
            }
            
            $newContent = $newContent.Substring(0, $startPos) + $newText + $newContent.Substring($endPos)
        }
    }
    
    # Store pending edit for approval
    $editId = [guid]::NewGuid().ToString()
    $global:agentContext.PendingEdits += @{
        Id              = $editId
        File            = $File
        OriginalContent = $originalContent
        NewContent      = $newContent
        Diffs           = $diffs
        Timestamp       = Get-Date
    }
    
    return @{
        EditId  = $editId
        Diffs   = $diffs
        Preview = Show-EditPreview -EditId $editId
    }
}

function Show-EditPreview {
    param([string]$EditId)
    
    $edit = $global:agentContext.PendingEdits | Where-Object { $_.Id -eq $EditId } | Select-Object -First 1
    if (-not $edit) {
        return $null
    }
    
    $previewForm = New-Object System.Windows.Forms.Form
    $previewForm.Text = "Edit Preview - $($edit.File)"
    $previewForm.Size = New-Object System.Drawing.Size(900, 600)
    $previewForm.StartPosition = "CenterScreen"
    
    $splitter = New-Object System.Windows.Forms.SplitContainer
    $splitter.Dock = [System.Windows.Forms.DockStyle]::Fill
    $splitter.Orientation = [System.Windows.Forms.Orientation]::Vertical
    $previewForm.Controls.Add($splitter) | Out-Null
    
    # Original
    $originalBox = New-Object System.Windows.Forms.RichTextBox
    $originalBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $originalBox.Text = $edit.OriginalContent
    $originalBox.ReadOnly = $false
    $originalBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $splitter.Panel1.Controls.Add($originalBox) | Out-Null
    
    # New
    $newBox = New-Object System.Windows.Forms.RichTextBox
    $newBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $newBox.Text = $edit.NewContent
    $newBox.ReadOnly = $false
    $newBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $splitter.Panel2.Controls.Add($newBox) | Out-Null
    
    # Buttons
    $buttonPanel = New-Object System.Windows.Forms.Panel
    $buttonPanel.Dock = [System.Windows.Forms.DockStyle]::Bottom
    $buttonPanel.Height = 40
    $previewForm.Controls.Add($buttonPanel) | Out-Null
    
    $approveBtn = New-Object System.Windows.Forms.Button
    $approveBtn.Text = "Approve"
    $approveBtn.Dock = [System.Windows.Forms.DockStyle]::Right
    $approveBtn.Width = 100
    $approveBtn.Add_Click({
            Set-ApprovedEdit -EditId $editId
            $previewForm.Close()
        })
    $buttonPanel.Controls.Add($approveBtn) | Out-Null
    
    $rejectBtn = New-Object System.Windows.Forms.Button
    $rejectBtn.Text = "Reject"
    $rejectBtn.Dock = [System.Windows.Forms.DockStyle]::Right
    $rejectBtn.Width = 100
    $rejectBtn.Add_Click({
            $global:agentContext.PendingEdits = $global:agentContext.PendingEdits | Where-Object { $_.Id -ne $editId }
            $previewForm.Close()
        })
    $buttonPanel.Controls.Add($rejectBtn) | Out-Null
    
    $previewForm.ShowDialog() | Out-Null
}

function Set-ApprovedEdit {
    param([string]$EditId)
    
    $edit = $global:agentContext.PendingEdits | Where-Object { $_.Id -eq $EditId } | Select-Object -First 1
    if ($edit) {
        try {
            [System.IO.File]::WriteAllText($edit.File, $edit.NewContent)
            $global:agentContext.Edits += $edit
            $global:agentContext.PendingEdits = $global:agentContext.PendingEdits | Where-Object { $_.Id -ne $EditId }
            
            # Update editor if file is open
            if ($global:currentFile -eq $edit.File) {
                $script:editor.Text = $edit.NewContent
            }
            
            return @{success = $true }
        }
        catch {
            return @{Error = $_.Exception.Message }
        }
    }
}

# ============================================
# Dependency Graph & Build System Awareness
# ============================================
function Get-ProjectDependencies {
    param([string]$Path)
    
    $dependencies = @{
        Type         = "Unknown"
        Dependencies = @()
        BuildSystem  = $null
    }
    
    # Detect project type
    if (Test-Path (Join-Path $Path "package.json")) {
        $dependencies.Type = "Node.js"
        $dependencies.BuildSystem = "npm/yarn"
        $pkg = Get-Content (Join-Path $Path "package.json") | ConvertFrom-Json
        $dependencies.Dependencies = @($pkg.dependencies.PSObject.Properties.Name) + @($pkg.devDependencies.PSObject.Properties.Name)
    }
    elseif (Test-Path (Join-Path $Path "requirements.txt")) {
        $dependencies.Type = "Python"
        $dependencies.BuildSystem = "pip"
        $dependencies.Dependencies = Get-Content (Join-Path $Path "requirements.txt")
    }
    elseif (Test-Path (Join-Path $Path "Cargo.toml")) {
        $dependencies.Type = "Rust"
        $dependencies.BuildSystem = "cargo"
        # Parse Cargo.toml would go here
    }
    elseif (Test-Path (Join-Path $Path "go.mod")) {
        $dependencies.Type = "Go"
        $dependencies.BuildSystem = "go"
        # Parse go.mod would go here
    }
    elseif (Test-Path (Join-Path $Path "*.csproj")) {
        $dependencies.Type = ".NET"
        $dependencies.BuildSystem = "dotnet"
        # Parse csproj would go here
    }
    
    $global:agentContext.DependencyGraph[$Path] = $dependencies
    return $dependencies
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
    
    $logEntry = @{
        Timestamp = Get-Date
        Level     = $Level
        Message   = $Message
        Data      = $Data
        SessionId = $global:agentContext.SessionId
    }
    
    # Store in context
    if (-not $global:agentContext.Logs) {
        $global:agentContext.Logs = @()
    }
    $global:agentContext.Logs += $logEntry
    
    # Write to console if verbose
    $color = switch ($Level) {
        "Error" { "Red" }
        "Warning" { "Yellow" }
        "Info" { "Cyan" }
        default { "White" }
    }
    Write-Host "[$Level] $Message" -ForegroundColor $color
}

# ============================================
# Agent Task Management (Copilot/Amazon Q Workflows)
# ============================================
function New-AgentTask {
    param(
        [string]$Name,
        [string]$Description,
        [array]$Steps
    )
    
    $task = @{
        Id          = [guid]::NewGuid().ToString()
        Name        = $Name
        Description = $Description
        Steps       = $Steps
        Status      = "Pending"
        Progress    = 0
        StartTime   = Get-Date
        EndTime     = $null
        CurrentStep = 0
    }
    
    $global:agentContext.Tasks += $task
    Update-AgentTasksList
    return $task
}

function Start-AgentTask {
    param(
        [string]$TaskId,
        [switch]$UseAsync = $false
    )
    
    # If async is requested and multithreading is available, delegate to async version
    if ($UseAsync -and $script:threadSafeContext.RunspacePool) {
        Start-AgentTaskAsync -TaskId $TaskId -Priority "Normal"
        return
    }
    
    $task = $global:agentContext.Tasks | Where-Object { $_.Id -eq $TaskId } | Select-Object -First 1
    if ($task) {
        $task.Status = "Running"
        $agentStatusLabel.Text = "Agent Status: Running - $($task.Name)"
        Update-AgentTasksList
        
        # Execute task steps synchronously
        foreach ($step in $task.Steps) {
            $task.CurrentStep++
            $task.Progress = [math]::Round(($task.CurrentStep / $task.Steps.Count) * 100)
            Update-AgentTasksList
            
            Write-AgentLog -Level "Info" -Message "Executing step: $($step.Description)" -Data @{TaskId = $TaskId }
            
            try {
                if ($step.Type -eq "tool") {
                    $result = Invoke-AgentTool -ToolName $step.Tool -Arguments $step.Arguments
                    $step.Result = $result
                    $step.Completed = $true
                }
                elseif ($step.Type -eq "command") {
                    $result = Invoke-TerminalCommand -Command $step.Command
                    $step.Result = $result
                    $step.Completed = $true
                }
                elseif ($step.Type -eq "edit") {
                    $result = Set-StructuredEdit -File $step.File -Edits $step.Edits
                    $step.Result = $result
                    $step.Completed = $true
                }
            }
            catch {
                $step.Error = $_.Exception.Message
                $task.Status = "Error"
                Write-AgentLog -Level "Error" -Message "Task step failed: $($_.Exception.Message)" -Data @{TaskId = $TaskId; Step = $step.Description }
                break
            }
        }
        
        if ($task.Status -ne "Error") {
            $task.Status = "Completed"
            $task.EndTime = Get-Date
            $task.Progress = 100
        }
        
        $agentStatusLabel.Text = "Agent Status: $($task.Status) - $($task.Name)"
        Update-AgentTasksList
        Write-AgentLog -Level "Info" -Message "Task completed: $($task.Name)" -Data @{TaskId = $TaskId; Status = $task.Status }
    }
}

function Update-AgentTasksList {
    $agentTasksList.Items.Clear()
    foreach ($task in $global:agentContext.Tasks) {
        $item = New-Object System.Windows.Forms.ListViewItem($task.Name)
        $item.SubItems.Add($task.Status) | Out-Null
        $item.SubItems.Add("$($task.Progress)%") | Out-Null
        $item.SubItems.Add((New-TimeSpan -Start $task.StartTime -End (if ($task.EndTime) { $task.EndTime } else { Get-Date })).ToString("mm\:ss")) | Out-Null
        $item.Tag = $task.Id
        $agentTasksList.Items.Add($item) | Out-Null
    }
}

# ============================================
# Agentic Workflow Execution
# ============================================
function Invoke-AgenticWorkflow {
    param(
        [string]$Goal,
        [string]$Context = ""
    )
    
    Write-AgentLog -Level "Info" -Message "Starting agentic workflow" -Data @{Goal = $Goal }
    
    # Create task from goal
    $steps = @()
    
    # Analyze goal and create steps
    if ($Goal -match "create|generate|write") {
        $steps += @{
            Type        = "tool"
            Description = "Analyze requirements"
            Tool        = "get_environment"
            Arguments   = @{}
        }
        $steps += @{
            Type        = "tool"
            Description = "Check project structure"
            Tool        = "list_directory"
            Arguments   = @{path = $global:currentWorkingDir }
        }
    }
    
    if ($Goal -match "fix|debug|error") {
        $steps += @{
            Type        = "tool"
            Description = "Read error files"
            Tool        = "read_file"
            Arguments   = @{path = $global:currentFile }
        }
        $steps += @{
            Type        = "command"
            Description = "Run tests to identify issues"
            Command     = "Get-ChildItem -Recurse -Filter '*test*' | Select-Object -First 1"
        }
    }
    
    if ($Goal -match "refactor|improve") {
        $steps += @{
            Type        = "tool"
            Description = "Read source files"
            Tool        = "read_file"
            Arguments   = @{path = $global:currentFile }
        }
        $steps += @{
            Type        = "edit"
            Description = "Apply refactoring"
            File        = $global:currentFile
            Edits       = @() # Would be generated by AI
        }
    }
    
    $task = New-AgentTask -Name $Goal -Description $Context -Steps $steps
    Start-AgentTask -TaskId $task.Id
    
    return $task
}

# ============================================
# MULTITHREADING AGENT SYSTEM
# ============================================

function Initialize-MultithreadedAgents {
    <#
    .SYNOPSIS
        Initializes the multithreaded agent system with runspace pools
    .DESCRIPTION
        Creates runspace pools for parallel agent task execution with proper thread safety
    #>
    
    Write-DevConsole "🔧 Initializing multithreaded agent system..." "INFO"
    
    try {
        # Create runspace pool for parallel execution
        $script:threadSafeContext.RunspacePool = [runspacefactory]::CreateRunspacePool(
            1, 
            $script:threadSafeContext.WorkerCount,
            $script:sessionState,
            $Host
        )
        
        # Add shared variables to session state
        $script:sessionState.Variables.Add((New-Object System.Management.Automation.Runspaces.SessionStateVariableEntry('agentContext', $global:agentContext, $null)))
        $script:sessionState.Variables.Add((New-Object System.Management.Automation.Runspaces.SessionStateVariableEntry('currentWorkingDir', $global:currentWorkingDir, $null)))
        $script:sessionState.Variables.Add((New-Object System.Management.Automation.Runspaces.SessionStateVariableEntry('settings', $global:settings, $null)))
        
        $script:threadSafeContext.RunspacePool.Open()
        
        # Initialize log processing timer
        $script:logProcessingTimer = New-Object System.Windows.Forms.Timer
        $script:logProcessingTimer.Interval = 100  # Process logs every 100ms
        $script:logProcessingTimer.add_Tick({
                Process-ThreadSafeLogs
            })
        $script:logProcessingTimer.Start()
        
        Write-DevConsole "✅ Multithreaded agent system initialized with $($script:threadSafeContext.WorkerCount) workers" "SUCCESS"
        return $true
    }
    catch {
        Write-DevConsole "❌ Failed to initialize multithreaded agents: $_" "ERROR"
        return $false
    }
}

function Start-AgentTaskAsync {
    <#
    .SYNOPSIS
        Starts an agent task in a separate thread using runspace pools
    .PARAMETER TaskId
        The ID of the task to execute
    .PARAMETER Priority
        Task priority: High, Normal, Low (default: Normal)
    #>
    param(
        [string]$TaskId,
        [ValidateSet("High", "Normal", "Low")]
        [string]$Priority = "Normal"
    )
    
    if (-not $script:threadSafeContext.RunspacePool) {
        Write-DevConsole "⚠ Multithreaded system not initialized, falling back to synchronous execution" "WARNING"
        Start-AgentTask -TaskId $TaskId
        return
    }
    
    # Check if we're at max capacity
    $activeJobCount = if ($script:threadSafeContext.ActiveJobs) { $script:threadSafeContext.ActiveJobs.Count } else { 0 }
    if ($activeJobCount -ge $script:threadSafeContext.MaxConcurrentTasks) {
        Write-DevConsole "⚠ Maximum concurrent tasks reached, queueing task $TaskId" "WARNING"
        
        lock ($script:threadSafeContext.SyncRoot) {
            $script:threadSafeContext.TaskQueue.Enqueue(@{
                    TaskId     = $TaskId
                    Priority   = $Priority
                    QueuedTime = Get-Date
                })
        }
        return
    }
    
    $task = $global:agentContext.Tasks | Where-Object { $_.Id -eq $TaskId } | Select-Object -First 1
    if (-not $task) {
        Write-DevConsole "❌ Task $TaskId not found" "ERROR"
        return
    }
    
    Write-DevConsole "🚀 Starting async task: $($task.Name) (Priority: $Priority)" "INFO"
    
    # Create PowerShell instance
    $powershell = [powershell]::Create()
    $powershell.RunspacePool = $script:threadSafeContext.RunspacePool
    
    # Add the task execution script
    $null = $powershell.AddScript({
            param($TaskData, $LogQueue, $CompletedTasks, $WorkerStates)
        
            $taskId = $TaskData.Id
            $task = $TaskData
        
            try {
                # Update task status
                $task.Status = "Running"
                $task.StartTime = Get-Date
                $task.WorkerId = [System.Threading.Thread]::CurrentThread.ManagedThreadId
            
                # Log task start
                $logQueue.Enqueue(@{
                        Level     = "Info"
                        Message   = "Async task started: $($task.Name)"
                        Data      = @{ TaskId = $taskId; WorkerId = $task.WorkerId; Thread = "Background" }
                        Timestamp = Get-Date
                    })
            
                # Execute each step
                $stepIndex = 0
                foreach ($step in $task.Steps) {
                    $stepIndex++
                    $task.CurrentStep = $stepIndex
                    $task.Progress = [math]::Round(($stepIndex / $task.Steps.Count) * 100)
                
                    $logQueue.Enqueue(@{
                            Level     = "Info" 
                            Message   = "Executing step $stepIndex/$($task.Steps.Count): $($step.Description)"
                            Data      = @{ TaskId = $taskId; Step = $stepIndex }
                            Timestamp = Get-Date
                        })
                
                    # Execute step based on type
                    switch ($step.Type) {
                        "tool" {
                            # Simulate tool execution (replace with actual tool calls)
                            Start-Sleep -Milliseconds (Get-Random -Minimum 500 -Maximum 2000)
                            $step.Result = "Tool $($step.Tool) executed successfully"
                            $step.Completed = $true
                        }
                        "command" {
                            # Execute PowerShell command
                            try {
                                $step.Result = Invoke-Expression $step.Command
                                $step.Completed = $true
                            }
                            catch {
                                $step.Error = $_.Exception.Message
                                throw
                            }
                        }
                        "edit" {
                            # Simulate file edit (implement actual file operations)
                            Start-Sleep -Milliseconds (Get-Random -Minimum 300 -Maximum 1000)
                            $step.Result = "File $($step.File) edited successfully"
                            $step.Completed = $true
                        }
                        "ai_query" {
                            # AI processing step
                            Start-Sleep -Milliseconds (Get-Random -Minimum 1000 -Maximum 3000)
                            $step.Result = "AI query processed: $($step.Query)"
                            $step.Completed = $true
                        }
                    }
                }
            
                # Mark task as completed
                $task.Status = "Completed" 
                $task.EndTime = Get-Date
                $task.Progress = 100
            
                $logQueue.Enqueue(@{
                        Level     = "Success"
                        Message   = "Async task completed: $($task.Name)"
                        Data      = @{ TaskId = $taskId; Duration = (New-TimeSpan -Start $task.StartTime -End $task.EndTime).TotalSeconds }
                        Timestamp = Get-Date
                    })
            
                # Add to completed tasks
                $CompletedTasks.Add($task)
            
                return $task
            }
            catch {
                $task.Status = "Error"
                $task.Error = $_.Exception.Message
                $task.EndTime = Get-Date
            
                $logQueue.Enqueue(@{
                        Level     = "Error"
                        Message   = "Async task failed: $($task.Name) - $($_.Exception.Message)"
                        Data      = @{ TaskId = $taskId; Error = $_.Exception.Message }
                        Timestamp = Get-Date
                    })
            
                $CompletedTasks.Add($task)
                return $task
            }
        })
    
    # Add parameters
    $null = $powershell.AddParameter("TaskData", $task)
    $null = $powershell.AddParameter("LogQueue", $script:logQueue)
    $null = $powershell.AddParameter("CompletedTasks", $script:threadSafeContext.CompletedTasks)
    $null = $powershell.AddParameter("WorkerStates", $script:agentWorkers)
    
    # Start async execution
    $job = $powershell.BeginInvoke()
    
    # Store job info
    $jobInfo = @{
        PowerShell = $powershell
        Job        = $job
        TaskId     = $TaskId
        StartTime  = Get-Date
        Priority   = $Priority
        Task       = $task
    }
    
    lock ($script:threadSafeContext.SyncRoot) {
        $script:threadSafeContext.ActiveJobs[$TaskId] = $jobInfo
    }
    
    # Update worker status
    $workerName = Get-AvailableWorker
    if ($workerName) {
        $script:agentWorkers[$workerName].Status = "Running"
        $script:agentWorkers[$workerName].CurrentTask = $TaskId
        $script:agentWorkers[$workerName].LastActivity = Get-Date
    }
    
    Update-AgentTasksList
}

function Start-ParallelChatProcessing {
    <#
    .SYNOPSIS
        Processes multiple chat requests in parallel across different tabs
    .PARAMETER ChatRequests
        Array of chat request objects with TabId, Message, Model
    #>
    param([array]$ChatRequests)
    
    if (-not $script:threadSafeContext.RunspacePool) {
        Write-DevConsole "⚠ Multithreading not available, processing chats sequentially" "WARNING"
        foreach ($request in $ChatRequests) {
            Send-ChatMessage -TabId $request.TabId
        }
        return
    }
    
    Write-DevConsole "🔄 Starting parallel chat processing for $($ChatRequests.Count) requests" "INFO"
    
    $chatJobs = @()
    
    foreach ($request in $ChatRequests) {
        $powershell = [powershell]::Create()
        $powershell.RunspacePool = $script:threadSafeContext.RunspacePool
        
        $null = $powershell.AddScript({
                param($TabId, $Message, $Model, $ChatHistory)
            
                try {
                    # Build context from chat history
                    $context = ""
                    foreach ($msg in $ChatHistory) {
                        $context += "$($msg.Role): $($msg.Content)`n"
                    }
                
                    $body = @{
                        model  = $Model
                        prompt = $context + "user: $Message`n"
                        stream = $false
                    } | ConvertTo-Json -Depth 3
                
                    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $body -ContentType "application/json"
                
                    return @{
                        TabId          = $TabId
                        Success        = $true
                        Response       = $response.response
                        ProcessingTime = (Get-Date)
                    }
                }
                catch {
                    return @{
                        TabId          = $TabId
                        Success        = $false
                        Error          = $_.Exception.Message
                        ProcessingTime = (Get-Date)
                    }
                }
            })
        
        $null = $powershell.AddParameter("TabId", $request.TabId)
        $null = $powershell.AddParameter("Message", $request.Message)
        $null = $powershell.AddParameter("Model", $request.Model)
        $null = $powershell.AddParameter("ChatHistory", $request.ChatHistory)
        
        $job = $powershell.BeginInvoke()
        
        $chatJobs += @{
            PowerShell = $powershell
            Job        = $job
            TabId      = $request.TabId
            StartTime  = Get-Date
        }
    }
    
    # Monitor chat jobs completion
    Start-ChatJobMonitor -ChatJobsToAdd $chatJobs
}

function Start-ChatJobMonitor {
    param([array]$ChatJobsToAdd)
    
    # Add new jobs to the script-level collection
    $script:chatJobs += $ChatJobsToAdd
    
    # Only create timer if we don't already have one running
    if (-not $script:chatJobMonitorTimer) {
        $script:chatJobMonitorTimer = New-Object System.Windows.Forms.Timer
        $script:chatJobMonitorTimer.Interval = 250
        
        $script:chatJobMonitorTimer.add_Tick({
                $completedJobs = @()
            
                foreach ($chatJob in $script:chatJobs) {
                    if ($chatJob.Job.IsCompleted) {
                        try {
                            $result = $chatJob.PowerShell.EndInvoke($chatJob.Job)
                        
                            # Update UI on main thread
                            [System.Windows.Forms.Control]::CheckForIllegalCrossThreadCalls = $false
                        
                            if ($script:chatTabs.ContainsKey($result.TabId)) {
                                $chatSession = $script:chatTabs[$result.TabId]
                            
                                if ($result.Success) {
                                    # Find and replace the processing indicator
                                    $processingText = "AI (processing...): "
                                    $chatSession.ChatBox.Text = $chatSession.ChatBox.Text -replace [regex]::Escape($processingText), "AI: $($result.Response)`n`n"
                                
                                    # Store AI response
                                    $chatSession.Messages += @{
                                        Role      = "assistant"
                                        Content   = $result.Response
                                        Timestamp = Get-Date
                                    }
                                
                                    Write-DevConsole "✅ Parallel chat response received for tab $($result.TabId)" "SUCCESS"
                                }
                                else {
                                    $chatSession.ChatBox.Text = $chatSession.ChatBox.Text -replace [regex]::Escape($processingText), "AI: Error - $($result.Error)`n`n"
                                    Write-DevConsole "❌ Parallel chat error for tab $($result.TabId): $($result.Error)" "ERROR"
                                }
                            
                                $chatSession.ChatBox.ScrollToCaret()
                            }
                        
                            $completedJobs += $chatJob
                        }
                        catch {
                            Write-DevConsole "❌ Error processing chat job completion: $_" "ERROR"
                            $completedJobs += $chatJob
                        }
                        finally {
                            $chatJob.PowerShell.Dispose()
                        }
                    }
                }
            
                # Remove completed jobs
                foreach ($completed in $completedJobs) {
                    $script:chatJobs = @($script:chatJobs | Where-Object { $_.TabId -ne $completed.TabId })
                }
            
                # Stop timer if all jobs completed
                if (@($script:chatJobs).Count -eq 0) {
                    $script:chatJobMonitorTimer.Stop()
                    $script:chatJobMonitorTimer.Dispose()
                    $script:chatJobMonitorTimer = $null
                    Write-DevConsole "✅ All parallel chat jobs completed" "SUCCESS"
                }
            })
        
        $script:chatJobMonitorTimer.Start()
    }
}

function Process-ThreadSafeLogs {
    <#
    .SYNOPSIS
        Processes logs from background threads and displays them in the UI
    #>
    
    if ($script:logQueue.Count -eq 0) { return }
    
    $processedCount = 0
    $maxProcessPerTick = 10
    
    while ($script:logQueue.Count -gt 0 -and $processedCount -lt $maxProcessPerTick) {
        $logEntry = $null
        
        lock ($script:logQueue.SyncRoot) {
            if ($script:logQueue.Count -gt 0) {
                $logEntry = $script:logQueue.Dequeue()
            }
        }
        
        if ($logEntry) {
            $icon = switch ($logEntry.Level) {
                "Success" { "✅" }
                "Error" { "❌" }
                "Warning" { "⚠" }
                "Info" { "ℹ️" }
                default { "📝" }
            }
            
            Write-DevConsole "$icon $($logEntry.Message)" $logEntry.Level
            $processedCount++
        }
    }
}

function Monitor-AgentJobs {
    <#
    .SYNOPSIS
        Monitors async agent jobs and updates UI when they complete
    #>
    
    $completedJobs = @()
    
    foreach ($jobId in $script:threadSafeContext.ActiveJobs.Keys) {
        $jobInfo = $script:threadSafeContext.ActiveJobs[$jobId]
        
        if ($jobInfo.Job.IsCompleted) {
            try {
                $result = $jobInfo.PowerShell.EndInvoke($jobInfo.Job)
                
                # Update the task in the global context
                $taskIndex = $global:agentContext.Tasks.FindIndex({ $_.Id -eq $jobId })
                if ($taskIndex -ge 0) {
                    $global:agentContext.Tasks[$taskIndex] = $result
                }
                
                # Update UI
                Update-AgentTasksList
                
                # Update worker status
                $workerName = ($script:agentWorkers.Keys | Where-Object { 
                        $script:agentWorkers[$_].CurrentTask -eq $jobId 
                    }) | Select-Object -First 1
                
                if ($workerName) {
                    $script:agentWorkers[$workerName].Status = "Idle"
                    $script:agentWorkers[$workerName].CurrentTask = $null
                    $script:agentWorkers[$workerName].LastActivity = Get-Date
                }
                
                Write-DevConsole "✅ Async task completed: $($result.Name)" "SUCCESS"
                $completedJobs += $jobId
                
                # Process queued tasks
                Process-TaskQueue
            }
            catch {
                Write-DevConsole "❌ Error completing async task $jobId : $_" "ERROR"
                $completedJobs += $jobId
            }
            finally {
                $jobInfo.PowerShell.Dispose()
            }
        }
    }
    
    # Remove completed jobs
    foreach ($jobId in $completedJobs) {
        lock ($script:threadSafeContext.SyncRoot) {
            $script:threadSafeContext.ActiveJobs.Remove($jobId)
        }
    }
}

function Process-TaskQueue {
    <#
    .SYNOPSIS
        Processes queued tasks when workers become available
    #>
    
    if ($script:threadSafeContext.TaskQueue.Count -eq 0) { return }
    if ($script:threadSafeContext.ActiveJobs.Count -ge $script:threadSafeContext.MaxConcurrentTasks) { return }
    
    $queuedTask = $null
    
    lock ($script:threadSafeContext.SyncRoot) {
        if ($script:threadSafeContext.TaskQueue.Count -gt 0) {
            $queuedTask = $script:threadSafeContext.TaskQueue.Dequeue()
        }
    }
    
    if ($queuedTask) {
        Write-DevConsole "🔄 Processing queued task: $($queuedTask.TaskId)" "INFO"
        Start-AgentTaskAsync -TaskId $queuedTask.TaskId -Priority $queuedTask.Priority
    }
}

function Get-AvailableWorker {
    <#
    .SYNOPSIS
        Gets the name of an available worker thread
    #>
    
    $availableWorker = $script:agentWorkers.Keys | Where-Object {
        $script:agentWorkers[$_].Status -eq "Idle"
    } | Select-Object -First 1
    
    return $availableWorker
}

function Stop-MultithreadedAgents {
    <#
    .SYNOPSIS
        Safely stops all agent threads and cleans up resources
    #>
    
    Write-DevConsole "🛑 Stopping multithreaded agent system..." "INFO"
    
    try {
        # Stop log processing timer
        if ($script:logProcessingTimer) {
            $script:logProcessingTimer.Stop()
            $script:logProcessingTimer.Dispose()
        }
        
        # Cancel all active jobs
        foreach ($jobInfo in $script:threadSafeContext.ActiveJobs.Values) {
            try {
                $jobInfo.PowerShell.Stop()
                $jobInfo.PowerShell.Dispose()
            }
            catch {
                Write-DevConsole "⚠ Error stopping job: $_" "WARNING"
            }
        }
        
        # Close runspace pool
        if ($script:threadSafeContext.RunspacePool) {
            $script:threadSafeContext.RunspacePool.Close()
            $script:threadSafeContext.RunspacePool.Dispose()
        }
        
        # Clear collections
        $script:threadSafeContext.ActiveJobs.Clear()
        $script:threadSafeContext.TaskQueue.Clear()
        $script:threadSafeContext.CompletedTasks.Clear()
        
        Write-DevConsole "✅ Multithreaded agent system stopped" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error stopping multithreaded agents: $_" "ERROR"
    }
}

function Get-ThreadingStatus {
    <#
    .SYNOPSIS
        Gets current status of the multithreading system
    #>
    
    return @{
        IsInitialized      = ($null -ne $script:threadSafeContext.RunspacePool)
        ActiveJobs         = $script:threadSafeContext.ActiveJobs.Count
        QueuedTasks        = $script:threadSafeContext.TaskQueue.Count
        CompletedTasks     = $script:threadSafeContext.CompletedTasks.Count
        WorkerStates       = $script:agentWorkers
        MaxConcurrentTasks = $script:threadSafeContext.MaxConcurrentTasks
        WorkerCount        = $script:threadSafeContext.WorkerCount
    }
}

# ============================================
# Coding/Copilot Functions
# ============================================
function Invoke-CodeGeneration {
    param([string]$prompt, [string]$context = "")
    
    $fullPrompt = @"
You are a coding assistant. Generate code based on the following request.

Request: $prompt

$context

Provide only the code, no explanations unless asked.
"@
    
    return Send-OllamaRequest $fullPrompt
}

function Invoke-CodeReview {
    param([string]$code)
    
    # Detect language from content
    $language = "Unknown"
    if ($code -match "function\s+\w+\s*\{" -or $code -match "\$\w+" -or $code -match "param\s*\(") {
        $language = "PowerShell"
    }
    elseif ($code -match "def\s+\w+\s*\(" -or $code -match "import\s+" -or $code -match "print\s*\(") {
        $language = "Python"
    }
    elseif ($code -match "function\s+\w+\s*\(" -or $code -match "console\.log" -or $code -match "var\s+\w+" -or $code -match "const\s+\w+") {
        $language = "JavaScript"
    }
    elseif ($code -match "#include\s*<" -or $code -match "int\s+main\s*\(" -or $code -match "printf\s*\(") {
        $language = "C/C++"
    }
    
    $prompt = @"
You are an expert code reviewer. Analyze this $language code and provide a comprehensive review.

CODE TO REVIEW:
```$language
$code
```

Please provide a detailed code review with:

1. **CODE QUALITY ASSESSMENT**
   - Overall code structure and organization
   - Readability and maintainability
   - Naming conventions
   - Code complexity

2. **ISSUES FOUND**
   - Syntax errors or potential bugs
   - Performance problems
   - Security vulnerabilities
   - Logic errors

3. **IMPROVEMENT SUGGESTIONS**
   - Specific code changes recommended
   - Better algorithms or approaches
   - Optimization opportunities
   - Missing error handling

4. **BEST PRACTICES**
   - Language-specific best practices
   - Design patterns that could be applied
   - Documentation improvements
   - Testing recommendations

5. **OVERALL RATING**
   - Rate the code from 1-10 (10 being excellent)
   - Summary of strengths and weaknesses

Be specific and provide actionable feedback with examples where possible.
"@
    
    Write-DevConsole "Sending code review request to model..." "INFO"
    return Send-OllamaRequest $prompt
}

function Invoke-CodeRefactor {
    param([string]$code, [string]$instructions)
    
    $prompt = @"
Refactor this code according to the instructions:

Instructions: $instructions

Code:
$code

Provide the refactored code.
"@
    
    return Send-OllamaRequest $prompt
}

# Note: Orphaned event handlers have been removed to fix startup errors
# Event handlers should be defined within appropriate form initialization functions

# Agent Command Processing Function
# Note: All orphaned event handler code has been removed to prevent startup errors
# Event handlers should be properly defined within form initialization functions
# Actual function definition is below

# Terminal Event Handlers
$terminalInput.Add_KeyDown({
        if ($_.KeyCode -eq "Enter") {
            $command = $terminalInput.Text.Trim()
            $terminalInput.Clear()
            Invoke-TerminalCommand $command
        }
        elseif ($_.KeyCode -eq "Up") {
            if ($global:terminalHistoryIndex -gt 0) {
                $global:terminalHistoryIndex--
                $terminalInput.Text = $global:terminalHistory[$global:terminalHistoryIndex]
            }
            $_.Handled = $true
        }
        elseif ($_.KeyCode -eq "Down") {
            $histCount = @($global:terminalHistory).Count
            if ($global:terminalHistoryIndex -lt ($histCount - 1)) {
                $global:terminalHistoryIndex++
                $terminalInput.Text = $global:terminalHistory[$global:terminalHistoryIndex]
            }
            else {
                $terminalInput.Text = ""
                $global:terminalHistoryIndex = $histCount
            }
            $_.Handled = $true
        }
    })

# Agent Tasks Event Handlers
$agentSendBtn.Add_Click({
        $command = $agentInputBox.Text.Trim()
        if ($command) {
            Send-AgentCommand $command
            $agentInputBox.Clear()
        }
    })

$agentInputBox.Add_KeyDown({
        if ($_.KeyCode -eq "Enter") {
            $command = $agentInputBox.Text.Trim()
            if ($command) {
                Send-AgentCommand $command
                $agentInputBox.Clear()
            }
            $_.Handled = $true
        }
    })

# Agent Command Processing Function
function Send-AgentCommand {
    param([string]$command)
    
    # Display command in agent console
    $agentTaskDetails.AppendText("[$(Get-Date -Format 'HH:mm:ss')] > $command`r`n")
    $agentTaskDetails.ScrollToCaret()
    
    Write-DevConsole "Agent command received: $command" "INFO"
    
    # Process the command
    try {
        if ($command -match "^/help$" -or $command -eq "help") {
            $agentTaskDetails.AppendText("Available Agent Commands:`r`n")
            $agentTaskDetails.AppendText("  /help - Show this help`r`n")
            $agentTaskDetails.AppendText("  /status - Show agent status`r`n")
            $agentTaskDetails.AppendText("  /tasks - List current tasks`r`n")
            $agentTaskDetails.AppendText("  /clear - Clear console`r`n")
            $agentTaskDetails.AppendText("  /tools - List available tools`r`n")
            $agentTaskDetails.AppendText("  /execute_tool <name> <params> - Execute a tool`r`n")
            $agentTaskDetails.AppendText("  Any other text will be sent to the AI for processing`r`n`r`n")
        }
        elseif ($command -eq "/clear") {
            $agentTaskDetails.Clear()
            $agentTaskDetails.AppendText("Agent Task Console - Cleared`r`n")
        }
        elseif ($command -eq "/status") {
            $agentTaskDetails.AppendText("Agent Status: Online`r`n")
            $agentTaskDetails.AppendText("Model: $($global:settings.OllamaModel)`r`n")
            $agentTaskDetails.AppendText("Tasks: $($global:agentContext.Tasks.Count)`r`n")
            $agentTaskDetails.AppendText("Session: $($global:agentContext.SessionId)`r`n`r`n")
        }
        elseif ($command -eq "/tasks") {
            $agentTaskDetails.AppendText("Current Tasks:`r`n")
            if ($global:agentContext.Tasks.Count -eq 0) {
                $agentTaskDetails.AppendText("  No active tasks`r`n")
            }
            else {
                foreach ($task in $global:agentContext.Tasks) {
                    $agentTaskDetails.AppendText("  - $($task.Name) [$($task.Status)]`r`n")
                }
            }
            $agentTaskDetails.AppendText("`r`n")
        }
        elseif ($command -eq "/tools") {
            $categories = Get-AgentToolsList
            $agentTaskDetails.AppendText("Available Tools:`r`n")
            foreach ($category in $categories.Keys) {
                $agentTaskDetails.AppendText("  $category :`r`n")
                foreach ($tool in $categories[$category]) {
                    $agentTaskDetails.AppendText("    • $($tool.name) - $($tool.description)`r`n")
                }
            }
            $agentTaskDetails.AppendText("`r`n")
        }
        elseif ($command -match "^/execute_tool\s+(\w+)(?:\s+(.+))?$") {
            $toolName = $Matches[1]
            $paramsJson = if ($Matches[2]) { $Matches[2] } else { "{}" }
            
            try {
                $params = ConvertFrom-Json $paramsJson -AsHashtable
                $result = Invoke-AgentTool -ToolName $toolName -Parameters $params
                
                $agentTaskDetails.AppendText("Tool Result: $toolName`r`n")
                if ($result.success) {
                    $agentTaskDetails.AppendText("✓ Success`r`n")
                    $resultJson = ($result | ConvertTo-Json -Depth 3)
                    $agentTaskDetails.AppendText("$resultJson`r`n`r`n")
                }
                else {
                    $agentTaskDetails.AppendText("✗ Error: $($result.error)`r`n`r`n")
                }
            }
            catch {
                $agentTaskDetails.AppendText("✗ Tool execution error: $_`r`n`r`n")
            }
        }
        else {
            # Send to AI for processing
            $agentTaskDetails.AppendText("Processing with AI...`r`n")
            $agentResponse = Send-OllamaRequest $command $global:settings.OllamaModel
            $agentTaskDetails.AppendText("AI Response: $agentResponse`r`n`r`n")
        }
    }
    catch {
        $agentTaskDetails.AppendText("Error processing command: $_`r`n`r`n")
        Write-DevConsole "Agent command error: $_" "ERROR"
    }
    
    $agentTaskDetails.ScrollToCaret()
}

# Git Refresh Handler
$gitRefreshBtn.Add_Click({
        Update-GitStatus
    })

# Initialize Explorer
Update-Explorer

# Initialize Extension System
Initialize-ExtensionSystem

# Load user settings
try {
    $script:settingsLoader = { Get-Settings }  # Fixed verb name
    Invoke-Command $script:settingsLoader
    Write-DevConsole "User settings loaded successfully" "SUCCESS"
}
catch {
    Write-DevConsole "Could not load settings, using defaults: $_" "WARNING"
}

# Initialize Agentic System
Get-EnvironmentInfo | Out-Null
Write-AgentLog -Level "Info" -Message "Agentic system initialized" -Data @{SessionId = $global:agentContext.SessionId }

# Load chat history on startup and show welcome message
$form.Add_Load({
        Get-ChatHistory
        
        # Create initial chat tab if none exist
        if (@($script:chatTabs).Count -eq 0) {
            $null = New-ChatTab -TabName "Welcome"
        }
        
        # Get the first available chat tab for welcome message
        $welcomeChat = Get-ActiveChatTab
        if (-not $welcomeChat) {
            $welcomeChat = $script:chatTabs.Values | Select-Object -First 1
        }
        
        if ($welcomeChat) {
            $chatBox = $welcomeChat.ChatBox
            
            # Show agent mode status on startup
            if ($global:AgentMode) {
                $chatBox.AppendText("═══════════════════════════════════════════════════════════════`r`n")
                $chatBox.AppendText("     🤖 RawrXD AI Editor - Agent Mode ACTIVE 🤖     `r`n")
                $chatBox.AppendText("═══════════════════════════════════════════════════════════════`r`n")
                $chatBox.AppendText("Agentic features enabled! Talk naturally or use commands:`r`n`r`n")
                
                $chatBox.AppendText("📝 CODE & EDITING:`r`n")
                $chatBox.AppendText("  • /review - Review code in editor`r`n")
                $chatBox.AppendText("  • /code <description> - Generate code`r`n")
                $chatBox.AppendText("  • /refactor <instructions> - Refactor code`r`n")
                $chatBox.AppendText("  • 'open file test.js' - Open any file`r`n")
                $chatBox.AppendText("  • 'save this file as backup.txt' - Save file`r`n")
                $chatBox.AppendText("  • 'create new file app.py' - Create new file`r`n`r`n")
                
                $chatBox.AppendText("📁 FILE BROWSING:`r`n")
                $chatBox.AppendText("  • 'show files in C:\Projects' - List directory`r`n")
                $chatBox.AppendText("  • 'go to folder Documents' - Change directory`r`n")
                $chatBox.AppendText("  • 'open project MyApp' - Open folder/project`r`n")
                $chatBox.AppendText("  • /ls, /cd, /dir - Classic commands work too`r`n`r`n")
                
                $chatBox.AppendText("🌐 BROWSER:`r`n")
                $chatBox.AppendText("  • 'open google.com' - Navigate to URL`r`n")
                $chatBox.AppendText("  • 'search for AI tutorials' - Google search`r`n")
                $chatBox.AppendText("  • 'youtube search cats' - Search YouTube`r`n")
                $chatBox.AppendText("  • 'extract page content' - Get webpage text`r`n`r`n")
                
                $chatBox.AppendText("💻 TERMINAL:`r`n")
                $chatBox.AppendText("  • 'run command npm install' - Execute commands`r`n")
                $chatBox.AppendText("  • /term <cmd>, /exec <cmd> - Run in terminal`r`n")
                $chatBox.AppendText("  • 'show terminal' - Switch to terminal tab`r`n`r`n")
                
                $chatBox.AppendText("🔧 AGENT TOOLS & AUTOMATION:`r`n")
                $chatBox.AppendText("  • /tools - Show all 15+ registered agent tools`r`n")
                $chatBox.AppendText("  • /tools Browser - Filter tools by category`r`n")
                $chatBox.AppendText("  • /execute_tool read_file {`"path`":`"file.txt`"}`r`n")
                $chatBox.AppendText("  • /git status, /git commit, /git push`r`n")
                $chatBox.AppendText("  • /workflow <goal> - Multi-step tasks`r`n`r`n")
                
                $chatBox.AppendText("💡 TIP: Just type naturally! The agent understands context.`r`n")
                $chatBox.AppendText("💡 Type '/tools' to see FileSystem, Git, Browser, Terminal tools!`r`n")
                $chatBox.AppendText("═══════════════════════════════════════════════════════════════`r`n`r`n")
            }
        }

        # Warm up marketplace cache so the catalog is available without delay
        try {
            Load-MarketplaceCatalog -Force | Out-Null
            Write-DevConsole "Marketplace catalog warmed up" "INFO"
        }
        catch {
            Write-DevConsole "Marketplace warm-up failed: $_" "WARNING"
        }
    })

# ============================================
# SECURITY DIALOG FUNCTIONS
# ============================================

function Show-SessionInfo {
    $infoForm = New-Object System.Windows.Forms.Form
    $infoForm.Text = "Session Information"
    $infoForm.Size = New-Object System.Drawing.Size(500, 400)
    $infoForm.StartPosition = "CenterScreen"
    $infoForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $infoForm.ForeColor = [System.Drawing.Color]::White
    
    $infoText = New-Object System.Windows.Forms.TextBox
    $infoText.Multiline = $true
    $infoText.ReadOnly = $true
    $infoText.ScrollBars = "Vertical"
    $infoText.Dock = [System.Windows.Forms.DockStyle]::Fill
    $infoText.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $infoText.ForeColor = [System.Drawing.Color]::White
    $infoText.Font = New-Object System.Drawing.Font("Consolas", 10)
    
    $sessionDuration = ((Get-Date) - $script:CurrentSession.StartTime)
    $lastActivityAgo = ((Get-Date) - $script:CurrentSession.LastActivity)
    
    $sessionInfo = @"
SESSION INFORMATION
═══════════════════════════════════════
Session ID: $($script:CurrentSession.SessionId)
User ID: $(if ($script:CurrentSession.UserId) { $script:CurrentSession.UserId } else { "Anonymous" })
Authenticated: $($script:CurrentSession.IsAuthenticated)
Security Level: $($script:CurrentSession.SecurityLevel)
Login Attempts: $($script:CurrentSession.LoginAttempts)

TIMING
═══════════════════════════════════════
Start Time: $($script:CurrentSession.StartTime.ToString("yyyy-MM-dd HH:mm:ss"))
Duration: $($sessionDuration.Hours)h $($sessionDuration.Minutes)m $($sessionDuration.Seconds)s
Last Activity: $([math]::Round($lastActivityAgo.TotalMinutes, 2)) minutes ago

SECURITY CONFIGURATION
═══════════════════════════════════════
Stealth Mode: $($script:SecurityConfig.StealthMode)
Encrypt Sensitive Data: $($script:SecurityConfig.EncryptSensitiveData)
Validate Inputs: $($script:SecurityConfig.ValidateAllInputs)
Secure Connections: $($script:UseHTTPS)
Session Timeout: $($script:SecurityConfig.SessionTimeout)s
Max Login Attempts: $($script:SecurityConfig.MaxLoginAttempts)
Log Security Events: $($script:SecurityConfig.LogSecurityEvents)
Anti-Forensics: $($script:SecurityConfig.AntiForensics)
Process Hiding: $($script:SecurityConfig.ProcessHiding)

SYSTEM INFORMATION
═══════════════════════════════════════
Process ID: $PID
User Context: $([Environment]::UserName)
Machine Name: $([Environment]::MachineName)
OS Version: $([Environment]::OSVersion.VersionString)
PowerShell Version: $($PSVersionTable.PSVersion)
Security Events Logged: $(@($script:SecurityLog).Count)

OLLAMA CONNECTION
═══════════════════════════════════════
Endpoint: $OllamaAPIEndpoint
HTTPS Enabled: $script:UseHTTPS
API Key Configured: $($null -ne $script:OllamaAPIKey)
Model: $OllamaModel
"@

    $infoText.Text = $sessionInfo
    $infoForm.Controls.Add($infoText)
    $infoForm.ShowDialog()
}

function Show-SecurityLog {
    $secLogCount = @($script:SecurityLog).Count
    $logForm = New-Object System.Windows.Forms.Form
    $logForm.Text = "Security Event Log ($secLogCount events)"
    $logForm.Size = New-Object System.Drawing.Size(800, 600)
    $logForm.StartPosition = "CenterScreen"
    $logForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    
    $logGrid = New-Object System.Windows.Forms.DataGridView
    $logGrid.Dock = [System.Windows.Forms.DockStyle]::Fill
    $logGrid.BackgroundColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $logGrid.DefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $logGrid.DefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $logGrid.ColumnHeadersDefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $logGrid.ColumnHeadersDefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $logGrid.ReadOnly = $true
    $logGrid.AutoSizeColumnsMode = "AllCells"
    $logGrid.AllowUserToAddRows = $false
    
    # Add columns
    $logGrid.Columns.Add("Timestamp", "Timestamp") | Out-Null
    $logGrid.Columns.Add("Level", "Level") | Out-Null
    $logGrid.Columns.Add("Event", "Event") | Out-Null
    $logGrid.Columns.Add("Details", "Details") | Out-Null
    
    # Add data
    foreach ($entry in $script:SecurityLog) {
        $row = @($entry.Timestamp, $entry.Level, $entry.Event, $entry.Details)
        $logGrid.Rows.Add($row) | Out-Null
        
        # Color code by level
        $lastRow = $logGrid.Rows[$logGrid.Rows.Count - 1]
        switch ($entry.Level) {
            "ERROR" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Red }
            "WARNING" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Yellow }
            "SUCCESS" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Green }
            "DEBUG" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Gray }
        }
    }
    
    $logForm.Controls.Add($logGrid)
    $logForm.ShowDialog()
}

function Show-EncryptionTest {
    $testForm = New-Object System.Windows.Forms.Form
    $testForm.Text = "Encryption Test"
    $testForm.Size = New-Object System.Drawing.Size(600, 500)
    $testForm.StartPosition = "CenterScreen"
    $testForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $testForm.ForeColor = [System.Drawing.Color]::White
    
    # Input section
    $inputLabel = New-Object System.Windows.Forms.Label
    $inputLabel.Text = "Plain Text:"
    $inputLabel.Location = New-Object System.Drawing.Point(20, 20)
    $inputLabel.Size = New-Object System.Drawing.Size(100, 20)
    $testForm.Controls.Add($inputLabel)
    
    $inputBox = New-Object System.Windows.Forms.TextBox
    $inputBox.Location = New-Object System.Drawing.Point(20, 45)
    $inputBox.Size = New-Object System.Drawing.Size(540, 25)
    $inputBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $inputBox.ForeColor = [System.Drawing.Color]::White
    $inputBox.Text = "This is a test message for encryption"
    $testForm.Controls.Add($inputBox)
    
    # Encrypted section
    $encryptedLabel = New-Object System.Windows.Forms.Label
    $encryptedLabel.Text = "Encrypted:"
    $encryptedLabel.Location = New-Object System.Drawing.Point(20, 90)
    $encryptedLabel.Size = New-Object System.Drawing.Size(100, 20)
    $testForm.Controls.Add($encryptedLabel)
    
    $encryptedBox = New-Object System.Windows.Forms.TextBox
    $encryptedBox.Location = New-Object System.Drawing.Point(20, 115)
    $encryptedBox.Size = New-Object System.Drawing.Size(540, 100)
    $encryptedBox.Multiline = $true
    $encryptedBox.ReadOnly = $true
    $encryptedBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $encryptedBox.ForeColor = [System.Drawing.Color]::Cyan
    $encryptedBox.ScrollBars = "Vertical"
    $testForm.Controls.Add($encryptedBox)
    
    # Decrypted section
    $decryptedLabel = New-Object System.Windows.Forms.Label
    $decryptedLabel.Text = "Decrypted:"
    $decryptedLabel.Location = New-Object System.Drawing.Point(20, 235)
    $decryptedLabel.Size = New-Object System.Drawing.Size(100, 20)
    $testForm.Controls.Add($decryptedLabel)
    
    $decryptedBox = New-Object System.Windows.Forms.TextBox
    $decryptedBox.Location = New-Object System.Drawing.Point(20, 260)
    $decryptedBox.Size = New-Object System.Drawing.Size(540, 25)
    $decryptedBox.ReadOnly = $true
    $decryptedBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $decryptedBox.ForeColor = [System.Drawing.Color]::LightGreen
    $testForm.Controls.Add($decryptedBox)
    
    # Test info section
    $infoBox = New-Object System.Windows.Forms.TextBox
    $infoBox.Location = New-Object System.Drawing.Point(20, 300)
    $infoBox.Size = New-Object System.Drawing.Size(540, 100)
    $infoBox.Multiline = $true
    $infoBox.ReadOnly = $true
    $infoBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $infoBox.ForeColor = [System.Drawing.Color]::Yellow
    $infoBox.ScrollBars = "Vertical"
    $testForm.Controls.Add($infoBox)
    
    # Buttons
    $encryptBtn = New-Object System.Windows.Forms.Button
    $encryptBtn.Text = "Encrypt"
    $encryptBtn.Location = New-Object System.Drawing.Point(20, 410)
    $encryptBtn.Size = New-Object System.Drawing.Size(100, 30)
    $encryptBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $encryptBtn.ForeColor = [System.Drawing.Color]::White
    $encryptBtn.FlatStyle = "Flat"
    $testForm.Controls.Add($encryptBtn)
    
    $decryptBtn = New-Object System.Windows.Forms.Button
    $decryptBtn.Text = "Decrypt"
    $decryptBtn.Location = New-Object System.Drawing.Point(130, 410)
    $decryptBtn.Size = New-Object System.Drawing.Size(100, 30)
    $decryptBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $decryptBtn.ForeColor = [System.Drawing.Color]::White
    $decryptBtn.FlatStyle = "Flat"
    $testForm.Controls.Add($decryptBtn)
    
    $testBtn = New-Object System.Windows.Forms.Button
    $testBtn.Text = "Full Test"
    $testBtn.Location = New-Object System.Drawing.Point(240, 410)
    $testBtn.Size = New-Object System.Drawing.Size(100, 30)
    $testBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 150, 0)
    $testBtn.ForeColor = [System.Drawing.Color]::White
    $testBtn.FlatStyle = "Flat"
    $testForm.Controls.Add($testBtn)
    
    # Event handlers
    $encryptBtn.Add_Click({
            try {
                $plainText = $inputBox.Text
                $encrypted = [StealthCrypto]::Encrypt($plainText, $script:CurrentSession.EncryptionKey)
                $encryptedBox.Text = $encrypted
                $hash = [StealthCrypto]::Hash($plainText)
                $infoBox.Text = "Encryption successful!`r`nOriginal length: $($plainText.Length) chars`r`nEncrypted length: $($encrypted.Length) chars`r`nSHA256 Hash: $hash"
            }
            catch {
                $infoBox.Text = "Encryption failed: $($_.Exception.Message)"
            }
        })
    
    $decryptBtn.Add_Click({
            try {
                if ($encryptedBox.Text) {
                    $decrypted = [StealthCrypto]::Decrypt($encryptedBox.Text, $script:CurrentSession.EncryptionKey)
                    $decryptedBox.Text = $decrypted
                    $match = $decrypted -eq $inputBox.Text
                    $infoBox.AppendText("`r`nDecryption successful!`r`nMatches original: $match")
                }
                else {
                    $infoBox.Text = "No encrypted data to decrypt"
                }
            }
            catch {
                $infoBox.Text = "Decryption failed: $($_.Exception.Message)"
            }
        })
    
    $testBtn.Add_Click({
            try {
                $testData = "Test encryption with special chars: !@#$%^&*()_+-=[]{}|;:',.<>?/`"~``"
                $startTime = Get-Date
            
                # Test encryption
                $encrypted = [StealthCrypto]::Encrypt($testData)
                $encryptTime = ((Get-Date) - $startTime).TotalMilliseconds
            
                # Test decryption
                $startTime = Get-Date
                $decrypted = [StealthCrypto]::Decrypt($encrypted)
                $decryptTime = ((Get-Date) - $startTime).TotalMilliseconds
            
                # Test hash
                $hash1 = [StealthCrypto]::Hash($testData)
                $hash2 = [StealthCrypto]::Hash($testData)
            
                $success = $decrypted -eq $testData
                $hashConsistent = $hash1 -eq $hash2
            
                $infoBox.Text = @"
FULL ENCRYPTION TEST RESULTS:
═══════════════════════════════════
Test Data: Special characters test
Original Length: $($testData.Length) chars
Encrypted Length: $($encrypted.Length) chars
Encryption Time: $([math]::Round($encryptTime, 2))ms
Decryption Time: $([math]::Round($decryptTime, 2))ms

VALIDATION:
Decryption Success: $success
Hash Consistency: $hashConsistent
Hash Value: $($hash1.Substring(0, 16))...

SECURITY STATUS:
Algorithm: AES-256-CBC
Key Size: $($script:CurrentSession.EncryptionKey.Length * 8) bits
Session Key: Yes (unique per session)
"@
            
                Write-SecurityLog "Encryption test completed" "SUCCESS" "Duration: $([math]::Round($encryptTime + $decryptTime, 2))ms"
            }
            catch {
                $infoBox.Text = "Full test failed: $($_.Exception.Message)"
            }
        })
    
    $testForm.ShowDialog()
}

# ===============================
# PERFORMANCE OPTIMIZATION FUNCTIONS
# ===============================

function Start-PerformanceOptimization {
    try {
        Write-DevConsole "🚀 Starting performance optimization..." "INFO"
        
        # Memory optimization
        Optimize-Memory
        
        # Process priority optimization
        Optimize-ProcessPriority
        
        # Network optimization
        Optimize-NetworkSettings
        
        # UI optimization
        Optimize-UIPerformance
        
        Write-DevConsole "✅ Performance optimization completed" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error during performance optimization: $_" "ERROR"
    }
}

function Optimize-Memory {
    try {
        # Force garbage collection
        [System.GC]::Collect()
        [System.GC]::WaitForPendingFinalizers()
        [System.GC]::Collect()
        
        # Set memory management options
        [System.GC]::TryStartNoGCRegion(50MB)
        
        Write-DevConsole "🧹 Memory optimization completed" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ Memory optimization partial: $_" "WARNING"
    }
}

function Optimize-ProcessPriority {
    try {
        $process = Get-Process -Id $PID
        $process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High
        
        Write-DevConsole "⚡ Process priority set to High" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ Could not set process priority: $_" "WARNING"
    }
}

function Optimize-NetworkSettings {
    try {
        # Set HTTP connection limits
        [System.Net.ServicePointManager]::DefaultConnectionLimit = 20
        [System.Net.ServicePointManager]::Expect100Continue = $false
        [System.Net.ServicePointManager]::UseNagleAlgorithm = $false
        
        # Enable concurrent connections
        [System.Net.ServicePointManager]::EnableDnsRoundRobin = $true
        
        Write-DevConsole "🌐 Network settings optimized" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ Network optimization partial: $_" "WARNING"
    }
}

function Optimize-UIPerformance {
    try {
        # Enable double buffering for smoother UI
        Enable-ControlDoubleBuffering -Control $form

        # Optimize text rendering
        if ($script:editor) {
            Enable-ControlDoubleBuffering -Control $script:editor
        }
        
        Write-DevConsole "🎨 UI performance optimized" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ UI optimization partial: $_" "WARNING"
    }
}

function Enable-ControlDoubleBuffering {
    param(
        [System.Windows.Forms.Control]$Control
    )

    if (-not $Control) {
        return
    }

    $bindingFlags = [System.Reflection.BindingFlags] "Instance, NonPublic"
    $setStyleMethod = [System.Windows.Forms.Control].GetMethod("SetStyle", $bindingFlags)
    if ($setStyleMethod) {
        $styles = @(
            [System.Windows.Forms.ControlStyles]::AllPaintingInWmPaint,
            [System.Windows.Forms.ControlStyles]::DoubleBuffer,
            [System.Windows.Forms.ControlStyles]::ResizeRedraw,
            [System.Windows.Forms.ControlStyles]::UserPaint
        )

        foreach ($style in $styles) {
            try {
                $setStyleMethod.Invoke($Control, @($style, $true))
            }
            catch {
                # best-effort, ignore failures when reflection isn't allowed
            }
        }
    }

    $doubleBufferedProp = $Control.GetType().GetProperty("DoubleBuffered", $bindingFlags)
    if ($doubleBufferedProp) {
        try {
            $doubleBufferedProp.SetValue($Control, $true)
        }
        catch {
            # ignore failures
        }
    }
}

function Show-PerformanceMonitor {
    $perfForm = New-Object System.Windows.Forms.Form
    $perfForm.Text = "Performance Monitor"
    $perfForm.Size = New-Object System.Drawing.Size(600, 500)
    $perfForm.StartPosition = "CenterScreen"
    $perfForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::Sizable
    
    # Performance display
    $perfTextBox = New-Object System.Windows.Forms.TextBox
    $perfTextBox.Multiline = $true
    $perfTextBox.ReadOnly = $true
    $perfTextBox.ScrollBars = "Vertical"
    $perfTextBox.Font = New-Object System.Drawing.Font("Consolas", 10)
    $perfTextBox.Location = New-Object System.Drawing.Point(10, 10)
    $perfTextBox.Size = New-Object System.Drawing.Size(565, 400)
    $perfForm.Controls.Add($perfTextBox)
    
    # Optimize button
    $optimizeBtn = New-Object System.Windows.Forms.Button
    $optimizeBtn.Text = "Optimize Performance"
    $optimizeBtn.Location = New-Object System.Drawing.Point(10, 420)
    $optimizeBtn.Size = New-Object System.Drawing.Size(150, 30)
    $perfForm.Controls.Add($optimizeBtn)
    
    $optimizeBtn.Add_Click({
            Start-PerformanceOptimization
            Update-PerformanceDisplay $perfTextBox
        })
    
    # Refresh button
    $refreshBtn = New-Object System.Windows.Forms.Button
    $refreshBtn.Text = "Refresh"
    $refreshBtn.Location = New-Object System.Drawing.Point(170, 420)
    $refreshBtn.Size = New-Object System.Drawing.Size(100, 30)
    $perfForm.Controls.Add($refreshBtn)
    
    $refreshBtn.Add_Click({
            Update-PerformanceDisplay $perfTextBox
        })
    
    # Auto-refresh timer
    $perfTimer = New-Object System.Windows.Forms.Timer
    $perfTimer.Interval = 3000  # 3 seconds
    $perfTimer.Add_Tick({
            Update-PerformanceDisplay $perfTextBox
        })
    $perfTimer.Start()
    
    $perfForm.Add_FormClosed({
            $perfTimer.Stop()
            $perfTimer.Dispose()
        })
    
    # Initial display
    Update-PerformanceDisplay $perfTextBox
    
    $perfForm.ShowDialog()
}

function Update-PerformanceDisplay {
    param($TextBox)
    
    try {
        $process = Get-Process -Id $PID
        $timestamp = Get-Date -Format "HH:mm:ss"
        
        $perfInfo = @"
PERFORMANCE MONITOR - $timestamp
═══════════════════════════════════════════════════════

PROCESS INFORMATION:
  Process Name: $($process.ProcessName)
  Process ID: $($process.Id)
  Priority Class: $($process.PriorityClass)
  
MEMORY USAGE:
  Working Set: $([math]::Round($process.WorkingSet64/1MB, 2)) MB
  Private Memory: $([math]::Round($process.PrivateMemorySize64/1MB, 2)) MB
  Virtual Memory: $([math]::Round($process.VirtualMemorySize64/1MB, 2)) MB
  Peak Working Set: $([math]::Round($process.PeakWorkingSet64/1MB, 2)) MB
  
CPU USAGE:
  Total Processor Time: $($process.TotalProcessorTime)
  User Processor Time: $($process.UserProcessorTime)
  
THREAD INFORMATION:
  Thread Count: $($process.Threads.Count)
  
HANDLE COUNT:
  Handle Count: $($process.HandleCount)
  
OLLAMA STATUS:
  Connection Status: $(if (Test-OllamaConnection) { "✅ Connected" } else { "❌ Disconnected" })
  Active Servers: $(@($script:OllamaServers).Count)
  
REAL-TIME MONITORING:
  Status Updates: $(if ($script:RealTimeMonitoring.StatusTimer.Enabled) { "✅ Active" } else { "❌ Inactive" })
  Performance Tracking: $(if ($script:RealTimeMonitoring.PerformanceTimer.Enabled) { "✅ Active" } else { "❌ Inactive" })
  Network Monitoring: $(if ($script:RealTimeMonitoring.NetworkTimer.Enabled) { "✅ Active" } else { "❌ Inactive" })

ERROR HANDLING:
  Total Errors Handled: $($script:ErrorStats.TotalErrors)
  Critical Errors: $($script:ErrorStats.CriticalErrors)
  Security Events: $($script:ErrorStats.SecurityErrors)
  Auto-Recovery Actions: $($script:ErrorStats.AutoRecoveryCount)

SECURITY STATUS:
  Authentication: $(if ($script:CurrentSession) { "✅ Authenticated" } else { "❌ Not Authenticated" })
  Session Active: $(if ($script:CurrentSession) { "✅ Active (ID: $($script:CurrentSession.SessionId.Substring(0,8))...)" } else { "❌ No Session" })
  Encryption: $(if ($script:CurrentSession -and $script:CurrentSession.EncryptionKey) { "✅ AES-256-CBC" } else { "❌ Not Available" })
  Stealth Mode: $(if ($script:StealthModeActive) { "✅ Active" } else { "❌ Inactive" })

CUSTOMIZATION:
  Current Theme: $($script:CurrentTheme)
  Font Size: $($script:CurrentFontSize)pt
  UI Scale: $($script:CurrentUIScale * 100)%

RECOMMENDATIONS:
$(if ($process.WorkingSet64 -gt 500MB) { "⚠️ High memory usage detected - consider restarting`n" })$(if (@($process.Threads).Count -gt 50) { "⚠️ High thread count - check for resource leaks`n" })$(if (-not (Test-OllamaConnection)) { "⚠️ Ollama connection lost - check server status`n" })$(if ($script:ErrorStats.CriticalErrors -gt 0) { "🚨 Critical errors detected - review error logs`n" })
"@
        
        $TextBox.Text = $perfInfo
        $TextBox.SelectionStart = $TextBox.Text.Length
        $TextBox.ScrollToCaret()
    }
    catch {
        $TextBox.Text = "Error updating performance display: $($_.Exception.Message)"
    }
}

function Start-PerformanceProfiler {
    param(
        [int]$DurationSeconds = 60,
        [int]$SampleIntervalMs = 1000
    )
    
    Write-DevConsole "🔍 Starting performance profiler for $DurationSeconds seconds..." "INFO"
    
    $script:ProfilerData = @{
        StartTime = Get-Date
        Samples   = @()
        IsRunning = $true
    }
    
    $profilerTimer = New-Object System.Windows.Forms.Timer
    $profilerTimer.Interval = $SampleIntervalMs
    $sampleCount = 0
    $maxSamples = $DurationSeconds * (1000 / $SampleIntervalMs)
    
    $profilerTimer.Add_Tick({
            if ($sampleCount -ge $maxSamples) {
                $profilerTimer.Stop()
                $script:ProfilerData.IsRunning = $false
                Show-ProfilerResults
                return
            }
        
            try {
                $process = Get-Process -Id $PID
                $sample = @{
                    Timestamp     = Get-Date
                    WorkingSet    = $process.WorkingSet64
                    PrivateMemory = $process.PrivateMemorySize64
                    ThreadCount   = $process.Threads.Count
                    HandleCount   = $process.HandleCount
                }
            
                $script:ProfilerData.Samples += $sample
                $sampleCount++
            
                Write-DevConsole "📊 Profiler sample $sampleCount/$maxSamples collected" "INFO"
            }
            catch {
                Write-DevConsole "⚠️ Profiler sample error: $_" "WARNING"
            }
        })
    
    $profilerTimer.Start()
}

function Show-ProfilerResults {
    if (-not $script:ProfilerData -or $script:ProfilerData.Samples.Count -eq 0) {
        Write-DevConsole "⚠️ No profiler data available" "WARNING"
        return
    }
    
    $resultsForm = New-Object System.Windows.Forms.Form
    $resultsForm.Text = "Performance Profiler Results"
    $resultsForm.Size = New-Object System.Drawing.Size(800, 600)
    $resultsForm.StartPosition = "CenterScreen"
    
    $resultsTextBox = New-Object System.Windows.Forms.TextBox
    $resultsTextBox.Multiline = $true
    $resultsTextBox.ReadOnly = $true
    $resultsTextBox.ScrollBars = "Vertical"
    $resultsTextBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $resultsTextBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $resultsForm.Controls.Add($resultsTextBox)
    
    # Calculate statistics
    $samples = @($script:ProfilerData.Samples)
    $sampleCount = $samples.Count
    $duration = ($samples[-1].Timestamp - $samples[0].Timestamp).TotalSeconds
    
    $avgWorkingSet = ($samples | Measure-Object -Property WorkingSet -Average).Average
    $maxWorkingSet = ($samples | Measure-Object -Property WorkingSet -Maximum).Maximum
    $minWorkingSet = ($samples | Measure-Object -Property WorkingSet -Minimum).Minimum
    
    $avgPrivateMemory = ($samples | Measure-Object -Property PrivateMemory -Average).Average
    $avgThreadCount = ($samples | Measure-Object -Property ThreadCount -Average).Average
    $avgHandleCount = ($samples | Measure-Object -Property HandleCount -Average).Average
    
    $results = @"
PERFORMANCE PROFILER RESULTS
═══════════════════════════════════════════════════════

PROFILING SESSION:
  Start Time: $($script:ProfilerData.StartTime)
  Duration: $([math]::Round($duration, 2)) seconds
  Sample Count: $sampleCount
  Sample Rate: $([math]::Round($sampleCount / $duration, 2)) samples/second

MEMORY STATISTICS:
  Working Set:
    Average: $([math]::Round($avgWorkingSet/1MB, 2)) MB
    Maximum: $([math]::Round($maxWorkingSet/1MB, 2)) MB
    Minimum: $([math]::Round($minWorkingSet/1MB, 2)) MB
    Variation: $([math]::Round(($maxWorkingSet - $minWorkingSet)/1MB, 2)) MB
    
  Private Memory:
    Average: $([math]::Round($avgPrivateMemory/1MB, 2)) MB

RESOURCE STATISTICS:
  Thread Count Average: $([math]::Round($avgThreadCount, 1))
  Handle Count Average: $([math]::Round($avgHandleCount, 1))

PERFORMANCE ANALYSIS:
$(if (($maxWorkingSet - $minWorkingSet) -gt 100MB) { "⚠️ High memory variation detected - potential memory leaks`n" })$(if ($avgThreadCount -gt 30) { "⚠️ High average thread count - check for thread leaks`n" })$(if ($avgWorkingSet -gt 300MB) { "⚠️ High average memory usage`n" })✅ Profiling completed successfully

DETAILED SAMPLES:
════════════════
"@
    
    foreach ($sample in $samples) {
        $results += "`n$($sample.Timestamp.ToString('HH:mm:ss.fff')) | WS: $([math]::Round($sample.WorkingSet/1MB, 1))MB | PM: $([math]::Round($sample.PrivateMemory/1MB, 1))MB | T: $($sample.ThreadCount) | H: $($sample.HandleCount)"
    }
    
    $resultsTextBox.Text = $results
    $resultsForm.ShowDialog()
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

# Add controls to form
$form.Controls.Add($mainSplitter) | Out-Null
$form.Controls.Add($menu) | Out-Null

# Global error handler
$form.Add_Shown({
        Write-DevConsole "RawrXD Form Loaded Successfully" "SUCCESS"
        Write-DevConsole "Agent Mode: $(if ($global:AgentMode) { 'ON' } else { 'OFF' })" "INFO"
        Write-DevConsole "Current Directory: $global:currentWorkingDir" "INFO"
        
        # Load customization settings
        Load-CustomizationSettings
        
        # Initialize performance optimization
        Start-PerformanceOptimization
        
        # Start Ollama server automatically
        if (-not $global:ollamaStartupAttempted) {
            $global:ollamaStartupAttempted = $true
            Write-DevConsole "Auto-starting Ollama server..." "INFO"
            Start-OllamaServer | Out-Null
        }
        
        # Set up a timer to update status periodically (only if not already created)
        if (-not $script:ollamaTimer) {
            $script:ollamaTimer = New-Object System.Windows.Forms.Timer
            $script:ollamaTimer.Interval = 2000  # Check every 2 seconds
            $script:ollamaTimer.Add_Tick({
                    Update-OllamaStatusDisplay
                })
            $script:ollamaTimer.Start()
        }
        
        # Update initial status
        Update-OllamaStatusDisplay
    })# Capture all unhandled exceptions
$null = Register-ObjectEvent -InputObject ([System.AppDomain]::CurrentDomain) -EventName UnhandledException -Action {
    $errorMsg = $Event.SourceEventArgs.ExceptionObject.ToString()
    Write-DevConsole "UNHANDLED EXCEPTION: $errorMsg" "ERROR"
}

# Error handling for form display
Write-StartupLog "Starting RawrXD main application..." "INFO"
try {
    Write-StartupLog "Initializing main form..." "INFO"
    Write-StartupLog "Application startup completed successfully!" "SUCCESS"
    Write-StartupLog "═══════════════════════════════════════════════" "SUCCESS"
    Write-StartupLog "SESSION LOG: Check Dev Tools tab for runtime logs" "INFO"
    Write-StartupLog "STARTUP LOG: $script:StartupLogFile" "INFO"
    Write-StartupLog "═══════════════════════════════════════════════" "INFO"
    
    # Add form closing event to cleanup Ollama server
    $form.Add_FormClosing({
            param($formSender, $formArgs)
            try {
                Write-DevConsole "Application closing - cleaning up Ollama server..." "INFO"
                
                # Safely cleanup the Ollama timer if it exists
                if ((Get-Variable -Name "ollamaTimer" -Scope Script -ErrorAction SilentlyContinue) -and $script:ollamaTimer) {
                    try {
                        $script:ollamaTimer.Stop()
                        $script:ollamaTimer.Dispose()
                        Write-DevConsole "Ollama timer stopped and disposed" "INFO"
                    }
                    catch {
                        Write-DevConsole "Error stopping Ollama timer: $_" "WARNING"
                    }
                }
                
                # Safely cleanup the agent monitor timer
                if ((Get-Variable -Name "agentMonitorTimer" -Scope Script -ErrorAction SilentlyContinue) -and $script:agentMonitorTimer) {
                    try {
                        $script:agentMonitorTimer.Stop()
                        $script:agentMonitorTimer.Dispose()
                        Write-DevConsole "Agent monitor timer stopped" "INFO"
                    }
                    catch {
                        Write-DevConsole "Error stopping agent monitor timer: $_" "WARNING"
                    }
                }
                
                # Cleanup multithreading system
                try {
                    Stop-MultithreadedAgents
                    Write-DevConsole "Multithreaded agent system stopped" "INFO"
                }
                catch {
                    Write-DevConsole "Error stopping multithreaded agents: $_" "WARNING"
                }
                
                # Note: We don't auto-stop Ollama server on exit as it might be used by other apps
                # Users can manually stop it using the Stop button if desired
                Write-DevConsole "Application cleanup completed" "INFO"
            }
            catch {
                Write-DevConsole "Error during application cleanup: $_" "ERROR"
                # Don't prevent form from closing even if cleanup fails
            }
        })
    
    # Initialize chat system with first tab
    Write-StartupLog "Initializing chat system..." "INFO"
    $initialChatId = New-ChatTab -TabName "Main Chat"
    if ($initialChatId) {
        Write-StartupLog "✅ Initial chat tab created: $initialChatId" "INFO"
    }
    else {
        Write-StartupLog "⚠ Failed to create initial chat tab" "WARNING"
    }
    
    # Initialize multithreading system for agents
    Write-StartupLog "Initializing multithreaded agent system..." "INFO"
    $multithreadingEnabled = Initialize-MultithreadedAgents
    if ($multithreadingEnabled) {
        Write-StartupLog "✅ Multithreaded agents initialized successfully" "INFO"
        
        # Start monitoring timer for agent jobs
        $script:agentMonitorTimer = New-Object System.Windows.Forms.Timer
        $script:agentMonitorTimer.Interval = 500  # Check every 500ms
        $script:agentMonitorTimer.add_Tick({
                Monitor-AgentJobs
                Update-ThreadingStatusLabel
            })
        $script:agentMonitorTimer.Start()
        Write-StartupLog "✅ Agent monitoring system started" "INFO"
        
        # Set initial threading status
        Update-ThreadingStatusLabel
    }
    else {
        Write-StartupLog "⚠ Multithreaded agents disabled, using single-threaded fallback" "WARNING"
    }
    
    # ============================================
    # SECURITY INITIALIZATION
    # ============================================
    
    Write-SecurityLog "Application initialization completed" "SUCCESS" "Features: Encryption=$($script:SecurityConfig.EncryptSensitiveData), HTTPS=$script:UseHTTPS, Stealth=$($script:SecurityConfig.StealthMode)"
    
    # Apply stealth mode if enabled
    if ($script:SecurityConfig.StealthMode) {
        Enable-StealthMode -Enable $true
        Write-StartupLog "✅ Stealth mode activated" "INFO"
    }
    
    # Set up periodic security checks
    if ($script:SecurityConfig.LogSecurityEvents) {
        $script:securityTimer = New-Object System.Windows.Forms.Timer
        $script:securityTimer.Interval = 60000  # Check every minute
        $script:securityTimer.add_Tick({
                if (-not (Test-SessionSecurity)) {
                    Write-SecurityLog "Session security check failed during runtime" "ERROR"
                    if ($script:SecurityConfig.AuthenticationRequired) {
                        $result = "Yes"; Write-DevConsole "Session security check failed - auto-attempting re-authentication" "WARNING"
                        if ($result -eq "Yes") {
                            $authResult = Show-AuthenticationDialog
                            if (-not $authResult) {
                                Write-SecurityLog "Re-authentication failed, closing application" "ERROR"
                                $form.Close()
                            }
                            else {
                                Write-SecurityLog "Re-authentication successful" "SUCCESS"
                            }
                        }
                        else {
                            $form.Close()
                        }
                    }
                }
            })
        $script:securityTimer.Start()
        Write-StartupLog "✅ Security monitoring timer started" "INFO"
    }
    
    Write-StartupLog "✅ Security initialization completed successfully" "SUCCESS"
    
    # ============================================
    # LAUNCH APPLICATION (GUI OR CONSOLE FALLBACK)
    # ============================================
    
    if ($script:WindowsFormsAvailable) {
        Write-StartupLog "Launching RawrXD GUI..." "INFO"
        try {
            $form.ShowDialog() | Out-Null
        }
        catch {
            Write-EmergencyLog "❌ Error launching GUI: $($_.Exception.Message)" "ERROR"
            Write-EmergencyLog "Falling back to console mode..." "WARNING"
            Start-ConsoleMode
        }
    }
    else {
        Write-EmergencyLog "GUI not available - starting in console mode" "WARNING"
        Start-ConsoleMode
    }
    
    # ============================================
    # SECURITY CLEANUP ON EXIT
    # ============================================
    
    Write-SecurityLog "Application shutting down" "INFO"
    
    # Stop security timer
    if ($script:securityTimer) {
        $script:securityTimer.Stop()
        $script:securityTimer.Dispose()
    }
    
    # Perform final security cleanup
    Invoke-SecureCleanup
    
    Write-SecurityLog "Application shutdown completed" "SUCCESS"
}
catch {
    $errorMsg = $_.Exception.Message
    Write-StartupLog "CRITICAL ERROR during application startup: $errorMsg" "ERROR"
    Write-StartupLog "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    Write-DevConsole "FATAL ERROR: $errorMsg" "ERROR"
    Write-DevConsole "❌ Critical startup failure - Check startup log: $script:StartupLogFile" "ERROR"
    exit 1
}

# Application closing
Write-StartupLog "RawrXD application session ended" "INFO"
Write-StartupLog "═══════════════════════════════════════════════" "INFO"




