# RawrBrowser - Visible Test Version
# Will definitely show a window and be easy to close

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Write-Host "=== TESTING BROWSER VISIBILITY ===" -ForegroundColor Green

# First test - can we show a simple MessageBox?
Write-Host "Testing basic window display..." -ForegroundColor Yellow
[System.Windows.Forms.MessageBox]::Show("Click OK to continue to browser test", "RawrBrowser Test", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)

Write-Host "MessageBox test complete. Creating browser form..." -ForegroundColor Yellow

# Create form with explicit visibility settings
$form = New-Object System.Windows.Forms.Form
$form.Text = "RawrBrowser - Test Window"
$form.Size = New-Object System.Drawing.Size(800, 600)
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.BackColor = [System.Drawing.Color]::Red  # Bright red so we can definitely see it
$form.TopMost = $true  # Force on top
$form.ShowInTaskbar = $true

# Add a simple label instead of WebBrowser first
$label = New-Object System.Windows.Forms.Label
$label.Text = "🦖 RAWR BROWSER TEST 🦖`nIf you can see this RED window, the display is working!`nClick the X to close."
$label.Font = New-Object System.Drawing.Font("Arial", 16, [System.Drawing.FontStyle]::Bold)
$label.ForeColor = [System.Drawing.Color]::White
$label.AutoSize = $false
$label.Size = New-Object System.Drawing.Size(760, 200)
$label.Location = New-Object System.Drawing.Point(20, 20)
$label.TextAlign = [System.Drawing.ContentAlignment]::MiddleCenter

$form.Controls.Add($label)

Write-Host "Showing form..." -ForegroundColor Yellow

# Show form and wait for it to close
$form.ShowDialog() | Out-Null

Write-Host "Form closed. Test complete!" -ForegroundColor Green