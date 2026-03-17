# Beast IDE Browser - Fixed Pipeline Version
# Handles async operations properly to avoid PipelineStoppedException

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Create main form
$form = New-Object System.Windows.Forms.Form
$form.Text = "🧠 BigDaddyG Beast IDE - Browser Integration"
$form.Size = New-Object System.Drawing.Size(1400, 900)
$form.StartPosition = "CenterScreen"
$form.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$form.Font = New-Object System.Drawing.Font("Segoe UI", 10)

# Create WebBrowser control
$browser = New-Object System.Windows.Forms.WebBrowser
$browser.Dock = [System.Windows.Forms.DockStyle]::Fill
$form.Controls.Add($browser)

# Create toolbar panel
$toolPanel = New-Object System.Windows.Forms.Panel
$toolPanel.Height = 50
$toolPanel.Dock = [System.Windows.Forms.DockStyle]::Top
$toolPanel.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
$form.Controls.Add($toolPanel)

# Bring browser to front
$browser.BringToFront()

# Create buttons
$loadButton = New-Object System.Windows.Forms.Button
$loadButton.Text = "📂 Load IDE"
$loadButton.Size = New-Object System.Drawing.Size(120, 35)
$loadButton.Location = New-Object System.Drawing.Point(10, 7)
$loadButton.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
$loadButton.ForeColor = [System.Drawing.Color]::White
$loadButton.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$toolPanel.Controls.Add($loadButton)

$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "Ready..."
$statusLabel.Size = New-Object System.Drawing.Size(300, 35)
$statusLabel.Location = New-Object System.Drawing.Point(140, 7)
$statusLabel.ForeColor = [System.Drawing.Color]::FromArgb(200, 200, 200)
$statusLabel.AutoSize = $false
$toolPanel.Controls.Add($statusLabel)

$refreshButton = New-Object System.Windows.Forms.Button
$refreshButton.Text = "🔄 Refresh"
$refreshButton.Size = New-Object System.Drawing.Size(100, 35)
$refreshButton.Location = New-Object System.Drawing.Point(450, 7)
$refreshButton.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
$refreshButton.ForeColor = [System.Drawing.Color]::White
$refreshButton.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$toolPanel.Controls.Add($refreshButton)

# Get IDE file path
$ideFilePath = Join-Path $PSScriptRoot "IDEre2.html"
if (-not (Test-Path $ideFilePath)) {
    $ideFilePath = Join-Path ([System.IO.Path]::GetDirectoryName($PSScriptRoot)) "IDEre2.html"
}

# Safe async operation handler
$scriptBlockForLoad = {
    try {
        $absolutePath = [System.IO.Path]::GetFullPath($ideFilePath)
        
        if (-not (Test-Path $absolutePath)) {
            $statusLabel.Text = "❌ File not found: $absolutePath"
            [System.Windows.Forms.MessageBox]::Show("IDE file not found at: $absolutePath", "Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
            return
        }
        
        $fileUri = "file:///$($absolutePath -replace '\\', '/')"
        $statusLabel.Text = "📂 Loading IDE from: $absolutePath"
        
        # Use Invoke method to navigate safely
        $browser.Navigate($fileUri)
        
        # Set up DocumentCompleted event
        $docCompletedHandler = {
            param($sender, $e)
            $statusLabel.Text = "✅ IDE Loaded Successfully - Ready to use GGUF models!"
        }
        
        $browser.add_DocumentCompleted($docCompletedHandler)
    }
    catch {
        $statusLabel.Text = "❌ Error: $($_.Exception.Message)"
        Write-Error "Failed to load IDE: $_"
    }
}

# Attach event handlers safely
$loadButton.Add_Click({
    & $scriptBlockForLoad
})

$refreshButton.Add_Click({
    try {
        $browser.Refresh()
        $statusLabel.Text = "🔄 Refreshing..."
    }
    catch {
        $statusLabel.Text = "❌ Refresh failed: $($_.Exception.Message)"
    }
})

# Auto-load IDE on startup
$form.Add_Shown({
    $statusLabel.Text = "⏳ Initializing GGUF Model Browser..."
    Start-Sleep -Milliseconds 500
    & $scriptBlockForLoad
})

# Show form
[void]$form.ShowDialog()
$form.Dispose()
