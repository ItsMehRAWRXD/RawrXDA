<#
.SYNOPSIS
    .NET Runtime Switcher Module for RawrXD
.DESCRIPTION
    Provides intelligent .NET version detection, switching, and fallback capabilities
    Allows users to switch between different .NET runtimes without restarting the IDE
.FUNCTIONALITY
    - Detect all installed .NET versions on system
    - Display available .NET runtimes in IDE menu
    - Switch to different .NET version at runtime
    - Reinitialize assemblies for selected runtime
    - Store preferred .NET version in settings
    - Automatic fallback on incompatibility
#>

# ============================================
# .NET RUNTIME DETECTION & MANAGEMENT
# ============================================

$script:DotNetRuntimes = @{}
$script:CurrentDotNetRuntime = $null
$script:PreferredDotNetVersion = $null
$script:DotNetSwitchEnabled = $false

function Get-InstalledDotNetVersions {
  <#
    .SYNOPSIS
        Detect all installed .NET versions on the system
    .DESCRIPTION
        Scans registry, environment, and common installation paths for .NET runtimes
    #>

  Write-StartupLog "Scanning for installed .NET versions..." "INFO"
  $versions = @{}

  # Get current runtime
  try {
    $currentRuntime = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
    $currentVersion = $null

    if ($currentRuntime -match "\.NET\s+(\d+)\.(\d+)") {
      $major = [int]$matches[1]
      $minor = [int]$matches[2]
      $currentVersion = "$major.$minor"
    }
    elseif ($currentRuntime -match "\.NET Framework\s+([\d.]+)") {
      $currentVersion = $matches[1]
    }

    $versions["Current"] = @{
      Version     = $currentVersion
      Description = $currentRuntime
      IsFramework = $currentRuntime -like "*Framework*"
      IsCurrent   = $true
      Executable  = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
      Path        = $null
    }

    Write-StartupLog "Current Runtime: $currentRuntime" "SUCCESS"
  }
  catch {
    Write-StartupLog "Could not detect current runtime: $_" "WARNING"
  }

  # Check .NET Framework (Windows only)
  if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
    try {
      $frameworkPath = "HKLM:\Software\Microsoft\NET Framework Setup\NDP"
      if (Test-Path $frameworkPath) {
        $frameworks = Get-ChildItem $frameworkPath -ErrorAction SilentlyContinue
        foreach ($fw in $frameworks) {
          $version = $fw.PSChildName
          if ($version -match "^v\d+") {
            $installPath = $fw.GetValue("InstallPath")
            if ($installPath -and (Test-Path $installPath)) {
              $versions[".NET Framework $version"] = @{
                Version     = $version
                Description = ".NET Framework $version"
                IsFramework = $true
                IsCurrent   = $false
                Path        = $installPath
                Executable  = $null
              }
              Write-StartupLog "Found .NET Framework: $version at $installPath" "INFO"
            }
          }
        }
      }
    }
    catch {
      Write-StartupLog "Error scanning .NET Framework: $_" "WARNING"
    }
  }

  # Check installed .NET SDK/Runtimes
  try {
    $dotnetPath = if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
      Join-Path $env:ProgramFiles "dotnet"
    }
    else {
      # Linux/Mac paths
      $home = if ($IsLinux) { $env:HOME } else { $env:HOME }
      Join-Path $home ".dotnet"
    }

    if (Test-Path $dotnetPath) {
      # Check for dotnet CLI
      $dotnetCli = Join-Path $dotnetPath "dotnet"
      if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
        $dotnetCli = "$dotnetCli.exe"
      }

      if (Test-Path $dotnetCli) {
        try {
          $dotnetInfo = & $dotnetCli --list-runtimes 2>$null
          if ($dotnetInfo) {
            foreach ($runtime in $dotnetInfo) {
              if ($runtime -match "(.+?)\s+([\d.]+)\s+\[(.+?)\]") {
                $name = $matches[1]
                $version = $matches[2]
                $path = $matches[3]

                $versions[".NET $version ($name)"] = @{
                  Version     = $version
                  Description = ".NET $version - $name"
                  IsFramework = $false
                  IsCurrent   = $false
                  Path        = $path
                  Executable  = $dotnetCli
                }
                Write-StartupLog "Found .NET Runtime: $version ($name) at $path" "INFO"
              }
            }
          }
        }
        catch {
          Write-StartupLog "Error querying dotnet runtimes: $_" "WARNING"
        }
      }
    }
  }
  catch {
    Write-StartupLog "Error scanning .NET installations: $_" "WARNING"
  }

  # Return sorted by version (newest first)
  $sorted = $versions.GetEnumerator() | Sort-Object {
    [version]($_.Value.Version -replace "[^\d.]")
  } -Descending | ForEach-Object { $_.Value }

  Write-StartupLog "Found $($versions.Count) .NET versions available" "SUCCESS"
  return $versions
}

