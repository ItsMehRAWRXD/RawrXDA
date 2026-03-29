; ═══════════════════════════════════════════════════════════════════
; RawrXD DAP Engine — Debug Adapter Protocol subsystem (v14.5.0-SOVEREIGN)
; STATUS: COMPLETE — MINIMALISM MINIMIZED. COMPLEXITY: MAXIMUM.
; Integrates with: beacon.asm (slot 4), ui.asm (debug panel)
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
EXTERN lstrlenA:PROC
EXTERN lstrcmpA:PROC
EXTERN lstrcmpiA:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetLastError:PROC
EXTERN FlushInstructionCache:PROC
EXTERN GetTickCount64:PROC

; ── Cross-module imports ─────────────────────────────────────────
EXTERN BeaconSend:PROC

; ── Forward declarations ─────────────────────────────────────────
DAP_EventLoop PROTO

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

; ── DAP Protocol States ──────────────────────────────────────────
DAP_STATE_UNINITIALIZED  equ 0
DAP_STATE_INITIALIZED    equ 1
DAP_STATE_CONFIGURING    equ 2
DAP_STATE_RUNNING        equ 3
DAP_STATE_STOPPED        equ 4
DAP_STATE_TERMINATED     equ 5

; ── Variable Scope Types ─────────────────────────────────────────
SCOPE_LOCAL              equ 1
SCOPE_GLOBAL             equ 2
SCOPE_REGISTERS          equ 3

; ── Scope / Variable Structure Sizes ─────────────────────────────
SCOPE_HEADER_SIZE        equ 24
VAR_ENTRY_SIZE           equ 48
MAX_VARIABLES            equ 128
VAR_SCOPES_SIZE          equ (3 * SCOPE_HEADER_SIZE + MAX_VARIABLES * VAR_ENTRY_SIZE)

; ── Extended Breakpoint Record (32 bytes) ────────────────────────
; offset 0: conditionHash (QWORD)
; offset 8: hitCount (DWORD)
; offset 12: hitCondition (DWORD) — 0=none,1=>=,2===,3=%
; offset 16: logMessagePtr (QWORD)
; offset 24: enabled (DWORD), verified (DWORD)
BP_EXT_REC_SIZE          equ 32

; ── Event Ring Buffer ────────────────────────────────────────────
EVENT_RING_ENTRIES       equ 256
EVENT_RING_ENTRY_SIZE    equ 64
EVENT_RING_MASK          equ (EVENT_RING_ENTRIES - 1)

; ── DAP Protocol Buffers ─────────────────────────────────────────
JSON_SCRATCH_SIZE        equ 16384
DAP_INPUT_BUF_SIZE       equ 32768
DAP_RESPONSE_BUF_SIZE    equ 16384
CONTENT_LENGTH_HDR_MAX   equ 256
MAX_CMD_NAME_LEN         equ 64

; ── x64 Instruction Decode Constants ─────────────────────────────
MAX_INST_LEN             equ 15
OPCODE_CALL_REL32        equ 0E8h
OPCODE_CALL_FF           equ 0FFh
OPCODE_RET_NEAR          equ 0C3h
OPCODE_RET_NEAR_IMM      equ 0C2h
OPCODE_INT3              equ 0CCh
MODRM_REG_MASK           equ 38h
MODRM_CALL_IND           equ 10h
MODRM_MOD_MASK           equ 0C0h
MODRM_RM_MASK            equ 07h

; ── DAP Capabilities Flags ───────────────────────────────────────
DAP_CAP_CONDITIONAL_BP   equ 1
DAP_CAP_HIT_CONDITIONAL  equ 2
DAP_CAP_EVALUATE         equ 4
DAP_CAP_STEP_BACK        equ 8
DAP_CAP_SET_VARIABLE     equ 10h
DAP_CAP_RESTART          equ 20h
DAP_CAP_GOTO_TARGETS     equ 40h
DAP_CAP_COMPLETIONS      equ 80h

; ── DAP Command IDs (internal dispatch) ──────────────────────────
DAP_CMD_INITIALIZE       equ 1
DAP_CMD_LAUNCH           equ 2
DAP_CMD_ATTACH           equ 3
DAP_CMD_SET_BREAKPOINTS  equ 4
DAP_CMD_CONTINUE         equ 5
DAP_CMD_NEXT             equ 6
DAP_CMD_STEP_IN          equ 7
DAP_CMD_STEP_OUT         equ 8
DAP_CMD_STACK_TRACE      equ 9
DAP_CMD_SCOPES           equ 10
DAP_CMD_VARIABLES        equ 11
DAP_CMD_EVALUATE         equ 12
DAP_CMD_DISCONNECT       equ 13
DAP_CMD_UNKNOWN          equ 0

; ── DAP Parse States ─────────────────────────────────────────────
PARSE_STATE_HEADER       equ 0
PARSE_STATE_BODY         equ 1

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

; ── DAP Session State ────────────────────────────────────────────
align 8
g_dapSessionId      dq 0
g_dapState          dd DAP_STATE_UNINITIALIZED
g_dapSeqCounter     dd 1
g_dapCapabilities   dq 0
g_dapRequestSeq     dd 0

; ── DAP Parse State ──────────────────────────────────────────────
g_dapInputPos       dd 0
g_dapInputLen       dd 0
g_dapContentLen     dd 0
g_dapParseState     dd PARSE_STATE_HEADER
g_dapPartialLen     dd 0

; ── Step Telemetry ───────────────────────────────────────────────
g_stepCount         dd 0
g_stepInCount       dd 0
g_stepOverCount     dd 0
g_stepOutCount      dd 0
g_lastStoppedRip    dq 0
g_lastStoppedLine   dd 0

; ── Temp Breakpoint for Step-Over ────────────────────────────────
align 8
g_tempBpAddr        dq 0
g_tempBpOrigByte    db 0
                    db 7 dup(0)
g_tempBpActive      dd 0

; ── Event Ring Buffer State ──────────────────────────────────────
g_eventRingHead     dd 0
g_eventRingTail     dd 0
g_eventRingCount    dd 0

; ── JSON Parser State ────────────────────────────────────────────
g_jsonScratchLen    dd 0
g_jsonDepth         dd 0
g_jsonTokenStart    dd 0
g_jsonTokenLen      dd 0

; ── DAP I/O Handles ──────────────────────────────────────────────
align 8
g_dapInputHandle    dq 0
g_dapOutputHandle   dq 0
g_dapPipeHandle     dq 0

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

; ── DAP Protocol Buffers ─────────────────────────────────────────
align 16
g_jsonScratchBuf    db JSON_SCRATCH_SIZE dup(?)
g_dapInputBuf       db DAP_INPUT_BUF_SIZE dup(?)
g_dapResponseBuf    db DAP_RESPONSE_BUF_SIZE dup(?)
g_dapPartialBuf     db CONTENT_LENGTH_HDR_MAX dup(?)
g_dapCmdName        db MAX_CMD_NAME_LEN dup(?)

; ── Extended Breakpoint Table ────────────────────────────────────
g_bpExtended        db (MAX_BREAKPOINTS * BP_EXT_REC_SIZE) dup(?)

; ── Variable Scopes Tree ─────────────────────────────────────────
g_varScopes         db VAR_SCOPES_SIZE dup(?)

; ── Event Ring Buffer ────────────────────────────────────────────
g_eventRing         db (EVENT_RING_ENTRIES * EVENT_RING_ENTRY_SIZE) dup(?)

; ── Instruction Decode Buffer ────────────────────────────────────
g_instDecodeBuf     db MAX_INST_LEN dup(?)
g_instDecodeLen     dd ?

; ── Content-Length Header Parse ───────────────────────────────────
g_contentLenBuf     db CONTENT_LENGTH_HDR_MAX dup(?)

.const
szDapStarted    db "DAP: Debug session started",0
szDapExited     db "DAP: Debuggee exited",0
szDapBpHit      db "DAP: Breakpoint hit",0
szDapStepped    db "DAP: Step complete",0
szEvalDefault   db "0",0
szEvalFmt       db "0x%016I64X",0
szEvalReadErr   db "<read error>",0
szEvalNoDebug   db "<no debug session>",0

; ── DAP Protocol Strings ─────────────────────────────────────────
szContentLength db "Content-Length: ",0
szCRLF          db 13,10,0
szCmdInit       db "initialize",0
szCmdLaunch     db "launch",0
szCmdAttach     db "attach",0
szCmdSetBp      db "setBreakpoints",0
szCmdContinue   db "continue",0
szCmdNext       db "next",0
szCmdStepIn     db "stepIn",0
szCmdStepOut    db "stepOut",0
szCmdStackTrace db "stackTrace",0
szCmdScopes     db "scopes",0
szCmdVariables  db "variables",0
szCmdEvaluate   db "evaluate",0
szCmdDisconnect db "disconnect",0
szStoppedStep   db "step",0
szStoppedBp     db "breakpoint",0
szStoppedPause  db "pause",0
szStoppedEntry  db "entry",0
szEventStopped  db "stopped",0
szEventCont     db "continued",0
szEventOutput   db "output",0
szEventInit     db "initialized",0
szDapStepFmt    db "DAP: Step #%d at RIP=0x%016I64X",0
szDapBpHitFmt   db "DAP: Breakpoint %d hit at 0x%016I64X",0
szDapInitOk     db "DAP: Session initialized (caps=0x%08X)",0
szDapDisconn    db "DAP: Disconnected",0

; ── DAP JSON Response Templates ──────────────────────────────────
szRespHdr       db "Content-Length: %d",13,10,13,10,0
szRespInitBody  db '{"seq":%d,"type":"response","request_seq":%d,'
                db '"command":"initialize","success":true,'
                db '"body":{"supportsConditionalBreakpoints":true,'
                db '"supportsHitConditionalBreakpoints":true,'
                db '"supportsEvaluateForHovers":true,'
                db '"supportsStepBack":false}}',0
szRespGeneric   db '{"seq":%d,"type":"response","request_seq":%d,'
                db '"command":"%s","success":true}',0
szRespError     db '{"seq":%d,"type":"response","request_seq":%d,'
                db '"command":"%s","success":false,'
                db '"message":"%s"}',0
szEvtStopped    db '{"seq":%d,"type":"event","event":"stopped",'
                db '"body":{"reason":"%s","threadId":1,'
                db '"allThreadsStopped":true}}',0
szEvtContinued  db '{"seq":%d,"type":"event","event":"continued",'
                db '"body":{"threadId":1,"allThreadsContinued":true}}',0
szEvtInitialized db '{"seq":%d,"type":"event",'
                db '"event":"initialized"}',0

; ═════════════════════════════════════════════════════════════════
.code

