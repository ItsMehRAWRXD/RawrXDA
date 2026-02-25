# Desktop Copilot - Quick Reference

## 🚀 Getting Started

### Option 1: Use Pre-Built Desktop Copilot
```powershell
# Run the base desktop copilot
.\desktop_copilot.ps1 -AutoStart -EnableVoice

# Add to startup for automatic launch
.\desktop_copilot.ps1 -AutoStart
```

### Option 2: Generate Custom Copilot
```powershell
# Interactive menu
.\copilot_generator.ps1

# Generate specific template
.\copilot_generator.ps1 -Template Gaming
.\copilot_generator.ps1 -Template FileOrganizer
.\copilot_generator.ps1 -Template Security
```

## 🎯 What Can You Build?

| Copilot Type | Purpose | Key Features |
|-------------|---------|--------------|
| **Gaming** | Optimize for gaming | Close apps, boost FPS, monitor temps |
| **File Organizer** | Auto-organize files | Sort downloads, find duplicates, clean |
| **Security** | Monitor threats | Detect suspicious activity, alerts |
| **Backup** | Automated backups | Schedule, verify, restore |
| **Health** | Wellness reminders | Breaks, hydration, screen time |
| **Network** | Monitor bandwidth | Track usage, speed tests, alerts |

## ⚙️ System Access

Desktop copilots have **FULL** system access:

```powershell
# File System
Get-ChildItem, Copy-Item, Move-Item, Remove-Item

# Processes
Get-Process, Stop-Process, Start-Process

# Network
Get-NetAdapter, Test-Connection, Get-NetTCPConnection

# System Info
Get-WmiObject, Get-CimInstance

# Registry
Get-ItemProperty, Set-ItemProperty

# Performance
Get-Counter, Measure-Command
```

## 🔧 Customization Examples

### Add Custom Menu Item
```powershell
[void] InitializeTrayIcon() {
    # ... existing code ...
    
    $customItem = $this.TrayMenu.Items.Add("My Custom Action")
    $customItem.add_Click({ $this.MyCustomMethod() }.GetNewClosure())
}

[void] MyCustomMethod() {
    # Your custom logic here
    Write-Host "Custom action!"
}
```

### Add Background Monitor
```powershell
[void] StartMonitoring() {
    $timer = New-Object System.Windows.Forms.Timer
    $timer.Interval = 5000  # 5 seconds
    $timer.add_Tick({
        # Check something every 5 seconds
        $cpu = (Get-WmiObject Win32_Processor).LoadPercentage
        if ($cpu -gt 90) {
            $this.Alert("High CPU!")
        }
    }.GetNewClosure())
    $timer.Start()
}
```

### Add Custom Hotkey (Win32 API)
```powershell
Add-Type @"
using System;
using System.Runtime.InteropServices;

public class Hotkey {
    [DllImport("user32.dll")]
    public static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);
}
"@

# Register Ctrl+Shift+X
[Hotkey]::RegisterHotKey($MainWindow.Handle, 1, 0x0002 -bor 0x0004, 0x58)
```

## 📊 Real-World Examples

### Gaming Performance Booster
```powershell
[void] OptimizeForGaming() {
    # Close resource hogs
    'chrome','teams','discord' | ForEach-Object {
        Get-Process $_ -EA SilentlyContinue | Stop-Process -Force
    }
    
    # High performance mode
    powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
    
    # Disable notifications
    Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\PushNotifications' -Name 'ToastEnabled' -Value 0
    
    $this.Notify("🎮 Gaming mode activated!")
}
```

### Smart File Organizer
```powershell
[void] OrganizeDownloads() {
    $downloads = "$env:USERPROFILE\Downloads"
    
    $categories = @{
        'Work'   = @('*.docx', '*.xlsx', '*.pdf')
        'Media'  = @('*.jpg', '*.png', '*.mp4')
        'Code'   = @('*.ps1', '*.py', '*.js')
    }
    
    foreach ($cat in $categories.Keys) {
        $target = Join-Path $downloads $cat
        New-Item -ItemType Directory -Path $target -Force | Out-Null
        
        foreach ($ext in $categories[$cat]) {
            Get-ChildItem $downloads -Filter $ext |
                Move-Item -Destination $target -Force
        }
    }
}
```

### Security Monitor
```powershell
[void] MonitorSecurity() {
    while ($true) {
        # Check for suspicious processes
        $suspicious = @('mimikatz', 'psexec')
        $found = Get-Process | Where-Object { $suspicious -contains $_.Name }
        
        if ($found) {
            $this.Alert("🚨 Suspicious process: $($found.Name)")
            $found | Stop-Process -Force
        }
        
        # Check for unusual network activity
        $external = Get-NetTCPConnection |
            Where-Object { $_.State -eq 'Established' -and 
                          $_.RemoteAddress -notmatch '^(192\.168|10\.)' }
        
        if ($external.Count -gt 20) {
            $this.Alert("⚠️ High number of external connections!")
        }
        
        Start-Sleep -Seconds 10
    }
}
```

## 🎨 UI Customization

