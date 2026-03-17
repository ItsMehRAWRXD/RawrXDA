# Running RawrXD with Administrator Privileges

## Quick Launch Options

### Option 1: Run As Admin (Recommended for Full Features)
```powershell
.\Run-As-Admin.ps1
```
- Automatically elevates to Administrator
- Enables Event Log features
- Full system-level access
- UAC prompt will appear (click "Yes")

### Option 2: Normal Launch (Limited Privileges)
```powershell
.\RawrXD.ps1
```
- Runs without elevation
- Event Log features disabled (silent fallback)
- Everything else works normally
- No UAC prompt

## What Requires Admin?

**Enabled with Admin:**
- Windows Event Log integration (`System.Diagnostics.EventLog`)
- Creating custom Event Log sources
- Full error logging to Windows Event Viewer

**Works Without Admin:**
- All file operations (logs go to `Powershield\logs\`)
- Ollama AI integration
- File explorer, editor, browser
- Git operations
- Extension marketplace
- Agent tasks
- Everything else

## Status Check

When RawrXD launches, check the startup log for:
```
[INFO] Running with Administrator privileges        ← Admin mode
[INFO] Running without Administrator privileges     ← Normal mode
```

Log file: `C:\Users\HiH8e\OneDrive\Desktop\Powershield\logs\startup_YYYY-MM-DD.log`

## Troubleshooting

**UAC prompt doesn't appear:**
- Right-click `Run-As-Admin.ps1` → "Run with PowerShell"
- Or run from an already-elevated PowerShell

**Don't want UAC prompts:**
- Just use `.\RawrXD.ps1` directly
- Event Log warnings will be silently suppressed

**Check if running as admin:**
Open RawrXD and look for the startup log message, or check:
```powershell
([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
```
