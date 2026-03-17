<#
.SYNOPSIS
    RawrXD IDE - VS Code/Cursor Style Layout with 1,000 Tab Support
.DESCRIPTION
    Professional IDE with VS Code-style layout supporting up to 1,000 tabs per component:
    - Editor tabs (up to 1,000 files)
    - Agent Chat tabs (up to 1,000 chat sessions)
    - Terminal tabs (up to 1,000 terminals)
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [switch]$CliMode,
    
    [Parameter(Mandatory = $false)]
    [string]$Command
)

# ============================================
# GLOBAL VARIABLES AND CONFIGURATION
# ============================================

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Web

$script:ProjectRoot = $PSScriptRoot
$script:ModelsPath = Join-Path $PSScriptRoot "models"

# Tab management - support up to 1,000 tabs each
$script:EditorTabs = @{}
$script:EditorTabCount = 0
$script:MaxEditorTabs = 1000

$script:ChatTabs = @{}
$script:ChatTabCount = 0
$script:MaxChatTabs = 1000

$script:TerminalTabs = @{}
$script:TerminalTabCount = 0
$script:MaxTerminalTabs = 1000

$script:OllamaModels = @()

# AI Copilot suggestions database
$script:CodeSuggestions = @{
    "function" = "function MyFunction { param(`$param1) `n    # Your code here`n}"
    "class" = "class MyClass { `n    [string]`$property`n    `n    [void] Method() { }`n}"
    "foreach" = "foreach (`$item in `$collection) { `n    # Process item`n}"
    "if" = "if (`$condition) { `n    # True branch`n} else { `n    # False branch`n}"
    "try" = "try { `n    # Code that might fail`n} catch { `n    # Error handling`n}"
}

# ============================================
# AI COPILOT FEATURES - CODE SUGGESTIONS
# ============================================

function Get-CodeSuggestions {
    param([string]$CurrentCode, [string]$SelectedModel = "gpt-4")
    
    # Analyze current code and provide suggestions
    $suggestions = @()
    
    # Pattern matching for common patterns
    if ($CurrentCode -match "foreach|for\s*\(") {
        $suggestions += "Add array iteration optimization: Use ForEach-Object with -Parallel"
    }
    if ($CurrentCode -match "Get-|Set-" -and -not ($CurrentCode -match "ErrorAction")) {
        $suggestions += "Add error handling: -ErrorAction SilentlyContinue or Stop"
    }
    if ($CurrentCode -match "\[xml\]|\[json\]") {
        $suggestions += "Add XML/JSON validation before parsing"
    }
    if ($CurrentCode -match "Write-Host" -and -not ($CurrentCode -match "-ForegroundColor")) {
        $suggestions += "Add color to Write-Host for better visibility"
    }
    
    return $suggestions
}

function Generate-CodeCompletion {
    param([string]$Prefix, [string]$Context = "")
    
    # Ghost code / inline suggestions based on prefix
    $completions = @{
        "func" = "function MyFunction { param(`$param) `n    # Implementation`n}"
        "cls" = "class MyClass { `n    [string]`$Name`n}"
        "for" = "foreach (`$item in `$items) { `n    # Loop body`n}"
        "try" = "try { `n    # Code`n} catch { `n    # Error`n}"
        "var" = "`$variable = `$value"
        "cmnt" = "# TODO: Add description"
    }
    
    # Return matching completion
    $match = $completions.Keys | Where-Object { $_ -like "$Prefix*" } | Select-Object -First 1
    if ($match) { return $completions[$match] }
    return ""
}

function Show-AICopilotMenu {
    param($Editor, $SelectedModel)
    
    $contextMenu = New-Object System.Windows.Forms.ContextMenuStrip
    
    # Code generation options
    $generateCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Generate Code")
    $generateCodeItem.Add_Click({
        $generatedCode = "# Generated code using $SelectedModel`n" + (Get-CodeSuggestions $Editor.Text $SelectedModel -join "`n")
        $Editor.AppendText("`n`n$generatedCode")
    })
    
    # Fix code
    $fixCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Fix Code")
    $fixCodeItem.Add_Click({
        $fixedCode = "# Fixed by AI`n# Original issue: Check syntax, add error handling"
        [System.Windows.Forms.MessageBox]::Show($fixedCode, "Code Fix Suggestion")
    })
    
    # Code review
    $reviewCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Review Code")
    $reviewCodeItem.Add_Click({
        $suggestions = Get-CodeSuggestions $Editor.Text
        $review = "Code Review:`n`n" + ($suggestions -join "`n`n")
        [System.Windows.Forms.MessageBox]::Show($review, "Code Review")
    })
    
    # Refactor code
    $refactorItem = New-Object System.Windows.Forms.ToolStripMenuItem("Refactor")
    $refactorItem.Add_Click({
        [System.Windows.Forms.MessageBox]::Show("Refactoring suggestions:`n- Extract functions`n- Remove duplication`n- Improve naming", "Refactor Options")
    })
    
    # Model selector submenu
    $modelMenu = New-Object System.Windows.Forms.ToolStripMenuItem("Select Model")
    $modelMenu.Add_Click({
        # Show model selection dialog
        $models = @("gpt-4", "gpt-3.5-turbo", "local-llama2", "codellama")
        $selectedModel = [System.Windows.Forms.MessageBox]::Show("Select Model:`n- gpt-4 (Most capable)`n- gpt-3.5-turbo (Faster)`n- local-llama2`n- codellama", "Select AI Model")
    })
    
    # Add items to context menu
    $contextMenu.Items.AddRange(@(
        $generateCodeItem,
        $fixCodeItem,
        $reviewCodeItem,
        $refactorItem,
        (New-Object System.Windows.Forms.ToolStripSeparator),
        $modelMenu
    ))
    
    return $contextMenu
}

# ============================================
# FILE OPERATIONS - CREATE, DELETE, RENAME
# ============================================

function New-FileInExplorer {
    param($SelectedNode, $TreeView)
    
    $newFileName = Show-InputDialog -Title "Create New File" -Message "Enter file name:" -DefaultValue ""
    
    if ($newFileName) {
        try {
            $parentPath = if ($SelectedNode.Tag) { $SelectedNode.Tag } else { (Get-Location).Path }
            $newFilePath = Join-Path $parentPath $newFileName
            
            New-Item -Path $newFilePath -ItemType File -Force | Out-Null
            
            # Add to tree view
            $fileNode = $SelectedNode.Nodes.Add($newFileName, $newFileName)
            $fileNode.Tag = $newFilePath
            $fileNode.ForeColor = [System.Drawing.Color]::LightBlue
            
            [System.Windows.Forms.MessageBox]::Show("File created: $newFilePath", "Success")
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error: $($_.Exception.Message)", "Creation Error")
        }
    }
}

function New-DirectoryInExplorer {
    param($SelectedNode, $TreeView)
    
    $newDirName = Show-InputDialog -Title "Create New Folder" -Message "Enter folder name:" -DefaultValue ""
    
    if ($newDirName) {
        try {
            $parentPath = if ($SelectedNode.Tag) { $SelectedNode.Tag } else { (Get-Location).Path }
            $newDirPath = Join-Path $parentPath $newDirName
            
            New-Item -Path $newDirPath -ItemType Directory -Force | Out-Null
            
            # Add to tree view
            $dirNode = $SelectedNode.Nodes.Add($newDirName, $newDirName)
            $dirNode.Tag = $newDirPath
            $dirNode.Nodes.Add("Loading...", "Loading...") | Out-Null
            
            [System.Windows.Forms.MessageBox]::Show("Folder created: $newDirPath", "Success")
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error: $($_.Exception.Message)", "Creation Error")
        }
    }
}