### Change Colors
```powershell
# Main window
$MainWindow.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)  # Dark

# Buttons
$button.BackColor = [System.Drawing.Color]::FromArgb(76, 175, 80)  # Green
$button.ForeColor = [System.Drawing.Color]::White
$button.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
```

### Custom Icon
```powershell
# From file
$TrayIcon.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon("path\to\icon.ico")

# From image
$bmp = New-Object System.Drawing.Bitmap("path\to\image.png")
$TrayIcon.Icon = [System.Drawing.Icon]::FromHandle($bmp.GetHicon())
```

### Notifications
```powershell
# Info
$TrayIcon.ShowBalloonTip(3000, "Title", "Message", [System.Windows.Forms.ToolTipIcon]::Info)

# Warning
$TrayIcon.ShowBalloonTip(5000, "Warning", "Be careful!", [System.Windows.Forms.ToolTipIcon]::Warning)

# Error
$TrayIcon.ShowBalloonTip(5000, "Error", "Something failed!", [System.Windows.Forms.ToolTipIcon]::Error)
```

## 📦 Distribution

### Package as EXE
```powershell
# Install PS2EXE
Install-Module PS2EXE -Scope CurrentUser

# Convert to EXE
Invoke-PS2EXE `
    -InputFile "my_copilot.ps1" `
    -OutputFile "MyCopilot.exe" `
    -NoConsole `
    -Title "My Copilot" `
    -Description "Personal assistant" `
    -Version "1.0.0.0" `
    -IconFile "icon.ico"
```

### Add to Startup
```powershell
$startupFolder = [Environment]::GetFolderPath('Startup')
$shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut("$startupFolder\MyCopilot.lnk")
$shortcut.TargetPath = "pwsh.exe"
$shortcut.Arguments = "-WindowStyle Hidden -File `"$PSScriptRoot\my_copilot.ps1`""
$shortcut.Save()
```

## 🔌 Integration

### Web API
```powershell
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:9999/")
$listener.Start()

while ($true) {
    $context = $listener.GetContext()
    
    if ($context.Request.Url.LocalPath -eq "/status") {
        $response = "OK" | ConvertTo-Json
        $buffer = [Text.Encoding]::UTF8.GetBytes($response)
        $context.Response.ContentLength64 = $buffer.Length
        $context.Response.OutputStream.Write($buffer, 0, $buffer.Length)
    }
    
    $context.Response.Close()
}
```

### Discord Webhook
```powershell
function Send-Discord([string]$message) {
    $webhook = "YOUR_WEBHOOK_URL"
    $payload = @{ content = $message } | ConvertTo-Json
    Invoke-RestMethod -Uri $webhook -Method Post -Body $payload -ContentType 'application/json'
}
```

### Email Alerts
```powershell
function Send-Alert([string]$subject, [string]$body) {
    Send-MailMessage `
        -From "copilot@example.com" `
        -To "you@example.com" `
        -Subject $subject `
        -Body $body `
        -SmtpServer "smtp.gmail.com" `
        -Port 587 `
        -UseSsl `
        -Credential (Get-Credential)
}
```

## 💡 Pro Tips

### Persistent Data
```powershell
# Save data
$data = @{ LastRun = Get-Date; Count = 42 }
$data | ConvertTo-Json | Set-Content "$env:APPDATA\MyCopilot\data.json"

# Load data
$data = Get-Content "$env:APPDATA\MyCopilot\data.json" | ConvertFrom-Json
```

### Logging
```powershell
function Write-Log([string]$message) {
    $logFile = "$env:APPDATA\MyCopilot\log.txt"
    "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') | $message" | 
        Add-Content -Path $logFile
}
```

### Error Handling
```powershell
try {
    # Risky operation
    Get-Process "nonexistent" -ErrorAction Stop
}
catch {
    Write-Log "Error: $_"
    $TrayIcon.ShowBalloonTip(3000, "Error", $_.Exception.Message, [System.Windows.Forms.ToolTipIcon]::Error)
}
```

## 🎯 Use Cases Summary

✅ **ANY repetitive task** - Automate it  
✅ **ANY system monitoring** - Track it  
✅ **ANY file operation** - Organize it  
✅ **ANY notification** - Get alerted  
✅ **ANY schedule** - Automate it  

**The key**: Desktop copilots aren't just for coding - they're for ANYTHING you need help with on your computer!

## 📚 Resources

- **Generated copilots**: `D:\lazy init ide\generated_copilots\`
- **Documentation**: `D:\lazy init ide\docs\DESKTOP_COPILOT_CUSTOMIZATION_GUIDE.md`
- **Base template**: `D:\lazy init ide\desktop\desktop_copilot.ps1`
- **Generator**: `D:\lazy init ide\desktop\copilot_generator.ps1`

## 🚀 Quick Start Commands

```powershell
# Try the base copilot
cd "D:\lazy init ide\desktop"
.\desktop_copilot.ps1

# Generate your own
.\copilot_generator.ps1

# Gaming copilot
.\copilot_generator.ps1 -Template Gaming

# Custom copilot
.\copilot_generator.ps1 -Template Custom
```

---

**Remember**: A desktop copilot is just a PowerShell script with system tray UI and full system access. Customize it for YOUR needs!
