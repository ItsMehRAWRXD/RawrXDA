# RawrXD-UI-FileExplorer.psm1 - File explorer component

function Initialize-FileExplorer {
    try {
        Write-RawrXDLog "Initializing file explorer..." -Level INFO -Component "FileExplorer"
        
        $explorerPanel = $global:RawrXD.Components.ExplorerPanel
        if (-not $explorerPanel) {
            throw "Explorer panel not found"
        }
        
        # Create explorer container with toolbar
        $explorerContainer = New-Object System.Windows.Forms.Panel
        $explorerContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
        
        # Create explorer toolbar
        $explorerToolbar = Create-ExplorerToolbar
        $explorerContainer.Controls.Add($explorerToolbar)
        
        # Create tree view
        $treeView = New-Object System.Windows.Forms.TreeView
        $treeView.Dock = [System.Windows.Forms.DockStyle]::Fill
        $treeView.Font = New-Object System.Drawing.Font($global:RawrXD.Settings.UI.FontFamily, 9)
        $treeView.ShowLines = $true
        $treeView.ShowRootLines = $true
        $treeView.HideSelection = $false
        $treeView.FullRowSelect = $true
        
        # Apply theme
        Apply-ExplorerTheme -TreeView $treeView
        
        # Set up explorer events
        Setup-ExplorerEvents -TreeView $treeView
        
        $explorerContainer.Controls.Add($treeView)
        $explorerPanel.Controls.Add($explorerContainer)
        
        # Store references
        $global:RawrXD.Components.FileExplorer = $treeView
        $global:RawrXD.Components.ExplorerContainer = $explorerContainer
        $global:RawrXD.Components.ExplorerToolbar = $explorerToolbar
        
        # Initialize explorer state
        $global:RawrXD.Explorer = @{
            CurrentPath = $global:RawrXD.Settings.FileExplorer.DefaultPath
            ExpandedPaths = New-Object System.Collections.ArrayList
            RecentFiles = New-Object System.Collections.ArrayList
        }
        
        # Load initial directory
        Load-Directory -Path $global:RawrXD.Explorer.CurrentPath
        
        Write-RawrXDLog "File explorer initialized successfully" -Level SUCCESS -Component "FileExplorer"
    }
    catch {
        Write-RawrXDLog "Failed to initialize file explorer: $($_.Exception.Message)" -Level ERROR -Component "FileExplorer"
        throw
    }
}

function Create-ExplorerToolbar {
    $toolbar = New-Object System.Windows.Forms.ToolStrip
    $toolbar.Dock = [System.Windows.Forms.DockStyle]::Top
    $toolbar.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    
    # Home button
    $homeBtn = New-Object System.Windows.Forms.ToolStripButton
    $homeBtn.Text = "Home"
    $homeBtn.ToolTipText = "Navigate to home directory"
    $homeBtn.add_Click({ Navigate-ToHome })
    
    # Up button
    $upBtn = New-Object System.Windows.Forms.ToolStripButton
    $upBtn.Text = "Up"
    $upBtn.ToolTipText = "Go to parent directory"
    $upBtn.add_Click({ Navigate-Up })
    
    # Refresh button
    $refreshBtn = New-Object System.Windows.Forms.ToolStripButton
    $refreshBtn.Text = "Refresh"
    $refreshBtn.ToolTipText = "Refresh current directory"
    $refreshBtn.add_Click({ Refresh-Explorer })
    
    # Separator
    $separator1 = New-Object System.Windows.Forms.ToolStripSeparator
    
    # Path label and textbox
    $pathLabel = New-Object System.Windows.Forms.ToolStripLabel
    $pathLabel.Text = "Path:"
    
    $pathTextBox = New-Object System.Windows.Forms.ToolStripTextBox
    $pathTextBox.Size = New-Object System.Drawing.Size(200, 25)
    $pathTextBox.Text = $global:RawrXD.Explorer.CurrentPath
    $pathTextBox.add_KeyDown({
        param($sender, $e)
        if ($e.KeyCode -eq "Enter") {
            Navigate-ToPath -Path $sender.Text
        }
    })
    
    # Separator
    $separator2 = New-Object System.Windows.Forms.ToolStripSeparator
    
    # Show hidden files toggle
    $hiddenBtn = New-Object System.Windows.Forms.ToolStripButton
    $hiddenBtn.Text = "Hidden"
    $hiddenBtn.ToolTipText = "Show hidden files"
    $hiddenBtn.CheckOnClick = $true
    $hiddenBtn.Checked = $global:RawrXD.Settings.FileExplorer.ShowHiddenFiles
    $hiddenBtn.add_Click({ Toggle-HiddenFiles })
    
    $toolbar.Items.AddRange(@($homeBtn, $upBtn, $refreshBtn, $separator1, $pathLabel, $pathTextBox, $separator2, $hiddenBtn))
    
    # Store toolbar references
    $global:RawrXD.Components.ExplorerButtons = @{
        Home = $homeBtn
        Up = $upBtn
        Refresh = $refreshBtn
        PathTextBox = $pathTextBox
        Hidden = $hiddenBtn
    }
    
    return $toolbar
}

