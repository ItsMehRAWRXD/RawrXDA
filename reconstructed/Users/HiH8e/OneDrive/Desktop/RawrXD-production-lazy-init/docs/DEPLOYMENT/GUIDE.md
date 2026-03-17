# Win32 IDE Deployment Guide

**Version:** 1.0.0  
**Date:** December 18, 2025  
**Status:** Production Ready ✅

---

## Prerequisites

### System Requirements
- **OS:** Windows 10 (version 1809+) or Windows 11
- **CPU:** x64 processor (Intel/AMD)
- **RAM:** 4 GB minimum, 8 GB recommended
- **Disk:** 500 MB for installation, 2 GB for workspace
- **Display:** 1920x1080 minimum resolution

### Runtime Dependencies
- **Visual C++ Redistributable 2022** (x64)
  - Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
- **Node.js** (v18 or later)
  - Required for AI orchestration bridge
  - Download: https://nodejs.org/
- **Windows SDK** (for LSP servers)
  - Included in most Windows 10/11 installations

---

## Installation Steps

### 1. Binary Distribution
```powershell
# Extract ZIP to installation directory
Expand-Archive -Path RawrXD-Win32-IDE-v1.0.0.zip -DestinationPath "C:\Program Files\RawrXD"

# Add to PATH (optional, for command-line access)
$env:PATH += ";C:\Program Files\RawrXD"
[Environment]::SetEnvironmentVariable("PATH", $env:PATH, "Machine")
```

### 2. Configuration
Create configuration file at: `%APPDATA%\RawrXD\config.json`

```json
{
  "theme": "dark",
  "defaultShell": "pwsh",
  "autoSave": true,
  "autoSaveInterval": 30,
  "fontSize": 12,
  "fontFamily": "Consolas",
  "tabSize": 4,
  "wordWrap": false,
  "telemetryEnabled": false,
  "aiProvider": "openai",
  "logLevel": "info"
}
```

### 3. Environment Variables
Set the following environment variables:

```powershell
# Required for AI features
[Environment]::SetEnvironmentVariable("OPENAI_API_KEY", "your-api-key-here", "User")

# Optional: Custom Node.js path
[Environment]::SetEnvironmentVariable("RAWRXD_NODE_PATH", "C:\Program Files\nodejs\node.exe", "User")

# Optional: Custom orchestration bridge directory
[Environment]::SetEnvironmentVariable("RAWRXD_CURSOR_WIN32_DIR", "C:\RawrXD\cursor-ai-copilot-extension-win32", "User")

# Optional: Default shell
[Environment]::SetEnvironmentVariable("RAWRXD_SHELL", "pwsh", "User")
```

### 4. Orchestration Bridge Setup
```powershell
# Clone or extract cursor-ai-copilot-extension-win32
cd C:\RawrXD
git clone https://github.com/your-org/cursor-ai-copilot-extension-win32.git

# Install dependencies
cd cursor-ai-copilot-extension-win32
npm install

# Verify bridge
node orchestration/ide_bridge.js --version
```

### 5. LSP Server Setup (Optional)
Install language servers for enhanced code intelligence:

```powershell
# C/C++ (clangd)
choco install llvm

# Python (pyright)
npm install -g pyright

# TypeScript/JavaScript
npm install -g typescript typescript-language-server

# Rust (rust-analyzer)
rustup component add rust-analyzer
```

---

## First Launch

### 1. Start the IDE
```powershell
& "C:\Program Files\RawrXD\AgenticIDEWin.exe"
```

Or double-click `AgenticIDEWin.exe` from File Explorer.

### 2. Open Workspace
- File → Open Folder
- Select your project directory
- File tree will populate automatically

### 3. Verify Features
- **File Tree:** Expand folders, right-click for context menu
- **Editor:** Open a file, verify syntax highlighting
- **Terminal:** Click Terminal panel, type a command
- **AI Chat:** Click Chat panel, type `/cursor hello world`
- **Paint:** Click Paint panel, draw on canvas

---

## Configuration Details

### Log Files
Logs are written to: `%LOCALAPPDATA%\RawrXD\logs\`

**Categories:**
- `startup.log` - IDE initialization
- `gui.log` - UI events
- `paint.log` - Paint operations
- `code_editor.log` - Editor actions
- `chat.log` - AI chat interactions
- `agentic.log` - Tool executions
- `performance.log` - Metrics and benchmarks
- `errors.log` - Error traces
- `audit.log` - Compliance audit trail

### Session Persistence
Session state saved to: `%APPDATA%\RawrXD\session.json`

**Includes:**
- Open tabs and cursor positions
- Terminal working directories
- Chat history
- Window layout
- Recent workspaces

### Keyboard Shortcuts
| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open File |
| Ctrl+S | Save File |
| Ctrl+Shift+S | Save As |
| Ctrl+W | Close Tab |
| Ctrl+Tab | Next Tab |
| Ctrl+Shift+Tab | Previous Tab |
| Ctrl+N | New File |
| Ctrl+P | Command Palette |
| Ctrl+` | Toggle Terminal |
| F5 | Run/Debug |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| Ctrl+G | Go to Line |
| Ctrl+/ | Toggle Comment |

---

## Troubleshooting

### IDE Won't Start
1. Check Visual C++ Redistributable is installed
2. Verify Windows version compatibility
3. Check logs in `%LOCALAPPDATA%\RawrXD\logs\startup.log`
4. Run from command line to see console output

### AI Features Not Working
1. Verify `OPENAI_API_KEY` environment variable is set
2. Check Node.js is installed: `node --version`
3. Verify bridge directory exists and contains `ide_bridge.js`
4. Check network connectivity
5. Review `chat.log` for error messages

