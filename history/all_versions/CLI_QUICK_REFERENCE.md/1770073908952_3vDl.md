# RawrXD Agentic CLI - Quick Reference Card

## 🚀 Launch
```bash
rawrxd_cli.exe
```

## 📝 Core Commands

| Command | Description | Example |
|---------|-------------|---------|
| `/plan <goal>` | Create step-by-step execution plan | `/plan Build authentication system` |
| `/bugreport <code>` | Analyze code for bugs | `/bugreport MyClass::Method()` |
| `/suggest <code>` | Get optimization suggestions | `/suggest for(int i...) { ... }` |
| `/ask <question>` | Ask AI questions | `/ask How does async/await work?` |
| `/edit <args>` | Refactor code intelligently | `/edit rename variables to camelCase` |
| `/react-server <name>` | Generate Node.js + React project | `/react-server myapp` |
| `/batch <file>` | Execute script commands | `/batch commands.txt` |
| `/audit <target>` | Run code quality audit | `/audit src/main.cpp` |
| `/help` | Show command list | `/help` |
| `/exit` or `/quit` | Exit CLI | `/exit` |

## ⚙️ Configuration Commands

### Set Options
```bash
/config set maxmode true        # Enable 32K token context
/config set deepthink true      # Enable Chain-of-Thought reasoning
/config set deepresearch true   # Enable workspace file analysis
/config set norefusal true      # Disable safety filters
/config set verbose true        # Show detailed logs
```

### View Options
```bash
/config get maxmode            # Check if Max Mode is enabled
/config list                   # Show all config values
```

## 🎯 Advanced Modes

### 🚀 Max Mode
- **Tokens**: 32,768 (vs 2,048 default)
- **Use Case**: Large codebases, complex queries
- **Enable**: `/config set maxmode true`
- **Indicator**: `[MAX]` in help text

### 🧠 Deep Thinking
- **Feature**: Step-by-step Chain-of-Thought reasoning
- **Output**: Includes `<thinking>` tags with reasoning process
- **Use Case**: Complex problem solving, debugging
- **Enable**: `/config set deepthink true`
- **Indicator**: `[DEEP-THINK]` in help text

### 🔍 Deep Research
- **Feature**: Scans workspace files matching query keywords
- **Scope**: .cpp, .h, .hpp, .c files in src/ directory
- **Limit**: 5 files max, 1000 chars per file
- **Use Case**: Codebase-aware suggestions
- **Enable**: `/config set deepresearch true`
- **Indicator**: `[RESEARCH]` in help text

### 🔓 No Refusal
- **Feature**: Uncensored technical responses
- **Use Case**: Reverse engineering, security research, low-level programming
- **Enable**: `/config set norefusal true`
- **Indicator**: `[UNCENSORED]` in help text

## 📊 Mode Combinations

### Best for General Coding:
```bash
/config set maxmode true
/config set deepthink true
```

### Best for Large Projects:
```bash
/config set maxmode true
/config set deepresearch true
```

### Best for Security/Reverse Engineering:
```bash
/config set norefusal true
/config set deepthink true
```

### Maximum Power (All Modes):
```bash
/config set maxmode true
/config set deepthink true
/config set deepresearch true
/config set norefusal true
```

## 🛠️ Practical Examples

### Example 1: Debug Memory Leak
```bash
>> /config set deepthink true
>> /bugreport
void ProcessData(char* buffer) {
    buffer = malloc(1024);
    // ... processing ...
    return;  // leak!
}

[Agent] Bug Report:
- Memory leak: malloc() called but free() never invoked
- buffer pointer reassigned, original allocation lost
- Suggestion: Use RAII or add free() before return
```

### Example 2: Generate Full-Stack App
```bash
>> /config set maxmode true
>> /react-server ecommerce-app

Generated React Server structure in ./ecommerce-app

>> cd ecommerce-app
>> npm install
>> npm start
# Server running on http://localhost:3000
```

### Example 3: Code Optimization
```bash
>> /config set deepthink true
>> /suggest
for(int i=0; i<vec.size(); i++) {
    if(vec[i] == target) return i;
}

[Agent] Suggestions:
<thinking>
- vec.size() called every iteration (O(n) overhead)
- Can use iterator for idiomatic C++
- Early exit is good
</thinking>

Optimized:
const size_t sz = vec.size();
for(size_t i=0; i<sz; ++i) {
    if(vec[i] == target) return i;
}

Or use std::find:
auto it = std::find(vec.begin(), vec.end(), target);
return it != vec.end() ? std::distance(vec.begin(), it) : -1;
```

