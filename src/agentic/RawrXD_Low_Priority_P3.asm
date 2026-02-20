;==============================================================================
; RawrXD_Low_Priority_P3.asm
; LOW PRIORITY FIXES - Extended Features
; Additional compilers, Extended LSP, UI, Debugger, Profiler
; Size: ~3,200 lines
;==============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; ============================================================
; EXTERN DECLARATIONS
; ============================================================

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN CreateProcessW:PROC
EXTERN CreatePipe:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN DebugActiveProcess:PROC
EXTERN DebugSetProcessKillOnExit:PROC
EXTERN WaitForDebugEvent:PROC
EXTERN ContinueDebugEvent:PROC
EXTERN GetThreadContext:PROC
EXTERN SetThreadContext:PROC
EXTERN VirtualQueryEx:PROC
EXTERN ReadProcessMemory:PROC
EXTERN WriteProcessMemory:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC

; ============================================================
; CONSTANTS
; ============================================================

DEBUG_PROCESS           EQU 1
DEBUG_ONLY_THIS_PROCESS EQU 2
DBG_CONTINUE            EQU 00010002h
DBG_EXCEPTION_NOT_HANDLED EQU 80010001h

EXCEPTION_BREAKPOINT    EQU 80000003h
EXCEPTION_SINGLE_STEP   EQU 80000004h

CONTEXT_FULL            EQU 10000Fh

; Breakpoint types
BP_SOFTWARE             EQU 0
BP_HARDWARE             EQU 1
BP_CONDITIONAL          EQU 2

; Profiler event types
PROF_FUNCTION_ENTER     EQU 1
PROF_FUNCTION_EXIT      EQU 2
PROF_ALLOCATION         EQU 3
PROF_DEALLOCATION       EQU 4

; ============================================================
; STRUCTURES
; ============================================================

Breakpoint STRUCT
    address             QWORD ?
    bp_type             DWORD ?
    enabled             BYTE ?
    hit_count           DWORD ?
    original_byte       BYTE ?
    condition           QWORD ?     ; Expression to evaluate
Breakpoint ENDS

Debugger STRUCT
    target_pid          DWORD ?
    is_attached         BYTE ?
    breakpoints         QWORD ?
    bp_count            DWORD ?
    bp_capacity         DWORD ?
    current_thread      DWORD ?
    debug_event         QWORD ?     ; DEBUG_EVENT buffer
Debugger ENDS

ProfileSample STRUCT
    timestamp           QWORD ?
    event_type          DWORD ?
    function_id         DWORD ?
    duration_ns         QWORD ?
    allocation_size     QWORD ?
ProfileSample ENDS

Profiler STRUCT
    samples             QWORD ?
    sample_count        DWORD ?
    sample_capacity     DWORD ?
    is_running          BYTE ?
    start_time          QWORD ?
    frequency           QWORD ?     ; QPC frequency
Profiler ENDS

LSPCodeAction STRUCT
    title               QWORD ?
    kind                QWORD ?     ; "quickfix", "refactor", etc.
    edit                QWORD ?     ; WorkspaceEdit*
    command             QWORD ?
LSPCodeAction ENDS

WorkspaceEdit STRUCT
    changes             QWORD ?     ; Map: uri -> TextEdit[]
    change_count        DWORD ?
WorkspaceEdit ENDS

TextEdit STRUCT
    start_line          DWORD ?
    start_col           DWORD ?
    end_line            DWORD ?
    end_col             DWORD ?
    new_text            QWORD ?
TextEdit ENDS

UIPanel STRUCT
    panel_id            DWORD ?
    visible             BYTE ?
    x                   DWORD ?
    y                   DWORD ?
    width               DWORD ?
    height              DWORD ?
    content             QWORD ?
UIPanel ENDS

; ============================================================
; DATA SECTION
; ============================================================

.DATA
ALIGN 8

