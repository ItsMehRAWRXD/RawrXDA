# RawrXD CLI Quick Reference - Feature Parity Edition

**45 Commands | Agentic | Autonomy | Debugger | Full IDE in Terminal**

```
╔════════════════════════════════════════════════════════════════════╗
║                  RawrXD AI Runtime (CLI)                          ║
║             Feature Parity with Win32 IDE - Feb 2026               ║
╚════════════════════════════════════════════════════════════════════╝
```

## 🔧 FILE OPERATIONS (5 commands)

```
!new                          Create new file
!open <path>                  Open file
!save                         Save current file
!save_as <path>               Save as new file
!close                        Close file
```

## ✂️ EDITOR OPERATIONS (7 commands)

```
!cut                          Cut to clipboard
!copy                         Copy to clipboard
!paste                        Paste from clipboard
!undo                         Undo last change
!redo                         Redo last change
!find <text>                  Find text in buffer
!replace <old> <new>          Replace text
```

## 🤖 AGENTIC OPERATIONS (4 commands)

```
!agent_execute <prompt>       Execute single agent command
!agent_loop <prompt> [n]      Start multi-turn agent loop (n iterations)
!agent_goal <goal>            Set agent goal
!agent_memory <obs>           Add observation to memory
!agent_memory show            Show all agent memory
```

## 🤖 AUTONOMY OPERATIONS (4 commands)

```
!autonomy_start               Enable autonomy mode
!autonomy_stop                Disable autonomy mode
!autonomy_goal <goal>         Set autonomy goal
!autonomy_rate <n>            Set max actions per minute
```

## 🐛 DEBUG OPERATIONS (7 commands)

```
!breakpoint_add <file>:<line> Add breakpoint (e.g., main.cpp:42)
!breakpoint_list              List all breakpoints
!breakpoint_remove <idx>      Remove breakpoint by index
!debug_start                  Start debugger
!debug_stop                   Stop debugger
!debug_step                   Step through code
!debug_continue               Continue execution
```

## 💻 TERMINAL OPERATIONS (4 commands)

```
!terminal_new                 Create new terminal pane
!terminal_split <orient>      Split terminal (horizontal/vertical)
!terminal_kill                Close current terminal pane
!terminal_list                List all terminal panes
```

## 🔥 HOTPATCH OPERATIONS (2 commands)

```
!hotpatch_create              Create hotpatch from current file
!hotpatch_apply <file>        Apply hotpatch without restart
```

## 🔍 SEARCH & TOOLS (3 commands)

```
!search <pattern> [path]      Search files for pattern
!analyze                      Analyze current file
!profile                      Profile code performance
```

## ⚙️ CONFIGURATION (5 commands)

```
!mode <mode>                  Set AI mode (ask|plan|edit|bugreport|codesuggest)
!engine <name>                Switch model (e.g., sovereign800b)
!deep <on|off>                Enable/disable deep thinking
!research <on|off>            Enable/disable deep research
!max <tokens>                 Set context token limit
```

## 🚀 IDE & SERVER (2 commands)

```
!generate_ide [path]          Generate React web IDE
!server <port>                Start backend API server (default 8080)
```

## 📊 STATUS & UTILITY (2 commands)

```
!status                       Show current editor status
!help                         Show this help message
!quit                         Exit CLI
```

---

## QUICK EXAMPLES

### Example 1: Open File and Edit

```
rawrxd> !open D:\code\main.cpp
✅ Opened: D:\code\main.cpp (1,234 bytes)
rawrxd> !find "function"
✅ Found at position 42
rawrxd> !replace "old" "new"
✅ Replaced 3 occurrence(s)
rawrxd> !save
✅ Saved: D:\code\main.cpp
```

### Example 2: Use Agent to Improve Code

```
rawrxd> !agent_goal "Improve code quality"
✅ Goal set: Improve code quality
rawrxd> !agent_loop "Refactor this file to use modern C++ features" 5
🚀 Starting agent loop: Refactor this file... (max 5 iterations)
[Agent Iter 1/5] Processing...
[Agent Iter 2/5] Processing...
...
✅ Agent loop completed
```

### Example 3: Debug Code

```
rawrxd> !breakpoint_add main.cpp:42
✅ Breakpoint added: main.cpp:42
rawrxd> !debug_start
🐛 Debugger started
rawrxd> !debug_step
➡️  Step executed
rawrxd> !debug_continue
▶️  Continuing execution
rawrxd> !debug_stop
⏹️  Debugger stopped
```

### Example 4: Enable Autonomy

```
rawrxd> !autonomy_start
🤖 Autonomy enabled. Use !autonomy_goal to set objective.
rawrxd> !autonomy_goal "Fix all compilation errors"
✅ Autonomy goal set: Fix all compilation errors
rawrxd> !autonomy_rate 30
✅ Max actions per minute: 30
rawrxd> !status
📊 RawrXD CLI Status:
  Autonomy: enabled
  Agent goal: Fix all compilation errors
```

