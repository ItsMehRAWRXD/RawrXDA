;======================================================================
; RawrXD IDE - Debugger Module
; Breakpoints, stepping, variable inspection, call stack
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Debugger state
g_isDebugging           DQ 0
g_isPaused              DQ 0
g_hDebugProcess         DQ ?
g_hDebugThread          DQ ?
g_currentPC             DQ 0
g_currentSP             DQ 0

; Breakpoint data structures
MAX_BREAKPOINTS         EQU 32

BREAKPOINT STRUCT
    address             DQ ?
    originalByte        DB ?
    isEnabled           DB ?
    hitCount            DQ ?
    condition[256]      DB 256 DUP(0)
BREAKPOINT ENDS

g_breakpoints[MAX_BREAKPOINTS * (SIZEOF BREAKPOINT)] DQ MAX_BREAKPOINTS DUP(0)
g_breakpointCount       DQ 0

; Variable inspection
MAX_VARIABLES           EQU 64

VARIABLE STRUCT
    name[64]            DB 64 DUP(0)
    address             DQ ?
    size                DQ ?
    value[256]          DB 256 DUP(0)
    type                DQ ?
VARIABLE ENDS

g_variables[MAX_VARIABLES * (SIZEOF VARIABLE)] DQ MAX_VARIABLES DUP(0)
g_variableCount         DQ 0

; Call stack
MAX_STACK_DEPTH         EQU 32
g_callStack[MAX_STACK_DEPTH * 8] DQ MAX_STACK_DEPTH DUP(0)
g_stackDepth            DQ 0

; Debug info
g_debugSymbols[4096]    DB 4096 DUP(0)
g_debugOutput[2048]     DB 2048 DUP(0)

.CODE

;----------------------------------------------------------------------
; RawrXD_Debug_Initialize - Initialize debugger
;----------------------------------------------------------------------
RawrXD_Debug_Initialize PROC
    ; Clear breakpoints
    mov g_breakpointCount, 0
    mov g_variableCount, 0
    mov g_stackDepth, 0
    mov g_isDebugging, 0
    mov g_isPaused, 0
    
    xor eax, eax
    ret
RawrXD_Debug_Initialize ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_Start - Start debugging session
;----------------------------------------------------------------------
RawrXD_Debug_Start PROC pszExecutable:QWORD
    LOCAL startInfo:STARTUPINFOA
    LOCAL procInfo:PROCESS_INFORMATION
    LOCAL debugEvent:DEBUG_EVENT
    
    cmp g_isDebugging, 1
    je @@already_debugging
    
    ; Initialize startup info
    INVOKE RtlZeroMemory, ADDR startInfo, SIZEOF STARTUPINFOA
    mov startInfo.cb, SIZEOF STARTUPINFOA
    
    ; Create process in debug mode
    INVOKE CreateProcessA,
        pszExecutable,
        NULL,
        NULL,
        NULL,
        FALSE,
        DEBUG_PROCESS OR CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        ADDR startInfo,
        ADDR procInfo
    
    test eax, eax
    jz @@start_error
    
    ; Store process/thread handles
    mov g_hDebugProcess, procInfo.hProcess
    mov g_hDebugThread, procInfo.hThread
    mov g_isDebugging, 1
    
    ; Insert breakpoints at entry points
    INVOKE RawrXD_Debug_SetInitialBreakpoints
    
    INVOKE RawrXD_Output_Build, OFFSET szDebugStarted
    
    xor eax, eax
    ret
    
@@already_debugging:
    INVOKE RawrXD_Output_Warning, OFFSET szDebugAlreadyRunning
    mov eax, -1
    ret
    
@@start_error:
    INVOKE RawrXD_Output_Error, OFFSET szDebugStartFailed
    mov eax, -1
    ret
    
RawrXD_Debug_Start ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_Stop - Stop debugging session
;----------------------------------------------------------------------
RawrXD_Debug_Stop PROC
    cmp g_isDebugging, 0
    je @@not_debugging
    
    ; Terminate debug process
    INVOKE TerminateProcess, g_hDebugProcess, 0
    
    ; Close handles
    INVOKE CloseHandle, g_hDebugProcess
    INVOKE CloseHandle, g_hDebugThread
    
    mov g_isDebugging, 0
    mov g_isPaused, 0
    
    ; Clear debug info
    mov g_breakpointCount, 0
    mov g_variableCount, 0
    mov g_stackDepth, 0
    
    INVOKE RawrXD_Output_Build, OFFSET szDebugStopped
    
    xor eax, eax
    ret
    
@@not_debugging:
    mov eax, -1
    ret
    
