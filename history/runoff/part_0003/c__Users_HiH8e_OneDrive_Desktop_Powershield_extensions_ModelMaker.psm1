# Model Maker Extension for RawrXD IDE
# Provides custom AI model creation and training capabilities
# Author: RawrXD
# Version: 1.0.0

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Extension metadata
$global:ModelMakerExtension = @{
    Name = "Model Maker"
    Version = "1.0.0"
    Author = "RawrXD" 
    Description = "Create custom AI models from your codebase and documentation"
    Id = "model-maker"
    Capabilities = @("ModelTraining", "CustomModels", "DataProcessing")
    Dependencies = @()
    Enabled = $true
}

# Extension initialization
function Initialize-ModelMakerExtension {
    Write-DevConsole "🧠 Initializing Model Maker Extension..." "INFO"
    
    # Add to Tools menu if available
    if ($script:toolsMenu) {
        $modelMakerItem = New-Object System.Windows.Forms.ToolStripMenuItem
        $modelMakerItem.Text = "🧠 Model Maker"
        $modelMakerItem.add_Click({ Show-ModelMakerDialog })
        $script:toolsMenu.DropDownItems.Add($modelMakerItem) | Out-Null
        
        Write-DevConsole "✅ Model Maker added to Tools menu" "SUCCESS"
    }
    
    # Register extension commands
    if ($global:extensionCommands) {
        $global:extensionCommands["model-maker"] = @{
            "create-model" = { param($args) New-CustomModel @args }
            "train-from-directory" = { param($args) Train-FromDirectory @args }
            "optimize-model" = { param($args) Optimize-ExistingModel @args }
            "export-training-data" = { param($args) Export-TrainingData @args }
        }
    }
    
    Write-DevConsole "✅ Model Maker Extension loaded successfully" "SUCCESS"
}

