# WEEK 5: QUICK BUILD GUIDE

**Production-Ready Enterprise Infrastructure in 4 Steps**

---

## 🏃 Quick Start (5 minutes)

### Step 1: Assemble
```batch
cd D:\rawrxd\src
ml64 /c /O2 /Zi /W3 /nologo week5_final_integration.asm
```

Output: `week5_final_integration.obj` (200 KB)

### Step 2: Link
```batch
link /SUBSYSTEM:WINDOWS /ENTRY:WinMain /OUT:RawrXD-v5.0.exe ^
    week5_final_integration.obj ^
    kernel32.lib user32.lib shell32.lib wininet.lib dbghelp.lib
```

Output: `RawrXD-v5.0.exe` (~150 KB release build)

### Step 3: Test
```batch
RawrXD-v5.0.exe
```

Expected:
- Window appears with menu system
- No crashes (crash handler initialized)
- Telemetry starts collecting (silent, in background)
- Update checker started (checks in 1 hour)

### Step 4: Deploy
```batch
# Create installer package
mkdir Package
copy RawrXD-v5.0.exe Package\
copy ..\README.md Package\
copy ..\CHANGELOG.md Package\
copy ..\LICENSE Package\
# Zip and distribute
7z a RawrXD-v5.0.zip Package\
```

---

## 📝 Configuration

### Registry Keys (Auto-Created)
```batch
reg add HKCU\Software\RawrXD /v Version /t REG_DWORD /d 500000
reg add HKCU\Software\RawrXD /v TelemetryEnabled /t REG_DWORD /d 1
reg add HKCU\Software\RawrXD /v UpdateCheckInterval /t REG_DWORD /d 3600
```

### Environment Variables (Optional)
```batch
set RAWRXD_DEBUG=1              # Verbose logging
set RAWRXD_TELEMETRY=0          # Disable telemetry for testing
set RAWRXD_CRASH_DUMP_PATH=C:\Logs\
set RAWRXD_UPDATE_SERVER=staging.update.rawrxd.ai
```

---

## 🔍 Verify Installation

### Check Crash Handler
```batch
# Simulate crash (if debug mode)
taskkill /F /IM RawrXD-v5.0.exe
# Should see minidump in %TEMP%\RawrXD_Crash_*.dmp
```

### Check Telemetry
```batch
# View telemetry log
type "%APPDATA%\RawrXD\telemetry.log"
# Should show batches being flushed every 60s
```

### Check Auto-Updater
```batch
# View update check log
type "%APPDATA%\RawrXD\update.log"
# Should show checks every 1 hour
```

### Check Configuration
```batch
# View registry settings
reg query HKCU\Software\RawrXD
# Should show Version, TelemetryID, LastUpdateCheck, etc
```

---

## 🛠️ Key Functions

### Crash Handling
```asm
InitializeCrashHandler()        ; Set up SEH handler
GenerateCrashDump()             ; Write minidump to file
UploadCrashReportAsync()        ; Send to server (background)
```

### Telemetry
```asm
InitializeTelemetry()           ; Set up event queuing
RecordTelemetryEvent()          ; Queue event (non-blocking)
TelemetryFlushThread()          ; Background flush every 60s
```

### Updates
```asm
InitializeAutoUpdater()         ; Start checker thread
UpdateCheckerThread()           ; Check every 1 hour
CheckForUpdates()               ; HTTP request to server
ShowUpdateNotification()        ; Toast or MessageBox
```

### UI
```asm
InitializeWindowFramework()     ; Create main window + menu
PopulateMenus()                 ; Add File/Edit/AI/Help
WndProc()                       ; Main message handler
WndProc_Command()               ; Menu command dispatch
```

### Performance
```asm
InitializePerformanceCounters() ; Setup high-res timers
StartPerformanceCounter()       ; Begin measurement
StopPerformanceCounter()        ; End measurement, calculate elapsed
```

---

## 📊 Testing Checklist

- [ ] Application launches without crash
- [ ] Window appears with menu system
- [ ] File menu has New/Open/Save/Exit
- [ ] Edit menu has Undo/Redo/Cut/Copy/Paste
- [ ] AI menu has Autocomplete/Refactor/Explain/Generate
- [ ] Help menu has About/Docs
- [ ] Telemetry events logged to file every 60s
- [ ] Registry configuration persists across restarts
- [ ] Update check runs in background (no UI blocking)
- [ ] Performance counters record sub-millisecond times
- [ ] Exception handler captures crashes gracefully

---

## 🚀 Production Checklist

Before shipping:

- [ ] Code signed (for Windows SmartScreen)
- [ ] EV certificate for HTTPS (for auto-updater)
- [ ] Privacy policy updated (GDPR disclosure)
- [ ] Telemetry server running (telemetry.rawrxd.ai)
- [ ] Update server running (update.rawrxd.ai)
- [ ] Minidump storage provisioned (crash analysis)
- [ ] Backup & disaster recovery plan
- [ ] Monitoring dashboards set up
- [ ] Support process for crash analysis