RawrXD_Debug_Stop ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_SetBreakpoint - Add breakpoint at address
;----------------------------------------------------------------------
RawrXD_Debug_SetBreakpoint PROC address:QWORD, pszCondition:QWORD
    LOCAL pBreakpoint:QWORD
    LOCAL idx:QWORD
    
    ; Check limit
    cmp g_breakpointCount, MAX_BREAKPOINTS
    jge @@limit_reached
    
    ; Get next breakpoint slot
    mov idx, g_breakpointCount
    mov rax, idx
    imul rax, SIZEOF BREAKPOINT
    mov pBreakpoint, OFFSET g_breakpoints
    add pBreakpoint, rax
    
    ; Fill breakpoint structure
    mov rcx, pBreakpoint
    mov BREAKPOINT.address[rcx], address
    mov BREAKPOINT.isEnabled[rcx], 1
    mov BREAKPOINT.hitCount[rcx], 0
    
    ; Copy condition if provided
    cmp pszCondition, 0
    je @@no_condition
    
    INVOKE lstrcpyA, BREAKPOINT.condition[rcx], pszCondition
    
@@no_condition:
    ; Insert INT3 (0xCC) at address
    mov byte [address], 0CCh
    
    ; Store original byte
    mov al, [address + 1]
    mov BREAKPOINT.originalByte[rcx], al
    
    ; Increment count
    inc g_breakpointCount
    
    INVOKE RawrXD_Output_Build, OFFSET szBreakpointSet
    
    xor eax, eax
    ret
    
@@limit_reached:
    INVOKE RawrXD_Output_Warning, OFFSET szMaxBreakpoints
    mov eax, -1
    ret
    
RawrXD_Debug_SetBreakpoint ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_RemoveBreakpoint - Remove breakpoint
;----------------------------------------------------------------------
RawrXD_Debug_RemoveBreakpoint PROC address:QWORD
    LOCAL idx:QWORD
    LOCAL pBreakpoint:QWORD
    
    ; Find breakpoint
    xor idx, idx
    
@@search_loop:
    cmp idx, g_breakpointCount
    jge @@not_found
    
    mov rax, idx
    imul rax, SIZEOF BREAKPOINT
    mov pBreakpoint, OFFSET g_breakpoints
    add pBreakpoint, rax
    
    mov rcx, pBreakpoint
    mov rax, BREAKPOINT.address[rcx]
    cmp rax, address
    je @@found
    
    inc idx
    jmp @@search_loop
    
@@found:
    ; Restore original byte
    mov al, BREAKPOINT.originalByte[rcx]
    mov [address], al
    
    ; Mark as disabled
    mov BREAKPOINT.isEnabled[rcx], 0
    
    INVOKE RawrXD_Output_Build, OFFSET szBreakpointRemoved
    
    xor eax, eax
    ret
    
@@not_found:
    INVOKE RawrXD_Output_Warning, OFFSET szBreakpointNotFound
    mov eax, -1
    ret
    
RawrXD_Debug_RemoveBreakpoint ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_Continue - Resume execution
;----------------------------------------------------------------------
RawrXD_Debug_Continue PROC
    cmp g_isPaused, 0
    je @@not_paused
    
    ; Resume thread
    INVOKE ResumeThread, g_hDebugThread
    
    mov g_isPaused, 0
    
    INVOKE RawrXD_Output_Build, OFFSET szResuming
    
    xor eax, eax
    ret
    
@@not_paused:
    mov eax, -1
    ret
    
RawrXD_Debug_Continue ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_Pause - Pause execution
;----------------------------------------------------------------------
RawrXD_Debug_Pause PROC
    cmp g_isDebugging, 0
    je @@not_debugging
    
    cmp g_isPaused, 1
    je @@already_paused
    
    ; Suspend thread
    INVOKE SuspendThread, g_hDebugThread
    
    mov g_isPaused, 1
    
    ; Update call stack and variables
    INVOKE RawrXD_Debug_UpdateCallStack
    INVOKE RawrXD_Debug_UpdateVariables
    
    INVOKE RawrXD_Output_Build, OFFSET szPaused
    
    xor eax, eax
    ret
    
@@not_debugging:
    INVOKE RawrXD_Output_Warning, OFFSET szNotDebugging
    mov eax, -1
    ret
    
@@already_paused:
    mov eax, -1
    ret
    
