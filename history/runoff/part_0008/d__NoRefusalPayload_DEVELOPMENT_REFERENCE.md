# Development Reference Guide - No-Refusal Payload Engine

## Quick Reference Card

### 🔨 Build Commands

| Task | Command | Time |
|------|---------|------|
| **Build (Debug)** | `Ctrl+Shift+B` or `.\build.ps1 -Configuration Debug` | ~10-15s |
| **Build (Release)** | `.\build.ps1 -Configuration Release` | ~10-15s |
| **Clean Rebuild** | `.\build.ps1 -Clean -Configuration Debug` | ~60s |
| **CMake Reconfigure** | Delete `build/` folder, press `Ctrl+Shift+B` | ~30s |

### 🐛 Debug Commands

| Action | Keyboard | Menu |
|--------|----------|------|
| Start Debug | `F5` | Run → Start Debugging |
| Continue | `F5` | Run → Continue |
| Step Over | `F10` | Run → Step Over |
| Step Into | `F11` | Run → Step Into |
| Step Out | `Shift+F11` | Run → Step Out |
| Stop Debug | `Shift+F5` | Run → Stop Debugging |
| Toggle Breakpoint | `Ctrl+K Ctrl+B` | In gutter |
| View Variables | (Debug view) | Run → View Variables |

### 📝 Editor Commands

| Action | Keyboard |
|--------|----------|
| Open Command Palette | `Ctrl+Shift+P` |
| Open Terminal | `Ctrl+`` ` `` |
| Find | `Ctrl+F` |
| Find & Replace | `Ctrl+H` |
| Go to Definition | `F12` |
| Go to Declaration | `Ctrl+Home` |
| Find All References | `Shift+F12` |
| Rename Symbol | `F2` |
| Format Document | `Shift+Alt+F` |

---

## 🔧 Common Development Tasks

### Task 1: Modify Assembly Code

**File**: `asm/norefusal_core.asm`

```asm
; Add to ProtectionLoop to change behavior
ProtectionLoop:
    ; ... existing code ...
    
    ; TODO: Add custom logic here
    mov ecx, 5000
    call Sleep
    
    jmp ProtectionLoop
```

**Rebuild**:
```powershell
Ctrl+Shift+B
```

**What happens**:
1. CMake detects `.asm` file change
2. ml64.exe recompiles assembly
3. Linker re-links object files
4. Executable updated

---

### Task 2: Extend C++ Supervisor

**File**: `src/PayloadSupervisor.cpp`

```cpp
void PayloadSupervisor::LaunchCore() {
    m_isActive = true;
    std::cout << "[+] Custom message before launch...\n";
    
    // Add new initialization
    MyCustomInitialization();
    
    Payload_Entry();
}

void PayloadSupervisor::MyCustomInitialization() {
    std::cout << "[*] Running custom initialization...\n";
    // Your code here
}
```

**Update header** (`include/PayloadSupervisor.hpp`):
```cpp
private:
    void MyCustomInitialization();
```

**Rebuild**: `Ctrl+Shift+B`

---

### Task 3: Add New Console Output

**Location**: `src/main.cpp`

```cpp
int main() {
    try {
        // ... existing code ...
        
        std::cout << "[*] Custom debug info...\n";
        std::cout << "Process ID: " << GetCurrentProcessId() << "\n";
        
        // ... rest of code ...
    }
}
```

**Rebuild**: `Ctrl+Shift+B`

---

### Task 4: Modify Debug Configuration

**File**: `.vscode/launch.json`

```json
{
    "name": "(msvc) Custom Launch",
    "type": "cppvsdbg",
    "request": "launch",
    "program": "${workspaceFolder}/build/Debug/NoRefusalEngine.exe",
    "stopAtEntry": true,           // Pause at main()
    "console": "integratedTerminal"
}
```

**Use**: F5 → Select new configuration

---

### Task 5: Performance Profiling

```powershell
# Use Windows Performance Toolkit
wpr.exe -start GeneralProfile
D:\NoRefusalPayload\build\Debug\NoRefusalEngine.exe
# Wait ~10 seconds
# Press Ctrl+C in another terminal
wpr.exe -stop trace.etl
wpa.exe trace.etl  # Opens Windows Performance Analyzer
```

---

## 🧪 Testing Checklist

Before committing changes:

- [ ] Code compiles without errors (`Ctrl+Shift+B`)
- [ ] No compiler warnings generated
- [ ] Executable runs without crashes
- [ ] Debugger can attach and step through code
- [ ] Output appears correctly formatted
- [ ] Assembly changes produce expected behavior

---

## 🐛 Debugging Techniques

### Technique 1: Inspect Variable Values

1. Set breakpoint
2. Run debugger (F5)
3. Hover over variable to see value
4. Or use Debug Console: `?variableName`

### Technique 2: Conditional Breakpoint

1. Right-click on breakpoint (red dot)
2. Select "Edit Breakpoint"
3. Enter condition: `supervisor.m_isActive == true`
4. Only breaks when condition is true

### Technique 3: Watch Expression

1. Debug view → Watch tab
2. Click "+" to add expression
3. Type: `supervisor.m_processHandle`
4. Value updates during execution

### Technique 4: Debug Console

In Debug view, bottom console:

```
?sizeof(Engine::PayloadSupervisor)
?&supervisor
@$bp
```

---

## 📊 Code Statistics

### Source Code Breakdown

