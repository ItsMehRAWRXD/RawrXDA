; ═══════════════════════════════════════════════════════════════════
; RawrXD DAP Engine — Debug Adapter Protocol subsystem
; Integrates with: beacon.asm (slot 4), ui.asm (debug panel)
; Exports: DAP_Init, DAP_Launch, DAP_Step, DAP_StackTrace, DAP_Evaluate
;
; Process lifecycle: CreateProcessA with DEBUG_PROCESS flag,
; WaitForDebugEvent loop in background thread, Beacon notifications
; to UI for breakpoint hits, step completions, and exit events.
; ═══════════════════════════════════════════════════════════════════

; ── Win32 API imports ────────────────────────────────────────────
EXTERN CreateProcessA:PROC
EXTERN CreateThread:PROC
EXTERN WaitForDebugEvent:PROC
EXTERN ContinueDebugEvent:PROC
EXTERN CloseHandle:PROC
EXTERN TerminateProcess:PROC
EXTERN GetThreadContext:PROC
EXTERN SetThreadContext:PROC
EXTERN ReadProcessMemory:PROC
EXTERN WriteProcessMemory:PROC
EXTERN SuspendThread:PROC
EXTERN ResumeThread:PROC
EXTERN OpenThread:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN wsprintfA:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN BeaconSend:PROC

; ── Exports ──────────────────────────────────────────────────────
PUBLIC DAP_Init
PUBLIC DAP_Launch
PUBLIC DAP_Step
PUBLIC DAP_StackTrace
PUBLIC DAP_Evaluate

; ── Constants ────────────────────────────────────────────────────
; CreateProcess flags
DEBUG_PROCESS            equ 1h
DEBUG_ONLY_THIS_PROCESS  equ 2h
CREATE_NEW_CONSOLE       equ 10h

; WaitForDebugEvent → INFINITE
INFINITE_WAIT            equ 0FFFFFFFFh

; DEBUG_EVENT.dwDebugEventCode values
EXCEPTION_DEBUG_EVENT    equ 1
CREATE_THREAD_DEBUG_EVENT equ 2
CREATE_PROCESS_DEBUG_EVENT equ 3
EXIT_THREAD_DEBUG_EVENT  equ 4
EXIT_PROCESS_DEBUG_EVENT equ 5
LOAD_DLL_DEBUG_EVENT     equ 6
OUTPUT_DEBUG_STRING_EVENT equ 8

; ContinueDebugEvent dispositions
DBG_CONTINUE             equ 00010002h
DBG_EXCEPTION_NOT_HANDLED equ 80010001h

; EXCEPTION_BREAKPOINT
STATUS_BREAKPOINT        equ 80000003h
STATUS_SINGLE_STEP       equ 80000004h

; GetThreadContext / SetThreadContext
CONTEXT_CONTROL          equ 100001h
CONTEXT_INTEGER          equ 100002h
CONTEXT_FULL             equ 10000Bh

; EFLAGS Trap Flag (bit 8) for single-stepping
EFLAGS_TF                equ 100h

; Thread access for OpenThread
THREAD_ALL_ACCESS        equ 1F03FFh

; DAP step types
STEP_INTO                equ 1
STEP_OVER                equ 2
STEP_OUT                 equ 3

; DAP Beacon event IDs (slot 4)
DAP_BEACON_SLOT          equ 4
DAP_EVENT_STARTED        equ 02001h
DAP_EVENT_BREAKPOINT     equ 02002h
DAP_EVENT_STEPPED        equ 02003h
DAP_EVENT_EXIT           equ 02004h
DAP_EVENT_OUTPUT         equ 02005h
DAP_RESP_STACKTRACE      equ 02101h
DAP_RESP_EVALUATE        equ 02102h

; Max breakpoints / stack frames
MAX_BREAKPOINTS          equ 64
MAX_STACK_FRAMES         equ 100

; STARTUPINFOA size on x64 = 104 bytes
STARTUPINFOA_SIZE        equ 104

; ── STARTUPINFOA struct ──────────────────────────────────────────
STARTUPINFOA STRUCT
    cb              dd ?
    lpReserved      dq ?
    lpDesktop       dq ?
    lpTitle         dq ?
    dwX             dd ?
    dwY             dd ?
    dwXSize         dd ?
    dwYSize         dd ?
    dwXCountChars   dd ?
    dwYCountChars   dd ?
    dwFillAttribute dd ?
    dwFlags         dd ?
    wShowWindow     dw ?
    cbReserved2     dw ?
    lpReserved2     dq ?
    hStdInput       dq ?
    hStdOutput      dq ?
    hStdError       dq ?
