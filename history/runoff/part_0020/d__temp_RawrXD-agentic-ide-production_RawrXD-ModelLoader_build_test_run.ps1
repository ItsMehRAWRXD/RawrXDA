# Test runner with timeout
$proc = Start-Process -FilePath ".\bin\Release\test_chat_streaming.exe" `
    -PassThru `
    -RedirectStandardOutput "test_stdout.txt" `
    -RedirectStandardError "test_stderr.txt" `
    -NoNewWindow

Write-Host "Process started with PID: $($proc.Id)"
Write-Host "Waiting for 10 seconds..."

$completed = $proc.WaitForExit(10000)

if (-not $completed) {
    Write-Host "Process did not complete within 10 seconds. Killing..."
    $proc.Kill()
    Write-Host "Process killed. Checking output..."
} else {
    Write-Host "Process completed with exit code: $($proc.ExitCode)"
}

Write-Host "`n=== STDOUT ==="
Get-Content "test_stdout.txt" -ErrorAction SilentlyContinue

Write-Host "`n=== STDERR ==="
Get-Content "test_stderr.txt" -ErrorAction SilentlyContinue

Write-Host "`n=== Test complete ==="
