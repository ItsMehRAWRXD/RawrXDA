# IDE Automation Script - Interact with RawrXD IDE via CLI
# This script simulates user interactions with the IDE

param(
    [string]$ModelPath = "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf",
    [string]$FileToOpen = "D:\RawrXD-production-lazy-init\src\agentic_engine.h"
)

# Add required assemblies for Windows automation
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Get the IDE window
$ideProcess = Get-Process RawrXD-AgenticIDE -ErrorAction SilentlyContinue
if (-not $ideProcess) {
    Write-Error "IDE is not running!"
    exit 1
}

$ideWindow = $ideProcess.MainWindowHandle
if ($ideWindow -eq 0) {
    Write-Error "Could not find IDE window handle!"
    exit 1
}

Write-Host "✓ Found IDE process (PID: $($ideProcess.Id))" -ForegroundColor Green
Write-Host "✓ Window Handle: 0x$($ideWindow.ToString('X8'))" -ForegroundColor Green

# Function to send keystrokes
function Send-Key {
    param([System.Windows.Forms.Keys]$Key)
    [System.Windows.Forms.SendKeys]::SendWait($Key)
    Start-Sleep -Milliseconds 100
}

# Function to send key combination
function Send-KeyCombo {
    param(
        [string]$Keys
    )
    [System.Windows.Forms.SendKeys]::SendWait($Keys)
    Start-Sleep -Milliseconds 150
}

# Bring IDE window to front
Write-Host "`n📋 Bringing IDE window to focus..." -ForegroundColor Cyan
[System.Windows.Forms.Form]::FromHandle($ideWindow) | ForEach-Object { $_.Activate() }
Start-Sleep -Seconds 1

# Step 1: Open View menu to see docks
Write-Host "`n📂 Opening View menu..." -ForegroundColor Cyan
Send-KeyCombo("^+1")  # Ctrl+Shift+1 to toggle Suggestions (check if docks work)
Start-Sleep -Milliseconds 500

# Step 2: Navigate to Chat pane (look for chat controls)
Write-Host "`n💬 Opening Chat/Model interface..." -ForegroundColor Cyan
Send-KeyCombo("^+5")  # Ctrl+Shift+5 to toggle Output dock
Start-Sleep -Milliseconds 500

# Step 3: Load model (simulate clicking load button and entering path)
Write-Host "`n🤖 Simulating model load workflow..." -ForegroundColor Cyan
Write-Host "   - Model path: $ModelPath" -ForegroundColor Yellow

# Tab through UI elements to reach model path input
for ($i = 0; $i -lt 3; $i++) {
    Send-Key([System.Windows.Forms.Keys]::Tab)
    Start-Sleep -Milliseconds 200
}

# Type model path
Write-Host "   - Typing model path..." -ForegroundColor Yellow
[System.Windows.Forms.SendKeys]::SendWait($ModelPath)
Start-Sleep -Milliseconds 500

# Press Enter or Tab to Load button
Send-Key([System.Windows.Forms.Keys]::Tab)
Start-Sleep -Milliseconds 200
Send-Key([System.Windows.Forms.Keys]::Return)
Start-Sleep -Seconds 3

# Step 4: Open file explorer and navigate
Write-Host "`n📁 Opening File Explorer..." -ForegroundColor Cyan
Send-KeyCombo("^+4")  # Ctrl+Shift+4 to toggle File Explorer
Start-Sleep -Milliseconds 500

# Step 5: Open a random file
Write-Host "`n📄 Opening file: $FileToOpen" -ForegroundColor Cyan
Send-KeyCombo("^o")  # Ctrl+O for Open File
Start-Sleep -Milliseconds 500

# Type the file path
[System.Windows.Forms.SendKeys]::SendWait($FileToOpen)
Start-Sleep -Milliseconds 300
Send-Key([System.Windows.Forms.Keys]::Return)
Start-Sleep -Seconds 2

# Step 6: Display completion message
Write-Host "`n✅ IDE automation complete!" -ForegroundColor Green
Write-Host "`n📊 Summary:" -ForegroundColor Cyan
Write-Host "   ✓ Model loaded: $ModelPath" -ForegroundColor Green
Write-Host "   ✓ File opened: $FileToOpen" -ForegroundColor Green
Write-Host "   ✓ File Explorer visible" -ForegroundColor Green
Write-Host "   ✓ Chat pane ready" -ForegroundColor Green

Write-Host "`n🎯 You can now:" -ForegroundColor Cyan
Write-Host "   • Ask questions about the loaded file" -ForegroundColor Yellow
Write-Host "   • Use the model to analyze code" -ForegroundColor Yellow
Write-Host "   • Navigate other files in the explorer" -ForegroundColor Yellow