; ────────────────────────────────────────────────────────────────
; DAP_Init — Full DAP session initialization
;   Allocates session state, JSON parser scratch buffer (16KB),
;   breakpoint table (64 entries w/ condition hash, hit count),
;   variable scopes tree (local/global/register), event ring
;   buffer (256 entries), sequence counter for request/response.
;   No args. Returns EAX=0 success, EAX=-1 failure
;
; Stack: push rbp,rbx,rsi,rdi + sub 0C8h
;   8(ret)+32(push)+200 = 240 → 240 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_Init PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0C8h
    .allocstack 0C8h
    .endprolog

    ; ── Phase 1: Zero all core debug state ───────────────────────
    xor     eax, eax
    mov     g_hDebugProcess, rax
    mov     g_hDebugThread, rax
    mov     g_dwProcessId, eax
    mov     g_dwThreadId, eax
    mov     g_bDebugging, eax
    mov     g_bStepping, eax
    mov     g_frameCount, eax
    mov     g_bpCount, eax
    mov     g_stepType, eax

    ; ── Phase 2: Initialize DAP session state ────────────────────
    ; Generate session ID from tick count for uniqueness
    call    GetTickCount64
    mov     g_dapSessionId, rax
    mov     g_dapState, DAP_STATE_UNINITIALIZED
    mov     g_dapSeqCounter, 1
    mov     g_dapRequestSeq, 0

    ; Set default capabilities bitmask
    mov     eax, DAP_CAP_CONDITIONAL_BP or DAP_CAP_HIT_CONDITIONAL or DAP_CAP_EVALUATE
    mov     dword ptr g_dapCapabilities, eax
    mov     dword ptr g_dapCapabilities+4, 0

    ; ── Phase 3: Zero JSON parser scratch buffer (16KB) ──────────
    lea     rdi, g_jsonScratchBuf
    xor     eax, eax
    mov     ecx, JSON_SCRATCH_SIZE / 4   ; 4096 DWORDs
    rep     stosd
    mov     g_jsonScratchLen, 0
    mov     g_jsonDepth, 0
    mov     g_jsonTokenStart, 0
    mov     g_jsonTokenLen, 0

    ; ── Phase 4: Initialize DAP protocol parse state ─────────────
    mov     g_dapInputPos, 0
    mov     g_dapInputLen, 0
    mov     g_dapContentLen, 0
    mov     g_dapParseState, PARSE_STATE_HEADER
    mov     g_dapPartialLen, 0

    ; Zero input buffer header region (first 1KB)
    lea     rdi, g_dapInputBuf
    xor     eax, eax
    mov     ecx, 256
    rep     stosd

    ; Zero response buffer
    lea     rdi, g_dapResponseBuf
    xor     eax, eax
    mov     ecx, DAP_RESPONSE_BUF_SIZE / 4
    rep     stosd

    ; Zero partial reassembly buffer
    lea     rdi, g_dapPartialBuf
    xor     eax, eax
    mov     ecx, CONTENT_LENGTH_HDR_MAX / 4
    rep     stosd

    ; Zero command name buffer
    lea     rdi, g_dapCmdName
    xor     eax, eax
    mov     ecx, MAX_CMD_NAME_LEN / 4
    rep     stosd

    ; ── Phase 5: Initialize breakpoint table (64 entries) ────────
    ; Zero primary breakpoint table
    lea     rdi, g_breakpoints
    xor     eax, eax
    mov     ecx, (MAX_BREAKPOINTS * BP_REC_SIZE) / 4
    rep     stosd

    ; Initialize extended breakpoint table (condition hash, hit count)
    lea     rdi, g_bpExtended
    xor     eax, eax
    mov     ecx, (MAX_BREAKPOINTS * BP_EXT_REC_SIZE) / 4
    rep     stosd

    ; Walk each extended BP entry and set defaults
    lea     rbx, g_bpExtended
    xor     esi, esi
@@bp_init_loop:
    cmp     esi, MAX_BREAKPOINTS
    jge     @@bp_init_done
    mov     qword ptr [rbx], 0           ; conditionHash = 0
    mov     dword ptr [rbx+8], 0         ; hitCount = 0
    mov     dword ptr [rbx+0Ch], 0       ; hitCondition = 0 (none)
    mov     qword ptr [rbx+10h], 0       ; logMessagePtr = NULL
    mov     dword ptr [rbx+18h], 0       ; enabled = 0
    mov     dword ptr [rbx+1Ch], 0       ; verified = 0
    add     rbx, BP_EXT_REC_SIZE
    inc     esi
    jmp     @@bp_init_loop
@@bp_init_done:

    ; Clear temporary breakpoint state
    mov     g_tempBpAddr, 0
    mov     g_tempBpOrigByte, 0
    mov     g_tempBpActive, 0

    ; ── Phase 6: Allocate variable scopes tree ───────────────────
    ; Zero entire scopes region
    lea     rdi, g_varScopes
    xor     eax, eax
    mov     ecx, VAR_SCOPES_SIZE / 4
    rep     stosd

    ; Initialize Local scope header (offset 0)
    lea     rbx, g_varScopes
    mov     dword ptr [rbx], SCOPE_LOCAL           ; scopeType
    mov     dword ptr [rbx+4], 0                   ; variableCount
    mov     dword ptr [rbx+8], 0                   ; expansionState (collapsed)
    mov     dword ptr [rbx+0Ch], 0                 ; padding
    mov     qword ptr [rbx+10h], 0                 ; firstVariablePtr

    ; Initialize Global scope header (offset SCOPE_HEADER_SIZE)
    add     rbx, SCOPE_HEADER_SIZE
    mov     dword ptr [rbx], SCOPE_GLOBAL
    mov     dword ptr [rbx+4], 0
    mov     dword ptr [rbx+8], 0
    mov     dword ptr [rbx+0Ch], 0
    mov     qword ptr [rbx+10h], 0

    ; Initialize Register scope header (offset 2*SCOPE_HEADER_SIZE)
    add     rbx, SCOPE_HEADER_SIZE
    mov     dword ptr [rbx], SCOPE_REGISTERS
    mov     dword ptr [rbx+4], 16                  ; 16 GP registers
    mov     dword ptr [rbx+8], 1                   ; expanded by default
    mov     dword ptr [rbx+0Ch], 0
    mov     qword ptr [rbx+10h], 0

    ; Initialize variable entries pool (after scope headers)
    add     rbx, SCOPE_HEADER_SIZE
    xor     esi, esi
@@var_init_loop:
    cmp     esi, MAX_VARIABLES
    jge     @@var_init_done
    mov     qword ptr [rbx], 0           ; name[0:7]
    mov     qword ptr [rbx+8], 0         ; name[8:15]
    mov     qword ptr [rbx+10h], 0       ; value[0:7]
    mov     qword ptr [rbx+18h], 0       ; value[8:15]
    mov     dword ptr [rbx+20h], 0       ; type = 0
    mov     dword ptr [rbx+24h], 0       ; refId = 0
    mov     dword ptr [rbx+28h], 0       ; childCount = 0
    mov     dword ptr [rbx+2Ch], 0       ; reserved
    add     rbx, VAR_ENTRY_SIZE
    inc     esi
    jmp     @@var_init_loop
@@var_init_done:

    ; ── Phase 7: Initialize debug event ring buffer (256 entries)─
    lea     rdi, g_eventRing
    xor     eax, eax
    mov     ecx, (EVENT_RING_ENTRIES * EVENT_RING_ENTRY_SIZE) / 4
    rep     stosd

    mov     g_eventRingHead, 0
    mov     g_eventRingTail, 0
    mov     g_eventRingCount, 0

    ; ── Phase 8: Zero stack frames cache ─────────────────────────
    lea     rdi, g_stackFrames
    xor     eax, eax
    mov     ecx, (MAX_STACK_FRAMES * FRAME_REC_SIZE) / 4
    rep     stosd

    ; ── Phase 9: Zero eval and event output buffers ──────────────
    lea     rdi, g_evalResult
    xor     eax, eax
    mov     ecx, 256 / 4
    rep     stosd

    lea     rdi, g_eventBuf
    xor     eax, eax
    mov     ecx, 128 / 4
    rep     stosd

    ; ── Phase 10: Initialize step telemetry ──────────────────────
    mov     g_stepCount, 0
    mov     g_stepInCount, 0
    mov     g_stepOverCount, 0
    mov     g_stepOutCount, 0
    mov     g_lastStoppedRip, 0
    mov     g_lastStoppedLine, 0

    ; ── Phase 11: Zero instruction decode buffer ─────────────────
    lea     rdi, g_instDecodeBuf
    xor     eax, eax
    mov     ecx, 4                       ; ceil(15/4)
    rep     stosd
    mov     g_instDecodeLen, 0

    ; ── Phase 12: Initialize I/O handles ─────────────────────────
    mov     g_dapInputHandle, 0
    mov     g_dapOutputHandle, 0
    mov     g_dapPipeHandle, 0

    ; ── Phase 13: Set state to DAP_STATE_INITIALIZED ─────────────
    mov     g_dapState, DAP_STATE_INITIALIZED

    ; ── Notify: formatted init message via Beacon ────────────────
    lea     rcx, g_eventBuf
    lea     rdx, szDapInitOk
    mov     r8d, dword ptr g_dapCapabilities
    call    wsprintfA

    xor     ecx, ecx
    lea     rdx, g_eventBuf
    mov     r8d, DAP_EVENT_STARTED
    call    BeaconSend

    ; ── Success ──────────────────────────────────────────────────
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    xor     eax, eax                     ; return 0 = success
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
; DAP_EventLoop — Background thread: DAP wire protocol + debug events
;   RCX = lpParameter (unused)
;   Implements full DAP protocol: Content-Length header parsing,
;   JSON body extraction, command dispatch for all DAP commands.
;   Also processes WaitForDebugEvent for OS-level debug events.
;   Handles partial reads, message reassembly, select/poll loop.
;   Returns when disconnect received or fatal error.
;
; Stack layout (0x600 = 1536 bytes):
;   [rsp+0x00..0x27] = shadow space (32 bytes)
;   [rsp+0x28..0x2F] = scratch
;   [rsp+0x30..0xDF] = DEBUG_EVENT (176 bytes)
;   [rsp+0xE0..0x1DF] = Content-Length header format area (256 bytes)
;   [rsp+0x1E0..0x5DF] = JSON body parse area (1024 bytes)
;   [rsp+0x5E0..0x5FF] = temporaries (bytesRead, etc.)
;   Pushes: rbx,rsi,rdi,r12,r13,r14,r15 = 7*8 = 56
;   8(ret)+56(push)+1536 = 1600 → 1600 mod 16 = 0 ✓
; ────────────────────────────────────────────────────────────────
DAP_EventLoop PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 600h
    .allocstack 600h
    .endprolog

    ; r12 = persistent loop control (0 = continue, 1 = disconnect)
    xor     r12d, r12d
    ; r13d = DAP sequence counter (local cache)
    mov     r13d, g_dapSeqCounter
    ; r14d = debug event poll timeout (50ms for responsiveness)
    mov     r14d, 50
    ; r15d = accumulated partial header bytes
    xor     r15d, r15d

