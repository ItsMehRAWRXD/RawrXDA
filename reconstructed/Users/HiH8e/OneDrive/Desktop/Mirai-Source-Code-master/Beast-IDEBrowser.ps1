# BigDaddyG Beast IDE Browser
# PowerShell-based browser for the Beast Swarm IDE
# Uses Windows Forms WebBrowser control for native integration

param(
    [string]$InitialUrl = "file:///C:/Users/HiH8e/OneDrive/Desktop/IDEre2.html"
)

# Error handling and cleanup
$ErrorActionPreference = "Stop"
$host.UI.RawUI.WindowTitle = "BigDaddyG Beast IDE Browser"

try {
    # Load required assemblies with error handling
    Write-Host "Loading Windows Forms assemblies..."
    Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
    Write-Host "✅ Assemblies loaded successfully"

    # Create main form with error handling
    Write-Host "Creating main form..."
    $form = New-Object System.Windows.Forms.Form
    $form.Text = "🔥 BigDaddyG Beast IDE Browser"
    $form.Size = New-Object System.Drawing.Size(1200, 800)
    $form.StartPosition = "CenterScreen"
    $form.BackColor = [System.Drawing.Color]::FromArgb(10, 10, 10)
    $form.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::Sizable
    $form.MaximizeBox = $true
    $form.MinimizeBox = $true
    
    # Add form closing event handler
    $form.Add_FormClosing({
        param($sender, $e)
        try {
            Write-Host "Cleaning up browser resources..."
            if ($webBrowser) {
                $webBrowser.Dispose()
            }
        } catch {
            Write-Host "Warning during cleanup: $($_.Exception.Message)"
        }
    })

    # Create WebBrowser control with error handling
    Write-Host "Creating WebBrowser control..."
    $webBrowser = New-Object System.Windows.Forms.WebBrowser
    $webBrowser.Size = New-Object System.Drawing.Size(1180, 720)
    $webBrowser.Location = New-Object System.Drawing.Point(10, 60)
    $webBrowser.ScriptErrorsSuppressed = $true
    $webBrowser.WebBrowserShortcutsEnabled = $true
    $webBrowser.IsWebBrowserContextMenuEnabled = $true

# Create address bar
$addressBar = New-Object System.Windows.Forms.TextBox
$addressBar.Size = New-Object System.Drawing.Size(900, 25)
$addressBar.Location = New-Object System.Drawing.Point(10, 10)
$addressBar.Text = "file:///C:/Users/HiH8e/OneDrive/Desktop/IDEre2.html"
$addressBar.Font = New-Object System.Drawing.Font("Consolas", 10)

# Create navigation buttons
$goButton = New-Object System.Windows.Forms.Button
$goButton.Text = "Go"
$goButton.Size = New-Object System.Drawing.Size(60, 25)
$goButton.Location = New-Object System.Drawing.Point(920, 10)
$goButton.BackColor = [System.Drawing.Color]::FromArgb(0, 255, 136)
$goButton.ForeColor = [System.Drawing.Color]::Black
$goButton.Font = New-Object System.Drawing.Font("Arial", 8, [System.Drawing.FontStyle]::Bold)

$backButton = New-Object System.Windows.Forms.Button
$backButton.Text = "←"
$backButton.Size = New-Object System.Drawing.Size(40, 25)
$backButton.Location = New-Object System.Drawing.Point(990, 10)

$forwardButton = New-Object System.Windows.Forms.Button
$forwardButton.Text = "→"
$forwardButton.Size = New-Object System.Drawing.Size(40, 25)
$forwardButton.Location = New-Object System.Drawing.Point(1035, 10)

$refreshButton = New-Object System.Windows.Forms.Button
$refreshButton.Text = "↻"
$refreshButton.Size = New-Object System.Drawing.Size(40, 25)
$refreshButton.Location = New-Object System.Drawing.Point(1080, 10)

$homeButton = New-Object System.Windows.Forms.Button
$homeButton.Text = "🏠"
$homeButton.Size = New-Object System.Drawing.Size(50, 25)
$homeButton.Location = New-Object System.Drawing.Point(1125, 10)

# Create status bar
$statusBar = New-Object System.Windows.Forms.Label
$statusBar.Size = New-Object System.Drawing.Size(1180, 20)
$statusBar.Location = New-Object System.Drawing.Point(10, 750)
$statusBar.Text = "Ready - BigDaddyG Beast IDE Browser"
$statusBar.Font = New-Object System.Drawing.Font("Consolas", 8)
$statusBar.ForeColor = [System.Drawing.Color]::FromArgb(0, 255, 136)

# Event handlers
$goButton.Add_Click({
    try {
        $url = $addressBar.Text
        if ($url -notmatch "^(http|https|file)://") {
            $url = "file:///" + $url
            $addressBar.Text = $url
        }
        $webBrowser.Navigate($url)
        $statusBar.Text = "Loading: $url"
    } catch {
        $statusBar.Text = "Error: $($_.Exception.Message)"
    }
})

$backButton.Add_Click({
    if ($webBrowser.CanGoBack) {
        $webBrowser.GoBack()
    }
})

$forwardButton.Add_Click({
    if ($webBrowser.CanGoForward) {
        $webBrowser.GoForward()
    }
})

$refreshButton.Add_Click({
    $webBrowser.Refresh()
})

$homeButton.Add_Click({
    $addressBar.Text = "file:///C:/Users/HiH8e/OneDrive/Desktop/IDEre2.html"
    $webBrowser.Navigate($addressBar.Text)
})

$addressBar.Add_KeyDown({
    if ($_.KeyCode -eq "Enter") {
        $goButton.PerformClick()
    }
})

$webBrowser.Add_DocumentCompleted({
    $addressBar.Text = $webBrowser.Url.AbsoluteUri
    $statusBar.Text = "Loaded: $($webBrowser.DocumentTitle)"
})

$webBrowser.Add_Navigating({
    $statusBar.Text = "Navigating to: $($_.Url.AbsoluteUri)"
})

# Add controls to form
$form.Controls.Add($addressBar)
$form.Controls.Add($goButton)
$form.Controls.Add($backButton)
$form.Controls.Add($forwardButton)
$form.Controls.Add($refreshButton)
$form.Controls.Add($homeButton)
$form.Controls.Add($webBrowser)
$form.Controls.Add($statusBar)

# Load initial page
$webBrowser.Navigate("file:///C:/Users/HiH8e/OneDrive/Desktop/IDEre2.html")

# Show form
$form.ShowDialog()