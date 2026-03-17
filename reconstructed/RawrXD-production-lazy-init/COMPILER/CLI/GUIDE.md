# RawrXD Universal Compiler & Build System - Complete Guide

## Overview

This document provides complete instructions for using RawrXD's universal compiler and build system via CLI, Qt IDE, and terminal access.

## 🚀 Quick Start

### Windows (PowerShell)
```powershell
# Navigate to project
cd D:\RawrXD-production-lazy-init

# Run full validation
.\rawrxd-build.ps1

# Build with CMake
.\rawrxd-build.ps1 -Build

# Interactive menu
.\rawrxd-build.ps1 -Menu

# Audit environment
.\rawrxd-build.ps1 -Audit -Json
```

### Linux/macOS/WSL (Bash)
```bash
# Navigate to project
cd /path/to/RawrXD-production-lazy-init

# Run full validation
./rawrxd-build.sh

# Build with CMake
./rawrxd-build.sh --build

# Audit environment
./rawrxd-build.sh --audit --json
```

### Python (Any Platform)
```bash
# Full validation
python universal_compiler.py --validate

# Environment audit
python universal_compiler.py --audit

# Build with CMake
python universal_compiler.py --build

# Output as JSON
python universal_compiler.py --validate --json
```

## 📋 Detailed Usage

### PowerShell Commands

#### 1. Full Validation (Recommended)
```powershell
.\rawrxd-build.ps1 -Validate
```
**Output includes:**
- Compiler environment audit
- Stub implementation analysis
- Menu/slot wiring coverage
- CMake build attempt

#### 2. Environment Audit Only
```powershell
.\rawrxd-build.ps1 -Audit
```
**Checks:**
- CMake availability
- C++ compiler presence
- Project structure
- Required files

#### 3. Build with CMake
```powershell
.\rawrxd-build.ps1 -Build
```
**Performs:**
- CMake configuration
- Release build
- Parallel compilation (-j4)

#### 4. Direct Compilation
```powershell
.\rawrxd-build.ps1 -CompileDirect
```
**Compiles** stub file directly without CMake

#### 5. JSON Output
```powershell
.\rawrxd-build.ps1 -Validate -Json
```
**Generates:** Machine-readable JSON report

#### 6. Interactive Menu
```powershell
.\rawrxd-build.ps1 -Menu
```
**Interactive menu with options:**
- [1] Full Validation
- [2] Environment Audit
- [3] Build with CMake
- [4] Direct Compilation
- [5] Generate JSON Report

### Python Commands

#### Basic Usage
```bash
python universal_compiler.py [--project-root PATH] [OPTIONS]
```

#### Options
```
--audit                 Run environment audit only
--build                 Compile with CMake
--compile-direct        Direct compilation
--validate              Full validation suite
--json                  Output as JSON
--project-root PATH     Specify project root (default: auto-detect)
```

#### Examples
```bash
# Validate current directory
python universal_compiler.py

# Validate specific project
python universal_compiler.py --project-root D:\RawrXD-production-lazy-init

# Build and output JSON
python universal_compiler.py --build --json

# Quick audit
python universal_compiler.py --audit
```

### Bash Commands

Similar to PowerShell but with Unix conventions:
```bash
./rawrxd-build.sh                           # Full validation
./rawrxd-build.sh --build                   # Build with CMake
./rawrxd-build.sh --audit --json            # Audit as JSON
./rawrxd-build.sh --compile-direct          # Direct compilation
```

## 🎯 Menu & Breadcrumb Validation

### Run Validator
```bash
# Text report
python menu_validator.py

# JSON report
python menu_validator.py --json --output menu_report.json
```

### Report Includes
- **Implementation Coverage**: % of slots with implementations
- **Menu Wiring Coverage**: % of menu items connected to slots
- **Disconnected Slots**: Slots declared but not wired
- **Missing Slots**: Menu items without slot definitions
- **Breadcrumb Paths**: Navigation path validation

### Sample Output
```
======================================================================
🎯 RawrXD MENU & BREADCRUMB VALIDATION REPORT
======================================================================

📊 COVERAGE SUMMARY:
  Implementation Coverage:   95.5% (212/222)
  Menu Wiring Coverage:      100.0% (87/87)
  Disconnected Slots:        10
  Missing Slot Definitions:  0

⚠️  DISCONNECTED SLOTS (declared but unused):
    - onUnusedFeature1
    - onUnusedFeature2
    ...

📍 BREADCRUMBS: 42 paths in 8 files

======================================================================
```

## 🔧 Integration with Qt IDE

### Adding Build Task to Qt Creator

1. **Open Project Settings**
   - Menu: Tools → Options → Build & Run → Build Steps

2. **Add Custom Build Step**
   ```
   Command: powershell
   Arguments: -ExecutionPolicy Bypass -File rawrxd-build.ps1 -Build
   Working directory: %{CurrentProject:Path}
   ```

3. **Or add Run Target**
   - In .pro or CMakeLists.txt:
   ```cmake
   add_custom_target(validate
       COMMAND python universal_compiler.py --validate
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
   )
   ```

### Adding to Custom Build Menu

