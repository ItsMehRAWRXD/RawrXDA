# RawrXD Complete Deployment Guide

**Date**: January 7, 2026  
**Build Status**: Full Integration Complete  
**Platform**: Windows 64-bit (x64)  
**Compiler**: MSVC 2022 with Qt 6.7.3  

---

## 📦 Package Contents

### 1. **Qt C++ IDE (`bin/Executables/RawrXD-AgenticIDE.exe`)**
- **Features**:
  - Multi-tab editor with real-time code completion
  - AI-powered ghost text suggestions (Cursor-style inline autocomplete)
  - LSP (Language Server Protocol) integration with Clang
  - Integrated terminal (PowerShell, Cmd, Git Bash)
  - Chat panel for AI assistance
  - File explorer and project management
  - Real-time linting and diagnostics
  - Todo tracking
  - Model selection and inference settings
  - Production-ready architecture

- **Requirements**:
  - Windows 10/11 x64
  - Qt 6.7.3 runtime (bundled)
  - GPU optional (NVIDIA CUDA, AMD HIP, Vulkan)
  - ~500MB free disk space
  - 8GB+ RAM recommended
  - Ollama/llama.cpp running locally for AI completions (recommended)

- **Keyboard Shortcuts**:
  - `Ctrl+N` - New file
  - `Ctrl+O` - Open file
  - `Ctrl+S` - Save file
  - `Ctrl+/` - Comment/uncomment
  - `Ctrl+F` - Find
  - `Tab` - Accept ghost text completion
  - `Esc` - Dismiss ghost text
  - `Ctrl+Alt+T` - New terminal

### 2. **Autonomous Agent (`bin/Executables/RawrXD-Agent.exe`)**
- Autonomous coding agent for refactoring, debugging, and code generation
- Command-line interface
- Integration with git, file operations, and build systems

### 3. **PowerShell Edition (`PowerShell/`)**
- 131+ PowerShell scripts for AI automation
- RawrXD.ps1 - Main IDE in PowerShell
- Advanced features: model loading, inference, completions, chatting
- Integration with Ollama/llama.cpp
- Real-time streaming responses
- Agent command execution

### 4. **Tools & Utilities**
- Model benchmarking tools
- GGUF loader and converter
- Tokenizer utilities
- GPU backend selector
- Vulkan shader compilation tools

### 5. **Documentation**
- Architecture guides
- Implementation roadmaps
- API documentation
- Quick start guides

---

## 🚀 Quick Start

### **Option 1: Qt IDE (Recommended)**
```powershell
cd "C:\Users\$env:USERNAME\OneDrive\Desktop\RawrXD-IDE\bin\Executables"
.\RawrXD-AgenticIDE.exe
```

**First Launch**:
1. Select a language (C++, Python, JavaScript, etc.)
2. Choose your model from dropdown (requires Ollama running)
3. Start typing to see AI completions

### **Option 2: PowerShell Edition**
```powershell
cd "C:\Users\$env:USERNAME\OneDrive\Desktop\RawrXD-IDE\PowerShell"
& '.\RawrXD.ps1'
```

### **Option 3: Standalone Agent**
```powershell
cd "C:\Users\$env:USERNAME\OneDrive\Desktop\RawrXD-IDE\bin\Executables"
.\RawrXD-Agent.exe --help
```

---

## 🤖 Setting Up AI Completions

