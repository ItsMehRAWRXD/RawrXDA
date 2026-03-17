# Advanced test script to validate IDE digestion integration
# This script will:
# 1. Start IDE
# 2. Open a test file via Ctrl+O
# 3. Send Ctrl+Shift+D
# 4. Capture any debug output

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Start IDE
$ideExe = "d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe"
$testSourceFile = "d:\lazy init ide\src\win32app\Win32IDE.cpp"

Write-Host "=== IDE Digestion Integration Test ===" -ForegroundColor Cyan
Write-Host "Starting IDE: $ideExe"

$ideProcess = Start-Process -FilePath $ideExe -PassThru
$idePid = $ideProcess.Id

Write-Host "IDE PID: $idePid"
Write-Host "Waiting 8 seconds for IDE to fully load..."
Start-Sleep -Seconds 8

# Get the IDE window
$wsh = New-Object -ComObject WScript.Shell

# Open file dialog with Ctrl+O
Write-Host "`nSending Ctrl+O (Open File dialog)..."
$wsh.AppActivate($idePid)
Start-Sleep -Milliseconds 300
[System.Windows.Forms.SendKeys]::SendWait('^o')
Start-Sleep -Seconds 2

# Type the file path
Write-Host "Typing file path..."
[System.Windows.Forms.SendKeys]::SendWait($testSourceFile)
Start-Sleep -Milliseconds 500

# Press Enter to open
Write-Host "Pressing Enter to open file..."
[System.Windows.Forms.SendKeys]::SendWait([System.Windows.Forms.SendKeys]::SendWait('{ENTER}'))
Start-Sleep -Seconds 3

# Now send Ctrl+Shift+D
Write-Host "`nSending Ctrl+Shift+D (Digestion hotkey)..."
$wsh.AppActivate($idePid)
Start-Sleep -Milliseconds 300
[System.Windows.Forms.SendKeys]::SendWait('^+d')
Start-Sleep -Seconds 1

Write-Host "`nDigestion hotkey sent! IDE should now show progress in status bar."
Write-Host "Check the status bar for digestion progress updates (0% → 100%)"
Write-Host "`nLet IDE run for 15 seconds to complete digestion..."
Start-Sleep -Seconds 15

# Check if output file was created
$outputFile = $testSourceFile + ".digest"
if (Test-Path $outputFile) {
    Write-Host "`n✓ SUCCESS: Output file created at $outputFile" -ForegroundColor Green
    $fileSize = (Get-Item $outputFile).Length
    Write-Host "  File size: $fileSize bytes"
    
    # Show file content
    Write-Host "`n=== Digestion Output ===" -ForegroundColor Cyan
    Get-Content $outputFile | Format-List
} else {
    Write-Host "`n✗ FAILURE: No output file found at $outputFile" -ForegroundColor Red
    Write-Host "  This suggests the digestion hotkey did not trigger"
}

Write-Host "`nTest complete! IDE is still running."
Write-Host "You can close the IDE manually or Ctrl+C to exit."
