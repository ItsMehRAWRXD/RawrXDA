Write-Host "=== SOVEREIGN NVME ORACLE - SERVICE UPDATER ===" -ForegroundColor Cyan

# Stop service
Write-Host "[*] Stopping service..." -ForegroundColor Yellow
sc.exe stop SovereignNVMeOracle | Out-Null
Start-Sleep -Seconds 2

# Copy updated binary
Write-Host "[*] Deploying updated binary..." -ForegroundColor Yellow
Copy-Item "D:\rawrxd\build\Release\nvme_oracle_service.exe" "C:\Sovereign\" -Force
Copy-Item "D:\rawrxd\build\Release\nvme_query.dll" "C:\Sovereign\" -Force

# Restart service
Write-Host "[*] Starting service..." -ForegroundColor Yellow
sc.exe start SovereignNVMeOracle | Out-Null
Start-Sleep -Seconds 2

# Check status
$svc = Get-Service SovereignNVMeOracle
Write-Host "[SUCCESS] Service Status: $($svc.Status)" -ForegroundColor Green

# Verify MMF
Write-Host "`n[*] Verifying MMF..." -ForegroundColor Yellow
try {
    $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting("Global\SOVEREIGN_NVME_TEMPS")
    $accessor = $mmf.CreateViewAccessor(0, 152)
    $signature = $accessor.ReadUInt32(0)
    $version = $accessor.ReadUInt32(4)
    $driveCount = $accessor.ReadUInt32(8)
    
    Write-Host "`n=== MMF DATA ===" -ForegroundColor Cyan
    Write-Host "Signature: 0x$($signature.ToString('X8')) $(if($signature -eq 0x534F5645){'✓ VALID'}else{'✗ INVALID'})" -ForegroundColor $(if($signature -eq 0x534F5645){'Green'}else{'Red'})
    Write-Host "Version: $version"
    Write-Host "Drive Count: $driveCount"
    Write-Host "`nDrive Temperatures:" -ForegroundColor Cyan
    for ($i = 0; $i -lt $driveCount; $i++) {
        $temp = $accessor.ReadInt32(16 + ($i * 4))
        $color = if($temp -eq -1){'Gray'}elseif($temp -gt 70){'Red'}elseif($temp -gt 55){'Yellow'}else{'Green'}
        Write-Host "  Drive ${i}: $temp °C" -ForegroundColor $color
    }
    $timestamp = $accessor.ReadInt64(144)
    Write-Host "`nLast Update: $timestamp ms" -ForegroundColor Cyan
    
    $accessor.Dispose()
    $mmf.Dispose()
    Write-Host "`n[SUCCESS] MMF is active and readable!" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] MMF not accessible: $_" -ForegroundColor Red
}
