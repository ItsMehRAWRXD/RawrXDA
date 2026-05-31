# RawrXD Un-TurnKey Documentation
# Manual Steps That Cannot Be Automated

**Version**: 14-Day Sprint Final  
**Date**: April 20, 2026  
**Purpose**: Document manual steps for expert users and edge cases

---

## What Is Un-TurnKey?

While the Turnkey Deployment (`Turnkey-Deploy.ps1`) automates 95% of the setup process, there remain scenarios where manual intervention is required or preferred:

1. **Expert customization** - Advanced users who want fine-grained control
2. **Edge cases** - Unusual system configurations
3. **Troubleshooting** - When automation fails
4. **Enterprise deployment** - Corporate environments with restrictions
5. **Development** - Contributing to RawrXD itself

---

## Manual Step 1: Visual Studio Installation (If Auto-Install Blocked)

### When to Do This
- Corporate policy blocks automated installation
- Specific VS edition required (Enterprise, Professional)
- Custom installation directory needed

### Manual Steps

1. **Download Visual Studio 2022**
   ```
   https://visualstudio.microsoft.com/downloads/
   ```

2. **Select Workloads**
   - ☑ Desktop development with C++
   - ☑ MSVC v143 - VS 2022 C++ x64/x86 build tools
   - ☑ Windows 10/11 SDK (10.0.19041.0 or later)
   - ☑ C++ CMake tools for Windows (optional but recommended)

3. **Custom Installation Path** (if needed)
   ```powershell
   # Set custom path before running turnkey
   $env:VS_CUSTOM_PATH = "D:\VS2022"
   ```

4. **Verify Installation**
   ```cmd
   "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
   where ml64.exe
   where link.exe
   ```

---

## Manual Step 2: Windows SDK Installation (If Missing)

### When to Do This
- Windows SDK not included in VS installation
- Specific SDK version required
- SDK corrupted or incomplete

### Manual Steps

1. **Download Windows SDK**
   ```
   https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
   ```

2. **Install Required Components**
   - Windows SDK Signing Tools for Desktop
   - Windows SDK for UWP Managed Apps
   - Windows SDK for UWP C++
   - Windows SDK for Desktop C++ x64/x86

3. **Verify SDK Path**
   ```powershell
   Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.19041.0\um\windows.h"
   ```

---

## Manual Step 3: Model Download (Manual)

### When to Do This
- Air-gapped systems without internet
- Custom model selection
- Model validation required
- Corporate proxy blocks download

### Manual Steps

1. **Download Model from HuggingFace**
   ```
   https://huggingface.co/models?search=gguf
   ```

2. **Recommended Models**
   | Model | Size | Use Case |
   |-------|------|----------|
   | Llama-2-7B-Q4_K_M | 4.1 GB | General purpose |
   | Mistral-7B-Q4_K_M | 4.5 GB | Better reasoning |
   | Phi-2-Q4_K_M | 1.6 GB | Fast inference |
   | CodeLlama-7B-Q4_K_M | 4.1 GB | Code generation |

3. **Verify Checksum** (optional but recommended)
   ```powershell
   Get-FileHash .\llama-2-7b.Q4_K_M.gguf -Algorithm SHA256
   # Compare with published hash
   ```

4. **Place in Models Directory**
   ```powershell
   mkdir D:\rawrxd\models -Force
   Copy-Item .\llama-2-7b.Q4_K_M.gguf D:\rawrxd\models\
   ```

---

## Manual Step 4: Configuration File Editing

### When to Do This
- Advanced inference parameters needed
- Custom UI settings
- Multiple model management
- API endpoint customization

### Manual Steps

1. **Locate Configuration File**
   ```powershell
   $configPath = "$env:APPDATA\RawrXD\rawrxd.config.json"
   # Or project root
   $configPath = "D:\rawrxd\rawrxd.config.json"
   ```

2. **Edit Configuration**
   ```json
   {
     "model": {
       "path": "D:\\models",
       "default_model": "llama-2-7b.Q4_K_M.gguf",
       "max_context": 8192,
       "gpu_layers": 0
     },
     "inference": {
       "temperature": 0.7,
       "top_p": 0.9,
       "top_k": 40,
       "max_tokens": 2048,
       "repeat_penalty": 1.1,
       "batch_size": 512
     },
     "ui": {
       "theme": "dark",
       "font": "Consolas",
       "font_size": 12,
       "tab_size": 4,
       "word_wrap": true
     },
     "paths": {
       "projects": "D:\\Projects",
       "temp": "D:\\Temp",
       "logs": "D:\\Logs"
     },
     "advanced": {
       "mlock": false,
       "mmap": true,
       "num_threads": 0
     }
   }
   ```

3. **Validate JSON**
   ```powershell
   Get-Content $configPath -Raw | ConvertFrom-Json | Out-Null
   Write-Host "Configuration valid" -ForegroundColor Green
   ```

---

## Manual Step 5: Build from Source (Full Manual)

### When to Do This
- Source modifications needed
- Debug build required
- Custom compiler flags
- Profiling/optimization

### Manual Steps

1. **Open VS Developer Command Prompt**
   ```
   Start Menu → Visual Studio 2022 → Developer Command Prompt for VS 2022
   ```

