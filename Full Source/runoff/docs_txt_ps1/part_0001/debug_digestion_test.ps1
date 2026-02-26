# Launch IDE, press Ctrl+Shift+D, and monitor output
# This script assumes the IDE is built and executable exists

$ideExe = "d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe"
$testFile = "d:\lazy init ide\src\win32app\Win32IDE.cpp"

if (!(Test-Path $ideExe)) {
    Write-Host "ERROR: IDE executable not found at $ideExe"
    exit 1
}

# Start the IDE in background
Write-Host "Starting RawrXD-Win32IDE..."
$ideProcess = Start-Process -FilePath $ideExe -PassThru -WindowStyle Normal

# Wait for IDE to fully load
Write-Host "Waiting for IDE to load (5 seconds)..."
Start-Sleep -Seconds 5

# Import the Windows Forms library for sending keystrokes
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait('%+d') # Ctrl+Shift+D

Write-Host "`nKey combination sent! Check IDE status bar for digestion messages."
Write-Host "Process ID: $($ideProcess.Id)"
Write-Host "Let the IDE run for 10 seconds to see the digestion engine output..."

# Keep IDE running for monitoring
Start-Sleep -Seconds 10

Write-Host "`nTest complete! IDE will continue running."
Write-Host "Close the IDE manually when done."
