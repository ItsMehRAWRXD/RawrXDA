# MASM Modules - Quick Start Guide

## 🚀 Get Started in 5 Minutes

### Step 1: Include the Master Header

Add this single line to any MASM file:

```asm
INCLUDE masm/masm_master_include.asm
```

This gives you access to:
- ✅ All Zero-Day Agentic Engine functions
- ✅ All constants and data structures
- ✅ All Win32 API declarations
- ✅ Helper macros

### Step 2: Call the Functions

```asm
.DATA
    pRouter     QWORD ?         ; Your router pointer
    pTools      QWORD ?         ; Your tools pointer
    pPlanner    QWORD ?         ; Your planner pointer
    pCallbacks  QWORD ?         ; Your callbacks pointer
    hEngine     QWORD ?         ; Engine handle

.CODE

MainFunction PROC
    ; Create the engine
    MOV rcx, [pRouter]
    MOV rdx, [pTools]
    MOV r8, [pPlanner]
    MOV r9, [pCallbacks]
    CALL ZeroDayAgenticEngine_Create
    
    ; Engine is now in RAX
    MOV [hEngine], rax
    
    ; Start a mission
    MOV rcx, [hEngine]
    MOV rdx, [pRouter]
    CALL ZeroDayAgenticEngine_StartMission
    
    ; Mission ID is in RAX
    ; ... use it ...
    
    ; Later: abort if needed
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_AbortMission
    
    ; Cleanup
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_Destroy
    
    RET
MainFunction ENDP

END
```

### Step 3: Compile

```powershell
# Method 1: Automatic (recommended)
.\Build-MASM-Modules.ps1

# Method 2: Manual
ml64.exe /c your_file.asm
link.exe your_file.obj bin\masm_modules.lib
```

### Step 4: Run

```powershell
your_program.exe
```

---

## 📚 Common Use Cases

### Use Case 1: Simple Mission Execution

```asm
INCLUDE masm/masm_master_include.asm

.DATA
    hEngine QWORD ?
    
.CODE

CreateAndRunMission PROC
    ; Create engine
    ; (assume router/tools/planner/callbacks in registers)
    CALL ZeroDayAgenticEngine_Create
    MOV [hEngine], rax
    
    ; Start mission (assume router in rdx)
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_StartMission
    ; Mission ID now in RAX
    
    ; Later cleanup
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_Destroy
    
    RET
CreateAndRunMission ENDP
```

### Use Case 2: Mission Monitoring

```asm
INCLUDE masm/masm_master_include.asm

.CODE

MonitorMission PROC
    LOCAL hEngine:QWORD
    LOCAL state:BYTE
    
    ; ... create engine ...
    MOV [hEngine], rax
    
    ; Start mission
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_StartMission
    
LoopCheckState:
    ; Check mission state
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_GetMissionState
    MOV [state], al
    
    ; Compare with states
    CMP al, MISSION_STATE_IDLE
    JE MissionIdle
    
    CMP al, MISSION_STATE_RUNNING
    JE MissionRunning
    
    CMP al, MISSION_STATE_COMPLETE
    JE MissionComplete
    
    CMP al, MISSION_STATE_ERROR
    JE MissionError
    
    ; ... continue ...
    JMP LoopCheckState
    
MissionComplete:
    ; Mission succeeded
    RET
    
MissionError:
    ; Mission failed
    RET
    
MissionRunning:
    ; Still executing
    ; Wait a bit and check again
    ; ... sleep/delay ...
    JMP LoopCheckState
    
MissionIdle:
    ; Not started
    RET
MonitorMission ENDP
```

### Use Case 3: Error Handling

```asm
INCLUDE masm/masm_master_include.asm

.CODE

SafeMissionExecution PROC
    LOCAL hEngine:QWORD
    
    ; Create engine
    CALL ZeroDayAgenticEngine_Create
    
    ; Check for allocation failure
    CMP rax, 0
    JE CreationFailed
    
    MOV [hEngine], rax
    
    ; Start mission
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_StartMission
    
    ; Check state for errors
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_GetMissionState
    
    CMP al, MISSION_STATE_ERROR
    JE ExecutionError
    
    ; Success path
    CALL NormalCleanup
    RET
    
CreationFailed:
    ; Handle allocation failure
    CALL HandleCreationError
    RET
    
ExecutionError:
    ; Handle mission error
    CALL HandleMissionError
    
    ; Still cleanup
    MOV rcx, [hEngine]
    CALL ZeroDayAgenticEngine_Destroy
    
    RET
SafeMissionExecution ENDP
```