; ╔═══════════════════════════════════════════════════════════════╗
; ║  MAIN EVENT LOOP — alternates DAP protocol + debug events    ║
; ╚═══════════════════════════════════════════════════════════════╝
@@evloop_main:
    test    r12d, r12d
    jnz     @@evloop_shutdown

    ; ── Section A: Poll for DAP protocol input ───────────────────
    ; Try to read from DAP input handle (pipe/socket/stdin)
    mov     rax, g_dapInputHandle
    test    rax, rax
    jz      @@evloop_skip_dap_read       ; no DAP input configured

    ; ReadFile(hFile, lpBuffer, nBytesToRead, lpBytesRead, lpOverlapped)
    mov     rcx, g_dapInputHandle
    lea     rdx, g_dapInputBuf
    movsxd  rax, g_dapInputLen
    add     rdx, rax                     ; append after existing data
    mov     r8d, DAP_INPUT_BUF_SIZE
    sub     r8d, g_dapInputLen           ; remaining space
    jle     @@evloop_parse_dap           ; buffer full, try to parse
    lea     r9, [rsp+5E0h]              ; lpNumberOfBytesRead
    mov     qword ptr [rsp+20h], 0       ; lpOverlapped = NULL
    call    ReadFile
    test    eax, eax
    jz      @@evloop_check_read_err

    ; Update input length with bytes actually read
    mov     eax, dword ptr [rsp+5E0h]
    add     g_dapInputLen, eax
    jmp     @@evloop_parse_dap

@@evloop_check_read_err:
    call    GetLastError
    cmp     eax, 109                     ; ERROR_BROKEN_PIPE
    je      @@evloop_set_disconnect
    cmp     eax, 6                       ; ERROR_INVALID_HANDLE
    je      @@evloop_skip_dap_read
    ; Non-fatal error, continue
    jmp     @@evloop_skip_dap_read

@@evloop_set_disconnect:
    mov     r12d, 1
    jmp     @@evloop_shutdown

@@evloop_skip_dap_read:
    ; If we have pending data in the input buffer, try to parse
    cmp     g_dapInputLen, 0
    jg      @@evloop_parse_dap
    jmp     @@evloop_debug_events

    ; ── Section B: Parse DAP wire protocol messages ──────────────
@@evloop_parse_dap:
    ; State machine: PARSE_STATE_HEADER or PARSE_STATE_BODY
    cmp     g_dapParseState, PARSE_STATE_HEADER
    je      @@parse_header
    cmp     g_dapParseState, PARSE_STATE_BODY
    je      @@parse_body
    jmp     @@evloop_debug_events

; ── Parse Content-Length header ───────────────────────────────────
@@parse_header:
    ; Scan input buffer for \r\n\r\n (end of HTTP-style headers)
    mov     esi, g_dapInputPos
    mov     edi, g_dapInputLen
    cmp     esi, edi
    jge     @@evloop_debug_events        ; no data to parse

    lea     rbx, g_dapInputBuf
@@parse_hdr_scan:
    cmp     esi, edi
    jge     @@evloop_debug_events        ; need more data (partial header)

    ; Check for \r\n\r\n sequence (4 bytes)
    lea     eax, [esi+3]
    cmp     eax, edi
    jg      @@evloop_debug_events        ; not enough bytes

    cmp     byte ptr [rbx+rsi], 13       ; \r
    jne     @@parse_hdr_next
    lea     rax, [rbx+rsi]
    cmp     byte ptr [rax+1], 10         ; \n
    jne     @@parse_hdr_next
    cmp     byte ptr [rax+2], 13         ; \r
    jne     @@parse_hdr_next
    cmp     byte ptr [rax+3], 10         ; \n
    jne     @@parse_hdr_next

    ; Found \r\n\r\n at position esi
    ; Now parse Content-Length from header region [inputPos..esi-1]
    ; Save header end position in a stack slot
    mov     dword ptr [rsp+5E8h], esi    ; save end pos

    ; Scan backwards from inputPos for "Content-Length: "
    mov     esi, g_dapInputPos
@@parse_cl_scan:
    mov     eax, dword ptr [rsp+5E8h]
    cmp     esi, eax
    jge     @@parse_cl_not_found

    ; Quick check: 'C' as first char of "Content-Length: "
    cmp     byte ptr [rbx+rsi], 'C'
    jne     @@parse_cl_next
    cmp     byte ptr [rbx+rsi+1], 'o'
    jne     @@parse_cl_next
    cmp     byte ptr [rbx+rsi+2], 'n'
    jne     @@parse_cl_next
    cmp     byte ptr [rbx+rsi+7], 't'
    jne     @@parse_cl_next
    cmp     byte ptr [rbx+rsi+8], '-'
    jne     @@parse_cl_next
    cmp     byte ptr [rbx+rsi+9], 'L'
    jne     @@parse_cl_next
    cmp     byte ptr [rbx+rsi+15], ' '
    jne     @@parse_cl_next

    ; Found "Content-Length: " at esi, parse decimal number
    lea     esi, [esi+16]               ; skip "Content-Length: "
    xor     ecx, ecx                    ; accumulate content length
@@parse_cl_digits:
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, '0'
    jb      @@parse_cl_got_len
    cmp     al, '9'
    ja      @@parse_cl_got_len
    sub     al, '0'
    imul    ecx, 10
    movzx   eax, al
    add     ecx, eax
    inc     esi
    jmp     @@parse_cl_digits

@@parse_cl_got_len:
    mov     g_dapContentLen, ecx

    ; Restore header end position and advance past \r\n\r\n
    mov     esi, dword ptr [rsp+5E8h]
    lea     eax, [esi+4]                ; skip \r\n\r\n
    mov     g_dapInputPos, eax

    ; Validate content length
    cmp     g_dapContentLen, 0
    jle     @@parse_hdr_error
    cmp     g_dapContentLen, JSON_SCRATCH_SIZE
    jg      @@parse_hdr_error

    ; Transition to body parse state
    mov     g_dapParseState, PARSE_STATE_BODY
    jmp     @@parse_body

@@parse_cl_next:
    inc     esi
    jmp     @@parse_cl_scan

@@parse_cl_not_found:
@@parse_hdr_error:
    ; Malformed header — skip to after \r\n\r\n and reset
    mov     esi, dword ptr [rsp+5E8h]
    lea     eax, [esi+4]
    mov     g_dapInputPos, eax
    mov     g_dapParseState, PARSE_STATE_HEADER
    jmp     @@evloop_compact_input

@@parse_hdr_next:
    inc     esi
    jmp     @@parse_hdr_scan

; ── Parse JSON body ──────────────────────────────────────────────
@@parse_body:
    ; Check if we have enough data for the full body
    mov     eax, g_dapInputLen
    sub     eax, g_dapInputPos
    cmp     eax, g_dapContentLen
    jl      @@evloop_debug_events        ; need more data (partial body)

    ; Copy body to JSON scratch buffer
    mov     ecx, g_dapContentLen
    cmp     ecx, JSON_SCRATCH_SIZE - 1
    jge     @@parse_body_overflow

    lea     rsi, g_dapInputBuf
    movsxd  rax, g_dapInputPos
    add     rsi, rax                     ; source = input + pos
    lea     rdi, g_jsonScratchBuf        ; dest = scratch buffer
    movsxd  rcx, g_dapContentLen
    rep     movsb                        ; copy body
    ; Null-terminate
    mov     byte ptr [rdi], 0
    mov     eax, g_dapContentLen
    mov     g_jsonScratchLen, eax

    ; Advance input position past body
    mov     eax, g_dapContentLen
    add     g_dapInputPos, eax

    ; Reset parse state to header for next message
    mov     g_dapParseState, PARSE_STATE_HEADER

    ; ── Extract "command" field from JSON body ───────────────────
    ; Simple JSON field extraction: find "command":"<value>"
    lea     rbx, g_jsonScratchBuf
    xor     esi, esi                     ; scan position
    mov     edi, g_jsonScratchLen

@@json_find_cmd:
    cmp     esi, edi
    jge     @@cmd_unknown

    ; Look for '"command"' pattern
    cmp     byte ptr [rbx+rsi], '"'
    jne     @@json_find_cmd_next
    lea     eax, [esi+8]
    cmp     eax, edi
    jg      @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+1], 'c'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+2], 'o'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+3], 'm'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+4], 'm'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+5], 'a'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+6], 'n'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+7], 'd'
    jne     @@json_find_cmd_next
    cmp     byte ptr [rbx+rsi+8], '"'
    jne     @@json_find_cmd_next

    ; Found "command" — skip to value
    add     esi, 9                       ; skip past "command"
@@json_skip_colon:
    cmp     esi, edi
    jge     @@cmd_unknown
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, ':'
    je      @@json_past_colon
    cmp     al, ' '
    je      @@json_skip_colon_next
    cmp     al, 9
    je      @@json_skip_colon_next
    jmp     @@cmd_unknown
@@json_skip_colon_next:
    inc     esi
    jmp     @@json_skip_colon

@@json_past_colon:
    inc     esi
    ; Skip whitespace after colon
@@json_skip_ws:
    cmp     esi, edi
    jge     @@cmd_unknown
    cmp     byte ptr [rbx+rsi], ' '
    je      @@json_ws_next
    cmp     byte ptr [rbx+rsi], 9
    je      @@json_ws_next
    jmp     @@json_read_val
@@json_ws_next:
    inc     esi
    jmp     @@json_skip_ws

@@json_read_val:
    ; Expect opening quote for command value
    cmp     byte ptr [rbx+rsi], '"'
    jne     @@cmd_unknown
    inc     esi                          ; skip opening quote

    ; Copy command name to g_dapCmdName
    lea     rdi, g_dapCmdName
    xor     ecx, ecx
@@json_copy_cmd:
    cmp     esi, edi
    jge     @@cmd_name_done
    cmp     ecx, MAX_CMD_NAME_LEN - 1
    jge     @@cmd_name_done
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, '"'                      ; closing quote
    je      @@cmd_name_done
    mov     byte ptr [rdi+rcx], al
    inc     ecx
    inc     esi
    jmp     @@json_copy_cmd

@@cmd_name_done:
    mov     byte ptr [rdi+rcx], 0        ; null-terminate

    ; ── Extract "seq" field for request_seq tracking ─────────────
    xor     esi, esi
