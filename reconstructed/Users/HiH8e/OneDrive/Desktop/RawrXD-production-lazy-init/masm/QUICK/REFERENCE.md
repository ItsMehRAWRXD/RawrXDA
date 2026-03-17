# RawrXD MASM IDE - Quick Reference & Build Guide

## 🚀 Quick Start

### 1. Verify MASM32 Installation
```batch
dir C:\masm32
```

### 2. Build
```batch
cd masm_ide
build.bat
```

### 3. Run
```batch
build\AgenticIDEWin.exe
```

---

## 📁 Project Structure

```
masm_ide/
├── src/                          Source files
│   ├── main.asm                 Main window & UI (650 LOC)
│   ├── tool_registry.asm        44 tools (500 LOC)
│   ├── model_invoker.asm        HTTP/LLM (600 LOC)
│   ├── action_executor.asm      Actions (500 LOC)
│   ├── agent_bridge.asm         Orchestrator (600 LOC)
│   └── loop_engine.asm          Autonomous loops (500 LOC)
│
├── include/                      Include files
│   ├── constants.inc            Menu/Control IDs (150 LOC)
│   ├── structures.inc           Data structures (400 LOC)
│   └── macros.inc               Utility macros (500 LOC)
│
└── build.bat                    Build script
```

---

## 🛠️ API Quick Reference

### Tool Registry
```asm
ToolRegistry_Init                    Initialize registry
ToolRegistry_ExecuteTool             Execute tool by name
ToolRegistry_GetStats                Get tool statistics
ToolRegistry_ListTools               Get all tools
```

### Model Invoker (LLM)
```asm
ModelInvoker_Init                    Initialize HTTP
ModelInvoker_Invoke                  Synchronous call (wish → plan)
ModelInvoker_InvokeAsync             Asynchronous call
ModelInvoker_SetBackend              Set LLM backend
ModelInvoker_SetEndpoint             Set endpoint URL
ModelInvoker_SetModel                Set model name
ModelInvoker_SetTemperature          Set temperature (0.0-1.0)
ModelInvoker_SetMaxTokens            Set token limit
```

### Action Executor
```asm
ActionExecutor_Init                  Initialize
ActionExecutor_ExecutePlan           Execute plan → results
ActionExecutor_SetProjectRoot        Set project root
ActionExecutor_SetDryRunMode         Enable preview mode
ActionExecutor_SetStopOnError        Error handling
ActionExecutor_Cancel                Cancel execution
ActionExecutor_Pause                 Pause execution
ActionExecutor_Resume                Resume execution
```

### IDEAgentBridge (Orchestrator)
```asm
IDEAgentBridge_Init                  Initialize bridge
IDEAgentBridge_ExecuteWish           Main entry (wish → execution)
IDEAgentBridge_ApprovePlan           User approves plan
IDEAgentBridge_RejectPlan            User rejects plan
IDEAgentBridge_CancelExecution       Cancel execution
IDEAgentBridge_GetExecutionHistory   Get execution history
```

### Callbacks
```asm
IDEAgentBridge_SetThinkingStartedCallback       Planning started
IDEAgentBridge_SetGeneratedPlanCallback         Plan ready
IDEAgentBridge_SetApprovalNeededCallback        Waiting for approval
IDEAgentBridge_SetExecutionStartedCallback      Execution begins
IDEAgentBridge_SetExecutionProgressCallback     Action complete
IDEAgentBridge_SetProgressUpdatedCallback       Progress update
IDEAgentBridge_SetCompletedCallback             Execution complete
IDEAgentBridge_SetErrorCallback                 Error occurred
```

### Autonomous Loop Engine
```asm
LoopEngine_Init                      Initialize
LoopEngine_RunAutonomousLoop         Main 5-stage loop
LoopEngine_SetMaxIterations          Set max iterations
LoopEngine_Cancel                    Cancel loop
LoopEngine_Pause                     Pause loop
LoopEngine_Resume                    Resume loop
```

---

## 📊 Structures

### EXECUTION_PLAN
```asm
szPlanID            db 64 dup(?)      Plan identifier
szWish              db 512 dup(?)     Original wish
dwActionCount       dd ?              Number of actions
pActions            dd ?              Pointer to actions
dwStatus            dd ?              Plan status (0=NEW, 1=EXECUTING, 2=READY)
qwCreated           dq ?              Creation timestamp
```

### ACTION_ITEM
```asm
szActionID          db 64 dup(?)      Action identifier
dwType              dd ?              Action type (0-7)
szDescription       db 256 dup(?)     Human-readable description
szParameters        db 1024 dup(?)    JSON/serialized parameters
```

### PLAN_RESULT
```asm
szPlanID            db 64 dup(?)      Plan identifier
bSuccess            dd ?              Success flag
dwTotalActions      dd ?              Total actions
dwSuccessCount      dd ?              Successful actions
dwFailureCount      dd ?              Failed actions
pActionResults      dd ?              Pointer to results
dwTotalDurationMs   dd ?              Total duration in ms
szSummary           db 512 dup(?)     Summary string
```

### LOOP_ITERATION
```asm
dwIteration         dd ?              Iteration number
dwStage             dd ?              Current stage (0-6)
szDescription       db 256 dup(?)     Stage description
planData            EXECUTION_PLAN    Generated plan
resultData          PLAN_RESULT       Execution results
szReflection        db 1024 dup(?)    Reflection analysis
szNextStep          db 512 dup(?)     Next iteration objective
dwDurationMs        dd ?              Iteration duration
bSuccess            dd ?              Success flag
```

---

## 🔧 Action Types

