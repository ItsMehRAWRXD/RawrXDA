# 🚀 BigDaddyG Copilot-Style CLI

## ✨ GitHub Copilot... but in Your Terminal!

**Inline ghost-text suggestions while you type - just like VS Code Copilot, but in CLI!**

---

## 🎯 Features

### 💬 Inline Ghost Text
```bash
$ bigdaddyg> console.█
              ╰─ log()  ← Grey ghost text appears!
                        
# Press TAB to accept
$ bigdaddyg> console.log()█
```

### ⌨️ Keyboard Shortcuts
- **TAB** - Accept ghost text suggestion
- **ESC** - Toggle ghost text on/off
- **Ctrl-C** - Exit
- **Backspace** - Delete and update suggestions

### 🤖 AI-Powered Commands
- `suggest <prompt>` - Get AI code suggestion
- `explain <file>` - AI explains code
- `chat <text>` - Chat with BigDaddyG AI
- `ls` - List files
- `open <file>` - Read file
- `write <file> <content>` - Save file

---

## 📦 Installation

### From BigDaddyG IDE Repository

```bash
# Clone the repo
git clone https://github.com/ItsMehRAWRXD/BigDaddyG-IDE.git
cd BigDaddyG-IDE

# Make executable
chmod +x copilot-cli.js

# Run it!
./copilot-cli.js
```

### Standalone Installation

```bash
# Just download the file
curl -O https://raw.githubusercontent.com/ItsMehRAWRXD/BigDaddyG-IDE/main/copilot-cli.js

# Make executable
chmod +x copilot-cli.js

# Install dependencies
npm install node-fetch

# Run!
./copilot-cli.js
```

---

## ⚙️ Configuration

**Config file:** `~/.bigdaddyg_cli/config.json`

```json
{
  "model": "bigdaddyg:40gb",
  "ollamaUrl": "http://localhost:11434",
  "temperature": 0.4,
  "debounceMs": 250,
  "maxTokens": 100
}
```

**Options:**
- `model` - AI model to use (Ollama model name)
- `ollamaUrl` - Ollama server URL
- `temperature` - Creativity (0.0 = deterministic, 1.0 = creative)
- `debounceMs` - Delay before fetching suggestion (ms)
- `maxTokens` - Max suggestion length

---

## 🔌 AI Backend Options

### Option 1: Embedded Model (Offline)
Uses BigDaddyG's embedded 40GB model:
```bash
# Start embedded model engine
node engine/embedded-model-engine.js

# Run CLI
./copilot-cli.js
# Ghost text powered by local 40GB model!
```

### Option 2: Ollama (Local)
```bash
# Install Ollama
curl https://ollama.ai/install.sh | sh

# Pull a model
ollama pull codellama
# or: ollama pull bigdaddyg:40gb

# Run CLI
./copilot-cli.js
```

### Option 3: Cloud APIs
Modify `BigDaddyGAI.queryModel()` to use:
- OpenAI API
- Anthropic Claude
- Azure OpenAI
- Any other LLM API

---

## 📸 Demo

```bash
$ bigdaddyg> const rev█
              ╰─ erse = str => str.split('').reverse().join('');
              
# Press TAB
$ bigdaddyg> const reverse = str => str.split('').reverse().join('');█

# Press ENTER to execute
✅ Code suggestion accepted!

$ bigdaddyg> explain reverse.js█
🤖 AI Explanation:

This function reverses a string by:
1. Splitting into array of characters
2. Reversing the array
3. Joining back into string

Time complexity: O(n)
```

---

## 🎓 How It Works

### Architecture

```
User Types
    ↓
Debounce (250ms)
    ↓
Query AI Model
    ↓
Stream Response
    ↓
Display Grey Ghost Text
    ↓
User Presses TAB
    ↓
Accept & Insert
```

### Key Components

1. **Raw Terminal Mode**
   - Captures every keypress
   - No line buffering
   - Real-time ghost text

2. **StreamCompleter**
   - Debounces typing
   - Queries AI
   - Manages ghost text display

3. **BigDaddyGAI**
   - Connects to Ollama/embedded model
   - Fallback heuristics when offline
   - Smart completion logic

---

## 🔥 Advanced Usage

### Chaining Commands

```bash
bigdaddyg> suggest create a web server | write server.js
```

