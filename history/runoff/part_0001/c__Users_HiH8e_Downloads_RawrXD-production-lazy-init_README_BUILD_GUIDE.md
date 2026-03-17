# RawrXD Complete Build & Installation Guide

## 🎯 Overview

This document provides complete instructions for building and using **RawrXD** - a sophisticated Qt6-based IDE with integrated MASM (x64 Assembly) support, featuring:

- **Qt C++ IDE** with advanced UI and feature management
- **MASM Feature Toggle System** - 212 features across 32 categories
- **Pure x64 MASM Components** - Threading, Chat Panels, Signal/Slot systems
- **Real-time Metrics Dashboard** - Performance monitoring
- **5-Preset Configuration** - From Minimal to Maximum

---

## 📋 Prerequisites

### **Required:**
- Windows 10 or 11 (x64)
- Visual Studio 2022 (Community, Professional, or BuildTools)
  - C++ Desktop Development workload
  - MASM support (ml64.exe)
- CMake 3.20+
- Qt 6.7.3 (installed at `C:\Qt\6.7.3\msvc2022_64\`)

### **Optional but Recommended:**
- CUDA Toolkit (for NVIDIA GPU support)
- Vulkan SDK (for graphics optimization)
- Git (for version control)

### **Check Installation:**

```batch
# Test CMake
cmake --version

# Test Visual Studio
cl.exe /?

# Test ml64 assembler
ml64.exe /?

# Test Qt
dir C:\Qt\6.7.3\msvc2022_64\bin
```

---

## 🚀 Quick Build (5 Steps)

### **Step 1: Navigate to Project**
```batch
cd C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init
```

### **Step 2: Run Build Script**
```batch
Build-RawrXD-Complete.bat
```

### **Step 3: Wait for Compilation**
- Phase 1 (Qt C++): ~5-10 minutes
- Phase 2 (MASM): ~2-3 minutes
- Total: ~10-15 minutes

### **Step 4: Verify Output**
```batch
# Check Qt executable
dir build\bin\Release\RawrXD-QtShell.exe

# Check MASM object files
dir src\masm\final-ide\obj\*.obj
```

### **Step 5: Launch**
```batch
# Launch Qt IDE
build\bin\Release\RawrXD-QtShell.exe

# Then: Tools → MASM Feature Settings
```

---

## 🔧 Advanced Build Options

### **PowerShell Build (with auto-launch):**
```powershell
powershell -ExecutionPolicy Bypass -File Build-RawrXD-Complete.ps1 -Run
```

### **Clean Rebuild:**
```batch
Build-RawrXD-Complete.bat /clean
```

### **Verbose Output:**
```powershell
powershell -ExecutionPolicy Bypass -File Build-RawrXD-Complete.ps1 -Verbose
```

### **Manual CMake Build:**
```batch
# Configure
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DQt6_DIR=C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6 ^
    -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2022_64

# Build
cmake --build . --config Release --parallel 8 --target RawrXD-QtShell
```

### **Manual MASM Compilation:**
```batch
cd src\masm\final-ide

# Compile support modules
ml64.exe /c /Cp /nologo /Zi /Fo"obj\asm_memory_x64.obj" asm_memory_x64.asm
ml64.exe /c /Cp /nologo /Zi /Fo"obj\asm_string_x64.obj" asm_string_x64.asm
ml64.exe /c /Cp /nologo /Zi /Fo"obj\console_log_x64.obj" console_log_x64.asm

# Compile Phase 3 components
ml64.exe /c /Cp /nologo /Zi /Fo"obj\threading_system.obj" threading_system.asm
ml64.exe /c /Cp /nologo /Zi /Fo"obj\chat_panels.obj" chat_panels.asm
ml64.exe /c /Cp /nologo /Zi /Fo"obj\signal_slot_system.obj" signal_slot_system.asm

# Link executable
cd obj
link.exe /NOLOGO /SUBSYSTEM:WINDOWS ^
    /OUT:"..\bin\RawrXD-Pure-MASM-IDE.exe" ^
    *.obj kernel32.lib user32.lib gdi32.lib
```

---

## 📚 Using MASM Feature Settings

### **Launch Features Panel:**

1. Start RawrXD-QtShell.exe
2. Click **Tools** menu
3. Select **MASM Feature Settings**

### **Interface Overview:**

```
┌─────────────────────────────────────────┐
│ MASM Feature Settings                   │
├─────────────────────────────────────────┤
│ [Feature Browser] [Metrics] [Details]   │
├─────────────────────────────────────────┤
│                                         │
│ Preset: [Minimal ▼]  [Apply] [Reset]   │
│                                         │
│ ☐ Threading & Concurrency              │
│   ☐ Thread Creation                     │
│   ☐ Mutex Management                    │
│   ☐ Semaphore Sync                      │
│ ☐ GPU Computing                         │
│   ☐ CUDA Support                        │
│   ☐ Vulkan Backend                      │
│ ... 30 more categories ...              │
│                                         │
│          [Export] [Import] [OK]         │
└─────────────────────────────────────────┘
```

### **Feature Categories (32 Total):**

| Category | Features | Description |
|----------|----------|-------------|
| Threading | 16 | Multithreading, synchronization |
| GPU Computing | 12 | CUDA, ROCm, Vulkan |
| Inference | 14 | GGML, TensorFlow, PyTorch |
| Memory | 18 | Allocation, caching, optimization |
| File I/O | 12 | Disk operations, streaming |
| Networking | 10 | TCP/IP, HTTP, WebSockets |
| Caching | 8 | LRU, memcache, Redis |
| UI Rendering | 22 | Qt rendering, themes, layouts |
| Security | 11 | Encryption, SSL/TLS, auth |
| Logging | 9 | Structured logging, telemetry |
| + 22 more | 88 | Various system components |

### **Preset Configurations:**

| Preset | Size | Features | Use Case |
|--------|------|----------|----------|
| **Minimal** | 10 MB | 45 | Embedded systems, low resources |
| **Light** | 25 MB | 80 | Development machines |
| **Medium** | 45 MB | 140 | Production servers |
| **Heavy** | 70 MB | 185 | Enterprise deployments |
| **Maximum** | 85 MB | 212 | Full-featured installations |

### **Hot-Reload Features (32 total):**

These features can be enabled/disabled **without restarting:**

- Logging levels
- Metrics collection
- Theme switching
- Caching backends
- Network protocols
- And 27 more...

---

## 📊 Metrics Dashboard

View real-time performance statistics:

```
Feature: Threading_System
├── CPU Time: 2.34 ms
├── Memory Used: 512 KB
├── Call Count: 1,245
├── Avg Latency: 1.87 µs
├── Max Latency: 45.2 µs
├── Errors: 0
├── Hit Rate: 98.5%
├── Last Called: 2.3s ago
├── Enabled: ✓
└── Category: Threading & Concurrency
```

---

## 🎯 MASM Feature Manager API

### **Core Methods (44 Total)**

#### **Feature Control:**
```cpp
// Enable/disable individual features
bool enable_feature(const QString& name);
bool disable_feature(const QString& name);
bool is_feature_enabled(const QString& name);
int get_enabled_features_count();
```

#### **Preset Management:**
```cpp
void apply_preset(const QString& preset);  // Minimal/Light/Medium/Heavy/Maximum
QString get_current_preset();
void save_custom_preset(const QString& name);
void load_custom_preset(const QString& name);
```

#### **Hot-Reload:**
```cpp
void hot_reload_feature(const QString& name);
void hot_reload_all_features();
bool is_feature_hot_reloadable(const QString& name);
int get_hot_reloadable_count();
```

#### **Metrics:**
```cpp
PerformanceMetrics get_feature_metrics(const QString& name);
void reset_feature_metrics(const QString& name);
QString export_metrics_json();
double get_category_average_latency(const QString& category);
```

#### **Configuration:**
```cpp
QString export_configuration_json();
bool import_configuration_json(const QString& json);
QMap<QString, bool> get_configuration();
bool validate_configuration(const QMap<QString, bool>& config);
```

---

## 🔍 Build Troubleshooting

### **Issue: `ml64.exe` not found**

**Solution:**
```batch
# Add to PATH or use full path:
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe
```

### **Issue: Qt not found**

**Solution:**
```batch
# Verify Qt installation:
dir C:\Qt\6.7.3\msvc2022_64\bin

# Or set in CMake:
-DQt6_DIR=C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6
```

### **Issue: CMake configuration fails**

**Solution:**
```batch
# Clean and retry:
rmdir /s /q build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

### **Issue: MASM compilation errors**

**Common causes:**
- Invalid instruction syntax
- Missing include files
- Incorrect directives

**Solution:**
```batch
# Compile with error details:
ml64.exe /c /Zi /Fo"output.obj" /Fl"output.lst" input.asm

# Check .lst file for errors
```

### **Issue: Linker errors**

**Solution:**
```batch
# Check linking command:
link.exe /NOLOGO /MAP:output.map /OUT:result.exe *.obj kernel32.lib

# Review .map file for undefined symbols
```

---

## 📁 Project Structure

```
RawrXD-production-lazy-init/
│
├── Build-RawrXD-Complete.bat              ← USE THIS
├── Build-RawrXD-Complete.ps1              ← OR THIS
├── FULL_BUILD_SUMMARY.md                  ← Complete summary
├── README_BUILD_GUIDE.md                  ← THIS FILE
│
├── CMakeLists.txt                         ← Qt build config
├── build/                                 ← OUTPUT: Qt compilation
│   ├── bin/Release/
│   │   └── RawrXD-QtShell.exe            ← Qt IDE executable
│   └── RawrXD-QtShell.vcxproj
│
├── src/
│   ├── qtapp/
│   │   ├── MainWindow.cpp/.h
│   │   ├── masm_feature_manager.cpp       ← 850 LOC, 44 API methods
│   │   ├── masm_feature_settings_panel.cpp/.h ← UI, 1,400 LOC
│   │   └── ...
│   │
│   └── masm/final-ide/
│       ├── threading_system.asm           ← 1,196 LOC, Phase 3
│       ├── chat_panels.asm                ← 1,432 LOC, Phase 3
│       ├── signal_slot_system.asm         ← 1,333 LOC, Phase 3
│       ├── win32_window_framework.asm     ← 819 LOC, Phase 1
│       ├── menu_system.asm                ← 644 LOC, Phase 1
│       ├── masm_theme_system_complete.asm ← 836 LOC, Phase 2
│       ├── masm_file_browser_complete.asm ← 1,106 LOC, Phase 2
│       ├── asm_memory_x64.asm             ← Support
│       ├── asm_string_x64.asm             ← Support
│       ├── console_log_x64.asm            ← Support
│       │
│       ├── obj/                           ← OUTPUT: .obj files
│       │   ├── asm_memory_x64.obj
│       │   ├── asm_string_x64.obj
│       │   ├── threading_system.obj
│       │   ├── chat_panels.obj
│       │   └── ... (10 total)
│       │
│       └── bin/                           ← OUTPUT: Executables
│           └── RawrXD-Pure-MASM-IDE.exe
│
└── 3rdparty/
    └── ggml/                              ← GGML submodule
```

---

## 🎓 Understanding the Architecture

### **Three-Layer Integration:**

```
┌─────────────────────────────────────┐
│  Qt GUI Layer                       │
│  (MainWindow, Dialogs, Menus)      │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  MASM Feature Manager               │
│  (44 API methods, 212 features)    │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│  Pure x64 MASM Components           │
│  • Threading (1,196 LOC)           │
│  • Chat (1,432 LOC)                │
│  • Signals (1,333 LOC)             │
│  • Win32 Framework                 │
│  • Theme System                    │
└─────────────────────────────────────┘
```

### **Data Flow:**

```
User Action (Enable Feature)
    ↓
Qt UI Layer (Settings Panel)
    ↓
MASM Feature Manager API
    ↓
Feature Toggle Logic
    ↓
MASM Component State Update
    ↓
Real-time Metrics Collection
    ↓
UI Dashboard Refresh
```

---

## 📈 Performance Expectations

| Operation | Latency | Notes |
|-----------|---------|-------|
| Enable Feature | <1 ms | In-memory toggle |
| Apply Preset | 10-50 ms | Batch updates |
| Hot-Reload Feature | 10-100 ms | Safe reloading |
| Metrics Update | <1 µs | Per-feature overhead |
| Export Config | 5-20 ms | JSON serialization |
| Import Config | 10-30 ms | Validation + updates |

---

## ✅ Verification Checklist

After building, verify:

- [ ] Qt IDE launches: `build\bin\Release\RawrXD-QtShell.exe`
- [ ] MASM Feature Settings opens: Tools → MASM Feature Settings
- [ ] All 212 features visible in tree
- [ ] Presets apply without errors
- [ ] Metrics dashboard shows data
- [ ] Features can be toggled individually
- [ ] Export/Import works correctly
- [ ] MASM object files exist: `src\masm\final-ide\obj\*.obj`

---

## 🆘 Support & Debugging

### **Enable Verbose Logging:**
```cpp
// In MainWindow initialization
MasmFeatureManager::getInstance()->setDebugLogging(true);
```

### **Check Build Log:**
```batch
# Build output is saved to:
build-output.log  (if using redirection)

# CMake cache:
build\CMakeCache.txt
```

### **Verify MASM Compilation:**
```batch
# Check object files
dir src\masm\final-ide\obj\*.obj /s

# Inspect object file symbols
dumpbin.exe /symbols src\masm\final-ide\obj\threading_system.obj
```

---

## 📝 License & Credits

- **RawrXD**: MASM/Qt Integration Framework
- **Qt 6.7.3**: UI framework
- **GGML**: Quantized inference engine
- **MASM**: Microsoft Macro Assembler (x64)

---

## 🎉 Summary

You now have a complete, production-ready build system for RawrXD featuring:

✅ Qt6-based IDE with advanced UI  
✅ 212-feature MASM toggle system  
✅ 5-preset configuration  
✅ Real-time metrics monitoring  
✅ Pure x64 MASM components  
✅ Complete build automation  

**Ready to build?**
```batch
cd C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init
.\Build-RawrXD-Complete.bat
```

**Estimated build time: 10-15 minutes**

---

*Last Updated: December 28, 2025*  
*Build System Version: 2.0*  
*Status: ✅ PRODUCTION READY*
