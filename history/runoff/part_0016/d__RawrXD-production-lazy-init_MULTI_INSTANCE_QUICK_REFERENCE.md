# RawrXD Multi-Instance Quick Reference

## Instance Specifications at a Glance

### What You Can Run Simultaneously:
- ✅ **Unlimited IDE windows** - each is independent
- ✅ **100 CLI instances** - (port range 11434-11533)
- ✅ **100 panels per IDE window** - chat/agent panels
- ✅ **20+ agents per IDE** - orchestrated coordination
- ✅ **Unlimited terminal sessions** - per-window pooling

### Resource Limits Per IDE Instance:

| Resource | Limit | Configurable |
|----------|-------|--------------|
| Chat Panels | 100 | Yes (m_maxPanels in AIChatPanelManager) |
| Agents | Unlimited* | No hard limit |
| Terminal Sessions | Unlimited* | No limit (system memory) |
| Model Cache | 1+ | Yes (per instance) |

*Soft limit - depends on available system memory

---

## Launch Multiple Instances

### PowerShell (Recommended)
```powershell
# Launch 3 IDE instances concurrently
1..3 | ForEach-Object { Start-Process rawrxd.exe; Start-Sleep -ms 500 }

# Launch 5 CLI instances
1..5 | ForEach-Object { Start-Process rawrxd-cli.exe; Start-Sleep -ms 200 }
```

### Command Prompt
```cmd
REM Launch IDE
start "" rawrxd.exe
start "" rawrxd.exe
start "" rawrxd.exe

REM Launch CLI instances (each gets unique port)
start "" rawrxd-cli.exe
start "" rawrxd-cli.exe
```

---

## Port Allocation (CLI Instances)

**Formula:** `11434 + (ProcessID % 100)`

**Range:** 11434-11533 (100 possible ports)

### Examples:
| PID | Calculation | Allocated Port |
|-----|------------|-----------------|
| 5128 | 11434 + (5128 % 100) | 11462 |
| 8765 | 11434 + (8765 % 100) | 11499 |
| 10234 | 11434 + (10234 % 100) | 11468 |

Check your instance's port in the console output:
```
RawrXD CLI Instance [PID: 5128] [Port: 11462]
```

---

## Panel Management

### Current Status
```cpp
// In any IDE instance
int activeCount = panelManager->panelCount();    // How many panels in use
int maxAvailable = panelManager->maxPanels();    // 100
```

### Creating Panels
```cpp
AIChatPanel* panel = panelManager->createPanel(parentWidget);
if (!panel) {
    // Hit 100-panel limit - graceful handling needed
}
```

### Panel Configuration (All panels get same settings)
```cpp
panelManager->setCloudConfig(true, "https://api.openai.com/v1", "sk-...");
panelManager->setLocalConfig(true, "http://localhost:11434/api/generate");
panelManager->setRequestTimeout(30000);  // 30 seconds
```

---

## Agent Orchestration

### Available Agent Types
```
CodeAnalyzer       - Code review
BugDetector        - Detect bugs
Refactorer         - Refactoring suggestions
TestGenerator      - Generate tests
Documenter         - Create documentation
PerformanceOptimizer - Optimize code
SecurityAuditor    - Security review
CodeCompleter      - Code completion
TaskPlanner        - Plan tasks
CodeReviewer       - Review code
DependencyAnalyzer - Analyze dependencies
MigrationAssistant - Assist migrations
```

### Task Priority Levels
```
Low          - Background optimization
Normal       - Standard tasks (default)
High         - User-initiated, important
Critical     - Must complete soon
Emergency    - Immediate action required
```

### Check Agent Status
```
Command: status
Shows:
- Agentic Mode: Enabled/Disabled
- Model Loaded: Yes/No
- Autonomous Mode: Enabled/Disabled
```

---

## Terminal Management

### Supported Shells
- **PowerShell** (pwsh.exe) - Modern, recommended
- **Command Prompt** (cmd.exe) - Legacy support

### Terminal Commands (CLI)
```
shell <command>     - Execute CMD command
ps <command>        - Execute PowerShell command
cmd <command>       - Explicit CMD command
```