### Piping to Files

```bash
bigdaddyg> explain mycode.js > explanation.txt
```

### Configuration

```bash
bigdaddyg> config
⚙️  BigDaddyG CLI Configuration:
{
  "model": "bigdaddyg:40gb",
  "ollamaUrl": "http://localhost:11434",
  "temperature": 0.4
}
```

---

## 🆚 Comparison

| Feature | GitHub Copilot | Cursor | BigDaddyG CLI |
|---------|----------------|--------|---------------|
| **Inline Ghost Text** | ✅ | ✅ | ✅ |
| **Works in CLI** | ❌ | ❌ | ✅ |
| **Offline Support** | ❌ | ❌ | ✅ |
| **Free** | ❌ ($10/mo) | ❌ ($20/mo) | ✅ |
| **Custom Models** | ❌ | ⚠️ Limited | ✅ |
| **40GB Local Model** | ❌ | ❌ | ✅ |

---

## 🎯 Use Cases

### 1. **Quick Prototyping**
```bash
$ bigdaddyg> const api = axios.get█
              ╰─ ('https://api.example.com')
# TAB → Code inserted!
```

### 2. **Learning**
```bash
$ bigdaddyg> explain react-component.jsx
🤖 This component uses hooks for state management...
```

### 3. **Code Review**
```bash
$ bigdaddyg> chat review my authentication code
🤖 Looking at your code, I notice...
```

### 4. **Refactoring**
```bash
$ bigdaddyg> suggest refactor this to async/await
🤖 Here's the refactored version...
```

---

## 🛠️ Customization

### Add Custom Commands

Edit `copilot-cli.js`:

```javascript
commands.mycommand = async args => {
  // Your custom logic
  console.log('Custom command executed!');
};
```

### Change Ghost Text Color

```javascript
const grey = s => `\x1b[90m${s}\x1b[0m`;  // Grey
// or
const blue = s => `\x1b[34m${s}\x1b[0m`;  // Blue
```

### Adjust Debounce

```json
{
  "debounceMs": 100  // Faster suggestions (more API calls)
  // or
  "debounceMs": 500  // Slower (fewer API calls)
}
```

---

## 🚀 Integration with BigDaddyG IDE

### Run from IDE
```javascript
// In BigDaddyG IDE terminal:
./copilot-cli.js

// Ghost text uses same AI backend!
```

### Shared Configuration
Both BigDaddyG IDE and CLI share:
- Same AI models
- Same Ollama instance
- Same embedded model
- Unified experience!

---

## 🎓 Educational Value

**Learn from this code:**
- Terminal raw mode manipulation
- VT100 escape sequences
- Async AI integration
- Debouncing strategies
- Stream handling
- Event-driven architecture

**Perfect for:**
- Understanding how Copilot works
- Building CLI tools
- Learning terminal programming
- AI integration patterns

---

## 📈 Performance

### With Local Model (Ollama/Embedded):
- Latency: 50-300ms
- No internet required
- Privacy: 100%
- Cost: $0

### With Cloud API:
- Latency: 200-1000ms
- Internet required
- Privacy: Data sent to cloud
- Cost: API charges

---

## 🎉 Benefits

**Why use BigDaddyG CLI over web-based tools:**

1. **Speed** - No browser overhead
2. **Privacy** - Works offline with local model
3. **Integration** - Use in scripts, automation
4. **Simplicity** - Just a CLI, no GUI needed
5. **Power** - Full filesystem access
6. **Free** - No subscriptions

---

## 🔮 Future Enhancements

- [ ] Multi-line ghost text
- [ ] Context-aware suggestions (analyze current file)
- [ ] Git integration (suggest commit messages)
- [ ] Syntax highlighting in output
- [ ] History search (Ctrl-R)
- [ ] Auto-completion for file paths
- [ ] Plugin system

---

## 🎊 Summary

**BigDaddyG Copilot CLI:**
- ✅ GitHub Copilot experience in terminal
- ✅ Works with BigDaddyG 40GB model
- ✅ 100% offline capable
- ✅ Free and open source
- ✅ Extensible and customizable

**Try it now and experience AI-powered CLI coding!** 🚀

---

*Part of the BigDaddyG IDE ecosystem*  
*https://github.com/ItsMehRAWRXD/BigDaddyG-IDE*