```
C++ Code:
  main.cpp:                 ~50 lines
  PayloadSupervisor.cpp:    ~100 lines
  PayloadSupervisor.hpp:    ~50 lines
  Total C++:               ~200 lines

Assembly Code:
  norefusal_core.asm:      ~150 lines
  
Total Project:            ~350 lines of code
```

### Build Metrics

| Metric | Value |
|--------|-------|
| Executable Size (Debug) | ~100 KB |
| Executable Size (Release) | ~50 KB |
| Link Time | ~3 seconds |
| Assembly Time | ~2 seconds |
| C++ Compile Time | ~5 seconds |
| Total Build Time | ~10 seconds |

---

## 📂 File Edit Examples

### Example 1: Change Protection Loop Interval

**File**: `asm/norefusal_core.asm`

**Before**:
```asm
retry_interval          dd 5000  ; 5 seconds
```

**After**:
```asm
retry_interval          dd 1000  ; 1 second (more responsive)
```

**Effect**: Protection loop runs more frequently

---

### Example 2: Add Custom Exception Handler

**File**: `asm/norefusal_core.asm`

**Modify** `ResilienceHandler`:
```asm
ResilienceHandler proc
    mov rax, [rcx]
    mov eax, [rax]
    
    cmp eax, 0C0000005h  ; STATUS_ACCESS_VIOLATION
    je restore_state
    
    cmp eax, 0DEADBEEF   ; Custom exception code
    je my_custom_handler
    
    xor rax, rax
    ret

my_custom_handler:
    ; Your custom handling here
    jmp restore_state

ResilienceHandler endp
```

---

### Example 3: Add Logging to C++

**File**: `src/PayloadSupervisor.cpp`

```cpp
bool PayloadSupervisor::InitializeHardening() {
    std::cout << "[DEBUG] InitializeHardening called\n";
    
    if (!SetConsoleCtrlHandler(&PayloadSupervisor::ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[ERROR] Handler registration failed with code: " 
                  << GetLastError() << "\n";
        return false;
    }
    
    std::cout << "[DEBUG] Handler registration successful\n";
    return true;
}
```

---

## 🔍 Error Resolution Guide

### Compile Error: "undeclared identifier"
1. Check header file is included
2. Verify namespace (e.g., `Engine::`)
3. Ensure forward declaration exists

### Linker Error: "unresolved external symbol"
1. Check function is exported with `extern "C"`
2. Verify library is linked (check CMakeLists.txt)
3. Confirm function signature matches definition

### Runtime Error: "Access Violation"
1. Check pointer validity before dereference
2. Verify memory allocation succeeded
3. Use debugger to inspect pointer value

### Assembly Error: "error A2008"
1. Check register sizes (rax vs eax)
2. Verify operand count matches instruction
3. Check alignment and addressing modes

---

## 📈 Performance Optimization

### Optimization 1: Reduce Sleep Interval
**File**: `asm/norefusal_core.asm`
```asm
retry_interval dd 100  ; Reduced from 5000ms
```
**Effect**: More frequent checks, higher CPU usage

### Optimization 2: Inline Critical Functions
**File**: `src/PayloadSupervisor.cpp`
```cpp
inline void PayloadSupervisor::LaunchCore() {
    // Force inline compilation
}
```

### Optimization 3: Release Build
```powershell
.\build.ps1 -Configuration Release
```
**Effect**: ~10-15% faster execution, no debug symbols

---

## 🔐 Debugging Security-Sensitive Code

### Caution: VEH and Debuggers
The exception handler may interact unexpectedly with debuggers.

**Safe Debugging**:
1. Set breakpoint BEFORE calling `Payload_Entry()`
2. Step through C++ initialization first
3. Use `stopAtEntry: true` if stepping into assembly
4. Monitor Debug Console for exception messages

---

## 📋 Pre-Deployment Checklist

- [ ] All changes committed to version control
- [ ] Code compiles without warnings (Release build)
- [ ] Unit tests pass (if applicable)
- [ ] Release build tested on target system
- [ ] Documentation updated
- [ ] Security review completed
- [ ] Performance profiling done

---

## 🎓 Learning Resources

### Assembly Deep Dive
- Read: `asm/norefusal_core.asm` comments
- Study: Each function's purpose
- Experiment: Modify protection loop

### C++ Integration
- Review: `src/PayloadSupervisor.cpp` structure
- Understand: Exception handling approach
- Practice: Extend with new features

### CMake Mastery
- Read: `CMakeLists.txt` comments
- Research: CMake documentation
- Experiment: Add new compilation flags

---

## 🚀 Advanced Tasks

### Task: Add Multi-Threading
```cpp
// In PayloadSupervisor.cpp
std::thread protectionThread(&PayloadSupervisor::LaunchCore, this);
protectionThread.detach();
```

### Task: Implement Heartbeat Monitoring
```cpp
std::chrono::system_clock::time_point lastBeat = 
    std::chrono::system_clock::now();
```

### Task: Add Custom Command Handler
```asm
; In norefusal_core.asm
; Monitor pipe or socket for commands
```

---

## 📞 Support Resources

| Issue | Solution |
|-------|----------|
| Won't build | Check VS 2022 installed with MASM support |
| Can't debug | Verify .vscode/launch.json is correct |
| Output not showing | Check Console in Debug view |
| Changes not reflecting | Ensure rebuild executed (Ctrl+Shift+B) |

---

**This reference guide covers 90% of common development tasks. For advanced scenarios, refer to component source code comments.**