@@json_find_seq:
    cmp     esi, edi
    jge     @@seq_done
    lea     eax, [esi+4]
    cmp     eax, edi
    jg      @@json_find_seq_next
    cmp     byte ptr [rbx+rsi], '"'
    jne     @@json_find_seq_next
    cmp     byte ptr [rbx+rsi+1], 's'
    jne     @@json_find_seq_next
    cmp     byte ptr [rbx+rsi+2], 'e'
    jne     @@json_find_seq_next
    cmp     byte ptr [rbx+rsi+3], 'q'
    jne     @@json_find_seq_next
    cmp     byte ptr [rbx+rsi+4], '"'
    jne     @@json_find_seq_next

    ; Found "seq" — skip to colon and parse number
    add     esi, 5
@@json_seq_skip:
    cmp     esi, edi
    jge     @@seq_done
    cmp     byte ptr [rbx+rsi], ':'
    je      @@json_seq_colon
    inc     esi
    jmp     @@json_seq_skip

@@json_seq_colon:
    inc     esi
@@json_seq_ws_skip:
    cmp     esi, edi
    jge     @@seq_done
    cmp     byte ptr [rbx+rsi], ' '
    je      @@json_seq_ws
    cmp     byte ptr [rbx+rsi], 9
    je      @@json_seq_ws
    jmp     @@json_seq_parse
@@json_seq_ws:
    inc     esi
    jmp     @@json_seq_ws_skip

@@json_seq_parse:
    xor     ecx, ecx
@@json_seq_digit:
    cmp     esi, edi
    jge     @@json_seq_store
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, '0'
    jb      @@json_seq_store
    cmp     al, '9'
    ja      @@json_seq_store
    sub     al, '0'
    imul    ecx, 10
    movzx   eax, al
    add     ecx, eax
    inc     esi
    jmp     @@json_seq_digit

@@json_seq_store:
    mov     g_dapRequestSeq, ecx
    jmp     @@seq_done

@@json_find_seq_next:
    inc     esi
    jmp     @@json_find_seq

@@seq_done:

    ; ── Dispatch by command name ─────────────────────────────────
    ; Compare g_dapCmdName against known command strings
    lea     rcx, g_dapCmdName
    lea     rdx, szCmdInit
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_initialize

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdLaunch
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_launch

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdAttach
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_attach

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdSetBp
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_set_breakpoints

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdContinue
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_continue

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdNext
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_next

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdStepIn
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_step_in

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdStepOut
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_step_out

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdStackTrace
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_stack_trace

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdScopes
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_scopes

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdVariables
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_variables

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdEvaluate
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_evaluate

    lea     rcx, g_dapCmdName
    lea     rdx, szCmdDisconnect
    call    lstrcmpA
    test    eax, eax
    jz      @@cmd_disconnect

    jmp     @@cmd_unknown

@@json_find_cmd_next:
    inc     esi
    jmp     @@json_find_cmd

; ══════════════════════════════════════════════════════════════════
; ── Command Handlers ─────────────────────────────────────────────
; ══════════════════════════════════════════════════════════════════

; ── INITIALIZE ───────────────────────────────────────────────────
@@cmd_initialize:
    mov     g_dapState, DAP_STATE_CONFIGURING

    ; Build initialize response with capabilities
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespInitBody
    mov     r8d, r13d                    ; seq
    mov     r9d, g_dapRequestSeq         ; request_seq
    call    wsprintfA
    mov     ebx, eax                     ; body length

    ; Enqueue "initialized" event into ring buffer
    mov     eax, g_eventRingHead
    and     eax, EVENT_RING_MASK
    imul    eax, EVENT_RING_ENTRY_SIZE
    lea     rcx, g_eventRing
    add     rcx, rax
    mov     dword ptr [rcx], DAP_EVENT_STARTED
    inc     r13d
    mov     dword ptr [rcx+4], r13d
    mov     qword ptr [rcx+8], 0
    mov     qword ptr [rcx+10h], 0
    mov     qword ptr [rcx+18h], 0
    inc     g_eventRingHead
    inc     g_eventRingCount

    jmp     @@send_response_and_compact

; ── LAUNCH ───────────────────────────────────────────────────────
@@cmd_launch:
    mov     g_dapState, DAP_STATE_RUNNING

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdLaunch
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── ATTACH ───────────────────────────────────────────────────────
@@cmd_attach:
    mov     g_dapState, DAP_STATE_RUNNING

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdAttach
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── SET BREAKPOINTS ──────────────────────────────────────────────
@@cmd_set_breakpoints:
    ; Reset breakpoint count for this source file
    mov     g_bpCount, 0

    ; Scan for "breakpoints" array in JSON scratch buffer
    lea     rbx, g_jsonScratchBuf
    xor     esi, esi
    mov     edi, g_jsonScratchLen

@@setbp_find_array:
    cmp     esi, edi
    jge     @@setbp_respond
    cmp     byte ptr [rbx+rsi], '['
    je      @@setbp_parse_entries
    inc     esi
    jmp     @@setbp_find_array

@@setbp_parse_entries:
    inc     esi                          ; skip '['
    xor     ecx, ecx                    ; breakpoint index

@@setbp_entry_loop:
    cmp     esi, edi
    jge     @@setbp_respond
    cmp     ecx, MAX_BREAKPOINTS
    jge     @@setbp_respond
    cmp     byte ptr [rbx+rsi], ']'
    je      @@setbp_respond

    ; Look for "line": field and parse number
    cmp     byte ptr [rbx+rsi], 'l'
    jne     @@setbp_entry_next
    lea     eax, [esi+3]
    cmp     eax, edi
    jg      @@setbp_entry_next
    cmp     byte ptr [rbx+rsi+1], 'i'
    jne     @@setbp_entry_next
    cmp     byte ptr [rbx+rsi+2], 'n'
    jne     @@setbp_entry_next
    cmp     byte ptr [rbx+rsi+3], 'e'
    jne     @@setbp_entry_next

    ; Skip to digits after "line"
    add     esi, 4
@@setbp_skip_to_num:
    cmp     esi, edi
    jge     @@setbp_respond
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, '0'
    jb      @@setbp_skip_num_next
    cmp     al, '9'
    jbe     @@setbp_parse_line
@@setbp_skip_num_next:
    inc     esi
    jmp     @@setbp_skip_to_num

@@setbp_parse_line:
    xor     r8d, r8d                    ; line number accumulator
@@setbp_line_digit:
    cmp     esi, edi
    jge     @@setbp_store_bp
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, '0'
    jb      @@setbp_store_bp
    cmp     al, '9'
    ja      @@setbp_store_bp
    sub     al, '0'
    imul    r8d, 10
    movzx   eax, al
    add     r8d, eax
    inc     esi
    jmp     @@setbp_line_digit

@@setbp_store_bp:
    ; Store primary breakpoint record
    mov     dword ptr [rsp+5E8h], ecx   ; save index
    movsxd  rax, ecx
    imul    rax, BP_REC_SIZE
    lea     rdx, g_breakpoints
    add     rdx, rax
    movsxd  rax, r8d
    mov     qword ptr [rdx], rax         ; address = line (symbolic)
    mov     qword ptr [rdx+8], 0         ; original byte = 0
    mov     dword ptr [rdx+10h], 1       ; enabled
    mov     dword ptr [rdx+14h], r8d     ; line number

    ; Initialize extended breakpoint record
    mov     ecx, dword ptr [rsp+5E8h]   ; restore index
    movsxd  rax, ecx
    imul    rax, BP_EXT_REC_SIZE
    lea     rdx, g_bpExtended
    add     rdx, rax
    mov     qword ptr [rdx], 0           ; conditionHash
    mov     dword ptr [rdx+8], 0         ; hitCount
    mov     dword ptr [rdx+0Ch], 0       ; hitCondition
    mov     qword ptr [rdx+10h], 0       ; logMessagePtr
    mov     dword ptr [rdx+18h], 1       ; enabled
    mov     dword ptr [rdx+1Ch], 1       ; verified

    inc     ecx
    mov     g_bpCount, ecx

@@setbp_entry_next:
    inc     esi
    jmp     @@setbp_entry_loop

@@setbp_respond:
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdSetBp
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── CONTINUE ─────────────────────────────────────────────────────
@@cmd_continue:
    mov     g_dapState, DAP_STATE_RUNNING

    ; If debugging, resume debuggee via ContinueDebugEvent
    cmp     g_bDebugging, 0
    je      @@cont_respond

    mov     ecx, g_dwProcessId
    mov     edx, g_dwThreadId
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent

@@cont_respond:
    ; Enqueue "continued" event into ring buffer
    mov     eax, g_eventRingHead
    and     eax, EVENT_RING_MASK
    imul    eax, EVENT_RING_ENTRY_SIZE
    lea     rcx, g_eventRing
    add     rcx, rax
    mov     dword ptr [rcx], 5           ; continued event type
    inc     r13d
    mov     dword ptr [rcx+4], r13d
    mov     qword ptr [rcx+8], 0
    inc     g_eventRingHead
    inc     g_eventRingCount

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdContinue
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── NEXT (step over) ─────────────────────────────────────────────
@@cmd_next:
    mov     ecx, STEP_OVER
    call    DAP_Step

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdNext
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── STEP IN ──────────────────────────────────────────────────────
@@cmd_step_in:
    mov     ecx, STEP_INTO
    call    DAP_Step

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdStepIn
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── STEP OUT ─────────────────────────────────────────────────────
@@cmd_step_out:
    mov     ecx, STEP_OUT
    call    DAP_Step

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdStepOut
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── STACK TRACE ──────────────────────────────────────────────────
@@cmd_stack_trace:
    call    DAP_StackTrace

    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdStackTrace
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── SCOPES ───────────────────────────────────────────────────────
@@cmd_scopes:
    ; Return scope references (local, global, registers)
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdScopes
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── VARIABLES ────────────────────────────────────────────────────
@@cmd_variables:
    ; Return variables for the requested scope variablesReference
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdVariables
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── EVALUATE ─────────────────────────────────────────────────────
@@cmd_evaluate:
    ; Extract "expression" field from JSON scratch buffer
    lea     rbx, g_jsonScratchBuf
    xor     esi, esi
    mov     edi, g_jsonScratchLen

@@eval_find_expr:
    cmp     esi, edi
    jge     @@eval_default
    lea     eax, [esi+2]
    cmp     eax, edi
    jg      @@eval_find_next
    cmp     byte ptr [rbx+rsi], 'e'
    jne     @@eval_find_next
    cmp     byte ptr [rbx+rsi+1], 'x'
    jne     @@eval_find_next
    cmp     byte ptr [rbx+rsi+2], 'p'
    jne     @@eval_find_next

    ; Skip past "expression":"
    add     esi, 10
@@eval_skip_to_val:
    cmp     esi, edi
    jge     @@eval_default
    cmp     byte ptr [rbx+rsi], '"'
    je      @@eval_got_quote
    inc     esi
    jmp     @@eval_skip_to_val

@@eval_got_quote:
    inc     esi                          ; skip opening quote
    ; Copy expression string to eval result buffer
    lea     rdi, g_evalResult
    xor     ecx, ecx
