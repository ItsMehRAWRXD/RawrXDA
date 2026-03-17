# Simple PowerShell GUI Test - Proves PowerShell CAN create GUIs!

# Ensure we're in STA mode (required for GUIs)
if ($Host.Runspace.ApartmentState -ne 'STA') {
  Write-Host "Restarting in STA mode for GUI support..." -ForegroundColor Yellow
  PowerShell.exe -STA -File $PSCommandPath
  exit
}

# Load required assemblies
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Write-Host "✓ PowerShell GUI Test Starting..." -ForegroundColor Green
Write-Host "✓ STA Mode: $($Host.Runspace.ApartmentState)" -ForegroundColor Green
Write-Host "✓ Windows.Forms Assembly Loaded" -ForegroundColor Green

# Create a simple form
$form = New-Object System.Windows.Forms.Form
$form.Text = "PowerShell GUI Test - IT WORKS!"
$form.Size = New-Object System.Drawing.Size(500, 350)
$form.StartPosition = "CenterScreen"
$form.BackColor = [System.Drawing.Color]::LightBlue

# Add a label
$label = New-Object System.Windows.Forms.Label
$label.Text = "✓ PowerShell CAN create GUIs!"
$label.Location = New-Object System.Drawing.Point(50, 30)
$label.Size = New-Object System.Drawing.Size(400, 30)
$label.Font = New-Object System.Drawing.Font("Arial", 14, [System.Drawing.FontStyle]::Bold)
$label.ForeColor = [System.Drawing.Color]::DarkBlue
$form.Controls.Add($label)

# Add info labels
$infoLabel1 = New-Object System.Windows.Forms.Label
$infoLabel1.Text = "PowerShell Version: $($PSVersionTable.PSVersion)"
$infoLabel1.Location = New-Object System.Drawing.Point(50, 80)
$infoLabel1.Size = New-Object System.Drawing.Size(400, 20)
$form.Controls.Add($infoLabel1)

$infoLabel2 = New-Object System.Windows.Forms.Label
$infoLabel2.Text = "Apartment State: $($Host.Runspace.ApartmentState)"
$infoLabel2.Location = New-Object System.Drawing.Point(50, 110)
$infoLabel2.Size = New-Object System.Drawing.Size(400, 20)
$form.Controls.Add($infoLabel2)

$infoLabel3 = New-Object System.Windows.Forms.Label
$infoLabel3.Text = ".NET Version: $([System.Environment]::Version)"
$infoLabel3.Location = New-Object System.Drawing.Point(50, 140)
$infoLabel3.Size = New-Object System.Drawing.Size(400, 20)
$form.Controls.Add($infoLabel3)

# Add a textbox
$textbox = New-Object System.Windows.Forms.TextBox
$textbox.Text = "Type something here..."
$textbox.Location = New-Object System.Drawing.Point(50, 180)
$textbox.Size = New-Object System.Drawing.Size(300, 25)
$form.Controls.Add($textbox)

# Add a button
$button = New-Object System.Windows.Forms.Button
$button.Text = "Click Me!"
$button.Location = New-Object System.Drawing.Point(50, 220)
$button.Size = New-Object System.Drawing.Size(100, 30)
$button.BackColor = [System.Drawing.Color]::LightGreen
$button.add_Click({
    [System.Windows.Forms.MessageBox]::Show(
      "You clicked the button!`n`nText from box: '$($textbox.Text)'`n`nPowerShell GUIs work perfectly!",
      "Button Clicked",
      "OK",
      "Information"
    )
  })
$form.Controls.Add($button)

# Add an exit button
$exitButton = New-Object System.Windows.Forms.Button
$exitButton.Text = "Exit"
$exitButton.Location = New-Object System.Drawing.Point(200, 220)
$exitButton.Size = New-Object System.Drawing.Size(100, 30)
$exitButton.BackColor = [System.Drawing.Color]::LightCoral
$exitButton.add_Click({ $form.Close() })
$form.Controls.Add($exitButton)

# Add a progress bar
$progressBar = New-Object System.Windows.Forms.ProgressBar
$progressBar.Location = New-Object System.Drawing.Point(50, 270)
$progressBar.Size = New-Object System.Drawing.Size(300, 20)
$progressBar.Value = 75
$form.Controls.Add($progressBar)

$progressLabel = New-Object System.Windows.Forms.Label
$progressLabel.Text = "Progress: 75%"
$progressLabel.Location = New-Object System.Drawing.Point(360, 270)
$progressLabel.Size = New-Object System.Drawing.Size(100, 20)
$form.Controls.Add($progressLabel)

Write-Host "✓ GUI Created Successfully - Showing Form..." -ForegroundColor Green

# Show the form
[void]$form.ShowDialog()

Write-Host "✓ GUI Test Completed Successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "CONCLUSION: PowerShell CAN absolutely create GUIs!" -ForegroundColor Cyan
Write-Host "- Windows.Forms works perfectly" -ForegroundColor White
Write-Host "- STA mode enables proper GUI support" -ForegroundColor White
Write-Host "- All standard controls are available" -ForegroundColor White
Write-Host "- Event handling works correctly" -ForegroundColor White