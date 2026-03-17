# BigDaddyG Extension - Quick Start Guide

## 🚀 Installation & Setup

### Prerequisites
- VS Code or Cursor IDE
- Ollama installed and running: `ollama serve` (default: http://localhost:11434)
- At least one model pulled: `ollama pull codellama`

### Quick Start
1. Press `Ctrl+L` to open BigDaddyG chat
2. Endpoint auto-filled: `http://localhost:11434`
3. Backend pre-selected: `Ollama Chat`
4. Click "🔄 Refresh" to load models
5. Start chatting!

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+L` | Open chat panel |
| `Enter` | Send message |
| `Shift+Enter` | New line in prompt |

---

## 🎯 Core Features

### 1. Chat with Local Models
```
Type: "Write a Python function to sort a list"
Press: Enter
Result: Instant response with code blocks
```

### 2. Code Block Actions
Every code block has 3 buttons:
- **📋 Copy**: Copy to clipboard
- **➕ Insert**: Insert at cursor position
- **✓ Apply**: Apply to workspace file

### 3. Workspace Context
Enable "📁 Workspace Context" to include:
- Active file and selection
- Open files list
- Current errors/warnings

### 4. Agent Modes
- **💬 Ask**: Standard Q&A
- **✏️ Edit**: Code editing (requires IDE Access)
- **📋 Plan**: Task planning

---

## 🎨 UI Guide

```
┌─────────────────────────────────────────┐
│ BigDaddyG Chat [Context: 45 files]     │ ← Header
├─────────────────────────────────────────┤
│ 👤 You: How do I fix this error?       │ ← User message
│ ┌─────────────────────────────────────┐ │
│ │ Selected code: [shows selection]    │ │
│ └─────────────────────────────────────┘ │
│                                         │
│ 🤖 BigDaddyG: Here's the fix:          │ ← AI response
│ ┌─────────────────────────────────────┐ │
│ │ ```cpp                              │ │
│ │ #include <windows.h>                │ │
│ │ ```                                 │ │
│ │ [📋 Copy] [➕ Insert] [✓ Apply]     │ │
│ └─────────────────────────────────────┘ │
├─────────────────────────────────────────┤
│ Endpoint: [http://localhost:11434]  [🔄]│ ← Config
│ Backend: [Ollama Chat ▼] Model: [...]  │
│ [💬 Ask ▼] [☑ Context] [☑ IDE Access] │
│ ┌─────────────────────────────────────┐ │
│ │ Type your message...                │ │ ← Input
│ └─────────────────────────────────────┘ │
│ [📤 Send] [🗑️ Clear]                   │
│ Status: Ready                           │
└─────────────────────────────────────────┘
```

---

## 🔧 Configuration

### Endpoints
| Service | Endpoint | Backend |
|---------|----------|---------|
| Ollama | `http://localhost:11434` | Ollama Chat |
| LM Studio | `http://localhost:1234` | OpenAI Chat |
| vLLM | `http://localhost:8000` | OpenAI Chat |
| Custom | `http://your-server:port` | (select type) |

### Models
Click "🔄 Refresh" to auto-load models, or type custom model ID manually.

---

## 💡 Usage Examples

### Example 1: Basic Code Generation
```
Prompt: "Create a REST API endpoint in Express.js"

Response:
```javascript
const express = require('express');
const app = express();

app.get('/api/users', (req, res) => {
  res.json({ users: [] });
});
```
[📋 Copy] [➕ Insert] [✓ Apply]
```

### Example 2: Fix Code with Context
1. Select code with error in editor
2. Enable "📁 Workspace Context"
3. Type: "Fix this error"
4. AI sees your selection + error diagnostics

### Example 3: Multi-File Editing (Agent Mode)
1. Set mode: "✏️ Edit"
2. Enable: "🔧 IDE Access"
3. Type: "Add error handling to all API routes"
4. AI generates AGENT_EDIT JSON
5. Extension auto-applies changes

### Example 4: File Navigation
When AI mentions files like `src/main.cpp`, they become clickable links:
- Click to open in editor
- Automatically detected: `.js`, `.ts`, `.cpp`, `.py`, etc.

---

## 🎓 Pro Tips

### Tip 1: Context Management
- Enable context only when needed (faster responses)
- Select specific code before asking for fixes
- Context badge shows: `Context: 45 files, 3 open`

### Tip 2: Multi-Turn Conversations
- History persists across sessions
- Click "🗑️ Clear" to reset context
- Scroll up to review previous answers

### Tip 3: Code Block Actions
- **Copy** → Paste anywhere
- **Insert** → Adds to current cursor position
- **Apply** → Writes to file (use in Edit mode)

### Tip 4: Custom Models
- Type model name instead of selecting
- Useful for variants: `codellama:7b-instruct`
- Works with quantized models: `deepseek-coder:6.7b-q4_K_M`

### Tip 5: Raw Prompts
- Check "⚡ Raw Prompt" for direct model access
- Uncheck to add system preface
- Use for advanced prompt engineering

---

## 🐛 Troubleshooting

### Issue: "Failed to load models"
**Solution**: Ensure Ollama is running
```bash
ollama serve
```

### Issue: "Connection refused"
**Solution**: Check endpoint URL
```bash
curl http://localhost:11434/api/tags
```

### Issue: "No response"
**Solution**: Verify model is pulled
```bash
ollama list
ollama pull codellama
```

### Issue: "Insert doesn't work"
**Solution**: Make sure you have a file open in editor and cursor is positioned

### Issue: "Context shows 0 files"
**Solution**: Open a workspace folder (File → Open Folder)

---

## 📊 Model Recommendations

| Task | Recommended Model | Size |
|------|------------------|------|
| Code completion | `codellama:7b` | 3.8GB |
| Code explanation | `deepseek-coder:6.7b` | 3.5GB |
| Multi-language | `qwen2.5-coder:7b` | 4.7GB |
| General chat | `bigdaddyg-ultra:latest` | Varies |
| Fast responses | `phi3:mini` | 2.3GB |

---

## 🔒 Privacy & Security

✅ **All data stays local**
- No cloud API calls
- No telemetry
- No usage tracking

✅ **Sandboxed execution**
- Webview isolation
- Path validation
- HTML escaping

✅ **Open source**
- Audit the code
- Modify as needed
- No proprietary restrictions

---

## 📈 Performance Tips

### Faster Responses
1. Use quantized models (Q4_K_M)
2. Reduce context (disable workspace context)
3. Use smaller models (7B vs 70B)

### Better Quality
1. Enable workspace context for relevant code
2. Use instruct-tuned models (`:instruct`)
3. Provide clear, specific prompts

### Resource Management
1. Run Ollama with `--cpu` on low-end systems
2. Limit open files in workspace
3. Close unused panels

---

## 🎯 Common Workflows

### Workflow 1: Debug Error
1. See error in Problems panel
2. Open BigDaddyG (`Ctrl+L`)
3. Enable "📁 Workspace Context"
4. Type: "Fix the error in [filename]"
5. Click "✓ Apply" on suggested fix

### Workflow 2: Add Feature
1. Set mode: "✏️ Edit"
2. Enable: "🔧 IDE Access"
3. Type: "Add authentication to the API"
4. Review AGENT_EDIT output
5. Changes auto-applied to workspace

### Workflow 3: Code Review
1. Select code block
2. Enable: "📁 Workspace Context"
3. Type: "Review this code for issues"
4. Get suggestions with reasoning
5. Copy improvements via "📋 Copy"

### Workflow 4: Learn Codebase
1. Type: "Explain the architecture of this project"
2. Context automatically includes file structure
3. Click file links in response to explore
4. Ask follow-up questions

---

## 🆘 Support

### Documentation
- Full feature list: `CURSOR-FEATURES-IMPLEMENTED.md`
- Architecture guide: `.github/copilot-instructions.md`
- API reference: Source code comments

### Community
- Report issues: GitHub Issues
- Feature requests: GitHub Discussions
- Contributions: Pull Requests

---

**Last Updated**: December 28, 2025  
**Version**: 1.0.0  
**Status**: Production Ready ✅