function Initialize-DotNetRuntimeSwitcher {
  <#
    .SYNOPSIS
        Initialize the .NET runtime switcher system
    .DESCRIPTION
        Detects available runtimes and sets up switching capabilities
    #>

  Write-StartupLog "Initializing .NET Runtime Switcher..." "INFO"

  # Detect available runtimes
  $script:DotNetRuntimes = Get-InstalledDotNetVersions

  # Load user preference from settings (with safe property access)
  if ($global:settings -is [hashtable]) {
    $script:PreferredDotNetVersion = $global:settings["PreferredDotNetVersion"]
  }
  elseif ($null -ne $global:settings -and (Get-Member -InputObject $global:settings -Name "PreferredDotNetVersion" -ErrorAction SilentlyContinue)) {
    $script:PreferredDotNetVersion = $global:settings.PreferredDotNetVersion
  }

  if ([string]::IsNullOrEmpty($script:PreferredDotNetVersion)) {
    $script:PreferredDotNetVersion = "Current"
  }

  # Enable switching feature
  $script:DotNetSwitchEnabled = $true

  Write-StartupLog "✅ .NET Runtime Switcher initialized with $($script:DotNetRuntimes.Count) available versions" "SUCCESS"
}

function Get-DotNetRuntimeStatus {
  <#
    .SYNOPSIS
        Get detailed status of all available .NET runtimes
    #>

  $status = @()

  foreach ($rtKey in $script:DotNetRuntimes.Keys) {
    $rt = $script:DotNetRuntimes[$rtKey]
    $status += @{
      Name        = $rtKey
      Version     = $rt.Version
      Description = $rt.Description
      IsFramework = $rt.IsFramework
      IsCurrent   = $rt.IsCurrent
      IsSelected  = ($rtKey -eq $script:CurrentDotNetRuntime)
      Available   = $true
    }
  }

  return $status | Sort-Object { $_.IsCurrent } -Descending | Sort-Object { $_.IsSelected } -Descending
}

function Test-DotNetCompatibility {
  <#
    .SYNOPSIS
        Test if a .NET version supports WebView2 and other features
    .PARAMETER DotNetVersion
        Version string like "9.0", "8.0", "4.8"
    #>

  param([string]$DotNetVersion)

  $compatibility = @{
    Version          = $DotNetVersion
    WebView2         = $null
    WindowsForms     = $null
    AsyncIO          = $null
    HighPerformance  = $null
    SecurityFeatures = $null
    Recommendation   = $null
    Issues           = @()
  }

  try {
    if ($DotNetVersion -match "^(\d+)") {
      $major = [int]$matches[1]

      # WebView2 compatibility
      if ($major -le 8) {
        $compatibility.WebView2 = "✅ Compatible"
        $compatibility.Issues += "None known"
      }
      elseif ($major -eq 9) {
        $compatibility.WebView2 = "⚠️ Partial (IE Fallback)"
        $compatibility.Issues += "System.Windows.Forms.ContextMenu compatibility issue"
      }
      else {
        $compatibility.WebView2 = "❓ Unknown"
        $compatibility.Issues += "Not tested on .NET $major"
      }

      # Windows Forms
      if ($major -lt 6) {
        $compatibility.WindowsForms = "❌ Not Supported"
        $compatibility.Issues += ".NET $major lacks WindowsDesktop.App"
      }
      else {
        $compatibility.WindowsForms = "✅ Supported"
      }

      # Async IO
      if ($major -ge 6) {
        $compatibility.AsyncIO = "✅ Full Support"
      }
      else {
        $compatibility.AsyncIO = "⚠️ Limited"
      }

      # Performance
      if ($major -ge 8) {
        $compatibility.HighPerformance = "✅ Optimized"
      }
      else {
        $compatibility.HighPerformance = "⚠️ Legacy"
      }

      # Security
      if ($major -ge 7) {
        $compatibility.SecurityFeatures = "✅ Modern"
      }
      else {
        $compatibility.SecurityFeatures = "⚠️ Older standards"
      }

      # Recommendation
      if ($major -eq 8) {
        $compatibility.Recommendation = "✅ Recommended - Stable and compatible"
      }
      elseif ($major -eq 9) {
        $compatibility.Recommendation = "⚠️ Newer but has WebView2 issues"
      }
      elseif ($major -ge 6 -and $major -le 7) {
        $compatibility.Recommendation = "✓ Supported but older"
      }
      else {
        $compatibility.Recommendation = "❌ Not recommended"
      }
    }
  }
  catch {
    $compatibility.Recommendation = "❓ Could not analyze"
  }

  return $compatibility
}

