# Agentic Framework - PowerShell Edition

Turn any Ollama model into an agentic one using a ReAct (Reason-Act-Observe) loop.

## Quick Start

```powershell
# Use the new elegant version (recommended)
.\Agentic.ps1 "How many .ps1 files are in the current directory?"

# Or use the original framework
.\Agentic-Framework.ps1 "How many .ps1 files are in the current directory?"
```

## What's New in Agentic.ps1

- ✅ **All bugs fixed** (parameter typos, URL encoding, empty results, context window limits)
- ✅ **9 new tools** (download, unzip, hash, registry, env vars, clipboard, speech, JSON/YAML/CSV)
- ✅ **Plugin system** - Add tools without touching the engine
- ✅ **Pure PowerShell 5.1/7+** compatible
- ✅ **Under 250 lines** of your code

## How It Works

1. **Think** - Model receives prompt and system instructions
2. **Call Tools** - Model decides which tool to use
3. **Observe Results** - Tool executes and returns results
4. **Repeat** - Model uses observations to continue or provide final answer

## Available Tools

- `shell(cmd)` - Run PowerShell/CLI commands
- `powershell(code)` - Execute PowerShell code directly
- `read_file(path)` - Read file contents
- `write_file(path, content)` - Write content to file
- `web_search(query)` - Search web using DuckDuckGo
- `list_dir(path)` - List directory contents
- `git_status(path)` - Get git status

## Example Usage

```powershell
# Simple query
.\Agentic-Framework.ps1 "Count all PowerShell files in this directory"

# Complex task
.\Agentic-Framework.ps1 "Read RawrXD.ps1, find all function definitions, and create a summary document"

# With custom model
.\Agentic-Framework.ps1 -Prompt "Analyze the codebase" -Model "bigdaddyg-personalized-agentic:latest"
```

## Integration with RawrXD

The framework can be integrated into RawrXD as an extension:

```powershell
# In RawrXD, you can call:
& .\Agentic-Framework.ps1 -Prompt $userQuery -Model $selectedModel
```

## Customization

### Add New Tools

Edit `tools\AgentTools.psm1` and add your function:

```powershell
function Invoke-MyCustomTool {
    param([string]$param1)
    # Your tool logic here
    return "Result"
}
$ExportFunctions += "Invoke-MyCustomTool"
```

Then update the `$SYSTEM_PROMPT` in `Agentic-Framework.ps1` to include your new tool.

### Adjust Model Behavior

Modify the `$SYSTEM_PROMPT` variable to change how the agent thinks and responds.

## Architecture

```
User Prompt
    ↓
Agent Loop (ReAct)
    ├─→ Think (Ollama API)
    ├─→ Act (Tool Call)
    ├─→ Observe (Tool Result)
    └─→ Repeat or Answer
```

## Comparison to Python Version

| Feature | Python | PowerShell |
|---------|--------|-------------|
| Framework | openai-functions-faker | Native PowerShell |
| Tools | Python functions | PowerShell functions |
| Loop | Python while | PowerShell while |
| Integration | Standalone | RawrXD compatible |

## Tips

1. **Increase MaxIterations** for complex tasks
2. **Use specific models** - agentic models work better
3. **Add more tools** as needed for your use case
4. **Monitor iterations** to debug agent behavior

## Troubleshooting

**Model forgets format**: Increase model size or add format reminders in system prompt

**Too many iterations**: Add better stopping conditions or reduce task complexity

**Tool errors**: Check tool function error handling

## Next Steps

- Add persistent memory (save conversation history)
- Implement batch tool calls
- Add structured output (JSON responses)
- Create RawrXD extension wrapper

