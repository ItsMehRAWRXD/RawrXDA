# Attempts to stop running AgentTools server processes
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'pwsh' -and $_.CommandLine -match 'AgentToolsServer.ps1' } | ForEach-Object {
    try {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction Stop
        Write-Host "Stopped AgentTools server process PID=$($_.ProcessId)"
    } catch {
        Write-Warning "Failed to stop PID=$($_.ProcessId): $_"
    }
}
