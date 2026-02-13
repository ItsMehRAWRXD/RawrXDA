# RawrXD OMEGA-1 Command Reference Card

**Quick Access Guide for Production Operations**

---

## 🚀 Quick Start

### Verify System Status
```powershell
cd "D:\lazy init ide\auto_generated_methods"
powershell -NoProfile -ExecutionPolicy Bypass -File "Deploy-RawrXD-OMEGA-1.ps1" -DeploymentMode "Test" -SkipIntegration
```

### Run Full Integration
```powershell
cd "D:\lazy init ide\auto_generated_methods"
powershell -NoProfile -ExecutionPolicy Bypass -File "Deploy-RawrXD-OMEGA-1.ps1" -DeploymentMode "Full"
```

### Start Autonomous Engine
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File "D:\lazy init ide\auto_generated_methods\RawrXD-OMEGA-1-Master.ps1" -AutonomousMode $true
```

---

## 📋 Integration Components

| Component | File | Size | Purpose |
|-----------|------|------|---------|
| **Master Orchestrator** | `Deploy-RawrXD-OMEGA-1.ps1` | 3.2 KB | Entry point for all deployments |
| **Core Engine** | `RawrXD-OMEGA-1-Master.ps1` | 20.65 KB | Autonomous system with Win32 integration |
| **Integration Layer** | `RawrXD-Complete-Integration.ps1` | 14.64 KB | 28-module unified loader |
| **Technical Docs** | `OMEGA-1-COMPLETE-INTEGRATION.md` | 18 KB | Architecture & implementation details |
| **Executive Summary** | `PRODUCTION-READY-SUMMARY.md` | 9.76 KB | High-level overview & status |
| **Final Report** | `FINAL-INTEGRATION-REPORT.md` | 12 KB | Complete verification report |
| **Configuration** | `manifest.json` | 5.76 KB | System metadata & state |

---

## 🔧 Common Operations

### Check Module Status
```powershell
$modules = Get-ChildItem "D:\lazy init ide\auto_generated_methods" -Filter "RawrXD*.psm1"
Write-Host "Total modules: $($modules.Count)"
$modules | Select-Object Name, @{N="Size(KB)";E={[Math]::Round($_.Length/1KB,2)}}
```

### View System Manifest
```powershell
Get-Content "D:\lazy init ide\auto_generated_methods\manifest.json" | ConvertFrom-Json | Format-Table -AutoSize
```

### Check Deployment Logs
```powershell
Get-Content "D:\lazy init ide\auto_generated_methods\logs\deployment.log" -Tail 50
```

### Monitor Autonomous Loop
```powershell
Get-Content "D:\lazy init ide\auto_generated_methods\logs\autonomous.log" -Tail 20 -Wait
```

### Verify Module Integrity
```powershell
$manifest = Get-Content "D:\lazy init ide\auto_generated_methods\manifest.json" | ConvertFrom-Json
$manifest.moduleHashes | Format-Table -AutoSize
```

---

## 📊 System Health Checks

### Quick Health Status
```powershell
$logsPath = "D:\lazy init ide\auto_generated_methods\logs"
if (Test-Path $logsPath) {
    $files = Get-ChildItem $logsPath
    Write-Host "Total log files: $($files.Count)"
    Write-Host "Latest update: $((Get-ChildItem $logsPath | Sort-Object LastWriteTime -Desc | Select-Object -First 1).LastWriteTime)"
} else {
    Write-Host "Logs directory not found"
}
```

### Module Loading Statistics
```powershell
$integrationFile = "D:\lazy init ide\auto_generated_methods\RawrXD-Complete-Integration.ps1"
$content = Get-Content $integrationFile
$imports = $content | Select-String "Import-Module" | Measure-Object
Write-Host "Modules to import: $($imports.Count)"
```

### Deployment Phases Status
```powershell
$phases = @(
    "Environment initialization",
    "Module discovery",
    "Module loading",
    "System verification",
    "State initialization",
    "Autonomous job spawning",
    "Comprehensive reporting",
    "Continuous monitoring"
)
$phases | ForEach-Object { Write-Host "✓ $_" }
```

---

## 🔍 Troubleshooting

### If deployment fails to start
```powershell
# Check execution policy
Get-ExecutionPolicy

# Check file exists
Test-Path "D:\lazy init ide\auto_generated_methods\Deploy-RawrXD-OMEGA-1.ps1"

# Check PowerShell version
$PSVersionTable
```

### If modules fail to load
```powershell
# Check all modules exist
$modules = Get-ChildItem "D:\lazy init ide\auto_generated_methods" -Filter "RawrXD*.psm1"
Write-Host "Found $($modules.Count) modules"

# Test module syntax
$modules | ForEach-Object {
    try {
        [scriptblock]::Create((Get-Content $_.FullName -Raw)) | Out-Null
        Write-Host "✓ $($_.Name)" -ForegroundColor Green
    } catch {
        Write-Host "✗ $($_.Name): $_" -ForegroundColor Red
    }
}
```

### If autonomous loop not responding
```powershell
# Check background jobs
Get-Job | Where-Object {$_.Name -like "*RawrXD*"} | Format-Table Name, State, HasMoreData