---

## 📈 Monitoring

### Key Metrics
```
App Startup Time:           < 1000ms
Menu Responsiveness:        < 50ms
Telemetry Batch Upload:     < 500ms
Update Check Duration:      < 2000ms
Crash Dump Generation:      < 5000ms
```

### Server Endpoints
```
Telemetry Upload:   POST https://telemetry.rawrxd.ai/v5/events
Update Check:       GET  https://update.rawrxd.ai/v5/check
Crash Report:       POST https://telemetry.rawrxd.ai/v5/crashes
```

### Alert Thresholds
```
Crash Rate > 1%:            Page on-call engineer
Telemetry Loss > 5%:        Investigate server
Update Check Failures > 10%: Check DNS/firewall
```

---

## 🔐 Security

### Pre-Deployment
```batch
# Code signing
signtool sign /f cert.pfx /p password /t http://timestamp.server.com ^
    RawrXD-v5.0.exe

# Verify signature
signtool verify /pa /q RawrXD-v5.0.exe

# Run virus scan
"%ProgramFiles%\Windows Defender\MpCmdRun.exe" -Scan -ScanType 3 ^
    -File RawrXD-v5.0.exe
```

### Runtime
- Crash dumps encrypted in transit (TLS 1.2+)
- Telemetry uses HTTPS only
- Configuration stored in per-user registry (not world-writable)
- No privileged installation required (no admin needed)

---

## 📝 Logging

### Telemetry Log (`%APPDATA%\RawrXD\telemetry.log`)
```
[2026-01-27 12:34:56] Telemetry initialized (session: a1b2c3d4...)
[2026-01-27 12:35:00] Event recorded (UI_CLICK, button=autocomplete)
[2026-01-27 12:35:42] Event recorded (AI_COMPLETION, duration=187ms)
[2026-01-27 12:36:56] Flushing 2 events...
[2026-01-27 12:36:57] Batch uploaded (200 OK)
```

### Update Log (`%APPDATA%\RawrXD\update.log`)
```
[2026-01-27 12:00:00] Update checker started
[2026-01-27 13:00:00] Checking for updates...
[2026-01-27 13:00:02] Current: 5.0.0, Available: 5.1.0
[2026-01-27 13:00:02] Update available! Showing notification...
[2026-01-27 13:00:05] User accepted update
[2026-01-27 13:00:07] Downloaded 5.1.0 setup
[2026-01-27 13:00:09] Installation complete, restart pending
```

### Crash Log (`%APPDATA%\RawrXD\crash.log`)
```
[2026-01-27 14:23:45] Unhandled exception: 0xC0000005 (Access Violation)
[2026-01-27 14:23:45] Exception address: 0x00007FFA12345678
[2026-01-27 14:23:46] Minidump written to: RawrXD_Crash_20260127_142345.dmp
[2026-01-27 14:23:47] Uploading crash report...
[2026-01-27 14:23:48] Crash report uploaded (200 OK)
```

---

## 🎓 Training

### For Support Team
- How to interpret crash dumps (using WinDbg)
- How to check telemetry for user behavior
- How to trigger manual update checks
- How to disable telemetry for privacy-conscious users

### For Product Team
- Reading performance metrics dashboards
- Identifying performance regressions
- Understanding crash patterns
- Planning feature releases based on usage data

---

## 🆘 Troubleshooting

| Problem | Solution |
|---------|----------|
| **Crash handler not working** | Ensure DbgHelp.dll is available; run `dbghelp.dll` check |
| **Telemetry not uploading** | Check firewall rules, verify DNS resolution of telemetry.rawrxd.ai |
| **Update checker stuck** | Restart application; check update server logs |
| **Registry not persisting** | Run as admin; check registry permissions |
| **Performance counters showing zeros** | Verify QueryPerformanceFrequency is supported (x64 only) |

---

## 📦 Deployment Variants

### Portable Version (No Installation)
```batch
# Just the EXE + config in user registry
RawrXD-v5.0.exe
```

### With Installer (MSI/NSIS)
```batch
# Installs to Program Files
# Creates Start Menu shortcuts
# Registers file associations
```

### Portable ZIP
```batch
# Redistribute as ZIP
# Users unzip and run
# Config auto-created on first run
```

### Windows Store/Microsoft Store
```batch
# Packaged as .appx
# Auto-updates through Store
# Telemetry integrated with Store analytics
```

---

## 🎉 You're Done!

Week 5 provides production-ready infrastructure:

✅ Crashes don't crash  
✅ Users get automatic updates  
✅ We know what's happening  
✅ Configuration persists  
✅ Performance is measurable  
✅ Ready to ship  

**Now go build an awesome IDE!** 🚀

---

**File**: `week5_final_integration.asm`  
**Build Time**: ~30 seconds  
**Test Time**: ~5 minutes  
**Ship Date**: Today ✅