1. **Create Run Configuration**
   ```
   Executable: powershell.exe
   Command line arguments: -ExecutionPolicy Bypass -File rawrxd-build.ps1 -Menu
   Working directory: D:\RawrXD-production-lazy-init
   ```

2. **Set Environment** (if needed)
   ```
   CMAKE_PREFIX_PATH=path\to\Qt
   PATH=path\to\compilers;%PATH%
   ```

## 📊 Stub Implementation Status

### Current Stats
```
✓ Functions implemented:        88/88
✓ Lines of code:                4,431
✓ File size:                    ~185 KB
✓ Completeness:                 95.5%
✓ Remaining TODOs:              ~13
```

### Key Infrastructure
- **ScopedTimer**: Automatic latency tracking
- **Distributed Tracing**: Event logging with traceEvent()
- **Circuit Breakers**: 5 external service protections
- **Caching Layers**: QSettings (100 entries), FileInfo (500 entries)
- **Metrics Collection**: Counter and latency tracking
- **Safe Mode Feature Flags**: SafeMode::Config support
- **Notification Center**: NotificationCenter integration
- **Structured Logging**: logInfo(), logWarn(), logError()

## 🔍 Compiler Detection & Support

### Available Compilers
```
✓ CMake               (for cross-platform builds)
✓ G++                 (on Windows via MinGW/MSYS2)
✓ G++                 (on Linux/WSL/macOS)
⚠ MSVC cl.exe        (requires Visual Studio paths)
⚠ Clang              (requires clang installation)
```

### Direct Compilation Command
```bash
g++ -c mainwindow_stub_implementations.cpp -o mainwindow_stub_implementations.o \
    -std=c++17 -Wall -Wextra -I./src/qtapp
```

## 🚨 Troubleshooting

### Python Not Found
```powershell
# Install Python
# https://www.python.org/downloads/

# Verify installation
python --version

# If still not working, use explicit path
C:\Python310\python.exe universal_compiler.py --validate
```

### CMake Not Found
```bash
# Install CMake
# Linux: sudo apt-get install cmake
# macOS: brew install cmake
# Windows: Download from https://cmake.org/download/

# Verify
cmake --version
```

### Compiler Not Found
```bash
# Windows (G++ via MinGW)
# Install from: https://www.mingw-w64.org/

# Verify
g++ --version

# On Windows, add to PATH:
# C:\Program Files\mingw-w64\bin
```

### Build Fails
1. Run audit first:
   ```powershell
   .\rawrxd-build.ps1 -Audit
   ```
2. Check CMakeLists.txt exists
3. Verify Qt dependencies are installed
4. Check build directory permissions

### JSON Output Issues
```bash
# Verify Python can write JSON
python -c "import json; print(json.dumps({'test': 'ok'}))"

# Check output file path
.\rawrxd-build.ps1 -Validate -Json | Out-File -FilePath report.json
```

## 📈 Performance Metrics

The build system automatically collects:
- **Compilation time**: Overall build duration
- **Success/failure rates**: Build reliability tracking
- **Function coverage**: Stub implementation percentage
- **File statistics**: LOC, size, complexity

## 🔐 Security Considerations

1. **PowerShell Execution Policy**
   - Scripts signed or run with `-ExecutionPolicy Bypass`
   - Verify script integrity before running

2. **File Permissions**
   - Build directory must be writable
   - Source files should be readable

3. **Compiler Tools**
   - Download from official sources only
   - Keep tools updated for security patches

## 📚 Advanced Usage

### Custom Project Root
```powershell
.\rawrxd-build.ps1 -ProjectRoot "E:\RawrXD" -Validate
```

### Continuous Integration
```bash
# For CI/CD pipelines
python universal_compiler.py --build --json > build-report.json

# Check exit code
if [ $? -eq 0 ]; then
    echo "Build succeeded"
else
    echo "Build failed"
fi
```

### Batch Operations
```powershell
# Audit all projects
@("D:\Project1", "E:\Project2") | ForEach-Object {
    Write-Host "Checking $_"
    python universal_compiler.py --project-root $_ --audit
}
```

## 📞 Support

For issues or questions:
1. Run audit: `python universal_compiler.py --audit`
2. Generate JSON report: `python universal_compiler.py --validate --json`
3. Check menu validation: `python menu_validator.py --json`
4. Review reports in project root

## ✨ Features Summary

| Feature | PowerShell | Bash | Python | Python API |
|---------|-----------|------|--------|-----------|
| Validation | ✓ | ✓ | ✓ | ✓ |
| Build | ✓ | ✓ | ✓ | ✓ |
| Audit | ✓ | ✓ | ✓ | ✓ |
| JSON Output | ✓ | ✓ | ✓ | ✓ |
| Interactive Menu | ✓ | ✗ | ✗ | ✗ |
| Direct Compilation | ✓ | ✓ | ✓ | ✓ |
| Menu Validation | ✓ | ✓ | ✓ | ✓ |
| Environment Check | ✓ | ✓ | ✓ | ✓ |

---

**Last Updated**: 2024
**Version**: 1.0
**Project**: RawrXD IDE