### Terminal Not Spawning
1. Verify default shell is installed (pwsh.exe or cmd.exe)
2. Check `RAWRXD_SHELL` environment variable
3. Run from admin command prompt
4. Review `terminal.log` for errors

### LSP Not Providing Hints
1. Verify language server is installed and in PATH
2. Check file extension is recognized
3. Open LSP server manually to test: `clangd --help`
4. Review `code_editor.log` for LSP communication errors

### File Tree Not Loading
1. Check workspace directory permissions
2. Verify directory is not on network share (slow)
3. Review `gui.log` for tree population errors
4. Try smaller directory first

---

## Uninstallation

### 1. Remove Files
```powershell
# Remove installation directory
Remove-Item -Path "C:\Program Files\RawrXD" -Recurse -Force

# Remove user data (optional, keeps settings/logs)
Remove-Item -Path "$env:APPDATA\RawrXD" -Recurse -Force
Remove-Item -Path "$env:LOCALAPPDATA\RawrXD" -Recurse -Force
```

### 2. Remove Environment Variables
```powershell
[Environment]::SetEnvironmentVariable("OPENAI_API_KEY", $null, "User")
[Environment]::SetEnvironmentVariable("RAWRXD_NODE_PATH", $null, "User")
[Environment]::SetEnvironmentVariable("RAWRXD_CURSOR_WIN32_DIR", $null, "User")
[Environment]::SetEnvironmentVariable("RAWRXD_SHELL", $null, "User")
```

### 3. Remove PATH Entry
```powershell
$path = [Environment]::GetEnvironmentVariable("PATH", "Machine")
$newPath = ($path.Split(';') | Where-Object { $_ -notlike "*RawrXD*" }) -join ';'
[Environment]::SetEnvironmentVariable("PATH", $newPath, "Machine")
```

---

## Enterprise Deployment

### Silent Installation
```powershell
# Extract to standard location
Expand-Archive -Path RawrXD-Win32-IDE.zip -DestinationPath "C:\Program Files\RawrXD" -Force

# Deploy config via GPO or script
Copy-Item -Path "\\domain\share\RawrXD\config.json" -Destination "$env:APPDATA\RawrXD\" -Force

# Set environment variables via GPO
```

### Multi-Tenant Configuration
Edit `config.json` to include tenant settings:

```json
{
  "tenantId": "org-12345",
  "tenantName": "ACME Corp",
  "ssoEnabled": true,
  "auditTrailEnabled": true,
  "telemetryEndpoint": "https://metrics.acme.com/rawrxd",
  "rateLimits": {
    "requestsPerMinute": 60,
    "tokensPerDay": 100000
  }
}
```

### Centralized Logging
Configure log forwarding:

```json
{
  "logging": {
    "remote": true,
    "syslogServer": "logs.acme.com",
    "syslogPort": 514,
    "facility": "local7"
  }
}
```

---

## Update Procedure

### 1. Backup Current Installation
```powershell
# Backup binaries
Copy-Item -Path "C:\Program Files\RawrXD" -Destination "C:\Backup\RawrXD-$(Get-Date -Format 'yyyyMMdd')" -Recurse

# Backup user data
Copy-Item -Path "$env:APPDATA\RawrXD" -Destination "C:\Backup\RawrXD-UserData-$(Get-Date -Format 'yyyyMMdd')" -Recurse
```

### 2. Stop IDE
Close all instances of AgenticIDEWin.exe

### 3. Install Update
```powershell
# Extract new version over existing
Expand-Archive -Path RawrXD-Win32-IDE-v1.1.0.zip -DestinationPath "C:\Program Files\RawrXD" -Force
```

### 4. Verify Update
```powershell
# Check version
& "C:\Program Files\RawrXD\AgenticIDEWin.exe" --version
```

---

## Performance Tuning

### Memory Optimization
Adjust `config.json`:

```json
{
  "performance": {
    "maxOpenTabs": 20,
    "terminalScrollbackLines": 5000,
    "cacheSizeMB": 256,
    "preloadFiles": false
  }
}
```

### Disk I/O Optimization
Enable SSD-specific settings:

```json
{
  "performance": {
    "fsCache": true,
    "fsCacheSizeMB": 512,
    "asyncFileWrites": true
  }
}
```

### Network Optimization
For remote AI services:

```json
{
  "network": {
    "connectionPoolSize": 10,
    "requestTimeoutMs": 30000,
    "retryAttempts": 3,
    "retryDelayMs": 1000
  }
}
```

---

## Security Considerations

### API Key Storage
- Store `OPENAI_API_KEY` in environment variables (User scope)
- Do NOT hardcode in config files
- Use DPAPI for encrypted storage (enterprise)

### Audit Trail
Enable comprehensive logging:

```json
{
  "security": {
    "auditEnabled": true,
    "auditLevel": "detailed",
    "auditRotationDays": 90,
    "auditEncryption": true
  }
}
```

### Network Security
Configure firewall rules:

```powershell
# Allow outbound HTTPS (for AI APIs)
New-NetFirewallRule -DisplayName "RawrXD HTTPS" -Direction Outbound -Program "C:\Program Files\RawrXD\AgenticIDEWin.exe" -Action Allow -Protocol TCP -RemotePort 443

# Allow Node.js orchestration bridge
New-NetFirewallRule -DisplayName "RawrXD Node Bridge" -Direction Outbound -Program "C:\Program Files\nodejs\node.exe" -Action Allow -Protocol TCP -RemotePort 443
```

---

## Support & Resources

**Documentation:** https://docs.rawrxd.com  
**Issues:** https://github.com/your-org/RawrXD/issues  
**Community:** https://discord.gg/rawrxd  
**Commercial Support:** support@rawrxd.com  

---

**Last Updated:** December 18, 2025  
**Document Version:** 1.0  
**Status:** Production Ready ✅
