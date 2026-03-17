#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Copilot Template Generator - Create Custom Desktop Copilots

.DESCRIPTION
    Interactive tool to generate custom desktop copilot templates for any purpose.
    Choose from pre-built templates or create your own from scratch.

.PARAMETER Template
    Template type to generate (Gaming, FileOrganizer, Security, Backup, Health, Network, Custom)

.PARAMETER OutputPath
    Where to save the generated copilot

.EXAMPLE
    .\copilot_generator.ps1 -Template Gaming
    .\copilot_generator.ps1 -Template Custom -OutputPath "D:\MyCustomCopilot"
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Gaming', 'FileOrganizer', 'Security', 'Backup', 'Health', 'Network', 'Custom', 'Interactive')]
    [string]$Template = 'Interactive',
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = "$PSScriptRoot\..\generated_copilots"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# COPILOT TEMPLATE GENERATOR
# ═══════════════════════════════════════════════════════════════════════════════

class CopilotGenerator {
    [string]$OutputPath
    [hashtable]$Config
    
    CopilotGenerator([string]$outputPath) {
        $this.OutputPath = $outputPath
        $this.Config = @{}
        
        if (-not (Test-Path $this.OutputPath)) {
            New-Item -ItemType Directory -Path $this.OutputPath -Force | Out-Null
        }
    }
    
    [void] ShowMenu() {
        Clear-Host
        Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║       DESKTOP COPILOT TEMPLATE GENERATOR                     ║" -ForegroundColor Cyan
        Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
        
        Write-Host "Select a copilot template to generate:`n" -ForegroundColor Yellow
        
        Write-Host "  1. 🎮 Gaming Performance Copilot" -ForegroundColor Green
        Write-Host "     → Optimize system for gaming, monitor FPS, manage processes`n"
        
        Write-Host "  2. 📁 File Organizer Copilot" -ForegroundColor Green
        Write-Host "     → Auto-organize downloads, find duplicates, clean folders`n"
        
        Write-Host "  3. 🔐 Security Watcher Copilot" -ForegroundColor Green
        Write-Host "     → Monitor for threats, detect suspicious activity, alerts`n"
        
        Write-Host "  4. 💾 Backup Automation Copilot" -ForegroundColor Green
        Write-Host "     → Automated backups, versioning, integrity checks`n"
        
        Write-Host "  5. 💪 Health & Wellness Copilot" -ForegroundColor Green
        Write-Host "     → Reminders for breaks, hydration, posture, screen time`n"
        
        Write-Host "  6. 🌐 Network Monitor Copilot" -ForegroundColor Green
        Write-Host "     → Bandwidth tracking, connection diagnostics, usage alerts`n"
        
        Write-Host "  7. 🎨 Custom Copilot (Blank Template)" -ForegroundColor Green
        Write-Host "     → Start from scratch with base structure`n"
        
        Write-Host "  8. 📚 View Examples & Documentation" -ForegroundColor Cyan
        Write-Host "  9. 🚀 Generate All Templates" -ForegroundColor Magenta
        Write-Host "  0. ❌ Exit`n" -ForegroundColor Red
        
        $choice = Read-Host "Enter your choice (0-9)"
        
        switch ($choice) {
            '1' { $this.GenerateGamingCopilot() }
            '2' { $this.GenerateFileOrganizerCopilot() }
            '3' { $this.GenerateSecurityCopilot() }
            '4' { $this.GenerateBackupCopilot() }
            '5' { $this.GenerateHealthCopilot() }
            '6' { $this.GenerateNetworkCopilot() }
            '7' { $this.GenerateCustomCopilot() }
            '8' { $this.ShowDocumentation() }
            '9' { $this.GenerateAllTemplates() }
            '0' { exit }
            default { 
                Write-Host "`n❌ Invalid choice!" -ForegroundColor Red
                Start-Sleep -Seconds 2
                $this.ShowMenu()
            }
        }
    }
    