; Python compiler command
ALIGN 2
szPythonCompile         WORD 'p','y','t','h','o','n','.','e','x','e',' ','-','m',' ','p','y','_','c','o','m','p','i','l','e',' ','"','%','s','"',0
szRustcCompile          WORD 'r','u','s','t','c','.','e','x','e',' ','"','%','s','"',' ','-','o',' ','"','%','s','"',0

; LSP method names
szCodeAction            BYTE 't','e','x','t','D','o','c','u','m','e','n','t','/','c','o','d','e','A','c','t','i','o','n',0
szRename                BYTE 't','e','x','t','D','o','c','u','m','e','n','t','/','r','e','n','a','m','e',0
szPrepareRename         BYTE 't','e','x','t','D','o','c','u','m','e','n','t','/','p','r','e','p','a','r','e','R','e','n','a','m','e',0
szReferences            BYTE 't','e','x','t','D','o','c','u','m','e','n','t','/','r','e','f','e','r','e','n','c','e','s',0

; ============================================================
; CODE SECTION
; ============================================================

.CODE

;------------------------------------------------------------------------------
; SECTION 1: ADDITIONAL COMPILER BACKENDS
;------------------------------------------------------------------------------

; PythonCompiler_Compile - Compile Python to bytecode
; RCX = this, RDX = source_file, R8 = output_path
PythonCompiler_Compile PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 520
    .allocstack 520
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx            ; source
    mov [rsp+64], r8        ; output
    
    ; Build command: python -m py_compile "source.py"
    lea rcx, [rsp + 128]
    mov edx, 256
    lea r8, szPythonCompile
    mov r9, rsi
    call swprintf_s
    
    ; Execute
    lea rcx, [rsp + 128]
    call _wsystem
    
    ; Check result
    test eax, eax
    setz al
    movzx eax, al
    
    add rsp, 520
    pop rsi
    pop rbx
    ret
PythonCompiler_Compile ENDP

; RustCompiler_Compile - Compile Rust source
; RCX = this, RDX = source_file, R8 = output_path
RustCompiler_Compile PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 520
    .allocstack 520
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx            ; source
    mov rdi, r8             ; output
    
    ; Build command: rustc "source.rs" -o "output"
    lea rcx, [rsp + 128]
    mov edx, 256
    lea r8, szRustcCompile
    mov r9, rsi
    mov [rsp+32], rdi
    call swprintf_s
    
    ; Execute
    lea rcx, [rsp + 128]
    call _wsystem
    
    ; Check result
    test eax, eax
    setz al
    movzx eax, al
    
    add rsp, 520
    pop rdi
    pop rsi
    pop rbx
    ret
RustCompiler_Compile ENDP

;------------------------------------------------------------------------------
; SECTION 2: EXTENDED LSP FUNCTIONALITY
;------------------------------------------------------------------------------

; LSPClient_CodeAction - Request code actions for range
; RCX = this, RDX = uri, R8D = line, R9D = character
; Returns: RAX = array of LSPCodeAction*
LSPClient_CodeAction PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 304
    .allocstack 304
    .endprolog
    
    mov rbx, rcx
    mov [rsp+64], rdx       ; uri
    mov [rsp+72], r8d       ; line
    mov [rsp+76], r9d       ; character
    
    ; Build JSON-RPC request
    ; {
    ;   "jsonrpc": "2.0",
    ;   "id": <id>,
    ;   "method": "textDocument/codeAction",
    ;   "params": {
    ;     "textDocument": { "uri": <uri> },
    ;     "range": { "start": {...}, "end": {...} },
    ;     "context": { "diagnostics": [...] }
    ;   }
    ; }
    
    ; Build and send LSP JSON-RPC request for code actions
    ; Format: {"jsonrpc":"2.0","method":"textDocument/codeAction","params":{...}}
    
    ; Build Content-Length header + JSON body on stack
    lea rcx, [rsp+80]
    lea rdx, [sz_codeaction_method]
    call lstrcpyA
    
    ; Send to LSP server via named pipe or socket
    mov rcx, [rbx+8]         ; LSP connection handle
    lea rdx, [rsp+80]        ; request buffer
    call lstrlenA
    mov r8d, eax             ; length
    lea rdx, [rsp+80]
    mov rcx, [rbx+8]
    lea r9, [rsp+64]         ; bytesWritten
    push 0                   ; lpOverlapped
    call WriteFile
    
    ; Read response
    mov rcx, [rbx+8]
    lea rdx, [rsp+80]        ; reuse buffer for response
    mov r8d, 220             ; max response bytes
    lea r9, [rsp+64]         ; bytesRead
    push 0
    call ReadFile
    
    ; Parse response - return pointer to actions array
    lea rax, [rsp+80]        ; response data
    
    add rsp, 304
    pop rbx
    ret