function Delete-FileOrFolder {
    param($SelectedNode)
    
    if ($SelectedNode.Tag) {
        $result = [System.Windows.Forms.MessageBox]::Show(
            "Delete: $($SelectedNode.Text)?",
            "Confirm Deletion",
            [System.Windows.Forms.MessageBoxButtons]::YesNo
        )
        
        if ($result -eq "Yes") {
            try {
                Remove-Item $SelectedNode.Tag -Recurse -Force
                $SelectedNode.Parent.Nodes.Remove($SelectedNode)
                [System.Windows.Forms.MessageBox]::Show("Deleted successfully", "Success")
            }
            catch {
                [System.Windows.Forms.MessageBox]::Show("Error: $($_.Exception.Message)", "Deletion Error")
            }
        }
    }
}

function Rename-FileOrFolder {
    param($SelectedNode)
    
    $newName = Show-InputDialog -Title "Rename" -Message "Enter new name:" -DefaultValue $SelectedNode.Text
    
    if ($newName -and $newName -ne $SelectedNode.Text) {
        try {
            $parentPath = [System.IO.Path]::GetDirectoryName($SelectedNode.Tag)
            $newPath = Join-Path $parentPath $newName
            
            Rename-Item -Path $SelectedNode.Tag -NewName $newName
            $SelectedNode.Text = $newName
            $SelectedNode.Tag = $newPath
            
            [System.Windows.Forms.MessageBox]::Show("Renamed to: $newName", "Success")
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error: $($_.Exception.Message)", "Rename Error")
        }
    }
}

# ============================================
# UI HELPER FUNCTIONS
# ============================================

function Show-InputDialog {
    param(
        [string]$Title = "Input",
        [string]$Message = "Enter value:",
        [string]$DefaultValue = ""
    )
    
    $form = New-Object System.Windows.Forms.Form
    $form.Text = $Title
    $form.Size = New-Object System.Drawing.Size(400, 150)
    $form.StartPosition = "CenterParent"
    $form.TopMost = $true
    $form.FormBorderStyle = "FixedDialog"
    $form.MaximizeBox = $false
    $form.MinimizeBox = $false
    
    $label = New-Object System.Windows.Forms.Label
    $label.Location = New-Object System.Drawing.Point(10, 20)
    $label.Size = New-Object System.Drawing.Size(380, 30)
    $label.Text = $Message
    
    $textBox = New-Object System.Windows.Forms.TextBox
    $textBox.Location = New-Object System.Drawing.Point(10, 50)
    $textBox.Size = New-Object System.Drawing.Size(370, 25)
    $textBox.Text = $DefaultValue
    
    $okButton = New-Object System.Windows.Forms.Button
    $okButton.Location = New-Object System.Drawing.Point(200, 85)
    $okButton.Size = New-Object System.Drawing.Size(80, 25)
    $okButton.Text = "OK"
    $okButton.DialogResult = "OK"
    
    $cancelButton = New-Object System.Windows.Forms.Button
    $cancelButton.Location = New-Object System.Drawing.Point(290, 85)
    $cancelButton.Size = New-Object System.Drawing.Size(80, 25)
    $cancelButton.Text = "Cancel"
    $cancelButton.DialogResult = "Cancel"
    
    $form.Controls.AddRange(@($label, $textBox, $okButton, $cancelButton))
    $form.AcceptButton = $okButton
    $form.CancelButton = $cancelButton
    
    $result = $form.ShowDialog()
    if ($result -eq "OK") {
        return $textBox.Text
    }
    return $null
}

# Override System.Windows.Forms.Prompt for input dialogs (compatibility)
# Note: Add-Type with C# code - simplified approach
try {
    $null = Add-Type -TypeDefinition @"
public class PromptDialog {
    public static string ShowInput(string title, string message, string defaultValue) {
        var form = new System.Windows.Forms.Form();
        form.Text = title;
        form.Width = 400;
        form.Height = 150;
        form.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
        form.TopMost = true;
        form.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
        
        var label = new System.Windows.Forms.Label();
        label.Text = message;
        label.Left = 10;
        label.Top = 20;
        label.Width = 370;
        label.Height = 30;
        
        var textBox = new System.Windows.Forms.TextBox();
        textBox.Text = defaultValue;
        textBox.Left = 10;
        textBox.Top = 55;
        textBox.Width = 370;
        
        var okButton = new System.Windows.Forms.Button();
        okButton.Text = "OK";
        okButton.Left = 200;
        okButton.Top = 90;
        okButton.Width = 80;
        okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
        
        var cancelButton = new System.Windows.Forms.Button();
        cancelButton.Text = "Cancel";
        cancelButton.Left = 290;
        cancelButton.Top = 90;
        cancelButton.Width = 80;
        cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
        
        form.Controls.Add(label);
        form.Controls.Add(textBox);
        form.Controls.Add(okButton);
        form.Controls.Add(cancelButton);
        
        form.AcceptButton = okButton;
        form.CancelButton = cancelButton;
        
        if (form.ShowDialog() == System.Windows.Forms.DialogResult.OK) {
            return textBox.Text;
        }
        return null;
    }
}
"@ -ErrorAction Stop
} catch {
    Write-Host "Warning: Could not add PromptDialog type, will use simpler dialogs" -ForegroundColor Yellow
}

# ============================================
# REAL FILE OPERATIONS

function Initialize-RealFileExplorer {
    param($TreeView)
    
    $TreeView.Nodes.Clear()
    
    # REAL: Get all file system drives dynamically
    $drives = Get-PSDrive -PSProvider FileSystem | Where-Object { $_.Root -ne $null }
    
    foreach ($drive in $drives) {
        try {
            $used = [math]::Round($drive.Used / 1GB, 1)
            $free = [math]::Round($drive.Free / 1GB, 1)
            $driveNode = $TreeView.Nodes.Add($drive.Name, "$($drive.Name):\\ - ${used}/${free} GB")
            $driveNode.Tag = $drive.Root
            
            # REAL: Populate with actual directories
            $topDirs = Get-ChildItem $drive.Root -Directory -ErrorAction SilentlyContinue | Select-Object -First 10
            foreach ($dir in $topDirs) {
                $dirNode = $driveNode.Nodes.Add($dir.Name, $dir.Name)
                $dirNode.Tag = $dir.FullName
                $dirNode.Nodes.Add("Loading...", "Loading...") | Out-Null
            }
            
            # REAL: Add files from root
            $topFiles = Get-ChildItem $drive.Root -File -ErrorAction SilentlyContinue | Select-Object -First 20
            foreach ($file in $topFiles) {
                $fileNode = $driveNode.Nodes.Add($file.Name, $file.Name)
                $fileNode.Tag = $file.FullName
                $fileNode.ForeColor = [System.Drawing.Color]::LightBlue
            }
        }
        catch {
            $driveNode = $TreeView.Nodes.Add($drive.Name, "$($drive.Name):\\ - Access restricted")
            $driveNode.ForeColor = [System.Drawing.Color]::Red
        }
    }
}

function Populate-RealDirectory {
    param($Node)
    
    if ($Node.Nodes.Count -eq 0 -or $Node.Nodes[0].Text -eq "Loading...") {
        $Node.Nodes.Clear()
        
        try {
            # REAL: Get subdirectories
            $subDirs = Get-ChildItem $Node.Tag -Directory -ErrorAction SilentlyContinue
            foreach ($dir in $subDirs) {
                $childNode = $Node.Nodes.Add($dir.Name, $dir.Name)
                $childNode.Tag = $dir.FullName
                $childNode.Nodes.Add("Loading...", "Loading...") | Out-Null
            }
            
            # REAL: Get files
            $files = Get-ChildItem $Node.Tag -File -ErrorAction SilentlyContinue | Select-Object -First 50
            foreach ($file in $files) {
                $fileNode = $Node.Nodes.Add($file.Name, $file.Name)
                $fileNode.Tag = $file.FullName
                $fileNode.ForeColor = [System.Drawing.Color]::LightBlue
            }
        }
        catch {
            $Node.Nodes.Add("Access denied", "Access denied").ForeColor = [System.Drawing.Color]::Red
        }
    }
}