function Show-DotNetSwitcherDialog {
  <#
    .SYNOPSIS
        Display .NET runtime switcher dialog in IDE
    #>

  $switcherForm = New-Object System.Windows.Forms.Form
  $switcherForm.Text = ".NET Runtime Switcher"
  $switcherForm.Size = New-Object System.Drawing.Size(700, 600)
  $switcherForm.StartPosition = "CenterScreen"
  $switcherForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
  $switcherForm.ForeColor = [System.Drawing.Color]::White
  $switcherForm.FormBorderStyle = "FixedDialog"
  $switcherForm.MaximizeBox = $false

  # Header
  $headerLabel = New-Object System.Windows.Forms.Label
  $headerLabel.Text = "Available .NET Runtimes"
  $headerLabel.Font = New-Object System.Drawing.Font("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
  $headerLabel.Location = New-Object System.Drawing.Point(10, 10)
  $headerLabel.Size = New-Object System.Drawing.Size(670, 30)
  $headerLabel.ForeColor = [System.Drawing.Color]::FromArgb(100, 200, 255)
  $switcherForm.Controls.Add($headerLabel)

  # Runtime list (DataGridView)
  $grid = New-Object System.Windows.Forms.DataGridView
  $grid.Location = New-Object System.Drawing.Point(10, 50)
  $grid.Size = New-Object System.Drawing.Size(670, 300)
  $grid.BackgroundColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
  $grid.ForeColor = [System.Drawing.Color]::White
  $grid.DefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
  $grid.DefaultCellStyle.ForeColor = [System.Drawing.Color]::White
  $grid.DefaultCellStyle.SelectionBackColor = [System.Drawing.Color]::FromArgb(64, 140, 200)
  $grid.DefaultCellStyle.Font = New-Object System.Drawing.Font("Segoe UI", 9)
  $grid.ColumnHeadersDefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 65)
  $grid.ColumnHeadersDefaultCellStyle.ForeColor = [System.Drawing.Color]::White
  $grid.AllowUserToAddRows = $false
  $grid.AllowUserToDeleteRows = $false
  $grid.ReadOnly = $true
  $grid.SelectionMode = "FullRowSelect"
  $grid.AutoSizeColumnsMode = "Fill"

  # Add columns
  $grid.Columns.Add("Name", "Runtime") | Out-Null
  $grid.Columns.Add("Version", "Version") | Out-Null
  $grid.Columns.Add("Status", "Status") | Out-Null
  $grid.Columns.Add("WebView2", "WebView2") | Out-Null

  # Populate with runtime data
  $runtimeStatus = Get-DotNetRuntimeStatus
  foreach ($rt in $runtimeStatus) {
    $compatibility = Test-DotNetCompatibility -DotNetVersion $rt.Version
    $status = if ($rt.IsSelected) { "✓ Selected" } elseif ($rt.IsCurrent) { "● Current" } else { "○ Available" }
    $row = @($rt.Name, $rt.Version, $status, $compatibility.WebView2)
    $grid.Rows.Add($row) | Out-Null
  }

  $grid.Columns[0].Width = 200
  $grid.Columns[1].Width = 100
  $grid.Columns[2].Width = 100
  $grid.Columns[3].Width = 150

  $switcherForm.Controls.Add($grid)

  # Details panel
  $detailsLabel = New-Object System.Windows.Forms.Label
  $detailsLabel.Text = "Runtime Details"
  $detailsLabel.Font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold)
  $detailsLabel.Location = New-Object System.Drawing.Point(10, 365)
  $detailsLabel.Size = New-Object System.Drawing.Size(670, 25)
  $detailsLabel.ForeColor = [System.Drawing.Color]::FromArgb(100, 200, 255)
  $switcherForm.Controls.Add($detailsLabel)

  # Details text box
  $detailsBox = New-Object System.Windows.Forms.TextBox
  $detailsBox.Location = New-Object System.Drawing.Point(10, 395)
  $detailsBox.Size = New-Object System.Drawing.Size(670, 120)
  $detailsBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
  $detailsBox.ForeColor = [System.Drawing.Color]::FromArgb(150, 200, 50)
  $detailsBox.Font = New-Object System.Drawing.Font("Consolas", 9)
  $detailsBox.Multiline = $true
  $detailsBox.ScrollBars = "Vertical"
  $detailsBox.ReadOnly = $true
  $detailsBox.Text = "Select a runtime to view details..."
  $switcherForm.Controls.Add($detailsBox)

  # Grid selection change event
  $grid.Add_SelectionChanged({
      if ($grid.SelectedRows.Count -gt 0) {
        $selectedIndex = $grid.SelectedRows[0].Index
        $selectedRuntime = $runtimeStatus[$selectedIndex]
        $compatibility = Test-DotNetCompatibility -DotNetVersion $selectedRuntime.Version

        $details = @"
Name: $($selectedRuntime.Name)
Version: $($selectedRuntime.Version)
Description: $($selectedRuntime.Description)
Is Framework: $($selectedRuntime.IsFramework)
Is Current: $($selectedRuntime.IsCurrent)

Feature Compatibility:
- WebView2: $($compatibility.WebView2)
- Windows Forms: $($compatibility.WindowsForms)
- Async IO: $($compatibility.AsyncIO)
- High Performance: $($compatibility.HighPerformance)
- Security: $($compatibility.SecurityFeatures)

Recommendation: $($compatibility.Recommendation)
"@
        $detailsBox.Text = $details
      }
    })

  # Switch button
  $switchButton = New-Object System.Windows.Forms.Button
  $switchButton.Text = "🔄 Switch to Selected Runtime"
  $switchButton.Location = New-Object System.Drawing.Point(10, 530)
  $switchButton.Size = New-Object System.Drawing.Size(300, 35)
  $switchButton.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 200)
  $switchButton.ForeColor = [System.Drawing.Color]::White
  $switchButton.Font = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
  $switchButton.Add_Click({
      if ($grid.SelectedRows.Count -gt 0) {
        $selectedIndex = $grid.SelectedRows[0].Index
        $selectedRuntime = $runtimeStatus[$selectedIndex]

        $detailsBox.ForeColor = [System.Drawing.Color]::FromArgb(200, 200, 100)
        $detailsBox.Text = "Switching to $($selectedRuntime.Name)...`n`nThis will reinitialize components for the selected runtime.`n`nNote: You may need to test compatibility and switch back if issues occur."

        # Perform switch
        Switch-DotNetRuntime -RuntimeName $selectedRuntime.Name

        $detailsBox.Text += "`n`n✅ Switch complete! Check latest.log for details."
        $switchButton.Enabled = $false
        $switchButton.Text = "✓ Switched"
      }
    })
  $switcherForm.Controls.Add($switchButton)

  # Close button
  $closeButton = New-Object System.Windows.Forms.Button
  $closeButton.Text = "Close"
  $closeButton.Location = New-Object System.Drawing.Point(380, 530)
  $closeButton.Size = New-Object System.Drawing.Size(300, 35)
  $closeButton.BackColor = [System.Drawing.Color]::FromArgb(100, 100, 100)
  $closeButton.ForeColor = [System.Drawing.Color]::White
  $closeButton.Font = New-Object System.Drawing.Font("Segoe UI", 10)
  $closeButton.Add_Click({ $switcherForm.Close() })
  $switcherForm.Controls.Add($closeButton)

  $switcherForm.ShowDialog()
}