    [void] GenerateGamingCopilot() {
        Write-Host "`n🎮 Generating Gaming Performance Copilot...`n" -ForegroundColor Green
        
        $this.Config = @{
            Name = "Gaming Performance Copilot"
            Description = "Optimize your system for maximum gaming performance"
            IconColor = "Green"
            Features = @(
                "Close background processes",
                "Enable high performance mode",
                "Monitor GPU/CPU temps",
                "Track FPS in real-time",
                "Optimize network settings",
                "Disable Windows notifications"
            )
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetGamingMethods()
        
        $outputFile = Join-Path $this.OutputPath "gaming_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "✅ Gaming Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateFileOrganizerCopilot() {
        Write-Host "`n📁 Generating File Organizer Copilot...`n" -ForegroundColor Green
        
        $this.Config = @{
            Name = "File Organizer Copilot"
            Description = "Keep your files organized automatically"
            IconColor = "Blue"
            Features = @(
                "Auto-organize downloads by type",
                "Find and remove duplicates",
                "Clean old temp files",
                "Smart folder creation",
                "File search and locate",
                "Size analysis reports"
            )
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetFileOrganizerMethods()
        
        $outputFile = Join-Path $this.OutputPath "file_organizer_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "✅ File Organizer Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateSecurityCopilot() {
        Write-Host "`n🔐 Generating Security Watcher Copilot...`n" -ForegroundColor Green
        
        $this.Config = @{
            Name = "Security Watcher Copilot"
            Description = "Monitor and protect your system from threats"
            IconColor = "Red"
            Features = @(
                "Monitor suspicious processes",
                "Track network connections",
                "Detect unauthorized access",
                "Alert on security events",
                "Quarantine threats",
                "Security audit reports"
            )
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetSecurityMethods()
        
        $outputFile = Join-Path $this.OutputPath "security_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "✅ Security Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateBackupCopilot() {
        Write-Host "`n💾 Generating Backup Automation Copilot...`n" -ForegroundColor Green
        
        $this.Config = @{
            Name = "Backup Automation Copilot"
            Description = "Automated backups with versioning and verification"
            IconColor = "Orange"
            Features = @(
                "Scheduled backups",
                "Incremental backup support",
                "Multi-destination backup",
                "Integrity verification",
                "Restore functionality",
                "Backup size reports"
            )
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetBackupMethods()
        
        $outputFile = Join-Path $this.OutputPath "backup_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "✅ Backup Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateHealthCopilot() {
        Write-Host "`n💪 Generating Health & Wellness Copilot...`n" -ForegroundColor Green
        
        $this.Config = @{
            Name = "Health & Wellness Copilot"
            Description = "Take care of your health while using computer"
            IconColor = "Purple"
            Features = @(
                "Break reminders (20-20-20 rule)",
                "Hydration reminders",
                "Posture check notifications",
                "Screen time tracking",
                "Blue light warnings",
                "Daily wellness report"
            )
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetHealthMethods()
        
        $outputFile = Join-Path $this.OutputPath "health_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "✅ Health Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateNetworkCopilot() {
        Write-Host "`n🌐 Generating Network Monitor Copilot...`n" -ForegroundColor Green
        
        $this.Config = @{
            Name = "Network Monitor Copilot"
            Description = "Monitor and analyze network activity"
            IconColor = "Cyan"
            Features = @(
                "Real-time bandwidth monitoring",
                "Per-application traffic tracking",
                "Connection diagnostics",
                "Network speed tests",
                "Usage alerts and caps",
                "Daily traffic reports"
            )
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetNetworkMethods()
        
        $outputFile = Join-Path $this.OutputPath "network_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "✅ Network Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateCustomCopilot() {
        Write-Host "`n🎨 Custom Copilot Generator`n" -ForegroundColor Magenta
        
        $name = Read-Host "Enter copilot name (e.g., 'My Custom Assistant')"
        $description = Read-Host "Enter description"
        
        Write-Host "`nWhat should your copilot do? (comma-separated features)" -ForegroundColor Yellow
        $featuresInput = Read-Host "Features"
        $features = $featuresInput -split ',' | ForEach-Object { $_.Trim() }
        
        $this.Config = @{
            Name = $name
            Description = $description
            IconColor = "Blue"
            Features = $features
        }
        
        $script = $this.GetBaseTemplate()
        $script += $this.GetCustomMethods()
        
        $fileName = $name -replace '[^a-zA-Z0-9]', '_'
        $outputFile = Join-Path $this.OutputPath "$($fileName.ToLower())_copilot.ps1"
        Set-Content -Path $outputFile -Value $script
        
        Write-Host "`n✅ Custom Copilot generated: $outputFile" -ForegroundColor Green
        $this.PostGenerationMenu($outputFile)
    }
    
    [void] GenerateAllTemplates() {
        Write-Host "`n🚀 Generating all copilot templates...`n" -ForegroundColor Magenta
        
        $this.GenerateGamingCopilot()
        $this.GenerateFileOrganizerCopilot()
        $this.GenerateSecurityCopilot()
        $this.GenerateBackupCopilot()
        $this.GenerateHealthCopilot()
        $this.GenerateNetworkCopilot()
        
        Write-Host "`n✅ All templates generated in: $($this.OutputPath)" -ForegroundColor Green
        Write-Host "`nPress any key to return to menu..." -ForegroundColor Yellow
        $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
        $this.ShowMenu()
    }
    
    [void] ShowDocumentation() {
        Clear-Host
        Write-Host "`n📚 DESKTOP COPILOT DOCUMENTATION`n" -ForegroundColor Cyan
        
        Write-Host "What is a Desktop Copilot?" -ForegroundColor Yellow
        Write-Host "A taskbar assistant with full system access that helps with ANY task.`n"
        
        Write-Host "Key Features:" -ForegroundColor Yellow
        Write-Host "  • Runs in system tray (always accessible)"
        Write-Host "  • Global hotkey (Ctrl+Shift+A)"
        Write-Host "  • Full system access (files, processes, network, registry)"
        Write-Host "  • Background monitoring"
        Write-Host "  • Smart notifications"
        Write-Host "  • Customizable for any purpose`n"
        
        Write-Host "How to Use Generated Copilots:" -ForegroundColor Yellow
        Write-Host "  1. Run the generated .ps1 file"
        Write-Host "  2. Copilot appears in system tray"
        Write-Host "  3. Right-click tray icon for menu"
        Write-Host "  4. Double-click to open main window"
        Write-Host "  5. Customize the code to add your features`n"
        
        Write-Host "Customization Tips:" -ForegroundColor Yellow
        Write-Host "  • Edit the ProcessQuery() method to handle specific questions"
        Write-Host "  • Add new menu items in InitializeTrayIcon()"
        Write-Host "  • Create custom monitoring loops"
        Write-Host "  • Add web API integration for remote control"
        Write-Host "  • Package as .exe for distribution`n"
        
        Write-Host "Examples:" -ForegroundColor Yellow
        Write-Host "  • Gaming: Close background apps, monitor temps"
        Write-Host "  • Files: Auto-organize downloads, find duplicates"
        Write-Host "  • Security: Monitor processes, alert on threats"
        Write-Host "  • Backup: Schedule backups, verify integrity"
        Write-Host "  • Health: Remind breaks, track screen time"
        Write-Host "  • Network: Monitor bandwidth, test speed`n"
        
        Write-Host "Press any key to return to menu..." -ForegroundColor Yellow
        $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
        $this.ShowMenu()
    }
    
    [void] PostGenerationMenu([string]$filePath) {
        Write-Host "`nWhat would you like to do?`n" -ForegroundColor Yellow
        Write-Host "  1. Run the copilot now"
        Write-Host "  2. Open in editor"
        Write-Host "  3. View generated code"
        Write-Host "  4. Generate another copilot"
        Write-Host "  5. Back to main menu`n"
        
        $choice = Read-Host "Choice"
        
        switch ($choice) {
            '1' { 
                Write-Host "`n▶️  Launching copilot..." -ForegroundColor Green
                Start-Process pwsh -ArgumentList "-NoExit -File `"$filePath`""
                $this.ShowMenu()
            }
            '2' { 
                Write-Host "`n📝 Opening in editor..." -ForegroundColor Green
                code $filePath
                $this.ShowMenu()
            }
            '3' { 
                Clear-Host
                Write-Host "`n📄 Generated Code:`n" -ForegroundColor Cyan
                Get-Content $filePath | Select-Object -First 50
                Write-Host "`n... (truncated, open file to see full code) ...`n" -ForegroundColor Gray
                Write-Host "Press any key to continue..." -ForegroundColor Yellow
                $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
                $this.ShowMenu()
            }
            '4' { $this.ShowMenu() }
            '5' { $this.ShowMenu() }
            default { $this.ShowMenu() }
        }
    }
    
    [string] GetBaseTemplate() {
        return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $($this.Config.Name)

.DESCRIPTION
    $($this.Config.Description)
    
    Features:
$(($this.Config.Features | ForEach-Object { "    • $_" }) -join "`n")

.PARAMETER AutoStart
    Start in system tray automatically

.EXAMPLE
    .\$(($this.Config.Name -replace '[^a-zA-Z0-9]', '_').ToLower()).ps1 -AutoStart
#>

param(
    [Parameter(Mandatory=`$false)]
    [switch]`$AutoStart,
    
    [Parameter(Mandatory=`$false)]
    [switch]`$Silent
)

Set-StrictMode -Version Latest
`$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

class CustomCopilot {
    [System.Windows.Forms.NotifyIcon]`$TrayIcon
    [System.Windows.Forms.ContextMenuStrip]`$TrayMenu
    [System.Windows.Forms.Form]`$MainWindow
    [hashtable]`$Data
    
    CustomCopilot() {
        `$this.Data = @{}
        `$this.InitializeTrayIcon()
        `$this.InitializeMainWindow()
    }
    
    [void] InitializeTrayIcon() {
        `$this.TrayIcon = New-Object System.Windows.Forms.NotifyIcon
        `$this.TrayIcon.Icon = [System.Drawing.SystemIcons]::Application
        `$this.TrayIcon.Text = "$($this.Config.Name)"
        `$this.TrayIcon.Visible = `$true
        
        `$this.TrayMenu = New-Object System.Windows.Forms.ContextMenuStrip
        
        `$openItem = `$this.TrayMenu.Items.Add("Open")
        `$openItem.add_Click({ `$this.ShowMainWindow() }.GetNewClosure())
        
        `$this.TrayMenu.Items.Add("-")
        
        # Add custom menu items here
        
        `$exitItem = `$this.TrayMenu.Items.Add("Exit")
        `$exitItem.add_Click({ `$this.ExitApplication() }.GetNewClosure())
        
        `$this.TrayIcon.ContextMenuStrip = `$this.TrayMenu
        `$this.TrayIcon.add_DoubleClick({ `$this.ShowMainWindow() }.GetNewClosure())
        
        `$this.TrayIcon.ShowBalloonTip(3000, "$($this.Config.Name) Ready", "Right-click tray icon for menu!", [System.Windows.Forms.ToolTipIcon]::Info)
    }
    
    [void] InitializeMainWindow() {
        `$this.MainWindow = New-Object System.Windows.Forms.Form
        `$this.MainWindow.Text = "$($this.Config.Name)"
        `$this.MainWindow.Size = New-Object System.Drawing.Size(800, 600)
        `$this.MainWindow.StartPosition = "CenterScreen"
        
        # Add your UI elements here
        
        `$this.MainWindow.add_FormClosing({
            param(`$sender, `$e)
            `$e.Cancel = `$true
            `$this.MainWindow.Hide()
        }.GetNewClosure())
    }
    
    [void] ShowMainWindow() {
        `$this.MainWindow.Show()
        `$this.MainWindow.Activate()
        `$this.MainWindow.BringToFront()
    }
    
    [void] ExitApplication() {
        `$this.TrayIcon.Visible = `$false
        `$this.TrayIcon.Dispose()
        [System.Windows.Forms.Application]::Exit()
    }
    
    [void] Run() {
        Write-Host "`n✅ $($this.Config.Name) running in system tray!" -ForegroundColor Green
        [System.Windows.Forms.Application]::Run()
    }
}

"@
    }
    
    [string] GetGamingMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # GAMING OPTIMIZATION METHODS
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] OptimizeForGaming() {
        Write-Host "🎮 Optimizing for gaming..." -ForegroundColor Green
        
        # Close resource-heavy processes
        `$processesToClose = @('chrome', 'firefox', 'teams', 'slack', 'discord')
        foreach (`$proc in `$processesToClose) {
            Get-Process `$proc -ErrorAction SilentlyContinue | Stop-Process -Force
        }
        
        # Set high performance power plan
        powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
        
        # Disable Windows notifications
        # Add registry tweaks here
        
        `$this.TrayIcon.ShowBalloonTip(3000, "Gaming Mode", "System optimized for gaming!", [System.Windows.Forms.ToolTipIcon]::Info)
    }
    
    [void] MonitorPerformance() {
        while (`$true) {
            `$cpu = (Get-WmiObject Win32_Processor).LoadPercentage
            `$gpu = # Get GPU usage
            
            if (`$cpu -gt 90) {
                `$this.TrayIcon.ShowBalloonTip(3000, "High CPU", "CPU usage is high!", [System.Windows.Forms.ToolTipIcon]::Warning)
            }
            
            Start-Sleep -Seconds 5
        }
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
    
    [string] GetFileOrganizerMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # FILE ORGANIZATION METHODS
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] OrganizeDownloads() {
        `$downloads = "`$env:USERPROFILE\Downloads"
        
        `$fileTypes = @{
            'Images' = @('*.jpg', '*.png', '*.gif')
            'Documents' = @('*.pdf', '*.docx', '*.txt')
            'Videos' = @('*.mp4', '*.avi', '*.mkv')
            'Archives' = @('*.zip', '*.rar', '*.7z')
        }
        
        foreach (`$type in `$fileTypes.Keys) {
            `$targetDir = Join-Path `$downloads `$type
            New-Item -ItemType Directory -Path `$targetDir -Force | Out-Null
            
            foreach (`$pattern in `$fileTypes[`$type]) {
                Get-ChildItem `$downloads -Filter `$pattern | Move-Item -Destination `$targetDir -Force
            }
        }
        
        `$this.TrayIcon.ShowBalloonTip(3000, "Downloads Organized", "Files sorted by type!", [System.Windows.Forms.ToolTipIcon]::Info)
    }
    
    [void] FindDuplicates() {
        `$files = Get-ChildItem -Recurse -File
        `$hashes = @{}
        `$duplicates = @()
        
        foreach (`$file in `$files) {
            `$hash = (Get-FileHash `$file.FullName -Algorithm MD5).Hash
            if (`$hashes.ContainsKey(`$hash)) {
                `$duplicates += `$file.FullName
            }
            else {
                `$hashes[`$hash] = `$file.FullName
            }
        }
        
        Write-Host "Found `$(`$duplicates.Count) duplicates!"
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
    
    [string] GetSecurityMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # SECURITY MONITORING METHODS
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] MonitorProcesses() {
        `$suspiciousNames = @('mimikatz', 'psexec', 'netcat')
        
        while (`$true) {
            `$processes = Get-Process
            foreach (`$proc in `$processes) {
                if (`$suspiciousNames -contains `$proc.ProcessName.ToLower()) {
                    `$this.Alert("⚠️ Suspicious process: `$(`$proc.ProcessName)")
                    `$proc | Stop-Process -Force
                }
            }
            Start-Sleep -Seconds 5
        }
    }
    
    [void] Alert([string]`$message) {
        `$this.TrayIcon.ShowBalloonTip(5000, "Security Alert", `$message, [System.Windows.Forms.ToolTipIcon]::Warning)
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
    
    [string] GetBackupMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # BACKUP AUTOMATION METHODS
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] PerformBackup() {
        `$sources = @("`$env:USERPROFILE\Documents", "`$env:USERPROFILE\Pictures")
        `$destination = "E:\Backups\`$(Get-Date -Format 'yyyy-MM-dd')"
        
        New-Item -ItemType Directory -Path `$destination -Force | Out-Null
        
        foreach (`$source in `$sources) {
            if (Test-Path `$source) {
                Copy-Item -Path `$source -Destination `$destination -Recurse -Force
            }
        }
        
        `$this.TrayIcon.ShowBalloonTip(3000, "Backup Complete", "Files backed up!", [System.Windows.Forms.ToolTipIcon]::Info)
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
    
    [string] GetHealthMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # HEALTH & WELLNESS METHODS
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] StartReminders() {
        while (`$true) {
            Start-Sleep -Seconds 1200  # 20 minutes
            `$this.TrayIcon.ShowBalloonTip(5000, "Eye Break", "Look away from screen for 20 seconds! 👀", [System.Windows.Forms.ToolTipIcon]::Info)
            
            Start-Sleep -Seconds 2400  # 40 more minutes
            `$this.TrayIcon.ShowBalloonTip(5000, "Hydration Reminder", "Time to drink water! 💧", [System.Windows.Forms.ToolTipIcon]::Info)
        }
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
    
    [string] GetNetworkMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # NETWORK MONITORING METHODS
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] MonitorBandwidth() {
        `$adapter = Get-NetAdapter | Where-Object Status -eq 'Up' | Select-Object -First 1
        
        while (`$true) {
            `$stats = Get-NetAdapterStatistics -Name `$adapter.Name
            Write-Host "Sent: `$(`$stats.SentBytes / 1MB) MB | Received: `$(`$stats.ReceivedBytes / 1MB) MB"
            Start-Sleep -Seconds 5
        }
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
    
    [string] GetCustomMethods() {
        return @"
    # ═══════════════════════════════════════════════════════════════════════════════
    # CUSTOM METHODS - ADD YOUR FUNCTIONALITY HERE
    # ═══════════════════════════════════════════════════════════════════════════════
    
    [void] CustomAction() {
        # Add your custom logic here
        Write-Host "Custom action executed!"
    }
}

`$copilot = [CustomCopilot]::new()
`$copilot.Run()
"@
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

$generator = [CopilotGenerator]::new($OutputPath)

if ($Template -eq 'Interactive') {
    $generator.ShowMenu()
}
else {
    switch ($Template) {
        'Gaming' { $generator.GenerateGamingCopilot() }
        'FileOrganizer' { $generator.GenerateFileOrganizerCopilot() }
        'Security' { $generator.GenerateSecurityCopilot() }
        'Backup' { $generator.GenerateBackupCopilot() }
        'Health' { $generator.GenerateHealthCopilot() }
        'Network' { $generator.GenerateNetworkCopilot() }
        'Custom' { $generator.GenerateCustomCopilot() }
    }
}