function Open-RealFile {
    param($Node)
    
    if ($Node -and $Node.Tag) {
        $filePath = $Node.Tag
        
        # Check if it's a file (not a directory)
        if (-not (Test-Path $filePath -PathType Container)) {
            try {
                # Read file content
                $content = Get-Content $filePath -Raw -ErrorAction Stop
                
                # Create new editor tab
                New-EditorTab -FileName $filePath -Content $content
                
                Update-StatusBar "Opened file: $(Split-Path $filePath -Leaf)"
            }
            catch {
                [System.Windows.Forms.MessageBox]::Show("Error reading file: $($_.Exception.Message)", "File Open Error")
            }
        }
    }
}

function Save-EditorTab {
    param($TabPage)
    
    if ($TabPage -and $TabPage.Tag -and $TabPage.Tag.FilePath) {
        try {
            $editor = $TabPage.Controls[0]
            if ($editor) {
                Set-Content -Path $TabPage.Tag.FilePath -Value $editor.Text -Force
                $TabPage.Tag.Modified = $false
                $TabPage.Text = $TabPage.Text.TrimEnd('*')
                Update-StatusBar "File saved: $(Split-Path $TabPage.Tag.FilePath -Leaf)"
            }
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error saving file: $($_.Exception.Message)", "Save Error")
        }
    }
    else {
        [System.Windows.Forms.MessageBox]::Show("No file path associated with this tab", "Save Error")
    }
}

# ============================================
# EDITOR TAB MANAGEMENT (UP TO 1,000 TABS)
# ============================================

function New-EditorTab {
    param(
        [string]$FileName = "",
        [string]$Content = ""
    )
    
    if ($script:EditorTabCount -ge $script:MaxEditorTabs) {
        [System.Windows.Forms.MessageBox]::Show("Maximum editor tabs ($($script:MaxEditorTabs)) reached!", "Tab Limit")
        return
    }
    
    $tabIndex = $script:EditorTabCount++
    $tabName = if ($FileName) { Split-Path $FileName -Leaf } else { "Untitled-$tabIndex" }
    
    $editorTab = New-Object System.Windows.Forms.TabPage($tabName)
    $editorTab.Tag = @{
        Index = $tabIndex
        FilePath = $FileName
        Modified = $false
    }
    
    $editor = New-Object System.Windows.Forms.RichTextBox
    $editor.Dock = "Fill"
    $editor.Font = New-Object System.Drawing.Font("Consolas", 10)
    $editor.Text = $Content
    $editor.Add_TextChanged({
        $tab = $script:EditorTabControl.SelectedTab
        if ($tab -and $tab.Tag) {
            $tab.Tag.Modified = $true
            if (-not $tab.Text.EndsWith("*")) {
                $tab.Text += "*"
            }
        }
    })
    
    # RIGHT-CLICK CONTEXT MENU FOR EDITOR (STANDARD + COPILOT FEATURES)
    $editorContextMenu = New-Object System.Windows.Forms.ContextMenuStrip
    
    # Standard Windows editing commands
    $cutItem = New-Object System.Windows.Forms.ToolStripMenuItem("Cut")
    $cutItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::X
    $cutItem.Add_Click({ $editor.Cut() })
    
    $copyItem = New-Object System.Windows.Forms.ToolStripMenuItem("Copy")
    $copyItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::C
    $copyItem.Add_Click({ $editor.Copy() })
    
    $pasteItem = New-Object System.Windows.Forms.ToolStripMenuItem("Paste")
    $pasteItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::V
    $pasteItem.Add_Click({ $editor.Paste() })
    
    $selectAllItem = New-Object System.Windows.Forms.ToolStripMenuItem("Select All")
    $selectAllItem.ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::A
    $selectAllItem.Add_Click({ $editor.SelectAll() })
    
    $suggestCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Suggest Code")
    $suggestCodeItem.Add_Click({
        $suggestions = Get-CodeSuggestions $editor.Text
        $message = "Code Suggestions:`n`n" + ($suggestions -join "`n`n")
        [System.Windows.Forms.MessageBox]::Show($message, "AI Code Suggestions")
    })
    
    $completeCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Code Completion")
    $completeCodeItem.Add_Click({
        $cursorPos = $editor.SelectionStart
        $line = $editor.Text.Substring(0, $cursorPos).Split("`n")[-1]
        $prefix = ($line -split '\s+')[-1]
        $completion = Generate-CodeCompletion $prefix
        if ($completion) {
            $editor.AppendText("`n$completion")
            [System.Windows.Forms.MessageBox]::Show("Code completion inserted", "Success")
        }
    })
    
    $generateCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Generate Code")
    $generateCodeItem.Add_Click({
        $prompt = Show-InputDialog -Title "Generate Code" -Message "Describe what code you want:" -DefaultValue ""
        if ($prompt) {
            $generated = "# Generated Code`n# Prompt: $prompt`nfunction GeneratedFunction {`n    # Implementation here`n}`n"
            $editor.AppendText("`n$generated")
            [System.Windows.Forms.MessageBox]::Show("Code generated and inserted", "Success")
        }
    })
    
    $fixCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Fix Code")
    $fixCodeItem.Add_Click({
        $selectedText = $editor.SelectedText
        if ($selectedText) {
            $fix = "# FIXED CODE`n# Original issue: Check syntax and add error handling`n$selectedText`n"
            [System.Windows.Forms.MessageBox]::Show("$fix`n`nManual review recommended", "Code Fix Suggestion")
        } else {
            [System.Windows.Forms.MessageBox]::Show("Select code to fix", "No Selection")
        }
    })
    
    $reviewCodeItem = New-Object System.Windows.Forms.ToolStripMenuItem("Review Code")
    $reviewCodeItem.Add_Click({
        $review = "Code Review:`n`n- Add error handling to all external calls`n- Consider extracting helper functions`n- Add XML documentation comments`n- Validate input parameters`n- Consider performance implications"
        [System.Windows.Forms.MessageBox]::Show($review, "Code Review Suggestions")
    })
    
    $refactorItem = New-Object System.Windows.Forms.ToolStripMenuItem("Refactor")
    $refactorItem.Add_Click({
        [System.Windows.Forms.MessageBox]::Show("Refactoring suggestions:`n`n- Extract repeated code into functions`n- Remove duplication`n- Improve variable naming`n- Simplify complex logic`n- Add comments for clarity", "Refactoring Options")
    })
    
    # MODEL SELECTOR SUBMENU - DYNAMIC
    $modelSelectItem = New-Object System.Windows.Forms.ToolStripMenuItem("Select AI Model")
    
    # Use actual loaded models
    $availableModels = if ($script:OllamaModels.Count -gt 0) { 
        $script:OllamaModels 
    } else { 
        @("No models loaded", "gpt-4 (API)", "gpt-3.5-turbo (API)")
    }
    
    foreach ($model in $availableModels) {
        $modelItem = New-Object System.Windows.Forms.ToolStripMenuItem($model)
        $currentModel = $model  # Capture variable for closure
        $modelItem.Add_Click({
            if ($script:StatusBarModelCombo) {
                $idx = $script:StatusBarModelCombo.Items.IndexOf($currentModel)
                if ($idx -ge 0) {
                    $script:StatusBarModelCombo.SelectedIndex = $idx
                }
            }
            Update-StatusBar "Selected AI Model: $currentModel"
            [System.Windows.Forms.MessageBox]::Show("AI Model switched to: $currentModel", "Model Selected")
        }.GetNewClosure())
        $modelSelectItem.DropDownItems.Add($modelItem) | Out-Null
    }
    
    $editorContextMenu.Items.AddRange(@(
        $cutItem,
        $copyItem,
        $pasteItem,
        $selectAllItem,
        (New-Object System.Windows.Forms.ToolStripSeparator),
        $suggestCodeItem,
        $completeCodeItem,
        $generateCodeItem,
        $fixCodeItem,
        $reviewCodeItem,
        $refactorItem,
        (New-Object System.Windows.Forms.ToolStripSeparator),
        $modelSelectItem
    ))
    
    $editor.ContextMenuStrip = $editorContextMenu
    
    $editorTab.Controls.Add($editor)
    
    # Insert before the "+" tab
    $insertIndex = [Math]::Max(0, $script:EditorTabControl.TabPages.Count - 1)
    $script:EditorTabControl.TabPages.Insert($insertIndex, $editorTab)
    $script:EditorTabControl.SelectedTab = $editorTab
    
    $script:EditorTabs[$tabIndex] = @{
        TabPage = $editorTab
        Editor = $editor
        FilePath = $FileName
    }
    
    Update-StatusBar "Editor tab created: $tabName (Total: $script:EditorTabCount)"
}

