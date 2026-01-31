# 🤖 Local Agentic Copilot - Setup Guide

A GitHub Copilot-like experience using your local Ollama models with agentic reasoning capabilities.

## Features

✨ **Copilot-like Code Completion** - Inline suggestions as you type  
🚀 **Agentic Mode Toggle** - Switch between autonomous and standard modes  
💻 **Code Generation** - Generate code from natural language prompts  
📝 **Code Explanation** - Get detailed explanations of code  
🔧 **Code Fixing** - Automatically fix issues in your code  
🌐 **100% Local** - No cloud, all processing happens on your machine  
⚡ **No GitHub Copilot Extension** - Works independently  

## Prerequisites

- **Ollama** running locally (`http://localhost:11434`)
- **VS Code** 1.85+
- At least one model installed (agentic recommended for best results)

### Recommended Models

**For Agentic Mode (Autonomous Reasoning):**
- `cheetah-stealth-agentic:latest` ✅ Best for autonomous coding
- `bigdaddyg-agentic:latest`
- `bigdaddyg-fast-agentic:latest`

**For Standard Mode:**
- `bigdaddyg-fast:latest`
- `qwen2.5:7b`
- Any other local model

## Installation

### Option 1: Install from VS Code (Coming Soon)

The extension will be available in the VS Code Extension Marketplace.

### Option 2: Install Locally

1. **Clone/Copy the extension** to `~/.vscode/extensions/`:
   ```powershell
   # Windows
   xcopy "local-agentic-copilot" "$env:USERPROFILE\.vscode\extensions\local-agentic-copilot" /E /I
   
   # macOS/Linux
   cp -r local-agentic-copilot ~/.vscode/extensions/
   ```

2. **Build the extension:**
   ```bash
   cd local-agentic-copilot
   npm install
   npm run compile
   ```

3. **Reload VS Code**

## Usage

### Quick Start

1. **Toggle Agentic Mode:**
   ```
   Ctrl+Shift+A (Windows/Linux)
   Cmd+Shift+A (macOS)
   ```
   Or use command palette: `Agentic Copilot: Toggle Agentic Mode`

2. **See Status:**
   Look at the bottom right corner of VS Code. You'll see:
   - 🚀 **AGENTIC** (green) - Autonomous reasoning active
   - ⏸️ **STANDARD** (gray) - Standard mode active

### Commands

**Ctrl+Shift+J** - Generate Code  
Generate code from a prompt with selected context.

**Ctrl+Shift+E** - Explain Code  
Get detailed explanation of selected code.

**Ctrl+Shift+F** - Fix Code  
Automatically fix issues in code.

**Ctrl+Shift+A** - Toggle Agentic Mode  
Switch between autonomous and standard modes.

Or use Command Palette (`Ctrl+Shift+P`):
- `Agentic Copilot: Generate Code`
- `Agentic Copilot: Explain Code`
- `Agentic Copilot: Fix Code`
- `Agentic Copilot: Toggle Agentic Mode`
- `Agentic Copilot: Show Agentic Status`

### Inline Completions

When **Agentic Mode** is enabled, you get real-time code suggestions as you type:

1. Start typing code
2. Wait ~500ms (configurable)
3. See suggestions appear inline
4. Press Tab to accept

### Configuration

Edit VS Code settings (`settings.json`):

```json
{
  "agenticCopilot.ollamaEndpoint": "http://localhost:11434",
  "agenticCopilot.agenticModel": "cheetah-stealth-agentic:latest",
  "agenticCopilot.standardModel": "bigdaddyg-fast:latest",
  "agenticCopilot.enableInlineCompletion": true,
  "agenticCopilot.completionDelay": 500,
  "agenticCopilot.autoFormat": true
}
```

## Agentic Mode vs Standard Mode

### 🚀 Agentic Mode
- **Purpose:** Autonomous reasoning and complex problem solving
- **Temperature:** 0.9 (more creative)
- **Use when:** You need intelligent suggestions, multi-step solutions
- **Model:** Your configured agentic model
- **Features:**
  - Multi-step planning
  - Autonomous decision-making
  - Complex reasoning chains
  - Creative problem-solving

