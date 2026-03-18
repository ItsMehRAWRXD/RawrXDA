$ErrorActionPreference = 'Continue'
$statusFile = "d:\rawrxd\build_status.txt"
$logFile = "d:\rawrxd\build_log_final.txt"

# Clean status
Remove-Item $statusFile -ErrorAction SilentlyContinue

Write-Host "Starting build..."
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"d:\rawrxd\build_ide.bat`"" -NoNewWindow -RedirectStandardOutput $logFile -RedirectStandardError "d:\rawrxd\build_err.txt" -PassThru -Wait

$exitCode = $proc.ExitCode
Write-Host "Build exit code: $exitCode"

if ($exitCode -eq 0) {
    "BUILD_SUCCESS" | Set-Content $statusFile
    Write-Host "BUILD SUCCESS"
} else {
    "BUILD_FAILED" | Set-Content $statusFile
    Write-Host "BUILD FAILED - checking errors:"
    Get-Content "d:\rawrxd\build_err.txt" -ErrorAction SilentlyContinue | Select-String "error" | Select-Object -First 20
    # Also check main log for errors
    Get-Content $logFile -ErrorAction SilentlyContinue | Select-String "error" | Select-Object -First 20
}