function Show-ModelMakerDialog {
    $form = New-Object System.Windows.Forms.Form
    $form.Text = "🧠 AI Model Maker"
    $form.Size = New-Object System.Drawing.Size(800, 650)
    $form.StartPosition = "CenterScreen"
    $form.FormBorderStyle = "FixedDialog"
    $form.MaximizeBox = $false
    $form.Font = New-Object System.Drawing.Font("Segoe UI", 9)
    
    # Create tab control
    $tabControl = New-Object System.Windows.Forms.TabControl
    $tabControl.Location = New-Object System.Drawing.Point(10, 10)
    $tabControl.Size = New-Object System.Drawing.Size(760, 550)
    $form.Controls.Add($tabControl)
    
    # Create Model Tab
    $createTab = New-Object System.Windows.Forms.TabPage
    $createTab.Text = "🎯 Create Model"
    $tabControl.TabPages.Add($createTab)
    
    # Model Name
    $nameLabel = New-Object System.Windows.Forms.Label
    $nameLabel.Text = "Model Name:"
    $nameLabel.Location = New-Object System.Drawing.Point(20, 30)
    $nameLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($nameLabel)
    
    $nameBox = New-Object System.Windows.Forms.TextBox
    $nameBox.Location = New-Object System.Drawing.Point(130, 27)
    $nameBox.Size = New-Object System.Drawing.Size(300, 25)
    $nameBox.Text = "custom-model-$(Get-Date -Format 'MMdd')"
    $createTab.Controls.Add($nameBox)
    
    # Base Model
    $baseLabel = New-Object System.Windows.Forms.Label
    $baseLabel.Text = "Base Model:"
    $baseLabel.Location = New-Object System.Drawing.Point(20, 70)
    $baseLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($baseLabel)
    
    $baseCombo = New-Object System.Windows.Forms.ComboBox
    $baseCombo.Location = New-Object System.Drawing.Point(130, 67)
    $baseCombo.Size = New-Object System.Drawing.Size(300, 25)
    $baseCombo.DropDownStyle = "DropDownList"
    $baseCombo.Items.AddRange(@(
        "bigdaddyg-fast:latest",
        "bigdaddyg:latest", 
        "bigdaddyg-personalized",
        "llama3.2",
        "codellama",
        "mistral"
    ))
    $baseCombo.SelectedIndex = 0
    $createTab.Controls.Add($baseCombo)
    
    # Training Source
    $sourceLabel = New-Object System.Windows.Forms.Label
    $sourceLabel.Text = "Training Source:"
    $sourceLabel.Location = New-Object System.Drawing.Point(20, 110)
    $sourceLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($sourceLabel)
    
    $sourceCombo = New-Object System.Windows.Forms.ComboBox
    $sourceCombo.Location = New-Object System.Drawing.Point(130, 107)
    $sourceCombo.Size = New-Object System.Drawing.Size(200, 25)
    $sourceCombo.DropDownStyle = "DropDownList"
    $sourceCombo.Items.AddRange(@("D:\ (Full Drive)", "Current Project", "Custom Directory"))
    $sourceCombo.SelectedIndex = 0
    $createTab.Controls.Add($sourceCombo)
    
    $browseBtn = New-Object System.Windows.Forms.Button
    $browseBtn.Text = "Browse..."
    $browseBtn.Location = New-Object System.Drawing.Point(340, 105)
    $browseBtn.Size = New-Object System.Drawing.Size(80, 25)
    $browseBtn.add_Click({
        $folderDialog = New-Object System.Windows.Forms.FolderBrowserDialog
        if ($folderDialog.ShowDialog() -eq "OK") {
            $pathBox.Text = $folderDialog.SelectedPath
        }
    })
    $createTab.Controls.Add($browseBtn)
    
    # Path Box
    $pathLabel = New-Object System.Windows.Forms.Label
    $pathLabel.Text = "Path:"
    $pathLabel.Location = New-Object System.Drawing.Point(20, 150)
    $pathLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($pathLabel)
    
    $pathBox = New-Object System.Windows.Forms.TextBox
    $pathBox.Location = New-Object System.Drawing.Point(130, 147)
    $pathBox.Size = New-Object System.Drawing.Size(400, 25)
    $pathBox.Text = "D:\"
    $createTab.Controls.Add($pathBox)
    
    # File Types
    $typesLabel = New-Object System.Windows.Forms.Label
    $typesLabel.Text = "File Extensions:"
    $typesLabel.Location = New-Object System.Drawing.Point(20, 190)
    $typesLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($typesLabel)
    
    $typesBox = New-Object System.Windows.Forms.TextBox
    $typesBox.Location = New-Object System.Drawing.Point(130, 187)
    $typesBox.Size = New-Object System.Drawing.Size(400, 25)
    $typesBox.Text = ".ps1,.py,.js,.md,.txt,.asm,.c,.cpp,.h,.cs"
    $createTab.Controls.Add($typesBox)
    
    # Max Files
    $maxLabel = New-Object System.Windows.Forms.Label
    $maxLabel.Text = "Max Files:"
    $maxLabel.Location = New-Object System.Drawing.Point(20, 230)
    $maxLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($maxLabel)
    
    $maxNumeric = New-Object System.Windows.Forms.NumericUpDown
    $maxNumeric.Location = New-Object System.Drawing.Point(130, 227)
    $maxNumeric.Size = New-Object System.Drawing.Size(100, 25)
    $maxNumeric.Minimum = 100
    $maxNumeric.Maximum = 10000
    $maxNumeric.Value = 1000
    $createTab.Controls.Add($maxNumeric)
    
    # Specialization
    $specLabel = New-Object System.Windows.Forms.Label
    $specLabel.Text = "Specialization:"
    $specLabel.Location = New-Object System.Drawing.Point(20, 270)
    $specLabel.Size = New-Object System.Drawing.Size(100, 20)
    $createTab.Controls.Add($specLabel)
    
    $specCombo = New-Object System.Windows.Forms.ComboBox
    $specCombo.Location = New-Object System.Drawing.Point(130, 267)
    $specCombo.Size = New-Object System.Drawing.Size(200, 25)
    $specCombo.DropDownStyle = "DropDownList"
    $specCombo.Items.AddRange(@(
        "General Programming",
        "PowerShell Automation", 
        "Assembly Optimization",
        "Web Development",
        "Security Analysis",
        "Documentation"
    ))
    $specCombo.SelectedIndex = 0
    $createTab.Controls.Add($specCombo)
    
    # Progress Area
    $progressLabel = New-Object System.Windows.Forms.Label
    $progressLabel.Text = "Status: Ready"
    $progressLabel.Location = New-Object System.Drawing.Point(20, 310)
    $progressLabel.Size = New-Object System.Drawing.Size(400, 20)
    $createTab.Controls.Add($progressLabel)
    
    $progressBar = New-Object System.Windows.Forms.ProgressBar
    $progressBar.Location = New-Object System.Drawing.Point(20, 335)
    $progressBar.Size = New-Object System.Drawing.Size(500, 20)
    $createTab.Controls.Add($progressBar)
    
    $outputBox = New-Object System.Windows.Forms.TextBox
    $outputBox.Location = New-Object System.Drawing.Point(20, 365)
    $outputBox.Size = New-Object System.Drawing.Size(700, 120)
    $outputBox.Multiline = $true
    $outputBox.ScrollBars = "Vertical"
    $outputBox.ReadOnly = $true
    $outputBox.Font = New-Object System.Drawing.Font("Consolas", 8)
    $createTab.Controls.Add($outputBox)
    
    # Create Button
    $createBtn = New-Object System.Windows.Forms.Button
    $createBtn.Text = "🚀 Create Model"
    $createBtn.Location = New-Object System.Drawing.Point(540, 270)
    $createBtn.Size = New-Object System.Drawing.Size(120, 30)
    $createBtn.BackColor = [System.Drawing.Color]::LightGreen
    $createBtn.add_Click({
        Start-ModelCreation -NameBox $nameBox -BaseCombo $baseCombo -PathBox $pathBox -TypesBox $typesBox -MaxNumeric $maxNumeric -SpecCombo $specCombo -ProgressLabel $progressLabel -ProgressBar $progressBar -OutputBox $outputBox
    })
    $createTab.Controls.Add($createBtn)
    
    # Manage Tab
    $manageTab = New-Object System.Windows.Forms.TabPage
    $manageTab.Text = "📊 Manage Models"
    $tabControl.TabPages.Add($manageTab)
    
    # Model List
    $modelListView = New-Object System.Windows.Forms.ListView
    $modelListView.Location = New-Object System.Drawing.Point(20, 20)
    $modelListView.Size = New-Object System.Drawing.Size(700, 300)
    $modelListView.View = "Details"
    $modelListView.FullRowSelect = $true
    $modelListView.GridLines = $true
    $modelListView.Columns.Add("Model Name", 200)
    $modelListView.Columns.Add("Size", 100)
    $modelListView.Columns.Add("Created", 150)
    $modelListView.Columns.Add("Type", 150)
    $manageTab.Controls.Add($modelListView)
    
    # Refresh model list
    Refresh-ModelList -ListView $modelListView
    
    # Model Actions
    $actionsPanel = New-Object System.Windows.Forms.Panel
    $actionsPanel.Location = New-Object System.Drawing.Point(20, 330)
    $actionsPanel.Size = New-Object System.Drawing.Size(700, 150)
    $manageTab.Controls.Add($actionsPanel)
    
    $testBtn = New-Object System.Windows.Forms.Button
    $testBtn.Text = "🧪 Test Model"
    $testBtn.Location = New-Object System.Drawing.Point(0, 10)
    $testBtn.Size = New-Object System.Drawing.Size(100, 30)
    $testBtn.add_Click({
        if ($modelListView.SelectedItems.Count -gt 0) {
            Test-SelectedModel -ModelName $modelListView.SelectedItems[0].Text
        }
    })
    $actionsPanel.Controls.Add($testBtn)
    
    $optimizeBtn = New-Object System.Windows.Forms.Button
    $optimizeBtn.Text = "⚡ Optimize"
    $optimizeBtn.Location = New-Object System.Drawing.Point(110, 10)
    $optimizeBtn.Size = New-Object System.Drawing.Size(100, 30)
    $optimizeBtn.add_Click({
        if ($modelListView.SelectedItems.Count -gt 0) {
            Optimize-SelectedModel -ModelName $modelListView.SelectedItems[0].Text
        }
    })
    $actionsPanel.Controls.Add($optimizeBtn)
    
    $deleteBtn = New-Object System.Windows.Forms.Button
    $deleteBtn.Text = "🗑️ Delete"
    $deleteBtn.Location = New-Object System.Drawing.Point(220, 10)
    $deleteBtn.Size = New-Object System.Drawing.Size(100, 30)
    $deleteBtn.BackColor = [System.Drawing.Color]::LightCoral
    $deleteBtn.add_Click({
        if ($modelListView.SelectedItems.Count -gt 0) {
            Delete-SelectedModel -ModelName $modelListView.SelectedItems[0].Text -ListView $modelListView
        }
    })
    $actionsPanel.Controls.Add($deleteBtn)
    
    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(690, 570)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 30)
    $closeBtn.add_Click({ $form.Close() })
    $form.Controls.Add($closeBtn)
    
    $form.ShowDialog()
}

