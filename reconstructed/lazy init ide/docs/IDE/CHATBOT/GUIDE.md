# RawrXD IDE Chatbot - Quick Reference

## 🚀 Quick Start

### CLI Mode (Interactive)
```powershell
cd "D:\lazy init ide\scripts"
.\ide_chatbot.ps1 -Mode interactive
```

### GUI Mode
1. Start API server:
```powershell
.\ide_chatbot.ps1 -Mode api -Port 8080
```

2. Open GUI in browser:
```
D:\lazy init ide\gui\ide_chatbot.html
```

### Single Question Mode
```powershell
.\ide_chatbot.ps1 -Question "How do I send a swarm to a directory?" -Mode single-question
```

## 💬 Sample Questions

### Swarm Operations
- "How do I send a swarm to a certain directory?"
- "How do I monitor swarm activity?"
- "How do I stop a swarm?"
- "How do I deploy agents to multiple directories?"

### Todo Management
- "How do I add a todo?"
- "How do I parse a !todos command?"
- "How do I create todos agentically?"
- "How do I view my todos?"
- "How do I complete a todo?"
- "What's the todo limit?"

### Model Operations
- "How do I create a 7B model?"
- "How do I train a model?"
- "How do I quantize a model?"
- "What model sizes are available?"
- "How do I use custom prompts?"

### Benchmarking
- "How do I benchmark model formats?"
- "How do I compare cloud vs local performance?"
- "How do I reverse engineer a cloud model?"
- "What formats can I benchmark?"

### Advanced Operations
- "What is virtual quantization?"
- "How does intelligent pruning work?"
- "How do I freeze model states?"
- "How do I switch between quantization levels?"

### System Info
- "Where are the important files?"
- "What scripts are available?"
- "Where is the todo data stored?"
- "Where are the model files?"

## 🎯 Features

### CLI Features
- ✅ Interactive conversation mode
- ✅ Natural language question processing
- ✅ Keyword-based intelligent search
- ✅ Conversation history tracking
- ✅ Code syntax highlighting in terminal
- ✅ Quick help commands

### GUI Features
- 🎨 Beautiful modern interface
- 🚀 Quick action buttons
- 💬 Real-time chat interface
- 📋 Code block copy functionality
- ⌨️ Keyboard shortcuts (Enter to send)
- 🎭 Typing indicator animation
- 📱 Responsive design
- 🔄 Status indicator

### API Features
- 🌐 REST API server
- 📡 JSON request/response
- 🔌 CORS enabled
- ⚡ Health check endpoint
- 🔄 Real-time processing

## 🛠️ API Usage

### Start API Server
```powershell
.\ide_chatbot.ps1 -Mode api -Port 8080
```

### API Endpoints

**POST /ask**
```json
Request:
{
  "question": "How do I send a swarm to a directory?"
}

Response:
{
  "question": "How do I send a swarm to a directory?",
  "answer": "To send a swarm to a specific directory:\n\n...",
  "timestamp": "2026-01-25T15:30:00Z"
}
```

**GET /health**
```json
Response:
{
  "status": "ok",
  "uptime": 123.45
}
```

### cURL Example
```bash
curl -X POST http://localhost:8080/ask \
  -H "Content-Type: application/json" \
  -d '{"question":"How do I add a todo?"}'
```

### PowerShell Example
```powershell
$body = @{ question = "How do I create a model?" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/ask" -Method Post -Body $body -ContentType "application/json"
```

## 🎨 GUI Customization

Edit `D:\lazy init ide\gui\ide_chatbot.html`:

### Change Theme Colors
```css
/* Update gradient colors */
background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
```

### Change API Endpoint
```javascript
const API_ENDPOINT = 'http://localhost:8080/ask';
```

### Add Custom Quick Actions
```html
<button class="quick-btn" onclick="askQuestion('Your question here')">
  🔥 Custom Action
</button>
```

