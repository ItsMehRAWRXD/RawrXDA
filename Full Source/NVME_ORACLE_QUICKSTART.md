# NVMe Oracle Service - Quick Start Guide

## Current Status
✅ **Binary built**: `D:\rawrxd\build\Release\nvme_oracle_service.exe`  
✅ **Installer ready**: `D:\rawrxd\scripts\Quick-Install-NVMeService.ps1`  
⏳ **Service not installed** - requires Administrator privileges

## Quick Install (Run as Administrator)

1. Open **PowerShell as Administrator** (right-click → Run as administrator)
2. Run the installer:
   ```powershell
   & "D:\rawrxd\scripts\Quick-Install-NVMeService.ps1"
   ```

## Manual Installation (Run as Administrator)

```powershell
# 1. Create target directory
New-Item -Path "C:\Sovereign" -ItemType Directory -Force

# 2. Copy binary
Copy-Item "D:\rawrxd\build\Release\nvme_oracle_service.exe" "C:\Sovereign\"

# 3. Create service
sc.exe create "SovereignNVMeOracle" binPath= "C:\Sovereign\nvme_oracle_service.exe" start= auto obj= "LocalSystem" DisplayName= "Sovereign NVMe Thermal Oracle"

# 4. Start service
sc.exe start "SovereignNVMeOracle"
```

## Test Console Mode (as Administrator)

```powershell
# Run directly in console (not as service)
& "D:\rawrxd\build\Release\nvme_oracle_service.exe"
```

Expected output:
```
Sovereign NVMe Oracle - Standalone Test Mode
Drive 0: 41 C
Drive 1: 36 C
Drive 2: 35 C
Drive 3: -2 (IOCTL failed - not NVMe?)
Drive 4: -2 (IOCTL failed - not NVMe?)
Drive 5: -1 (cannot open drive)
```

## Verify MMF Access

After service is running:
```powershell
# Read temperatures from MMF
$mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting("Global\SOVEREIGN_NVME_TEMPS")
$accessor = $mmf.CreateViewAccessor(0, 152)
$signature = $accessor.ReadUInt32(0)
$driveCount = $accessor.ReadUInt32(8)
Write-Host "Signature: 0x$($signature.ToString('X8'))"
Write-Host "Drive Count: $driveCount"
for ($i = 0; $i -lt $driveCount; $i++) {
    $temp = $accessor.ReadInt32(16 + ($i * 4))
    Write-Host "Drive ${i}: $temp C"
}
$accessor.Dispose()
$mmf.Dispose()
```

## Service Management

```powershell
# Check status
sc.exe query SovereignNVMeOracle

# Stop
sc.exe stop SovereignNVMeOracle

# Start
sc.exe start SovereignNVMeOracle

# Remove
sc.exe delete SovereignNVMeOracle
```

## MMF Layout

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Signature (0x534F5645 = "SOVE") |
| 4 | 4 | Version (1) |
| 8 | 4 | DriveCount (5) |
| 12 | 4 | Reserved |
| 16 | 64 | Temperatures[16] (int32 each) |
| 144 | 8 | Timestamp (GetTickCount64) |

## Error Codes

When running in console test mode:
- `-1` = Cannot open drive (permission denied or drive doesn't exist)
- `-2` = IOCTL failed (drive exists but doesn't support temperature query - not NVMe)
- `-3` = No temperature sensors found
- `-4` = Temperature value out of range

## Architecture

```
┌─────────────────────────────────────────────────┐
│           Windows Service Manager               │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│         nvme_oracle_service.exe                 │
│  ┌───────────────────────────────────────────┐  │
│  │  ServiceMain()                            │  │
│  │    ├─ CreateFileMapping(Global\...)       │  │
│  │    └─ Loop every 1s:                      │  │
│  │         QueryNVMeTemp(0..4) → MMF         │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│       Global\SOVEREIGN_NVME_TEMPS (MMF)         │
│  [Signature][Ver][Count][...][Temps][Timestamp] │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│           IDE / Test Harness                    │
│       OpenExisting("Global\...") → Read         │
└─────────────────────────────────────────────────┘
```
