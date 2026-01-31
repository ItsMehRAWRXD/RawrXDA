# VS Code Recovery Guide

## Current Situation
VS Code was not found on E drive after moving from D drive. Here's how to recover it.

## What We Found
- ✅ **Cursor** is located at: `E:\Everything\cursor\Cursor.exe`
- ❌ **VS Code** was not found on E drive
- ❌ **VS Code** was not found on D drive either

## Possible Scenarios

### Scenario 1: VS Code Wasn't Moved
VS Code might still be in its original installation location (usually `%LOCALAPPDATA%\Programs\Microsoft VS Code`).

**Check:**
```powershell
Get-ChildItem "$env:LOCALAPPDATA\Programs" -Filter "*code*" -Recurse
```

### Scenario 2: Portable Installation
If you had a portable version, it might be in a different location.

**Search for:**
- `VSCode-win32-x64` folders
- `code-portable.exe`
- Any folder containing VS Code files

### Scenario 3: Need to Reinstall
If VS Code was lost during the move, you'll need to reinstall it.

## Solutions

### Option 1: Reinstall VS Code (Recommended)
1. Download VS Code from: https://code.visualstudio.com/
2. Choose **User Installer** (installs to AppData, doesn't require admin)
3. Or choose **System Installer** if you want it in Program Files
4. During installation, you can choose a custom location on E drive

### Option 2: Install Portable Version on E Drive
1. Download VS Code Portable: https://code.visualstudio.com/#alt-downloads
2. Extract to: `E:\Everything\~dev\VSCode` or `E:\VSCode`
3. Run `code.exe` from that location

### Option 3: Check if It's Still Installed
Run this to check all possible locations:
```powershell
# Check AppData
Get-ChildItem "$env:LOCALAPPDATA\Programs" -Filter "*code*" -Recurse -ErrorAction SilentlyContinue

# Check Program Files
Get-ChildItem "$env:ProgramFiles" -Filter "*code*" -Recurse -ErrorAction SilentlyContinue

# Check for shortcuts
Get-ChildItem "$env:APPDATA\Microsoft\Windows\Start Menu\Programs" -Recurse -Filter "*code*" -ErrorAction SilentlyContinue
```

## After Finding/Reinstalling VS Code

### Add to PATH
If VS Code is at `E:\Everything\~dev\VSCode\bin`, add it to PATH:

```powershell
$vscodeBin = "E:\Everything\~dev\VSCode\bin"
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
[Environment]::SetEnvironmentVariable("Path", "$currentPath;$vscodeBin", "User")
```

### Create Desktop Shortcut
```powershell
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$env:USERPROFILE\Desktop\VS Code.lnk")
$Shortcut.TargetPath = "E:\Everything\~dev\VSCode\Code.exe"  # Update path
$Shortcut.Save()
```

## Quick Commands

### Search for VS Code
```powershell
# Search E drive
Get-ChildItem E:\ -Recurse -Filter "code.exe" -ErrorAction SilentlyContinue -Depth 6

# Search D drive
Get-ChildItem D:\ -Recurse -Filter "code.exe" -ErrorAction SilentlyContinue -Depth 4

# Search all drives
Get-PSDrive -PSProvider FileSystem | ForEach-Object {
    Get-ChildItem $_.Root -Recurse -Filter "code.exe" -ErrorAction SilentlyContinue -Depth 3
}
```

### Check if VS Code is in PATH
```powershell
Get-Command code -ErrorAction SilentlyContinue
```

## Next Steps
1. Run the search commands above
2. If not found, reinstall VS Code to E drive
3. Update PATH if needed
4. Verify installation works

## Need Help?
If you remember approximately where you moved it, or if you have any backup locations, let me know and I can search more specifically!

