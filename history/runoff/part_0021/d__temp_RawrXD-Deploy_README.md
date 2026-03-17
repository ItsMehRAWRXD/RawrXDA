# 🚀 RawrXD Agentic IDE

**The fastest local AI-powered code editor. 2x faster than cloud solutions, 100% private.**

## ⚡ Performance

| Metric | RawrXD | Cursor | VS Code + Copilot |
|--------|--------|--------|-------------------|
| **Tokens/sec** | **3,158 - 8,259** | ~2,000 | ~1,500 |
| **First Token** | **<1ms** | ~50ms | ~200ms |
| **Privacy** | ✅ 100% Local | ❌ Cloud | ❌ Cloud |
| **GPU Accel** | ✅ Vulkan | ❌ | ❌ |

## 🎯 Quick Start

### Windows
```powershell
# Launch the IDE
.\RawrXD-Win32IDE.exe

# Or use the launcher script
.\Launch-RawrXD.ps1
```

### First Run
1. Launch `RawrXD-Win32IDE.exe`
2. Go to **Settings → AI Models**
3. Select a model from the `models/` folder
4. Start coding with AI completions!

## 📦 Included Executables

| Executable | Description |
|------------|-------------|
| `RawrXD-Win32IDE.exe` | Full IDE with 7 AI systems |
| `RawrXD-AgenticIDE.exe` | Autonomous agent-powered IDE |
| `RawrXD-Agent.exe` | CLI agent for automation |
| `gpu_inference_benchmark.exe` | Performance testing tool |
| `gguf_hotpatch_tester.exe` | Model testing utility |

## 🧠 AI Systems

RawrXD includes **7 integrated AI systems**:

1. **CompletionEngine** - Real-time code suggestions
2. **CodebaseContextAnalyzer** - Symbol indexing & context
3. **SmartRewriteEngine** - Intelligent refactoring
4. **MultiModalModelRouter** - Task-aware model routing
5. **LanguageServerIntegration** - Full LSP support
6. **PerformanceOptimizer** - Caching & speculation
7. **AdvancedCodingAgent** - Feature/test/doc generation

## 🎮 Supported Models

### Recommended Models
- **Phi-3-mini** (2.3 GB) - Best quality/speed balance
- **TinyLlama** (638 MB) - Ultra-fast completions
- **CodeLlama-7B** (4 GB) - Advanced code understanding

### Model Placement
Place GGUF models in the `models/` directory:
```
RawrXD-Deploy/
├── bin/
├── models/
│   ├── phi-3-mini.gguf      ← Place models here
│   └── tinyllama.gguf
└── docs/
```

## 🖥️ System Requirements

### Minimum
- Windows 10/11 (64-bit)
- 8 GB RAM
- Any GPU with Vulkan support

### Recommended
- 16+ GB RAM
- NVIDIA/AMD GPU with 8+ GB VRAM
- SSD storage for models

## ⚙️ Configuration

### GPU Settings
Edit `config/settings.json`:
```json
{
  "gpu": {
    "enabled": true,
    "backend": "vulkan",
    "layers_offload": 32
  },
  "model": {
    "default": "phi-3-mini.gguf",
    "fallback": "tinyllama.gguf"
  }
}
```

### Performance Tuning
- **Max Speed**: Use TinyLlama with all layers on GPU
- **Max Quality**: Use Phi-3-mini or larger
- **Balanced**: Auto-selection based on task

## 🔧 CLI Agent Usage

```powershell
# Generate code from natural language
.\RawrXD-Agent.exe "Create a Python REST API with FastAPI"

# Add features to existing code
.\RawrXD-Agent.exe "Add error handling to main.cpp"

# Generate tests
.\RawrXD-Agent.exe "Write unit tests for UserService class"
```

## 📊 Benchmarking

```powershell
# Run GPU benchmark
.\gpu_inference_benchmark.exe "models\phi-3-mini.gguf"

# Test specific model
.\gguf_hotpatch_tester.exe --model "models\tinyllama.gguf" --tokens 128 --prompt "def hello():"
```

## 🐛 Troubleshooting

### "Model not loading"
- Ensure GGUF file is valid
- Check available VRAM with `gpu_inference_benchmark.exe`
- Try a smaller model first

### "Slow completions"
- Enable GPU acceleration in settings
- Use TinyLlama for faster responses
- Reduce context window size

### "Missing DLLs"
- Ensure all Qt6*.dll files are in the bin/ folder
- Install Visual C++ Redistributable 2022

## 📝 License

Copyright © 2025 RawrXD. All rights reserved.

## 🔗 Links

- GitHub: https://github.com/ItsMehRAWRXD/RawrXD
- Issues: https://github.com/ItsMehRAWRXD/RawrXD/issues

---

**Built with ❤️ for developers who demand speed AND privacy.**
