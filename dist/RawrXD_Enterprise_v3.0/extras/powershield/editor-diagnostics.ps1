<#
.SYNOPSIS
    Built-in Editor Diagnostics and Auto-Repair System for RawrXD
.DESCRIPTION
    Continuously monitors editor health and automatically fixes common issues:
    - Greyed-out text detection and repair
    - Missing color properties
    - Focus handling problems
    - Event handler validation
    - Auto-recovery on state corruption
.FUNCTIONALITY
    - Real-time editor state monitoring
    - Automatic color restoration
    - Event handler injection/repair
    - Diagnostic reporting
    - Self-healing without user intervention
#>

# ============================================
# EDITOR DIAGNOSTICS & REPAIR SYSTEM
# ============================================

$script:EditorDiagnostics = @{
    LastCheckTime = $null
    HealthStatus = "Unknown"
    IssuesDetected = @()
    RepairAttempts = 0
    LastRepairTime = $null
    AutoRepairEnabled = $true
    CheckIntervalMs = 2000  # Check every 2 seconds when focused
}

# Helper function to get current editor (supports tabbed and legacy editors)
function Get-CurrentEditor {
    # Try tabbed editor system first
    if ($script:editorTabControl -and $script:editorTabControl.SelectedTab) {
        $selectedPath = $script:editorTabControl.SelectedTab.Tag
        if ($selectedPath -and $script:editorBoxes -and $script:editorBoxes.ContainsKey($selectedPath)) {
            return $script:editorBoxes[$selectedPath]
        }
    }
    # Fallback to legacy single editor
    if ($script:editor) {
        return $script:editor
    }
    return $null
}

function Test-EditorHealth {
    <#
    .SYNOPSIS
        Diagnose current editor state and identify problems
    .DESCRIPTION
        Comprehensive health check for text editor component
    #>
    param(
        [Parameter(ValueFromPipeline = $true)]
        [object]$Editor = $null
    )

    # Try to get current editor from tabbed system or fallback to legacy
    if ($null -eq $Editor) {
        $Editor = Get-CurrentEditor
    }

    if ($null -eq $Editor) {
        return @{
            Status = "ERROR"
            Issues = @("Editor object is null")
            HealthScore = 0
        }
    }

    $issues = @()
    $healthScore = 100

    try {
        # Check 1: BackColor Property
        if ($Editor.BackColor -eq [System.Drawing.Color]::Empty) {
            $issues += "❌ BackColor is empty (default system color)"
            $healthScore -= 15
        }
        elseif ($Editor.BackColor.GetBrightness() -lt 0.2) {
            $issues += "✅ BackColor is dark (good)"
        }
        else {
            $issues += "⚠️ BackColor is too light: $($Editor.BackColor)"
            $healthScore -= 10
        }

        # Check 2: ForeColor Property
        if ($Editor.ForeColor -eq [System.Drawing.Color]::Empty) {
            $issues += "❌ ForeColor is empty (default system color)"
            $healthScore -= 15
        }
        elseif ($Editor.ForeColor.GetBrightness() -gt 0.8) {
            $issues += "✅ ForeColor is bright (good)"
        }
        else {
            $issues += "⚠️ ForeColor is too dark: $($Editor.ForeColor)"
            $healthScore -= 10
        }

        # Check 3: SelectionColor Property
        if ($Editor.SelectionColor -eq [System.Drawing.Color]::Empty) {
            $issues += "❌ SelectionColor is empty"
            $healthScore -= 15
        }
        elseif ($Editor.SelectionColor.GetBrightness() -gt 0.8) {
            $issues += "✅ SelectionColor is bright (good)"
        }
        else {
            $issues += "⚠️ SelectionColor may be too dark"
            $healthScore -= 10
        }

        # Check 4: BorderStyle
        if ($Editor.BorderStyle -ne [System.Windows.Forms.BorderStyle]::None) {
            $issues += "⚠️ BorderStyle is not None: $($Editor.BorderStyle)"
            $healthScore -= 5
        }
        else {
            $issues += "✅ BorderStyle is correct (None)"
        }

        # Check 5: Text visibility
        if ($Editor.Text.Length -gt 0) {
            $firstLine = ($Editor.Text -split "`n")[0]
            if ($firstLine.Length -gt 0 -and $firstLine -ne "greyed") {
                $issues += "✅ Text is present and should be visible"
            }
        }

        # Check 6: Event handlers
        $gf = $Editor | Get-Member -Name "GotFocus" -ErrorAction SilentlyContinue
        $lf = $Editor | Get-Member -Name "LostFocus" -ErrorAction SilentlyContinue

        if ($gf -and $lf) {
            $issues += "✅ Focus event handlers are present"
        }
        else {
            $issues += "⚠️ Some focus event handlers may be missing"
            $healthScore -= 10
        }

        # Check 7: ReadOnly state (should not be readonly)
        if ($Editor.ReadOnly) {
            $issues += "❌ Editor is in ReadOnly mode!"
            $healthScore -= 25
        }
        else {
            $issues += "✅ Editor is in edit mode"
        }

        # Check 8: Enabled state
        if (-not $Editor.Enabled) {
            $issues += "❌ Editor is disabled!"
            $healthScore -= 25
        }
        else {
            $issues += "✅ Editor is enabled"
        }

    }
    catch {
        $issues += "❌ Error during diagnostics: $($_.Exception.Message)"
        $healthScore = 0
    }

    # Determine overall status
    $status = if ($healthScore -ge 80) { "HEALTHY" } elseif ($healthScore -ge 50) { "DEGRADED" } else { "CRITICAL" }

    return @{
        Status = $status
        HealthScore = $healthScore
        Issues = $issues
        Timestamp = Get-Date
    }
}

