# 🔧 VISUAL STUDIO 2022 FIX & LAUNCH GUIDE

**VS 2022 Location**: D:\Microsoft Visual Studio 2022\Enterprise  
**Goal**: Repair and launch BotBuilder project  
**Time**: 5-15 minutes depending on repair level

---

## ⚡ QUICK FIX (FIRST TRY THIS - 2 MINUTES)

### Option 1: Direct Launch with Cache Clear
```powershell
# Open PowerShell and run:
cd c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master
.\LAUNCH-BOTBUILDER.ps1
```

**What this does**:
- ✅ Clears Visual Studio cache
- ✅ Launches VS 2022 from D: drive
- ✅ Opens BotBuilder.sln directly
- ✅ Ready to press F5 and run

**Expected result**: VS opens with BotBuilder project loaded, no errors

---

## 🔄 IF QUICK FIX DOESN'T WORK - RUN REPAIR

### Option 2: Full Repair (5-10 minutes)
```powershell
# Open PowerShell as Administrator and run:
cd c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master
.\REPAIR-VS2022.ps1
```

**What this does**:
- ✅ Clears Visual Studio cache completely
- ✅ Clears MEF cache
- ✅ Clears temporary files
- ✅ Offers to run VS repair
- ✅ Launches VS 2022

**When prompted**:
- Type `y` to run repair (recommended first time)
- Type `n` to skip repair and just launch

**Expected result**: VS repairs itself and launches cleanly

---

## 🛠️ MANUAL STEPS IF NEEDED

### Step 1: Close Everything
```powershell
# Kill any running VS instances
Get-Process devenv -ErrorAction SilentlyContinue | Stop-Process -Force
```

### Step 2: Clear Cache
```powershell
# Clear component model cache
Remove-Item "$env:LOCALAPPDATA\Microsoft\VisualStudio\17.0_*\ComponentModelCache\*" -Force -ErrorAction SilentlyContinue

# Clear MEF cache
Remove-Item "$env:TEMP\VisualStudioComponentCache" -Recurse -Force -ErrorAction SilentlyContinue
New-Item "$env:TEMP\VisualStudioComponentCache" -ItemType Directory -Force

# Clear ReSharper cache (if installed)
Remove-Item "$env:LOCALAPPDATA\JetBrains\ReSharperPlatformCache" -Recurse -Force -ErrorAction SilentlyContinue
```

### Step 3: Run Repair from Command Line
```cmd
# Open Command Prompt as Administrator and run:
"D:\Microsoft Visual Studio 2022\Enterprise\Common7\IDE\devenv.exe" /repair
```

**Wait for it to complete** (5-10 minutes)

### Step 4: Launch
```cmd
# Open the solution
"D:\Microsoft Visual Studio 2022\Enterprise\Common7\IDE\devenv.exe" "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder\BotBuilder.sln"
```

---

## 🎯 TROUBLESHOOTING

### If VS Crashes on Startup
```powershell
# 1. Stop VS
Get-Process devenv -ErrorAction SilentlyContinue | Stop-Process -Force

# 2. Clear all caches
Remove-Item "$env:LOCALAPPDATA\Microsoft\VisualStudio" -Recurse -Force
Remove-Item "$env:APPDATA\Microsoft\VisualStudio" -Recurse -Force

# 3. Launch again
"D:\Microsoft Visual Studio 2022\Enterprise\Common7\IDE\devenv.exe"
```

### If MSBuild Fails
```powershell
# Clear project build cache
cd c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder
Remove-Item "bin" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "obj" -Recurse -Force -ErrorAction SilentlyContinue

# In VS: Rebuild Solution (Ctrl+Alt+F7)
```

### If IntelliSense is Broken
```powershell
# 1. Close VS
Get-Process devenv | Stop-Process -Force

# 2. Clear IntelliSense cache
Remove-Item "$env:LOCALAPPDATA\Microsoft\VisualStudio\17.0_*\ComponentModelCache" -Recurse -Force

# 3. Reopen VS
"D:\Microsoft Visual Studio 2022\Enterprise\Common7\IDE\devenv.exe"

# 4. Edit → IntelliSense → Rescan Solution
```

### If Slow Performance
```powershell
# Disable unnecessary extensions:
# In VS: Tools → Extensions and Updates → Disable Extensions → Search for:
# - R# (if you don't use it)
# - Web essentials
# - Any extensions you don't need
```

---

## ✅ VERIFY VS IS WORKING

Once VS launches, verify it's working:

```
✓ Visual Studio window opens
✓ BotBuilder.sln loads (if launched with script)
✓ Solution Explorer shows project structure
✓ No red X marks on files
✓ No errors in Error List
✓ IntelliSense works (type in code, autocomplete shows up)
✓ Can build project (Ctrl+Shift+B)
✓ Can run project (F5)
```

---

## 🚀 YOUR NEXT STEPS

### 1. Run Quick Fix (1st attempt)
```powershell
.\LAUNCH-BOTBUILDER.ps1
```

### 2. If that works → Skip to Step 4 ✅

### 3. If that fails → Run Repair
```powershell
.\REPAIR-VS2022.ps1
```

### 4. Test BotBuilder
```
Press F5 in VS to run the app
Click all 4 tabs to verify it works
Close the app
```

### 5. Commit
```powershell
git add Projects/BotBuilder/
git commit -m "Phase 3 Task 2: BotBuilder GUI verified working"
git push origin phase3-botbuilder-gui
```

---

## 📁 YOUR LAUNCHER SCRIPTS

Created for you in workspace root:
- `LAUNCH-BOTBUILDER.ps1` - Quick launch (recommended)
- `REPAIR-VS2022.ps1` - Full repair + launch
- `REPAIR-VS2022.bat` - Batch version if PowerShell fails

---

## 💡 QUICK REFERENCE

| Issue | Solution |
|-------|----------|
| VS won't launch | Run `REPAIR-VS2022.ps1` |
| Build fails | Clear `bin` and `obj` folders, rebuild |
| IntelliSense broken | Clear component cache, restart VS |
| Slow performance | Disable extensions, see troubleshooting |
| Can't open solution | Run `LAUNCH-BOTBUILDER.ps1` directly |

---

## 🎯 SUCCESS CHECKLIST

Before you proceed with BotBuilder testing:

```
✓ Visual Studio 2022 launches
✓ BotBuilder.sln opens without errors
✓ Solution Explorer shows all projects
✓ No error list items
✓ Can build project (Ctrl+Shift+B)
✓ Can run app (F5)
✓ Application window appears
✓ All 4 tabs are visible and clickable
✓ Can click buttons without crashes
✓ Exit button closes cleanly
```

---

## 🔥 DO THIS NOW

```powershell
# 1. Open PowerShell
# 2. Navigate to workspace
cd c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master

# 3. Run quick fix
.\LAUNCH-BOTBUILDER.ps1

# 4. Wait for VS to open
# 5. Test BotBuilder
# 6. Close VS
# 7. Come back here for next steps
```

**That's it!** VS 2022 will be fixed and ready to use!

---

*Visual Studio 2022 Fix Guide*  
*Location: D:\Microsoft Visual Studio 2022\Enterprise*  
*November 21, 2025*