### ⏸️ Standard Mode
- **Purpose:** Direct, safe code suggestions
- **Temperature:** 0.7 (balanced)
- **Use when:** You need quick suggestions, standard completions
- **Model:** Your configured standard model
- **Features:**
  - Fast completions
  - Direct suggestions
  - Conservative approach

## PowerShell Agentic Mode Toggle

For command-line control, use the included PowerShell script:

```powershell
# Enable agentic mode
.\Agentic-Mode-Toggle.ps1 on

# Disable agentic mode
.\Agentic-Mode-Toggle.ps1 off

# Check status
.\Agentic-Mode-Toggle.ps1 status
```

Configuration is saved to: `$env:APPDATA\Ollama\agentic-config.json`

## Troubleshooting

### "Ollama is not running"
- Start Ollama: `ollama serve`
- Check endpoint: `http://localhost:11434/api/tags`

### "Model not found"
- Check available models: 
  ```bash
  ollama list
  ```
- Pull a model:
  ```bash
  ollama pull cheetah-stealth-agentic:latest
  ```

### Completions not showing
- Enable in settings: `"agenticCopilot.enableInlineCompletion": true`
- Make sure agentic mode is ON
- Check timeout setting if responses are slow
- Ensure Ollama is running

### Slow responses
- Use a faster model (e.g., `bigdaddyg-fast:latest`)
- Increase timeout in settings
- Check system resources (CPU/RAM)

## Performance Tips

1. **Use smaller models** for faster completions (3-7B parameters)
2. **Enable GPU acceleration** in Ollama for faster inference
3. **Adjust completion delay** in settings (higher = less frequent calls)
4. **Reduce context window** if responses are slow

## Architecture

```
┌─────────────────────────────────────────┐
│   VS Code Editor                        │
│  ┌──────────────────────────────────┐  │
│  │ Agentic Copilot Extension        │  │
│  ├─ Completion Provider              │  │
│  ├─ Command Handlers                 │  │
│  ├─ Status Bar                       │  │
│  └─ Webview Panels                   │  │
│  └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
              ↓ HTTP
┌─────────────────────────────────────────┐
│   Ollama API (localhost:11434)          │
│  ┌──────────────────────────────────┐  │
│  │ Model Inference Engine           │  │
│  ├─ Agentic Model                   │  │
│  ├─ Standard Model                  │  │
│  └─ (Any other Ollama model)        │  │
│  └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## Agentic Capabilities Tested

The extension was tested with the interactive agentic test suite. Results:

| Task | Score | Capabilities |
|------|-------|--------------|
| Code Analysis | 70/100 | ✓ Autonomous suggestions |
| Documentation | 85/100 | ✓ Multi-step reasoning |
| Cross-Project | 70/100 | ✓ Complex problem-solving |

## Keyboard Shortcuts

| Command | Windows/Linux | macOS |
|---------|--------------|-------|
| Toggle Agentic | `Ctrl+Shift+A` | `Cmd+Shift+A` |
| Generate Code | `Ctrl+Shift+J` | `Cmd+Shift+J` |
| Explain Code | N/A | N/A |
| Fix Code | N/A | N/A |

## Tips & Tricks

### 🎯 Use Agentic Mode for:
- Complex algorithms
- Architecture decisions
- Multi-file refactoring
- Learning/understanding code

### ⚡ Use Standard Mode for:
- Quick completions
- Simple functions
- Bug fixes
- Performance-critical tasks

### 💡 Pro Tips:
- Select code context before using commands for better results
- Use comments to explain what you want
- Start agentic suggestions with descriptive comments
- Check generated code before accepting

## Limitations

- Models must be running via Ollama
- Model quality depends on installed models
- Some complex tasks may require manual refinement
- Context window limited by model architecture

## Future Enhancements

🔄 Version 2.0 planned features:
- Multi-model support
- Custom prompt templates
- History/persistence
- Integration with source control
- Extended context from git
- Performance metrics dashboard

## Contributing

This is part of the **RawrXD** project. Contributions welcome!

## License

MIT

## Support

For issues:
1. Check troubleshooting section
2. Verify Ollama is running
3. Check VS Code console for errors (`Ctrl+Shift+J`)
4. Refer to GitHub issues

---

**Made with 🚀 using Local AI**  
Copilot-like experience without the cloud dependency!