### **Prerequisites**:
- Ollama installed (https://ollama.ai)
- Model downloaded (e.g., `mistral`, `neural-chat`, `llama2`)

### **Steps**:

1. **Start Ollama**:
   ```powershell
   ollama serve
   ```
   (Leave this running in a separate terminal)

2. **Launch RawrXD**:
   ```powershell
   .\RawrXD-AgenticIDE.exe
   ```

3. **Configure Model**:
   - Settings → Inference Settings
   - Model: `mistral` (or your preferred model)
   - Endpoint: `http://localhost:11434` (default)
   - Timeout: 5000ms (adjustable)

4. **Test**:
   - Open a file or create new
   - Start typing (e.g., `for (int i`)
   - Wait 300ms for ghost text suggestion
   - Press `Tab` to accept

---

## 📋 File Structure

```
C:\Users\{UserName}\OneDrive\Desktop\RawrXD-IDE\
├── bin/
│   ├── Executables/
│   │   ├── RawrXD-AgenticIDE.exe (main Qt IDE)
│   │   ├── RawrXD-Agent.exe (autonomous agent)
│   │   └── *.exe (utilities)
│   └── Libraries/
│       ├── Qt6*.dll (Qt runtime)
│       ├── ggml.dll (ML inference)
│       └── *.dll (dependencies)
├── PowerShell/
│   ├── RawrXD.ps1 (main PS IDE)
│   ├── RawrXD-*.ps1 (feature scripts)
│   └── 126+ utility scripts
├── Qt-IDE/
│   └── Configuration files & plugins
├── Tools/
│   ├── model-benchmark
│   ├── gguf-converter
│   └── tokenizer-utils
├── Documentation/
│   ├── README.md
│   ├── ARCHITECTURE.md
│   ├── API_GUIDE.md
│   └── *.md
└── DEPLOYMENT_GUIDE.md (this file)
```

---

## ⚙️ Configuration Files

### **IDE Settings** (`Qt-IDE/settings.json`):
```json
{
  "theme": "dark",
  "fontSize": 12,
  "modelEndpoint": "http://localhost:11434",
  "modelName": "mistral",
  "completionTimeout": 5000,
  "autoSave": true,
  "lspEnabled": true
}
```

### **PowerShell Config** (`PowerShell/config.json`):
```json
{
  "ollama_endpoint": "http://localhost:11434",
  "default_model": "mistral",
  "streaming": true,
  "temperature": 0.7
}
```

---

## 🎯 Features Breakdown

### **Qt IDE (RawrXD-AgenticIDE)**
- ✅ Multi-tab editor
- ✅ AI code completion (Cursor-style ghost text)
- ✅ LSP integration (Clang-based diagnostics)
- ✅ Integrated terminal
- ✅ Chat panel
- ✅ File explorer
- ✅ Project management
- ✅ Todo tracking
- ✅ GPU acceleration (optional)
- ✅ Vulkan support
- ✅ Code refactoring suggestions
- ✅ Real-time linting
- ✅ Debugger support (DAP protocol)

### **PowerShell Edition**
- ✅ Full IDE experience in PowerShell
- ✅ Streaming AI responses
- ✅ Agent command execution
- ✅ File operations
- ✅ Git integration
- ✅ Build system integration
- ✅ Real-time model switching

### **Autonomous Agent**
- ✅ Code analysis
- ✅ Automated refactoring
- ✅ Bug detection and fixing
- ✅ Code generation
- ✅ Test generation
- ✅ Documentation generation

---

## 🔧 Troubleshooting

### **IDE Won't Start**
```powershell
# Check Qt dependencies
Get-ChildItem "C:\Program Files\Qt\6.7.3\bin\Qt6*.dll"

# Rebuild without Vulkan
# Edit CMakeLists.txt: set(ENABLE_VULKAN OFF)
```

### **No AI Completions**
```powershell
# Verify Ollama is running
curl http://localhost:11434/api/tags

# Check IDE settings
# Settings → Inference Settings → Verify endpoint and model
```

### **Performance Issues**
- Reduce max suggestions (Settings → 3 instead of 5)
- Increase timeout (Settings → 7000ms)
- Disable GPU acceleration if unstable
- Reduce debounce delay to 500ms

### **Build Errors**
```powershell
# Clean rebuild
cd "D:\RawrXD-production-lazy-init\build"
cmake --build . --config Release --clean-first
cmake --build . --config Release --target RawrXD-AgenticIDE
```

---

## 📊 System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Windows 10 x64 | Windows 11 x64 |
| CPU | Intel i5 / AMD Ryzen 5 | Intel i7 / AMD Ryzen 7 |
| RAM | 8 GB | 16 GB |
| GPU | None (CPU mode) | NVIDIA RTX 3060+ / AMD RX 6700 |
| Disk | 500 MB | 2 GB |
| Network | N/A (local Ollama) | 1 Mbps (cloud models) |

---

## 🔐 Security & Privacy

- ✅ All inference runs locally (no data sent to cloud)
- ✅ Open source architecture
- ✅ No telemetry by default
- ✅ Encrypted configuration storage
- ✅ Secure token validation for cloud APIs

---

## 📞 Support & Resources

- **GitHub**: https://github.com/ItsMehRAWRXD/RawrXD
- **Documentation**: See `Documentation/` folder
- **Issues**: Report via GitHub Issues
- **Discord**: https://discord.gg/rawrxd (placeholder)

---

## 🎉 What's Next?

1. **Install Ollama**: https://ollama.ai
2. **Download Model**: `ollama pull mistral`
3. **Launch IDE**: `RawrXD-AgenticIDE.exe`
4. **Start Coding**: Type and let AI assist!

---

**Built with ❤️ by RawrXD Team**  
*Making AI-powered development accessible to everyone*

Version: 1.0.0  
Last Updated: January 7, 2026