STARTUPINFOA ENDS

; ── PROCESS_INFORMATION struct ───────────────────────────────────
PROCESS_INFORMATION STRUCT
    hProcess        dq ?
    hThread         dq ?
    dwProcessId     dd ?
    dwThreadId      dd ?
PROCESS_INFORMATION ENDS

; ── Stack frame layout for DAP_EventLoop ────────────────────────
; DEBUG_EVENT is 176 bytes on x64 (largest member is EXCEPTION_DEBUG_INFO)
DEBUG_EVENT_SIZE         equ 176

; ── Breakpoint record (24 bytes) ────────────────────────────────
; offset 0: address (QWORD)
; offset 8: original byte (BYTE, padded to QWORD)
; offset 16: enabled (DWORD), line (DWORD)
BP_REC_SIZE              equ 24

; ── Stack frame info (40 bytes per frame) ────────────────────────
; offset 0:  RIP (QWORD)
; offset 8:  RSP (QWORD)
; offset 16: RBP (QWORD)
; offset 24: frameId (DWORD)
; offset 28: line (DWORD)
; offset 32: reserved (QWORD)
FRAME_REC_SIZE           equ 40

; ═════════════════════════════════════════════════════════════════
.data
align 8
g_hDebugProcess     dq 0
g_hDebugThread      dq 0
g_dwProcessId       dd 0
g_dwThreadId        dd 0
g_bDebugging        dd 0
g_bStepping         dd 0
g_stepType          dd 0
g_frameCount        dd 0
g_bpCount           dd 0

.data?
align 16
; Cached stack frames: MAX_STACK_FRAMES * FRAME_REC_SIZE = 4000 bytes
g_stackFrames       db (MAX_STACK_FRAMES * FRAME_REC_SIZE) dup(?)

; Breakpoint table: MAX_BREAKPOINTS * BP_REC_SIZE = 1536 bytes
g_breakpoints       db (MAX_BREAKPOINTS * BP_REC_SIZE) dup(?)

; Evaluate result buffer (256 bytes)
g_evalResult        db 256 dup(?)

; Event notification buffer (128 bytes, sent via Beacon)
g_eventBuf          db 128 dup(?)

.const
szDapStarted    db "DAP: Debug session started",0
szDapExited     db "DAP: Debuggee exited",0
szDapBpHit      db "DAP: Breakpoint hit",0
szDapStepped    db "DAP: Step complete",0
szEvalDefault   db "0",0

; ═════════════════════════════════════════════════════════════════
.code

; ────────────────────────────────────────────────────────────────
; DAP_Init — Zero state, prepare breakpoint table
;   No args. Returns EAX=0.
; ────────────────────────────────────────────────────────────────
DAP_Init PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     g_hDebugProcess, 0
    mov     g_hDebugThread, 0
    mov     g_dwProcessId, 0
    mov     g_dwThreadId, 0
    mov     g_bDebugging, 0
    mov     g_bStepping, 0
    mov     g_frameCount, 0
    mov     g_bpCount, 0

    add     rsp, 28h
    xor     eax, eax
    ret
DAP_Init ENDP