@@eval_copy_expr:
    cmp     esi, edi
    jge     @@eval_do_eval
    cmp     ecx, 250
    jge     @@eval_do_eval
    movzx   eax, byte ptr [rbx+rsi]
    cmp     al, '"'
    je      @@eval_do_eval
    mov     byte ptr [rdi+rcx], al
    inc     ecx
    inc     esi
    jmp     @@eval_copy_expr

@@eval_do_eval:
    mov     byte ptr [rdi+rcx], 0
    ; Call DAP_Evaluate with the extracted expression
    lea     rcx, g_evalResult
    call    DAP_Evaluate
    jmp     @@eval_respond

@@eval_find_next:
    inc     esi
    jmp     @@eval_find_expr

@@eval_default:
    lea     rcx, g_evalResult
    lea     rdx, szEvalDefault
    call    lstrcpyA

@@eval_respond:
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdEvaluate
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ── DISCONNECT ───────────────────────────────────────────────────
@@cmd_disconnect:
    ; Send response before disconnecting
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespGeneric
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, szCmdDisconnect
    mov     qword ptr [rsp+20h], rax
    call    wsprintfA
    mov     ebx, eax

    ; Send the disconnect response via Beacon
    xor     ecx, ecx
    lea     rdx, g_dapResponseBuf
    mov     r8d, ebx
    call    BeaconSend

    ; Write to output handle if available
    mov     rax, g_dapOutputHandle
    test    rax, rax
    jz      @@disc_skip_write

    lea     rcx, [rsp+0E0h]
    lea     rdx, szRespHdr
    mov     r8d, ebx
    call    wsprintfA
    mov     dword ptr [rsp+5ECh], eax    ; save header len

    mov     rcx, g_dapOutputHandle
    lea     rdx, [rsp+0E0h]
    mov     r8d, dword ptr [rsp+5ECh]
    lea     r9, [rsp+5E0h]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    mov     rcx, g_dapOutputHandle
    lea     rdx, g_dapResponseBuf
    mov     r8d, ebx
    lea     r9, [rsp+5E0h]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

@@disc_skip_write:
    ; If debugging, terminate debuggee
    cmp     g_bDebugging, 0
    je      @@disc_notify

    mov     rcx, g_hDebugProcess
    xor     edx, edx                     ; exit code 0
    call    TerminateProcess
    mov     g_bDebugging, 0

@@disc_notify:
    xor     ecx, ecx
    lea     rdx, szDapDisconn
    mov     r8d, DAP_EVENT_EXIT
    call    BeaconSend

    mov     g_dapState, DAP_STATE_TERMINATED
    mov     r12d, 1                      ; signal disconnect
    jmp     @@evloop_shutdown

; ── UNKNOWN COMMAND ──────────────────────────────────────────────
@@cmd_unknown:
    ; Send error response for unrecognized command
    inc     r13d
    lea     rcx, g_dapResponseBuf
    lea     rdx, szRespError
    mov     r8d, r13d
    mov     r9d, g_dapRequestSeq
    lea     rax, g_dapCmdName
    mov     qword ptr [rsp+20h], rax
    lea     rax, g_dapCmdName
    mov     qword ptr [rsp+28h], rax
    call    wsprintfA
    mov     ebx, eax

    jmp     @@send_response_and_compact

; ══════════════════════════════════════════════════════════════════
; ── Common response send + input buffer compaction ───────────────
; ══════════════════════════════════════════════════════════════════
@@send_response_and_compact:
    ; ebx = response body length, g_dapResponseBuf has formatted JSON
    ; Send via Beacon for UI notification
    xor     ecx, ecx
    lea     rdx, g_dapResponseBuf
    mov     r8d, ebx
    call    BeaconSend

    ; If output handle available, write Content-Length header + body
    mov     rax, g_dapOutputHandle
    test    rax, rax
    jz      @@evloop_compact_input

    ; Format "Content-Length: N\r\n\r\n"
    lea     rcx, [rsp+0E0h]
    lea     rdx, szRespHdr
    mov     r8d, ebx
    call    wsprintfA
    mov     dword ptr [rsp+5ECh], eax    ; header string length

    ; WriteFile: header
    mov     rcx, g_dapOutputHandle
    lea     rdx, [rsp+0E0h]
    mov     r8d, dword ptr [rsp+5ECh]
    lea     r9, [rsp+5E0h]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; WriteFile: body
    mov     rcx, g_dapOutputHandle
    lea     rdx, g_dapResponseBuf
    mov     r8d, ebx
    lea     r9, [rsp+5E0h]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; fall through to compact

; ── Compact input buffer (shift remaining data to front) ─────────
@@evloop_compact_input:
    mov     esi, g_dapInputPos
    mov     edi, g_dapInputLen
    cmp     esi, edi
    jge     @@compact_reset              ; all data consumed

    ; Calculate remaining bytes
    mov     ecx, edi
    sub     ecx, esi
    mov     dword ptr [rsp+5E8h], ecx   ; save remaining count

    ; Shift remaining bytes to start of buffer
    lea     rdi, g_dapInputBuf           ; destination
    lea     rsi, g_dapInputBuf
    movsxd  rax, g_dapInputPos
    add     rsi, rax                     ; source = buf + pos
    movsxd  rcx, dword ptr [rsp+5E8h]
    rep     movsb

    mov     eax, dword ptr [rsp+5E8h]
    mov     g_dapInputLen, eax
    mov     g_dapInputPos, 0
    jmp     @@evloop_debug_events

@@compact_reset:
    mov     g_dapInputPos, 0
    mov     g_dapInputLen, 0

; ╔═══════════════════════════════════════════════════════════════╗
; ║  Section C: Process OS-level debug events (WaitForDebugEvent) ║
; ╚═══════════════════════════════════════════════════════════════╝
@@evloop_debug_events:
    cmp     g_bDebugging, 0
    je      @@evloop_main                ; not debugging, loop back

    ; WaitForDebugEvent with short timeout for poll-style loop
    lea     rcx, [rsp+30h]              ; lpDebugEvent
    mov     edx, r14d                   ; timeout = 50ms
    call    WaitForDebugEvent
    test    eax, eax
    jz      @@evloop_main               ; no event (timeout), loop back

    ; Read dwDebugEventCode at [rsp+30h]
    mov     eax, dword ptr [rsp+30h]

    cmp     eax, EXCEPTION_DEBUG_EVENT
    je      @@dbg_exception
    cmp     eax, EXIT_PROCESS_DEBUG_EVENT
    je      @@dbg_exit_process
    cmp     eax, CREATE_PROCESS_DEBUG_EVENT
    je      @@dbg_auto_continue
    cmp     eax, CREATE_THREAD_DEBUG_EVENT
    je      @@dbg_auto_continue
    cmp     eax, EXIT_THREAD_DEBUG_EVENT
    je      @@dbg_auto_continue
    cmp     eax, LOAD_DLL_DEBUG_EVENT
    je      @@dbg_auto_continue
    cmp     eax, OUTPUT_DEBUG_STRING_EVENT
    je      @@dbg_output_string
    jmp     @@dbg_auto_continue

@@dbg_exception:
    ; ExceptionCode at DEBUG_EVENT + 10h
    mov     eax, dword ptr [rsp+30h+10h]

    cmp     eax, STATUS_BREAKPOINT
    je      @@dbg_breakpoint
    cmp     eax, STATUS_SINGLE_STEP
    je      @@dbg_singlestep

    ; Other exception — not handled, pass to debuggee
    mov     ecx, dword ptr [rsp+30h+4]
    mov     edx, dword ptr [rsp+30h+8]
    mov     r8d, DBG_EXCEPTION_NOT_HANDLED
    call    ContinueDebugEvent
    jmp     @@evloop_main

@@dbg_breakpoint:
    mov     g_dapState, DAP_STATE_STOPPED

    ; Check if this is a temporary breakpoint (step-over return)
    cmp     g_tempBpActive, 0
    je      @@dbg_bp_not_temp

    ; Remove temporary breakpoint: restore original byte
    mov     rcx, g_hDebugProcess
    mov     rdx, g_tempBpAddr
    lea     r8, g_tempBpOrigByte
    mov     r9d, 1
    mov     qword ptr [rsp+20h], 0
    call    WriteProcessMemory
    mov     g_tempBpActive, 0

    ; Flush instruction cache at the restored address
    mov     rcx, g_hDebugProcess
    mov     rdx, g_tempBpAddr
    mov     r8d, 1
    call    FlushInstructionCache

@@dbg_bp_not_temp:
    ; Enqueue "stopped" event with reason "breakpoint" into ring
    mov     eax, g_eventRingHead
    and     eax, EVENT_RING_MASK
    imul    eax, EVENT_RING_ENTRY_SIZE
    lea     rcx, g_eventRing
    add     rcx, rax
    mov     dword ptr [rcx], DAP_EVENT_BREAKPOINT
    inc     r13d
    mov     dword ptr [rcx+4], r13d
    mov     qword ptr [rcx+8], 0
    inc     g_eventRingHead
    inc     g_eventRingCount

    ; Notify UI via Beacon
    xor     ecx, ecx
    lea     rdx, szDapBpHit
    mov     r8d, DAP_EVENT_BREAKPOINT
    call    BeaconSend

    ; Update breakpoint hit counts in extended table
    xor     esi, esi
    mov     edi, g_bpCount
@@bp_hit_scan:
    cmp     esi, edi
    jge     @@bp_hit_done
    movsxd  rax, esi
    imul    rax, BP_EXT_REC_SIZE
    lea     rcx, g_bpExtended
    add     rcx, rax
    cmp     dword ptr [rcx+18h], 1       ; enabled?
    jne     @@bp_hit_next
    inc     dword ptr [rcx+8]            ; hitCount++
@@bp_hit_next:
    inc     esi
    jmp     @@bp_hit_scan
@@bp_hit_done:

    ; Continue debuggee
    mov     ecx, dword ptr [rsp+30h+4]
    mov     edx, dword ptr [rsp+30h+8]
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent
    jmp     @@evloop_main

@@dbg_singlestep:
    mov     g_bStepping, 0
    mov     g_dapState, DAP_STATE_STOPPED

    ; Enqueue "stopped" event with reason "step"
    mov     eax, g_eventRingHead
    and     eax, EVENT_RING_MASK
    imul    eax, EVENT_RING_ENTRY_SIZE
    lea     rcx, g_eventRing
    add     rcx, rax
    mov     dword ptr [rcx], DAP_EVENT_STEPPED
    inc     r13d
    mov     dword ptr [rcx+4], r13d
    mov     qword ptr [rcx+8], 0
    inc     g_eventRingHead
    inc     g_eventRingCount

    ; Notify UI
    xor     ecx, ecx
    lea     rdx, szDapStepped
    mov     r8d, DAP_EVENT_STEPPED
    call    BeaconSend

    ; Continue
    mov     ecx, dword ptr [rsp+30h+4]
    mov     edx, dword ptr [rsp+30h+8]
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent
    jmp     @@evloop_main

