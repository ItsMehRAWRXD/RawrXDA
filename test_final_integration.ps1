#Comprehensive IDE Digestion Integration Test

# This test will:
# 1. Start the IDE
# 2. Automatically open a test file  
# 3. Trigger Ctrl+Shift+D
# 4. Monitor for digestion completion
# 5. Verify output file creation

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$ideExe = "d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe"
$testFile = "d:\lazy init ide\src\win32app\Win32IDE.cpp"
$testOutputFile = "d:\lazy init ide\build\bin\Release\manual_test_output.json"

# Clean up any previous output
if (Test-Path $testOutputFile) { Remove-Item $testOutputFile -Force }

Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "        RawrXD Win32IDE Digestion Integration Test" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "[1] Starting IDE executable..."
Write-Host "    Path: $ideExe"

# Start the IDE
$ideProcess = Start-Process -FilePath $ideExe -PassThru
$idePid = $ideProcess.Id

if (-not $ideProcess.Responding) {
    Write-Host "    ERROR: IDE process did not start!" -ForegroundColor Red
    exit 1
}

Write-Host "    ✓ IDE started with PID $idePid"
Write-Host "    Waiting 6 seconds for IDE to fully initialize..."

Start-Sleep -Seconds 6

Write-Host ""
Write-Host "[2] Opening test file via Ctrl+O..."

# Send keyboard input via WScript.Shell
$wsh = New-Object -ComObject WScript.Shell
$wsh.AppActivate($idePid)
Start-Sleep -Milliseconds 200

# Ctrl+O to open file dialog
[System.Windows.Forms.SendKeys]::SendWait('^o')
Start-Sleep -Seconds 2

# Type the file path
Write-Host "    Typing path: $testFile"
[System.Windows.Forms.SendKeys]::SendWait($testFile)
Start-Sleep -Milliseconds 500

# Press Enter
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 3

Write-Host "    ✓ File open command sent"

# Ensure IDE is focused
$wsh.AppActivate($idePid)
Start-Sleep -Milliseconds 300

Write-Host ""
Write-Host "[3] Sending Ctrl+Shift+D (Digestion hotkey)..."
[System.Windows.Forms.SendKeys]::SendWait('^+d')
Start-Sleep -Milliseconds 500

Write-Host "    ✓ Hotkey sent to IDE"
Write-Host "    Waiting for digestion to complete (10 seconds)..."

Start-Sleep -Seconds 10

Write-Host ""
Write-Host "[4] Verifying results..."

# Check for output file
if (Test-Path $testOutputFile) {
    Write-Host "    ✓ Output file created!" -ForegroundColor Green
    $fileSize = (Get-Item $testOutputFile).Length
    Write-Host "    File size: $fileSize bytes"
    Write-Host ""
    Write-Host "=== Digestion Output ===" -ForegroundColor Cyan
    Get-Content $testOutputFile
    Write-Host "═════════════════════════" -ForegroundColor Cyan
} else {
    Write-Host "    ✗ No output file found" -ForegroundColor Red
    Write-Host "    Expected: $testOutputFile"
    Write-Host ""
    Write-Host "POSSIBLE ISSUES:" -ForegroundColor Yellow
    Write-Host "  1. File wasn't opened (Ctrl+O dialog may have failed)"
    Write-Host "  2. Ctrl+Shift+D hotkey wasn't received by IDE"
    Write-Host "  3. IDE lost focus during hotkey send"
    Write-Host "  4. Digestion engine failed silently"
    Write-Host ""
    Write-Host "DEBUG: Check IDE console/output for error messages"
}

Write-Host ""
Write-Host "[5] Cleaning up..."
Stop-Process -Id $idePid -Force -ErrorAction SilentlyContinue
Write-Host "    ✓ IDE process terminated"

Write-Host ""
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Test Complete" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