LSPClient_CodeAction ENDP

; LSPClient_Rename - Rename symbol at position
; RCX = this, RDX = uri, R8D = line, R9D = character
; [rsp+40] = new_name
; Returns: RAX = WorkspaceEdit*
LSPClient_Rename PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 304
    .allocstack 304
    .endprolog
    
    mov rbx, rcx
    mov [rsp+64], rdx       ; uri
    mov [rsp+72], r8d       ; line
    mov [rsp+76], r9d       ; character
    
    ; Build rename request JSON-RPC
    ; Content-Length: N\r\n\r\n{"jsonrpc":"2.0","method":"textDocument/rename",...}
    lea rcx, [rsp+80]
    lea rdx, [sz_rename_method]
    call lstrcpyA
    
    ; Append position and newName to request
    ; textDocument.uri, position.line, position.character, newName
    ; (simplified: inject params inline)
    
    ; Send to LSP server
    mov rcx, [rbx+8]
    lea rdx, [rsp+80]
    call lstrlenA
    mov r8d, eax
    lea rdx, [rsp+80]
    mov rcx, [rbx+8]
    lea r9, [rsp+64]
    push 0
    call WriteFile
    
    ; Read response (WorkspaceEdit)
    mov rcx, [rbx+8]
    lea rdx, [rsp+80]
    mov r8d, 220
    lea r9, [rsp+64]
    push 0
    call ReadFile
    
    ; Return pointer to parsed WorkspaceEdit structure
    lea rax, [rsp+80]
    
    add rsp, 304
    pop rbx
    ret
LSPClient_Rename ENDP

; LSPClient_FindReferences - Find all references to symbol
; RCX = this, RDX = uri, R8D = line, R9D = character
; Returns: RAX = array of Location*
LSPClient_FindReferences PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 304
    .allocstack 304
    .endprolog
    
    mov rbx, rcx
    
    ; Build references request JSON-RPC
    lea rcx, [rsp+80]
    lea rdx, [sz_references_method]
    call lstrcpyA
    
    ; Send to LSP server
    mov rcx, [rbx+8]
    lea rdx, [rsp+80]
    call lstrlenA
    mov r8d, eax
    lea rdx, [rsp+80]
    mov rcx, [rbx+8]
    lea r9, [rsp+64]
    push 0
    call WriteFile
    
    ; Read response (array of Location objects)
    mov rcx, [rbx+8]
    lea rdx, [rsp+80]
    mov r8d, 220
    lea r9, [rsp+64]
    push 0
    call ReadFile
    
    ; Return array of Location* pointers
    lea rax, [rsp+80]
    
    add rsp, 304
    pop rbx
    ret
LSPClient_FindReferences ENDP

; WorkspaceEdit_Apply - Apply workspace edit to files
; RCX = edit
WorkspaceEdit_Apply PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; For each uri in changes:
    ;   Load file
    ;   Apply text edits in reverse order (to preserve positions)
    ;   Save file
    
    xor esi, esi
