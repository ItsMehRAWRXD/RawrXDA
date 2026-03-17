# PowerShell HTML Browser & IDE
# Proper HTML rendering with organized layout - no scattered elements

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Main Form
$mainForm = New-Object System.Windows.Forms.Form
$mainForm.Text = "PowerShell HTML Browser & IDE"
$mainForm.Size = New-Object System.Drawing.Size(1600, 1000)
$mainForm.StartPosition = "CenterScreen"
$mainForm.WindowState = "Maximized"

# Create Menu Bar
$menuStrip = New-Object System.Windows.Forms.MenuStrip

$fileMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$fileMenu.Text = "&File"

$newFileItem = New-Object System.Windows.Forms.ToolStripMenuItem -Property @{
  Text         = "New HTML File"
  ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::N
}

$openFileItem = New-Object System.Windows.Forms.ToolStripMenuItem -Property @{
  Text         = "Open..."
  ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::O
}

$saveFileItem = New-Object System.Windows.Forms.ToolStripMenuItem -Property @{
  Text         = "Save"
  ShortcutKeys = [System.Windows.Forms.Keys]::Control -bor [System.Windows.Forms.Keys]::S
}

$fileMenu.DropDownItems.AddRange(@($newFileItem, $openFileItem, $saveFileItem))

$viewMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$viewMenu.Text = "&View"

$refreshItem = New-Object System.Windows.Forms.ToolStripMenuItem -Property @{
  Text         = "Refresh Preview"
  ShortcutKeys = [System.Windows.Forms.Keys]::F5
}

$devToolsItem = New-Object System.Windows.Forms.ToolStripMenuItem -Property @{
  Text         = "Developer Console"
  ShortcutKeys = [System.Windows.Forms.Keys]::F12
}

$viewMenu.DropDownItems.AddRange(@($refreshItem, $devToolsItem))

$menuStrip.Items.AddRange(@($fileMenu, $viewMenu))
$mainForm.Controls.Add($menuStrip)
$mainForm.MainMenuStrip = $menuStrip

# Status Bar
$statusStrip = New-Object System.Windows.Forms.StatusStrip
$statusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel -Property @{
  Text   = "Ready"
  Spring = $true
}
$lineColLabel = New-Object System.Windows.Forms.ToolStripStatusLabel -Property @{
  Text = "Ln 1, Col 1"
}
$statusStrip.Items.AddRange(@($statusLabel, $lineColLabel))
$mainForm.Controls.Add($statusStrip)

# Main Container - FIXED LAYOUT to prevent scattering
$mainContainer = New-Object System.Windows.Forms.SplitContainer
$mainContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$mainContainer.Orientation = [System.Windows.Forms.Orientation]::Horizontal
$mainContainer.SplitterDistance = 500
$mainContainer.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
$mainForm.Controls.Add($mainContainer)

# ===== TOP PANEL: Code Editor =====
$editorPanel = New-Object System.Windows.Forms.Panel
$editorPanel.Dock = [System.Windows.Forms.DockStyle]::Fill
$editorPanel.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$mainContainer.Panel1.Controls.Add($editorPanel)

# Editor Label
$editorLabel = New-Object System.Windows.Forms.Label
$editorLabel.Text = "HTML Editor"
$editorLabel.Dock = [System.Windows.Forms.DockStyle]::Top
$editorLabel.Height = 30
$editorLabel.ForeColor = [System.Drawing.Color]::White
$editorLabel.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
$editorLabel.Font = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
$editorLabel.TextAlign = "MiddleLeft"
$editorLabel.Padding = New-Object System.Windows.Forms.Padding(10, 0, 0, 0)
$editorPanel.Controls.Add($editorLabel)

