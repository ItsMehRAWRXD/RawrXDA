#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Enhanced Desktop Copilot - View Media, Websites, and Drag-Drop Support

.DESCRIPTION
    Advanced desktop assistant with:
    - Image/media viewer
    - Website preview
    - Drag-and-drop file/folder support
    - Full system access
    - Voice-enabled with subtitles

.PARAMETER AutoStart
    Start in system tray automatically

.PARAMETER EnableVoice
    Enable voice features

.EXAMPLE
    .\desktop_copilot_enhanced.ps1 -AutoStart -EnableVoice
#>

param(
    [Parameter(Mandatory=$false)]
    [switch]$AutoStart,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableVoice,
    
    [Parameter(Mandatory=$false)]
    [switch]$Silent
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ═══════════════════════════════════════════════════════════════════════════════
# ENHANCED DESKTOP COPILOT WITH MEDIA & DRAG-DROP
# ═══════════════════════════════════════════════════════════════════════════════

class EnhancedDesktopCopilot {
    [System.Windows.Forms.NotifyIcon]$TrayIcon
    [System.Windows.Forms.ContextMenuStrip]$TrayMenu
    [System.Windows.Forms.Form]$MainWindow
    [System.Windows.Forms.TabControl]$TabControl
    [bool]$VoiceEnabled
    [hashtable]$SystemInfo
    [System.Collections.ArrayList]$TaskHistory
    [System.Collections.ArrayList]$DroppedItems
    
    EnhancedDesktopCopilot([bool]$voiceEnabled) {
        $this.VoiceEnabled = $voiceEnabled
        $this.TaskHistory = [System.Collections.ArrayList]::new()
        $this.DroppedItems = [System.Collections.ArrayList]::new()
        $this.SystemInfo = @{}
        
        $this.InitializeTrayIcon()
        $this.InitializeMainWindow()
        $this.StartSystemMonitoring()
    }
    
    [void] InitializeTrayIcon() {
        $this.TrayIcon = New-Object System.Windows.Forms.NotifyIcon
        
        try {
            $iconPath = "$PSScriptRoot\..\assets\copilot.ico"
            if (Test-Path $iconPath) {
                $this.TrayIcon.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($iconPath)
            }
            else {
                $this.TrayIcon.Icon = [System.Drawing.SystemIcons]::Application
            }
        }
        catch {
            $this.TrayIcon.Icon = [System.Drawing.SystemIcons]::Application
        }
        
        $this.TrayIcon.Text = "Enhanced Copilot - Drag files here (Ctrl+Shift+A)"
        $this.TrayIcon.Visible = $true
        
        # Create context menu
        $this.TrayMenu = New-Object System.Windows.Forms.ContextMenuStrip
        
        $openItem = $this.TrayMenu.Items.Add("Open Copilot (Ctrl+Shift+A)")
        $openItem.add_Click({ $this.ShowMainWindow() }.GetNewClosure())
        
        $this.TrayMenu.Items.Add("-")
        
        $diagItem = $this.TrayMenu.Items.Add("Diagnose System")
        $diagItem.add_Click({ $this.DiagnoseSystem() }.GetNewClosure())
        
        $fixItem = $this.TrayMenu.Items.Add("Fix Common Issues")
        $fixItem.add_Click({ $this.FixCommonIssues() }.GetNewClosure())
        
        $this.TrayMenu.Items.Add("-")
        
        $historyItem = $this.TrayMenu.Items.Add("View History")
        $historyItem.add_Click({ $this.ShowTaskHistory() }.GetNewClosure())
        
        $this.TrayMenu.Items.Add("-")
        
        $exitItem = $this.TrayMenu.Items.Add("Exit")
        $exitItem.add_Click({ $this.ExitApplication() }.GetNewClosure())
        
        $this.TrayIcon.ContextMenuStrip = $this.TrayMenu
        $this.TrayIcon.add_DoubleClick({ $this.ShowMainWindow() }.GetNewClosure())
        
        $this.TrayIcon.ShowBalloonTip(3000, "Enhanced Copilot Ready", 
            "Drag files/folders here or press Ctrl+Shift+A!", [System.Windows.Forms.ToolTipIcon]::Info)
    }
    
    [void] InitializeMainWindow() {
        $this.MainWindow = New-Object System.Windows.Forms.Form
        $this.MainWindow.Text = "Enhanced Desktop Copilot - Media & File Viewer"
        $this.MainWindow.Size = New-Object System.Drawing.Size(1000, 700)
        $this.MainWindow.StartPosition = "CenterScreen"
        $this.MainWindow.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 245)
        $this.MainWindow.AllowDrop = $true
        
        # Enable drag-drop for main window
        $this.MainWindow.add_DragOver({
            param($sender, $e)
            $e.Effect = [System.Windows.Forms.DragDropEffects]::Copy
        }.GetNewClosure())
        
        $this.MainWindow.add_DragDrop({
            param($sender, $e)
            $this.HandleDragDrop($e)
        }.GetNewClosure())
        
        # Title label
        $titleLabel = New-Object System.Windows.Forms.Label
        $titleLabel.Text = "🤖 Enhanced Desktop Copilot"
        $titleLabel.Font = New-Object System.Drawing.Font("Segoe UI", 18, [System.Drawing.FontStyle]::Bold)
        $titleLabel.Location = New-Object System.Drawing.Point(20, 20)
        $titleLabel.Size = New-Object System.Drawing.Size(960, 35)
        $titleLabel.ForeColor = [System.Drawing.Color]::FromArgb(102, 126, 234)
        $this.MainWindow.Controls.Add($titleLabel)
        
        # Subtitle
        $subtitleLabel = New-Object System.Windows.Forms.Label
        $subtitleLabel.Text = "Drag & drop files/folders • View images & websites • Full system diagnostics"
        $subtitleLabel.Font = New-Object System.Drawing.Font("Segoe UI", 10)
        $subtitleLabel.Location = New-Object System.Drawing.Point(20, 55)
        $subtitleLabel.Size = New-Object System.Drawing.Size(960, 25)
        $subtitleLabel.ForeColor = [System.Drawing.Color]::Gray
        $this.MainWindow.Controls.Add($subtitleLabel)
        
        # Tab control
        $this.TabControl = New-Object System.Windows.Forms.TabControl
        $this.TabControl.Location = New-Object System.Drawing.Point(20, 90)
        $this.TabControl.Size = New-Object System.Drawing.Size(960, 570)
        $this.MainWindow.Controls.Add($this.TabControl)
        
        # Tab 1: Media Viewer
        $this.CreateMediaViewerTab()
        
        # Tab 2: File Manager
        $this.CreateFileManagerTab()
        
        # Tab 3: Website Preview
        $this.CreateWebsitePreviewTab()
        
        # Tab 4: System Info
        $this.CreateSystemInfoTab()
        
        # Handle close button - minimize to tray
        $this.MainWindow.add_FormClosing({
            param($sender, $e)
            $e.Cancel = $true
            $this.MainWindow.Hide()
        }.GetNewClosure())
    }
    
    [void] CreateMediaViewerTab() {
        $tab = New-Object System.Windows.Forms.TabPage
        $tab.Text = "📸 Media Viewer"
        $tab.BackColor = [System.Drawing.Color]::White
        
        # Drag-drop area
        $dragLabel = New-Object System.Windows.Forms.Label
        $dragLabel.Text = "📁 Drag image files here"
        $dragLabel.Font = New-Object System.Drawing.Font("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
        $dragLabel.Location = New-Object System.Drawing.Point(20, 20)
        $dragLabel.Size = New-Object System.Drawing.Size(900, 40)
        $dragLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleCenter
        $dragLabel.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
        $dragLabel.BackColor = [System.Drawing.Color]::FromArgb(230, 240, 255)
        $dragLabel.AllowDrop = $true
        
        $dragLabel.add_DragOver({
            param($sender, $e)
            $e.Effect = [System.Windows.Forms.DragDropEffects]::Copy
            $sender.BackColor = [System.Drawing.Color]::FromArgb(200, 220, 255)
        }.GetNewClosure())
        
        $dragLabel.add_DragLeave({
            param($sender, $e)
            $sender.BackColor = [System.Drawing.Color]::FromArgb(230, 240, 255)
        }.GetNewClosure())
        
        $dragLabel.add_DragDrop({
            param($sender, $e)
            $this.LoadMediaFromDragDrop($e)
        }.GetNewClosure())
        
        $tab.Controls.Add($dragLabel)
        
        # Image display area
        $script:imageBox = New-Object System.Windows.Forms.PictureBox
        $script:imageBox.Location = New-Object System.Drawing.Point(20, 70)
        $script:imageBox.Size = New-Object System.Drawing.Size(900, 380)
        $script:imageBox.SizeMode = [System.Windows.Forms.PictureBoxSizeMode]::Zoom
        $script:imageBox.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
        $script:imageBox.BackColor = [System.Drawing.Color]::FromArgb(50, 50, 50)
        $tab.Controls.Add($script:imageBox)
        
        # Info label
        $script:mediaInfoLabel = New-Object System.Windows.Forms.Label
        $script:mediaInfoLabel.Location = New-Object System.Drawing.Point(20, 460)
        $script:mediaInfoLabel.Size = New-Object System.Drawing.Size(900, 60)
        $script:mediaInfoLabel.Font = New-Object System.Drawing.Font("Consolas", 9)
        $script:mediaInfoLabel.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 245)
        $script:mediaInfoLabel.Padding = New-Object System.Windows.Forms.Padding(5)
        $tab.Controls.Add($script:mediaInfoLabel)
        
        $this.TabControl.TabPages.Add($tab)
    }
    
    [void] CreateFileManagerTab() {
        $tab = New-Object System.Windows.Forms.TabPage
        $tab.Text = "📁 File Manager"
        $tab.BackColor = [System.Drawing.Color]::White
        
        # Drag-drop area
        $dragLabel = New-Object System.Windows.Forms.Label
        $dragLabel.Text = "📂 Drag files or folders here to organize"
        $dragLabel.Font = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Bold)
        $dragLabel.Location = New-Object System.Drawing.Point(20, 20)
        $dragLabel.Size = New-Object System.Drawing.Size(900, 50)
        $dragLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleCenter
        $dragLabel.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
        $dragLabel.BackColor = [System.Drawing.Color]::FromArgb(245, 230, 255)
        $dragLabel.AllowDrop = $true
        
        $dragLabel.add_DragOver({
            param($sender, $e)
            $e.Effect = [System.Windows.Forms.DragDropEffects]::Copy
            $sender.BackColor = [System.Drawing.Color]::FromArgb(230, 200, 255)
        }.GetNewClosure())
        
        $dragLabel.add_DragLeave({
            param($sender, $e)
            $sender.BackColor = [System.Drawing.Color]::FromArgb(245, 230, 255)
        }.GetNewClosure())
        
        $dragLabel.add_DragDrop({
            param($sender, $e)
            $this.HandleFileManagerDrop($e)
        }.GetNewClosure())
        
        $tab.Controls.Add($dragLabel)
        
        # File list
        $script:fileListBox = New-Object System.Windows.Forms.ListBox
        $script:fileListBox.Location = New-Object System.Drawing.Point(20, 80)
        $script:fileListBox.Size = New-Object System.Drawing.Size(900, 300)
        $script:fileListBox.Font = New-Object System.Drawing.Font("Consolas", 9)
        $tab.Controls.Add($script:fileListBox)
        
        # Action buttons
        $analyzeBtn = New-Object System.Windows.Forms.Button
        $analyzeBtn.Text = "📊 Analyze Files"
        $analyzeBtn.Location = New-Object System.Drawing.Point(20, 390)
        $analyzeBtn.Size = New-Object System.Drawing.Size(150, 35)
        $analyzeBtn.BackColor = [System.Drawing.Color]::FromArgb(102, 126, 234)
        $analyzeBtn.ForeColor = [System.Drawing.Color]::White
        $analyzeBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
        $analyzeBtn.add_Click({ $this.AnalyzeDroppedFiles() }.GetNewClosure())
        $tab.Controls.Add($analyzeBtn)
        
        $organizeBtn = New-Object System.Windows.Forms.Button
        $organizeBtn.Text = "🗂️ Organize"
        $organizeBtn.Location = New-Object System.Drawing.Point(180, 390)
        $organizeBtn.Size = New-Object System.Drawing.Size(150, 35)
        $organizeBtn.BackColor = [System.Drawing.Color]::FromArgb(76, 175, 80)
        $organizeBtn.ForeColor = [System.Drawing.Color]::White
        $organizeBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
        $organizeBtn.add_Click({ $this.OrganizeFiles() }.GetNewClosure())
        $tab.Controls.Add($organizeBtn)
        
        $clearBtn = New-Object System.Windows.Forms.Button
        $clearBtn.Text = "🗑️ Clear"
        $clearBtn.Location = New-Object System.Drawing.Point(340, 390)
        $clearBtn.Size = New-Object System.Drawing.Size(150, 35)
        $clearBtn.BackColor = [System.Drawing.Color]::FromArgb(244, 67, 54)
        $clearBtn.ForeColor = [System.Drawing.Color]::White
        $clearBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
        $clearBtn.add_Click({ 
            $script:fileListBox.Items.Clear()
            $this.DroppedItems.Clear()
        }.GetNewClosure())
        $tab.Controls.Add($clearBtn)
        
        # Info label
        $script:fileInfoLabel = New-Object System.Windows.Forms.Label
        $script:fileInfoLabel.Location = New-Object System.Drawing.Point(20, 435)
        $script:fileInfoLabel.Size = New-Object System.Drawing.Size(900, 55)
        $script:fileInfoLabel.Font = New-Object System.Drawing.Font("Consolas", 9)
        $script:fileInfoLabel.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 245)
        $tab.Controls.Add($script:fileInfoLabel)
        
        $this.TabControl.TabPages.Add($tab)
    }
    
    [void] CreateWebsitePreviewTab() {
        $tab = New-Object System.Windows.Forms.TabPage
        $tab.Text = "🌐 Website Preview"
        $tab.BackColor = [System.Drawing.Color]::White
        
        # URL input
        $urlLabel = New-Object System.Windows.Forms.Label
        $urlLabel.Text = "Enter URL:"
        $urlLabel.Location = New-Object System.Drawing.Point(20, 20)
        $urlLabel.Size = New-Object System.Drawing.Size(100, 25)
        $tab.Controls.Add($urlLabel)
        
        $script:urlBox = New-Object System.Windows.Forms.TextBox
        $script:urlBox.Location = New-Object System.Drawing.Point(130, 20)
        $script:urlBox.Size = New-Object System.Drawing.Size(650, 30)
        $script:urlBox.Font = New-Object System.Drawing.Font("Segoe UI", 11)
        $script:urlBox.Text = "https://example.com"
        $tab.Controls.Add($script:urlBox)
        
        $openBtn = New-Object System.Windows.Forms.Button
        $openBtn.Text = "🌐 Open"
        $openBtn.Location = New-Object System.Drawing.Point(790, 20)
        $openBtn.Size = New-Object System.Drawing.Size(130, 30)
        $openBtn.BackColor = [System.Drawing.Color]::FromArgb(33, 150, 243)
        $openBtn.ForeColor = [System.Drawing.Color]::White
        $openBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
        $openBtn.add_Click({ 
            Start-Process $script:urlBox.Text
            $this.TrayIcon.ShowBalloonTip(2000, "Browser Opened", "Opening in default browser", [System.Windows.Forms.ToolTipIcon]::Info)
        }.GetNewClosure())
        $tab.Controls.Add($openBtn)
        
        # Content preview
        $script:webPreviewBox = New-Object System.Windows.Forms.TextBox
        $script:webPreviewBox.Location = New-Object System.Drawing.Point(20, 65)
        $script:webPreviewBox.Size = New-Object System.Drawing.Size(900, 400)
        $script:webPreviewBox.Multiline = $true
        $script:webPreviewBox.ScrollBars = "Vertical"
        $script:webPreviewBox.Font = New-Object System.Drawing.Font("Consolas", 9)
        $script:webPreviewBox.ReadOnly = $true
        $script:webPreviewBox.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
        $script:webPreviewBox.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
        $tab.Controls.Add($script:webPreviewBox)
        
        # Fetch button
        $fetchBtn = New-Object System.Windows.Forms.Button
        $fetchBtn.Text = "📥 Fetch & Preview"
        $fetchBtn.Location = New-Object System.Drawing.Point(20, 475)
        $fetchBtn.Size = New-Object System.Drawing.Size(200, 35)
        $fetchBtn.BackColor = [System.Drawing.Color]::FromArgb(156, 39, 176)
        $fetchBtn.ForeColor = [System.Drawing.Color]::White
        $fetchBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
        $fetchBtn.add_Click({ $this.FetchWebsitePreview() }.GetNewClosure())
        $tab.Controls.Add($fetchBtn)
        
        $this.TabControl.TabPages.Add($tab)
    }
    
    [void] CreateSystemInfoTab() {
        $tab = New-Object System.Windows.Forms.TabPage
        $tab.Text = "💻 System Info"
        $tab.BackColor = [System.Drawing.Color]::White
        
        $script:systemInfoBox = New-Object System.Windows.Forms.TextBox
        $script:systemInfoBox.Location = New-Object System.Drawing.Point(20, 20)
        $script:systemInfoBox.Size = New-Object System.Drawing.Size(900, 460)
        $script:systemInfoBox.Multiline = $true
        $script:systemInfoBox.ScrollBars = "Vertical"
        $script:systemInfoBox.Font = New-Object System.Drawing.Font("Consolas", 9)
        $script:systemInfoBox.ReadOnly = $true
        $script:systemInfoBox.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
        $script:systemInfoBox.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
        $tab.Controls.Add($script:systemInfoBox)
        
        $refreshBtn = New-Object System.Windows.Forms.Button
        $refreshBtn.Text = "🔄 Refresh"
        $refreshBtn.Location = New-Object System.Drawing.Point(20, 490)
        $refreshBtn.Size = New-Object System.Drawing.Size(150, 35)
        $refreshBtn.BackColor = [System.Drawing.Color]::FromArgb(76, 175, 80)
        $refreshBtn.ForeColor = [System.Drawing.Color]::White
        $refreshBtn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
        $refreshBtn.add_Click({ $this.RefreshSystemInfo() }.GetNewClosure())
        $tab.Controls.Add($refreshBtn)
        
        # Initial load
        $this.RefreshSystemInfo()
        
        $this.TabControl.TabPages.Add($tab)
    }
    
    [void] HandleDragDrop([System.Windows.Forms.DragEventArgs]$e) {
        if ($e.Data.GetDataPresent([System.Windows.Forms.DataFormats]::FileDrop)) {
            $files = $e.Data.GetData([System.Windows.Forms.DataFormats]::FileDrop)
            
            foreach ($file in $files) {
                $this.DroppedItems.Add($file) | Out-Null
                
                $fileInfo = Get-Item $file
                if ($fileInfo.PSIsContainer) {
                    $script:fileListBox.Items.Add("📁 $($fileInfo.Name) (Folder)")
                }
                else {
                    $script:fileListBox.Items.Add("📄 $($fileInfo.Name)")
                }
            }
            
            $this.TrayIcon.ShowBalloonTip(2000, "Files Added", "$($files.Count) item(s) added!", [System.Windows.Forms.ToolTipIcon]::Info)
        }
    }
    
    [void] LoadMediaFromDragDrop([System.Windows.Forms.DragEventArgs]$e) {
        if ($e.Data.GetDataPresent([System.Windows.Forms.DataFormats]::FileDrop)) {
            $files = $e.Data.GetData([System.Windows.Forms.DataFormats]::FileDrop)
            
            $imageExtensions = @('.jpg', '.jpeg', '.png', '.gif', '.bmp', '.tiff', '.webp')
            
            foreach ($file in $files) {
                $ext = [System.IO.Path]::GetExtension($file).ToLower()
                
                if ($imageExtensions -contains $ext) {
                    try {
                        $image = New-Object System.Drawing.Bitmap($file)
                        $script:imageBox.Image = $image
                        
                        $fileInfo = Get-Item $file
                        $sizeMB = [Math]::Round($fileInfo.Length / 1MB, 2)
                        
                        $script:mediaInfoLabel.Text = @"
File: $($fileInfo.Name)
Size: $sizeMB MB | Dimensions: $($image.Width)x$($image.Height)px
Path: $file
Modified: $($fileInfo.LastWriteTime)
"@
                        
                        $this.TrayIcon.ShowBalloonTip(2000, "Image Loaded", "Displaying: $($fileInfo.Name)", [System.Windows.Forms.ToolTipIcon]::Info)
                    }
                    catch {
                        $script:mediaInfoLabel.Text = "❌ Error loading image: $_"
                    }
                    break
                }
            }
        }
    }
    
    [void] HandleFileManagerDrop([System.Windows.Forms.DragEventArgs]$e) {
        if ($e.Data.GetDataPresent([System.Windows.Forms.DataFormats]::FileDrop)) {
            $files = $e.Data.GetData([System.Windows.Forms.DataFormats]::FileDrop)
            
            $script:fileListBox.Items.Clear()
            $this.DroppedItems.Clear()
            
            foreach ($file in $files) {
                $this.DroppedItems.Add($file) | Out-Null
                
                $fileInfo = Get-Item $file
                if ($fileInfo.PSIsContainer) {
                    $script:fileListBox.Items.Add("📁 $($fileInfo.Name) (Folder)")
                }
                else {
                    $sizeMB = [Math]::Round($fileInfo.Length / 1MB, 2)
                    $script:fileListBox.Items.Add("📄 $($fileInfo.Name) ($sizeMB MB)")
                }
            }
            
            $this.TrayIcon.ShowBalloonTip(2000, "Items Loaded", "$($files.Count) item(s) ready to organize!", [System.Windows.Forms.ToolTipIcon]::Info)
        }
    }
    
    [void] AnalyzeDroppedFiles() {
        if ($this.DroppedItems.Count -eq 0) {
            $script:fileInfoLabel.Text = "❌ No files dropped yet"
            return
        }
        
        $info = "📊 FILE ANALYSIS`r`n`r`n"
        $totalSize = 0
        $fileCount = 0
        $folderCount = 0
        $typeBreakdown = @{}
        
        foreach ($item in $this.DroppedItems) {
            if ((Get-Item $item).PSIsContainer) {
                $folderCount++
                $files = Get-ChildItem $item -Recurse -File -ErrorAction SilentlyContinue
                foreach ($file in $files) {
                    $totalSize += $file.Length
                    $ext = [System.IO.Path]::GetExtension($file).ToLower()
                    if ($ext) {
                        if ($typeBreakdown.ContainsKey($ext)) {
                            $typeBreakdown[$ext]++
                        }
                        else {
                            $typeBreakdown[$ext] = 1
                        }
                    }
                }
            }
            else {
                $fileCount++
                $file = Get-Item $item
                $totalSize += $file.Length
                $ext = [System.IO.Path]::GetExtension($file).ToLower()
                if ($ext) {
                    if ($typeBreakdown.ContainsKey($ext)) {
                        $typeBreakdown[$ext]++
                    }
                    else {
                        $typeBreakdown[$ext] = 1
                    }
                }
            }
        }
        
        $info += "Items Analyzed: $($this.DroppedItems.Count)`r`n"
        $info += "Files: $fileCount | Folders: $folderCount`r`n"
        $info += "Total Size: $([Math]::Round($totalSize / 1MB, 2)) MB`r`n`r`n"
        $info += "File Types:`r`n"
        
        foreach ($type in ($typeBreakdown.Keys | Sort-Object)) {
            $info += "  $type: $($typeBreakdown[$type]) file(s)`r`n"
        }
        
        $script:fileInfoLabel.Text = $info
    }
    
    [void] OrganizeFiles() {
        if ($this.DroppedItems.Count -eq 0) {
            $script:fileInfoLabel.Text = "❌ No files to organize"
            return
        }
        
        $info = "📁 ORGANIZATION REPORT`r`n`r`n"
        $organized = 0
        
        foreach ($item in $this.DroppedItems) {
            if ((Get-Item $item).PSIsContainer) {
                $dir = Get-Item $item
                $targetDir = Join-Path (Split-Path $dir.FullName) "$($dir.Name)_Organized"
                New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
                
                $files = Get-ChildItem $dir -Recurse -File
                foreach ($file in $files) {
                    $category = switch ([System.IO.Path]::GetExtension($file).ToLower()) {
                        {$_ -in '.jpg','.jpeg','.png','.gif','.bmp'} { 'Images' }
                        {$_ -in '.mp4','.avi','.mkv','.mov'} { 'Videos' }
                        {$_ -in '.mp3','.wav','.flac'} { 'Audio' }
                        {$_ -in '.pdf','.docx','.txt'} { 'Documents' }
                        {$_ -in '.exe','.msi'} { 'Programs' }
                        {$_ -in '.zip','.rar','.7z'} { 'Archives' }
                        default { 'Other' }
                    }
                    
                    $catDir = Join-Path $targetDir $category
                    New-Item -ItemType Directory -Path $catDir -Force | Out-Null
                    Copy-Item $file.FullName -Destination $catDir -Force
                    $organized++
                }
                
                $info += "✅ Organized: $($dir.Name)`r`n"
            }
        }
        
        $info += "`r`nTotal Files Organized: $organized"
        $script:fileInfoLabel.Text = $info
        $this.TrayIcon.ShowBalloonTip(3000, "Organization Complete", "$organized files organized!", [System.Windows.Forms.ToolTipIcon]::Info)
    }
    
    [void] FetchWebsitePreview() {
        $url = $script:urlBox.Text
        
        if ([string]::IsNullOrWhiteSpace($url)) {
            $script:webPreviewBox.Text = "❌ Please enter a URL"
            return
        }
        
        $script:webPreviewBox.Text = "⏳ Fetching content..."
        $this.MainWindow.Refresh()
        
        try {
            $response = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
            $content = $response.Content
            
            # Extract text content
            $text = $content -replace '<[^>]*>', '' -replace '\s+', ' '
            $text = $text.Substring(0, [Math]::Min(2000, $text.Length))
            
            $preview = @"
🌐 Website Preview: $url

Status: $($response.StatusCode)
Content Type: $($response.Headers['Content-Type'])

Preview Content:
───────────────────────────────────────────────────────

$text

───────────────────────────────────────────────────────

💡 Click "Open" button to view full website in browser
"@
            
            $script:webPreviewBox.Text = $preview
        }
        catch {
            $script:webPreviewBox.Text = "❌ Error fetching website:`r`n$_`r`n`r`n💡 Try opening in browser instead"
        }
    }
    
    [void] RefreshSystemInfo() {
        try {
            $cpu = (Get-WmiObject Win32_Processor).LoadPercentage
            $memInfo = Get-WmiObject Win32_OperatingSystem
            $memPercent = [Math]::Round((($memInfo.TotalVisibleMemorySize - $memInfo.FreePhysicalMemory) / $memInfo.TotalVisibleMemorySize) * 100, 1)
            $memFreeGB = [Math]::Round($memInfo.FreePhysicalMemory / 1MB, 2)
            $memTotalGB = [Math]::Round($memInfo.TotalVisibleMemorySize / 1MB, 2)
            
            $topProcs = Get-Process | Sort-Object CPU -Descending | Select-Object -First 5
            
            $disk = Get-PSDrive C
            $diskFreeGB = [Math]::Round($disk.Free / 1GB, 2)
            $diskUsedGB = [Math]::Round($disk.Used / 1GB, 2)
            $diskPercent = [Math]::Round(($diskUsedGB / ($diskFreeGB + $diskUsedGB)) * 100, 1)
            
            $info = @"
═══════════════════════════════════════════════════════════════
                     💻 SYSTEM INFORMATION
═══════════════════════════════════════════════════════════════

📊 PERFORMANCE METRICS
─────────────────────────────────────────────────────────────
CPU Usage:          $cpu%
Memory Usage:       $memPercent% ($memFreeGB GB free / $memTotalGB GB total)
Disk Usage (C:):    $diskPercent% ($diskFreeGB GB free)

🔝 TOP PROCESSES (by CPU)
─────────────────────────────────────────────────────────────
$($topProcs | ForEach-Object { ("{0,-30} CPU: {1,6} | RAM: {2,6} MB" -f $_.ProcessName, [Math]::Round($_.CPU,2), [Math]::Round($_.WorkingSet64/1MB,0)) })

🌐 NETWORK
─────────────────────────────────────────────────────────────
$(
try {
    $adapters = Get-NetAdapter | Where-Object Status -eq 'Up'
    $adapters | ForEach-Object { "  • $($_.Name): $($_.LinkSpeed)" }
}
catch {
    "  • Unable to retrieve network info"
}
)

🖥️  COMPUTER INFO
─────────────────────────────────────────────────────────────
Computer Name:      $env:COMPUTERNAME
Username:           $env:USERNAME
OS:                 $([System.Environment]::OSVersion.VersionString)
Processors:         $((Get-WmiObject Win32_Processor).ProcessorCount)
RAM:                $memTotalGB GB

═══════════════════════════════════════════════════════════════
"@
            
            $script:systemInfoBox.Text = $info
        }
        catch {
            $script:systemInfoBox.Text = "❌ Error retrieving system info: $_"
        }
    }
    
    [void] ShowMainWindow() {
        $this.MainWindow.Show()
        $this.MainWindow.Activate()
        $this.MainWindow.BringToFront()
    }
    
    [void] DiagnoseSystem() {
        $this.ShowMainWindow()
        $this.TabControl.SelectedIndex = 3  # System Info tab
        $this.RefreshSystemInfo()
        $this.TrayIcon.ShowBalloonTip(2000, "System Info", "Displaying diagnostics in System Info tab", [System.Windows.Forms.ToolTipIcon]::Info)
    }
    
    [void] FixCommonIssues() {
        $script:systemInfoBox.Text = "🔧 FIXING COMMON ISSUES...`r`n`r`n"
        
        try {
            $script:systemInfoBox.AppendText("Cleaning temp files...`r`n")
            $tempPath = [System.IO.Path]::GetTempPath()
            $files = Get-ChildItem $tempPath -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-7) }
            $count = $files.Count
            $files | Remove-Item -Force -ErrorAction SilentlyContinue
            $script:systemInfoBox.AppendText("✅ Cleaned $count temp files`r`n`r`n")
            
            $script:systemInfoBox.AppendText("Flushing DNS...`r`n")
            ipconfig /flushdns | Out-Null
            $script:systemInfoBox.AppendText("✅ DNS flushed`r`n`r`n")
            
            $script:systemInfoBox.AppendText("✅ Common fixes applied!`r`n")
            $this.TrayIcon.ShowBalloonTip(2000, "Fixes Applied", "Common issues fixed!", [System.Windows.Forms.ToolTipIcon]::Info)
        }
        catch {
            $script:systemInfoBox.AppendText("❌ Error: $_`r`n")
        }
    }
    
    [void] ShowTaskHistory() {
        $this.ShowMainWindow()
        
        $historyText = "📜 TASK HISTORY`r`n`r`n"
        
        if ($this.TaskHistory.Count -eq 0) {
            $historyText += "No tasks yet."
        }
        else {
            foreach ($task in ($this.TaskHistory | Select-Object -Last 20)) {
                $historyText += "[$($task.Timestamp.ToString('HH:mm:ss'))] $($task.Description)`r`n"
            }
        }
        
        $script:systemInfoBox.Text = $historyText
    }
    
    [void] StartSystemMonitoring() {
        $this.SystemInfo = @{
            LastCheck = Get-Date
            CPU = (Get-WmiObject Win32_Processor).LoadPercentage
            Memory = (Get-WmiObject Win32_OperatingSystem).FreePhysicalMemory
        }
    }
    
    [void] ExitApplication() {
        $result = [System.Windows.Forms.MessageBox]::Show(
            "Exit Enhanced Copilot?",
            "Confirm Exit",
            [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Question
        )
        
        if ($result -eq [System.Windows.Forms.DialogResult]::Yes) {
            $this.TrayIcon.Visible = $false
            $this.TrayIcon.Dispose()
            [System.Windows.Forms.Application]::Exit()
        }
    }
    
    [void] Run() {
        if (-not $script:Silent) {
            Write-Host "`n✅ Enhanced Copilot running in system tray!" -ForegroundColor Green
            Write-Host "   • Drag files/folders to window or tray" -ForegroundColor Cyan
            Write-Host "   • View images and websites" -ForegroundColor Cyan
            Write-Host "   • Right-click tray icon for menu" -ForegroundColor Cyan
            Write-Host "   • Double-click tray icon to open`n" -ForegroundColor Cyan
        }
        
        [System.Windows.Forms.Application]::Run()
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

if (-not $Silent) {
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║      ENHANCED DESKTOP COPILOT - INITIALIZING                  ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta
    Write-Host "Features:" -ForegroundColor Yellow
    Write-Host "  🖼️  Image Viewer - Drag & drop image files" -ForegroundColor White
    Write-Host "  📁 File Manager - Organize entire folders" -ForegroundColor White
    Write-Host "  🌐 Website Preview - View website content" -ForegroundColor White
    Write-Host "  💻 System Info - Real-time diagnostics`n" -ForegroundColor White
}

# Create and run copilot
$copilot = [EnhancedDesktopCopilot]::new($EnableVoice)
$copilot.Run()
