#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Agentic IDE - Simplified GUI Interface
    
.DESCRIPTION
    Clean, minimal Windows.Forms interface for agentic code generation.
    Bypasses complex initialization issues by building GUI from scratch.
#>

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Script configuration
$script:RawrXDPath = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$script:ModulePath = Join-Path $script:RawrXDPath 'RawrXD-Agentic-Module.psm1'

# Import module
try {
    Import-Module $script:ModulePath -Force -ErrorAction Stop
    Enable-RawrXDAgentic -Model 'bigdaddyg-fast:latest'
} catch {
    [System.Windows.Forms.MessageBox]::Show("Failed to initialize agentic mode: $_", "Error", "OK", "Error")
    exit 1
}

# Create main form
$form = New-Object System.Windows.Forms.Form
$form.Text = "RawrXD Agentic IDE"
$form.Size = New-Object System.Drawing.Size(1000, 700)
$form.StartPosition = "CenterScreen"
$form.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$form.ForeColor = [System.Drawing.Color]::White
$form.Font = New-Object System.Drawing.Font("Courier New", 10)

# Title label
$titleLabel = New-Object System.Windows.Forms.Label
$titleLabel.Text = "🚀 RawrXD Agentic Code Generator"
$titleLabel.Location = New-Object System.Drawing.Point(10, 10)
$titleLabel.Size = New-Object System.Drawing.Size(400, 30)
$titleLabel.Font = New-Object System.Drawing.Font("Courier New", 14, [System.Drawing.FontStyle]::Bold)
$titleLabel.ForeColor = [System.Drawing.Color]::Cyan
$form.Controls.Add($titleLabel)

# ===== INPUT SECTION =====
$promptLabel = New-Object System.Windows.Forms.Label
$promptLabel.Text = "What do you want to generate?"
$promptLabel.Location = New-Object System.Drawing.Point(10, 50)
$promptLabel.Size = New-Object System.Drawing.Size(300, 20)
$promptLabel.ForeColor = [System.Drawing.Color]::LimeGreen
$form.Controls.Add($promptLabel)

$promptBox = New-Object System.Windows.Forms.TextBox
$promptBox.Multiline = $true
$promptBox.Location = New-Object System.Drawing.Point(10, 75)
$promptBox.Size = New-Object System.Drawing.Size(480, 100)
$promptBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
$promptBox.ForeColor = [System.Drawing.Color]::White
$promptBox.Text = "Create a PowerShell function to..."
$form.Controls.Add($promptBox)

# ===== ANALYSIS TYPE SECTION =====
$analysisLabel = New-Object System.Windows.Forms.Label
$analysisLabel.Text = "Analysis Type:"
$analysisLabel.Location = New-Object System.Drawing.Point(10, 185)
$analysisLabel.Size = New-Object System.Drawing.Size(150, 20)
$analysisLabel.ForeColor = [System.Drawing.Color]::LimeGreen
$form.Controls.Add($analysisLabel)

$analysisCombo = New-Object System.Windows.Forms.ComboBox
$analysisCombo.Items.AddRange(@('Generate', 'Improve', 'Debug', 'Refactor', 'Test', 'Document'))
$analysisCombo.SelectedIndex = 0
$analysisCombo.Location = New-Object System.Drawing.Point(10, 210)
$analysisCombo.Size = New-Object System.Drawing.Size(150, 25)
$analysisCombo.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
$analysisCombo.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($analysisCombo)

# ===== BUTTONS SECTION =====
$generateButton = New-Object System.Windows.Forms.Button
$generateButton.Text = "⚡ Generate"
$generateButton.Location = New-Object System.Drawing.Point(170, 210)
$generateButton.Size = New-Object System.Drawing.Size(120, 35)
$generateButton.BackColor = [System.Drawing.Color]::FromArgb(0, 100, 200)
$generateButton.ForeColor = [System.Drawing.Color]::White
$generateButton.Font = New-Object System.Drawing.Font("Courier New", 10, [System.Drawing.FontStyle]::Bold)
$form.Controls.Add($generateButton)