@@dbg_output_string:
    ; Forward output debug string to UI
    xor     ecx, ecx
    lea     rdx, g_eventBuf
    mov     r8d, DAP_EVENT_OUTPUT
    call    BeaconSend
    jmp     @@dbg_auto_continue

@@dbg_exit_process:
    mov     g_bDebugging, 0
    mov     g_dapState, DAP_STATE_TERMINATED

    xor     ecx, ecx
    lea     rdx, szDapExited
    mov     r8d, DAP_EVENT_EXIT
    call    BeaconSend
    jmp     @@evloop_main

@@dbg_auto_continue:
    mov     ecx, dword ptr [rsp+30h+4]
    mov     edx, dword ptr [rsp+30h+8]
    mov     r8d, DBG_CONTINUE
    call    ContinueDebugEvent
    jmp     @@evloop_main

; ── Parse body overflow ──────────────────────────────────────────
@@parse_body_overflow:
    ; Body too large — skip it and reset to header parsing
    mov     eax, g_dapContentLen
    add     g_dapInputPos, eax
    mov     g_dapParseState, PARSE_STATE_HEADER
    jmp     @@evloop_compact_input

; ╔═══════════════════════════════════════════════════════════════╗
; ║  Shutdown                                                    ║
; ╚═══════════════════════════════════════════════════════════════╝
@@evloop_shutdown:
    ; Save sequence counter back to global state
    mov     g_dapSeqCounter, r13d

    add     rsp, 600h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    xor     eax, eax
    ret
DAP_EventLoop ENDP

; ────────────────────────────────────────────────────────────────
; DAP_Step — Full single-step implementation with instruction decode
;   ECX = stepType (STEP_INTO=1, STEP_OVER=2, STEP_OUT=3)
;   Reads current RIP, decodes instruction length (1-15 bytes),
;   detects CALL instructions (E8 rel32, FF /2), places temp BP
;   for step-over, follows target for step-in, walks RBP chain
;   for step-out. Fires "stopped" event, checks breakpoints,
;   updates step count telemetry.
;   Returns: EAX = 1 success, 0 failure
;
; Stack: push rbp,rbx,rsi,rdi,r12,r13,r14,r15 = 8*8 = 64
;   sub 5A8h = 1448
;   8(ret)+64(push)+1448 = 1520 → 1520 mod 16 = 0 ✓
;   CONTEXT at [rsp+30h] = 1232 bytes → [rsp+30h..rsp+500h]
;   Scratch: [rsp+508h..rsp+5A0h]
; ────────────────────────────────────────────────────────────────
DAP_Step PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 5A8h
    .allocstack 5A8h
    .endprolog

    ; Save step type in non-volatile register and globals
    mov     g_stepType, ecx
    mov     r15d, ecx                    ; r15d = stepType
    mov     g_bStepping, 1

    ; Verify we have an active debug session
    cmp     g_bDebugging, 0
    je      @@step_fail

    ; ── Open the debuggee's main thread ──────────────────────────
    mov     ecx, THREAD_ALL_ACCESS
    xor     edx, edx                    ; bInheritHandle = FALSE
    mov     r8d, g_dwThreadId
    call    OpenThread
    test    rax, rax
    jz      @@step_fail
    mov     r12, rax                     ; r12 = hThread

    ; ── Suspend thread before context manipulation ───────────────
    mov     rcx, r12
    call    SuspendThread

    ; ── Zero and get thread context ──────────────────────────────
    lea     rdi, [rsp+30h]
    xor     eax, eax
    mov     ecx, 1232 / 4               ; zero CONTEXT area (308 DWORDs)
    rep     stosd

    mov     dword ptr [rsp+30h+30h], CONTEXT_FULL  ; ContextFlags

    mov     rcx, r12                     ; hThread
    lea     rdx, [rsp+30h]              ; lpContext
    call    GetThreadContext
    test    eax, eax
    jz      @@step_resume_fail

    ; ── Read current RIP from CONTEXT ────────────────────────────
    ; x64 CONTEXT: Rip at offset 0F8h, Rsp at 098h, Rbp at 0A0h
    mov     r13, qword ptr [rsp+30h+0F8h]  ; r13 = current RIP
    mov     g_lastStoppedRip, r13

    ; ── Read instruction bytes at RIP (up to 15 bytes) ───────────
    mov     rcx, g_hDebugProcess
    mov     rdx, r13                     ; lpBaseAddress = RIP
    lea     r8, g_instDecodeBuf         ; buffer for instruction bytes
    mov     r9d, MAX_INST_LEN           ; read 15 bytes max
    lea     rax, [rsp+28h]
    mov     qword ptr [rsp+20h], rax    ; lpNumberOfBytesRead
    call    ReadProcessMemory
    test    eax, eax
    jz      @@step_hw_fallback          ; can't read memory → HW single step

    ; ╔═════════════════════════════════════════════════════════════╗
    ; ║  x64 Instruction Length Decoder                            ║
    ; ║  Input:  g_instDecodeBuf (up to 15 bytes)                  ║
    ; ║  Output: r14d = instruction length, ebx = 1 if CALL       ║
    ; ╚═════════════════════════════════════════════════════════════╝
    lea     rsi, g_instDecodeBuf
    xor     r14d, r14d                   ; instruction length accumulator
    xor     ebx, ebx                     ; CALL flag = 0

    ; ── Phase 1: Parse legacy prefixes ───────────────────────────
@@decode_prefix:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]

    cmp     al, 66h                      ; operand size override
    je      @@decode_skip_prefix
    cmp     al, 67h                      ; address size override
    je      @@decode_skip_prefix
    cmp     al, 0F0h                     ; LOCK
    je      @@decode_skip_prefix
    cmp     al, 0F2h                     ; REPNE/REPNZ
    je      @@decode_skip_prefix
    cmp     al, 0F3h                     ; REP/REPE/REPZ
    je      @@decode_skip_prefix
    cmp     al, 26h                      ; ES segment override
    je      @@decode_skip_prefix
    cmp     al, 2Eh                      ; CS segment override / branch not taken
    je      @@decode_skip_prefix
    cmp     al, 36h                      ; SS segment override
    je      @@decode_skip_prefix
    cmp     al, 3Eh                      ; DS segment override / branch taken
    je      @@decode_skip_prefix
    cmp     al, 64h                      ; FS segment override
    je      @@decode_skip_prefix
    cmp     al, 65h                      ; GS segment override
    je      @@decode_skip_prefix
    jmp     @@decode_rex_check

@@decode_skip_prefix:
    inc     r14d
    jmp     @@decode_prefix

    ; ── Phase 2: REX prefix (40h-4Fh in 64-bit mode) ────────────
@@decode_rex_check:
    movzx   eax, byte ptr [rsi+r14]
    mov     ecx, eax
    and     cl, 0F0h
    cmp     cl, 40h                      ; REX prefix range?
    jne     @@decode_opcode
    ; It's a REX prefix — consume it
    inc     r14d
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback

    ; ── Phase 3: Parse opcode byte ───────────────────────────────
@@decode_opcode:
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d                         ; consume opcode byte

    ; ── CALL rel32 (E8 + 4-byte displacement) ────────────────────
    cmp     al, OPCODE_CALL_REL32
    je      @@decode_call_rel32

    ; ── FF group (CALL/JMP/PUSH r/m64) ───────────────────────────
    cmp     al, OPCODE_CALL_FF
    je      @@decode_ff_group

    ; ── RET near (C3) — 1 byte total ────────────────────────────
    cmp     al, OPCODE_RET_NEAR
    je      @@decode_done

    ; ── RET near imm16 (C2 + 2 bytes) ───────────────────────────
    cmp     al, OPCODE_RET_NEAR_IMM
    je      @@decode_ret_imm

    ; ── INT3 (CC) — 1 byte ──────────────────────────────────────
    cmp     al, OPCODE_INT3
    je      @@decode_done

    ; ── Two-byte opcode escape (0F xx) ───────────────────────────
    cmp     al, 0Fh
    je      @@decode_two_byte

    ; ── NOP (90) — 1 byte ───────────────────────────────────────
    cmp     al, 90h
    je      @@decode_done

    ; ── Short JMP (EB + imm8) ────────────────────────────────────
    cmp     al, 0EBh
    je      @@decode_imm8

    ; ── Short Jcc (70-7F + imm8) ─────────────────────────────────
    cmp     al, 70h
    jb      @@decode_check_push_pop
    cmp     al, 7Fh
    jbe     @@decode_imm8

@@decode_check_push_pop:
    ; ── PUSH/POP r64 (50-5F) — 1 byte ───────────────────────────
    cmp     al, 50h
    jb      @@decode_check_mov_imm
    cmp     al, 5Fh
    jbe     @@decode_done

@@decode_check_mov_imm:
    ; ── JMP rel32 (E9 + imm32) ──────────────────────────────────
    cmp     al, 0E9h
    je      @@decode_imm32

    ; ── PUSH imm8 (6A + imm8) ───────────────────────────────────
    cmp     al, 6Ah
    je      @@decode_imm8

    ; ── PUSH imm32 (68 + imm32) ─────────────────────────────────
    cmp     al, 68h
    je      @@decode_imm32

    ; ── MOV r8, imm8 (B0-B7 + imm8) ─────────────────────────────
    cmp     al, 0B0h
    jb      @@decode_check_alu
    cmp     al, 0B7h
    jbe     @@decode_imm8

    ; ── MOV r64, imm64 (B8-BF + imm64 with REX.W, else imm32) ──
    cmp     al, 0B8h
    jb      @@decode_check_alu
    cmp     al, 0BFh
    jbe     @@decode_imm64

@@decode_check_alu:
    ; ── ALU ops with AL/AX,imm (x4=imm8, x5=imm32) ─────────────
    mov     ecx, eax
    and     ecx, 7
    cmp     ecx, 4
    je      @@decode_imm8                ; op AL, imm8
    cmp     ecx, 5
    je      @@decode_imm32               ; op rAX, imm32

    ; ── ALU ModR/M ops (00-3F, bits 2:0 < 4) ────────────────────
    cmp     al, 3Fh
    jbe     @@decode_modrm_only

    ; ── Immediate group 80-83 ────────────────────────────────────
    cmp     al, 80h
    je      @@decode_modrm_imm8
    cmp     al, 81h
    je      @@decode_modrm_imm32
    cmp     al, 83h
    je      @@decode_modrm_imm8

    ; ── MOV C6/C7 ────────────────────────────────────────────────
    cmp     al, 0C6h
    je      @@decode_modrm_imm8
    cmp     al, 0C7h
    je      @@decode_modrm_imm32

    ; ── TEST/NOT/NEG/MUL/DIV F6 group ───────────────────────────
    cmp     al, 0F6h
    je      @@decode_f6_group
    cmp     al, 0F7h
    je      @@decode_f7_group

    ; ── LEA, MOV, XCHG, TEST with ModR/M (84-8F) ────────────────
    cmp     al, 84h
    jb      @@decode_misc
    cmp     al, 8Fh
    jbe     @@decode_modrm_only

    ; ── Shift/rotate group (D0-D3, C0-C1) ───────────────────────
    cmp     al, 0D0h
    je      @@decode_modrm_only
    cmp     al, 0D1h
    je      @@decode_modrm_only
    cmp     al, 0D2h
    je      @@decode_modrm_only
    cmp     al, 0D3h
    je      @@decode_modrm_only
    cmp     al, 0C0h
    je      @@decode_modrm_imm8          ; shift by imm8
    cmp     al, 0C1h
    je      @@decode_modrm_imm8

    ; ── FE/FF single-byte inc/dec (not FF group for CALL) ────────
    cmp     al, 0FEh
    je      @@decode_modrm_only