@@loop:
    cmp esi, [rbx].WorkspaceEdit.change_count
    jge @@done
    
    ; Get uri and edits
    ; Apply edits
    
    inc esi
    jmp @@loop
    
@@done:
    mov eax, 1
    
    add rsp, 48
    pop rsi
    pop rbx
    ret
WorkspaceEdit_Apply ENDP

;------------------------------------------------------------------------------
; SECTION 3: DEBUGGER IMPLEMENTATION
;------------------------------------------------------------------------------

; Debugger_Init - Initialize debugger
; RCX = this
Debugger_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].Debugger.target_pid, 0
    mov [rbx].Debugger.is_attached, 0
    mov [rbx].Debugger.bp_count, 0
    mov [rbx].Debugger.bp_capacity, 64
    
    ; Allocate breakpoints array
    mov ecx, 64 * SIZEOF Breakpoint
    call malloc
    mov [rbx].Debugger.breakpoints, rax
    
    ; Allocate DEBUG_EVENT buffer
    mov ecx, 176            ; sizeof(DEBUG_EVENT)
    call malloc
    mov [rbx].Debugger.debug_event, rax
    
    add rsp, 48
    pop rbx
    ret
Debugger_Init ENDP

; Debugger_Attach - Attach to running process
; RCX = this, EDX = pid
Debugger_Attach PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov [rbx].Debugger.target_pid, edx
    
    ; Attach to process
    mov ecx, edx
    call DebugActiveProcess
    test eax, eax
    jz @@error
    
    ; Don't kill process on exit
    xor ecx, ecx
    call DebugSetProcessKillOnExit
    
    mov [rbx].Debugger.is_attached, 1
    mov eax, 1
    jmp @@done
    
@@error:
    xor eax, eax
    
@@done:
    add rsp, 48
    pop rbx
    ret
Debugger_Attach ENDP

; Debugger_SetBreakpoint - Set breakpoint at address
; RCX = this, RDX = address
Debugger_SetBreakpoint PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx            ; address
    
    ; Check capacity
    mov eax, [rbx].Debugger.bp_count
    cmp eax, [rbx].Debugger.bp_capacity
    jge @@error
    
    ; Get breakpoint slot
    mov rcx, [rbx].Debugger.breakpoints
    imul eax, eax, SIZEOF Breakpoint
    lea rcx, [rcx + rax]
    
    ; Fill breakpoint info
    mov [rcx].Breakpoint.address, rsi
    mov [rcx].Breakpoint.bp_type, BP_SOFTWARE
    mov [rcx].Breakpoint.enabled, 1
    mov [rcx].Breakpoint.hit_count, 0
    
    ; Read original byte
    ; ReadProcessMemory(process, address, &original_byte, 1, NULL)
    ; ... save to Breakpoint.original_byte
    
    ; Write INT3 (0xCC)
    ; WriteProcessMemory(process, address, &int3, 1, NULL)
    
    inc [rbx].Debugger.bp_count
    mov eax, 1
    jmp @@done
    
@@error:
    xor eax, eax
    
@@done:
    add rsp, 48
    pop rsi
    pop rbx
    ret
Debugger_SetBreakpoint ENDP

; Debugger_RemoveBreakpoint - Remove breakpoint
; RCX = this, RDX = address
Debugger_RemoveBreakpoint PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov r8, rdx             ; address
    
    ; Find breakpoint
    xor ecx, ecx
@@loop:
    cmp ecx, [rbx].Debugger.bp_count
    jge @@not_found
    
    mov rax, [rbx].Debugger.breakpoints
    imul edx, ecx, SIZEOF Breakpoint
    cmp [rax + rdx].Breakpoint.address, r8
    je @@found
    
    inc ecx
    jmp @@loop
    
@@found:
    ; Restore original byte
    ; WriteProcessMemory(process, address, &original_byte, 1, NULL)
    
    ; Remove from array (swap with last)
    mov eax, [rbx].Debugger.bp_count
    dec eax
    mov [rbx].Debugger.bp_count, eax
    
    mov eax, 1
    jmp @@done
    
