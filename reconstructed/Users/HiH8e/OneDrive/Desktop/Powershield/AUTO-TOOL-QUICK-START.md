# 🚀 Quick Start: Auto Tool Invocation

## What is it?
Your IDE now automatically executes tools when you use natural language! No more manual `/execute_tool` commands needed.

## Try These Commands

### ✅ File Operations
```
"create a new file app.py"
"show me the contents of readme.md"
"list files in C:\projects"
"edit config.json replace 'localhost' with '127.0.0.1'"
```

### ✅ Directory Operations
```
"create a new directory src"
"make folder logs"
"mkdir build"
"list files in ./src"
```

### ✅ Terminal Commands
```
"run Get-Process"
"execute npm install"
"/exec dir"
"/term Get-ChildItem"
```

### ✅ Search
```
"search for 'TODO' in src folder"
"find 'function main'"
"grep 'error' in logs"
```

## What You'll See

When you type a command like:
```
"create file test.txt"
```

The IDE will show:
```
You > create file test.txt
Agent > 🔧 Auto-invoked tools: create_file
Agent >   ✅ create_file: Success
AI > I've successfully created the file test.txt in your current directory.
```

## How is this different?

### Old Way (Manual)
```
You: "I need to create a file"
AI: "Please use /execute_tool create_file {\"path\":\"file.txt\"}"
You: "/execute_tool create_file {\"path\":\"C:\\code\\file.txt\"}"
AI: "Done"
```

### New Way (Automatic) ✅
```
You: "create file test.txt"
Agent > 🔧 Auto-invoked tools: create_file
Agent >   ✅ create_file: Success
AI: "I've created test.txt for you."
```

## Features

✅ **Smart Path Resolution** - Works with absolute and relative paths
✅ **Parameter Extraction** - Automatically extracts file names, paths, commands
✅ **Confidence Scoring** - Only executes when confident (80%+)
✅ **Error Handling** - Gracefully handles failures
✅ **Real-time Feedback** - Shows which tools were invoked
✅ **Context Enhancement** - AI gets real tool results, not hallucinations

## Still Works Manually

You can still use manual commands when needed:
```
/execute_tool read_file {"path":"C:\\code\\app.py"}
/tools
```

## Supported Patterns

The system recognizes these patterns:

| Pattern | Tool | Example |
|---------|------|---------|
| "create file X" | create_file | "create file app.py" |
| "make folder X" | create_directory | "make folder logs" |
| "show me X" | read_file | "show me readme.md" |
| "list files in X" | list_directory | "list files in src" |
| "edit X replace Y with Z" | edit_file | "edit app.py replace 'old' with 'new'" |
| "run command X" | run_terminal | "run Get-Process" |
| "search for X" | search_files | "search for 'TODO'" |

## Tips

💡 **Use natural language** - "show me the files" works better than "ls"
💡 **Be specific** - "create file C:\\code\\app.py" is clearer than "make file"
💡 **Check feedback** - Look for the 🔧 icon showing which tools ran
💡 **Relative paths** - "create file ./src/app.py" works from current directory

## Need Help?

- Type `/tools` to see all available tools
- Check the Developer Console (F12) for detailed logging
- Read AUTO-TOOL-INVOCATION-COMPLETE.md for full documentation

---

**🎉 Enjoy your newly agentic IDE! It now understands what you want and does it automatically.**
