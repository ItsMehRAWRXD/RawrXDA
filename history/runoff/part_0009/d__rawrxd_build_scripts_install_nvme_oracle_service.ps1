#Requires -RunAsAdministrator
param(
    [string]$ServiceName = "SovereignNVMeOracle",
    [string]$BinaryPath = "D:\rawrxd\build\nvme_oracle_service.exe"
)

Write-Host "Installing service $ServiceName with binary $BinaryPath"

# Stop and delete if exists
$svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($svc) {
    if ($svc.Status -ne 'Stopped') {
        Write-Host "Stopping existing service..."
        Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
    }
    Write-Host "Removing existing service..."
    sc.exe delete $ServiceName | Out-Null
    Start-Sleep -Seconds 1
}

# Create service (LocalSystem)
$create = sc.exe create $ServiceName binPath= '"' + $BinaryPath + '"' start= auto obj= LocalSystem
Write-Host $create

# Allow service to interact with desktop disabled; no special privileges needed beyond LocalSystem

# Start service
Start-Sleep -Seconds 1
Write-Host "Starting service..."
sc.exe start $ServiceName | Out-Null

Write-Host "Service installed and started."