### Terminal Commands (Qt IDE)
```
[Use TerminalWidget from View menu]
- Select shell type (PowerShell or CMD)
- Click "Start" button
- Enter commands in command line
- "✨ Fix" button asks AI to fix errors
```

### Terminal Session Properties
- **Max History:** 5000 lines per session
- **Async I/O:** Non-blocking output reading
- **Cleanup:** Automatic on session end

---

## Settings & Configuration

### Settings Files Location
- **IDE:** `~/.rawrxd/settings.ini` (QSettings, user-specific)
- **CLI:** `~/.rawrxd/cli_settings.json` (per instance)
- **Overclock:** `~/.rawrxd/overclock.json`
- **Sessions:** `~/.rawrxd/sessions/` (session data + RAG vectors)

### Load/Save Settings
```
Commands:
save          - Save current settings
loadsettings  - Load saved settings
settings      - Show current settings
```

### Critical Setting Groups
```
compute:
  - enable_gpu_matmul: GPU matrix multiplication
  - enable_masm_cpu_backend: MASM acceleration
  - enable_gpu_attention: GPU attention mechanism

overclock:
  - enable_overclock_governor: Auto overclock
  - target_all_core_mhz: Target frequency
  - applied_core_offset_mhz: Current offset
  - applied_gpu_offset_mhz: GPU offset
```

---

## Session Persistence & RAG

### Initialize Session System
```cpp
SessionPersistence::instance().initialize(
    "~/.rawrxd",    // Storage path
    true            // Enable RAG (retrieval-augmented generation)
);
```

### Save/Load Session
```cpp
// Save
QJsonObject sessionData;
sessionData["chat_history"] = ...;
sessionData["agent_states"] = ...;
SessionPersistence::instance().saveSession("session_123", sessionData);

// Load
QJsonObject loaded = SessionPersistence::instance().loadSession("session_123");
```

### RAG (Semantic Search)
```cpp
// Add to vector store
QVector<double> embedding = ...;  // 768-dim or similar
SessionPersistence::instance().addToVectorStore(
    "chat_msg_456",
    embedding,
    {{"content", "What is the best sorting algorithm?"}}
);

// Search similar
QVector<QString> results = 
    SessionPersistence::instance().searchSimilar(queryVector, 5, 0.7);
// Returns top 5 results with cosine similarity >= 0.7
```

---

## CLI Command Reference

### Model Management
```
load <path>         - Load GGUF model
unload              - Unload model
models              - List available GGUF models
modelinfo           - Show loaded model details
```

### Inference
```
infer <prompt>      - Single inference
stream <prompt>     - Streaming inference
chat                - Interactive chat mode
temp <0.0-2.0>      - Set temperature
topp <0.0-1.0>      - Set top-p
maxtokens <count>   - Set max tokens
```

### Agentic Features
```
plan <goal>         - Create execution plan
execute <taskid>    - Execute planned task
status              - Show agentic engine status
selfcorrect         - Run self-correction
analyze <file>      - Analyze code
generate <prompt>   - Generate code
refactor <file>     - Suggest refactorings
```

### Autonomous Mode
```
autonomous [on|off] - Enable/disable autonomous
goal <description>  - Set autonomous goal
```

### Hot-Reload & Debugging
```
hotreload enable    - Enable hot-reload
hotreload disable   - Disable hot-reload
hotreload patch <func> <code> - Apply patch
hotreload revert <id> - Revert patch
hotreload list      - List active patches
```

### System & Telemetry
```
telemetry           - Show telemetry status
snapshot            - Take telemetry snapshot
overclock           - Show overclock status
g                   - Toggle overclock governor
a                   - Apply overclock profile
r                   - Reset overclock offsets
```

### Utilities
```
grep <pattern> [path] - Search files for pattern
read <file> [start] [end] - Read file contents
search <query> [path] - Semantic search (requires Qt IDE)
```

### Settings
```
save                - Save settings
loadsettings        - Load settings
settings            - Show current settings
```

