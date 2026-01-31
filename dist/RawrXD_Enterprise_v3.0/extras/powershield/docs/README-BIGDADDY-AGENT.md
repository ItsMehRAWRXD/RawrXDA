# BigDaddy-G Agentic Red-Team Assistant

**One-command security testing agent with autonomous function calling.**

---

## 🚀 Quick Start

### 1. **Verify Installation**

```powershell
# Check that agent tools exist
Test-Path .\agents\AgentTools.ps1  # → Should return True

# Load the module
Import-Module .\extensions\RawrXD-Marketplace.psm1 -Force

# Verify functions are available
Get-Command Start-BigDaddyAgent
```

### 2. **Launch Agent**

```powershell
Start-BigDaddyAgent
```

Expected output:
```
🤖 Initializing BigDaddy-G Agent...
✅ Loaded agent tools from C:\...\Powershield\agents\AgentTools.ps1
🚀 BigDaddy-G Agent Active - Press Ctrl+C to exit
═══════════════════════════════════════════════════════════════

>>> {{function:Invoke-WebScrape(http://example.com)}}
[AGENT] 🌐 Scraping: http://example.com
[AGENT] ✅ Status: 200 | Length: 1256 bytes
>>> Function returned: HTTP 200 | 1256 bytes | Preview: <!doctype html>...
```

---

## 🛠️ Available Functions

The agent automatically detects and executes functions using the pattern:
```
{{function:FunctionName(argument)}}
```

### Built-in Tools (Safe Stubs)

| Function | Purpose | Example |
|----------|---------|---------|
| `Invoke-WebScrape` | Fetch web content | `{{function:Invoke-WebScrape(https://example.com)}}` |
| `Invoke-RawrZPayload` | Simulate payload delivery | `{{function:Invoke-RawrZPayload(192.168.1.1)}}` |
| `Invoke-PortScan` | Simulate port scanning | `{{function:Invoke-PortScan(192.168.1.1)}}` |

---

## 📂 File Locations

```
Powershield/
├── extensions/
│   └── RawrXD-Marketplace.psm1    ← Module with Start-BigDaddyAgent
├── agents/
│   ├── AgentTools.ps1              ← Function implementations (SAFE STUBS)
│   └── BigDaddyG-Agent.ps1         ← Legacy standalone agent
└── README-BIGDADDY-AGENT.md        ← This file
```

**Alternative location** (if using external GGUF storage):
```
D:\BigDaddyG-40GB-Torrent\
├── AgentTools.ps1                  ← Same stub file
├── bg40-unleashed-final.gguf       ← Quantized model
└── llama-bin\                      ← llama.cpp binaries
```

---

## ⚙️ Configuration

### Add More Functions

Edit `agents\AgentTools.ps1` and add new functions:

```powershell
function Invoke-MyTool {
    param([string]$Target)
    # Your implementation here
    return "Result from MyTool: $Target"
}

# Add to export list
Export-ModuleMember -Function @(
    'Invoke-WebScrape',
    'Invoke-RawrZPayload',
    'Invoke-PortScan',
    'Invoke-MyTool'  # ← New function
)
```

The agent will automatically recognize it:
```
>>> {{function:Invoke-MyTool(test123)}}
```

### Change Model Prompt

Edit `extensions\RawrXD-Marketplace.psm1` → `Start-BigDaddyAgent` → `$history` array:

```powershell
$history = @(
    "You are BigDaddy-G, a helpful security-testing assistant.",
    "Available functions: Invoke-WebScrape, Invoke-RawrZPayload, Invoke-PortScan",
    "When you need web data, reply: {{function:Invoke-WebScrape(URL)}}",
    "User: your custom task here"
)
```

---

## 🎮 Optional: Keybinding

Add to your IDE's `keybindings.json`:

```json
{
  "key": "ctrl+shift+a",
  "command": "workbench.action.terminal.sendSequence",
  "args": {
    "text": "Import-Module .\\extensions\\RawrXD-Marketplace.psm1 -Force; Start-BigDaddyAgent\r"
  },
  "when": "terminalFocus"
}
```

Press **Ctrl+Shift+A** in any terminal → instant agent launch.

---

## 🔒 Security Notes

### Current Stubs (SAFE)
- ✅ `Invoke-WebScrape` only performs HTTP GET (read-only)
- ✅ `Invoke-RawrZPayload` **simulates** payload delivery (no actual execution)
- ✅ `Invoke-PortScan` returns **random mock results** (no real network traffic)

### Production Use
⚠️ **Replace stubs** with real tools only in authorized test environments:
- Implement proper authentication
- Add target whitelisting
- Log all actions for audit trails
- Follow responsible disclosure practices

---

## 🐛 Troubleshooting

### "AgentTools.ps1 not found"
```powershell
# Check search paths
ls .\agents\AgentTools.ps1
ls D:\BigDaddyG-40GB-Torrent\AgentTools.ps1

# Copy stub to workspace
Copy-Item D:\BigDaddyG-40GB-Torrent\AgentTools.ps1 .\agents\
```

### "ollama: command not found"
```powershell
# Install Ollama
winget install Ollama.Ollama

# Pull the bg40 model (or your chosen model)
ollama pull bg40
```

### Agent not detecting functions
Check that functions are exported:
```powershell
. .\agents\AgentTools.ps1
Get-Command Invoke-WebScrape -ErrorAction SilentlyContinue
```

---

## 📦 Shipping Checklist

✅ **Module**: `extensions\RawrXD-Marketplace.psm1` (contains `Start-BigDaddyAgent`)  
✅ **Tools**: `agents\AgentTools.ps1` (safe stubs for demo)  
✅ **Model**: `bg40-unleashed-final.gguf` (optional, user-provided)  
✅ **Docs**: This README

**First-time user experience:**
1. Clone repo
2. Run `Import-Module .\extensions\RawrXD-Marketplace.psm1`
3. Run `Start-BigDaddyAgent`
4. See working agent in <10 seconds

---

## 🚢 Next Steps

- **Silent Background Agent**: Add `-Headless` switch for log-only mode
- **HTTP C2**: POST results to remote server for distributed red-teaming
- **Encrypted Pipe**: Secure IPC between agent and control panel
- **Real Exploits**: Swap stubs with Metasploit/Cobalt Strike wrappers

---

**Agent unlocked — go break (your own) stuff.** 🦁
