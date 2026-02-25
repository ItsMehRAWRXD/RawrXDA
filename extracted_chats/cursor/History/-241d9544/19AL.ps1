# Auto Backup All AI Chats
# Automated daily backup system for all AI conversations

param(
    [string]$BackupPath = "D:\01-AI-Models\Chat-History\Auto-Backups",
    [string]$Schedule = "Daily",
    [switch]$CreateTask = $false,
    [switch]$RunNow = $false
)

Write-Host "🤖 Auto Backup All AI Chats" -ForegroundColor Green
Write-Host "===========================" -ForegroundColor Cyan

# Create backup directory structure
$backupDirs = @{
    "Daily" = "Daily"
    "Weekly" = "Weekly"
    "Monthly" = "Monthly"
}

foreach ($dir in $backupDirs.Values) {
    $fullPath = Join-Path $BackupPath $dir
    if (!(Test-Path $fullPath)) {
        New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
        Write-Host "✓ Created: $dir" -ForegroundColor Green
    }
}

function Backup-CursorChats {
    param([string]$BackupDir)
    
    Write-Host "`n💬 Backing up Cursor chats..." -ForegroundColor Yellow
    
    $cursorBackupDir = Join-Path $BackupDir "Cursor"
    if (!(Test-Path $cursorBackupDir)) {
        New-Item -ItemType Directory -Path $cursorBackupDir -Force | Out-Null
    }
    
    try {
        & "D:\01-AI-Models\Cursor-Chat-Backup.ps1" -OutputPath $cursorBackupDir
        Write-Host "  ✓ Cursor backup complete" -ForegroundColor Green
    } catch {
        Write-Host "  ❌ Cursor backup failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Backup-CopilotChats {
    param([string]$BackupDir)
    
    Write-Host "`n🤖 Backing up Copilot chats..." -ForegroundColor Yellow
    
    $copilotBackupDir = Join-Path $BackupDir "GitHub-Copilot"
    if (!(Test-Path $copilotBackupDir)) {
        New-Item -ItemType Directory -Path $copilotBackupDir -Force | Out-Null
    }
    
    try {
        & "D:\01-AI-Models\Copilot-Chat-Recovery.ps1" -OutputPath $copilotBackupDir -SearchAll
        Write-Host "  ✓ Copilot backup complete" -ForegroundColor Green
    } catch {
        Write-Host "  ❌ Copilot backup failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Backup-WebAIChats {
    param([string]$BackupDir)
    
    Write-Host "`n🌐 Backing up web AI chats..." -ForegroundColor Yellow
    
    $webBackupDir = Join-Path $BackupDir "Web-AI"
    if (!(Test-Path $webBackupDir)) {
        New-Item -ItemType Directory -Path $webBackupDir -Force | Out-Null
    }
    
    try {
        # Copy from main download directory
        $sourceDir = "D:\01-AI-Models\Chat-History\Downloaded"
        if (Test-Path $sourceDir) {
            $services = @("OpenAI", "Google", "AWS", "Moonshot", "Anthropic")
            
            foreach ($service in $services) {
                $serviceDir = Join-Path $sourceDir $service
                if (Test-Path $serviceDir) {
                    $destDir = Join-Path $webBackupDir $service
                    Copy-Item -Path $serviceDir -Destination $destDir -Recurse -Force
                    Write-Host "  ✓ Copied $service" -ForegroundColor Green
                }
            }
        }
    } catch {
        Write-Host "  ❌ Web AI backup failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Create-BackupReport {
    param([string]$BackupDir)
    
    $reportFile = Join-Path $BackupDir "backup_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
    
    $report = @"
AI Chat Backup Report
====================
Date: $(Get-Date)
Backup Directory: $BackupDir

Backup Summary:
"@
    
    # Count files in each service directory
    $services = @("Cursor", "GitHub-Copilot", "Web-AI")
    
    foreach ($service in $services) {
        $serviceDir = Join-Path $BackupDir $service
        if (Test-Path $serviceDir) {
            $fileCount = (Get-ChildItem -Path $serviceDir -Recurse -File).Count
            $totalSize = [math]::Round(((Get-ChildItem -Path $serviceDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB), 2)
            
            $report += "`n$service`: $fileCount files ($totalSize MB)"
        } else {
            $report += "`n$service`: No backup found"
        }
    }
    
    $report += "`n`nBackup completed successfully!"
    
    Set-Content -Path $reportFile -Value $report -Encoding UTF8
    Write-Host "  ✓ Backup report saved: $reportFile" -ForegroundColor Green
}

function Create-ScheduledTask {
    Write-Host "`n📅 Creating scheduled task..." -ForegroundColor Yellow
    
    $taskName = "AI-Chat-Auto-Backup"
    $taskDescription = "Automatically backup all AI chat conversations daily"
    $scriptPath = "D:\01-AI-Models\Auto-Backup-All-Chats.ps1"
    
    try {
        # Remove existing task if it exists
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
        
        # Create new task
        $action = New-ScheduledTaskAction -Execute "PowerShell.exe" -Argument "-ExecutionPolicy Bypass -File `"$scriptPath`" -RunNow"
        $trigger = New-ScheduledTaskTrigger -Daily -At "02:00"
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
        $principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType S4U
        
        Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description $taskDescription
        
        Write-Host "  ✓ Scheduled task created: $taskName" -ForegroundColor Green
        Write-Host "  ⏰ Runs daily at 2:00 AM" -ForegroundColor Cyan
    } catch {
        Write-Host "  ❌ Failed to create scheduled task: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Create-DesktopShortcut {
    Write-Host "`n🔗 Creating desktop shortcut..." -ForegroundColor Yellow
    
    try {
        $desktopPath = [Environment]::GetFolderPath("Desktop")
        $shortcutPath = "$desktopPath\Backup-All-AI-Chats.lnk"
        
        $WshShell = New-Object -comObject WScript.Shell
        $Shortcut = $WshShell.CreateShortcut($shortcutPath)
        $Shortcut.TargetPath = "powershell.exe"
        $Shortcut.Arguments = "-ExecutionPolicy Bypass -File `"D:\01-AI-Models\Auto-Backup-All-Chats.ps1`" -RunNow"
        $Shortcut.Description = "Backup all AI chat conversations"
        $Shortcut.Save()
        
        Write-Host "  ✓ Desktop shortcut created: Backup-All-AI-Chats" -ForegroundColor Green
    } catch {
        Write-Host "  ❌ Failed to create desktop shortcut: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Main execution
try {
    $currentBackupDir = Join-Path $BackupPath $Schedule
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupDir = Join-Path $currentBackupDir "backup_$timestamp"
    
    if (!(Test-Path $backupDir)) {
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    }
    
    Write-Host "`n🎯 Starting automated backup..." -ForegroundColor Cyan
    Write-Host "📁 Backup directory: $backupDir" -ForegroundColor White
    
    # Run backups
    Backup-CursorChats -BackupDir $backupDir
    Backup-CopilotChats -BackupDir $backupDir
    Backup-WebAIChats -BackupDir $backupDir
    
    # Create backup report
    Create-BackupReport -BackupDir $backupDir
    
    # Create scheduled task if requested
    if ($CreateTask) {
        Create-ScheduledTask
    }
    
    # Create desktop shortcut
    Create-DesktopShortcut
    
    Write-Host "`n🎉 Auto backup complete!" -ForegroundColor Green
    Write-Host "📊 Backup saved to: $backupDir" -ForegroundColor Cyan
    
    # Show summary
    $totalFiles = (Get-ChildItem -Path $backupDir -Recurse -File).Count
    $totalSize = [math]::Round(((Get-ChildItem -Path $backupDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB), 2)
    
    Write-Host "`n📈 Backup Summary:" -ForegroundColor Yellow
    Write-Host "  Total files: $totalFiles" -ForegroundColor White
    Write-Host "  Total size: $totalSize MB" -ForegroundColor White
    Write-Host "  Backup type: $Schedule" -ForegroundColor White
    
} catch {
    Write-Host "❌ Error during auto backup: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
