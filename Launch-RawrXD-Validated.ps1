# Launch-RawrXD-Validated.ps1
# Starts the IDE with WSOD watchdog monitoring

$watchdog = Start-Process powershell -ArgumentList "-File D:\rawrxd\RawrXD-WSOD-Watchdog.ps1 -StartIDE -AutoHeal -LogDir D:\rawrxd\Logs" -WindowStyle Hidden -PassThru

Write-Host "WSOD Watchdog active (PID: $($watchdog.Id))"
Write-Host "Logs: D:\rawrxd\Logs\"
