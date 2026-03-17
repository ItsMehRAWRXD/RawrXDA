# RawrXD Agentic IDE - Quick Start Guide

## 🚀 5-Minute Setup

### Step 1: Extract
Unzip `RawrXD-v1.0.0.zip` to your preferred location.

### Step 2: Add a Model
Download a GGUF model and place it in the `models/` folder:

**Recommended Models:**
- [Phi-3-mini-4k-instruct-q4](https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf) (2.3 GB) - Best balance
- [TinyLlama-1.1B-Chat](https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF) (638 MB) - Fastest

### Step 3: Launch
Double-click `Launch-RawrXD.bat` or run:
```powershell
.\Launch-RawrXD.ps1
```

### Step 4: Configure (Optional)
Edit `config/settings.json` to customize:
- GPU settings
- Default model
- Completion behavior

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Space` | Trigger completion |
| `Tab` | Accept suggestion |
| `Escape` | Dismiss suggestion |
| `Ctrl+Shift+A` | Open AI Agent |
| `Ctrl+Shift+R` | Smart Rewrite |
| `F5` | Run current file |

## 🎯 Features

### Inline Completions
Just type! AI suggestions appear automatically after 150ms.

### Agent Mode
Press `Ctrl+Shift+A` or use CLI:
```
.\bin\RawrXD-Agent.exe "Add logging to all functions"
```

### Smart Rewrite
Select code, press `Ctrl+Shift+R`, describe the change.

## 📊 Performance Tips

1. **Enable GPU**: Ensure Vulkan drivers are installed
2. **Model Selection**: Use TinyLlama for speed, Phi-3 for quality
3. **Context Window**: Reduce in settings if memory-constrained

## 🆘 Need Help?

- Check `docs/README.md` for full documentation
- Report issues: https://github.com/ItsMehRAWRXD/RawrXD/issues

---
**Happy Coding! 🎉**