$copyButton = New-Object System.Windows.Forms.Button
$copyButton.Text = "📋 Copy"
$copyButton.Location = New-Object System.Drawing.Point(300, 210)
$copyButton.Size = New-Object System.Drawing.Size(100, 35)
$copyButton.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 80)
$copyButton.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($copyButton)

$clearButton = New-Object System.Windows.Forms.Button
$clearButton.Text = "🗑️ Clear"
$clearButton.Location = New-Object System.Drawing.Point(410, 210)
$clearButton.Size = New-Object System.Drawing.Size(80, 35)
$clearButton.BackColor = [System.Drawing.Color]::FromArgb(150, 50, 50)
$clearButton.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($clearButton)

# ===== OUTPUT SECTION =====
$outputLabel = New-Object System.Windows.Forms.Label
$outputLabel.Text = "Generated Code:"
$outputLabel.Location = New-Object System.Drawing.Point(10, 255)
$outputLabel.Size = New-Object System.Drawing.Size(300, 20)
$outputLabel.ForeColor = [System.Drawing.Color]::LimeGreen
$form.Controls.Add($outputLabel)

$outputBox = New-Object System.Windows.Forms.TextBox
$outputBox.Multiline = $true
$outputBox.WordWrap = $true
$outputBox.ScrollBars = "Both"
$outputBox.Location = New-Object System.Drawing.Point(10, 280)
$outputBox.Size = New-Object System.Drawing.Size(980, 400)
$outputBox.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
$outputBox.ForeColor = [System.Drawing.Color]::Cyan
$outputBox.Font = New-Object System.Drawing.Font("Courier New", 9)
$outputBox.ReadOnly = $true
$form.Controls.Add($outputBox)

# ===== STATUS BAR =====
$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "✅ Ready - Generate code or paste existing code to analyze"
$statusLabel.Location = New-Object System.Drawing.Point(10, 685)
$statusLabel.Size = New-Object System.Drawing.Size(980, 20)
$statusLabel.ForeColor = [System.Drawing.Color]::LimeGreen
$statusLabel.Font = New-Object System.Drawing.Font("Courier New", 9)
$form.Controls.Add($statusLabel)

# ===== EVENT HANDLERS =====
$generateButton.Add_Click({
    $statusLabel.Text = "⏳ Processing..."
    $form.Refresh()
    
    try {
        $analysisType = $analysisCombo.SelectedItem
        $prompt = $promptBox.Text
        
        if ($analysisType -eq 'Generate') {
            $result = Invoke-RawrXDAgenticCodeGen -Prompt $prompt
            $statusLabel.Text = "✅ Code generated successfully"
        } else {
            $result = Invoke-RawrXDAgenticAnalysis -Code $prompt -AnalysisType $analysisType.ToLower()
            $statusLabel.Text = "✅ Analysis complete"
        }
        
        $outputBox.Text = $result
    } catch {
        $outputBox.Text = "❌ Error: $($_.Exception.Message)"
        $statusLabel.Text = "❌ Error occurred"
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, "Error", "OK", "Error")
    }
})

$copyButton.Add_Click({
    try {
        [System.Windows.Forms.Clipboard]::SetText($outputBox.Text)
        $statusLabel.Text = "✅ Code copied to clipboard"
        [System.Windows.Forms.MessageBox]::Show("Code copied to clipboard!", "Success", "OK", "Information")
    } catch {
        [System.Windows.Forms.MessageBox]::Show("Failed to copy: $_", "Error", "OK", "Error")
    }
})

$clearButton.Add_Click({
    $outputBox.Text = ""
    $promptBox.Text = "Create a PowerShell function to..."
    $analysisCombo.SelectedIndex = 0
    $statusLabel.Text = "✅ Cleared"
})

# Show form
$form.ShowDialog() | Out-Null