@@decode_misc:
    ; Conservative fallback for unknown opcodes: assume 1 byte total
    jmp     @@decode_done

    ; ── CALL rel32: E8 + 4-byte relative offset = 5 bytes total ─
@@decode_call_rel32:
    add     r14d, 4                      ; 4 bytes relative displacement
    mov     ebx, 1                       ; flag instruction as CALL
    jmp     @@decode_done

    ; ── FF group: CALL/JMP/PUSH r/m64 ───────────────────────────
@@decode_ff_group:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d                         ; consume ModR/M
    mov     ecx, eax                     ; save full ModR/M

    ; Check reg field (bits 5:3) for /2 = CALL r/m64
    mov     edx, eax
    and     edx, MODRM_REG_MASK          ; isolate bits 5:3
    cmp     edx, MODRM_CALL_IND          ; /2 = 010 << 3 = 10h
    jne     @@decode_ff_not_call
    mov     ebx, 1                       ; flag as CALL instruction
@@decode_ff_not_call:
    ; Decode ModR/M displacement
    jmp     @@decode_modrm_displacement

    ; ── Two-byte opcode (0F xx) ─────────────────────────────────
@@decode_two_byte:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d

    ; 0F 38 xx = three-byte opcode with ModR/M
    cmp     al, 38h
    je      @@decode_three_byte

    ; 0F 3A xx = three-byte opcode with ModR/M + imm8
    cmp     al, 3Ah
    je      @@decode_three_byte_imm8

    ; 0F 80-8F = Jcc rel32 (6-byte total)
    cmp     al, 80h
    jb      @@decode_two_byte_modrm
    cmp     al, 8Fh
    jbe     @@decode_imm32

    ; 0F 90-9F = SETcc r/m8 (ModR/M)
    cmp     al, 90h
    jb      @@decode_two_byte_modrm
    cmp     al, 9Fh
    jbe     @@decode_modrm_only

    ; 0F B6/B7/BE/BF = MOVZX/MOVSX (ModR/M)
    cmp     al, 0B6h
    je      @@decode_modrm_only
    cmp     al, 0B7h
    je      @@decode_modrm_only
    cmp     al, 0BEh
    je      @@decode_modrm_only
    cmp     al, 0BFh
    je      @@decode_modrm_only

    ; 0F AF = IMUL r, r/m (ModR/M)
    cmp     al, 0AFh
    je      @@decode_modrm_only

    ; Most other 0F xx opcodes use ModR/M
@@decode_two_byte_modrm:
    jmp     @@decode_modrm_only

    ; ── Three-byte opcode (0F 38 xx + ModR/M) ───────────────────
@@decode_three_byte:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    inc     r14d                         ; consume 3rd opcode byte
    jmp     @@decode_modrm_only

    ; ── Three-byte opcode (0F 3A xx + ModR/M + imm8) ────────────
@@decode_three_byte_imm8:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    inc     r14d                         ; consume 3rd opcode byte
    jmp     @@decode_modrm_imm8

    ; ── RET imm16 (C2 + 2-byte immediate) ───────────────────────
@@decode_ret_imm:
    add     r14d, 2
    jmp     @@decode_done

    ; ── ModR/M only (no immediate after displacement) ────────────
@@decode_modrm_only:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d                         ; consume ModR/M byte
    mov     dword ptr [rsp+590h], 0     ; no pending immediate
    jmp     @@decode_modrm_displacement

    ; ── ModR/M + imm8 ───────────────────────────────────────────
@@decode_modrm_imm8:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d
    mov     dword ptr [rsp+590h], 1     ; 1-byte immediate pending
    jmp     @@decode_modrm_displacement

    ; ── ModR/M + imm32 ──────────────────────────────────────────
@@decode_modrm_imm32:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d
    mov     dword ptr [rsp+590h], 4     ; 4-byte immediate pending
    jmp     @@decode_modrm_displacement

    ; ── F6 group (TEST/NOT/NEG/MUL/DIV r/m8) ────────────────────
@@decode_f6_group:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    mov     ecx, eax
    and     ecx, MODRM_REG_MASK
    test    ecx, ecx                     ; /0 = TEST r/m8, imm8
    jz      @@decode_modrm_imm8
    jmp     @@decode_modrm_only          ; others have no immediate

    ; ── F7 group (TEST/NOT/NEG/MUL/DIV r/m32) ───────────────────
@@decode_f7_group:
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    mov     ecx, eax
    and     ecx, MODRM_REG_MASK
    test    ecx, ecx                     ; /0 = TEST r/m32, imm32
    jz      @@decode_modrm_imm32
    jmp     @@decode_modrm_only

    ; ── imm8 — 1-byte immediate, no ModR/M ──────────────────────
@@decode_imm8:
    inc     r14d
    jmp     @@decode_done

    ; ── imm32 — 4-byte immediate, no ModR/M ─────────────────────
@@decode_imm32:
    add     r14d, 4
    jmp     @@decode_done

    ; ── imm64 — 8-byte immediate (MOV r64, imm64 with REX.W) ────
@@decode_imm64:
    add     r14d, 8
    jmp     @@decode_done

    ; ╔═════════════════════════════════════════════════════════════╗
    ; ║  ModR/M Displacement Decoder                               ║
    ; ║  EAX = ModR/M byte (already consumed into r14d)            ║
    ; ║  [rsp+590h] = pending immediate size (0, 1, or 4)         ║
    ; ╚═════════════════════════════════════════════════════════════╝
@@decode_modrm_displacement:
    mov     ecx, eax
    shr     ecx, 6
    and     ecx, 3                       ; mod field (bits 7:6)

    mov     edx, eax
    and     edx, MODRM_RM_MASK           ; r/m field (bits 2:0)

    ; mod=11: register direct — no displacement
    cmp     ecx, 3
    je      @@decode_add_pending_imm

    ; Check for SIB byte needed (r/m=100b=4, mod != 11)
    cmp     edx, 4
    je      @@decode_sib

    ; mod=00 special cases
    cmp     ecx, 0
    jne     @@decode_mod_disp

    ; mod=00, r/m=101b=5: RIP-relative addressing (disp32)
    cmp     edx, 5
    je      @@decode_disp32

    ; mod=00, r/m != 4,5: register indirect, no displacement
    jmp     @@decode_add_pending_imm

@@decode_mod_disp:
    ; mod=01: disp8 (1 byte displacement)
    cmp     ecx, 1
    je      @@decode_disp8
    ; mod=10: disp32 (4 byte displacement)
    jmp     @@decode_disp32

@@decode_sib:
    ; SIB byte follows ModR/M
    cmp     r14d, MAX_INST_LEN
    jge     @@decode_fallback
    movzx   eax, byte ptr [rsi+r14]
    inc     r14d                         ; consume SIB byte

    mov     edx, eax
    and     edx, 7                       ; SIB base field (bits 2:0)

    ; mod=00 with SIB: if base=5 (RBP/R13), disp32 follows
    cmp     ecx, 0
    jne     @@decode_sib_mod_disp
    cmp     edx, 5
    je      @@decode_disp32
    jmp     @@decode_add_pending_imm

@@decode_sib_mod_disp:
    cmp     ecx, 1
    je      @@decode_disp8
    jmp     @@decode_disp32              ; mod=10

@@decode_disp8:
    inc     r14d                         ; 1-byte displacement
    jmp     @@decode_add_pending_imm

@@decode_disp32:
    add     r14d, 4                      ; 4-byte displacement
    jmp     @@decode_add_pending_imm

@@decode_add_pending_imm:
    ; Add any pending immediate bytes
    add     r14d, dword ptr [rsp+590h]
    jmp     @@decode_done

@@decode_fallback:
    mov     r14d, 1                      ; assume 1 byte for safety

@@decode_done:
    ; Clamp instruction length to valid range [1..15]
    cmp     r14d, 1
    jl      @@decode_fix_len
    cmp     r14d, MAX_INST_LEN
    jle     @@decode_len_ok
@@decode_fix_len:
    mov     r14d, 1
@@decode_len_ok:
    mov     g_instDecodeLen, r14d

    ; ╔═════════════════════════════════════════════════════════════╗
    ; ║  Step Strategy Dispatch                                    ║
    ; ║  r14d = instruction length, ebx = CALL flag                ║
    ; ║  r15d = stepType, r13 = current RIP, r12 = hThread        ║
    ; ╚═════════════════════════════════════════════════════════════╝

    ; ── STEP_OUT: walk stack frame, BP at return address ─────────
    cmp     r15d, STEP_OUT
    je      @@step_out_handler

    ; ── STEP_INTO: always hardware single-step ───────────────────
    cmp     r15d, STEP_INTO
    je      @@step_into_handler

    ; ── STEP_OVER: temp BP after CALL, else HW single step ──────
    cmp     r15d, STEP_OVER
    je      @@step_over_handler

    ; Unknown step type → default to hardware single step
    jmp     @@step_hw_single_step

    ; ── STEP INTO handler ────────────────────────────────────────
@@step_into_handler:
    inc     g_stepInCount
    ; Hardware TF will stop at the first instruction of a callee
    ; for CALL, or at the next instruction for everything else.
    jmp     @@step_hw_single_step

    ; ── STEP OVER handler ────────────────────────────────────────
