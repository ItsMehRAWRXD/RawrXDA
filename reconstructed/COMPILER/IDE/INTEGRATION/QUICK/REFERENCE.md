# RawrXD IDE - Quick Reference Guide

## 🚀 Quick Start Commands

### PowerShell (Windows Native)
```powershell
# View compiler status
D:\compiler-manager-fixed.ps1 -Status

# Full system audit
D:\compiler-manager-fixed.ps1 -Audit

# Build Debug
D:\compiler-manager-fixed.ps1 -Build

# Build Release
D:\compiler-manager-fixed.ps1 -Build -Config Release

# Setup environment
D:\compiler-manager-fixed.ps1 -Setup
```

### Python CLI (Cross-Platform)
```bash
# Detect compilers
python D:\compiler-cli.py detect

# Full system audit
python D:\compiler-cli.py audit

# Build Debug configuration
python D:\compiler-cli.py build --config Debug

# Build Release configuration
python D:\compiler-cli.py build --config Release

# Clean build directory
python D:\compiler-cli.py clean

# Run tests
python D:\compiler-cli.py test
```

### VS Code
```
Ctrl+Shift+B       → Build menu (select build task)
F5                 → Start debugging
Ctrl+Shift+D       → Debug view
Ctrl+Shift+`       → Open terminal
Ctrl+B             → Toggle sidebar
```

### Qt Creator
```bash
# Setup and launch
python D:\qt-creator-launcher.py --setup --launch

# Verify setup
python D:\qt-creator-launcher.py --verify

# Just setup kits
python D:\qt-creator-launcher.py --setup
```

---

## 🔍 Compiler Detection Results

### Available Compilers
| Compiler | Version | Location |
|----------|---------|----------|
| GCC (MinGW) | 15.2.0 | `C:\ProgramData\mingw64\mingw64\bin\g++.exe` |
| Clang | 21.1.6 | `C:\Program Files\LLVM\bin\clang++.exe` |
| CMake | 4.2.0 | `C:\Program Files\CMake\bin\cmake.exe` |

### System Status
- ✅ Build environment: READY
- ✅ Compilers detected: 3
- ✅ CMake available: Yes
- ✅ Git available: Yes
- ✅ Source files: Found

---

## 📦 Tool Files

| Tool | File | Purpose |
|------|------|---------|
| PowerShell CLI | `D:\compiler-manager-fixed.ps1` | Windows native build automation |
| Python CLI | `D:\compiler-cli.py` | Cross-platform compiler management |
| IDE Setup | `D:\ide-integration-setup.py` | Generate VS Code/Qt configurations |
| Qt Launcher | `D:\qt-creator-launcher.py` | Qt Creator integration |

---

## 🎨 VS Code Configuration

### Build Tasks (Ctrl+Shift+B)
1. **RawrXD: Build Debug** ← Default
2. RawrXD: Build Release
3. RawrXD: Clean
4. RawrXD: Configure CMake
5. RawrXD: Run Tests

### Debug Configuration (F5)
1. **RawrXD IDE (Debug)** ← Default
2. RawrXD IDE (Release)

### Keyboard Shortcuts
| Command | Shortcut |
|---------|----------|
| Build | Ctrl+Shift+B |
| Debug | F5 |
| Stop | Shift+F5 |
| Step Over | F10 |
| Step Into | F11 |
| Continue | F5 |

---

## 🎯 Common Workflows

### Build and Run
```bash
# PowerShell
D:\compiler-manager-fixed.ps1 -Build
# Then manually run executable

# Python
python D:\compiler-cli.py build
# Then manually run executable

# VS Code
Ctrl+Shift+B → Select "RawrXD: Build Debug"
```

### Debug in VS Code
```
1. Press F5 (or click Debug → Start Debugging)
2. Select "RawrXD IDE (Debug)" configuration
3. Debug window opens, program pauses at entry
4. Set breakpoints by clicking line numbers
5. Press F10/F11 to step, F5 to continue
6. Press Shift+F5 to stop debugging
```

### Full System Check
```powershell
# PowerShell - Complete audit
D:\compiler-manager-fixed.ps1 -Audit

# Python - Complete audit
python D:\compiler-cli.py audit
```

### Switch Build Configuration
```powershell
# Build Release
D:\compiler-manager-fixed.ps1 -Build -Config Release

# Build Debug
D:\compiler-manager-fixed.ps1 -Build -Config Debug
```

---

## 🔧 Configuration Files

### VS Code (.vscode/)
```
.vscode/
├── c_cpp_properties.json    # IntelliSense settings
├── tasks.json               # Build tasks
├── launch.json              # Debug configurations
├── settings.json            # Editor settings
└── extensions.json          # Recommended extensions
```

### CMake
```
CMakeLists.txt              # Project definition
CMakeUserPresets.json       # Build presets for Qt Creator
build/                      # Build output directory
```

---

## ⚙️ Environment Variables

### Set in PowerShell
```powershell
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.7\msvc2022_64"
$env:Qt6_DIR = "C:\Qt\6.7\msvc2022_64"
```

### Set in Python
```python
import os
os.environ['CMAKE_PREFIX_PATH'] = "C:\Qt\6.7\msvc2022_64"
os.environ['Qt6_DIR'] = "C:\Qt\6.7\msvc2022_64"
```

---

## 🐛 Troubleshooting

### Issue: Build fails
```bash
# Full audit to diagnose
python D:\compiler-cli.py audit

# Or PowerShell
D:\compiler-manager-fixed.ps1 -Audit

# Clean and rebuild
python D:\compiler-cli.py clean
python D:\compiler-cli.py build
```

### Issue: IntelliSense not working
1. Close VS Code
2. Delete `.vscode` folder
3. Run: `python D:\ide-integration-setup.py`
4. Reopen VS Code
5. Wait for indexing to complete

### Issue: Can't find compiler
```bash
# Run detection
python D:\compiler-cli.py detect

# PowerShell equivalent
D:\compiler-manager-fixed.ps1 -Status
```

### Issue: Qt Creator doesn't load project
```bash
python D:\qt-creator-launcher.py --setup
```

---

## 📚 Documentation

| Document | Location |
|----------|----------|
| Full Integration Guide | `D:\COMPILER_IDE_INTEGRATION_COMPLETE.md` |
| This Quick Reference | `D:\COMPILER_IDE_INTEGRATION_QUICK_REFERENCE.md` |
| Python CLI Help | `python D:\compiler-cli.py --help` |

---

## 🎓 Common Parameters

### PowerShell Parameters
```powershell
-Audit              # Full system audit
-Status             # Show compiler status
-Setup              # Setup environment
-Build              # Build project
-Test               # Run tests
-Compiler auto|msvc|gcc|clang    # Compiler choice
-Config Debug|Release             # Build config
-BuildDir <path>                  # Build directory
```

### Python CLI Arguments
```bash
detect              # Detect compilers
audit               # Full audit report
build               # Build project
clean               # Clean build
test                # Run tests
--config Debug|Release    # Configuration
--help              # Show help
```

---

## 🎉 You're All Set!

You now have universal compiler access via:
- ✅ PowerShell CLI (Windows native)
- ✅ Python CLI (Cross-platform)
- ✅ VS Code IDE (Recommended for development)
- ✅ Qt Creator IDE (Recommended for Qt development)
- ✅ Direct terminal/CLI access

Choose your preferred interface and start building! 🚀