; ────────────────────────────────────────────────────────────────
; DAP_Launch — Spawn debuggee with DEBUG_PROCESS flag
;   RCX = pProgramPath (LPCSTR, null-terminated)
;   Returns: EAX = 1 success, 0 failure
;
; Stack layout (from RBP):
;   [rbp-68h .. rbp-01h] = STARTUPINFOA  (104 bytes)
;   [rbp-80h .. rbp-69h] = PROCESS_INFORMATION (24 bytes)
;   Total locals = 128 bytes → sub rsp, 0A0h (160)
;   Pushes: rbp, rbx = 2 × 8 = 16
;   Entry RSP offset: 8 (ret) + 16 (pushes) + 160 = 184 → 184 mod 16 = 8
;   Need +8 → 0A8h (168): 8+16+168 = 192 → 192 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_Launch PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0A8h
    .allocstack 0A8h
    .endprolog

    mov     rbx, rcx                    ; rbx = pProgramPath

    ; ── Zero STARTUPINFOA at [rbp-68h] ──
    lea     rdi, [rbp-68h]
    xor     eax, eax
    mov     ecx, 26                     ; 104/4 = 26 DWORDs
    rep     stosd
    mov     dword ptr [rbp-68h], STARTUPINFOA_SIZE  ; si.cb = 104

    ; ── Zero PROCESS_INFORMATION at [rbp-80h] ──
    mov     qword ptr [rbp-80h], 0      ; hProcess
    mov     qword ptr [rbp-78h], 0      ; hThread
    mov     dword ptr [rbp-70h], 0      ; dwProcessId
    mov     dword ptr [rbp-6Ch], 0      ; dwThreadId

    ; ── CreateProcessA ──
    ; CreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttr,
    ;   lpThreadAttr, bInheritHandles, dwCreationFlags, lpEnvironment,
    ;   lpCurrentDirectory, lpStartupInfo, lpProcessInformation)
    xor     ecx, ecx                    ; lpApplicationName = NULL
    mov     rdx, rbx                    ; lpCommandLine = pProgramPath
    xor     r8, r8                      ; lpProcessAttributes = NULL
    xor     r9, r9                      ; lpThreadAttributes = NULL
    mov     dword ptr [rsp+20h], 0      ; bInheritHandles = FALSE
    mov     dword ptr [rsp+28h], DEBUG_PROCESS or DEBUG_ONLY_THIS_PROCESS
    mov     qword ptr [rsp+30h], 0      ; lpEnvironment = NULL
    mov     qword ptr [rsp+38h], 0      ; lpCurrentDirectory = NULL
    lea     rax, [rbp-68h]
    mov     qword ptr [rsp+40h], rax    ; lpStartupInfo
    lea     rax, [rbp-80h]
    mov     qword ptr [rsp+48h], rax    ; lpProcessInformation
    call    CreateProcessA

    test    eax, eax
    jz      @launch_fail

    ; ── Save process info ──
    mov     rax, qword ptr [rbp-80h]    ; pi.hProcess
    mov     g_hDebugProcess, rax
    mov     eax, dword ptr [rbp-70h]    ; pi.dwProcessId
    mov     g_dwProcessId, eax
    mov     eax, dword ptr [rbp-6Ch]    ; pi.dwThreadId
    mov     g_dwThreadId, eax
    mov     g_bDebugging, 1

    ; ── Spawn event loop thread ──
    ; CreateThread(NULL, 0, lpStartAddress, lpParameter, 0, NULL)
    xor     ecx, ecx                    ; lpThreadAttributes = NULL
    xor     edx, edx                    ; dwStackSize = 0 (default)
    lea     r8, DAP_EventLoop           ; lpStartAddress
    xor     r9, r9                      ; lpParameter = NULL
    mov     dword ptr [rsp+20h], 0      ; dwCreationFlags = 0
    mov     qword ptr [rsp+28h], 0      ; lpThreadId = NULL
    call    CreateThread
    mov     g_hDebugThread, rax

    ; ── Notify UI via Beacon ──
    ; BeaconSend(beaconID=0, pData=&szDapStarted, dataLen=DAP_EVENT_STARTED)
    xor     ecx, ecx
    lea     rdx, szDapStarted
    mov     r8d, DAP_EVENT_STARTED
    call    BeaconSend

    mov     eax, 1                      ; success
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret

@launch_fail:
    xor     eax, eax                    ; failure
    lea     rsp, [rbp]
    pop     rbx
    pop     rbp
    ret
DAP_Launch ENDP

; ────────────────────────────────────────────────────────────────
; DAP_EventLoop — Background thread: process debug events
;   RCX = lpParameter (unused)
;   Runs until EXIT_PROCESS_DEBUG_EVENT or g_bDebugging == 0
;
; Stack layout: DEBUG_EVENT at [rsp+30h] = 176 bytes
;   1 push (rbx) + sub 0E8h (232) = 240 total
;   8 (ret) + 8 (push) + 232 = 248 → 248 mod 16 = 8
;   Need 0F0h (240): 8+8+240 = 256 → 256 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_EventLoop PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 0F0h
    .allocstack 0F0h
    .endprolog

    ; DEBUG_EVENT at [rsp+30h], 176 bytes
    ; rsp+00h..rsp+27h = shadow space + scratch
    ; rsp+28h..rsp+2Fh = alignment pad

