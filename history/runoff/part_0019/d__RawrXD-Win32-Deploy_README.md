# RawrXD Win32 IDE - Deployment Package

**Version:** 1.0.0  
**Build Date:** December 18, 2025  
**Package Type:** Portable Deployment

---

## Contents

```
RawrXD-Win32-Deploy/
├── bin/
│   └── AgenticIDEWin.exe      # Main executable
├── config/
│   └── config.json            # Default configuration
├── docs/
│   ├── DEPLOYMENT_GUIDE.md
│   ├── FEATURE_PARITY_FULL.md
│   ├── TESTING_CHECKLIST.md
│   ├── WIN32_DELIVERY_REPORT.md
│   └── WIN32_README.md
├── orchestration/
│   └── (Node.js bridge files)
├── RawrXD.bat                 # CMD launcher
├── RawrXD.ps1                 # PowerShell launcher
└── README.md                  # This file
```

---

## Quick Start

### 1. Set Environment Variable (Required for AI features)
```powershell
# PowerShell (Recommended)
[Environment]::SetEnvironmentVariable("OPENAI_API_KEY", "your-api-key-here", "User")

# Or CMD
setx OPENAI_API_KEY "your-api-key-here"
```

### 2. Install Prerequisites
- **Visual C++ Redistributable 2022** (x64)
  - Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
- **Node.js** (v18 or later)
  - Download: https://nodejs.org/

### 3. Launch IDE

**PowerShell:**
```powershell
.\RawrXD.ps1
```

**CMD:**
```cmd
RawrXD.bat
```

**Direct:**
```
bin\AgenticIDEWin.exe
```

---

## First Run Setup

The launcher automatically:
1. Creates configuration directory: `%APPDATA%\RawrXD\`
2. Copies default config: `config.json`
3. Creates logs directory: `%LOCALAPPDATA%\RawrXD\logs\`
4. Checks for required dependencies

---

## Configuration

Configuration file location: `%APPDATA%\RawrXD\config.json`

Edit to customize:
- Theme (dark/light)
- Default shell (pwsh/cmd/bash)
- Font size and family
- Auto-save settings
- LSP server configuration
- Performance tuning

See `docs/DEPLOYMENT_GUIDE.md` for full configuration reference.

---

## Optional: Add to PATH

To run from any directory:

**PowerShell:**
```powershell
$deployPath = "D:\RawrXD-Win32-Deploy"
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "User")
[Environment]::SetEnvironmentVariable("PATH", "$currentPath;$deployPath", "User")
```

Then run from anywhere:
```powershell
RawrXD.ps1
```

---

## Orchestration Bridge Setup

If using AI orchestration features:

1. Copy or clone `cursor-ai-copilot-extension-win32` to `orchestration/`
2. Install dependencies:
   ```powershell
   cd orchestration
   npm install
   ```
3. Set environment variable:
   ```powershell
   [Environment]::SetEnvironmentVariable("RAWRXD_CURSOR_WIN32_DIR", "D:\RawrXD-Win32-Deploy\orchestration", "User")
   ```

---

## Troubleshooting

### IDE Won't Start
1. Check logs: `%LOCALAPPDATA%\RawrXD\logs\startup.log`
2. Verify Visual C++ Redistributable installed
3. Run from command line to see errors

### AI Features Not Working
1. Verify `OPENAI_API_KEY` is set
2. Check Node.js is installed: `node --version`
3. Review `%LOCALAPPDATA%\RawrXD\logs\chat.log`

### Missing DLL Errors
Install Visual C++ Redistributable 2022 (x64):
https://aka.ms/vs/17/release/vc_redist.x64.exe

---

## Documentation

Full documentation available in `docs/` directory:

- **DEPLOYMENT_GUIDE.md** - Detailed installation guide
- **WIN32_README.md** - Feature overview
- **FEATURE_PARITY_FULL.md** - Complete feature matrix
- **TESTING_CHECKLIST.md** - Testing procedures
- **WIN32_DELIVERY_REPORT.md** - Project summary

---

## System Requirements

### Minimum
- Windows 10 (version 1809+) or Windows 11
- x64 processor
- 4 GB RAM
- 500 MB disk space

### Recommended
- Windows 11
- 8 GB+ RAM
- SSD storage
- 2560x1440+ display

---

## Support

**Issues:** Report via GitHub Issues  
**Documentation:** See `docs/` directory  
**License:** See LICENSE file

---

## Version History

### 1.0.0 (2025-12-18)
- Initial production release
- Complete Win32 native implementation
- All core features functional
- Full documentation suite

---

**Last Updated:** December 18, 2025  
**Package Version:** 1.0.0  
**Status:** Production Ready ✅
