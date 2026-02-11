# IDE Autonomous Healer - Quick Start Guide

## TL;DR

The IDE now has a fully autonomous self-healing diagnostic system. Instead of manually debugging with DebugView:

**Before:** Run IDE → Press Ctrl+Shift+D → Nothing happens → Open DebugView → Trace messages manually  
**Now:** Run launcher → Full automatic diagnostic → Auto-recover failures → JSON report

## Run It Now

```bash
# 1. Navigate to build output
cd d:\lazy init ide\build\bin\Release

# 2. Run the autonomous diagnostic
IDEAutoHealerLauncher.exe --full-diagnostic

# 3. Check the report
type diagnostic_report.json
```

Expected output:
```
✓ [SUCCESS] Autonomous diagnostic thread launched
✓ [STAGE 1] Starting Full Diagnostic Sequence
...
✓ Diagnostic completed successfully
✓ All stages completed without critical failures
```

## What It Does

### Automatically:

1. ✅ Launches the IDE process
2. ✅ Finds the main window
3. ✅ Opens a test file
4. ✅ Sends Ctrl+Shift+D hotkey
5. ✅ Monitors for digest output
6. ✅ Detects failures
7. ✅ Applies healing strategies
8. ✅ Generates detailed report

### Monitors 12 Stages:

```
0. IDE Launch           ✓
1. Window Created       ✓
2. Menu Initialized     ✓
3. Editor Ready         ✓
4. File Opened          ✓
5. Hotkey Sent          ✓
6. Message Received     ✓
7. Thread Spawned       ✓
8. Engine Running       ✓
9. Engine Complete      ✓
10. Output Verified     ✓
11. Success             ✓
```

### Auto-Recovers From:

- Hotkey not being sent → Resends hotkey
- Message not routing → Reposts message
- Window focus lost → Refocuses window
- IDE process dies → Restarts IDE
- Multiple failures → Applies healing cascade

## Key Files

| File | Purpose |
|------|---------|
| `IDEAutoHealerLauncher.exe` | Main autonomous diagnostic tool |
| `IDEDiagnosticAutoHealer.lib` | Core healing engine (linked into IDE) |
| `diagnostic_report.json` | Detailed results with beacon checkpoints |
| `D:\AUTONOMOUS_IDE_HEALER_COMPLETE.md` | Full documentation |

## Beacon Checkpoints

Each stage emits a "beacon" - a timestamped checkpoint saved to disk:

```
[Beacon] Stage=0, Result=0x00000000    IDE Launch successful
[Beacon] Stage=1, Result=0x00000000    Window found
[Beacon] Stage=4, Result=0x00000000    File opened
[Beacon] Stage=5, Result=0x00000000    Hotkey sent
...
[Beacon] Stage=11, Result=0x00000000   SUCCESS
```

If a beacon shows non-zero result code, the healer automatically applies recovery.

## Monitor in Real-Time

Download **DebugView** from Sysinternals, then:

```
1. Run DebugView
2. Menu: Capture → Capture Global Win32
3. Filter for [AutoHealer] or [Beacon]
4. Run: IDEAutoHealerLauncher.exe --full-diagnostic
5. Watch real-time progress in DebugView
```

## Understanding the Report

```json
{
  "timestamp": 45231847,
  "totalBeacons": 12,
  "healingAttempts": 0,
  "beacons": [...],
  "appliedHealings": []
}
```

- **totalBeacons: 12** = All stages completed successfully
- **healingAttempts: 0** = No failures, no healing needed
- **appliedHealings: []** = No recovery strategies applied

## If Something Fails

### Check diagnostics file:

```bash
type diagnostic_report.json | findstr "healing"
```

### View real-time debug output:

```powershell
# Run DebugView
"C:\path\to\DebugView.exe"

# In filter box, type: [AutoHealer]
# Then run: IDEAutoHealerLauncher.exe --full-diagnostic
```

### Verify engine works in isolation:

```bash
# If healer reports engine failure, test directly:
cd d:\lazy init ide\build\bin\Release
digestion_test_harness.exe

# Should output progress: 0% → 33% → 66% → 100%
# And create valid JSON digest file
```

## Integration

### Use in Your IDE

If you build with the IDE target:

```cpp
// In your IDE code
#include "IDEDiagnosticAutoHealer.h"

// On startup or error recovery
IDEDiagnosticAutoHealer::Instance().StartFullDiagnostic();
```

### Integrate Into CI/CD

```powershell
# PowerShell build script
& "build\bin\Release\IDEAutoHealerLauncher.exe" --full-diagnostic

# Check exit code
if ($LASTEXITCODE -eq 0) {
    Write-Host "IDE diagnostics PASSED"
} else {
    Write-Host "IDE diagnostics FAILED"
    exit 1
}
```

## The Complete Pipeline

```
IDEAutoHealerLauncher.exe (Console app)
    ↓
IDEDiagnosticAutoHealer (Core engine)
    ├─ Launches RawrXD-Win32IDE.exe
    ├─ Emits beacon at each stage
    ├─ Detects timeout/failure
    ├─ Applies healing strategy
    └─ Generates JSON report
         ↓
    RawrXD-Win32IDE.exe (Instrumented target)
         ├─ [OutputDebugString] at hotkey reception
         ├─ [OutputDebugString] at message dispatch
         ├─ [OutputDebugString] at thread creation
         └─ [OutputDebugString] at engine completion
```

## Success Criteria

### Fully Working System Shows:

✅ All 12 beacons reach final stage (SUCCESS)  
✅ `diagnostic_report.json` has `"totalBeacons": 12`  
✅ `"healingAttempts": 0` (no recovery needed)  
✅ Console shows: "✓ All stages completed without critical failures"  
✅ Digest file (`.digest`) created successfully  

### If healing was needed (still good):

✅ Report shows `"healingAttempts": 1-3`  
✅ `"appliedHealings"` array has strategy enums  
✅ Final beacon still reaches SUCCESS stage  
✅ Example: `"appliedHealings": [2, 6]` means MESSAGE_REPOST then PROCESS_RESTART were applied

### Complete Failure (needs investigation):

❌ Stops before stage 1 (IDE won't launch)  
❌ Stops at stage 2-3 (window creation issue)  
❌ Healing attempts maxed out with no recovery  
→ Check `AUTONOMOUS_IDE_HEALER_COMPLETE.md` troubleshooting section

## Next Steps

1. **Run the diagnostic now:**
   ```bash
   d:\lazy init ide\build\bin\Release\IDEAutoHealerLauncher.exe
   ```

2. **Verify the report:**
   ```bash
   type diagnostic_report.json
   ```

3. **Read full docs:**
   ```bash
   type D:\AUTONOMOUS_IDE_HEALER_COMPLETE.md
   ```

4. **Monitor in real-time:**
   - Download DebugView from Sysinternals
   - Filter for `[AutoHealer]`
   - Run launcher with `--verbose`

---

## Summary

| Aspect | Before | After |
|--------|--------|-------|
| Testing | Manual DebugView | Fully autonomous |
| Diagnostics | View raw messages | Structured JSON report |
| Failure recovery | Manual restart | Auto-healing with retry |
| Checkpoint system | None | Persistent beaconing |
| User interaction | High | None (fire and forget) |

The IDE now has **production-grade autonomous diagnostics** that detect and heal failures without any user intervention. 

---

**Build Status:** ✅ COMPLETE  
**Executable:** `build\bin\Release\IDEAutoHealerLauncher.exe`  
**Library:** `build\lib\Release\IDEDiagnosticAutoHealer.lib`  
**Documentation:** `D:\AUTONOMOUS_IDE_HEALER_COMPLETE.md`