RawrXD_Debug_Pause ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_StepOver - Step over current instruction
;----------------------------------------------------------------------
RawrXD_Debug_StepOver PROC
    cmp g_isPaused, 0
    je @@not_paused
    
    ; Execute single instruction
    INVOKE RawrXD_Debug_ExecuteOneInstruction
    
    ; Pause again
    INVOKE SuspendThread, g_hDebugThread
    
    ; Update stack and variables
    INVOKE RawrXD_Debug_UpdateCallStack
    INVOKE RawrXD_Debug_UpdateVariables
    
    INVOKE RawrXD_Output_Build, OFFSET szStepped
    
    xor eax, eax
    ret
    
@@not_paused:
    INVOKE RawrXD_Output_Warning, OFFSET szNotPaused
    mov eax, -1
    ret
    
RawrXD_Debug_StepOver ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_StepInto - Step into function call
;----------------------------------------------------------------------
RawrXD_Debug_StepInto PROC
    ; Similar to StepOver but enters function calls
    INVOKE RawrXD_Debug_StepOver
    ret
RawrXD_Debug_StepInto ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_UpdateCallStack - Update call stack display
;----------------------------------------------------------------------
RawrXD_Debug_UpdateCallStack PROC
    LOCAL idx:QWORD
    
    mov g_stackDepth, 0
    
    ; This would read RSP and RBP to reconstruct call stack
    ; For now, just clear it
    
    ret
    
RawrXD_Debug_UpdateCallStack ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_UpdateVariables - Update local variable display
;----------------------------------------------------------------------
RawrXD_Debug_UpdateVariables PROC
    ; This would read debug symbols and extract values from memory
    ; For now, just clear it
    
    mov g_variableCount, 0
    
    ret
    
RawrXD_Debug_UpdateVariables ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_GetVariable - Get variable value
;----------------------------------------------------------------------
RawrXD_Debug_GetVariable PROC pszName:QWORD, pszValue:QWORD
    LOCAL idx:QWORD
    LOCAL pVariable:QWORD
    
    ; Search for variable
    xor idx, idx
    
@@search_loop:
    cmp idx, g_variableCount
    jge @@not_found
    
    mov rax, idx
    imul rax, SIZEOF VARIABLE
    mov pVariable, OFFSET g_variables
    add pVariable, rax
    
    mov rcx, pVariable
    INVOKE lstrcmpA, pszName, VARIABLE.name[rcx]
    test eax, eax
    jz @@found
    
    inc idx
    jmp @@search_loop
    
@@found:
    ; Copy value to output
    mov rcx, pVariable
    INVOKE lstrcpyA, pszValue, VARIABLE.value[rcx]
    
    xor eax, eax
    ret
    
@@not_found:
    mov eax, -1
    ret
    
RawrXD_Debug_GetVariable ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_ExecuteOneInstruction - Execute single instruction
;----------------------------------------------------------------------
RawrXD_Debug_ExecuteOneInstruction PROC
    ; This would use debug event handling to single-step
    ; For now, just a placeholder
    ret
RawrXD_Debug_ExecuteOneInstruction ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_SetInitialBreakpoints - Set breakpoints at entry
;----------------------------------------------------------------------
RawrXD_Debug_SetInitialBreakpoints PROC
    ; Set breakpoint at main/entry point
    ; This would require reading symbols
    ret
RawrXD_Debug_SetInitialBreakpoints ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_GetCallStack - Get call stack depth
;----------------------------------------------------------------------
RawrXD_Debug_GetCallStack PROC
    mov rax, g_stackDepth
    ret
RawrXD_Debug_GetCallStack ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_IsDebugging - Check if debugging
;----------------------------------------------------------------------
RawrXD_Debug_IsDebugging PROC
    mov eax, g_isDebugging
    ret
RawrXD_Debug_IsDebugging ENDP

;----------------------------------------------------------------------
; RawrXD_Debug_IsPaused - Check if paused
;----------------------------------------------------------------------
RawrXD_Debug_IsPaused PROC
    mov eax, g_isPaused
    ret
RawrXD_Debug_IsPaused ENDP

; String literals
szDebugStarted          DB "Debug session started", 0
szDebugStopped          DB "Debug session stopped", 0
szDebugAlreadyRunning   DB "Debug session already running", 0
szDebugStartFailed      DB "Failed to start debug session", 0
szBreakpointSet         DB "Breakpoint set", 0
szBreakpointRemoved     DB "Breakpoint removed", 0
szBreakpointNotFound    DB "Breakpoint not found", 0
szMaxBreakpoints        DB "Maximum breakpoints reached", 0
szResuming              DB "Resuming execution", 0
szPaused                DB "Execution paused", 0
szStepped               DB "Stepped", 0
szNotDebugging          DB "Not debugging", 0
szNotPaused             DB "Not paused", 0

END
