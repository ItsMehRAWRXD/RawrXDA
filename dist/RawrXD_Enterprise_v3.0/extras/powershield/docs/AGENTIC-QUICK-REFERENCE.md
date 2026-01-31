# ============================================
# BIGDADDYG AGENTIC - QUICK REFERENCE
# One-liner commands to get started
# ============================================

## 🚀 LAUNCH

```powershell
# Launch RawrXD IDE with default settings
.\RawrXD.ps1

# Launch and auto-select agentic model (add this to RawrXD if needed)
.\RawrXD.ps1
```

## 🤖 SELECT MODEL IN IDE

In the Ollama Model dropdown, select:
- **bg-ide-agentic** ← Use this for full agentic functionality

## 💬 EXAMPLE AGENTIC COMMANDS

### File Operations
```
"List all PowerShell files"
"Read the RawrXD.ps1 file"
"Find all files containing 'agent'"
"Create a new file called test.ps1"
"Delete the test.ps1 file"
"Show me the file info for RawrXD.ps1"
```

### Terminal/Command Execution
```
"Execute: Get-Process | Select-Object -First 5"
"Run the AgentTools.ps1 script"
"List all services running"
"Get the current user and domain"
```

### Git Operations
```
"Show git status"
"Commit all changes with message 'Updated agentic features'"
"Show the last 5 commits"
"Push to main branch"
```

### Code Analysis
```
"Analyze this code for bugs: [paste code]"
"Review this PowerShell script"
"Suggest refactoring for this code"
"Explain what this function does"
```

### Multi-Step Workflows
```
"List all Python files, analyze them for bugs, and create a report"
"Generate a REST API endpoint and save it to api/endpoint.js"
"Commit all changes and push to main"
"Find all TODO comments and list them"
```

## 🔧 DIRECT TOOL INVOCATION (Advanced)

If natural language doesn't work, use the format:
```
/execute_tool list_directory {"path":".", "recursive":true, "filter":"*.ps1"}
```

## 📊 TOOL CATEGORIES

View available tools:
```
/tools              ← List all tools
/tools FileSystem   ← List filesystem tools
/tools Terminal     ← List terminal tools
/tools Git          ← List git tools
/tools Code         ← List code analysis tools
```

## 🧪 TESTING

Quick verification:
```powershell
# Test 1: Model available
ollama list | Select-String "bg-ide-agentic"

# Test 2: Function call generation
ollama run bg-ide-agentic "List all files"

# Test 3: IDE integration
.\RawrXD.ps1
# Select bg-ide-agentic and type: "List all PowerShell files"
```

## 🔄 FUNCTION CALL FORMAT

When the AI responds with:
```
{{function:tool_name(arg1, arg2, kwarg=value)}}
```

RawrXD IDE automatically:
1. ✅ Parses the function call
2. ✅ Invokes the tool
3. ✅ Captures the result
4. ✅ Displays it to you

## 🎯 BEST PRACTICES

1. **Be specific**: "List all PS1 files" → better results than "List files"
2. **Chain operations**: Ask for multiple steps, AI will chain calls
3. **Provide context**: "In this directory" vs generic requests
4. **Multi-turn**: Follow up with related questions for better workflow
5. **Complex tasks**: Break into steps or let AI chain multiple tools

## ⚡ COMMON WORKFLOWS

### Development Workflow
```
1. "List all uncommitted changes"
   {{function:git_status('.')}}

2. "Show me what changed"
   {{function:git_diff('.', 'filename.ps1')}}

3. "Commit these changes"
   {{function:git_commit('.', 'Implemented feature X')}}

4. "Push to main"
   {{function:git_push('.', 'origin', 'main')}}
```

### Code Review Workflow
```
1. "Read the main.ps1 file"
   {{function:read_file('main.ps1')}}

2. "Analyze this code for bugs"
   {{function:analyze_code(content, 'powershell')}}

3. "Suggest refactoring"
   {{function:suggest_refactoring(content)}}

4. "Format the code"
   {{function:format_code(content, 'powershell')}}
```

### Project Analysis Workflow
```
1. "Find all project files"
   {{function:search_files('.', '*', recursive=true)}}

2. "Detect project dependencies"
   {{function:detect_dependencies('.')}}

3. "Analyze for potential issues"
   {{function:analyze_code(all_files, 'powershell')}}
```

## 🛠️ TROUBLESHOOTING

**Model not responding with function calls?**
- Make sure you selected `bg-ide-agentic` model
- Try restarting RawrXD.ps1
- Verify ollama is running: `ollama list`

**Function calls not executing?**
- Check that AgentTools are loaded (check RawrXD logs)
- Verify the function name matches registry
- Check argument syntax in the function call

**Tool returns error?**
- Invalid path → verify file/directory exists
- Permission denied → run with elevated privileges
- Command error → check command syntax

## 📞 SUPPORT COMMANDS

```
/tools              ← List all available tools
/env                ← Show environment info
/deps               ← Show project dependencies
/help               ← Show help
```

## 🎓 LEARNING

Start simple:
1. ✅ "List files" → builds confidence
2. ✅ "Read a file" → understand tool output
3. ✅ "Create a file" → see tool execution
4. ✅ Chain multiple tools → complex workflows
5. ✅ Explore multi-turn conversations

## 🚀 ADVANCED USAGE

### Agentic Mode Levels
In RawrXD IDE, you can set agent mode:
- **Off** - No agentic features
- **Auto** - Agentic when context requires
- **Max** - Always agentic with automatic context gathering

### Tool Customization
Add new tools to RawrXD by:
1. Define handler function
2. Call `Register-AgentTool`
3. Reload RawrXD
4. Use in natural language

Example:
```powershell
Register-AgentTool -Name "custom_tool" `
  -Description "My custom tool" `
  -Handler { param([string]$input) return "processed" }
```

## 📈 PERFORMANCE TIPS

- Use specific paths instead of recursive searches
- Limit file sizes for analysis
- Chain related operations
- Use filters to narrow results

## ✨ FEATURES

✅ Natural language commands
✅ Automatic tool execution
✅ Multi-step workflows
✅ Real-time result display
✅ Git integration
✅ Code analysis
✅ File management
✅ Terminal execution
✅ Web operations
✅ AI-powered assistance

---

**Quick Start**: Launch RawrXD.ps1, select bg-ide-agentic, and start typing!