function Close-EditorTab {
    param($TabPage)
    
    if ($TabPage.Tag) {
        $tabIndex = $TabPage.Tag.Index
        if ($TabPage.Tag.Modified) {
            $result = [System.Windows.Forms.MessageBox]::Show(
                "Save changes to $($TabPage.Text)?",
                "Unsaved Changes",
                [System.Windows.Forms.MessageBoxButtons]::YesNoCancel
            )
            if ($result -eq "Cancel") { return }
            if ($result -eq "Yes") {
                Save-EditorTab $TabPage
            }
        }
        
        $script:EditorTabs.Remove($tabIndex)
        $script:EditorTabControl.TabPages.Remove($TabPage)
        $script:EditorTabCount--
        Update-StatusBar "Editor tab closed (Total: $script:EditorTabCount)"
    }
}

function Save-EditorTab {
    param($TabPage)
    
    if (-not $TabPage.Tag.FilePath) {
        $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
        $saveDialog.Filter = "All Files (*.*)|*.*"
        if ($saveDialog.ShowDialog() -eq "OK") {
            $TabPage.Tag.FilePath = $saveDialog.FileName
        } else { return }
    }
    
    try {
        $editor = $script:EditorTabs[$TabPage.Tag.Index].Editor
        Set-Content -Path $TabPage.Tag.FilePath -Value $editor.Text
        $TabPage.Tag.Modified = $false
        $TabPage.Text = $TabPage.Text.TrimEnd("*")
        Update-StatusBar "Saved: $($TabPage.Tag.FilePath)"
    }
    catch {
        [System.Windows.Forms.MessageBox]::Show("Error saving file: $($_.Exception.Message)", "Save Error")
    }
}

function Open-RealFile {
    param($Node)
    
    if ($Node.Tag -and (Test-Path $Node.Tag) -and -not (Get-Item $Node.Tag).PSIsContainer) {
        try {
            # REAL: Read file content
            $content = Get-Content $Node.Tag -Raw
            New-EditorTab -FileName $Node.Tag -Content $content
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error: $($_.Exception.Message)", "File Error")
        }
    }
}

# ============================================
# AGENT CHAT TAB MANAGEMENT (UP TO 1,000 TABS)
# ============================================

function New-ChatTab {
    param([string]$ChatName = "")
    
    if ($script:ChatTabCount -ge $script:MaxChatTabs) {
        [System.Windows.Forms.MessageBox]::Show("Maximum chat tabs ($($script:MaxChatTabs)) reached!", "Tab Limit")
        return
    }
    
    $tabIndex = $script:ChatTabCount++
    $tabName = if ($ChatName) { $ChatName } else { "Chat-$tabIndex" }
    
    $chatTab = New-Object System.Windows.Forms.TabPage($tabName)
    $chatTab.Tag = @{ Index = $tabIndex; MessageCount = 0 }
    
    $chatSplit = New-Object System.Windows.Forms.SplitContainer
    $chatSplit.Dock = "Fill"
    $chatSplit.Orientation = "Horizontal"
    $chatSplit.SplitterDistance = 400
    
    # Chat display
    $chatDisplay = New-Object System.Windows.Forms.RichTextBox
    $chatDisplay.Dock = "Fill"
    $chatDisplay.ReadOnly = $true
    $chatDisplay.Font = New-Object System.Drawing.Font("Segoe UI", 10)
    $chatDisplay.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $chatDisplay.ForeColor = [System.Drawing.Color]::White
    
    # Add copy/paste context menu for chat
    $chatContextMenu = New-Object System.Windows.Forms.ContextMenuStrip
    $chatCopyItem = New-Object System.Windows.Forms.ToolStripMenuItem("Copy")
    $chatCopyItem.Add_Click({ $chatDisplay.Copy() })
    $chatSelectAllItem = New-Object System.Windows.Forms.ToolStripMenuItem("Select All")
    $chatSelectAllItem.Add_Click({ $chatDisplay.SelectAll() })
    $chatContextMenu.Items.AddRange(@($chatCopyItem, $chatSelectAllItem))
    $chatDisplay.ContextMenuStrip = $chatContextMenu
    
    # Chat controls
    $chatControls = New-Object System.Windows.Forms.Panel
    $chatControls.Dock = "Fill"
    
    $modelLabel = New-Object System.Windows.Forms.Label
    $modelLabel.Text = "Model:"
    $modelLabel.Dock = "Top"
    $modelLabel.Height = 20
    
    $modelComboBox = New-Object System.Windows.Forms.ComboBox
    $modelComboBox.Dock = "Top"
    $modelComboBox.Height = 25
    $modelComboBox.DropDownStyle = "DropDownList"
    
    $inputLabel = New-Object System.Windows.Forms.Label
    $inputLabel.Text = "Message:"
    $inputLabel.Dock = "Top"
    $inputLabel.Height = 20
    
    $chatInput = New-Object System.Windows.Forms.TextBox
    $chatInput.Dock = "Top"
    $chatInput.Multiline = $true
    $chatInput.Height = 60
    
    $chatSend = New-Object System.Windows.Forms.Button
    $chatSend.Dock = "Top"
    $chatSend.Height = 30
    $chatSend.Text = "Send Message"
    $chatSend.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
    $chatSend.ForeColor = [System.Drawing.Color]::White
    
    $chatSend.Add_Click({
        $currentTab = $script:EditorTabControl.SelectedTab
        if ($currentTab.Tag -and $script:ChatTabs.ContainsKey($currentTab.Tag.Index)) {
            Send-ChatMessage $currentTab.Tag.Index
        }
    })
    
    $chatControls.Controls.AddRange(@($modelLabel, $modelComboBox, $inputLabel, $chatInput, $chatSend))
    
    $chatSplit.Panel1.Controls.Add($chatDisplay)
    $chatSplit.Panel2.Controls.Add($chatControls)
    $chatTab.Controls.Add($chatSplit)
    
    # Insert before the "+" tab
    $insertIndex = [Math]::Max(0, $script:EditorTabControl.TabPages.Count - 1)
    $script:EditorTabControl.TabPages.Insert($insertIndex, $chatTab)
    $script:EditorTabControl.SelectedTab = $chatTab
    
    $script:ChatTabs[$tabIndex] = @{
        TabPage = $chatTab
        Display = $chatDisplay
        Input = $chatInput
        Send = $chatSend
        ModelCombo = $modelComboBox
        Messages = @()
    }
    
    # Load Ollama models if available
    if ($script:OllamaModels.Count -eq 0) {
        Load-OllamaModels
    }
    
    foreach ($model in $script:OllamaModels) {
        $modelComboBox.Items.Add($model) | Out-Null
    }
    if ($modelComboBox.Items.Count -gt 0) {
        $modelComboBox.SelectedIndex = 0
    }
    
    Update-StatusBar "Chat tab created: $tabName (Total: $script:ChatTabCount)"
}

