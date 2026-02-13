# Test file opening in RawrXD
# This script tests if files can be opened and displayed properly

Write-Host "🧪 Testing RawrXD file opening functionality..." -ForegroundColor Cyan

# Create a test file
$testFilePath = Join-Path $PSScriptRoot "test-file.txt"
$testContent = @"
This is a test file.
It should be visible when opened in RawrXD.
If you can see this text, file opening is working correctly.
"@

Set-Content -Path $testFilePath -Value $testContent -Encoding UTF8
Write-Host "✅ Created test file: $testFilePath" -ForegroundColor Green

# Test reading the file content
$content = [System.IO.File]::ReadAllText($testFilePath)
Write-Host "📖 File content read: $($content.Length) characters" -ForegroundColor Yellow
Write-Host "Content preview: $($content.Substring(0, [Math]::Min(50, $content.Length)))..." -ForegroundColor Gray

# Test RichTextBox creation and content assignment
Add-Type -AssemblyName System.Windows.Forms
$testEditor = New-Object System.Windows.Forms.RichTextBox
$testEditor.Text = $content
$testEditor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$testEditor.ForeColor = [System.Drawing.Color]::FromArgb(255, 255, 255)
$testEditor.Font = New-Object System.Drawing.Font("Segoe UI", 9)

Write-Host "✅ RichTextBox created and content assigned" -ForegroundColor Green
Write-Host "Text length in editor: $($testEditor.Text.Length)" -ForegroundColor Yellow

# Test the Set-EditorTextWithVisibility function if it exists
if (Get-Command Set-EditorTextWithVisibility -ErrorAction SilentlyContinue) {
    Write-Host "🔧 Testing Set-EditorTextWithVisibility function..." -ForegroundColor Cyan
    Set-EditorTextWithVisibility -Content $content
    Write-Host "✅ Set-EditorTextWithVisibility executed" -ForegroundColor Green
} else {
    Write-Host "⚠️ Set-EditorTextWithVisibility function not found" -ForegroundColor Yellow
}

Write-Host "🎯 Test complete. Check if RawrXD can open $testFilePath" -ForegroundColor Cyan