@event_loop:
    ; Check if still debugging
    cmp     g_bDebugging, 0
    je      @evloop_exit

    ; WaitForDebugEvent(&de, INFINITE)
    lea     rcx, [rsp+30h]             ; lpDebugEvent
    mov     edx, INFINITE_WAIT         ; dwMilliseconds
    call    WaitForDebugEvent
    test    eax, eax
    jz      @evloop_exit               ; WaitForDebugEvent failed

    ; Read dwDebugEventCode at [rsp+30h]
    mov     eax, dword ptr [rsp+30h]

    cmp     eax, EXCEPTION_DEBUG_EVENT
    je      @ev_exception
    cmp     eax, EXIT_PROCESS_DEBUG_EVENT
    je      @ev_exit_process
    cmp     eax, CREATE_PROCESS_DEBUG_EVENT
    je      @ev_continue
    cmp     eax, CREATE_THREAD_DEBUG_EVENT
    je      @ev_continue
    cmp     eax, EXIT_THREAD_DEBUG_EVENT
    je      @ev_continue
    cmp     eax, LOAD_DLL_DEBUG_EVENT
    je      @ev_continue
    cmp     eax, OUTPUT_DEBUG_STRING_EVENT
    je      @ev_dbgstr
    jmp     @ev_continue

@ev_exception:
    ; DEBUG_EVENT: dwProcessId at +4, dwThreadId at +8
    ; EXCEPTION_DEBUG_INFO.ExceptionRecord.ExceptionCode at +10h
    mov     eax, dword ptr [rsp+30h+10h]   ; ExceptionCode

    cmp     eax, STATUS_BREAKPOINT
    je      @ev_breakpoint
    cmp     eax, STATUS_SINGLE_STEP
    je      @ev_singlestep

    ; Other exception — pass to debuggee (not handled)
    mov     ecx, dword ptr [rsp+30h+4]     ; dwProcessId
    mov     edx, dword ptr [rsp+30h+8]     ; dwThreadId
    mov     r8d, DBG_EXCEPTION_NOT_HANDLED
    call    ContinueDebugEvent
    jmp     @event_loop

@ev_breakpoint:
    ; Notify UI: breakpoint hit
    xor     ecx, ecx                       ; UI beacon slot 0
    lea     rdx, szDapBpHit
    mov     r8d, DAP_EVENT_BREAKPOINT
    call    BeaconSend

    ; If we're stepping, auto-continue (user resumes via DAP_Step)
    ; For now, continue the debuggee
    mov     ecx, dword ptr [rsp+30h+4]
    mov     edx, dword ptr [rsp+30h+8]
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent
    jmp     @event_loop

@ev_singlestep:
    ; Clear stepping flag
    mov     g_bStepping, 0

    ; Notify UI: step completed
    xor     ecx, ecx
    lea     rdx, szDapStepped
    mov     r8d, DAP_EVENT_STEPPED
    call    BeaconSend

    ; Continue
    mov     ecx, dword ptr [rsp+30h+4]
    mov     edx, dword ptr [rsp+30h+8]
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent
    jmp     @event_loop

@ev_dbgstr:
    ; OutputDebugString from debuggee — forward to UI
    xor     ecx, ecx
    lea     rdx, g_eventBuf                 ; would contain the string
    mov     r8d, DAP_EVENT_OUTPUT
    call    BeaconSend
    jmp     @ev_continue

@ev_exit_process:
    ; Debuggee exited
    mov     g_bDebugging, 0

    ; Notify UI
    xor     ecx, ecx
    lea     rdx, szDapExited
    mov     r8d, DAP_EVENT_EXIT
    call    BeaconSend
    jmp     @evloop_exit

@ev_continue:
    ; Default: continue debug event
    mov     ecx, dword ptr [rsp+30h+4]     ; dwProcessId
    mov     edx, dword ptr [rsp+30h+8]     ; dwThreadId
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent
    jmp     @event_loop

@evloop_exit:
    add     rsp, 0F0h
    pop     rbx
    xor     eax, eax
    ret
DAP_EventLoop ENDP