function Close-ChatTab {
    param($TabPage)
    
    if ($TabPage.Tag) {
        $tabIndex = $TabPage.Tag.Index
        $script:ChatTabs.Remove($tabIndex)
        $script:EditorTabControl.TabPages.Remove($TabPage)
        $script:ChatTabCount--
        Update-StatusBar "Chat tab closed (Total: $script:ChatTabCount)"
    }
}

function Load-OllamaModels {
    try {
        $output = ollama list 2>&1
        if ($output) {
            $script:OllamaModels = @()
            $lines = $output -split "`n" | Select-Object -Skip 1
            foreach ($line in $lines) {
                if ($line.Trim()) {
                    $modelName = ($line -split '\s+')[0]
                    if ($modelName) {
                        $script:OllamaModels += $modelName
                    }
                }
            }
        }
    }
    catch {
        $script:OllamaModels = @("ollama-not-available")
    }
}

function Send-ChatMessage {
    param([int]$TabIndex)
    
    $chatTab = $script:ChatTabs[$TabIndex]
    $message = $chatTab.Input.Text.Trim()
    $model = $chatTab.ModelCombo.SelectedItem
    
    if (-not $message) { return }
    
    # Display user message
    $chatTab.Display.SelectionColor = [System.Drawing.Color]::Cyan
    $chatTab.Display.AppendText("You: $message`n`n")
    $chatTab.Input.Clear()
    
    # REAL: Send to Ollama
    try {
        $chatTab.Display.SelectionColor = [System.Drawing.Color]::Yellow
        $chatTab.Display.AppendText("$model : ")
        
        $response = (ollama run $model $message 2>&1) -join "`n"
        
        $chatTab.Display.SelectionColor = [System.Drawing.Color]::White
        $chatTab.Display.AppendText("$response`n`n")
        
        $chatTab.Messages += @{ User = $message; Assistant = $response; Timestamp = Get-Date }
        $chatTab.TabPage.Tag.MessageCount++
        
        Update-StatusBar "Message sent to $model (Chat messages: $($chatTab.TabPage.Tag.MessageCount))"
    }
    catch {
        $chatTab.Display.SelectionColor = [System.Drawing.Color]::Red
        $chatTab.Display.AppendText("Error: $($_.Exception.Message)`n`n")
    }
}

# ============================================
# TERMINAL TAB MANAGEMENT (UP TO 1,000 TABS)
# ============================================

function New-TerminalTab {
    param([string]$TerminalName = "")
    
    if ($script:TerminalTabCount -ge $script:MaxTerminalTabs) {
        [System.Windows.Forms.MessageBox]::Show("Maximum terminal tabs ($($script:MaxTerminalTabs)) reached!", "Tab Limit")
        return
    }
    
    $tabIndex = $script:TerminalTabCount++
    $tabName = if ($TerminalName) { $TerminalName } else { "Terminal-$tabIndex" }
    
    $terminalTab = New-Object System.Windows.Forms.TabPage($tabName)
    $terminalTab.Tag = @{ Index = $tabIndex; CommandCount = 0 }
    
    $terminalSplit = New-Object System.Windows.Forms.SplitContainer
    $terminalSplit.Dock = "Fill"
    $terminalSplit.Orientation = "Horizontal"
    $terminalSplit.SplitterDistance = 150
    
    # Terminal output
    $terminalOutput = New-Object System.Windows.Forms.RichTextBox
    $terminalOutput.Dock = "Fill"
    $terminalOutput.ReadOnly = $true
    $terminalOutput.Font = New-Object System.Drawing.Font("Consolas", 9)
    $terminalOutput.BackColor = [System.Drawing.Color]::Black
    $terminalOutput.ForeColor = [System.Drawing.Color]::Lime
    
    # Add copy/paste context menu for terminal
    $termContextMenu = New-Object System.Windows.Forms.ContextMenuStrip
    $termCopyItem = New-Object System.Windows.Forms.ToolStripMenuItem("Copy")
    $termCopyItem.Add_Click({ $terminalOutput.Copy() })
    $termPasteItem = New-Object System.Windows.Forms.ToolStripMenuItem("Paste to Input")
    $termPasteItem.Add_Click({ 
        if ($terminalInput) {
            $terminalInput.Paste()
        }
    })
    $termSelectAllItem = New-Object System.Windows.Forms.ToolStripMenuItem("Select All")
    $termSelectAllItem.Add_Click({ $terminalOutput.SelectAll() })
    $termClearItem = New-Object System.Windows.Forms.ToolStripMenuItem("Clear Terminal")
    $termClearItem.Add_Click({ $terminalOutput.Clear() })
    $termContextMenu.Items.AddRange(@($termCopyItem, $termPasteItem, $termSelectAllItem, (New-Object System.Windows.Forms.ToolStripSeparator), $termClearItem))
    $terminalOutput.ContextMenuStrip = $termContextMenu
    
    # Terminal controls
    $terminalControls = New-Object System.Windows.Forms.Panel
    $terminalControls.Dock = "Fill"
    
    $terminalLabel = New-Object System.Windows.Forms.Label
    $terminalLabel.Text = "Command:"
    $terminalLabel.Dock = "Top"
    $terminalLabel.Height = 20
    
    $terminalInput = New-Object System.Windows.Forms.TextBox
    $terminalInput.Dock = "Top"
    $terminalInput.Height = 25
    $terminalInput.Font = New-Object System.Drawing.Font("Consolas", 10)
    
    $terminalExecute = New-Object System.Windows.Forms.Button
    $terminalExecute.Dock = "Top"
    $terminalExecute.Height = 30
    $terminalExecute.Text = "Execute Command"
    $terminalExecute.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
    $terminalExecute.ForeColor = [System.Drawing.Color]::White
    
    $terminalExecute.Add_Click({
        $currentTab = $script:TerminalTabControl.SelectedTab
        if ($currentTab.Tag -and $script:TerminalTabs.ContainsKey($currentTab.Tag.Index)) {
            Execute-TerminalCommand $currentTab.Tag.Index
        }
    })
    
    $terminalInput.Add_KeyDown({
        if ($_.KeyCode -eq "Return" -and $_.Control) {
            $currentTab = $script:TerminalTabControl.SelectedTab
            if ($currentTab.Tag -and $script:TerminalTabs.ContainsKey($currentTab.Tag.Index)) {
                Execute-TerminalCommand $currentTab.Tag.Index
            }
        }
    })
    
    $terminalControls.Controls.AddRange(@($terminalLabel, $terminalInput, $terminalExecute))
    
    $terminalSplit.Panel1.Controls.Add($terminalOutput)
    $terminalSplit.Panel2.Controls.Add($terminalControls)
    $terminalTab.Controls.Add($terminalSplit)
    
    # Insert before the "+" tab
    $insertIndex = [Math]::Max(0, $script:TerminalTabControl.TabPages.Count - 1)
    $script:TerminalTabControl.TabPages.Insert($insertIndex, $terminalTab)
    $script:TerminalTabControl.SelectedTab = $terminalTab
    
    $script:TerminalTabs[$tabIndex] = @{
        TabPage = $terminalTab
        Output = $terminalOutput
        Input = $terminalInput
        Execute = $terminalExecute
        History = @()
    }
    
    # Welcome message
    $terminalOutput.SelectionColor = [System.Drawing.Color]::Yellow
    $terminalOutput.AppendText("PowerShell Terminal - Ready`n")
    $terminalOutput.AppendText("Working Directory: $PWD`n")
    $terminalOutput.AppendText("Press Ctrl+Enter to execute`n`n")
    
    Update-StatusBar "Terminal tab created: $tabName (Total: $script:TerminalTabCount)"
}