function Apply-ExplorerTheme {
    param($TreeView)
    
    $theme = $global:RawrXD.Settings.UI.Theme
    
    if ($theme -eq "Dark") {
        $TreeView.BackColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
        $TreeView.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
        $TreeView.LineColor = [System.Drawing.Color]::FromArgb(100, 100, 100)
    }
    else {
        $TreeView.BackColor = [System.Drawing.Color]::White
        $TreeView.ForeColor = [System.Drawing.Color]::Black
        $TreeView.LineColor = [System.Drawing.Color]::Black
    }
}

function Setup-ExplorerEvents {
    param($TreeView)
    
    # Node double-click event for opening files
    $TreeView.add_NodeMouseDoubleClick({
        param($sender, $e)
        $node = $e.Node
        if ($node.Tag -and $node.Tag.Type -eq "File") {
            Open-ExplorerFile -FilePath $node.Tag.FullPath
        }
    })
    
    # Node before expand event for lazy loading
    $TreeView.add_BeforeExpand({
        param($sender, $e)
        $node = $e.Node
        if ($node.Tag -and $node.Tag.Type -eq "Directory" -and $node.Nodes.Count -eq 1 -and $node.Nodes[0].Tag -eq "PLACEHOLDER") {
            Load-DirectoryNodes -ParentNode $node -DirectoryPath $node.Tag.FullPath
        }
    })
    
    # Node selection changed event
    $TreeView.add_AfterSelect({
        param($sender, $e)
        $node = $e.Node
        if ($node.Tag -and $node.Tag.Type -eq "Directory") {
            Update-PathTextBox -Path $node.Tag.FullPath
        }
    })
    
    # Context menu for right-click actions
    $contextMenu = Create-ExplorerContextMenu
    $TreeView.ContextMenuStrip = $contextMenu
}

function Load-Directory {
    param([string]$Path)
    
    $treeView = $global:RawrXD.Components.FileExplorer
    if (-not $treeView) { return }
    
    try {
        $treeView.BeginUpdate()
        $treeView.Nodes.Clear()
        
        if (-not (Test-Path $Path)) {
            $Path = $env:USERPROFILE
        }
        
        $global:RawrXD.Explorer.CurrentPath = $Path
        Update-PathTextBox -Path $Path
        
        # Add drives as root nodes
        $drives = Get-PSDrive -PSProvider FileSystem
        foreach ($drive in $drives) {
            $driveNode = Create-DirectoryNode -Name "$($drive.Name):\" -FullPath "$($drive.Name):\" -IsRoot $true
            $treeView.Nodes.Add($driveNode) | Out-Null
            
            # Expand current drive if it matches the path
            if ($Path.StartsWith("$($drive.Name):", [StringComparison]::OrdinalIgnoreCase)) {
                Load-DirectoryNodes -ParentNode $driveNode -DirectoryPath $driveNode.Tag.FullPath
                $driveNode.Expand()
                
                # Navigate to the specific path
                Navigate-ToNodePath -TargetPath $Path -ParentNode $driveNode
            }
        }
        
        Write-RawrXDLog "Loaded directory: $Path" -Level SUCCESS -Component "FileExplorer"
    }
    catch {
        Write-RawrXDLog "Failed to load directory: $($_.Exception.Message)" -Level ERROR -Component "FileExplorer"
    }
    finally {
        $treeView.EndUpdate()
    }
}