---

## 🔧 Configuration

Edit constants in `masm_master_include.asm`:

```asm
; Logging level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
ACTIVE_LOG_LEVEL            EQU 1

; Auto-retry on failure
DEFAULT_AUTO_RETRY_ENABLED  EQU 1

; Mission timeout (milliseconds)
DEFAULT_MISSION_TIMEOUT_MS  EQU 30000  ; 30 seconds

; Thread stack size (bytes)
DEFAULT_THREAD_STACK_SIZE   EQU 1048576  ; 1 MB
```

---

## 🧪 Testing

### Test 1: Can I Include It?

```asm
; test_include.asm
INCLUDE masm/masm_master_include.asm

.CODE
TestProc PROC
    RET
TestProc ENDP
END
```

Compile:
```powershell
ml64.exe /c test_include.asm
# Should compile with no errors
```

### Test 2: Can I Call It?

```asm
; test_call.asm
INCLUDE masm/masm_master_include.asm

.CODE
TestCall PROC
    MOV rcx, 0              ; NULL engine pointer
    CALL ZeroDayAgenticEngine_Destroy  ; Should fail gracefully
    RET
TestCall ENDP
END
```

### Test 3: Full Build

```powershell
.\Build-MASM-Modules.ps1 -Configuration Release

# Output should show:
# [SUCCESS] MASM tools found
# [1/2] Compiling zero_day_agentic_engine.asm...
# [SUCCESS] Compiled...
# [2/2] Compiling zero_day_integration.asm...
# [SUCCESS] Compiled...
# [SUCCESS] Linked: masm_modules.lib
# [SUCCESS] Build completed successfully!
```

---

## 📖 API Reference

### Engine Functions

**Create Engine**
```asm
; Input:
;   rcx = Router pointer
;   rdx = Tools pointer
;   r8 = Planner pointer
;   r9 = Callbacks pointer
; Output:
;   rax = Engine pointer (0 if failed)
CALL ZeroDayAgenticEngine_Create
```

**Start Mission**
```asm
; Input:
;   rcx = Engine pointer
;   rdx = Router pointer
; Output:
;   rax = Mission ID string pointer
;   (NULL if failed)
CALL ZeroDayAgenticEngine_StartMission
```

**Abort Mission**
```asm
; Input:
;   rcx = Engine pointer
; Output:
;   al = BOOL (1=success, 0=failed)
CALL ZeroDayAgenticEngine_AbortMission
```

**Get Mission State**
```asm
; Input:
;   rcx = Engine pointer
; Output:
;   al = State (see constants below)
CALL ZeroDayAgenticEngine_GetMissionState
```

**Get Mission ID**
```asm
; Input:
;   rcx = Engine pointer
; Output:
;   rax = Mission ID pointer
CALL ZeroDayAgenticEngine_GetMissionId
```

**Cleanup**
```asm
; Input:
;   rcx = Engine pointer
; Output:
;   None (always succeeds)
CALL ZeroDayAgenticEngine_Destroy
```

### Constants

```asm
; Mission States
MISSION_STATE_IDLE      = 0
MISSION_STATE_RUNNING   = 1
MISSION_STATE_ABORTED   = 2
MISSION_STATE_COMPLETE  = 3
MISSION_STATE_ERROR     = 4

; Signal Types
SIGNAL_TYPE_STREAM      = 1
SIGNAL_TYPE_COMPLETE    = 2
SIGNAL_TYPE_ERROR       = 3

; Log Levels
LOG_LEVEL_DEBUG         = 0
LOG_LEVEL_INFO          = 1
LOG_LEVEL_WARN          = 2
LOG_LEVEL_ERROR         = 3
```

---

## ⚠️ Common Pitfalls

### ❌ Don't: Call Without Creating
```asm
; WRONG - engine is not initialized
CALL ZeroDayAgenticEngine_StartMission
```

### ✅ Do: Always Create First
```asm
; RIGHT - create engine first
CALL ZeroDayAgenticEngine_Create
MOV [hEngine], rax
MOV rcx, [hEngine]
CALL ZeroDayAgenticEngine_StartMission
```