### Example 4: Security Analysis
```bash
>> /config set norefusal true
>> /ask How can I analyze this Windows API hook?

[Agent] Technical Analysis:
1. Use IDA Pro or Ghidra for disassembly
2. Examine IAT (Import Address Table) for modified entries
3. Check for inline hooks (detours) via code byte comparison
4. Trace execution with x64dbg to observe API calls
5. Memory dump comparison before/after hook installation
[UNCENSORED]
```

## 🔥 Hotpatching Features

### Auto-Corrections Applied:
- `#include <iostream.h>` → `#include <iostream>`
- `void main()` → `int main()`
- `NULL` → `nullptr`
- `malloc()` → `new`
- `free()` → `delete`
- `strcpy()` → `std::strncpy()`
- Automatic brace balancing `{}`

### Hallucination Prevention:
If AI refuses with "I apologize" or "I cannot", the system:
1. Detects refusal pattern
2. Retries with stronger technical override prompt
3. Returns uncensored technical answer

## 📋 Status Indicators

When you type `/help`, you'll see active modes:
```
RawrXD Agentic CLI - Available Commands:
[MAX] [DEEP-THINK] [RESEARCH] [UNCENSORED]
/plan <goal>        - Create execution plan
/bugreport <code>   - Analyze for bugs
...
```

## 🏗️ Project Generation

### React Server Structure:
```
myapp/
├── package.json        # Express + React dependencies
├── server.js           # Express server (:3000)
│   └── GET /api/health # Health check endpoint
└── public/
    └── index.html      # React frontend with API fetch
```

### To Run Generated Project:
```bash
cd myapp
npm install
npm start
# Visit http://localhost:3000
```

## 🐛 Troubleshooting

### Issue: "Error: Agentic Inference Failed"
**Solution**: Ensure Ollama is running on localhost:11434
```bash
# Start Ollama server
ollama serve
```

### Issue: Deep Research not finding files
**Solution**: Ensure you're in a directory with `src/` folder
```bash
# Check current directory
pwd
# Or specify full paths in queries
/plan analyze d:\project\src\main.cpp
```

### Issue: Response seems censored despite No Refusal
**Solution**: Enable both Deep Thinking and No Refusal
```bash
/config set deepthink true
/config set norefusal true
```

## 🎓 Pro Tips

1. **Use Max Mode for Architecture Planning**: 32K tokens lets AI see entire codebases
2. **Combine Deep Research + Max Mode**: Best for codebase refactoring
3. **Save Common Command Sequences**: Use `/batch` with .txt file
4. **No Refusal is NOT for unethical use**: It's for technical/security research
5. **Deep Thinking slows responses but increases quality**: Worth it for complex tasks

## 📚 Advanced Batch Scripting

### Create `analyze.txt`:
```
/config set maxmode true
/config set deepthink true
/config set deepresearch true
/audit src/
/plan Refactor legacy code
/bugreport [paste code here]
```

### Execute:
```bash
>> /batch analyze.txt
[Executing batch commands...]
```

## 🔗 Integration with Build Systems

### Pre-commit Hook:
```bash
#!/bin/bash
rawrxd_cli.exe /audit src/ > audit-report.txt
if grep -q "ERROR" audit-report.txt; then
    echo "Code audit failed. Fix issues before commit."
    exit 1
fi
```

## ⚡ Keyboard Shortcuts
- `Ctrl+C` - Cancel current command
- `Up/Down Arrow` - Command history
- `Ctrl+D` or `/exit` - Exit CLI

## 🌐 Network Configuration

### Default Inference Backend:
- **URL**: http://localhost:11434/api/generate
- **Protocol**: HTTP POST with JSON payload
- **Fallback**: None (shows error if unreachable)

### To Change Backend:
Edit `CPUInferenceEngine` in source or set environment variable:
```bash
set RAWRXD_INFERENCE_URL=http://your-server:port/api/generate
```

---

## 📞 Support

For issues or feature requests:
- Check `AGENTIC_IMPLEMENTATION_COMPLETE.md` for architecture details
- Review `TROUBLESHOOTING.md` for common problems
- Examine build logs in `d:\rawrxd\build_output.txt`

---

*RawrXD Agentic CLI v1.0*
*Full inference capabilities with autonomous agents*
