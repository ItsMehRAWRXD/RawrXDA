# 🚀 VS Code Local Agentic Copilot - Getting Started

## ✅ Installation Complete!

The **Local Agentic Copilot** extension has been installed and is ready to use in VS Code.

### Location
```
C:\Users\HiH8e\.vscode\extensions\local-agentic-copilot
```

---

## 🎯 Quick Start

### 1. **Reload VS Code**
Press `Ctrl+Shift+P` and run:
```
Developer: Reload Window
```

### 2. **Ensure Ollama is Running**
In a terminal or PowerShell:
```bash
ollama serve
```

### 3. **Enable Agentic Mode**
Press `Ctrl+Shift+A` to toggle agentic mode on/off

You should see:
- 🚀 **AGENTIC** (green) in the status bar = Active
- ⏸️ **STANDARD** (gray) = Inactive

---

## ⌨️ Keyboard Shortcuts

| Action | Shortcut | Function |
|--------|----------|----------|
| **Toggle Agentic Mode** | `Ctrl+Shift+A` | Enable/disable autonomous mode |
| **Generate Code** | `Ctrl+Shift+J` | Generate code from prompt |
| **Explain Code** | (Use Command Palette) | Get code explanation |
| **Fix Code** | (Use Command Palette) | Automatically fix code |

---

## 🎮 Using the Extension

### Method 1: Using Keyboard Shortcuts

**Toggle Agentic Mode:**
```
Press Ctrl+Shift+A
```

**Generate Code:**
```
1. Select code or place cursor where you want to generate
2. Press Ctrl+Shift+J
3. Enter your prompt in the input box
4. Wait for code to be generated and inserted
```

### Method 2: Using Command Palette

Press `Ctrl+Shift+P` and type:
- `Agentic Copilot: Toggle Agentic Mode`
- `Agentic Copilot: Generate Code`
- `Agentic Copilot: Explain Code`
- `Agentic Copilot: Fix Code Issues`
- `Agentic Copilot: Show Agentic Status`

### Method 3: Using Status Bar

Click the status indicator in the bottom right:
- Shows current mode (AGENTIC or STANDARD)
- Click to toggle between modes

---

## 📝 Usage Examples

### Example 1: Generate a Function

1. Create a new file: `test.py`
2. Press `Ctrl+Shift+J`
3. Enter: `Create a Python function to validate email addresses`
4. The function will be generated and inserted

### Example 2: Generate with Context

1. Write some code
2. Select it
3. Press `Ctrl+Shift+J`
4. Enter: `Extend this function to handle validation errors`
5. New code will be inserted after your selection

### Example 3: Explain Code

1. Select code you want to understand
2. Press `Ctrl+Shift+P`
3. Type `Agentic Copilot: Explain Code`
4. A side panel opens with explanation

### Example 4: Fix Code Issues

1. Select code with potential issues
2. Press `Ctrl+Shift+P`
3. Type `Agentic Copilot: Fix Code Issues`
4. Issues will be fixed and replaced

---

## ⚙️ Configuration

Edit VS Code settings (`Ctrl+,`):

### Search for "agentic" to configure:

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

### Configuration Options

| Setting | Default | Description |
|---------|---------|-------------|
| `ollamaEndpoint` | `http://localhost:11434` | Ollama API endpoint |
| `agenticModel` | `cheetah-stealth-agentic:latest` | Model for agentic mode |
| `standardModel` | `bigdaddyg-fast:latest` | Model for standard mode |
| `enableInlineCompletion` | `true` | Enable real-time suggestions |
| `completionDelay` | `500` ms | Delay before showing completions |
| `autoFormat` | `true` | Auto-format generated code |

---

## 🚀 Features

### ✅ Inline Code Completions
- Real-time suggestions as you type
- Context-aware recommendations
- Based on current file content
- Only when **AGENTIC** mode is enabled

### ✅ Code Generation
- Generate complete functions
- Create code from descriptions
- Maintains context from selection
- Production-ready output

### ✅ Code Explanation
- Opens side panel with explanation
- Detailed breakdown of code
- Best practices highlighted
- Easy to understand format

