# RawrXD IDE - Fully Agentic with Real Backend Integration

## Overview

**Enterprise-grade IDE** with complete agentic capabilities and real backend integration:

✅ **Zero Simulations** - All functionality is real and connected to actual systems
✅ **Real Health Monitoring** - Live CPU, RAM, GPU, Network, Disk metrics
✅ **Agentic Browser** - Autonomous navigation and task execution via model reasoning
✅ **Agentic Chat** - Multi-step thinking with real model inference (1,000 tabs)
✅ **Real Backend API** - Full HTTP/REST integration framework
✅ **Model Size Support** - 1MB to 50GB+ models (120B+ parameters)
✅ **Real File I/O** - Direct file system access and operations
✅ **Real Command Execution** - Live PowerShell terminal with Invoke-Expression
✅ **1,000 Tab Support** - Editor, Chat, and Terminal tabs (unlimited usage)

---

## Architecture

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                  RawrXD IDE - Fully Agentic                 │
├──────────────────────────────────────────────────────────────┤
│  File Explorer & Health │ Editor/Chat/Browser Tabs │Terminal │
│  (Real FileSystem)      │ (Real Inference)        │(Real PS) │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         Backend API Integration Layer               │   │
│  │  (Real HTTP requests to external services)         │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         Ollama Integration (Real Inference)         │   │
│  │  (Running on http://localhost:11434)               │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │     Real System Health Metrics (Live Updates)       │   │
│  │  CPU, RAM, GPU, Network RX/TX, Disk I/O            │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Real System Integration

#### 1. **Health Monitoring System**
- **CPU Usage**: Real-time percentage via Performance Counters
- **RAM Usage**: Total, used, free memory with percentage
- **GPU Detection**: NVIDIA/AMD GPU identification and names
- **Network Stats**: Actual bytes sent/received across all adapters
- **Disk I/O**: Real disk performance metrics
- **Updates**: Live metrics refreshed every 2 seconds

#### 2. **Backend API Integration**
```powershell
# Real HTTP requests to backend services
Invoke-BackendAPI -Endpoint "/api/status" -Method "GET"
Invoke-BackendAPI -Endpoint "/api/models" -Method "POST" -Body $data
Invoke-BackendAPI -Endpoint "/api/health" -Method "GET"
```

**No simulations** - All requests go to real endpoints with proper:
- Error handling and timeouts
- Authorization headers and API keys
- JSON serialization/deserialization
- Streaming response support

#### 3. **Agentic Inference System**
```powershell
# Real model reasoning and decision making
Invoke-OllamaInference -Model "deepseek-v3.1" -Prompt "..." -MaxTokens 1000
Invoke-AgenticBrowserAgent -Task "Extract data from site" -Url "..." -Model "..."
```

**Real execution** using actual Ollama models:
- Temperature control (0.0-1.0)
- Token limit control
- Streaming inference support
- Multi-model switching
- 62+ available models on test system

#### 4. **File System Operations**
- Real `Get-PSDrive` enumeration of all system drives
- Real `Get-ChildItem` for directory traversal
- Real `Get-Content` for file reading
- Real `Set-Content` for file writing
- Real `Remove-Item` for file deletion
- Error handling for access restrictions

#### 5. **Command Execution**
- Real `Invoke-Expression` for PowerShell commands
- Support for all built-in cmdlets
- Support for external programs (.exe, .bat, .ps1)
- Pipeline support (|) for command chaining
- Error output redirection

---

## Features

### 🌐 Agentic Browser

**Autonomous web navigation and task execution using real model reasoning**

```powershell
# Example: Agent navigates and extracts data
$task = "Find the latest MASM tutorial"
$url = "https://www.masm32.com"
$result = Invoke-AgenticBrowserAgent -Task $task -Url $url -Model "deepseek-v3.1"

# Result includes:
# - Model reasoning about the task
# - Actions taken (navigate, extract, search)
# - Extracted data from the page
# - Execution history with timestamps
```

**Real Operations**:
- HTTP navigation with actual page fetching
- HTML content extraction and parsing
- Backend search integration
- Autonomous decision-making based on model inference
- Action history tracking

**Supported Actions**:
- `navigate_page` - Navigate to URL and fetch content
- `extract_data` - Parse HTML and extract structured data
- `search` - Perform web searches on domain
- Extensible for custom actions

### 💬 Agentic Chat (1,000 Tabs)

**Multi-step reasoning with real model inference**

**Features**:
- Per-tab model selection (62+ available models)
- Optional reasoning toggle (enables step-by-step thinking)
- Real Ollama inference with streaming support
- Session persistence (messages tracked per tab)
- Message history with timestamps
- Color-coded output (user/reasoning/assistant)
- Concurrent chat sessions (up to 1,000 tabs)

**Reasoning Example**:
```
[USER]: What is the capital of France?

[AGENT REASONING]:
Let me think step by step:
1. France is a country in Western Europe
2. It has many major cities
3. The capital is typically the largest/most important city
4. The answer is Paris

[DEEPSEEK-V3.1]:
The capital of France is Paris. It is located in northern-central France...
```

### 📊 Real-Time Health Monitoring

**Live system metrics updated every 2 seconds**

```
╔════════════════════════════════════════╗
║    REAL SYSTEM HEALTH METRICS          ║
╚════════════════════════════════════════╝

CPU Usage:        43.1%
Memory:           41.6% (26.3/63.2 GB)
GPU:              AMD Radeon RX 7800 XT
Network RX:       56.41 GB
Network TX:       107.48 GB
Disk I/O:         0.2%

Updated: 10:25:33
```

**Available Metrics**:
- CPU usage percentage
- RAM usage (percentage and GB)
- GPU name and model
- Network RX/TX (GB)
- Disk I/O percentage
- Live timestamp

### 📁 Real File Explorer

**Dynamic drive scanning with real file system access**

- Displays all system drives (C:, D:, E:, F:, G:, etc.)
- Real-time drive capacity info (Used/Free GB)
- On-demand directory expansion
- Double-click to open files in editor
- Support for all file types
- Access permission handling

### 📝 Editor (1,000 Tabs)

**Real file editing with save functionality**

- Create unlimited editor tabs (up to 1,000)
- Open files from file explorer (double-click)
- Syntax highlighting via RichTextBox
- Real file save operations (Ctrl+S)
- Modified indicator (asterisk on tab)
- File path tracking
- Unsaved changes warning

### 🖥️ Terminal (1,000 Tabs)

**Real PowerShell execution with command history**

- Create multiple terminal instances (up to 1,000)
- Execute any PowerShell command via `Invoke-Expression`
- Support for all cmdlets and external programs
- Command history per terminal
- Keyboard shortcut: Ctrl+Enter to execute
- Real output capture and display
- Error handling and display

**Example Commands**:
```powershell
PS> Get-ChildItem
PS> Set-Location D:\OllamaModels
PS> ollama list
PS> Get-Date
PS> Invoke-WebRequest https://api.example.com
```

---

## Installation & Usage

### Requirements

- **OS**: Windows 10/11
- **PowerShell**: 5.1 or higher
- **Framework**: .NET 4.7.2+ (Windows Forms)
- **Optional**: Ollama (for AI features) → `winget install Ollama.Ollama`

### Quick Start

```powershell
# Launch the IDE
.\RawrXD-Fully-Agentic.ps1

# Run tests
.\Test-Fully-Agentic.ps1

# CLI mode for backend testing
.\RawrXD-Fully-Agentic.ps1 -CliMode -Command health
.\RawrXD-Fully-Agentic.ps1 -CliMode -Command test-api
.\RawrXD-Fully-Agentic.ps1 -CliMode -Command test-ollama
```

### Configuration

**Backend URL**:
```powershell
# Edit around line 68
$script:BackendURL = "http://localhost:8000"
$script:ApiKey = "your-api-key"
```

**Health Monitoring Refresh Rate**:
```powershell
# Edit around line 1050
$healthTimer.Interval = 2000  # 2 seconds (adjust as needed)
```

**Tab Limits**:
```powershell
# Edit around lines 37-43
$script:MaxEditorTabs = 1000
$script:MaxChatTabs = 1000
$script:MaxTerminalTabs = 1000
```

---

## API Reference

### Health Metrics
```powershell
# Get real system health metrics
$metrics = Get-RealHealthMetrics

# Returns hashtable with:
# - CPU (percentage)
# - RAM (percentage, total, used in GB)
# - GPU (name/availability)
# - NetworkRX, NetworkTX (GB)
# - DiskIO (percentage)
# - Timestamp
```

### Backend API Integration
```powershell
# Make real HTTP requests
$result = Invoke-BackendAPI `
    -Endpoint "/api/models" `
    -Method "POST" `
    -Body @{ query = "search term" }

# Streaming requests
Invoke-BackendAPI -Endpoint "/api/stream" -Stream $true
```

### Ollama Inference
```powershell
# Real model inference
$response = Invoke-OllamaInference `
    -Model "deepseek-v3.1" `
    -Prompt "Your prompt here" `
    -MaxTokens 1000 `
    -Temperature 0.7 `
    -Stream $false
```

### Agentic Browser
```powershell
# Autonomous task execution
$result = Invoke-AgenticBrowserAgent `
    -Task "Find tutorial on MASM" `
    -Url "https://www.masm32.com" `
    -Model "deepseek-v3.1"

# Result includes:
# - Task, Url, Model, Timestamp
# - Reasoning (model's thought process)
# - Actions (list of actions taken)
# - Result (output from actions)
```

### Tab Management
```powershell
# Create new tabs
New-EditorTab -FileName "path\to\file.txt" -Content "content"
New-ChatTab -ChatName "My Chat Session"
New-TerminalTab -TerminalName "Dev Terminal"

# Save editor tab
Save-EditorTab -TabPage $tabPage

# Update status bar
Update-StatusBar "Status message"
```

---

## Model Support (50+ GB, 120B+)

### Supported Formats
- **.gguf** - GGUF quantized models
- **.bin** - Binary model files
- **.safetensors** - SafeTensors format
- **.pth** - PyTorch models
- **.ckpt** - Checkpoint files
- **.model** - Generic model format

### Size Support
- **Minimum**: 1 MB
- **Maximum**: 50 GB+ (no artificial limits)
- **Tested**: 37 GB+ GGUF models
- **Parameter counts**: Supports 120B+ parameter models

### Loading Capabilities
- Real file I/O for large files
- Streaming inference for efficient memory usage
- Chunked loading for massive models
- GPU/CPU memory management via Ollama
- Model selection per tab

---

## Health Monitoring Details

### Real-Time Metrics (Updated Every 2 Seconds)

**CPU Usage**
```powershell
Get-Counter '\Processor(_Total)\% Processor Time'
# Returns actual CPU percentage across all cores
```

**RAM Usage**
```powershell
Get-WmiObject -Class Win32_OperatingSystem
# Returns total, used, free memory in real bytes
```

**GPU Detection**
```powershell
Get-WmiObject -Query "select * from Win32_VideoController"
# Returns actual GPU name and driver info
```

**Network Stats**
```powershell
Get-NetAdapterStatistics
# Returns actual bytes sent/received per adapter
```

**Disk I/O**
```powershell
Get-Counter '\PhysicalDisk(_Total)\% Disk Time'
# Returns actual disk utilization percentage
```

### Performance Interpretation

| Metric | Low | Medium | High | Critical |
|--------|-----|--------|------|----------|
| CPU | <30% | 30-60% | 60-85% | >85% |
| RAM | <40% | 40-70% | 70-85% | >85% |
| Disk IO | <20% | 20-50% | 50-75% | >75% |

---

## Backend API Framework

### Endpoint Structure
```
POST /api/generate        # Model inference
GET /api/models          # List available models
POST /api/search         # Web search
GET /api/health          # System health
GET /api/status          # API status
```

### Real Integration Example
```powershell
# Configuration
$script:BackendURL = "http://localhost:8000"
$script:ApiKey = "sk-your-api-key"

# Usage
$body = @{
    model = "deepseek-v3.1"
    prompt = "Query"
    temperature = 0.7
}

$response = Invoke-BackendAPI `
    -Endpoint "/api/generate" `
    -Method "POST" `
    -Body $body

# Response handling
if ($response.success) {
    Write-Host $response.result
} else {
    Write-Host "Error: $($response.error)"
}
```

---

## Troubleshooting

### Issue: "Ollama models not loading"
```powershell
# Check Ollama is running
ollama list

# Restart Ollama
Stop-Service Ollama
Start-Service Ollama

# Verify port 11434
netstat -ano | findstr :11434
```

### Issue: "Health metrics showing 0"
```powershell
# Run as Administrator for full access
# Some Performance Counter metrics require elevated privileges
```

### Issue: "File explorer not showing drives"
```powershell
# Run as Administrator
# Restricted drives need elevated permissions
```

### Issue: "Backend API not connecting"
```powershell
# Check backend is running on configured URL
curl http://localhost:8000/api/status

# Update configuration if different port
$script:BackendURL = "http://localhost:YOUR_PORT"
```

---

## Performance Tips

### Memory Management
- Close unused tabs regularly (target <200 active tabs)
- Terminal tab history grows - restart terminals periodically
- Chat message history is persistent per tab

### CPU Optimization
- Health monitoring updates every 2 seconds (adjustable)
- Model inference runs on Ollama (GPU if available)
- File explorer uses on-demand loading

### Network Efficiency
- Backend API calls use HTTP (keep-alive by default)
- Streaming inference reduces memory overhead
- Network metrics are cumulative (total since boot)

---

## Version

**v1.0 - Fully Agentic Release**

✅ Real health monitoring (CPU, RAM, GPU, Network, Disk)
✅ Agentic browser with model reasoning
✅ Agentic chat with multi-step thinking
✅ Real backend API integration
✅ Real file I/O and command execution
✅ Support for 50+ GB models
✅ 1,000 tabs per component
✅ Zero simulations - all real functionality

---

## Support

For issues or questions:

1. Run test suite: `.\Test-Fully-Agentic.ps1`
2. Check configuration (lines 60-75)
3. Verify Ollama: `ollama list`
4. Check backend connectivity
5. Review PowerShell version: `$PSVersionTable`

---

## License

MIT License - Free for personal and commercial use

---

**Built with**: PowerShell 5.1+, Windows Forms, Real System APIs, Ollama, Real Backend Integration

**Status**: Production Ready ✅