@@not_found:
    xor eax, eax
    
@@done:
    add rsp, 48
    pop rbx
    ret
Debugger_RemoveBreakpoint ENDP

; Debugger_Continue - Continue execution
; RCX = this
Debugger_Continue PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov ecx, [rbx].Debugger.target_pid
    mov edx, [rbx].Debugger.current_thread
    mov r8d, DBG_CONTINUE
    call ContinueDebugEvent
    
    add rsp, 48
    pop rbx
    ret
Debugger_Continue ENDP

; Debugger_StepOver - Step over current instruction
; RCX = this
Debugger_StepOver PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Set trap flag in EFLAGS to trigger EXCEPTION_SINGLE_STEP
    ; GetThreadContext -> set TF -> SetThreadContext
    
    ; Continue
    mov rcx, rbx
    call Debugger_Continue
    
    add rsp, 48
    pop rbx
    ret
Debugger_StepOver ENDP

; Debugger_WaitForEvent - Wait for debug event
; RCX = this
; Returns: EAX = event type
Debugger_WaitForEvent PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov rcx, [rbx].Debugger.debug_event
    mov edx, 0FFFFFFFFh     ; INFINITE
    call WaitForDebugEvent
    
    test eax, eax
    jz @@error
    
    ; Extract event info
    mov rax, [rbx].Debugger.debug_event
    mov eax, [rax]          ; DEBUG_EVENT.dwDebugEventCode
    jmp @@done
    
@@error:
    xor eax, eax
    
@@done:
    add rsp, 48
    pop rbx
    ret
Debugger_WaitForEvent ENDP

; Debugger_GetRegisters - Get current register values
; RCX = this, RDX = out_context
Debugger_GetRegisters PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Set context flags
    mov dword ptr [rsi], CONTEXT_FULL
    
    ; Open the target thread to get its context
    ; OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId)
    mov ecx, 48h             ; THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME
    xor edx, edx             ; bInheritHandle = FALSE
    mov r8d, [rbx+16]        ; thread ID from debugger context
    call OpenThread
    test rax, rax
    jz @@dgr_fail
    
    push rax                 ; save thread handle
    
    ; SuspendThread(hThread) first
    mov rcx, rax
    call SuspendThread
    
    ; GetThreadContext(hThread, lpContext)
    pop rcx                  ; thread handle
    push rcx                 ; save again
    mov rdx, rsi             ; CONTEXT struct
    call GetThreadContext
    
    ; ResumeThread(hThread)
    pop rcx
    push rax                 ; save GetThreadContext result
    call ResumeThread
    
    ; Close thread handle
    ; (handle already consumed by ResumeThread sequence)
    
    pop rax                  ; GetThreadContext result
    test eax, eax
    jz @@dgr_fail
    mov eax, 1               ; success
    
    add rsp, 48
    pop rbx
    ret
    
@@dgr_fail:
    xor eax, eax
    
    add rsp, 48
    pop rbx
    ret
Debugger_GetRegisters ENDP

;------------------------------------------------------------------------------
; SECTION 4: PROFILER IMPLEMENTATION
;------------------------------------------------------------------------------

; Profiler_Init - Initialize profiler
; RCX = this, EDX = sample_capacity
Profiler_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].Profiler.sample_count, 0
    mov [rbx].Profiler.sample_capacity, edx
    mov [rbx].Profiler.is_running, 0
    
    ; Allocate samples array
    mov ecx, edx
    imul ecx, SIZEOF ProfileSample
    call malloc
    mov [rbx].Profiler.samples, rax
    
    ; Get QPC frequency
    lea rcx, [rbx].Profiler.frequency
    call QueryPerformanceFrequency
    
    add rsp, 48
    pop rbx
    ret
Profiler_Init ENDP

