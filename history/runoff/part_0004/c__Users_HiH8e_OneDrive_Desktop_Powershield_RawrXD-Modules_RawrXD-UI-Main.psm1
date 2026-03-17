# RawrXD-UI-Main.psm1 - Main GUI form and layout

function Start-RawrXDGUI {
    try {
        Write-RawrXDLog "Starting RawrXD GUI..." -Level INFO
        
        # Enable visual styles
        [System.Windows.Forms.Application]::EnableVisualStyles()
        [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false)
        
        # Create main form
        $form = New-Object System.Windows.Forms.Form
        $form.Text = "RawrXD v$($global:RawrXD.Version) - AI-Powered Editor"
        $form.Size = New-Object System.Drawing.Size($global:RawrXD.Settings.UI.WindowSize.Width, $global:RawrXD.Settings.UI.WindowSize.Height)
        $form.StartPosition = "CenterScreen"
        $form.MinimumSize = New-Object System.Drawing.Size(800, 600)
        
        # Store form reference
        $global:RawrXD.Form = $form
        
        # Apply theme
        Apply-Theme -Form $form
        
        # Create main layout
        Create-MainLayout -Form $form
        
        # Create menu bar
        Create-MenuBar -Form $form
        
        # Create status bar
        Create-StatusBar -Form $form
        
        # Set up event handlers
        Setup-FormEvents -Form $form
        
        Write-RawrXDLog "GUI components created successfully" -Level SUCCESS
        
        # Show the form
        [System.Windows.Forms.Application]::Run($form)
    }
    catch {
        Write-RawrXDLog "Failed to start GUI: $($_.Exception.Message)" -Level ERROR
        throw
    }
}

function Apply-Theme {
    param($Form)
    
    $theme = $global:RawrXD.Settings.UI.Theme
    
    if ($theme -eq "Dark") {
        $Form.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
        $Form.ForeColor = [System.Drawing.Color]::White
    }
    else {
        $Form.BackColor = [System.Drawing.SystemColors]::Control
        $Form.ForeColor = [System.Drawing.SystemColors]::ControlText
    }
}

function Create-MainLayout {
    param($Form)
    
    # Main horizontal splitter (Left: Explorer+Editor, Right: Chat+Browser)
    $mainSplitter = New-Object System.Windows.Forms.SplitContainer
    $mainSplitter.Dock = [System.Windows.Forms.DockStyle]::Fill
    $mainSplitter.Orientation = [System.Windows.Forms.Orientation]::Vertical
    $mainSplitter.Panel1MinSize = 300
    $mainSplitter.Panel2MinSize = 250
    
    # Left side splitter (Explorer and Editor)
    $leftSplitter = New-Object System.Windows.Forms.SplitContainer
    $leftSplitter.Dock = [System.Windows.Forms.DockStyle]::Fill
    $leftSplitter.Orientation = [System.Windows.Forms.Orientation]::Horizontal
    $leftSplitter.Panel1MinSize = 100
    $leftSplitter.Panel2MinSize = 150
    
    # Right side tab control for Chat and Browser
    $rightTabs = New-Object System.Windows.Forms.TabControl
    $rightTabs.Dock = [System.Windows.Forms.DockStyle]::Fill
    $rightTabs.Font = New-Object System.Drawing.Font($global:RawrXD.Settings.UI.FontFamily, 9)
    
    # Create Chat tab
    $chatTab = New-Object System.Windows.Forms.TabPage
    $chatTab.Text = "AI Chat"
    $chatTab.Name = "ChatTab"
    $rightTabs.TabPages.Add($chatTab)
    
    # Create Browser tab
    $browserTab = New-Object System.Windows.Forms.TabPage
    $browserTab.Text = "Browser"
    $browserTab.Name = "BrowserTab"
    $rightTabs.TabPages.Add($browserTab)
    
    # Add controls to splitters
    $mainSplitter.Panel1.Controls.Add($leftSplitter)
    $mainSplitter.Panel2.Controls.Add($rightTabs)
    $Form.Controls.Add($mainSplitter)
    
    # Store references for other modules
    $global:RawrXD.Components.MainSplitter = $mainSplitter
    $global:RawrXD.Components.LeftSplitter = $leftSplitter
    $global:RawrXD.Components.RightTabs = $rightTabs
    $global:RawrXD.Components.ExplorerPanel = $leftSplitter.Panel1
    $global:RawrXD.Components.EditorPanel = $leftSplitter.Panel2
    $global:RawrXD.Components.ChatTab = $chatTab
    $global:RawrXD.Components.BrowserTab = $browserTab
    
    Write-RawrXDLog "Main layout created" -Level SUCCESS
}

