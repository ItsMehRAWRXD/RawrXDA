# Editor Diagnostics - Quick Reference Card

## User-Facing Features

### Accessing Diagnostics
```
Menu: Tools → Editor
```

### Menu Options
| Option | Function | Result |
|--------|----------|--------|
| 📊 Show Diagnostics | View current editor health | Shows dialog with health score & issues |
| 🔨 Repair Colors | Quick color fix | BackColor/ForeColor/SelectionColor restored |
| 🚀 Full Repair | Complete restoration | All properties reset, event handlers injected |
| ⚙️ Toggle Auto-Repair | Enable/disable auto-fixing | Turns continuous repair on/off |

---

## Developer-Facing Functions

### Diagnostic Functions

```powershell
# Get health report
$health = Test-EditorHealth
# Returns: @{ Status, HealthScore (0-100), Issues (array) }

# Get full diagnostics
$diag = Get-EditorDiagnostics
# Returns: @{ Status, Score, Issues, RepairAttempts, MonitoringActive, etc. }
```

### Repair Functions

```powershell
# Quick color fix
Repair-EditorColors
# ✓ Restores colors only (~50ms)

# Fix event handlers
Repair-EditorEventHandlers
# ✓ Re-adds focus handlers (~50ms)

# Full repair (use this for most issues)
Repair-EditorState
# ✓ Fixes everything (~200ms)
```

### Monitoring Functions

```powershell
# Start monitoring (auto-repairs every 2 seconds)
Start-EditorMonitoring -AutoRepair

# Stop monitoring
Stop-EditorMonitoring

# Show diagnostics UI
Show-EditorDiagnosticsDialog
```

---

## How It Works

### Initialization
```
1. RawrXD starts
2. Editor (RichTextBox) created
3. Initialize-EditorDiagnosticsModule called
4. Module loads editor-diagnostics.ps1
5. Initial health check performed
6. Timer starts (checks every 2 seconds)
```

### Runtime Loop
```
Every 2 seconds:
  1. Get editor health score
  2. If score < 80 and auto-repair enabled:
     → Call Repair-EditorState
     → Log repair attempt
     → Force UI refresh
  3. Update diagnostics record
```

### On Problem Detection
```
If editor becomes greyed out:
  1. Next timer tick (within 2 seconds)
  2. Health check detects low score
  3. Auto-repair triggers
  4. Colors restored
  5. Problem fixed silently
  6. User continues working
```

---

## Health Score Formula

```
Total Score = 100 points

Deductions:
- BackColor wrong:      -15 points
- ForeColor wrong:      -15 points
- SelectionColor wrong: -15 points
- ReadOnly = true:      -25 points
- Enabled = false:      -25 points
- BorderStyle wrong:    -5 points
- No event handlers:    -10 points

Score < 80 = AUTO-REPAIR TRIGGERS
```

---

## Common Scenarios

### Scenario: Text is Greyed Out

**What Happens:**
```
1. Editor colors corrupted
2. Monitor detects health score dropped
3. Auto-repair triggers within 2 seconds
4. Colors restored
5. Text appears again
```

**User Experience:**
- May see greyed text briefly
- Text becomes visible again automatically
- No restart needed

### Scenario: Editor Won't Accept Input

**Causes:**
- ReadOnly property set to true
- Enabled property set to false
- Event handlers missing

**Fix:**
```
1. Click Tools → Editor → Full Repair
2. Editor state fully restored
3. Typing works again
```

### Scenario: Focus Events Don't Work

**What Happens:**
```
1. Clicking editor doesn't select text
2. Tab/focus navigation broken
3. Monitor detects missing handlers
4. Repair-EditorEventHandlers called
5. Event handlers re-added
6. Focus works again
```

---

## Performance Specifications

| Operation | Time | Impact |
|-----------|------|--------|
| Module load | 50ms | One-time at startup |
| Initial check | 30ms | One-time at startup |
| Initial repair | 100ms | If needed at startup |
| Health check | 15ms | Every 2 seconds |
| Color repair | 50ms | On demand or auto |
| Full repair | 200ms | On demand or auto |
| **Total CPU** | < 0.1% | When idle |

---

## Configuration

### Change Monitor Interval

Edit `editor-diagnostics.ps1`, search for:
```powershell
$script:editorMonitorTimer.Interval = 2000  # milliseconds
```

Change to desired value (lower = faster response, more CPU)

### Disable Auto-Repair

At runtime:
```powershell
$script:EditorDiagnostics.AutoRepairEnabled = $false
```

Or use menu: Tools → Editor → Toggle Auto-Repair

### Manual Repair All Issues

```powershell
Repair-EditorState -Editor $script:editor
```

---

## Debugging Commands

### Check Current Health
```powershell
Test-EditorHealth | ForEach-Object { Write-Host "Score: $($_.HealthScore), Status: $($_.Status)" }
```

### View All Issues
```powershell
(Test-EditorHealth).Issues | ForEach-Object { Write-Host $_ }
```

### Check Repair History
```powershell
$script:EditorDiagnostics | Format-List
```

### Force Immediate Repair
```powershell
Repair-EditorState
```

### Disable Monitoring (advanced)
```powershell
Stop-EditorMonitoring
```

---

## Error Handling

### All Repair Functions Return Boolean

```powershell
if (Repair-EditorColors) {
    Write-Host "✅ Repair successful"
} else {
    Write-Host "❌ Repair failed"
}
```

### All Operations Log to latest.log

Search log for:
- "Editor Diagnostics" - general info
- "ERROR" - if something fails
- "🔧" - auto-repair in progress

---

## Files & Locations

```
Powershield/
├── RawrXD.ps1 (main IDE)
├── editor-diagnostics.ps1 (NEW - diagnostics module)
└── EDITOR-DIAGNOSTICS-GUIDE.md (documentation)
```

---

## Testing

### Verify Module Loads
```powershell
cd Powershield
pwsh -NoProfile -Command ". .\RawrXD.ps1"
# Check for "✅ Editor Diagnostics module loaded" in output
```

### Verify Tests Pass
```powershell
pwsh -NoProfile -File .\test-rawrxd-fixes.ps1
# Should show: Passed: 8, Failed: 0
```

### Manual Module Test
```powershell
. .\editor-diagnostics.ps1
$health = Test-EditorHealth
Write-Host "Health: $($health.Status), Score: $($health.HealthScore)"
```

---

## Support

### Issue: Auto-repair triggers too often
→ Use menu to toggle off, check logs for root cause

### Issue: Colors still greyed after repair
→ Try Full Repair (🚀), then restart IDE

### Issue: Menu doesn't appear
→ Check latest.log for "Editor Diagnostics" ERROR messages

### Issue: Performance concerns
→ Increase monitor interval to 5000ms or disable auto-repair

---

## Summary

✅ **What It Does**
- Continuously monitors text editor health
- Automatically fixes common problems
- Provides diagnostic information
- Offers manual repair options

✅ **When It Activates**
- On app startup (initial check)
- Every 2 seconds during runtime
- When user clicks repair buttons

✅ **What Gets Fixed**
- Color properties (BackColor, ForeColor, SelectionColor)
- Event handlers (GotFocus, LostFocus, KeyPress)
- State properties (ReadOnly, Enabled)
- Display refresh issues

✅ **How to Use**
- Let it run automatically (default)
- Use Tools → Editor menu for manual options
- Check diagnostics if issues occur

---

**System Status**: 🟢 ACTIVE & SELF-HEALING