# HTML Code Editor
$codeEditor = New-Object System.Windows.Forms.RichTextBox
$codeEditor.Dock = [System.Windows.Forms.DockStyle]::Fill
$codeEditor.Font = New-Object System.Drawing.Font("Consolas", 11)
$codeEditor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$codeEditor.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
$codeEditor.AcceptsTab = $true
$codeEditor.WordWrap = $false
$codeEditor.ScrollBars = "Both"
$codeEditor.Text = @"
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PowerShell HTML Browser</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 40px;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        h1 {
            color: #333;
            border-bottom: 3px solid #667eea;
            padding-bottom: 10px;
            margin-bottom: 30px;
        }
        .card {
            background: #f8f9fa;
            padding: 20px;
            margin: 20px 0;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }
        button {
            background: #667eea;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            margin: 10px 5px;
            transition: all 0.3s;
        }
        button:hover {
            background: #764ba2;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        }
        input, textarea {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
            box-sizing: border-box;
        }
        input:focus, textarea:focus {
            outline: none;
            border-color: #667eea;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }
        .grid-item {
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 PowerShell HTML Browser & IDE</h1>
        
        <div class="card">
            <h2>Welcome!</h2>
            <p>This is a fully functional HTML browser built with PowerShell and Windows Forms.</p>
            <p>Edit the HTML code in the editor above and click "Refresh Preview" (F5) to see your changes.</p>
        </div>

        <div class="grid">
            <div class="grid-item">
                <h3>Feature 1</h3>
                <p>Proper layout management prevents UI elements from scattering.</p>
            </div>
            <div class="grid-item">
                <h3>Feature 2</h3>
                <p>WebBrowser control renders modern HTML, CSS, and JavaScript.</p>
            </div>
            <div class="grid-item">
                <h3>Feature 3</h3>
                <p>Split-panel design with resizable editor and preview.</p>
            </div>
        </div>

        <div class="card">
            <h3>Interactive Form Example</h3>
            <label>Your Name:</label>
            <input type="text" id="nameInput" placeholder="Enter your name...">
            
            <label>Message:</label>
            <textarea id="messageInput" rows="4" placeholder="Enter a message..."></textarea>
            
            <button onclick="showAlert()">Submit</button>
            <button onclick="clearForm()">Clear</button>
        </div>

        <div id="output" class="card" style="display:none;">
            <h3>Output:</h3>
            <p id="outputText"></p>
        </div>
    </div>

    <script>
        function showAlert() {
            const name = document.getElementById('nameInput').value;
            const message = document.getElementById('messageInput').value;
            const output = document.getElementById('output');
            const outputText = document.getElementById('outputText');
            
            if (name && message) {
                outputText.innerHTML = '<strong>Name:</strong> ' + name + '<br><strong>Message:</strong> ' + message;
                output.style.display = 'block';
            } else {
                alert('Please fill in both fields!');
            }
        }

        function clearForm() {
            document.getElementById('nameInput').value = '';
            document.getElementById('messageInput').value = '';
            document.getElementById('output').style.display = 'none';
        }
    </script>
</body>
</html>
"@
$editorPanel.Controls.Add($codeEditor)

# ===== BOTTOM PANEL: Browser Preview =====
$previewPanel = New-Object System.Windows.Forms.Panel
$previewPanel.Dock = [System.Windows.Forms.DockStyle]::Fill
$previewPanel.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$mainContainer.Panel2.Controls.Add($previewPanel)

# Browser Toolbar
$browserToolbar = New-Object System.Windows.Forms.Panel
$browserToolbar.Dock = [System.Windows.Forms.DockStyle]::Top
$browserToolbar.Height = 40
$browserToolbar.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
$previewPanel.Controls.Add($browserToolbar)

# Back Button
$backButton = New-Object System.Windows.Forms.Button
$backButton.Text = "◀"
$backButton.Location = New-Object System.Drawing.Point(5, 8)
$backButton.Size = New-Object System.Drawing.Size(35, 25)
$backButton.FlatStyle = "Flat"
$backButton.ForeColor = [System.Drawing.Color]::White
$browserToolbar.Controls.Add($backButton)

# Forward Button
$forwardButton = New-Object System.Windows.Forms.Button
$forwardButton.Text = "▶"
$forwardButton.Location = New-Object System.Drawing.Point(45, 8)
$forwardButton.Size = New-Object System.Drawing.Size(35, 25)
$forwardButton.FlatStyle = "Flat"
$forwardButton.ForeColor = [System.Drawing.Color]::White
$browserToolbar.Controls.Add($forwardButton)

# Refresh Button
$refreshButton = New-Object System.Windows.Forms.Button
$refreshButton.Text = "🔄"
$refreshButton.Location = New-Object System.Drawing.Point(85, 8)
$refreshButton.Size = New-Object System.Drawing.Size(35, 25)
$refreshButton.FlatStyle = "Flat"
$refreshButton.ForeColor = [System.Drawing.Color]::White
$browserToolbar.Controls.Add($refreshButton)

# URL/Title Label
$urlLabel = New-Object System.Windows.Forms.Label
$urlLabel.Text = "Preview"
$urlLabel.Location = New-Object System.Drawing.Point(130, 12)
$urlLabel.AutoSize = $true
$urlLabel.ForeColor = [System.Drawing.Color]::White
$urlLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$browserToolbar.Controls.Add($urlLabel)

# WebBrowser Control - THIS PREVENTS SCATTERED LAYOUT
$webBrowser = New-Object System.Windows.Forms.WebBrowser
$webBrowser.Dock = [System.Windows.Forms.DockStyle]::Fill
$webBrowser.ScriptErrorsSuppressed = $false
$webBrowser.IsWebBrowserContextMenuEnabled = $true
$webBrowser.AllowWebBrowserDrop = $true
$previewPanel.Controls.Add($webBrowser)

# Developer Console (Initially Hidden)
$consolePanel = New-Object System.Windows.Forms.Panel
$consolePanel.Dock = [System.Windows.Forms.DockStyle]::Bottom
$consolePanel.Height = 0
$consolePanel.BackColor = [System.Drawing.Color]::Black
$consolePanel.Visible = $false
$mainForm.Controls.Add($consolePanel)

$consoleOutput = New-Object System.Windows.Forms.RichTextBox
$consoleOutput.Dock = [System.Windows.Forms.DockStyle]::Fill
$consoleOutput.BackColor = [System.Drawing.Color]::Black
$consoleOutput.ForeColor = [System.Drawing.Color]::LimeGreen
$consoleOutput.Font = New-Object System.Drawing.Font("Consolas", 9)
$consoleOutput.ReadOnly = $true
$consolePanel.Controls.Add($consoleOutput)

# Global Variables
$script:currentFile = $null
$script:isDirty = $false

# Helper Functions
function Update-Preview {
  try {
    $html = $codeEditor.Text
    $webBrowser.DocumentText = $html
    $statusLabel.Text = "Preview updated - $(Get-Date -Format 'HH:mm:ss')"
    Log-Console "Preview refreshed successfully"
  }
  catch {
    $statusLabel.Text = "Error updating preview: $($_.Exception.Message)"
    Log-Console "ERROR: $($_.Exception.Message)" -Color "Red"
  }
}

function Log-Console {
  param(
    [string]$Message,
    [string]$Color = "LimeGreen"
  )
  $timestamp = Get-Date -Format "HH:mm:ss.fff"
  $consoleOutput.SelectionStart = $consoleOutput.TextLength
  $consoleOutput.SelectionLength = 0
  $consoleOutput.SelectionColor = [System.Drawing.Color]::Gray
  $consoleOutput.AppendText("[$timestamp] ")
  $consoleOutput.SelectionColor = [System.Drawing.Color]::FromName($Color)
  $consoleOutput.AppendText("$Message`r`n")
  $consoleOutput.ScrollToCaret()
}

function Update-LineCol {
  $index = $codeEditor.SelectionStart
  $line = $codeEditor.GetLineFromCharIndex($index) + 1
  $lineStart = $codeEditor.GetFirstCharIndexFromLine($line - 1)
  $col = $index - $lineStart + 1
  $lineColLabel.Text = "Ln $line, Col $col"
}

function Save-File {
  param([string]$Path)
    
  try {
    if ([string]::IsNullOrEmpty($Path)) {
      $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
      $saveDialog.Filter = "HTML Files (*.html)|*.html|All Files (*.*)|*.*"
      $saveDialog.DefaultExt = "html"
            
      if ($saveDialog.ShowDialog() -eq "OK") {
        $Path = $saveDialog.FileName
      }
      else {
        return
      }
    }
        
    $codeEditor.Text | Set-Content -Path $Path -Encoding UTF8
    $script:currentFile = $Path
    $script:isDirty = $false
    $mainForm.Text = "PowerShell HTML Browser & IDE - $([System.IO.Path]::GetFileName($Path))"
    $statusLabel.Text = "Saved: $Path"
    Log-Console "File saved: $Path"
  }
  catch {
    [System.Windows.Forms.MessageBox]::Show(
      "Failed to save file: $($_.Exception.Message)",
      "Save Error",
      "OK",
      "Error"
    )
    Log-Console "SAVE ERROR: $($_.Exception.Message)" -Color "Red"
  }
}

function Open-File {
  $openDialog = New-Object System.Windows.Forms.OpenFileDialog
  $openDialog.Filter = "HTML Files (*.html)|*.html|All Files (*.*)|*.*"
    
  if ($openDialog.ShowDialog() -eq "OK") {
    try {
      $content = Get-Content -Path $openDialog.FileName -Raw
      $codeEditor.Text = $content
      $script:currentFile = $openDialog.FileName
      $script:isDirty = $false
      $mainForm.Text = "PowerShell HTML Browser & IDE - $([System.IO.Path]::GetFileName($openDialog.FileName))"
      $statusLabel.Text = "Loaded: $($openDialog.FileName)"
      Update-Preview
      Log-Console "File opened: $($openDialog.FileName)"
    }
    catch {
      [System.Windows.Forms.MessageBox]::Show(
        "Failed to open file: $($_.Exception.Message)",
        "Open Error",
        "OK",
        "Error"
      )
      Log-Console "OPEN ERROR: $($_.Exception.Message)" -Color "Red"
    }
  }
}

# Event Handlers
$codeEditor.add_TextChanged({
    $script:isDirty = $true
    Update-LineCol
  })

$codeEditor.add_SelectionChanged({
    Update-LineCol
  })

$newFileItem.add_Click({
    if ($script:isDirty) {
      $result = [System.Windows.Forms.MessageBox]::Show(
        "Do you want to save changes?",
        "Unsaved Changes",
        "YesNoCancel",
        "Question"
      )
        
      if ($result -eq "Yes") {
        Save-File -Path $script:currentFile
      }
      elseif ($result -eq "Cancel") {
        return
      }
    }
    
    $codeEditor.Text = @"
<!DOCTYPE html>
<html>
<head>
    <title>New Document</title>
</head>
<body>
    <h1>New HTML Document</h1>
</body>
</html>
"@
    $script:currentFile = $null
    $script:isDirty = $false
    $mainForm.Text = "PowerShell HTML Browser & IDE - New Document"
    Update-Preview
    Log-Console "New file created"
  })

$openFileItem.add_Click({ Open-File })

$saveFileItem.add_Click({ Save-File -Path $script:currentFile })

$refreshItem.add_Click({ Update-Preview })

$refreshButton.add_Click({ Update-Preview })

$backButton.add_Click({
    if ($webBrowser.CanGoBack) {
      $webBrowser.GoBack()
      Log-Console "Navigated back"
    }
  })

$forwardButton.add_Click({
    if ($webBrowser.CanGoForward) {
      $webBrowser.GoForward()
      Log-Console "Navigated forward"
    }
  })

$devToolsItem.add_Click({
    if ($consolePanel.Visible) {
      $consolePanel.Visible = $false
      $consolePanel.Height = 0
      Log-Console "Console hidden"
    }
    else {
      $consolePanel.Visible = $true
      $consolePanel.Height = 200
      Log-Console "Console visible"
    }
  })

# WebBrowser Events
$webBrowser.add_DocumentCompleted({
    $urlLabel.Text = if ($webBrowser.DocumentTitle) { $webBrowser.DocumentTitle } else { "Preview" }
    Log-Console "Document loaded: $($webBrowser.DocumentTitle)"
  })

$webBrowser.add_Navigating({
    param($sender, $e)
    $statusLabel.Text = "Loading: $($e.Url)"
  })

# Form Close Event
$mainForm.add_FormClosing({
    param($sender, $e)
    
    if ($script:isDirty) {
      $result = [System.Windows.Forms.MessageBox]::Show(
        "Do you want to save changes before closing?",
        "Unsaved Changes",
        "YesNoCancel",
        "Question"
      )
        
      if ($result -eq "Yes") {
        Save-File -Path $script:currentFile
      }
      elseif ($result -eq "Cancel") {
        $e.Cancel = $true
      }
    }
  })

# Keyboard Shortcuts
$mainForm.add_KeyDown({
    param($sender, $e)
    
    if ($e.Control -and $e.KeyCode -eq "S") {
      Save-File -Path $script:currentFile
      $e.Handled = $true
    }
    elseif ($e.KeyCode -eq "F5") {
      Update-Preview
      $e.Handled = $true
    }
    elseif ($e.KeyCode -eq "F12") {
      $devToolsItem.PerformClick()
      $e.Handled = $true
    }
  })

# Initialize
Update-Preview
Log-Console "PowerShell HTML Browser & IDE initialized"
Log-Console "Press F5 to refresh preview, F12 for console"

# Show Form
[void]$mainForm.ShowDialog()