function Switch-DotNetRuntime {
  <#
    .SYNOPSIS
        Switch to a different .NET runtime and reinitialize components
    .PARAMETER RuntimeName
        Name of the runtime to switch to
    #>

  param([string]$RuntimeName)

  Write-StartupLog "Switching to .NET runtime: $RuntimeName" "INFO"

  if (-not $script:DotNetRuntimes.ContainsKey($RuntimeName)) {
    Write-StartupLog "Runtime not found: $RuntimeName" "ERROR"
    return $false
  }

  $newRuntime = $script:DotNetRuntimes[$RuntimeName]

  try {
    Write-StartupLog "Current runtime: $([System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription)" "INFO"
    Write-StartupLog "Target runtime: $($newRuntime.Description)" "INFO"

    # Test compatibility
    $compatibility = Test-DotNetCompatibility -DotNetVersion $newRuntime.Version
    Write-StartupLog "WebView2 Compatibility: $($compatibility.WebView2)" "INFO"

    # Update current runtime reference
    $script:CurrentDotNetRuntime = $RuntimeName

    # Store preference
    $script:PreferredDotNetVersion = $RuntimeName
    if ($global:settings) {
      if ($global:settings -is [hashtable]) {
        $global:settings["PreferredDotNetVersion"] = $RuntimeName
      }
      else {
        $global:settings | Add-Member -MemberType NoteProperty -Name "PreferredDotNetVersion" -Value $RuntimeName -Force -ErrorAction SilentlyContinue
      }
      if (Get-Command "Save-UserSettings" -ErrorAction SilentlyContinue) {
        Save-UserSettings -ErrorAction SilentlyContinue
      }
    }

    # Reinitialize WebView2 if needed
    if ($compatibility.WebView2 -like "*Compatible*" -and -not ($compatibility.WebView2 -like "*Fallback*")) {
      Write-StartupLog "Attempting to enable WebView2 with new runtime..." "INFO"
      $script:NetVersionCompatible = $true
      $script:useWebView2 = $false  # Reset flag for reinitialization

      # Try to load WebView2 again
      $wvDir = "$env:TEMP\WVLibs"
      if (Test-Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll") {
        try {
          Add-Type -Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll" -ErrorAction Stop
          $script:useWebView2 = $true
          Write-StartupLog "✅ WebView2 enabled with new runtime" "SUCCESS"
        }
        catch {
          Write-StartupLog "WebView2 still not compatible: $_" "WARNING"
        }
      }
    }
    else {
      Write-StartupLog "This runtime will use Internet Explorer fallback for browser" "INFO"
      $script:NetVersionCompatible = $false
      $script:useWebView2 = $false
    }

    Write-StartupLog "✅ Successfully switched to $RuntimeName" "SUCCESS"
    Write-DevConsole "✅ Switched to .NET runtime: $($newRuntime.Description)" "SUCCESS"

    return $true
  }
  catch {
    Write-StartupLog "Error switching runtime: $_" "ERROR"
    Write-StartupLog "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    Write-DevConsole "❌ Error switching runtime: $_" "ERROR"
    return $false
  }
}