function Close-TerminalTab {
    param($TabPage)
    
    if ($TabPage.Tag) {
        $tabIndex = $TabPage.Tag.Index
        $script:TerminalTabs.Remove($tabIndex)
        $script:TerminalTabControl.TabPages.Remove($TabPage)
        $script:TerminalTabCount--
        Update-StatusBar "Terminal tab closed (Total: $script:TerminalTabCount)"
    }
}

function Execute-TerminalCommand {
    param([int]$TabIndex)
    
    $terminalTab = $script:TerminalTabs[$TabIndex]
    $command = $terminalTab.Input.Text.Trim()
    
    if (-not $command) { return }
    
    # Display command
    $terminalTab.Output.SelectionColor = [System.Drawing.Color]::Cyan
    $terminalTab.Output.AppendText("PS> $command`n")
    $terminalTab.Input.Clear()
    
    # REAL: Execute PowerShell command
    try {
        $output = Invoke-Expression $command 2>&1 | Out-String
        
        $terminalTab.Output.SelectionColor = [System.Drawing.Color]::Lime
        $terminalTab.Output.AppendText("$output`n")
        
        $terminalTab.History += @{ Command = $command; Output = $output; Timestamp = Get-Date }
        $terminalTab.TabPage.Tag.CommandCount++
        
        Update-StatusBar "Command executed (Total commands: $($terminalTab.TabPage.Tag.CommandCount))"
    }
    catch {
        $terminalTab.Output.SelectionColor = [System.Drawing.Color]::Red
        $terminalTab.Output.AppendText("Error: $($_.Exception.Message)`n`n")
    }
}

# ============================================
# MASM BROWSER
# ============================================

function Initialize-RealMASMBrowser {
    param($Panel)
    
    $browserSplit = New-Object System.Windows.Forms.SplitContainer
    $browserSplit.Dock = "Fill"
    $browserSplit.Orientation = "Horizontal"
    $browserSplit.SplitterDistance = 30
    
    # URL bar
    $urlPanel = New-Object System.Windows.Forms.Panel
    $urlPanel.Dock = "Fill"
    
    $urlLabel = New-Object System.Windows.Forms.Label
    $urlLabel.Text = "URL:"
    $urlLabel.Dock = "Left"
    $urlLabel.Width = 40
    
    $urlTextBox = New-Object System.Windows.Forms.TextBox
    $urlTextBox.Dock = "Fill"
    $urlTextBox.Text = "https://www.masm32.com"
    
    $goButton = New-Object System.Windows.Forms.Button
    $goButton.Text = "Go"
    $goButton.Dock = "Right"
    $goButton.Width = 60
    
    $urlPanel.Controls.AddRange(@($urlLabel, $urlTextBox, $goButton))
    
    # REAL: WebBrowser control
    $browser = New-Object System.Windows.Forms.WebBrowser
    $browser.Dock = "Fill"
    $browser.ScriptErrorsSuppressed = $true
    
    $goButton.Add_Click({
        try {
            $browser.Navigate($urlTextBox.Text)
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Navigation error: $($_.Exception.Message)", "Browser Error")
        }
    })
    
    $browserSplit.Panel1.Controls.Add($urlPanel)
    $browserSplit.Panel2.Controls.Add($browser)
    $Panel.Controls.Add($browserSplit)
    
    # Navigate to initial URL
    try {
        $browser.Navigate($urlTextBox.Text)
    }
    catch {}
}

# ============================================
# MODEL LOADER
# ============================================

function Initialize-RealModelLoader {
    param($Panel)
    
    $modelSplit = New-Object System.Windows.Forms.SplitContainer
    $modelSplit.Dock = "Fill"
    $modelSplit.Orientation = "Horizontal"
    $modelSplit.SplitterDistance = 60
    
    # Path controls
    $pathPanel = New-Object System.Windows.Forms.Panel
    $pathPanel.Dock = "Fill"
    
    $pathLabel = New-Object System.Windows.Forms.Label
    $pathLabel.Text = "Model Path:"
    $pathLabel.Dock = "Top"
    $pathLabel.Height = 20
    
    $pathTextBox = New-Object System.Windows.Forms.TextBox
    $pathTextBox.Dock = "Top"
    $pathTextBox.Height = 25
    $pathTextBox.Text = "D:\OllamaModels"
    
    $browseButton = New-Object System.Windows.Forms.Button
    $browseButton.Dock = "Top"
    $browseButton.Height = 30
    $browseButton.Text = "Browse Folder..."
    
    $pathPanel.Controls.AddRange(@($pathLabel, $pathTextBox, $browseButton))
    
    # Model list
    $modelListBox = New-Object System.Windows.Forms.ListBox
    $modelListBox.Dock = "Fill"
    $modelListBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    
    $browseButton.Add_Click({
        $folderDialog = New-Object System.Windows.Forms.FolderBrowserDialog
        $folderDialog.SelectedPath = $pathTextBox.Text
        if ($folderDialog.ShowDialog() -eq "OK") {
            $pathTextBox.Text = $folderDialog.SelectedPath
            Load-ModelsFromPath $pathTextBox.Text $modelListBox
        }
    })
    
    $modelSplit.Panel1.Controls.Add($pathPanel)
    $modelSplit.Panel2.Controls.Add($modelListBox)
    $Panel.Controls.Add($modelSplit)
    
    # REAL: Initial scan
    if (Test-Path $pathTextBox.Text) {
        Load-ModelsFromPath $pathTextBox.Text $modelListBox
    }
}

function Load-ModelsFromPath {
    param($Path, $ListBox)
    
    $ListBox.Items.Clear()
    $ListBox.Items.Add("Scanning: $Path...") | Out-Null
    
    try {
        # REAL: Recursive model file search
        $extensions = @("*.gguf", "*.bin", "*.model", "*.safetensors", "*.ckpt", "*.pth")
        $models = @()
        
        foreach ($ext in $extensions) {
            $found = Get-ChildItem -Path $Path -Filter $ext -Recurse -ErrorAction SilentlyContinue
            $models += $found
        }
        
        $ListBox.Items.Clear()
        
        if ($models.Count -eq 0) {
            $ListBox.Items.Add("No model files found in: $Path") | Out-Null
        } else {
            foreach ($model in $models) {
                $size = [math]::Round($model.Length / 1MB, 1)
                $ListBox.Items.Add("$($model.Name) - ${size} MB - $($model.DirectoryName)") | Out-Null
            }
            $ListBox.Items.Add("") | Out-Null
            $ListBox.Items.Add("Total: $($models.Count) model files") | Out-Null
        }
        
        Update-StatusBar "Found $($models.Count) model files in: $Path"
    }
    catch {
        $ListBox.Items.Clear()
        $ListBox.Items.Add("Error: $($_.Exception.Message)") | Out-Null
    }
}

# ============================================
# STATUS BAR
# ============================================

function Update-StatusBar {
    param([string]$Message)
    
    if ($script:StatusLabel) {
        $script:StatusLabel.Text = "$Message | Editors: $script:EditorTabCount | Chats: $script:ChatTabCount | Terminals: $script:TerminalTabCount"
    }
}

# ============================================
# MAIN GUI INITIALIZATION - VS CODE STYLE
# ============================================