## 📚 Knowledge Base Categories

The chatbot knows about:

1. **Swarm Operations** (keywords: swarm, agent, directory, deploy)
2. **Todo Management** (keywords: todo, task, list, !todos, agentic)
3. **Model Operations** (keywords: model, create, train, quantize)
4. **Benchmarking** (keywords: benchmark, test, performance, cloud)
5. **Advanced Operations** (keywords: prune, freeze, virtual, quantization)
6. **File System** (keywords: file, directory, path, location)

## 🔧 Extending the Chatbot

### Add New Question/Answer

Edit `ide_chatbot.ps1` and add to `$script:KnowledgeBase`:

```powershell
"new_category" = @{
    Keywords = @("keyword1", "keyword2")
    Answers = @{
        "answer_key" = @{
            Question = "Your question?"
            Answer = @"
Your detailed answer here.

Can include:
- Multiple lines
- Code blocks
- Examples
"@
        }
    }
}
```

### Add GUI Quick Action

Edit `ide_chatbot.html`:

```html
<button class="quick-btn" onclick="askQuestion('Your new question?')">
  🆕 New Action
</button>
```

## 💡 Tips

1. **Use Natural Language**: Ask questions naturally, the chatbot will find the best match
2. **Try Keywords**: Include relevant keywords (swarm, todo, model, etc.)
3. **Check Quick Actions**: GUI has preset questions for common tasks
4. **Use Verbose Mode**: CLI verbose mode shows more details
5. **API for Integration**: Use API mode to integrate with other tools
6. **Conversation History**: CLI mode tracks your conversation
7. **Copy Code**: GUI allows easy code copying from answers

## 🐛 Troubleshooting

### API Server Won't Start
```powershell
# Check if port is in use
netstat -an | Select-String "8080"

# Try different port
.\ide_chatbot.ps1 -Mode api -Port 8081
```

### GUI Can't Connect to API
1. Ensure API server is running
2. Check firewall settings
3. Verify API_ENDPOINT in HTML file matches server port

### No Answer Found
- Try rephrasing your question
- Use more specific keywords
- Type "help" to see available commands
- Check the knowledge base for similar questions

## 📍 File Locations

- **CLI Script**: `D:\lazy init ide\scripts\ide_chatbot.ps1`
- **GUI Interface**: `D:\lazy init ide\gui\ide_chatbot.html`
- **Documentation**: `D:\lazy init ide\docs\IDE_CHATBOT_GUIDE.md`

## 🎓 Example Workflows

### Workflow 1: Deploy Swarm
```
You: How do I send a swarm to a directory?
Bot: [Shows swarm deployment commands]
You: How do I monitor them?
Bot: [Shows monitoring commands]
```

### Workflow 2: Todo Management
```
You: How do I add todos?
Bot: [Shows add methods]
You: How do I use the !todos command?
Bot: [Shows parsing examples]
```

### Workflow 3: Model Creation
```
You: How do I create a 7B model?
Bot: [Shows creation commands]
You: How do I train it?
Bot: [Shows training commands]
```

## 🚀 Advanced Usage

### Run as Background Service
```powershell
Start-Job -ScriptBlock {
    cd "D:\lazy init ide\scripts"
    .\ide_chatbot.ps1 -Mode api -Port 8080
}
```

### Integrate with Win32IDE
```cpp
// Call API from C++
std::string answer = HttpPost(
    "http://localhost:8080/ask",
    R"({"question":"How do I create a model?"})"
);
```

### Batch Questions
```powershell
$questions = @(
    "How do I add a todo?",
    "How do I create a model?",
    "Where are important files?"
)

foreach ($q in $questions) {
    .\ide_chatbot.ps1 -Question $q -Mode single-question
}
```

## 📞 Support

For more help:
1. Type "help" in interactive mode
2. Check quick actions in GUI
3. Review knowledge base in script
4. Ask specific questions with keywords