function Create-MenuBar {
    param($Form)
    
    $menuStrip = New-Object System.Windows.Forms.MenuStrip
    
    # File menu
    $fileMenu = New-Object System.Windows.Forms.ToolStripMenuItem
    $fileMenu.Text = "&File"
    
    $newItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $newItem.Text = "&New"
    $newItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::N
    $newItem.add_Click({ New-File })
    
    $openItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $openItem.Text = "&Open"
    $openItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::O
    $openItem.add_Click({ Open-File })
    
    $saveItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $saveItem.Text = "&Save"
    $saveItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::S
    $saveItem.add_Click({ Save-File })
    
    $exitItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $exitItem.Text = "E&xit"
    $exitItem.add_Click({ $Form.Close() })
    
    $fileMenu.DropDownItems.AddRange(@($newItem, $openItem, $saveItem, "-", $exitItem))
    
    # Edit menu
    $editMenu = New-Object System.Windows.Forms.ToolStripMenuItem
    $editMenu.Text = "&Edit"
    
    $undoItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $undoItem.Text = "&Undo"
    $undoItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::Z
    $undoItem.add_Click({ Undo-Edit })
    
    $redoItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $redoItem.Text = "&Redo"
    $redoItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::Y
    $redoItem.add_Click({ Redo-Edit })
    
    $findItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $findItem.Text = "&Find"
    $findItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::F
    $findItem.add_Click({ Show-FindDialog })
    
    $editMenu.DropDownItems.AddRange(@($undoItem, $redoItem, "-", $findItem))
    
    # AI menu
    $aiMenu = New-Object System.Windows.Forms.ToolStripMenuItem
    $aiMenu.Text = "&AI"
    
    $chatItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $chatItem.Text = "Open &Chat"
    $chatItem.add_Click({ Show-ChatPanel })
    
    $analyzeItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $analyzeItem.Text = "&Analyze Code"
    $analyzeItem.add_Click({ Analyze-CurrentFile })
    
    $aiMenu.DropDownItems.AddRange(@($chatItem, $analyzeItem))
    
    # Tools menu
    $toolsMenu = New-Object System.Windows.Forms.ToolStripMenuItem
    $toolsMenu.Text = "&Tools"
    
    $terminalItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $terminalItem.Text = "&Terminal"
    $terminalItem.add_Click({ Show-Terminal })
    
    $gitItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $gitItem.Text = "&Git Status"
    $gitItem.add_Click({ Show-GitStatus })
    
    $settingsItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $settingsItem.Text = "&Settings"
    $settingsItem.add_Click({ Show-SettingsDialog })
    
    $toolsMenu.DropDownItems.AddRange(@($terminalItem, $gitItem, "-", $settingsItem))
    
    # Help menu
    $helpMenu = New-Object System.Windows.Forms.ToolStripMenuItem
    $helpMenu.Text = "&Help"
    
    $aboutItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $aboutItem.Text = "&About"
    $aboutItem.add_Click({ Show-AboutDialog })
    
    $helpMenu.DropDownItems.AddRange(@($aboutItem))
    
    $menuStrip.Items.AddRange(@($fileMenu, $editMenu, $aiMenu, $toolsMenu, $helpMenu))
    $Form.MainMenuStrip = $menuStrip
    $Form.Controls.Add($menuStrip)
    
    $global:RawrXD.Components.MenuStrip = $menuStrip
    
    Write-RawrXDLog "Menu bar created" -Level SUCCESS
}