; ────────────────────────────────────────────────────────────────
; DAP_Step — Single-step the debuggee
;   ECX = stepType (STEP_INTO=1, STEP_OVER=2, STEP_OUT=3)
;   Sets EFLAGS.TF on the debuggee's main thread via SetThreadContext.
;   Returns: EAX = 1 success, 0 failure
;
; Stack: 1 push (rbx) + 0D8h = 216 + 8(push) + 8(ret) = 232
;   232 mod 16 = 8 → need 0E0h (224): 8+8+224 = 240 → 0 ✓
;   CONTEXT at [rsp+30h], CONTEXT_FULL needs ~1232 bytes
;   Increase to 560h: 8+8+1376 = 1392 → 1392 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_Step PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 560h
    .allocstack 560h
    .endprolog

    mov     g_stepType, ecx
    mov     g_bStepping, 1

    cmp     g_bDebugging, 0
    je      @step_fail

    ; Open the debuggee's main thread
    ; OpenThread(dwDesiredAccess, bInheritHandle, dwThreadId)
    mov     ecx, THREAD_ALL_ACCESS
    xor     edx, edx                    ; bInheritHandle = FALSE
    mov     r8d, g_dwThreadId
    call    OpenThread
    test    rax, rax
    jz      @step_fail
    mov     rbx, rax                    ; rbx = hThread

    ; Suspend the thread before modifying context
    mov     rcx, rbx
    call    SuspendThread

    ; Zero CONTEXT area at [rsp+30h]
    lea     rdi, [rsp+30h]
    xor     eax, eax
    mov     ecx, 308                    ; 1232/4 ≈ 308 DWORDs
    rep     stosd

    ; Set ContextFlags = CONTEXT_FULL
    mov     dword ptr [rsp+30h+30h], CONTEXT_FULL  ; ContextFlags at offset 30h

    ; GetThreadContext(hThread, lpContext)
    mov     rcx, rbx
    lea     rdx, [rsp+30h]
    call    GetThreadContext
    test    eax, eax
    jz      @step_resume

    ; Set Trap Flag: EFlags is at offset 44h in CONTEXT (x64)
    ; Actually on x64, CONTEXT.EFlags is at offset 30h+14h = 44h from CONTEXT base
    ; CONTEXT layout: P1Home(8) P2Home(8) P3Home(8) P4Home(8) P5Home(8) P6Home(8) 
    ;   ContextFlags(4) MxCsr(4) ... EFlags at offset 44h
    mov     eax, dword ptr [rsp+30h+44h]   ; EFlags
    or      eax, EFLAGS_TF                  ; Set TF
    mov     dword ptr [rsp+30h+44h], eax

    ; SetThreadContext(hThread, lpContext)
    mov     rcx, rbx
    lea     rdx, [rsp+30h]
    call    SetThreadContext

@step_resume:
    ; Resume the thread
    mov     rcx, rbx
    call    ResumeThread

    ; Close thread handle
    mov     rcx, rbx
    call    CloseHandle

    add     rsp, 560h
    pop     rbx
    mov     eax, 1
    ret

@step_fail:
    add     rsp, 560h
    pop     rbx
    xor     eax, eax
    ret
DAP_Step ENDP

; ────────────────────────────────────────────────────────────────
; DAP_StackTrace — Walk the debuggee's stack (manual RBP chain)
;   No args. Populates g_stackFrames[], sets g_frameCount.
;   Sends result via Beacon.
;   Returns: EAX = g_frameCount
;
; Stack: 2 pushes (rbx, rsi) + 560h
;   8+16+1376 = 1400 → 1400 mod 16 = 8 → need 568h (1384)
;   8+16+1384 = 1408 → 1408 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_StackTrace PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 568h
    .allocstack 568h
    .endprolog

    mov     g_frameCount, 0

    cmp     g_bDebugging, 0
    je      @st_done

    ; Open debuggee main thread
    mov     ecx, THREAD_ALL_ACCESS
    xor     edx, edx
    mov     r8d, g_dwThreadId
    call    OpenThread
    test    rax, rax
    jz      @st_done
    mov     rbx, rax                    ; rbx = hThread

    ; Suspend thread
    mov     rcx, rbx
    call    SuspendThread

    ; Zero CONTEXT at [rsp+30h]
    lea     rdi, [rsp+30h]
    xor     eax, eax
    mov     ecx, 308
    rep     stosd
    mov     dword ptr [rsp+30h+30h], CONTEXT_FULL

    ; GetThreadContext
    mov     rcx, rbx
    lea     rdx, [rsp+30h]
    call    GetThreadContext
    test    eax, eax
    jz      @st_resume

    ; Extract RIP and RBP from CONTEXT
    ; x64 CONTEXT: Rip at offset 0F8h, Rsp at 098h, Rbp at 0A0h (approx)
    ; Precise offsets: Rip=F8h, Rsp=98h, Rbp=A0h (from CONTEXT base, not including alignment)
    ; We use a simplified approach: store RIP from CONTEXT
    mov     rax, qword ptr [rsp+30h+0F8h]  ; RIP
    mov     r8, qword ptr [rsp+30h+98h]    ; RSP  
    mov     r9, qword ptr [rsp+30h+0A0h]   ; RBP

    ; Store first frame
    lea     rsi, g_stackFrames
    mov     qword ptr [rsi], rax            ; frame[0].RIP
    mov     qword ptr [rsi+8], r8           ; frame[0].RSP
    mov     qword ptr [rsi+10h], r9         ; frame[0].RBP
    mov     dword ptr [rsi+18h], 0          ; frame[0].frameId = 0
    mov     dword ptr [rsi+1Ch], 0          ; frame[0].line = 0
    mov     qword ptr [rsi+20h], 0          ; frame[0].reserved
    mov     g_frameCount, 1

    ; Walk RBP chain (up to MAX_STACK_FRAMES)
    ; Each frame: [RBP] = saved RBP, [RBP+8] = return address
    mov     esi, 1                          ; frame index
