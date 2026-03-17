# RawrXD SafeMode CLI

## 🚀 Full IDE Functionality via Command Line

SafeMode CLI provides complete IDE functionality without Qt/GUI dependencies. Use it when the GUI or IDE fails, or for autonomous/agentic operations via command line.

## ✨ Features

### Model Management (Ollama-style)
- **Poly Model Loading**: GGUF, Q4_0, Q8_0, F16, F32
- **Compression Tiers**: Automatic tier hopping (70B → 21B → 6B → 2B)
- **Real DEFLATE**: 60-75% compression ratio
- **KV Cache Compression**: 10x reduction

### Agentic Tools
- **File Operations**: read, write, append, delete, rename, copy, move, list
- **Git Integration**: status, add, commit, push, pull, diff, branch, checkout, stash
- **Shell Execution**: Run any command with output capture
- **Autonomous AI**: Tool-using agent with multi-step reasoning

### Inference
- **Generate**: Single-shot text generation
- **Stream**: Token-by-token streaming
- **Chat**: Interactive conversation with history
- **Agent**: Autonomous task completion with tool use

### System
- **API Server**: REST API compatible with Ollama
- **Telemetry**: CPU/GPU temp, power, VRAM monitoring
- **Governor**: Thermal management and overclocking
- **Settings**: Persistent configuration

## 📦 Installation

### Quick Start (PowerShell)
```powershell
# Install globally
.\scripts\Install-RawrSafeMode.ps1

# Or run directly
.\scripts\rawr.ps1 help
```

### Build Native Binary
```powershell
# Configure
cmake -B build -G "Visual Studio 17 2022"

# Build
cmake --build build --config Release --target RawrXD-SafeMode

# Run
.\build\bin-msvc\RawrXD-SafeMode.exe
```

## 🎮 Usage

### Interactive REPL
```bash
rawr              # Enter REPL mode
rawr repl         # Same as above
```

### Model Commands
```bash
# Ollama-style model loading
rawr run llama3.2
rawr run D:\models\codellama-7b.Q4_0.gguf

# Model management
rawr list         # List available models
rawr show         # Show current model info
rawr tier quality # Switch to quality tier
rawr tiers        # List all tiers
```

### Inference
```bash
# Generate text
rawr gen "Write a Python function to sort a list"
rawr stream "Explain quantum computing"

# Interactive chat
rawr chat

# Autonomous agent
rawr agent "Create a new Python project with tests"
```

### Agentic Tools
```bash
# List all tools
rawr tools

# File operations
rawr file read src/main.cpp
rawr file write test.txt "Hello World"
rawr file list ./src

# Git operations
rawr git status
rawr git add .
rawr git commit "feat: add new feature"
rawr git push

# Shell commands
rawr exec "dir /s *.cpp"
rawr exec "python -m pytest"
```

### System
```bash
# Status and monitoring
rawr status
rawr telemetry

# API server
rawr api start 11434
rawr api stop

# Overclock governor
rawr governor start
rawr governor stop

# Settings
rawr workspace D:\projects\myapp
rawr settings
rawr save
```

## 🔧 Command-Line Arguments

### Native Binary
```bash
RawrXD-SafeMode.exe [options] [command]

Options:
  -h, --help           Show help
  -w, --workspace DIR  Set workspace root
  -m, --model PATH     Load model on startup
  -a, --api [PORT]     Start API server (default: 11434)
  -g, --governor       Start overclock governor
  -v, --verbose        Enable verbose output
  --unsafe             Allow file ops outside workspace

One-shot commands:
  gen "prompt"         Generate and exit
  tool <name> <json>   Execute tool and exit
  exec <cmd>           Execute shell and exit
```

### PowerShell Module
```powershell
.\rawr.ps1 [command] [arguments] [-Model <path>] [-Workspace <dir>] [-Port <num>]

# Examples
.\rawr.ps1 run llama3.2
.\rawr.ps1 gen "Hello world" -Model codellama
.\rawr.ps1 agent "Fix the bug" -Workspace D:\project
```

## 🛠️ Tool Schema

All tools accept JSON parameters:

```json
{
  "file_read": {
    "params": { "path": "File path to read" },
    "required": ["path"]
  },
  "file_write": {
    "params": { "path": "File path", "content": "Text to write" },
    "required": ["path", "content"]
  },
  "git_commit": {
    "params": { "message": "Commit message", "sign_off": "true/false" },
    "required": ["message"]
  },
  "exec": {
    "params": { "command": "Shell command" },
    "required": ["command"]
  }
}
```

## 🔌 API Compatibility

SafeMode API is compatible with Ollama's REST API:

```bash
# Generate
curl http://localhost:11434/api/generate -d '{"model":"llama3.2","prompt":"Hello"}'

# Chat
curl http://localhost:11434/api/chat -d '{"model":"llama3.2","messages":[{"role":"user","content":"Hi"}]}'

# Models
curl http://localhost:11434/api/tags
```

## 🎯 Use Cases

### Emergency Recovery
When Qt/GUI crashes, use SafeMode to:
- Continue development work
- Commit and push changes
- Run builds and tests
- Generate code with AI

### Automation
Use in scripts and CI/CD:
```powershell
# Generate documentation
rawr gen "Write README for this project" > README.md

# Autonomous refactoring
rawr agent "Refactor all functions to use async/await"
```

### Headless Servers
Run on servers without display:
```bash
# Start API server for remote access
RawrXD-SafeMode.exe -a 11434

# Or in Docker
docker run -p 11434:11434 rawrxd/safemode
```

## 📊 Performance

- **Model Loading**: Streaming with progress, async support
- **Inference**: 70+ tokens/sec target
- **Tier Hopping**: <100ms transitions
- **Tool Execution**: <50ms per operation

## 🔒 Security

- **Workspace Sandboxing**: File ops restricted to workspace by default
- **Path Validation**: Prevents directory traversal
- **Configurable**: Use `--unsafe` to allow global access

## 📁 File Structure

```
scripts/
├── rawr.ps1                 # PowerShell CLI module
├── rawr-safemode.bat        # Windows launcher
└── Install-RawrSafeMode.ps1 # Global installer

src/
└── safemode_cli.cpp         # Native C++ CLI

build/bin-msvc/
└── RawrXD-SafeMode.exe      # Compiled binary
```

## 🤝 Integration

SafeMode integrates with:
- **Ollama**: Drop-in replacement for `ollama` command
- **VS Code**: Use as external terminal tool
- **CI/CD**: GitHub Actions, Azure Pipelines
- **Scripts**: PowerShell, Batch, Shell

---

**RawrXD SafeMode CLI** - Your backup when the IDE fails 🦁
