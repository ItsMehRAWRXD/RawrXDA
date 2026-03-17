# Test RawrXD file opening simulation
# This simulates the file opening process to debug the issue

Write-Host "🧪 Testing RawrXD file opening simulation..." -ForegroundColor Cyan

# Load the RawrXD script to get access to functions
Write-Host "📚 Loading RawrXD functions..." -ForegroundColor Yellow
. ".\RawrXD.ps1" 2>$null

# Create a test file
$testFilePath = Join-Path $PSScriptRoot "test-file.txt"
$testContent = @"
This is a test file for RawrXD.
It contains multiple lines.
If file opening works, you should see this content.
Testing 1, 2, 3...
"@

Set-Content -Path $testFilePath -Value $testContent -Encoding UTF8
Write-Host "✅ Created test file: $testFilePath" -ForegroundColor Green

# Simulate the file reading process
Write-Host "📖 Simulating file reading..." -ForegroundColor Yellow
$content = [System.IO.File]::ReadAllText($testFilePath)
Write-Host "📊 Content length: $($content.Length) characters" -ForegroundColor Cyan

# Create a mock editor for testing
Write-Host "🔧 Creating mock editor..." -ForegroundColor Yellow
Add-Type -AssemblyName System.Windows.Forms
$script:editor = New-Object System.Windows.Forms.RichTextBox
$script:editor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$script:editor.ForeColor = [System.Drawing.Color]::FromArgb(255, 255, 255)
$script:editor.Font = New-Object System.Drawing.Font("Segoe UI", 9)

# Test the Set-EditorTextWithVisibility function
Write-Host "🎯 Testing Set-EditorTextWithVisibility..." -ForegroundColor Cyan
Set-EditorTextWithVisibility -Content $content

Write-Host "📝 Editor text length after setting: $($script:editor.Text.Length)" -ForegroundColor Green
Write-Host "👁️ Editor visible text preview: $($script:editor.Text.Substring(0, [Math]::Min(50, $script:editor.Text.Length)))..." -ForegroundColor Magenta

# Check if the text is actually set
if ($script:editor.Text -eq $content) {
    Write-Host "✅ Text matches expected content" -ForegroundColor Green
} else {
    Write-Host "❌ Text does not match expected content" -ForegroundColor Red
    Write-Host "Expected length: $($content.Length), Actual length: $($script:editor.Text.Length)" -ForegroundColor Red
}

Write-Host "🎯 Test complete" -ForegroundColor Cyan