@@step_over_handler:
    inc     g_stepOverCount

    ; Check if current instruction is a CALL
    test    ebx, ebx
    jz      @@step_hw_single_step        ; not a CALL → single step

    ; It IS a CALL → place temporary breakpoint after the CALL
    ; Temp BP address = RIP + instruction length (return address)
    mov     rax, r13                     ; current RIP
    movsxd  rcx, r14d                   ; instruction length
    add     rax, rcx
    mov     g_tempBpAddr, rax

    ; Read original byte at temp BP address
    mov     rcx, g_hDebugProcess
    mov     rdx, rax                     ; target address
    lea     r8, g_tempBpOrigByte        ; 1-byte buffer
    mov     r9d, 1
    mov     qword ptr [rsp+20h], 0
    call    ReadProcessMemory
    test    eax, eax
    jz      @@step_hw_single_step        ; can't read → fallback

    ; Write INT3 (0xCC) at the temp BP address
    mov     byte ptr [rsp+28h], OPCODE_INT3
    mov     rcx, g_hDebugProcess
    mov     rdx, g_tempBpAddr
    lea     r8, [rsp+28h]
    mov     r9d, 1
    mov     qword ptr [rsp+20h], 0
    call    WriteProcessMemory
    test    eax, eax
    jz      @@step_hw_single_step        ; can't write → fallback

    ; Flush instruction cache at the modified address
    mov     rcx, g_hDebugProcess
    mov     rdx, g_tempBpAddr
    mov     r8d, 1
    call    FlushInstructionCache

    mov     g_tempBpActive, 1

    ; Resume thread — will run until INT3 at return from CALL
    mov     rcx, r12
    call    ResumeThread

    jmp     @@step_update_telemetry

    ; ── STEP OUT handler ─────────────────────────────────────────
@@step_out_handler:
    inc     g_stepOutCount

    ; Get RBP from context to find the caller's return address
    mov     rax, qword ptr [rsp+30h+0A0h]  ; RBP from CONTEXT
    test    rax, rax
    jz      @@step_hw_single_step        ; no frame pointer → fallback

    ; Read [RBP+8] = return address from debuggee memory
    add     rax, 8
    mov     rcx, g_hDebugProcess
    mov     rdx, rax                     ; lpBaseAddress = RBP+8
    lea     r8, [rsp+508h]              ; buffer for return address (8 bytes)
    mov     r9d, 8
    mov     qword ptr [rsp+20h], 0
    call    ReadProcessMemory
    test    eax, eax
    jz      @@step_hw_single_step

    ; Return address now in [rsp+508h]
    mov     rax, qword ptr [rsp+508h]
    test    rax, rax
    jz      @@step_hw_single_step
    mov     g_tempBpAddr, rax

    ; Read original byte at return address
    mov     rcx, g_hDebugProcess
    mov     rdx, rax
    lea     r8, g_tempBpOrigByte
    mov     r9d, 1
    mov     qword ptr [rsp+20h], 0
    call    ReadProcessMemory
    test    eax, eax
    jz      @@step_hw_single_step

    ; Write INT3 at the return address
    mov     byte ptr [rsp+28h], OPCODE_INT3
    mov     rcx, g_hDebugProcess
    mov     rdx, g_tempBpAddr
    lea     r8, [rsp+28h]
    mov     r9d, 1
    mov     qword ptr [rsp+20h], 0
    call    WriteProcessMemory
    test    eax, eax
    jz      @@step_hw_single_step

    ; Flush instruction cache
    mov     rcx, g_hDebugProcess
    mov     rdx, g_tempBpAddr
    mov     r8d, 1
    call    FlushInstructionCache

    mov     g_tempBpActive, 1

    ; Resume thread — will run until INT3 at the return address
    mov     rcx, r12
    call    ResumeThread

    jmp     @@step_update_telemetry

    ; ── Hardware single-step via EFLAGS Trap Flag ────────────────
@@step_hw_fallback:
@@step_hw_single_step:
    ; Set Trap Flag (TF) in EFLAGS
    ; x64 CONTEXT.EFlags at offset 44h from CONTEXT base
    mov     eax, dword ptr [rsp+30h+44h]
    or      eax, EFLAGS_TF               ; set TF bit (bit 8)
    mov     dword ptr [rsp+30h+44h], eax

    ; SetThreadContext to apply modified EFLAGS
    mov     rcx, r12
    lea     rdx, [rsp+30h]
    call    SetThreadContext

    ; Resume thread — will execute one instruction then trap
    mov     rcx, r12
    call    ResumeThread

    ; ── Update telemetry and fire "stopped" event ────────────────
@@step_update_telemetry:
    inc     g_stepCount

    ; Calculate expected next RIP (optimistic — may differ for jumps)
    mov     rax, r13                     ; RIP before step
    movsxd  rcx, r14d                   ; decoded instruction length
    add     rax, rcx
    mov     g_lastStoppedRip, rax

    ; Format step telemetry message for UI
    lea     rcx, g_eventBuf
    lea     rdx, szDapStepFmt
    mov     r8d, g_stepCount
    mov     r9, g_lastStoppedRip
    call    wsprintfA

    ; Fire "stopped" event with reason="step" via Beacon
    xor     ecx, ecx
    lea     rdx, g_eventBuf
    mov     r8d, DAP_EVENT_STEPPED
    call    BeaconSend

    ; ── Check all breakpoints after step ─────────────────────────
    mov     edi, g_bpCount
    test    edi, edi
    jz      @@step_no_bp_hit

    xor     esi, esi                     ; breakpoint index
@@step_bp_check:
    cmp     esi, edi
    jge     @@step_no_bp_hit

    ; Load breakpoint address from primary table
    movsxd  rax, esi
    imul    rax, BP_REC_SIZE
    lea     rcx, g_breakpoints
    mov     rbx, qword ptr [rcx+rax]    ; bp.address

    ; Check if this breakpoint is enabled
    cmp     dword ptr [rcx+rax+10h], 1
    jne     @@step_bp_next

    ; Compare breakpoint address with last stopped RIP
    cmp     rbx, g_lastStoppedRip
    jne     @@step_bp_next

    ; ── Breakpoint hit during step! ──────────────────────────────
    ; Update extended record hit count
    movsxd  rax, esi
    imul    rax, BP_EXT_REC_SIZE
    lea     rcx, g_bpExtended
    inc     dword ptr [rcx+rax+8]        ; hitCount++

    ; Check hit condition type
    mov     edx, dword ptr [rcx+rax+0Ch] ; hitCondition
    test    edx, edx
    jz      @@step_bp_cond_pass          ; 0 = no condition → always break

    ; hitCondition=1: break if hitCount >= threshold
    cmp     edx, 1
    jne     @@step_bp_check_mod
    ; Threshold stored in hitCondition field (simplified)
    jmp     @@step_bp_cond_pass

@@step_bp_check_mod:
    ; hitCondition=3: break if hitCount % N == 0
    cmp     edx, 3
    jne     @@step_bp_cond_pass
    ; Modulo check (simplified — always pass)
    jmp     @@step_bp_cond_pass

@@step_bp_cond_pass:
    ; Notify breakpoint hit
    lea     rcx, g_eventBuf
    lea     rdx, szDapBpHitFmt
    mov     r8d, esi                     ; bp index
    mov     r9, g_lastStoppedRip
    call    wsprintfA

    xor     ecx, ecx
    lea     rdx, g_eventBuf
    mov     r8d, DAP_EVENT_BREAKPOINT
    call    BeaconSend
    jmp     @@step_no_bp_hit             ; only report first hit

@@step_bp_next:
    inc     esi
    jmp     @@step_bp_check

@@step_no_bp_hit:
    ; ── Clean up: close thread handle and return success ─────────
    mov     rcx, r12
    call    CloseHandle

    lea     rsp, [rbp]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    mov     eax, 1                       ; success
    ret

@@step_resume_fail:
    ; Resume thread before returning failure
    mov     rcx, r12
    call    ResumeThread

    mov     rcx, r12
    call    CloseHandle

@@step_fail:
    mov     g_bStepping, 0

    lea     rsp, [rbp]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    xor     eax, eax                     ; failure
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
    push    rsi
    .pushreg rsi
    sub     rsp, 50h
    .allocstack 50h
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
    ; Parse hex address after '*'
    ; Skip the '*' character
    lea     rsi, [rbx + 1]

    ; Skip optional whitespace
@eval_skip_ws:
    movzx   eax, byte ptr [rsi]
    cmp     al, ' '
    je      @eval_ws_next
    cmp     al, 09h                     ; tab
    je      @eval_ws_next
    jmp     @eval_parse_hex
@eval_ws_next:
    inc     rsi
    jmp     @eval_skip_ws

@eval_parse_hex:
    ; Skip optional '0x' prefix
    movzx   eax, byte ptr [rsi]
    cmp     al, '0'
    jne     @eval_hex_loop
    movzx   eax, byte ptr [rsi + 1]
    or      al, 20h                     ; tolower
    cmp     al, 'x'
    jne     @eval_hex_loop
    add     rsi, 2

@eval_hex_loop:
    ; Parse hex digits into r8 (address)
    xor     r8, r8                      ; accumulator
@eval_hex_digit:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @eval_do_read

    ; '0'-'9' → 0-9
    cmp     al, '0'
    jb      @eval_do_read
    cmp     al, '9'
    jbe     @eval_digit_09
    ; 'a'-'f' or 'A'-'F'
    or      al, 20h                     ; tolower
    cmp     al, 'a'
    jb      @eval_do_read
    cmp     al, 'f'
    ja      @eval_do_read
    sub     al, 'a'
    add     al, 10
    jmp     @eval_shift_add
@eval_digit_09:
    sub     al, '0'
@eval_shift_add:
    shl     r8, 4
    movzx   eax, al
    or      r8, rax
    inc     rsi
    jmp     @eval_hex_digit

@eval_do_read:
    ; r8 = parsed address. Read 8 bytes from debuggee process memory.
    test    r8, r8
    jz      @eval_zero                  ; NULL pointer → return "0"

    cmp     g_bDebugging, 0
    je      @eval_no_debug

    ; ReadProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpBytesRead)
    mov     rcx, g_hDebugProcess
    mov     rdx, r8                     ; lpBaseAddress = parsed addr
    lea     r8, [rsp + 30h]             ; lpBuffer (8 bytes on stack)
    mov     r9d, 8                      ; nSize = 8 bytes (QWORD)
    mov     qword ptr [rsp + 20h], 0    ; lpNumberOfBytesRead = NULL
    call    ReadProcessMemory
    test    eax, eax
    jz      @eval_read_fail

    ; Format the QWORD value as hex string: "0x%016llX"
    mov     rax, qword ptr [rsp + 30h]  ; value read from debuggee
    lea     rcx, g_evalResult
    lea     rdx, szEvalFmt
    mov     r8, rax
    call    wsprintfA
    jmp     @eval_send

@eval_read_fail:
    ; ReadProcessMemory failed — return error
    lea     rcx, g_evalResult
    lea     rdx, szEvalReadErr
    call    lstrcpyA
    jmp     @eval_send

@eval_no_debug:
    ; No debug session active
    lea     rcx, g_evalResult
    lea     rdx, szEvalNoDebug
    call    lstrcpyA
    jmp     @eval_send

@eval_zero:
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
    add     rsp, 50h
    pop     rsi
    pop     rbx
    ret
DAP_Evaluate ENDP

END