2. **Navigate to Project**
   ```cmd
   cd /d D:\rawrxd
   ```

3. **Create Build Directory**
   ```cmd
   mkdir build-manual
   cd build-manual
   ```

4. **Configure with CMake**
   ```cmd
   cmake .. -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_BUILD_TYPE=Release ^
     -DCMAKE_C_COMPILER=cl ^
     -DCMAKE_CXX_COMPILER=cl ^
     -DCMAKE_ASM_MASM_COMPILER=ml64
   ```

5. **Build**
   ```cmd
   cmake --build . --config Release --parallel 8
   ```

6. **Verify Output**
   ```cmd
   dir bin\Release\RawrXD-Win32IDE.exe
   ```

---

## Manual Step 6: Troubleshooting

### Issue: ml64.exe Not Found

**Symptoms**: Build fails with "ml64.exe not found"

**Manual Fix**:
```powershell
# Find ml64.exe
$ml64 = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "ml64.exe" | Select-Object -First 1

# Add to PATH
$env:PATH = "$($ml64.DirectoryName);$env:PATH"

# Verify
where ml64.exe
```

### Issue: Windows SDK Not Found

**Symptoms**: "windows.h not found" or "sdkddkver.h not found"

**Manual Fix**:
```powershell
# Set SDK paths manually
$env:INCLUDE = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\ucrt"
$env:LIB = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\ucrt\x64"
```

### Issue: Model Won't Load

**Symptoms**: "Failed to load model" or "GGUF format error"

**Manual Fix**:
```powershell
# Verify model file
$modelPath = "D:\rawrxd\models\llama-2-7b.Q4_K_M.gguf"

# Check file exists and has content
if ((Test-Path $modelPath) -and ((Get-Item $modelPath).Length -gt 1GB)) {
    Write-Host "Model file looks valid" -ForegroundColor Green
} else {
    Write-Host "Model file missing or corrupt" -ForegroundColor Red
}

# Try alternative model
# Download from different source
```

### Issue: IDE Crashes on Launch

**Symptoms**: "RawrXD-Win32IDE.exe has stopped working"

**Manual Fix**:
```powershell
# Check dependencies
$binary = "D:\rawrxd\bin\RawrXD-Win32IDE.exe"
dumpbin /dependents $binary

# Check for missing DLLs
$required = @("kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll")
foreach ($dll in $required) {
    $path = "C:\Windows\System32\$dll"
    if (Test-Path $path) {
        Write-Host "✓ $dll" -ForegroundColor Green
    } else {
        Write-Host "✗ $dll MISSING" -ForegroundColor Red
    }
}

# Check Event Viewer
Get-WinEvent -FilterHashtable @{LogName='Application'; ID=1000} -MaxEvents 5 | Format-List
```

---

## Manual Step 7: Enterprise Deployment

### When to Do This
- Corporate environment with restrictions
- Group Policy constraints
- Network isolated systems
- License compliance requirements

### Manual Steps

1. **Pre-Installation Checklist**
   - [ ] IT approval obtained
   - [ ] License compliance verified
   - [ ] Network exceptions requested (if needed)
   - [ ] Antivirus exclusions configured

2. **Silent Installation**
   ```powershell
   # Run with admin rights
   .\Turnkey-Deploy.ps1 -SkipModelDownload
   
   # Model must be pre-staged
   mkdir C:\ProgramData\RawrXD\models
   Copy-Item \\fileserver\models\*.gguf C:\ProgramData\RawrXD\models\
   ```

3. **System-Wide Configuration**
   ```powershell
   # Create system-wide config
   $systemConfig = @{
       model = @{ path = "C:\ProgramData\RawrXD\models" }
       paths = @{
           projects = "$env:PUBLIC\Documents\RawrXD\Projects"
           temp = "$env:TEMP\RawrXD"
       }
   }
   
   $systemConfig | ConvertTo-Json -Depth 3 | 
       Out-File "C:\ProgramData\RawrXD\config.json"
   ```

4. **Shortcut Creation**
   ```powershell
   $WshShell = New-Object -ComObject WScript.Shell
   $Shortcut = $WshShell.CreateShortcut("C:\ProgramData\Microsoft\Windows\Start Menu\Programs\RawrXD IDE.lnk")
   $Shortcut.TargetPath = "C:\Program Files\RawrXD\RawrXD-Win32IDE.exe"
   $Shortcut.Save()
   ```

---

## Quick Reference: Manual Commands

```powershell
# Full manual build
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Run with custom config
$env:RAWRXD_CONFIG = "D:\custom\config.json"
.\RawrXD-Win32IDE.exe

# Debug mode
.\RawrXD-Win32IDE.exe --debug --log-level verbose

# Validate installation
Test-Path .\RawrXD-Win32IDE.exe
.\RawrXD-Win32IDE.exe --version
```

---

## Support

If manual steps don't resolve your issue:

1. Check logs: `%TEMP%\rawrxd-*.log`
2. Run validation: `.\scripts\turnkey\Validate-Deployment.ps1`
3. Review documentation: `docs\TROUBLESHOOTING.md`
4. Submit issue with logs attached

---

**Remember**: The Turnkey Deployment is the recommended approach. Only use these manual steps when necessary.