function Load-DirectoryNodes {
    param(
        $ParentNode,
        [string]$DirectoryPath
    )
    
    try {
        # Remove placeholder node
        if ($ParentNode.Nodes.Count -eq 1 -and $ParentNode.Nodes[0].Tag -eq "PLACEHOLDER") {
            $ParentNode.Nodes.RemoveAt(0)
        }
        
        $showHidden = $global:RawrXD.Settings.FileExplorer.ShowHiddenFiles
        
        # Get directories first
        try {
            $directories = Get-ChildItem -Path $DirectoryPath -Directory -Force:$showHidden -ErrorAction SilentlyContinue |
                Sort-Object Name
            
            foreach ($dir in $directories) {
                if (-not $showHidden -and $dir.Attributes -band [System.IO.FileAttributes]::Hidden) {
                    continue
                }
                
                $dirNode = Create-DirectoryNode -Name $dir.Name -FullPath $dir.FullName
                $ParentNode.Nodes.Add($dirNode) | Out-Null
            }
        }
        catch {
            Write-RawrXDLog "Access denied to directory: $DirectoryPath" -Level WARNING -Component "FileExplorer"
        }
        
        # Get files
        try {
            $files = Get-ChildItem -Path $DirectoryPath -File -Force:$showHidden -ErrorAction SilentlyContinue |
                Sort-Object Name
            
            foreach ($file in $files) {
                if (-not $showHidden -and $file.Attributes -band [System.IO.FileAttributes]::Hidden) {
                    continue
                }
                
                $fileNode = Create-FileNode -Name $file.Name -FullPath $file.FullName -Size $file.Length -LastModified $file.LastWriteTime
                $ParentNode.Nodes.Add($fileNode) | Out-Null
            }
        }
        catch {
            Write-RawrXDLog "Access denied to files in: $DirectoryPath" -Level WARNING -Component "FileExplorer"
        }
    }
    catch {
        Write-RawrXDLog "Error loading directory nodes: $($_.Exception.Message)" -Level ERROR -Component "FileExplorer"
    }
}

function Create-DirectoryNode {
    param(
        [string]$Name,
        [string]$FullPath,
        [bool]$IsRoot = $false
    )
    
    $node = New-Object System.Windows.Forms.TreeNode
    $node.Text = $Name
    $node.Tag = @{
        Type = "Directory"
        FullPath = $FullPath
        IsRoot = $IsRoot
    }
    
    # Set folder icon (using text representation for simplicity)
    if ($IsRoot) {
        $node.Text = "📁 $Name"
    }
    else {
        $node.Text = "📂 $Name"
    }
    
    # Add placeholder for lazy loading
    $placeholder = New-Object System.Windows.Forms.TreeNode
    $placeholder.Text = "Loading..."
    $placeholder.Tag = "PLACEHOLDER"
    $node.Nodes.Add($placeholder) | Out-Null
    
    return $node
}

function Create-FileNode {
    param(
        [string]$Name,
        [string]$FullPath,
        [long]$Size,
        [DateTime]$LastModified
    )
    
    $node = New-Object System.Windows.Forms.TreeNode
    $extension = [System.IO.Path]::GetExtension($Name).ToLower()
    
    # Set file icon based on extension
    $icon = switch ($extension) {
        '.ps1' { "📜" }
        '.psm1' { "📄" }
        '.txt' { "📝" }
        '.md' { "📋" }
        '.json' { "⚙️" }
        '.xml' { "🔧" }
        '.html' { "🌐" }
        '.css' { "🎨" }
        '.js' { "💛" }
        '.ts' { "💙" }
        '.py' { "🐍" }
        '.cs' { "🔷" }
        '.cpp' { "⚡" }
        '.java' { "☕" }
        '.log' { "📊" }
        default { "📄" }
    }
    
    $node.Text = "$icon $Name"
    $node.Tag = @{
        Type = "File"
        FullPath = $FullPath
        Size = $Size
        LastModified = $LastModified
        Extension = $extension
    }
    
    return $node
}

function Create-ExplorerContextMenu {
    $contextMenu = New-Object System.Windows.Forms.ContextMenuStrip
    
    # Open file
    $openItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $openItem.Text = "Open"
    $openItem.add_Click({ 
        $node = $global:RawrXD.Components.FileExplorer.SelectedNode
        if ($node -and $node.Tag.Type -eq "File") {
            Open-ExplorerFile -FilePath $node.Tag.FullPath
        }
    })
    
    # Open in external editor
    $openExternalItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $openExternalItem.Text = "Open with default app"
    $openExternalItem.add_Click({ 
        $node = $global:RawrXD.Components.FileExplorer.SelectedNode
        if ($node -and $node.Tag.Type -eq "File") {
            Start-Process $node.Tag.FullPath
        }
    })
    
    # Separator
    $separator1 = New-Object System.Windows.Forms.ToolStripSeparator
    
    # Copy path
    $copyPathItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $copyPathItem.Text = "Copy path"
    $copyPathItem.add_Click({ 
        $node = $global:RawrXD.Components.FileExplorer.SelectedNode
        if ($node) {
            [System.Windows.Forms.Clipboard]::SetText($node.Tag.FullPath)
        }
    })
    
    # Properties
    $propertiesItem = New-Object System.Windows.Forms.ToolStripMenuItem
    $propertiesItem.Text = "Properties"
    $propertiesItem.add_Click({ Show-FileProperties })
    
    $contextMenu.Items.AddRange(@($openItem, $openExternalItem, $separator1, $copyPathItem, $propertiesItem))
    
    return $contextMenu
}

