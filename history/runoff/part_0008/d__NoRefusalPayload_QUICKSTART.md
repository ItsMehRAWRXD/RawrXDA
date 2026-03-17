# Quick Start Guide - No-Refusal Payload Engine

## 🚀 Get Up and Running in 2 Minutes

### Step 1: Verify Project Setup

```powershell
# Navigate to project
cd D:\NoRefusalPayload

# List project files
Get-ChildItem -Recurse
```

### Step 2: Open in VS Code

```powershell
code D:\NoRefusalPayload
```

Or manually:
1. Launch **Visual Studio Code**
2. **File** → **Open Folder**
3. Select `D:\NoRefusalPayload`
4. Wait for CMake auto-configuration

### Step 3: Build the Project

**Option A: Keyboard Shortcut (Fastest)**
```
Press: Ctrl+Shift+B
```

**Option B: Command Palette**
```
Ctrl+Shift+P → Type "CMake: Build" → Press Enter
```

**Option C: Terminal**
```powershell
cd D:\NoRefusalPayload
.\build.ps1 -Configuration Debug
```

### Step 4: Run the Executable

After successful build, run from VS Code terminal:

```powershell
D:\NoRefusalPayload\build\Debug\NoRefusalEngine.exe
```

Or use the run task:
```
Ctrl+Shift+P → Type "Run Task: run-executable" → Press Enter
```

## 📊 What You Should See

### Build Output
```
[+] Build completed successfully!
Output: D:\NoRefusalPayload\build\Debug\NoRefusalEngine.exe
```

### Execution Output
```
========================================
--- No-Refusal IDE Environment Init ---
========================================

[*] Initializing hardening mechanisms...
[+] Console control handler registered.
[+] Process marked as critical.
[*] Protecting memory regions...
[+] ASM payload memory protected (PAGE_EXECUTE_READ).
[*] Launching payload core...
[+] Transitioning execution to MASM No-Refusal Core...
[+] Entering protected execution loop...
```

The process will then run indefinitely in the protection loop.

## 🔍 Debug the Code

### Set a Breakpoint
1. Open `src/main.cpp`
2. Click in the gutter on line 23 (before `supervisor.LaunchCore()`)
3. A red dot appears

### Start Debugging
```
Press: F5
```

Or:
```
Ctrl+Shift+P → Type "Debug" → Select "C/C++: (msvc) Launch with Console"
```

### Step Through Code
- **F10**: Step over
- **F11**: Step into
- **Shift+F11**: Step out
- **F5**: Continue execution

## 📁 Key Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Build configuration |
| `src/main.cpp` | C++ entry point |
| `src/PayloadSupervisor.cpp` | Hardening implementation |
| `asm/norefusal_core.asm` | MASM x64 core |
| `README.md` | Full documentation |

## ⚙️ Build Configurations

### Debug Build (Recommended for Development)
```powershell
.\build.ps1 -Configuration Debug
```
- Includes debug symbols
- Slower execution
- Better for debugging

### Release Build (Optimized)
```powershell
.\build.ps1 -Configuration Release
```
- Optimized code
- Faster execution
- Harder to debug

### Clean Rebuild
```powershell
.\build.ps1 -Clean -Configuration Debug
```

## 🛠️ Common Tasks

### View CMake Configure Log
```powershell
cat D:\NoRefusalPayload\build\CMakeFiles\CMakeOutput.log
```

### View Build Errors
```powershell
# Check the most recent error
Get-Content D:\NoRefusalPayload\build\CMakeFiles\CMakeError.log | tail -50
```

### Rebuild from Scratch
```powershell
rm -Recurse -Force D:\NoRefusalPayload\build
.\build.ps1 -Configuration Debug
```

## 🚨 Troubleshooting

### Problem: "CMake not found"
**Solution**: Ensure CMake is installed
```powershell
choco install cmake -y
# OR download from https://cmake.org/
```

### Problem: "ml64.exe not found"
**Solution**: Install MSVC tools
```
Visual Studio Installer → Modify → "Desktop development with C++"
```

### Problem: Build fails with assembly errors
**Solution**: Check CMakeLists.txt has ASM_MASM enabled
```cmake
enable_language(ASM_MASM)
```

### Problem: Executable won't start
**Solution**: Run from elevated command prompt
```powershell
# Right-click PowerShell → "Run as administrator"
D:\NoRefusalPayload\build\Debug\NoRefusalEngine.exe
```

## 📚 Learning Path

1. **Read**: `README.md` - Understand architecture
2. **Study**: `asm/norefusal_core.asm` - Learn assembly details
3. **Review**: `src/PayloadSupervisor.cpp` - Understand C++ layer
4. **Debug**: Set breakpoint and step through execution
5. **Modify**: Customize protection loop or add new features

## 🔐 Security Notes

- This is **educational code only**
- Never use for unauthorized system access
- The protection loop consumes **100% CPU** on one thread
- Requires **administrator privileges** for some operations
- Debuggers will be detected and behavior will change

## ✅ Verification Checklist

- [ ] Project folder exists at `D:\NoRefusalPayload`
- [ ] CMakeLists.txt and source files present
- [ ] VS Code opens without errors
- [ ] CMake configures automatically
- [ ] Build completes successfully (Ctrl+Shift+B)
- [ ] Executable runs (see output above)
- [ ] Can set breakpoints and debug (F5)

## 💡 Next Steps

1. **Modify the payload**: Edit `asm/norefusal_core.asm`
2. **Add features**: Extend `PayloadSupervisor.cpp`
3. **Experiment**: Change the protection loop behavior
4. **Profile**: Use Windows Performance Analyzer
5. **Integrate**: Add to larger project structure

## 📞 Quick Reference

| Action | Keyboard | Menu |
|--------|----------|------|
| Build | `Ctrl+Shift+B` | Terminal → Run Task → build-debug |
| Debug | `F5` | Run → Start Debugging |
| Stop | `Shift+F5` | Run → Stop Debugging |
| Run Terminal | `Ctrl+`` ` `` | Terminal → New Terminal |
| Open Cmd Palette | `Ctrl+Shift+P` | View → Command Palette |

---

**Happy coding!** For detailed documentation, see `README.md` and `VSCODE_INTEGRATION.md`.
