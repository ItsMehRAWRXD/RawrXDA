# MyCopilot IDE - User Guide

**Version:** 1.0.0  
**Last Updated:** October 17, 2025  
**Platform:** Windows 10/11 (x64)

---

## 📖 Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Getting Started](#getting-started)
4. [Features](#features)
5. [Using the AI Agent](#using-the-ai-agent)
6. [Advanced Configuration](#advanced-configuration)
7. [Troubleshooting](#troubleshooting)
8. [FAQ](#faq)
9. [Keyboard Shortcuts](#keyboard-shortcuts)

---

## 🎯 Introduction

MyCopilot IDE is an AI-powered development environment that combines a modern code editor with an intelligent PowerShell-based AI agent. The UnifiedAgentProcessor provides multi-model AI support for code generation, cloud resource management, and task automation.

### Key Capabilities

- 🤖 **Multi-Model AI** - GitHub Copilot, Amazon Q, and local model support
- 💻 **Code Generation** - Intelligent code creation with context awareness
- ☁️ **Cloud Management** - Azure, AWS, and GCP resource automation
- ✅ **Task Automation** - Todo management and workflow optimization
- 🔄 **Real-time Processing** - Live IPC communication between UI and backend
- 📊 **Weighted Selection** - Automatic model selection based on success rates

---

## 📦 Installation

### Option 1: Installer (Recommended)

1. Download `MyCopilot-IDE-Setup-1.0.0.exe` from the build directory
2. Double-click the installer
3. Follow the installation wizard
4. Launch from Start Menu or Desktop shortcut

**Location:** `C:\Program Files\MyCopilot-IDE\MyCopilot-IDE.exe`

### Option 2: Portable Version

1. Download `MyCopilot-IDE-Portable-1.0.0.exe`
2. Copy to any location (USB drive, Desktop, etc.)
3. Double-click to run - no installation required

**Advantages:** 
- No admin rights needed
- Run from USB drives
- Multiple versions side-by-side

### Option 3: Unpacked Version (Developers)

1. Navigate to `build-20251013-142008\win-unpacked\`
2. Run `MyCopilot-IDE.exe` directly
3. Great for debugging and development

### System Requirements

- **OS:** Windows 10 version 1809 or later / Windows 11
- **RAM:** 4 GB minimum, 8 GB recommended
- **Disk:** 2 GB available space
- **PowerShell:** 5.1 or later (pre-installed on Windows)
- **Internet:** Required for cloud AI models (GitHub Copilot, Amazon Q)

---

## 🚀 Getting Started

### First Launch

1. **Launch the Application**
   ```powershell
   # Using the launcher script:
   & 'D:\MyCopilot-IDE\Launch-MyCopilot.ps1'
   
   # Or double-click the EXE:
   & 'D:\MyCopilot-IDE\build-20251013-142008\MyCopilot-IDE-Portable-1.0.0.exe'
   ```

2. **UI Overview**
   - **Left Panel:** Todo Tracker for task management
   - **Center:** Code Editor with syntax highlighting
   - **Right Panel:** AI Request interface
   - **Bottom:** Response output area

3. **Test the AI Agent**
   - Click the "AI Request" section
   - Enter: `generate code for fibonacci sequence`
   - Click "Submit" or press Enter
   - Watch the response appear in the output area

### Understanding the Interface

#### Todo Tracker
- Add tasks with the "Add Todo" button
- Mark complete by clicking the checkbox
- Delete with the trash icon
- Tasks persist across sessions

#### Code Editor
- Full syntax highlighting
- Auto-save functionality
- Copy/paste support
- Multi-line editing

#### AI Request Panel
- **Request Type Dropdown:** Select Code, Cloud, or Task
- **Input Field:** Enter your natural language request
- **Model Selector:** Choose AI model (auto-selected by default)
- **Submit Button:** Send request to backend

---

## ✨ Features

### 1. Code Generation

Generate code in any language using natural language descriptions.

**Example Requests:**
```
generate code for fibonacci in Python
create a REST API endpoint for user authentication
write a SQL query to find duplicate records
build a React component for user profile
```

**Supported Languages:**
- Python, JavaScript, TypeScript, PowerShell
- C#, Java, Go, Rust, C++
- SQL, HTML/CSS, and more

### 2. Cloud Resource Management

Manage cloud infrastructure across major providers.

**Azure Examples:**
```
list all resource groups in my subscription
create a new storage account in eastus
scale my app service to 3 instances
show cost analysis for the last month
```

**AWS Examples:**
```
list all EC2 instances in us-east-1
create an S3 bucket with versioning enabled
update Lambda function memory to 512MB
```

**GCP Examples:**
```
list all Cloud Run services
deploy container to GCP with autoscaling
show BigQuery dataset information
```

### 3. Task Automation (Agentic)

Automate complex multi-step workflows.

**Example Requests:**
```
todo review all code files for security issues
create a deployment plan for production
analyze project structure and suggest improvements
generate documentation from code comments
```

### 4. IDE Integration

Interact with the IDE and workspace.

**Example Requests:**
```
search all files for TODO comments
find all functions in the current file
refactor variable names for consistency
format code according to style guide
```

---

## 🤖 Using the AI Agent

### Model Selection

The UnifiedAgentProcessor supports multiple AI models:

1. **GitHub Copilot** (Default)
   - Endpoint: `https://api.github.com/copilot`
   - Best for: Code generation, general programming
   - Requires: GitHub Copilot subscription

2. **Amazon Q**
   - Endpoint: `https://q.aws.amazon.com/api`
   - Best for: AWS resources, cloud architecture
   - Requires: AWS account and credentials

3. **Local Model**
   - Endpoint: `http://localhost:11434/api`
   - Best for: Privacy, offline work
   - Requires: Local LLM server (Ollama, LM Studio, etc.)

### Automatic Model Selection

The agent uses **weighted selection** based on success rates:

- Each model tracks success/failure history
- Higher success rate = higher selection probability
- Weights adjust dynamically over time
- Manual override available in UI

### Request Processing Flow

```
User Input → SelectProcessor() → MatchKeywords() → 
InvokeModel() → ProcessResponse() → Display Result
```

1. **Input Analysis:** Request type and keywords identified
2. **Processor Selection:** Best processor chosen (Code/Cloud/Task)
3. **Model Invocation:** Weighted model selection and API call
4. **Response Handling:** Parse, validate, and format output
5. **Context Update:** Success rates and history updated

---

## ⚙️ Advanced Configuration

### API Key Management

#### GitHub Copilot
```powershell
# Set your GitHub token
$env:GITHUB_TOKEN = "ghp_your_token_here"

# Or use Secret Management
Install-Module Microsoft.PowerShell.SecretManagement
Set-Secret -Name "GitHubToken" -Secret "ghp_your_token_here"
```

#### Amazon Q
```powershell
# Set AWS credentials
$env:AWS_ACCESS_KEY_ID = "your_access_key"
$env:AWS_SECRET_ACCESS_KEY = "your_secret_key"

# Or use AWS CLI configuration
aws configure
```

#### Local Model
```powershell
# Start Ollama (example)
ollama serve

# Test endpoint
Invoke-RestMethod -Uri "http://localhost:11434/api/tags"
```

### Customizing Model Endpoints

Edit `UnifiedAgentProcessor-Fixed.psm1`:

```powershell
$this.Models = @(
    [ModelConfig]::new(
        "GitHub-Copilot", 
        "https://api.github.com/copilot",  # Change this
        $this.GetSecret("GitHubToken")
    ),
    [ModelConfig]::new(
        "Your-Custom-Model",
        "https://your-api.com/endpoint",
        $this.GetSecret("YourApiKey")
    )
)
```

### Processor Keywords

Customize keyword matching in processors:

```powershell
# CodeGenerator keywords
@("generate", "create", "write", "build", "code")

# CloudManager keywords  
@("deploy", "azure", "aws", "gcp", "cloud", "resource")

# TodoManager keywords
@("todo", "task", "review", "analyze", "plan")
```

### Logging and Debugging

Enable developer tools for debugging:

1. Press `Ctrl+Shift+I` to open DevTools
2. Check the **Console** tab for errors
3. View **Network** tab for API calls
4. Inspect **Application** tab for storage

**PowerShell Backend Logs:**
```powershell
# Check Setup-UnifiedAgent.ps1 output
# Logs appear in the Electron console
```

---

## 🔧 Troubleshooting

### Application Won't Start

**Issue:** EXE doesn't launch or crashes immediately

**Solutions:**
1. Check Windows Event Viewer for error details
2. Try running as Administrator
3. Ensure PowerShell 5.1+ is installed:
   ```powershell
   $PSVersionTable.PSVersion
   ```
4. Verify antivirus isn't blocking the application
5. Run the unpacked version for better error messages

### AI Requests Failing

**Issue:** "Error processing request" or no response

**Solutions:**
1. **Check API Keys:**
   ```powershell
   # Test GitHub token
   $headers = @{ Authorization = "Bearer $env:GITHUB_TOKEN" }
   Invoke-RestMethod -Uri "https://api.github.com/user" -Headers $headers
   ```

2. **Test Network Connectivity:**
   ```powershell
   Test-NetConnection -ComputerName api.github.com -Port 443
   ```

3. **Verify Model Endpoint:**
   - Open DevTools (Ctrl+Shift+I)
   - Check Console for API errors
   - Look for 401 (auth) or 404 (endpoint) errors

4. **Try Local Model:**
   - Switch to local model in UI
   - Ensure local server is running
   - Test with: `curl http://localhost:11434/api/tags`

### PowerShell Backend Not Responding

**Issue:** UI loads but requests timeout

**Solutions:**
1. **Check IPC Communication:**
   ```javascript
   // In DevTools Console
   window.electron.processRequest({ 
     type: 'code', 
     content: 'test' 
   }).then(console.log)
   ```

2. **Restart Application:**
   - Close completely (not just minimize)
   - Kill any hung processes:
     ```powershell
     Get-Process -Name "MyCopilot-IDE" | Stop-Process -Force
     Get-Process -Name "pwsh","powershell" | Where-Object { 
       $_.CommandLine -like "*UnifiedAgent*" 
     } | Stop-Process -Force
     ```

3. **Check PowerShell Execution Policy:**
   ```powershell
   Get-ExecutionPolicy
   # Should be at least RemoteSigned
   Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
   ```

### Performance Issues

**Issue:** Application runs slowly or freezes

**Solutions:**
1. **Close Unused Tabs:** Limit open editors
2. **Clear Cache:** Delete temp files
3. **Increase Memory:** Add more RAM if possible
4. **Disable DevTools:** Only open when debugging
5. **Check Background Processes:** Close other heavy apps

### Module Import Errors

**Issue:** "Cannot find type [UnifiedAgentProcessor]"

**Solutions:**
1. **Verify Module Exists:**
   ```powershell
   Test-Path "D:\MyCopilot-IDE\UnifiedAgentProcessor-Fixed.psm1"
   ```

2. **Check Module Content:**
   ```powershell
   Get-Content "D:\MyCopilot-IDE\UnifiedAgentProcessor-Fixed.psm1" -TotalCount 10
   ```

3. **Force Reload:**
   ```powershell
   Remove-Module UnifiedAgentProcessor* -ErrorAction SilentlyContinue
   Import-Module "D:\MyCopilot-IDE\UnifiedAgentProcessor-Fixed.psm1" -Force
   ```

---

## ❓ FAQ

### Q: Is internet required?

**A:** Yes for cloud AI models (GitHub Copilot, Amazon Q). Local models work offline but require a local LLM server.

### Q: Can I use my own API keys?

**A:** Yes! Set environment variables or use the Secret Management module. See [Advanced Configuration](#advanced-configuration).

### Q: What languages are supported?

**A:** The code generator supports all major programming languages. The UI is currently English-only.

### Q: How do I update the application?

**A:** Download the latest version and reinstall. Portable version can be replaced directly.

### Q: Is my code sent to external servers?

**A:** Only when using cloud AI models. Local model keeps everything on your machine.

### Q: Can I run multiple instances?

**A:** Yes, but each needs separate configuration. Use the portable version for multiple instances.

### Q: How do I uninstall?

**A:** 
- **Installer:** Use Windows "Add or Remove Programs"
- **Portable:** Simply delete the EXE file
- **Unpacked:** Delete the folder

### Q: Where are settings stored?

**A:** User preferences are stored in:
- Windows: `%APPDATA%\MyCopilot-IDE\`
- Portable: Same folder as EXE

### Q: Can I contribute code?

**A:** Yes! The source code is available in `D:\MyCopilot-IDE\`. See BUILD-COMPLETE-REPORT.md for build instructions.

---

## ⌨️ Keyboard Shortcuts

### General
- `Ctrl+Shift+I` - Open DevTools (debugging)
- `Ctrl+R` - Reload application
- `Ctrl+Q` - Quit application
- `F11` - Toggle fullscreen

### Editor
- `Ctrl+C` - Copy
- `Ctrl+V` - Paste
- `Ctrl+X` - Cut
- `Ctrl+Z` - Undo
- `Ctrl+Y` - Redo
- `Ctrl+A` - Select all

### AI Requests
- `Enter` - Submit request (in input field)
- `Shift+Enter` - New line (in input field)
- `Esc` - Cancel request

### Todo Management
- `Alt+N` - New todo
- `Alt+D` - Delete selected todo
- `Space` - Toggle todo completion (when focused)

---

## 📞 Support

### Getting Help

1. **Documentation:** Review this guide and BUILD-COMPLETE-REPORT.md
2. **Test Scripts:** Run `Test-Agent-Fixed.ps1` to verify functionality
3. **Logs:** Check DevTools Console for errors
4. **Issues:** Report bugs with detailed error messages

### Useful Commands

```powershell
# Test the agent
& 'D:\MyCopilot-IDE\Test-Agent-Fixed.ps1'

# Launch with logging
& 'D:\MyCopilot-IDE\Launch-MyCopilot.ps1' -Version Portable

# Check system info
Get-ComputerInfo | Select-Object WindowsVersion, OsArchitecture
$PSVersionTable
```

---

## 🔄 Updates and Roadmap

### Current Version: 1.0.0

**Included:**
- ✅ UnifiedAgentProcessor with 8 capabilities
- ✅ Multi-model AI support
- ✅ Modern IDE interface
- ✅ PowerShell IPC backend
- ✅ Portable and installer versions

**Planned Enhancements:**
- 🎨 Custom application icon
- 🔐 Code signing for production
- 🧪 End-to-end integration tests
- 🌐 Multi-language UI support
- 📊 Usage analytics and insights

---

## 📄 License and Credits

**MyCopilot IDE** - Copyright © 2025

**Built with:**
- Electron 27.3.11
- PowerShell 5.1+
- electron-builder 24.13.3
- 427 npm packages

**Third-party licenses:** See LICENSE.electron.txt and LICENSES.chromium.html in the application directory.

---

**Thank you for using MyCopilot IDE!** 🚀

For more information, see:
- [BUILD-COMPLETE-REPORT.md](../BUILD-COMPLETE-REPORT.md)
- [SESSION-SUMMARY.md](../SESSION-SUMMARY.md)
- [Launch-MyCopilot.ps1](../Launch-MyCopilot.ps1)