function Navigate-ToHome {
    Load-Directory -Path $env:USERPROFILE
}

function Navigate-Up {
    $currentPath = $global:RawrXD.Explorer.CurrentPath
    $parentPath = Split-Path $currentPath -Parent
    if ($parentPath -and (Test-Path $parentPath)) {
        Load-Directory -Path $parentPath
    }
}

function Navigate-ToPath {
    param([string]$Path)
    
    if (Test-Path $Path) {
        Load-Directory -Path $Path
    }
    else {
        [System.Windows.Forms.MessageBox]::Show("Path not found: $Path", "Navigation Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning)
        Update-PathTextBox -Path $global:RawrXD.Explorer.CurrentPath
    }
}

function Navigate-ToNodePath {
    param(
        [string]$TargetPath,
        $ParentNode
    )
    
    # This function recursively expands nodes to reach a specific path
    # Implementation would be complex - simplified for now
}

function Refresh-Explorer {
    Load-Directory -Path $global:RawrXD.Explorer.CurrentPath
}

function Toggle-HiddenFiles {
    $hiddenBtn = $global:RawrXD.Components.ExplorerButtons.Hidden
    $global:RawrXD.Settings.FileExplorer.ShowHiddenFiles = $hiddenBtn.Checked
    Refresh-Explorer
    Write-RawrXDLog "Show hidden files: $($hiddenBtn.Checked)" -Level INFO -Component "FileExplorer"
}

function Update-PathTextBox {
    param([string]$Path)
    
    $pathTextBox = $global:RawrXD.Components.ExplorerButtons.PathTextBox
    if ($pathTextBox) {
        $pathTextBox.Text = $Path
    }
}

function Open-ExplorerFile {
    param([string]$FilePath)
    
    if (Get-Command Load-FileContent -ErrorAction SilentlyContinue) {
        Load-FileContent -FilePath $FilePath
        
        # Add to recent files
        if ($global:RawrXD.Explorer.RecentFiles -notcontains $FilePath) {
            $global:RawrXD.Explorer.RecentFiles.Add($FilePath) | Out-Null
            if ($global:RawrXD.Explorer.RecentFiles.Count -gt 10) {
                $global:RawrXD.Explorer.RecentFiles.RemoveAt(0)
            }
        }
        
        Write-RawrXDLog "Opened file from explorer: $FilePath" -Level SUCCESS -Component "FileExplorer"
    }
}

function Show-FileProperties {
    $node = $global:RawrXD.Components.FileExplorer.SelectedNode
    if (-not $node) { return }
    
    $info = if ($node.Tag.Type -eq "File") {
        $file = Get-Item $node.Tag.FullPath -ErrorAction SilentlyContinue
        if ($file) {
            @"
File: $($file.Name)
Path: $($file.FullName)
Size: $(Format-FileSize -Size $file.Length)
Created: $($file.CreationTime)
Modified: $($file.LastWriteTime)
Attributes: $($file.Attributes)
Type: $(Get-FileExtensionLanguage -FilePath $file.FullName)
"@
        }
        else { "File information not available" }
    }
    else {
        $dir = Get-Item $node.Tag.FullPath -ErrorAction SilentlyContinue
        if ($dir) {
            @"
Directory: $($dir.Name)
Path: $($dir.FullName)
Created: $($dir.CreationTime)
Modified: $($dir.LastWriteTime)
Attributes: $($dir.Attributes)
"@
        }
        else { "Directory information not available" }
    }
    
    [System.Windows.Forms.MessageBox]::Show($info, "Properties", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-FileExplorer',
    'Load-Directory',
    'Navigate-ToHome',
    'Navigate-Up',
    'Navigate-ToPath',
    'Refresh-Explorer',
    'Toggle-HiddenFiles',
    'Open-ExplorerFile',
    'Show-FileProperties'
)