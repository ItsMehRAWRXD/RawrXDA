# RawrXD IDE: OS→GUI Features Quick Start

## Memory Monitor (Status Bar)

**Location:** Bottom-right corner of the window

**Display Format:**
```
WS: XXX MB | Private: XXX MB | Sys Free: XXX MB
```

**Features:**
- Real-time updates every 1 second
- Green text: Normal operation (< 5 GB working set)
- Red bold text: Warning zone (> 5 GB working set)
- Auto-cleanup: Automatic memory scrub at > 10 GB

**What It Shows:**
- **WS (Working Set):** Active memory being used by the IDE
- **Private:** Memory not shared with other processes
- **Sys Free:** Available system RAM

---

## Diagnostics Panel

**Access:** View Menu → Diagnostics (or Ctrl+Alt+D if bound)

**Panel Features:**
1. **Summary Line:** One-line overview of all metrics
2. **Run Memory Check Button:** Click to capture current system state
3. **Metrics Tree:** Detailed breakdown of:
   - Working Set Memory (MB)
   - Private Usage (MB)
   - Physical RAM Available (MB)
   - Physical RAM Total (MB)
   - Virtual Commit Used (MB)
   - Virtual Commit Limit (MB)

**Checkpoint Persistence:**
Every time you click "Run Memory Check", a JSON file is written to:
```
./checkpoints/diagnostics_YYYYMMDD_HHMMSS.json
```

This contains all metrics for auditing and troubleshooting.

---

## Manual Memory Cleanup

**Hotkey:** Ctrl+Alt+S

**What Happens:**
1. Tells Windows to trim the IDE's unused working set memory
2. Status bar message: "Memory scrubbed - working set trimmed"
3. Console log entry for audit trail

**When to Use:**
- After heavy file operations
- Before running large inference tasks
- If memory usage seems high

---

## Understanding the Output

### Status Bar Colors

```
Green (#4ec9b0):
WS: 150 MB | Private: 95 MB | Sys Free: 42000 MB
→ Normal operation

Red + Bold:
WS: 5500 MB | Private: 4200 MB | Sys Free: 25000 MB
→ Warning: High memory usage
→ Auto-scrub will trigger at 10,000 MB
```

### Diagnostics Tree Example

```
Metric                      Value
─────────────────────────────────
Working Set (MB)            2450
Private Usage (MB)          1820
Physical Free (MB)          38000
Physical Total (MB)         64000
Commit Used (MB)            28000
Commit Limit (MB)           82000
```

**Interpretation:**
- Working Set: IDE is using ~2.5 GB of RAM
- Private: Not shared with other processes
- System has 38 GB free out of 64 GB total RAM
- Virtual memory commit is at ~28 GB of 82 GB limit

---

## Checkpoint Files

**Location:** `./checkpoints/` subdirectory (auto-created)

**File Format:** `diagnostics_YYYYMMDD_HHMMSS.json`

**Example Content:**
```json
{
  "type": "diagnostics",
  "timestamp": 1735358400,
  "working_set_mb": 2450,
  "private_usage_mb": 1820,
  "physical_free_mb": 38000,
  "physical_total_mb": 64000,
  "commit_used_mb": 28000,
  "commit_limit_mb": 82000
}
```

**Use Cases:**
- Compare memory usage over time
- Debug IDE performance regressions
- Audit system resource usage patterns
- Create performance reports

---

## Build & Launch

**Build Command (VS Code):**
```
Ctrl+Shift+B → Select "Build RawrXD IDE"
```

Or terminal:
```powershell
cmake --build "build_masm" --config Release --target brutal_gzip
cmake --build "build_masm" --config Release --target RawrXD-QtShell
```

**Launch IDE:**
```
.\build_masm\bin\Release\RawrXD-QtShell.exe
```

---

## Troubleshooting

### Memory Display Shows Weird Values
- The status bar automatically clamps unrealistic values
- If you see 0 MB, the system call may have failed
- Check console output (bottom panel) for error messages

### Diagnostics Panel Won't Open
- Ensure View menu includes "Diagnostics" checkbox
- If missing, rebuild the project
- Check that setupDiagnosticsPanel() is called in MainWindow

### Checkpoints Directory Not Created
- First "Run Memory Check" click auto-creates `./checkpoints/`
- If it fails, check write permissions in the IDE working directory

### Auto-Scrub Not Triggering
- Auto-scrub only activates when WS > 10,000 MB
- If your system has < 20 GB RAM, this may never trigger
- Use Ctrl+Alt+S for manual cleanup instead

---

## Integration with Inference

When running AI models:

1. **Monitor panel** shows real-time memory impact
2. **Status bar** updates every second during inference
3. **Run Diagnostics** before and after model loads to compare
4. **Checkpoints** create audit trail of resource usage
5. **Auto-scrub** prevents runaway memory accumulation

---

## Performance Notes

- Status bar monitor is **1 Hz refresh** (minimal CPU impact)
- Diagnostics panel is **on-demand** (only runs when you click)
- Checkpoint writing is **asynchronous** (non-blocking)
- Memory scrub is **instant** (< 1ms, no restart needed)

---

## API Summary for Developers

If extending the IDE:

### Get Current Process Memory
```cpp
PROCESS_MEMORY_COUNTERS_EX pmcEx;
GetProcessMemoryInfo(GetCurrentProcess(), 
                     (PPROCESS_MEMORY_COUNTERS)&pmcEx, sizeof(pmcEx));
double wsMB = pmcEx.WorkingSetSize / (1024.0 * 1024.0);
```

### Get System Memory
```cpp
MEMORYSTATUSEX statex; statex.dwLength = sizeof(statex);
GlobalMemoryStatusEx(&statex);
double freeMB = statex.ullAvailPhys / (1024.0 * 1024.0);
double totalMB = statex.ullTotalPhys / (1024.0 * 1024.0);
```

### Trigger Memory Scrub
```cpp
SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
```

All three are wrapped in the MainWindow memory monitor and can be accessed as signals/slots if needed.

---

**Version:** 1.0  
**Last Updated:** December 27, 2025  
**Status:** Production Ready