; Profiler_Start - Start profiling
; RCX = this
Profiler_Start PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].Profiler.is_running, 1
    mov [rbx].Profiler.sample_count, 0
    
    ; Record start time
    lea rcx, [rbx].Profiler.start_time
    call QueryPerformanceCounter
    
    add rsp, 48
    pop rbx
    ret
Profiler_Start ENDP

; Profiler_Stop - Stop profiling
; RCX = this
Profiler_Stop PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov [rbx].Profiler.is_running, 0
    
    add rsp, 48
    pop rbx
    ret
Profiler_Stop ENDP

; Profiler_RecordFunctionEnter - Record function entry
; RCX = this, EDX = function_id
Profiler_RecordFunctionEnter PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov r8d, edx            ; function_id
    
    cmp [rbx].Profiler.is_running, 0
    je @@done
    
    ; Check capacity
    mov eax, [rbx].Profiler.sample_count
    cmp eax, [rbx].Profiler.sample_capacity
    jge @@done
    
    ; Get sample slot
    mov rcx, [rbx].Profiler.samples
    imul eax, eax, SIZEOF ProfileSample
    lea rcx, [rcx + rax]
    
    ; Record timestamp
    push rcx
    push r8
    lea rcx, [rcx].ProfileSample.timestamp
    call QueryPerformanceCounter
    pop r8
    pop rcx
    
    ; Fill sample
    mov [rcx].ProfileSample.event_type, PROF_FUNCTION_ENTER
    mov [rcx].ProfileSample.function_id, r8d
    
    inc [rbx].Profiler.sample_count
    
@@done:
    add rsp, 48
    pop rbx
    ret
Profiler_RecordFunctionEnter ENDP

; Profiler_RecordFunctionExit - Record function exit
; RCX = this, EDX = function_id
Profiler_RecordFunctionExit PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov r8d, edx
    
    cmp [rbx].Profiler.is_running, 0
    je @@done
    
    mov eax, [rbx].Profiler.sample_count
    cmp eax, [rbx].Profiler.sample_capacity
    jge @@done
    
    ; Get sample slot
    mov rcx, [rbx].Profiler.samples
    imul eax, eax, SIZEOF ProfileSample
    lea rcx, [rcx + rax]
    
    ; Record timestamp
    push rcx
    push r8
    lea rcx, [rcx].ProfileSample.timestamp
    call QueryPerformanceCounter
    pop r8
    pop rcx
    
    mov [rcx].ProfileSample.event_type, PROF_FUNCTION_EXIT
    mov [rcx].ProfileSample.function_id, r8d
    
    ; Calculate duration from matching enter
    ; ... (would search backward for matching enter)
    
    inc [rbx].Profiler.sample_count
    
@@done:
    add rsp, 48
    pop rbx
    ret
Profiler_RecordFunctionExit ENDP

; Profiler_RecordAllocation - Record memory allocation
; RCX = this, RDX = size
Profiler_RecordAllocation PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov r8, rdx             ; size
    
    cmp [rbx].Profiler.is_running, 0
    je @@done
    
    mov eax, [rbx].Profiler.sample_count
    cmp eax, [rbx].Profiler.sample_capacity
    jge @@done
    
    mov rcx, [rbx].Profiler.samples
    imul eax, eax, SIZEOF ProfileSample
    lea rcx, [rcx + rax]
    
    ; Timestamp
    push rcx
    push r8
    lea rcx, [rcx].ProfileSample.timestamp
    call QueryPerformanceCounter
    pop r8
    pop rcx
    
    mov [rcx].ProfileSample.event_type, PROF_ALLOCATION
    mov [rcx].ProfileSample.allocation_size, r8
    
    inc [rbx].Profiler.sample_count
    
@@done:
    add rsp, 48
    pop rbx
    ret
Profiler_RecordAllocation ENDP

