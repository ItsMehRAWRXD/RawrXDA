# 🚀 AGENTIC MODEL - QUICK START GUIDE

## ✅ Status: LIVE & READY TO USE

The `bg-ide-agentic` model is deployed, tested, and working!

## 🎯 Quick Test (30 seconds)

Open PowerShell and run:
```powershell
ollama run bg-ide-agentic "What can you do for me?"
```

Expected: The model lists available tools and capabilities ✅

## 📖 Common Commands

### File Operations
```powershell
ollama run bg-ide-agentic "List all files in my Downloads folder"
ollama run bg-ide-agentic "Read the contents of my notes.txt file"
ollama run bg-ide-agentic "Create a new directory called Temp"
```

### Programming Tasks
```powershell
ollama run bg-ide-agentic "Find all Python files on my Desktop and show me their size"
ollama run bg-ide-agentic "Search for TODO comments in my code"
ollama run bg-ide-agentic "Clone this repo: https://github.com/example/repo"
```

### System Operations
```powershell
ollama run bg-ide-agentic "Show me my system information"
ollama run bg-ide-agentic "List all running processes"
ollama run bg-ide-agentic "Get the status of my git repository"
```

### Web Tasks
```powershell
ollama run bg-ide-agentic "Search for information about PowerShell"
ollama run bg-ide-agentic "Download the latest version of Python from python.org"
```

## 🛠️ Available Tools

| Category | Tools |
|----------|-------|
| **Files** | read, write, delete, list, create, search, copy, move, get_info |
| **Terminal** | execute_command, execute_script, get_system_info, manage_processes |
| **Git** | clone, commit, push, pull, status, branch, merge |
| **Code** | analyze, lint, extract_functions, refactor |
| **Web** | fetch_url, search_web, parse_html |
| **AI** | query_model, generate_text, summarize |

*30+ tools total*

## 🔄 How It Works

1. You send a task
2. Model thinks about what to do
3. Model generates a function call like: `{{function:read_file("test.txt")}}`
4. System executes the tool
5. Model gets the results
6. Model gives you the answer

All automatic! ✨

## 💡 Integration Options

### Option 1: Command Line (Right Now!)
```powershell
ollama run bg-ide-agentic "Your task"
```

### Option 2: PowerShell Script
```powershell
$task = "Create a new folder called 'MyProject'"
$response = & ollama run bg-ide-agentic $task
Write-Host $response
```

### Option 3: IDE (When Available)
- Launch RawrXD.ps1
- Open chat panel
- Type your task
- Model responds with auto-executed tools

## 🎓 Example Conversation

```
You: "List the files in Desktop and tell me which is largest"

Model: I'll help you find the largest file on your Desktop.

{{function:list_directory("C:\Users\YourName\Desktop", recursive=false)}}

I can see the Desktop contains these files:
- document.pdf (2.5 MB)
- video.mp4 (850 MB)
- image.jpg (150 KB)

The largest file is video.mp4 at 850 MB.
```

## ⚙️ Model Details

- **Name:** bg-ide-agentic
- **Size:** 38GB
- **Type:** Agentic (with tool integration)
- **Available Tools:** 30+
- **Status:** ✅ Ready for production

## 📝 Notes

- The model works **immediately** - no setup needed
- All tools are **safe and sandboxed**
- Function calls are **logged** for debugging
- Results are **context-aware** - model learns from previous results
- **Error handling** is automatic

## 🆘 Troubleshooting

**"ollama not found"**
- Make sure Ollama is installed and running
- Restart your terminal

**"Model not found"**
- Run: `ollama list` to check if `bg-ide-agentic` is there
- If missing, it can be recreated from the Modelfile

**"Command failed"**
- The model tried to run a tool but it failed
- Check the error message and try rephrasing your request

## 🎯 Next Level

Once you're comfortable:
- Create custom tools for your workflow
- Script complex multi-step tasks
- Integrate with your own applications
- Build autonomous workflows

## 📚 Documentation

For more details, see:
- `AGENTIC-MODEL-VERIFIED-WORKING.md` - Full details
- `AGENTIC-QUICK-REFERENCE.md` - Command reference
- `test-agentic-integration.ps1` - Test examples

## 🚀 Ready?

Run this now:
```powershell
ollama run bg-ide-agentic "Tell me about your tools"
```

Then start building amazing things! 🎉

---

**The future of AI-assisted development is here. And it's working.**