function Create-StatusBar {
    param($Form)
    
    $statusStrip = New-Object System.Windows.Forms.StatusStrip
    
    # File info label
    $fileLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
    $fileLabel.Text = "Ready"
    $fileLabel.Spring = $true
    $fileLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleLeft
    
    # Line/Column info
    $positionLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
    $positionLabel.Text = "Line: 1, Column: 1"
    $positionLabel.AutoSize = $true
    
    # AI status
    $aiStatusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
    $aiStatusLabel.Text = if ($global:RawrXD.OllamaAvailable) { "AI: Connected" } else { "AI: Disconnected" }
    $aiStatusLabel.AutoSize = $true
    
    # Git status
    $gitStatusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
    $gitStatusLabel.Text = "Git: ---"
    $gitStatusLabel.AutoSize = $true
    
    $statusStrip.Items.AddRange(@($fileLabel, $positionLabel, $aiStatusLabel, $gitStatusLabel))
    $Form.Controls.Add($statusStrip)
    
    $global:RawrXD.Components.StatusStrip = $statusStrip
    $global:RawrXD.Components.FileLabel = $fileLabel
    $global:RawrXD.Components.PositionLabel = $positionLabel
    $global:RawrXD.Components.AiStatusLabel = $aiStatusLabel
    $global:RawrXD.Components.GitStatusLabel = $gitStatusLabel
    
    Write-RawrXDLog "Status bar created" -Level SUCCESS
}

function Setup-FormEvents {
    param($Form)
    
    # Form closing event
    $Form.add_FormClosing({
        param($sender, $e)
        
        # Save settings
        if ($global:RawrXD.Settings) {
            # Update window size
            $global:RawrXD.Settings.UI.WindowSize.Width = $Form.Width
            $global:RawrXD.Settings.UI.WindowSize.Height = $Form.Height
            
            # Update splitter positions
            if ($global:RawrXD.Components.MainSplitter) {
                $global:RawrXD.Settings.UI.SplitterPositions.MainSplitter = $global:RawrXD.Components.MainSplitter.SplitterDistance
            }
            if ($global:RawrXD.Components.LeftSplitter) {
                $global:RawrXD.Settings.UI.SplitterPositions.LeftSplitter = $global:RawrXD.Components.LeftSplitter.SplitterDistance
            }
            
            Export-RawrXDSettings -Settings $global:RawrXD.Settings
        }
        
        Write-RawrXDLog "Application closing..." -Level INFO
    })
    
    # Form shown event
    $Form.add_Shown({
        Write-RawrXDLog "GUI is ready" -Level SUCCESS
        
        # Set splitter distances after form is properly sized
        if ($global:RawrXD.Components.MainSplitter) {
            $mainWidth = $global:RawrXD.Components.MainSplitter.Width
            $desiredMainDistance = $global:RawrXD.Settings.UI.SplitterPositions.MainSplitter
            $maxMainDistance = $mainWidth - $global:RawrXD.Components.MainSplitter.Panel2MinSize - $global:RawrXD.Components.MainSplitter.SplitterWidth
            $global:RawrXD.Components.MainSplitter.SplitterDistance = [Math]::Min($desiredMainDistance, $maxMainDistance)
        }
        
        if ($global:RawrXD.Components.LeftSplitter) {
            $leftHeight = $global:RawrXD.Components.LeftSplitter.Height
            $desiredLeftDistance = $global:RawrXD.Settings.UI.SplitterPositions.LeftSplitter
            $maxLeftDistance = $leftHeight - $global:RawrXD.Components.LeftSplitter.Panel2MinSize - $global:RawrXD.Components.LeftSplitter.SplitterWidth
            $global:RawrXD.Components.LeftSplitter.SplitterDistance = [Math]::Min($desiredLeftDistance, $maxLeftDistance)
        }
        
        # Initialize child components
        if (Get-Command Initialize-FileExplorer -ErrorAction SilentlyContinue) {
            Initialize-FileExplorer
        }
        
        if (Get-Command Initialize-TextEditor -ErrorAction SilentlyContinue) {
            Initialize-TextEditor
        }
        
        if (Get-Command Initialize-ChatPanel -ErrorAction SilentlyContinue) {
            Initialize-ChatPanel
        }
        
        Update-StatusBar
    })
    
    Write-RawrXDLog "Form events configured" -Level SUCCESS
}