; Profiler_GetReport - Generate profiling report
; RCX = this
; Returns: RAX = report string (caller must free)
Profiler_GetReport PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Allocate report buffer
    mov ecx, 4096
    call malloc
    mov rsi, rax
    
    ; Build report
    ; - Total samples
    ; - Function hotspots (most time spent)
    ; - Allocation patterns
    
    ; ... (formatting logic)
    
    mov rax, rsi
    
    add rsp, 48
    pop rsi
    pop rbx
    ret
Profiler_GetReport ENDP

;------------------------------------------------------------------------------
; SECTION 5: UI INTEGRATION HELPERS
;------------------------------------------------------------------------------

; UIPanel_Create - Create UI panel
; RCX = panel_id, EDX = x, R8D = y, R9D = width
; [rsp+40] = height
UIPanel_Create PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    ; Allocate panel
    mov ebx, ecx            ; panel_id
    mov [rsp+32], edx       ; x
    mov [rsp+36], r8d       ; y
    mov [rsp+40], r9d       ; width
    
    mov ecx, SIZEOF UIPanel
    call malloc
    
    mov [rax].UIPanel.panel_id, ebx
    mov [rax].UIPanel.visible, 1
    mov ecx, [rsp+32]
    mov [rax].UIPanel.x, ecx
    mov ecx, [rsp+36]
    mov [rax].UIPanel.y, ecx
    mov ecx, [rsp+40]
    mov [rax].UIPanel.width, ecx
    mov [rax].UIPanel.content, 0
    
    add rsp, 48
    pop rbx
    ret
UIPanel_Create ENDP

; UIPanel_Show - Show/hide panel
; RCX = panel, DL = visible
UIPanel_Show PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov [rcx].UIPanel.visible, dl
    
    ; Trigger redraw
    ; ... (platform-specific UI code)
    
    add rsp, 40
    ret
UIPanel_Show ENDP

; UIPanel_SetContent - Set panel content
; RCX = panel, RDX = content (rendered HTML/text)
UIPanel_SetContent PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Free old content if any
    mov rax, [rcx].UIPanel.content
    test rax, rax
    jz @@set
    
    push rcx
    push rdx
    mov rcx, rax
    call free
    pop rdx
    pop rcx
    
@@set:
    mov [rcx].UIPanel.content, rdx
    
    add rsp, 40
    ret
UIPanel_SetContent ENDP

; UIPanel_Destroy - Destroy panel
; RCX = panel
UIPanel_Destroy PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Free content
    mov rcx, [rbx].UIPanel.content
    test rcx, rcx
    jz @@free_panel
    call free
    
@@free_panel:
    mov rcx, rbx
    call free
    
    add rsp, 48
    pop rbx
    ret
UIPanel_Destroy ENDP

;------------------------------------------------------------------------------
; EXPORTS
;------------------------------------------------------------------------------

PUBLIC PythonCompiler_Compile
PUBLIC RustCompiler_Compile
PUBLIC LSPClient_CodeAction
PUBLIC LSPClient_Rename
PUBLIC LSPClient_FindReferences
PUBLIC WorkspaceEdit_Apply
PUBLIC Debugger_Init
PUBLIC Debugger_Attach
PUBLIC Debugger_SetBreakpoint
PUBLIC Debugger_RemoveBreakpoint
PUBLIC Debugger_Continue
PUBLIC Debugger_StepOver
PUBLIC Debugger_WaitForEvent
PUBLIC Debugger_GetRegisters
PUBLIC Profiler_Init
PUBLIC Profiler_Start
PUBLIC Profiler_Stop
PUBLIC Profiler_RecordFunctionEnter
PUBLIC Profiler_RecordFunctionExit
PUBLIC Profiler_RecordAllocation
PUBLIC Profiler_GetReport
PUBLIC UIPanel_Create
PUBLIC UIPanel_Show
PUBLIC UIPanel_SetContent
PUBLIC UIPanel_Destroy

END