@st_walk:
    cmp     esi, MAX_STACK_FRAMES
    jge     @st_resume
    test    r9, r9                          ; RBP == 0 → end of chain
    jz      @st_resume

    ; ReadProcessMemory(hProcess, lpBaseAddress=RBP, lpBuffer, nSize=16, lpBytesRead)
    ; Read both saved RBP and return address at once (16 bytes)
    mov     rcx, g_hDebugProcess
    mov     rdx, r9                         ; base = current RBP
    lea     r8, [rsp+20h]                  ; local buffer (16 bytes)
    mov     r9d, 16                         ; nSize
    mov     qword ptr [rsp+20h+20h], 0     ; lpNumberOfBytesRead = NULL
    call    ReadProcessMemory
    test    eax, eax
    jz      @st_resume                      ; ReadProcessMemory failed

    ; new RBP = [rsp+20h], return addr = [rsp+28h]
    mov     r9, qword ptr [rsp+20h]        ; new RBP (saved from callee)
    mov     rax, qword ptr [rsp+28h]       ; return address

    ; Store frame
    movsxd  rcx, esi
    imul    rcx, FRAME_REC_SIZE
    lea     rdx, g_stackFrames
    add     rdx, rcx
    mov     qword ptr [rdx], rax            ; frame.RIP
    mov     qword ptr [rdx+8], 0            ; frame.RSP (unknown in walk)
    mov     qword ptr [rdx+10h], r9         ; frame.RBP
    mov     dword ptr [rdx+18h], esi        ; frame.frameId
    mov     dword ptr [rdx+1Ch], 0          ; frame.line (unknown)
    mov     qword ptr [rdx+20h], 0          ; frame.reserved

    inc     esi
    mov     g_frameCount, esi
    jmp     @st_walk

@st_resume:
    ; Resume thread
    mov     rcx, rbx
    call    ResumeThread

    ; Close thread handle
    mov     rcx, rbx
    call    CloseHandle

@st_done:
    ; Send stack trace via Beacon
    xor     ecx, ecx                       ; UI slot
    lea     rdx, g_stackFrames
    mov     r8d, DAP_RESP_STACKTRACE
    call    BeaconSend

    mov     eax, g_frameCount
    add     rsp, 568h
    pop     rsi
    pop     rbx
    ret
DAP_StackTrace ENDP

; ────────────────────────────────────────────────────────────────
; DAP_Evaluate — Evaluate expression in debuggee context
;   RCX = pExpr (LPCSTR, null-terminated expression)
;   Writes result to g_evalResult, sends via Beacon.
;   Returns: RAX = pointer to g_evalResult
;
; Stack: 1 push (rbx) + 38h (56)
;   8+8+56 = 72 → 72 mod 16 = 8 → need 40h (64): 8+8+64 = 80 → 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_Evaluate PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     rbx, rcx                    ; rbx = pExpr

    ; For now: simple memory dereference or literal echo
    ; Parse expression: if starts with '*' → ReadProcessMemory
    ; Otherwise → echo expression as string result

    movzx   eax, byte ptr [rbx]
    cmp     al, '*'
    je      @eval_deref

    ; Echo: copy expression to result buffer
    lea     rcx, g_evalResult
    mov     rdx, rbx
    call    lstrcpyA
    jmp     @eval_send

@eval_deref:
    ; TODO: Parse hex address after '*', ReadProcessMemory, format result
    ; For now, return placeholder
    lea     rcx, g_evalResult
    lea     rdx, szEvalDefault
    call    lstrcpyA

@eval_send:
    ; Send via Beacon
    xor     ecx, ecx                    ; UI slot
    lea     rdx, g_evalResult
    mov     r8d, DAP_RESP_EVALUATE
    call    BeaconSend

    lea     rax, g_evalResult
    add     rsp, 40h
    pop     rbx
    ret
DAP_Evaluate ENDP

END