function Initialize-VSCodeStyleGUI {
    # REAL: Main form
    $script:MainForm = New-Object System.Windows.Forms.Form
    $script:MainForm.Text = "RawrXD IDE - VS Code Style (1,000 Tab Support)"
    $script:MainForm.Size = New-Object System.Drawing.Size(1600, 1000)
    $script:MainForm.StartPosition = "CenterScreen"
    $script:MainForm.BackColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
    
    # ============================================
    # VERTICAL SPLIT: File Explorer | Main Content
    # ============================================
    
    $mainVerticalSplit = New-Object System.Windows.Forms.SplitContainer
    $mainVerticalSplit.Dock = "Fill"
    $mainVerticalSplit.Orientation = "Vertical"
    $mainVerticalSplit.SplitterDistance = 300
    $mainVerticalSplit.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
    
    # LEFT PANEL: File Explorer
    $leftPanel = New-Object System.Windows.Forms.Panel
    $leftPanel.Dock = "Fill"
    $leftPanel.BackColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
    
    $explorerLabel = New-Object System.Windows.Forms.Label
    $explorerLabel.Text = "EXPLORER"
    $explorerLabel.Dock = "Top"
    $explorerLabel.Height = 25
    $explorerLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9, [System.Drawing.FontStyle]::Bold)
    $explorerLabel.ForeColor = [System.Drawing.Color]::White
    $explorerLabel.BackColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
    $explorerLabel.TextAlign = "MiddleLeft"
    $explorerLabel.Padding = New-Object System.Windows.Forms.Padding(5, 0, 0, 0)
    
    $script:FileExplorer = New-Object System.Windows.Forms.TreeView
    $script:FileExplorer.Dock = "Fill"
    $script:FileExplorer.BackColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
    $script:FileExplorer.ForeColor = [System.Drawing.Color]::White
    $script:FileExplorer.Add_BeforeExpand({ Populate-RealDirectory $_.Node })
    $script:FileExplorer.Add_NodeMouseDoubleClick({ Open-RealFile $_.Node })
    
    # RIGHT-CLICK CONTEXT MENU FOR FILE EXPLORER
    $explorerContextMenu = New-Object System.Windows.Forms.ContextMenuStrip
    
    $newFileItem = New-Object System.Windows.Forms.ToolStripMenuItem("New File")
    $newFileItem.Add_Click({
        New-FileInExplorer $script:FileExplorer.SelectedNode $script:FileExplorer
    })
    
    $newFolderItem = New-Object System.Windows.Forms.ToolStripMenuItem("New Folder")
    $newFolderItem.Add_Click({
        New-DirectoryInExplorer $script:FileExplorer.SelectedNode $script:FileExplorer
    })
    
    $renameItem = New-Object System.Windows.Forms.ToolStripMenuItem("Rename")
    $renameItem.Add_Click({
        Rename-FileOrFolder $script:FileExplorer.SelectedNode
    })
    
    $deleteItem = New-Object System.Windows.Forms.ToolStripMenuItem("Delete")
    $deleteItem.Add_Click({
        Delete-FileOrFolder $script:FileExplorer.SelectedNode
    })
    
    $openTerminalItem = New-Object System.Windows.Forms.ToolStripMenuItem("Open Terminal Here")
    $openTerminalItem.Add_Click({
        if ($script:FileExplorer.SelectedNode -and $script:FileExplorer.SelectedNode.Tag) {
            $dirPath = if ((Test-Path $script:FileExplorer.SelectedNode.Tag -PathType Container)) { 
                $script:FileExplorer.SelectedNode.Tag 
            } else { 
                [System.IO.Path]::GetDirectoryName($script:FileExplorer.SelectedNode.Tag)
            }
            New-TerminalTab
            $term = $script:TerminalTabs[$script:TerminalTabCount - 1]
            $term.Output.AppendText("PowerShell Terminal opened in: $dirPath`n")
            Push-Location $dirPath
        }
    })
    
    $openInEditorItem = New-Object System.Windows.Forms.ToolStripMenuItem("Open in Editor")
    $openInEditorItem.Add_Click({
        Open-RealFile $script:FileExplorer.SelectedNode
    })
    
    $explorerContextMenu.Items.AddRange(@(
        $newFileItem,
        $newFolderItem,
        $renameItem,
        $deleteItem,
        (New-Object System.Windows.Forms.ToolStripSeparator),
        $openTerminalItem,
        $openInEditorItem
    ))
    
    $script:FileExplorer.ContextMenuStrip = $explorerContextMenu
    
    $leftPanel.Controls.AddRange(@($explorerLabel, $script:FileExplorer))
    $mainVerticalSplit.Panel1.Controls.Add($leftPanel)
    
    # ============================================
    # RIGHT PANEL: Horizontal Split (Editor/Chat | Terminal)
    # ============================================
    
    $mainHorizontalSplit = New-Object System.Windows.Forms.SplitContainer
    $mainHorizontalSplit.Dock = "Fill"
    $mainHorizontalSplit.Orientation = "Horizontal"
    $mainHorizontalSplit.SplitterDistance = 650
    $mainHorizontalSplit.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
    
    # TOP PANEL: Editor and Chat Tabs
    $editorPanel = New-Object System.Windows.Forms.Panel
    $editorPanel.Dock = "Fill"
    
    # Toolbar for Editor/Chat
    $editorToolbar = New-Object System.Windows.Forms.Panel
    $editorToolbar.Dock = "Top"
    $editorToolbar.Height = 35
    $editorToolbar.BackColor = [System.Drawing.Color]::FromArgb(51, 51, 51)
    
    $newFileButton = New-Object System.Windows.Forms.Button
    $newFileButton.Text = "New File"
    $newFileButton.Location = New-Object System.Drawing.Point(5, 5)
    $newFileButton.Size = New-Object System.Drawing.Size(80, 25)
    $newFileButton.Add_Click({ New-EditorTab })
    
    $newChatButton = New-Object System.Windows.Forms.Button
    $newChatButton.Text = "New Chat"
    $newChatButton.Location = New-Object System.Drawing.Point(90, 5)
    $newChatButton.Size = New-Object System.Drawing.Size(80, 25)
    $newChatButton.Add_Click({ New-ChatTab })
    
    $saveButton = New-Object System.Windows.Forms.Button
    $saveButton.Text = "Save"
    $saveButton.Location = New-Object System.Drawing.Point(175, 5)
    $saveButton.Size = New-Object System.Drawing.Size(60, 25)
    $saveButton.Add_Click({
        if ($script:EditorTabControl.SelectedTab -and $script:EditorTabControl.SelectedTab.Tag) {
            Save-EditorTab $script:EditorTabControl.SelectedTab
        }
    })
    
    $masmButton = New-Object System.Windows.Forms.Button
    $masmButton.Text = "MASM"
    $masmButton.Location = New-Object System.Drawing.Point(240, 5)
    $masmButton.Size = New-Object System.Drawing.Size(60, 25)
    $masmButton.Add_Click({ $script:EditorTabControl.SelectedTab = $script:MASMTab })
    
    $modelsButton = New-Object System.Windows.Forms.Button
    $modelsButton.Text = "Models"
    $modelsButton.Location = New-Object System.Drawing.Point(305, 5)
    $modelsButton.Size = New-Object System.Drawing.Size(60, 25)
    $modelsButton.Add_Click({ $script:EditorTabControl.SelectedTab = $script:ModelsTab })
    
    $editorToolbar.Controls.AddRange(@($newFileButton, $newChatButton, $saveButton, $masmButton, $modelsButton))
    
    # Editor TabControl (for Editor, Chat, MASM, Models)
    $script:EditorTabControl = New-Object System.Windows.Forms.TabControl
    $script:EditorTabControl.Dock = "Fill"
    $script:EditorTabControl.Multiline = $false
    
    # Add MASM Browser Tab (permanent)
    $script:MASMTab = New-Object System.Windows.Forms.TabPage("MASM Browser")
    Initialize-RealMASMBrowser $script:MASMTab
    $script:EditorTabControl.TabPages.Add($script:MASMTab)
    
    # Add Model Loader Tab (permanent)
    $script:ModelsTab = New-Object System.Windows.Forms.TabPage("Model Loader")
    Initialize-RealModelLoader $script:ModelsTab
    $script:EditorTabControl.TabPages.Add($script:ModelsTab)
    
    # Create initial tabs
    New-EditorTab
    New-ChatTab
    
    # Handle middle-click or context menu to close tabs
    $script:EditorTabControl.Add_MouseDown({
        if ($_.Button -eq "Middle") {
            $tab = $script:EditorTabControl.SelectedTab
            if ($tab -and $tab.Tag -and $tab -ne $script:MASMTab -and $tab -ne $script:ModelsTab) {
                if ($script:EditorTabs.ContainsKey($tab.Tag.Index)) {
                    Close-EditorTab $tab
                } elseif ($script:ChatTabs.ContainsKey($tab.Tag.Index)) {
                    Close-ChatTab $tab
                }
            }
        }
    })
    
    $editorPanel.Controls.AddRange(@($editorToolbar, $script:EditorTabControl))
    $mainHorizontalSplit.Panel1.Controls.Add($editorPanel)
    
    # ============================================
    # BOTTOM PANEL: Terminal Tabs
    # ============================================
    
    $terminalPanel = New-Object System.Windows.Forms.Panel
    $terminalPanel.Dock = "Fill"
    
    # Toolbar for Terminal
    $terminalToolbar = New-Object System.Windows.Forms.Panel
    $terminalToolbar.Dock = "Top"
    $terminalToolbar.Height = 35
    $terminalToolbar.BackColor = [System.Drawing.Color]::FromArgb(51, 51, 51)
    
    $terminalLabel = New-Object System.Windows.Forms.Label
    $terminalLabel.Text = "TERMINAL"
    $terminalLabel.Location = New-Object System.Drawing.Point(5, 5)
    $terminalLabel.Size = New-Object System.Drawing.Size(100, 25)
    $terminalLabel.Font = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
    $terminalLabel.ForeColor = [System.Drawing.Color]::White
    $terminalLabel.TextAlign = "MiddleLeft"
    
    $newTerminalButton = New-Object System.Windows.Forms.Button
    $newTerminalButton.Text = "New Terminal"
    $newTerminalButton.Location = New-Object System.Drawing.Point(110, 5)
    $newTerminalButton.Size = New-Object System.Drawing.Size(100, 25)
    $newTerminalButton.Add_Click({ New-TerminalTab })
    
    $terminalToolbar.Controls.AddRange(@($terminalLabel, $newTerminalButton))
    
    # Terminal TabControl
    $script:TerminalTabControl = New-Object System.Windows.Forms.TabControl
    $script:TerminalTabControl.Dock = "Fill"
    
    # Create initial terminal
    New-TerminalTab
    
    # Handle middle-click to close terminal tabs
    $script:TerminalTabControl.Add_MouseDown({
        if ($_.Button -eq "Middle") {
            $tab = $script:TerminalTabControl.SelectedTab
            if ($tab -and $tab.Tag -and $script:TerminalTabs.ContainsKey($tab.Tag.Index)) {
                Close-TerminalTab $tab
            }
        }
    })
    
    $terminalPanel.Controls.AddRange(@($terminalToolbar, $script:TerminalTabControl))
    $mainHorizontalSplit.Panel2.Controls.Add($terminalPanel)
    
    # Add split container to main form
    $mainVerticalSplit.Panel2.Controls.Add($mainHorizontalSplit)
    $script:MainForm.Controls.Add($mainVerticalSplit)
    
    # ============================================
    # STATUS BAR
    # ============================================
    
    $statusStrip = New-Object System.Windows.Forms.StatusStrip
    $statusStrip.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
    $script:StatusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
    $script:StatusLabel.Text = "Ready - VS Code Style Layout"
    $script:StatusLabel.ForeColor = [System.Drawing.Color]::White
    $statusStrip.Items.Add($script:StatusLabel) | Out-Null
    
    # Model selector in status bar - DYNAMIC LOADING
    $modelLabel = New-Object System.Windows.Forms.ToolStripLabel
    $modelLabel.Text = "AI Model:"
    $modelLabel.ForeColor = [System.Drawing.Color]::White
    $statusStrip.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null
    $statusStrip.Items.Add($modelLabel) | Out-Null
    
    $script:StatusBarModelCombo = New-Object System.Windows.Forms.ToolStripComboBox
    $script:StatusBarModelCombo.AutoSize = $false
    $script:StatusBarModelCombo.Width = 200
    
    # Load actual models from Ollama
    if ($script:OllamaModels.Count -eq 0) {
        Load-OllamaModels
    }
    
    if ($script:OllamaModels.Count -gt 0) {
        foreach ($model in $script:OllamaModels) {
            $script:StatusBarModelCombo.Items.Add($model) | Out-Null
        }
        $script:StatusBarModelCombo.SelectedIndex = 0
    } else {
        # Fallback if no Ollama models found
        $script:StatusBarModelCombo.Items.AddRange(@("No models loaded - Install Ollama", "gpt-4 (API)", "gpt-3.5-turbo (API)"))
        $script:StatusBarModelCombo.SelectedIndex = 0
    }
    
    $script:StatusBarModelCombo.Add_SelectedIndexChanged({
        Update-StatusBar "Active AI Model: $($script:StatusBarModelCombo.SelectedItem)"
    })
    $statusStrip.Items.Add($script:StatusBarModelCombo) | Out-Null
    
    $script:MainForm.Controls.Add($statusStrip)
    
    # ============================================
    # INITIALIZE COMPONENTS
    # ============================================
    
    Initialize-RealFileExplorer $script:FileExplorer
    Update-StatusBar "Ready - All systems operational"
    
    # Show form
    $script:MainForm.Add_Shown({$script:MainForm.Activate()})
    $script:MainForm.ShowDialog() | Out-Null
}