function Start-ModelCreation {
    param(
        $NameBox, $BaseCombo, $PathBox, $TypesBox, $MaxNumeric, $SpecCombo,
        $ProgressLabel, $ProgressBar, $OutputBox
    )
    
    $modelName = $NameBox.Text.Trim()
    $baseModel = $BaseCombo.SelectedItem
    $sourcePath = $PathBox.Text.Trim()
    $extensions = $TypesBox.Text.Split(',') | ForEach-Object { $_.Trim() }
    $maxFiles = [int]$MaxNumeric.Value
    $specialization = $SpecCombo.SelectedItem
    
    if ([string]::IsNullOrEmpty($modelName)) {
        [System.Windows.Forms.MessageBox]::Show("Please enter a model name.", "Error")
        return
    }
    
    $ProgressLabel.Text = "Status: Initializing..."
    $ProgressBar.Value = 10
    $OutputBox.AppendText("🧠 Starting model creation: $modelName`r`n")
    $OutputBox.AppendText("Base Model: $baseModel`r`n")
    $OutputBox.AppendText("Source Path: $sourcePath`r`n")
    $OutputBox.AppendText("Specialization: $specialization`r`n")
    $OutputBox.AppendText("`r`n")
    
    try {
        # Create the training script arguments
        $scriptPath = Join-Path $PSScriptRoot "..\Train-DDrive-Model.ps1"
        $arguments = @(
            "-BasePath", $sourcePath
            "-BaseModel", $baseModel
            "-OutputModelName", $modelName
            "-MaxFiles", $maxFiles
            "-IncludeExtensions", $extensions
        )
        
        $ProgressLabel.Text = "Status: Scanning files..."
        $ProgressBar.Value = 30
        $OutputBox.AppendText("📁 Scanning source directory for training data...`r`n")
        
        # Execute training in background
        $job = Start-Job -ScriptBlock {
            param($ScriptPath, $Args)
            & $ScriptPath @Args
        } -ArgumentList $scriptPath, $arguments
        
        # Monitor progress
        while ($job.State -eq "Running") {
            [System.Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 500
            
            if ($ProgressBar.Value -lt 90) {
                $ProgressBar.Value += 5
            }
        }
        
        $result = Receive-Job $job
        Remove-Job $job
        
        if ($result -match "success|created successfully") {
            $ProgressLabel.Text = "Status: Model created successfully!"
            $ProgressBar.Value = 100
            $OutputBox.AppendText("✅ Model '$modelName' created successfully!`r`n")
            $OutputBox.AppendText("🎉 Model is now available in RawrXD chat!`r`n")
            
            Write-DevConsole "✅ Custom model '$modelName' created via Model Maker extension" "SUCCESS"
        } else {
            $ProgressLabel.Text = "Status: Error during creation"
            $OutputBox.AppendText("❌ Error creating model. Check logs for details.`r`n")
        }
        
    } catch {
        $ProgressLabel.Text = "Status: Error"
        $OutputBox.AppendText("❌ Error: $($_.Exception.Message)`r`n")
        Write-DevConsole "❌ Model creation failed: $($_.Exception.Message)" "ERROR"
    }
}

function Refresh-ModelList {
    param($ListView)
    
    $ListView.Items.Clear()
    
    try {
        # Get list of Ollama models
        $models = & ollama list 2>$null
        if ($models) {
            foreach ($line in $models) {
                if ($line -match "^(\S+)\s+(\S+)\s+(\S+)\s+(.+)") {
                    $item = New-Object System.Windows.Forms.ListViewItem
                    $item.Text = $matches[1]
                    $item.SubItems.Add($matches[2])
                    $item.SubItems.Add($matches[3])
                    $item.SubItems.Add("Custom")
                    $ListView.Items.Add($item)
                }
            }
        }
    } catch {
        Write-DevConsole "⚠️ Could not refresh model list: $($_.Exception.Message)" "WARNING"
    }
}

function Test-SelectedModel {
    param($ModelName)
    
    $testPrompt = "Hello! Please introduce yourself and tell me about your capabilities."
    
    try {
        $body = @{
            model = $ModelName
            prompt = $testPrompt
            stream = $false
            options = @{ temperature = 0.3; num_predict = 200 }
        } | ConvertTo-Json -Depth 3
        
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $body -ContentType "application/json" -TimeoutSec 30
        
        [System.Windows.Forms.MessageBox]::Show("Model Response:`n`n$($response.response)", "Model Test - $ModelName", "OK", "Information")
        
        Write-DevConsole "✅ Model test completed: $ModelName" "SUCCESS"
        
    } catch {
        [System.Windows.Forms.MessageBox]::Show("Error testing model: $($_.Exception.Message)", "Test Error", "OK", "Error")
    }
}

function Optimize-SelectedModel {
    param($ModelName)
    
    $optimizedName = "$ModelName-optimized"
    
    try {
        # Create optimized Modelfile
        $modelfileContent = @"
FROM $ModelName

# Optimized parameters for enhanced performance
PARAMETER temperature 0.3
PARAMETER top_p 0.9
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1
PARAMETER num_ctx 8192
PARAMETER num_predict 2048

# Enhanced system prompt for better responses
SYSTEM """
You are an AI assistant optimized for high-quality responses. Provide detailed, accurate, and helpful information while maintaining a professional and friendly tone.
"""
"@
        
        $tempFile = [System.IO.Path]::GetTempFileName() + ".Modelfile"
        $modelfileContent | Out-File -FilePath $tempFile -Encoding UTF8
        
        & ollama create $optimizedName -f $tempFile
        Remove-Item $tempFile -Force
        
        [System.Windows.Forms.MessageBox]::Show("Model optimized successfully as '$optimizedName'!", "Optimization Complete", "OK", "Information")
        
        Write-DevConsole "✅ Model optimized: $ModelName -> $optimizedName" "SUCCESS"
        
    } catch {
        [System.Windows.Forms.MessageBox]::Show("Error optimizing model: $($_.Exception.Message)", "Optimization Error", "OK", "Error")
    }
}

function Delete-SelectedModel {
    param($ModelName, $ListView)
    
    $result = [System.Windows.Forms.MessageBox]::Show(
        "Are you sure you want to delete model '$ModelName'?`n`nThis action cannot be undone.",
        "Confirm Delete",
        "YesNo",
        "Warning"
    )
    
    if ($result -eq "Yes") {
        try {
            & ollama rm $ModelName
            [System.Windows.Forms.MessageBox]::Show("Model '$ModelName' deleted successfully.", "Delete Complete", "OK", "Information")
            Refresh-ModelList -ListView $ListView
            
            Write-DevConsole "🗑️ Model deleted: $ModelName" "SUCCESS"
            
        } catch {
            [System.Windows.Forms.MessageBox]::Show("Error deleting model: $($_.Exception.Message)", "Delete Error", "OK", "Error")
        }
    }
}

# Extension cleanup
function Cleanup-ModelMakerExtension {
    Write-DevConsole "🧠 Model Maker Extension cleanup complete" "INFO"
}

# Export extension functions for RawrXD
Export-ModuleMember -Function Initialize-ModelMakerExtension, Cleanup-ModelMakerExtension, Show-ModelMakerDialog -Variable ModelMakerExtension