### Example 5: Work with Terminal

```
rawrxd> !terminal_new
✅ New terminal pane: Terminal-1
rawrxd> !terminal_split horizontal
✅ Terminal split horizontal
rawrxd> !terminal_list
📋 Terminal panes:
  [0] Terminal-1
  [1] Terminal-2
```

---

## STATE MANAGEMENT

The CLI maintains full IDE state:

| State | Tracked | Example |
|-------|---------|---------|
| Current File | ✅ | `main.cpp` |
| Editor Buffer | ✅ | Full file contents |
| Undo/Redo Stacks | ✅ | 100-item history each |
| Clipboard | ✅ | Multiple clipboard items |
| Agent Goal | ✅ | "Improve code quality" |
| Agent Memory | ✅ | Observations list |
| Breakpoints | ✅ | file:line list |
| Terminal Panes | ✅ | Active panes |
| Autonomy Mode | ✅ | enabled/disabled |

---

## ADVANCED USAGE

### Multi-Step Workflow

```bash
# 1. Open file
!open solution.cpp

# 2. Find problem area
!find "TODO"

# 3. Set agent goal
!agent_goal "Implement this TODO"

# 4. Run agent
!agent_execute "Complete the TODO at this location"

# 5. Save result
!save

# 6. Enable autonomy for follow-up improvements
!autonomy_start
!autonomy_goal "Optimize performance"
```

### Agentic Development Loop

```bash
# Set up agent
!agent_goal "Refactor the codebase"
!agent_memory "Use const-correctness"
!agent_memory "Prefer std:: over custom implementations"

# Start reasoning loop
!agent_loop "Analyze all files in directory" 10

# Check results
!status
```

### Debug + Develop Combined

```bash
# Start with breakpoints
!breakpoint_add main.cpp:10
!breakpoint_add handler.cpp:25

# Debug
!debug_start
!debug_step
!debug_continue

# Make changes while debugging
!open main.cpp
!find "bug"
!replace "buggy_code" "fixed_code"

# Continue
!debug_continue
```

---

## TIPS & TRICKS

1. **Use !status frequently** - See all state at a glance
2. **Undo/Redo** - Full edit history maintained
3. **Agent Memory** - Build context for agent with observations
4. **Autonomy Rate** - Prevent runaway agents with rate limiting
5. **Terminal Panes** - Split terminal for parallel work
6. **Hotpatch** - Apply changes without restarting
7. **Search** - Find patterns across files before refactoring

---

## KEYBOARD SHORTCUTS (Terminal Dependent)

```
Ctrl+C          Cancel current operation
Ctrl+D          Quit (some terminals)
Up Arrow        Command history (if implemented)
Tab             Command auto-complete (if implemented)
```

---

## FEATURE MATRIX: CLI vs Win32 IDE

| Feature | CLI | Win32 GUI |
|---------|-----|-----------|
| File Operations | ✅ 5/5 | ✅ 5/5 |
| Editor Operations | ✅ 7/7 | ✅ 7/7 |
| Agentic | ✅ 4/4 | ✅ 4/4 |
| Autonomy | ✅ 4/4 | ✅ 4/4 |
| Debugging | ✅ 7/7 | ✅ 7/7 |
| Terminal | ✅ 4/4 | ✅ 4/4 |
| Hotpatch | ✅ 2/2 | ✅ 2/2 |
| Tools | ✅ 3/3 | ✅ 3/3 |
| Configuration | ✅ 5/5 | ✅ 5/5 |
| IDE & Server | ✅ 2/2 | ✅ 2/2 |
| Status | ✅ 2/2 | ✅ 2/2 |
| **TOTAL** | ✅ **45/45** | ✅ **45/45** |

---

## GETTING HELP

```bash
!help                        # Show full command list
!status                      # Show current state
!agent_goal                  # Show current agent goal (no args)
!agent_memory show           # Show all agent memory
!breakpoint_list             # List active breakpoints
!terminal_list               # List active terminals
```

---

## SUPPORT & TROUBLESHOOTING

| Issue | Solution |
|-------|----------|
| "No file open" | Use `!open <path>` to open a file |
| Undo not working | File must be opened or new file created |
| Agent loop won't start | Set goal first with `!agent_goal <goal>` |
| Breakpoint not working | Debugger must be running (`!debug_start`) |
| Terminal closed | Create new one with `!terminal_new` |

---

**Version: 2.0 | Updated: Feb 6, 2026 | Feature Parity Complete**

Both CLI and Win32 IDE share the same command set, state management, and behavior. You can seamlessly switch between them.