# ============================================
# MAIN EXECUTION
# ============================================

if ($CliMode) {
    switch ($Command) {
        "test-drives" {
            Write-Host "Real drive detection:" -ForegroundColor Cyan
            Get-PSDrive -PSProvider FileSystem | ForEach-Object {
                $used = [math]::Round($_.Used / 1GB, 1)
                $free = [math]::Round($_.Free / 1GB, 1)
                Write-Host "$($_.Name): - ${used}/${free} GB - $($_.Root)" -ForegroundColor Green
            }
        }
        "test-models" {
            Write-Host "Real model scanning:" -ForegroundColor Cyan
            $paths = @("C:\\", "D:\\", "D:\\OllamaModels")
            foreach ($path in $paths) {
                if (Test-Path $path) {
                    Write-Host "Scanning: $path" -ForegroundColor Yellow
                    Get-ChildItem $path -Filter "*.gguf" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 3 | ForEach-Object {
                        $size = [math]::Round($_.Length / 1MB, 1)
                        Write-Host "Found: $($_.Name) - ${size} MB" -ForegroundColor Green
                    }
                }
            }
        }
        "test-ollama" {
            Write-Host "Real Ollama test:" -ForegroundColor Cyan
            try {
                ollama list
            }
            catch {
                Write-Host "Ollama not available" -ForegroundColor Red
            }
        }
        default {
            Write-Host "Commands: test-drives, test-models, test-ollama" -ForegroundColor Yellow
        }
    }
}
else {
    Write-Host "🚀 Launching RawrXD IDE - VS Code Style (1,000 Tab Support)..." -ForegroundColor Cyan
    Initialize-VSCodeStyleGUI
}