### General
```
help, h, ?          - Show help
version, v          - Show version
mode                - Toggle input mode (CLI vs single-key)
quit, q, exit       - Exit CLI
```

---

## Troubleshooting

### Issue: Panel Limit Exceeded
**Symptom:** Cannot create new chat panels  
**Cause:** Reached 100-panel limit per instance  
**Fix:** Close unused panels, use separate IDE instance

### Issue: Port Conflict on CLI
**Symptom:** CLI fails to start, "port already in use"  
**Cause:** PID modulo collision or manually bound port  
**Fix:** Kill conflicting process, or let system assign next port in range

### Issue: Settings File Corruption
**Symptom:** Settings invalid after multi-instance edit  
**Cause:** Race condition between IDE instances  
**Workaround:** Close all instances, delete corrupt settings, restart

### Issue: Terminal Hangs
**Symptom:** Terminal not responding to input  
**Cause:** Command waiting for interactive input  
**Fix:** Use Ctrl+C (interrupt), or close/restart terminal session

### Issue: Agent Task Stuck
**Symptom:** Task shows "Busy" but not progressing  
**Cause:** Agent resource exhausted or error  
**Fix:** Check `status`, increase `maxConcurrentTasks`, or restart instance

---

## Performance Tuning

### Memory Optimization
- **Close unused panels:** Each panel consumes ~5-10MB
- **Limit terminal history:** Reduce `maxHistoryLines` from 5000
- **Disable RAG if not needed:** Saves vector storage overhead

### CPU Optimization
- **Reduce polling frequency:** Analysis timer default 2000ms
- **Disable auto-analysis:** Turn off real-time code analysis
- **Single-instance mode:** Run one IDE + multiple CLIs

### Network Optimization (if using cloud API)
- **Increase request timeout:** Default 30s, adjust for slow connections
- **Batch requests:** Multiple panels → single API call
- **Use local endpoint:** `http://localhost:11434` instead of cloud

---

## Example Workflows

### Workflow 1: Parallel Code Review (3 IDEs)
```
IDE #1: Load large project, run CodeAnalyzer agent
IDE #2: Load same project, run SecurityAuditor agent
IDE #3: Load same project, run PerformanceOptimizer agent

[All run in parallel, results aggregated manually]
```

### Workflow 2: Multi-Model Inference
```
CLI #1: Load GPT-2, run inference on task A (port 11434)
CLI #2: Load Llama-2, run inference on task B (port 11435)
CLI #3: Load Mistral, run inference on task C (port 11436)

[Run 3 different models simultaneously]
```

### Workflow 3: Terminal-Based Development
```
IDE #1: Code editor + chat panel (100 panels available)
CLI #1: PowerShell for compilation/build
CLI #2: PowerShell for testing
CLI #3: PowerShell for deployment monitoring
```

### Workflow 4: Agentic Task Delegation
```
IDE #1: Create task: "Refactor function X in module Y"
[AgentOrchestrator assigns to Refactorer agent]
[Agent analyzes, generates suggestions, saves to history]
[RAG system stores embeddings for future searches]
```

---

## Glossary

| Term | Definition |
|------|-----------|
| **Panel** | Chat interface widget, max 100 per IDE instance |
| **Agent** | AI-powered bot with specific capabilities |
| **Orchestrator** | Manages agent task assignment & coordination |
| **RAG** | Retrieval-Augmented Generation - semantic search on history |
| **Terminal Session** | PowerShell/CMD shell instance running in background |
| **IPC** | Inter-Process Communication (for cross-instance messaging) |
| **Port Allocation** | Dynamic assignment of network ports per CLI instance |
| **Session Persistence** | Saving/loading IDE state across restarts |

---

## Additional Resources

- **Full Architecture:** See `MULTI_INSTANCE_ARCHITECTURE.md`
- **CLI Source:** `src/cli_command_handler.cpp`
- **Agent Orchestration:** `src/qtapp/AgentOrchestrator.h`
- **Terminal Management:** `src/qtapp/TerminalManager.cpp`
- **Settings:** `src/settings.cpp`, `src/session_persistence.cpp`