```
0 = FILE_EDIT          Modify/create files
1 = SEARCH_FILES       Search file system
2 = RUN_BUILD          Execute build
3 = EXECUTE_TESTS      Run tests
4 = COMMIT_GIT         Git operations
5 = INVOKE_COMMAND     Run arbitrary command
6 = RECURSIVE_AGENT    Recursive agent call
7 = QUERY_USER         Interactive input
```

---

## 📝 Tool Categories (44 Total)

| Category | Count | Tools |
|----------|-------|-------|
| File Operations | 12 | read, write, delete, list, create, move, copy, search, grep, compare, merge, info |
| Build & Test | 4 | compile, run_tests, profile, run_command |
| Git Operations | 10 | status, add, commit, push, pull, branch, checkout, diff, log, stash |
| Network | 4 | http_get, http_post, fetch_webpage, download_file |
| LSP Tools | 6 | definitions, references, symbols, completion, hover, refactor |
| AI/ML | 5 | ask_model, generate_code, explain, review, fix_bug |
| Editor | 6 | open, close, get_open, get_active, get_selection, insert_text |
| Memory | 4 | add_memory, get_memory, search_memory, clear_memory |

---

## 🎯 Loop Stages

1. **PLANNING** - Generate multi-step plan via LLM
2. **EXECUTION** - Execute all plan actions
3. **VERIFICATION** - Verify all actions succeeded
4. **REFLECTION** - Analyze results and insights
5. **ADAPTATION** - Plan next iteration
6. **COMPLETED** - Loop finished successfully
7. **FAILED** - Loop failed or cancelled

---

## 🐛 Debugging

### Enable Debug Output
```asm
ifndef DEBUG
    DEBUG equ 1
endif

DbgOut addr szDebugMessage
```

### Assert Macro
```asm
Assert condition, addr szErrorMessage
```

### Check Compilation
```batch
build\AgenticIDEWin.exe
```

---

## 📚 Macros Reference

### String Operations
```asm
szCopy dest, src              Copy zero-terminated string
szLen str                     Get string length (result in eax)
szCmp str1, str2              Compare strings (result in eax)
szCat dest, src               Concatenate strings
```

### Memory Operations
```asm
MemAlloc size                 Allocate memory
MemFree ptr                   Free memory
MemZero ptr, size             Zero memory
MemCopy dest, src, size       Copy memory
```

### UI Operations
```asm
CreateWnd class, title, style, x, y, w, h, parent, id
CreateWndEx exstyle, class, title, style, ...
ShowWnd hwnd                  Show window
HideWnd hwnd                  Hide window
EnableWnd hwnd                Enable window
DisableWnd hwnd               Disable window
```

### Message Boxes
```asm
MsgInfo msg                   Information dialog
MsgError msg                  Error dialog
MsgWarning msg                Warning dialog
MsgQuestion msg               Yes/No dialog
```

---

## 🔌 Integration Points

### Callbacks Pattern
```asm
; Register callback
invoke IDEAgentBridge_SetCompletedCallback, addr MyCallback

; Callback definition
MyCallback proc result:DWORD, duration:DWORD
    ; Handle completion
    ret
MyCallback endp
```

### Async Execution
```asm
; Execute asynchronously
invoke IDEAgentBridge_ExecuteWish, addr szWish, 0  ; no approval needed
; Returns immediately, callbacks fired on completion
```

### Wish Execution Pipeline
```
IDEAgentBridge_ExecuteWish(wish)
    ↓
[WishWorkerThread starts]
    ↓
ModelInvoker_Invoke(wish) → EXECUTION_PLAN
    ↓
g_pfnGeneratedPlan callback
    ↓
[if require_approval] g_pfnApprovalNeeded callback
    ↓
[if approved] IDEAgentBridge_ApprovePlan()
    ↓
[ExecutionWorkerThread starts]
    ↓
ActionExecutor_ExecutePlan(plan) → PLAN_RESULT
    ↓
g_pfnCompleted callback
    ↓
History updated
```

---

## 🚀 Deployment

### Release Build
```batch
build.bat
REM Output: build\AgenticIDEWin.exe (~200KB)
```

### Minimal Dependencies
- Windows 7+ (or later)
- No .NET Framework required
- No Visual C++ runtime required
- No Qt Framework required

### Run
```batch
AgenticIDEWin.exe
```

---

## 📞 Support

### Build Issues
- Verify MASM32 at `C:\masm32`
- Check MASM32 bin directory has `ml.exe` and `link.exe`
- Review build.bat output for specific errors

### Runtime Issues
- Check Windows Event Viewer for errors
- Enable debug output in source code
- Verify project root directory exists
- Check HTTP endpoint is reachable (if using Ollama)

---

## 💡 Tips & Tricks

### Run with Specific Model
```asm
invoke ModelInvoker_SetModel, addr szLlama2
invoke ModelInvoker_SetModel, addr szClaude
invoke ModelInvoker_SetModel, addr szGPT4
```

### Configure Temperature
```asm
; Lower temp = more deterministic (0.1-0.3)
; Higher temp = more creative (0.7-1.0)
fld [fp_temperature]  ; Load as REAL4
invoke ModelInvoker_SetTemperature, fld_result
```

### Enable Dry-Run Mode
```asm
invoke ActionExecutor_SetDryRunMode, 1
; Now executions are previewed without making changes
```

### Access Execution History
```asm
invoke IDEAgentBridge_GetExecutionHistory, 10
; eax = pointer to history array
; ecx = count of entries (max 10)
```

---

**Status: ✅ READY FOR PRODUCTION**

Pure MASM IDE implementation complete with full agentic capabilities, zero external dependencies, and high performance.