### ❌ Don't: Forget to Cleanup
```asm
; WRONG - memory leak
CALL ZeroDayAgenticEngine_Create
MOV [hEngine], rax
; ... do stuff ...
; No cleanup!
```

### ✅ Do: Always Destroy
```asm
; RIGHT - proper cleanup
CALL ZeroDayAgenticEngine_Create
MOV [hEngine], rax
; ... do stuff ...
MOV rcx, [hEngine]
CALL ZeroDayAgenticEngine_Destroy
```

### ❌ Don't: Ignore Return Values
```asm
; WRONG - no error checking
CALL ZeroDayAgenticEngine_Create
; What if rax is 0?
CALL ZeroDayAgenticEngine_StartMission
```

### ✅ Do: Check Return Values
```asm
; RIGHT - error handling
CALL ZeroDayAgenticEngine_Create
CMP rax, 0
JE HandleError
MOV [hEngine], rax
; ... continue ...
```

---

## 🔍 Debugging

### Enable Debug Logging

Edit `masm_master_include.asm`:
```asm
ACTIVE_LOG_LEVEL = LOG_LEVEL_DEBUG  ; Maximum verbosity
```

Recompile:
```powershell
.\Build-MASM-Modules.ps1
```

### Check Build Output

```powershell
# View detailed build log
Get-Content bin\build_error.log
```

### Inspect Generated Code

```powershell
# Disassemble object file
dumpbin.exe /DISASM bin\masm_Release\zero_day_agentic_engine.obj | Out-File engine_asm.txt
```

### Use WinDbg

```powershell
# Debug your program
windbg.exe your_program.exe

# Set breakpoints
bp ZeroDayAgenticEngine_Create
bp ZeroDayAgenticEngine_StartMission

# Run and debug
```

---

## 📊 Performance Tips

### 1. Minimize Log Overhead
```asm
; Set to WARN or ERROR level in production
ACTIVE_LOG_LEVEL = LOG_LEVEL_ERROR
```

### 2. Reuse Engine Instances
```asm
; GOOD - Create once
CALL ZeroDayAgenticEngine_Create
MOV [hEngine], rax

; ... run multiple missions ...
MOV rcx, [hEngine]
CALL ZeroDayAgenticEngine_StartMission
; ... another mission ...
MOV rcx, [hEngine]
CALL ZeroDayAgenticEngine_StartMission

; Cleanup when done
MOV rcx, [hEngine]
CALL ZeroDayAgenticEngine_Destroy
```

### 3. Monitor Thread Count
```asm
; Engine creates one thread per mission
; Don't start too many missions simultaneously
; Use complexity analysis to route simple missions elsewhere
```

---

## ✨ Integration Checklist

- [ ] Added `INCLUDE masm/masm_master_include.asm` to your file
- [ ] Updated all function calls to use new symbols
- [ ] Verified compilation with `Build-MASM-Modules.ps1`
- [ ] Linked with `bin\masm_modules.lib`
- [ ] Tested with basic mission execution
- [ ] Added error handling for NULL returns
- [ ] Verified cleanup in all code paths
- [ ] Enabled logging for debugging
- [ ] Tested with production configuration
- [ ] Updated documentation

---

## 📞 Support Resources

**Files to Reference:**
- `MASM_ACCESSIBILITY_VERIFICATION.md` - Verify everything works
- `MASM_BUILD_INTEGRATION_GUIDE.md` - Detailed build procedures
- `ZERO_DAY_AGENTIC_ENGINE_QUICK_REFERENCE.md` - API details
- `ZERO_DAY_AGENTIC_ENGINE_IMPROVEMENTS.md` - Technical deep dive
- `zero_day_agentic_engine.asm` - Source code documentation

**Build Commands:**
```powershell
# Full build
.\Build-MASM-Modules.ps1

# Debug build
.\Build-MASM-Modules.ps1 -Configuration Debug

# Clean and rebuild
.\Build-MASM-Modules.ps1 -Clean -Configuration Release
```

---

## 🎯 Next Steps

1. **Copy the include line**: `INCLUDE masm/masm_master_include.asm`
2. **Add function calls**: Use one of the examples above
3. **Build**: `.\Build-MASM-Modules.ps1`
4. **Test**: Run your program
5. **Debug**: Use WinDbg or check logs if issues arise
6. **Deploy**: Copy `masm_modules.lib` to your project

---

**Status**: ✅ Ready to Use  
**Last Updated**: December 30, 2025