# Resume suspended job
Resume-Job -Name "RawrXD_AutonomousLoop"
```

---

## 📈 Performance Monitoring

### Autonomous Loop Status
```powershell
$manifest = Get-Content "D:\lazy init ide\auto_generated_methods\manifest.json" | ConvertFrom-Json
Write-Host "Current Generation: $($manifest.generation)"
Write-Host "Last Mutation: $($manifest.lastMutationTime)"
Write-Host "Module Count: $($manifest.moduleCount)"
Write-Host "Integrity Hash: $($manifest.systemHash)"
```

### Memory and CPU Usage
```powershell
$psProcess = Get-Process -Name powershell, pwsh -ErrorAction SilentlyContinue
$psProcess | Format-Table ProcessName, Id, @{N="CPU(s)";E={$_.CPU}}, @{N="Memory(MB)";E={[Math]::Round($_.WorkingSet/1MB,2)}}
```

---

## 🔐 Security Operations

### Verify System Integrity
```powershell
$manifest = Get-Content "D:\lazy init ide\auto_generated_methods\manifest.json" | ConvertFrom-Json

$modules = Get-ChildItem "D:\lazy init ide\auto_generated_methods" -Filter "RawrXD*.psm1"
foreach ($module in $modules) {
    $content = Get-Content $module.FullName -Raw
    $hash = [System.Security.Cryptography.SHA256]::ComputeHash([System.Text.Encoding]::UTF8.GetBytes($content)) | ForEach-Object {$_.ToString("x2")} | Join-String
    $storedHash = $manifest.moduleHashes.($module.BaseName)
    
    if ($hash -eq $storedHash) {
        Write-Host "✓ $($module.Name) - Verified" -ForegroundColor Green
    } else {
        Write-Host "✗ $($module.Name) - MODIFIED" -ForegroundColor Red
    }
}
```

### List Security Modules
```powershell
$secModules = Get-ChildItem "D:\lazy init ide\auto_generated_methods" -Filter "*Security*", "*Crypto*", "*Encryption*"
$secModules | Format-Table Name
```

---

## 📚 Documentation Quick Links

| Document | Purpose | Location |
|----------|---------|----------|
| **FINAL-INTEGRATION-REPORT.md** | Complete status & metrics | D:\lazy init ide\auto_generated_methods\ |
| **OMEGA-1-COMPLETE-INTEGRATION.md** | Technical architecture | D:\lazy init ide\auto_generated_methods\ |
| **PRODUCTION-READY-SUMMARY.md** | Executive overview | D:\lazy init ide\auto_generated_methods\ |

---

## 🎯 Usage Examples

### Example 1: Full System Deployment
```powershell
cd "D:\lazy init ide\auto_generated_methods"
powershell -NoProfile -ExecutionPolicy Bypass -File "Deploy-RawrXD-OMEGA-1.ps1" -DeploymentMode "Full"
# System runs all integration phases and starts autonomous monitoring
```

### Example 2: Test & Verify Without Execution
```powershell
cd "D:\lazy init ide\auto_generated_methods"
powershell -NoProfile -ExecutionPolicy Bypass -File "RawrXD-Complete-Integration.ps1" -Mode "DryRun"
# Tests all phases without modifying state
```

### Example 3: Autonomous Background Operation
```powershell
# Terminal 1: Start background autonomous engine
pwsh -NoProfile -ExecutionPolicy Bypass -File "D:\lazy init ide\auto_generated_methods\RawrXD-OMEGA-1-Master.ps1" -AutonomousMode $true

# Terminal 2: Monitor logs
Get-Content "D:\lazy init ide\auto_generated_methods\logs\autonomous.log" -Wait
```

---

## ⚡ System Requirements

- **PowerShell:** 5.1 or higher (7.4+ recommended)
- **Administrator:** Required for Win32 operations
- **.NET Framework:** 4.5+ (or .NET Core 3.1+)
- **Disk Space:** 100 MB minimum
- **Memory:** 256 MB minimum for autonomous operation
- **Execution Policy:** Bypass or RemoteSigned

---

## 📞 Support & Debug

### Enable Verbose Output
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Deploy-RawrXD-OMEGA-1.ps1" -DeploymentMode "Full" -Verbose
```

### Enable Debug Output
```powershell
Set-PSDebug -Trace 1
powershell -NoProfile -ExecutionPolicy Bypass -File "Deploy-RawrXD-OMEGA-1.ps1" -DeploymentMode "Test"
Set-PSDebug -Trace 0
```

### Collect Diagnostics
```powershell
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputPath = "D:\lazy init ide\auto_generated_methods\diagnostics_$timestamp.zip"

# Collect logs and modules
Compress-Archive -Path "D:\lazy init ide\auto_generated_methods\logs", `
                       "D:\lazy init ide\auto_generated_methods\RawrXD*.psm1", `
                       "D:\lazy init ide\auto_generated_methods\manifest.json" `
                 -DestinationPath $outputPath

Write-Host "Diagnostics saved to: $outputPath"
```

---

## 🔄 Rollback Procedures

### Restore Previous State
```powershell
$backupDir = "D:\lazy init ide\auto_generated_methods\backups"
if (Test-Path $backupDir) {
    $latestBackup = Get-ChildItem $backupDir | Sort-Object LastWriteTime -Desc | Select-Object -First 1
    Copy-Item "$($latestBackup.FullName)\*" -Destination "D:\lazy init ide\auto_generated_methods\" -Force -Recurse
    Write-Host "Restored from: $($latestBackup.Name)"
}
```

### Stop Autonomous Loop
```powershell
$job = Get-Job -Name "RawrXD_AutonomousLoop" -ErrorAction SilentlyContinue
if ($job) {
    Stop-Job -Name "RawrXD_AutonomousLoop"
    Remove-Job -Name "RawrXD_AutonomousLoop"
    Write-Host "Autonomous loop stopped"
}
```

---

**Last Updated:** January 24, 2026  
**Version:** 1.0.0  
**Status:** Production Ready ✅