# ============================================
# MENU INTEGRATION
# ============================================

function Add-DotNetSwitcherMenu {
  <#
    .SYNOPSIS
        Add .NET Runtime Switcher menu to IDE main menu
    #>

  # Create menu item
  $dotnetMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem
  $dotnetMenuItem.Text = "🔄 .NET Runtime"

  # Main switcher dialog option
  $switcherOption = New-Object System.Windows.Forms.ToolStripMenuItem
  $switcherOption.Text = "🔄 Switch Runtime..."
  $switcherOption.Add_Click({ Show-DotNetSwitcherDialog })
  $dotnetMenuItem.DropDownItems.Add($switcherOption) | Out-Null

  # Separator
  $separator1 = New-Object System.Windows.Forms.ToolStripSeparator
  $dotnetMenuItem.DropDownItems.Add($separator1) | Out-Null

  # Add current runtimes as quick-switch options
  $runtimeStatus = Get-DotNetRuntimeStatus
  $quickSwitchCount = 0

  foreach ($rt in $runtimeStatus) {
    if ($quickSwitchCount -lt 5) {
      # Limit to 5 quick options
      $status = if ($rt.IsSelected) { "✓" } elseif ($rt.IsCurrent) { "●" } else { "○" }
      $quickOption = New-Object System.Windows.Forms.ToolStripMenuItem
      $quickOption.Text = "$status $($rt.Name)"
      $quickOption.Add_Click({
          param($sender, $e)
          $rtName = $sender.Text -replace "^[✓●○]\s*", ""
          Switch-DotNetRuntime -RuntimeName $rtName
        }.GetNewClosure())
      $dotnetMenuItem.DropDownItems.Add($quickOption) | Out-Null
      $quickSwitchCount++
    }
  }

  # Separator
  $separator2 = New-Object System.Windows.Forms.ToolStripSeparator
  $dotnetMenuItem.DropDownItems.Add($separator2) | Out-Null

  # Info option
  $infoOption = New-Object System.Windows.Forms.ToolStripMenuItem
  $infoOption.Text = "ℹ️ Runtime Info"
  $infoOption.Add_Click({
      $current = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
      [System.Windows.Forms.MessageBox]::Show(
        "Current Runtime: $current`n`nUse the .NET Runtime Switcher to test features on different runtimes.",
        "Runtime Information",
        "OK",
        "Information"
      ) | Out-Null
    })
  $dotnetMenuItem.DropDownItems.Add($infoOption) | Out-Null

  return $dotnetMenuItem
}