### ✅ Code Fixing
- Automatically detects issues
- Proposes fixes
- Explains what was changed
- Replace or accept

### ✅ Status Indicator
- See current mode at a glance
- Click to toggle
- Color-coded (green = agentic)

---

## 💡 Tips & Tricks

### Tip 1: Use Agentic Mode for Complex Code
```
Switch to Agentic Mode (Ctrl+Shift+A) when:
- Designing algorithms
- Creating complex functions
- Multi-step solutions
```

### Tip 2: Use Standard Mode for Quick Suggestions
```
Use Standard Mode when:
- Need quick completions
- Simple one-liners
- Performance is priority
```

### Tip 3: Provide Good Context
```python
# BAD: Too vague
# Generate a function

# GOOD: Clear description
# Generate a function to parse CSV files with proper error handling
```

### Tip 4: Use Selection for Context
```
Before generating:
1. Select relevant code
2. Press Ctrl+Shift+J
3. Model understands context better
```

### Tip 5: Check Agentic Status
```
Press Ctrl+Shift+P → "Show Agentic Status"
See which model is active and temperature settings
```

---

## 🛠️ Troubleshooting

### Issue: Extension Not Showing Up
**Solution:** Reload VS Code
```
Ctrl+Shift+P → Developer: Reload Window
```

### Issue: "Ollama is not running"
**Solution:** Start Ollama
```powershell
ollama serve
```

### Issue: "Model not found"
**Solution:** Pull the model
```bash
ollama pull cheetah-stealth-agentic:latest
```

### Issue: Completions Not Showing
**Checklist:**
- [ ] Agentic mode is ON (check status bar)
- [ ] Ollama is running
- [ ] `enableInlineCompletion` is true in settings
- [ ] You've typed at least 5 characters
- [ ] Wait ~500ms (configurable)

### Issue: Slow Responses
**Solutions:**
- Use faster model: `bigdaddyg-fast:latest`
- Increase timeout in settings
- Check CPU usage
- Enable GPU acceleration in Ollama

### Issue: Code Quality Issues
**Solutions:**
- Provide more context (select code before generating)
- Use more specific prompts
- Use Agentic mode for complex tasks
- Check generated code before accepting

---

## 📊 Modes Explained

### 🚀 Agentic Mode
- **Temperature:** 0.9 (creative)
- **Model:** `cheetah-stealth-agentic:latest`
- **Best For:** Complex reasoning, multi-step solutions
- **Speed:** Medium (more accurate)
- **Use When:** Designing algorithms, refactoring, testing

### ⏸️ Standard Mode
- **Temperature:** 0.7 (balanced)
- **Model:** `bigdaddyg-fast:latest`
- **Best For:** Quick suggestions, completions
- **Speed:** Fast (immediate feedback)
- **Use When:** Writing simple code, quick fixes

---

## 🎓 Learning Resources

- **Status Bar:** Shows current mode and model
- **Command Palette:** Explore available commands
- **Settings:** Customize behavior to your preference
- **Examples:** Try with simple code first

---

## 🔐 Privacy & Security

✅ **100% Local Processing**
- No data sent to cloud
- No GitHub API calls
- Ollama runs on your machine
- Complete privacy

---

## 📞 Support

If something isn't working:

1. Check Ollama is running: `ollama serve`
2. Check model exists: `ollama list`
3. Reload VS Code: `Ctrl+Shift+P → Developer: Reload Window`
4. Check console: `Ctrl+Shift+J` (Developer Tools)
5. Review settings: `Ctrl+,` → search "agentic"

---

## ✨ What's Next?

1. **Try toggling agentic mode** - `Ctrl+Shift+A`
2. **Generate some code** - `Ctrl+Shift+J`
3. **Check inline completions** - Start typing
4. **Explore commands** - `Ctrl+Shift+P` → type "agentic"
5. **Customize settings** - `Ctrl+,` → search "agentic"

---

**Enjoy your local AI coding assistant! 🤖**

No cloud. No subscriptions. Pure local autonomous AI.