function Repair-EditorColors {
    <#
    .SYNOPSIS
        Restore editor colors to proper state
    .DESCRIPTION
        Force all color properties to correct values
    #>
    param(
        [Parameter(ValueFromPipeline = $true)]
        [object]$Editor = $null
    )

    if ($null -eq $Editor) { $Editor = Get-CurrentEditor }

    if ($null -eq $Editor) {
        # Silently return - no editor open yet is normal
        return $false
    }

    try {
        Write-StartupLog "🔧 Repairing editor colors..." "INFO"

        # Force all colors to correct values
        $Editor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
        $Editor.ForeColor = [System.Drawing.Color]::White
        $Editor.SelectionColor = [System.Drawing.Color]::White
        $Editor.SelectionBackColor = [System.Drawing.Color]::FromArgb(51, 77, 119)

        # Refresh the control
        $Editor.Refresh()

        # Force a redraw
        $Editor.Invalidate()

        Write-StartupLog "✅ Editor colors restored" "SUCCESS"
        $script:EditorDiagnostics.LastRepairTime = Get-Date
        $script:EditorDiagnostics.RepairAttempts += 1

        return $true
    }
    catch {
        Write-StartupLog "Failed to repair colors: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Repair-EditorEventHandlers {
    <#
    .SYNOPSIS
        Reinject critical event handlers if missing
    #>
    param(
        [Parameter(ValueFromPipeline = $true)]
        [object]$Editor = $null
    )

    if ($null -eq $Editor) { $Editor = Get-CurrentEditor }

    if ($null -eq $Editor) {
        return $false
    }

    try {
        Write-StartupLog "🔧 Repairing event handlers..." "INFO"

        # Re-add GotFocus handler
        $Editor.Add_GotFocus({
            param($sender, $e)
            $sender.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
            $sender.ForeColor = [System.Drawing.Color]::White
            $sender.SelectionColor = [System.Drawing.Color]::White
            $sender.Refresh()
        })

        # Re-add LostFocus handler
        $Editor.Add_LostFocus({
            param($sender, $e)
            $sender.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
            $sender.ForeColor = [System.Drawing.Color]::White
        })

        # Re-add KeyPress handler
        $Editor.Add_KeyPress({
            param($sender, $e)
            $sender.SelectionColor = [System.Drawing.Color]::White
        })

        Write-StartupLog "✅ Event handlers restored" "SUCCESS"
        return $true
    }
    catch {
        Write-StartupLog "Failed to repair event handlers: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Repair-EditorState {
    <#
    .SYNOPSIS
        Full editor state restoration
    .DESCRIPTION
        Complete repair including colors, properties, and event handlers
    #>
    param(
        [Parameter(ValueFromPipeline = $true)]
        [object]$Editor = $null
    )

    if ($null -eq $Editor) { $Editor = Get-CurrentEditor }

    if ($null -eq $Editor) {
        # Silently return - no editor open yet is normal
        return $false
    }

    try {
        Write-StartupLog "🚀 Starting full editor repair..." "INFO"

        # 1. Restore basic properties
        $Editor.ReadOnly = $false
        $Editor.Enabled = $true
        $Editor.BorderStyle = [System.Windows.Forms.BorderStyle]::None

        # 2. Restore colors
        Repair-EditorColors -Editor $Editor | Out-Null

        # 3. Restore event handlers
        Repair-EditorEventHandlers -Editor $Editor | Out-Null

        # 4. Force refresh
        $Editor.Refresh()
        $Editor.Invalidate()

        # 5. Force focus and selection update
        if ($Editor.Parent) {
            $Editor.Focus()
            $Editor.SelectAll()
            $Editor.DeselectAll()
        }

        Write-StartupLog "✅ Full editor repair completed" "SUCCESS"
        $script:EditorDiagnostics.HealthStatus = "REPAIRED"

        return $true
    }
    catch {
        Write-StartupLog "Full repair failed: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Start-EditorMonitoring {
    <#
    .SYNOPSIS
        Start continuous editor health monitoring with auto-repair
    .DESCRIPTION
        Launches background monitoring that checks editor state and auto-repairs issues
    #>
    param(
        [int]$IntervalMs = 2000,
        [switch]$AutoRepair
    )

    try {
        Write-StartupLog "🚀 Starting editor monitoring system..." "INFO"

        # Create monitor timer if it doesn't exist
        if ($null -eq $script:editorMonitorTimer) {
            $script:editorMonitorTimer = New-Object System.Windows.Forms.Timer
            $script:editorMonitorTimer.Interval = $IntervalMs

            $script:editorMonitorTimer.Add_Tick({
                try {
                    $currentEditor = Get-CurrentEditor
                    if ($null -eq $currentEditor) {
                        return
                    }

                    # Quick health check
                    $health = Test-EditorHealth -Editor $currentEditor
                    $script:EditorDiagnostics.HealthStatus = $health.Status
                    $script:EditorDiagnostics.IssuesDetected = $health.Issues

                    # Auto-repair if enabled and problems detected
                    if ($script:EditorDiagnostics.AutoRepairEnabled -and $health.HealthScore -lt 80) {
                        Write-DevConsole "🔧 Auto-repairing editor (score: $($health.HealthScore))" "WARNING"
                        Repair-EditorState -Editor $currentEditor | Out-Null
                    }
                } catch { }
            })
        }

        $script:editorMonitorTimer.Start()
        $script:EditorDiagnostics.AutoRepairEnabled = if ($AutoRepair) { $true } else { $false }

        Write-StartupLog "✅ Editor monitoring started (interval: ${IntervalMs}ms, auto-repair: $($script:EditorDiagnostics.AutoRepairEnabled))" "SUCCESS"
        return $true
    }
    catch {
        Write-StartupLog "Failed to start monitoring: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Stop-EditorMonitoring {
    <#
    .SYNOPSIS
        Stop editor health monitoring
    #>
    try {
        if ($null -ne $script:editorMonitorTimer) {
            $script:editorMonitorTimer.Stop()
            $script:editorMonitorTimer.Dispose()
            $script:editorMonitorTimer = $null
            Write-StartupLog "✅ Editor monitoring stopped" "SUCCESS"
            return $true
        }
    }
    catch {
        Write-StartupLog "Error stopping monitor: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Get-EditorDiagnostics {
    <#
    .SYNOPSIS
        Get current editor diagnostics report
    #>
    $health = Test-EditorHealth

    $report = @{
        Timestamp = Get-Date
        HealthStatus = $health.Status
        HealthScore = $health.HealthScore
        Issues = $health.Issues
        RepairAttempts = $script:EditorDiagnostics.RepairAttempts
        LastRepairTime = $script:EditorDiagnostics.LastRepairTime
        AutoRepairEnabled = $script:EditorDiagnostics.AutoRepairEnabled
        MonitoringActive = $null -ne $script:editorMonitorTimer
    }

    return $report
}

function Show-EditorDiagnosticsDialog {
    <#
    .SYNOPSIS
        Display editor diagnostics in visual format
    #>
    try {
        $diag = Get-EditorDiagnostics

        $message = @"
📊 EDITOR DIAGNOSTICS REPORT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Status: $($diag.HealthStatus)
Health Score: $($diag.HealthScore)/100
Last Repair: $($diag.LastRepairTime)
Total Repairs: $($diag.RepairAttempts)

Monitoring Active: $($diag.MonitoringActive)
Auto-Repair Enabled: $($diag.AutoRepairEnabled)

Issues Detected:
$($diag.Issues | ForEach-Object { "  $_" } | Out-String)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
"@

        [System.Windows.Forms.MessageBox]::Show(
            $message,
            "Editor Diagnostics",
            "OK",
            "Information"
        ) | Out-Null
    }
    catch {
        Write-DevConsole "Error showing diagnostics: $($_.Exception.Message)" "ERROR"
    }
}

# ============================================
# INITIALIZATION
# ============================================

function Initialize-EditorDiagnosticsSystem {
    <#
    .SYNOPSIS
        Initialize editor diagnostics when app starts
    #>
    try {
        Write-StartupLog "Initializing Editor Diagnostics System..." "INFO"

        # Wait for editor to be created (tabbed system or legacy)
        $waitCount = 0
        $editorFound = $false
        while (-not $editorFound -and $waitCount -lt 50) {
            [System.Windows.Forms.Application]::DoEvents()
            # Check for tabbed editor system
            if ($script:editorTabControl -and $script:editorTabControl.TabCount -gt 0) {
                $editorFound = $true
            }
            # Check for legacy editor
            elseif ($script:editor) {
                $editorFound = $true
            }
            else {
                Start-Sleep -Milliseconds 100
                $waitCount++
            }
        }

        if (-not $editorFound) {
            Write-StartupLog "Editor not found during diagnostics initialization (will retry on demand)" "DEBUG"
            # Don't return false - diagnostics can still work later
        }

        # Perform initial health check
        $health = Test-EditorHealth
        Write-StartupLog "Initial editor health: $($health.Status) (score: $($health.HealthScore)/100)" "INFO"

        # If initial health is poor, repair immediately
        if ($health.HealthScore -lt 80) {
            Write-StartupLog "Initial repair needed..." "WARNING"
            Repair-EditorState | Out-Null
        }

        # Start continuous monitoring with auto-repair enabled
        Start-EditorMonitoring -IntervalMs 2000 -AutoRepair

        Write-StartupLog "✅ Editor Diagnostics System initialized" "SUCCESS"
        return $true
    }
    catch {
        Write-StartupLog "Failed to initialize Editor Diagnostics: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

# ============================================
# MENU INTEGRATION
# ============================================

function Add-EditorDiagnosticsMenu {
    <#
    .SYNOPSIS
        Add diagnostics menu item to IDE
    .DESCRIPTION
        Creates menu item for quick access to diagnostics and repairs
    #>
    try {
        $diagnosticsMenu = New-Object System.Windows.Forms.ToolStripMenuItem
        $diagnosticsMenu.Text = "🔧 Editor"

        # Show Diagnostics
        $showDiagItem = New-Object System.Windows.Forms.ToolStripMenuItem
        $showDiagItem.Text = "📊 Show Diagnostics"
        $showDiagItem.Add_Click({ Show-EditorDiagnosticsDialog })
        $diagnosticsMenu.DropDownItems.Add($showDiagItem) | Out-Null

        # Repair Editor
        $repairItem = New-Object System.Windows.Forms.ToolStripMenuItem
        $repairItem.Text = "🔨 Repair Colors"
        $repairItem.Add_Click({
            Repair-EditorColors
            [System.Windows.Forms.MessageBox]::Show("Colors restored!", "Editor Repair", "OK", "Information") | Out-Null
        })
        $diagnosticsMenu.DropDownItems.Add($repairItem) | Out-Null

        # Full Repair
        $fullRepairItem = New-Object System.Windows.Forms.ToolStripMenuItem
        $fullRepairItem.Text = "🚀 Full Repair"
        $fullRepairItem.Add_Click({
            Repair-EditorState
            [System.Windows.Forms.MessageBox]::Show("Full repair completed!", "Editor Repair", "OK", "Information") | Out-Null
        })
        $diagnosticsMenu.DropDownItems.Add($fullRepairItem) | Out-Null

        # Separator
        $sep = New-Object System.Windows.Forms.ToolStripSeparator
        $diagnosticsMenu.DropDownItems.Add($sep) | Out-Null

        # Toggle Auto-Repair
        $toggleItem = New-Object System.Windows.Forms.ToolStripMenuItem
        $toggleItem.Text = "⚙️ Auto-Repair: $(if ($script:EditorDiagnostics.AutoRepairEnabled) { 'ON' } else { 'OFF' })"
        $toggleItem.Add_Click({
            $script:EditorDiagnostics.AutoRepairEnabled = -not $script:EditorDiagnostics.AutoRepairEnabled
            [System.Windows.Forms.MessageBox]::Show(
                "Auto-Repair: $(if ($script:EditorDiagnostics.AutoRepairEnabled) { 'ENABLED' } else { 'DISABLED' })",
                "Auto-Repair Status",
                "OK",
                "Information"
            ) | Out-Null
        })
        $diagnosticsMenu.DropDownItems.Add($toggleItem) | Out-Null

        return $diagnosticsMenu
    }
    catch {
        Write-StartupLog "Failed to create diagnostics menu: $($_.Exception.Message)" "ERROR"
        return $null
    }
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Functions are automatically available in the parent scope when dot-sourced
# Export-ModuleMember -Function @(
#     "Test-EditorHealth"
#     "Repair-EditorColors"
#     "Repair-EditorEventHandlers"
#     "Repair-EditorState"
#     "Start-EditorMonitoring"
#     "Stop-EditorMonitoring"
#     "Get-EditorDiagnostics"
#     "Show-EditorDiagnosticsDialog"
#     "Initialize-EditorDiagnosticsSystem"
#     "Add-EditorDiagnosticsMenu"
# )

Write-StartupLog "✅ Editor Diagnostics module loaded" "SUCCESS"