# Menu event handlers (placeholders - will be implemented by other modules)
function New-File { 
    if (Get-Command New-EditorFile -ErrorAction SilentlyContinue) {
        New-EditorFile
    }
}

function Open-File { 
    if (Get-Command Open-EditorFile -ErrorAction SilentlyContinue) {
        Open-EditorFile
    }
}

function Save-File { 
    if (Get-Command Save-EditorFile -ErrorAction SilentlyContinue) {
        Save-EditorFile
    }
}

function Undo-Edit {
    if (Get-Command Undo-EditorAction -ErrorAction SilentlyContinue) {
        Undo-EditorAction
    }
}

function Redo-Edit {
    if (Get-Command Redo-EditorAction -ErrorAction SilentlyContinue) {
        Redo-EditorAction
    }
}

function Show-FindDialog {
    if (Get-Command Show-EditorFindDialog -ErrorAction SilentlyContinue) {
        Show-EditorFindDialog
    }
}

function Show-ChatPanel {
    $global:RawrXD.Components.RightTabs.SelectedTab = $global:RawrXD.Components.ChatTab
}

function Analyze-CurrentFile {
    if (Get-Command Analyze-EditorFile -ErrorAction SilentlyContinue) {
        Analyze-EditorFile
    }
}

function Show-Terminal {
    # Implementation for terminal window
    Write-RawrXDLog "Terminal feature not yet implemented" -Level WARNING
}

function Show-GitStatus {
    # Implementation for git status
    if ($global:RawrXD.CurrentFile) {
        $gitStatus = Get-GitStatus -Path (Split-Path $global:RawrXD.CurrentFile)
        if ($gitStatus.IsGitRepo) {
            $message = "Branch: $($gitStatus.Branch)`nChanges: $($gitStatus.HasChanges)"
            [System.Windows.Forms.MessageBox]::Show($message, "Git Status", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
        }
        else {
            [System.Windows.Forms.MessageBox]::Show("Current directory is not a Git repository.", "Git Status", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
        }
    }
}

function Show-SettingsDialog {
    # Implementation for settings dialog
    Write-RawrXDLog "Settings dialog not yet implemented" -Level WARNING
}

function Show-AboutDialog {
    $about = @"
RawrXD v$($global:RawrXD.Version)
Build: $($global:RawrXD.Build)

A modular AI-powered text editor with Ollama integration.

Created: $(Get-Date -Format 'yyyy-MM-dd')
PowerShell Version: $($PSVersionTable.PSVersion)
"@
    [System.Windows.Forms.MessageBox]::Show($about, "About RawrXD", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
}

function Update-StatusBar {
    if ($global:RawrXD.Components.FileLabel) {
        $fileName = if ($global:RawrXD.CurrentFile) { 
            [System.IO.Path]::GetFileName($global:RawrXD.CurrentFile) 
        } else { 
            "No file open" 
        }
        $global:RawrXD.Components.FileLabel.Text = $fileName
    }
    
    if ($global:RawrXD.Components.AiStatusLabel) {
        $global:RawrXD.Components.AiStatusLabel.Text = if ($global:RawrXD.OllamaAvailable) { "AI: Connected" } else { "AI: Disconnected" }
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Start-RawrXDGUI',
    'Apply-Theme',
    'Create-MainLayout',
    'Create-MenuBar', 
    'Create-StatusBar',
    'Setup-FormEvents',
    'Update-StatusBar'
)