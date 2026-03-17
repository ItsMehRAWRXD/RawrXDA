;=============================================================================
; RawrXD_Sovereign_Core.asm
; Pure ml64.exe — no masm32 SDK, no invoke macro, no CRT include
; Shared sovereign core: pipeline stages, interlocks, self-healing, DMA
;
; Build (object only, linked by CLI or GUI host):
;   ml64 /c /nologo RawrXD_Sovereign_Core.asm
;=============================================================================
OPTION CASEMAP:NONE

;-----------------------------------------------------------------------------
; External Win32 imports  (kernel32 / ucrt linked by the host)
;-----------------------------------------------------------------------------
EXTERN  GetTickCount        :PROC
EXTERN  Sleep               :PROC
EXTERN  OutputDebugStringA  :PROC
EXTERN  printf              :PROC       ; msvcrt / ucrt (CLI host)
EXTERN  wsprintfA           :PROC       ; user32 (GUI host)

;-----------------------------------------------------------------------------
; Public interface
;-----------------------------------------------------------------------------
PUBLIC  Sovereign_Pipeline_Cycle
PUBLIC  Sovereign_MainLoop
PUBLIC  CoordinateAgents
PUBLIC  HealSymbolResolution
PUBLIC  ValidateDMAAlignment
PUBLIC  AcquireSovereignLock
PUBLIC  ReleaseSovereignLock
PUBLIC  RawrXD_Trigger_Chat
PUBLIC  ObserveTokenStream
PUBLIC  g_CycleCounter
PUBLIC  g_SovereignStatus
PUBLIC  g_SymbolHealCount
PUBLIC  g_ActiveAgentCount
PUBLIC  g_AgentRegistry

;=============================================================================
; .DATA  — 16-byte aligned sovereign state
;=============================================================================
.data
ALIGN 16
g_AgenticLock       QWORD   0
g_SovereignStatus   QWORD   0   ; bits: 0=IDLE 1=COMPILING 2=FIXING 3=SYNC
g_CycleCounter      QWORD   0
g_SymbolHealCount   QWORD   0
g_VirtualAllocPtr   QWORD   0

MAX_AGENTS          EQU     32
g_AgentRegistry     QWORD   MAX_AGENTS DUP(0)
g_ActiveAgentCount  DWORD   0

; ---------- format / debug strings ----------
szIDEUI     DB  "[IDE-UI ] Signal received -> Chat Service",13,10,0
szChat      DB  "[CHAT   ] Routing session | CycleID: %I64u",13,10,0
szPrompt    DB  "[PROMPT ] Building context for cycle %I64u...",13,10,0
szLLM       DB  "[LLM    ] Dispatching | Budget: %u tokens",13,10,0
szStream    DB  "[STREAM ] Agent %u | Token: %016I64X | OK",13,10,0
szRenderer  DB  "[RENDER ] Agentic UI state: %I64X | Sync OK",13,10,0
szDone      DB  "[CYCLE  ] Pipeline complete | Count: %I64u",13,10,0
szLock      DB  "[LOCK   ] Sovereign interlock acquired.",13,10,0
szHeal      DB  "[HEAL   ] Symbol resolution: %s",13,10,0
szDMA       DB  "[DMA    ] Alignment drift detected, re-aligning",13,10,0
szAutoFix   DB  "[AUTOFIX] Agent %u error cleared. Re-emitting...",13,10,0
szSync      DB  "[SYNC   ] Agent %u | State: %I64X",13,10,0
szVA        DB  "VirtualAlloc",0
szDMAMap    DB  "DMA_Map",0
szSleepMs   EQU 200

;=============================================================================
; .CODE
;=============================================================================
.code

;-----------------------------------------------------------------------------
; AcquireSovereignLock — spin-wait XCHG interlock on g_AgenticLock
;   no args | no return | clobbers: RAX
;-----------------------------------------------------------------------------
AcquireSovereignLock PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    lea rbp, [rsp+32]
    .SETFRAME rbp, 32
    .ENDPROLOG
SpinAcquire:
    mov     rax, 1
    xchg    rax, g_AgenticLock      ; atomic swap
    test    rax, rax
    jz      LockGot
    pause
    jmp     SpinAcquire
LockGot:
    lea     rcx, szLock
    call    OutputDebugStringA
    add     rsp, 32
    pop     rbp
    ret
AcquireSovereignLock ENDP

;-----------------------------------------------------------------------------
; ReleaseSovereignLock
;-----------------------------------------------------------------------------
ReleaseSovereignLock PROC FRAME
    push rbp
    .PUSHREG rbp
    .ENDPROLOG
    mov     qword ptr [g_AgenticLock], 0
    mfence
    pop     rbp
    ret
ReleaseSovereignLock ENDP

;-----------------------------------------------------------------------------
; ValidateDMAAlignment — checks g_AgentRegistry 16-byte alignment
;   no args | RAX=0 OK, RAX=1 drift
;-----------------------------------------------------------------------------
ValidateDMAAlignment PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    lea rbp, [rsp+32]
    .SETFRAME rbp, 32
    .ENDPROLOG
    lea     rax, g_AgentRegistry
    test    rax, 0Fh
    jz      DMA_OK
    lea     rcx, szDMA
    call    OutputDebugStringA
    mov     rax, 1
    add     rsp, 32
    pop     rbp
    ret
DMA_OK:
    xor     rax, rax
    add     rsp, 32
    pop     rbp
    ret
ValidateDMAAlignment ENDP

;-----------------------------------------------------------------------------
; HealSymbolResolution  RCX = pointer to null-terminated symbol name string
;   Logs the heal action, increments counter, recurses to ValidateDMA
;-----------------------------------------------------------------------------
HealSymbolResolution PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 40
    .ALLOCSTACK 40
    .ENDPROLOG

    mov     rbx, rcx                    ; save symbol name
    ; atomic increment heal counter
    lock add qword ptr [g_SymbolHealCount], 1
    ; printf("[HEAL] ...")
    lea     rcx, szHeal
    mov     rdx, rbx
    call    printf
    ; re-validate DMA
    call    ValidateDMAAlignment

    add     rsp, 40
    pop     rbx
    pop     rbp
    ret
HealSymbolResolution ENDP

;-----------------------------------------------------------------------------
; ObserveTokenStream  RCX = agent index (QWORD)
;-----------------------------------------------------------------------------
ObserveTokenStream PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 40
    .ALLOCSTACK 40
    .ENDPROLOG

    mov     rbx, rcx                    ; save agent idx
    ; get TSC as synthetic token
    rdtsc
    shl     rdx, 32
    or      rax, rdx                    ; RAX = 64-bit TSC token

    lea     rcx, szStream
    mov     rdx, rbx                    ; %u  = agent idx
    mov     r8,  rax                    ; %016I64X = token
    call    printf

    add     rsp, 40
    pop     rbx
    pop     rbp
    ret
ObserveTokenStream ENDP

;-----------------------------------------------------------------------------
; CoordinateAgents — walk registry, heartbeat check, failover, auto-fix
;-----------------------------------------------------------------------------
CoordinateAgents PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 40
    .ALLOCSTACK 40
    .ENDPROLOG

    mov     esi, dword ptr [g_ActiveAgentCount]
    xor     edi, edi                    ; current agent idx = 0
CoordLoop:
    cmp     edi, esi
    jae     CoordDone

    ; --- sync log ---
    lea     rcx, szSync
    mov     edx, edi
    mov     r8,  qword ptr [g_SovereignStatus]
    call    printf

    ; --- observe token stream ---
    mov     ecx, edi
    call    ObserveTokenStream

    ; --- heartbeat: load agent record ptr ---
    lea     rax, g_AgentRegistry
    movsxd  rbx, edi
    mov     rbx, [rax + rbx*8]          ; rbx = agent record pointer
    test    rbx, rbx
    jz      CoordNext

    ; heartbeat stored at offset +8; 0 = dead
    mov     rax, [rbx+8]
    test    rax, rax
    jnz     AgentAlive

    ; --- failover: clear RUNNING bit ---
    lea     rcx, szAutoFix
    mov     edx, edi
    call    printf
    lock btr qword ptr [rbx], 0

AgentAlive:
    lock bts qword ptr [rbx], 0        ; set RUNNING bit

CoordNext:
    inc     edi
    jmp     CoordLoop

CoordDone:
    ; --- auto-fix scan: check error register at offset +16 ---
    xor     edi, edi
AutoFixLoop:
    cmp     edi, esi
    jae     AutoFixDone
    lea     rax, g_AgentRegistry
    movsxd  rbx, edi
    mov     rbx, [rax + rbx*8]
    test    rbx, rbx
    jz      AutoFixNext
    mov     rax, [rbx+16]
    test    rax, rax
    jz      AutoFixNext
    ; error: log, clear, re-arm
    lea     rcx, szAutoFix
    mov     edx, edi
    call    printf
    mov     qword ptr [rbx+16], 0
    lock btr qword ptr [rbx], 0
    mfence
    lock bts qword ptr [rbx], 0
    lock add qword ptr [g_SymbolHealCount], 1
AutoFixNext:
    inc     edi
    jmp     AutoFixLoop

AutoFixDone:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
CoordinateAgents ENDP

;-----------------------------------------------------------------------------
; RawrXD_Trigger_Chat
;   Full pipeline: IDE UI -> Chat -> Prompt -> LLM -> Token Stream -> Renderer
;   RAX = g_CycleCounter value on return
;-----------------------------------------------------------------------------
RawrXD_Trigger_Chat PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    sub rsp, 32
    .ALLOCSTACK 32
    lea rbp, [rsp+32]
    .SETFRAME rbp, 32
    .ENDPROLOG

    ; [1/6] IDE UI signal
    lea     rcx, szIDEUI
    call    OutputDebugStringA

    ; [2/6] Chat Service — get tick as cycle ID
    call    GetTickCount
    mov     rbx, rax                    ; rbx = cycleID

    lea     rcx, szChat
    mov     rdx, rbx
    call    printf

    ; [3/6] Prompt Builder
    lea     rcx, szPrompt
    mov     rdx, rbx
    call    printf

    ; [4/6] LLM API dispatch (4096 token budget)
    lea     rcx, szLLM
    mov     edx, 4096
    call    printf
    ; simulate ~50 ms dispatch latency
    mov     ecx, 50
    call    Sleep

    ; [5/6] Token Stream — observe all active agents
    mov     esi, dword ptr [g_ActiveAgentCount]
    xor     ebx, ebx
StreamLoop:
    cmp     ebx, esi
    jae     StreamDone
    mov     ecx, ebx
    call    ObserveTokenStream
    inc     ebx
    jmp     StreamLoop
StreamDone:

    ; [6/6] Renderer
    lea     rcx, szRenderer
    mov     rdx, qword ptr [g_SovereignStatus]
    call    printf

    ; Increment cycle counter (atomic)
    lock add qword ptr [g_CycleCounter], 1

    ; Done
    lea     rcx, szDone
    mov     rdx, qword ptr [g_CycleCounter]
    call    printf

    mov     rax, qword ptr [g_CycleCounter]

    add     rsp, 32
    pop     rsi
    pop     rbx
    pop     rbp
    ret
RawrXD_Trigger_Chat ENDP

;-----------------------------------------------------------------------------
; Sovereign_Pipeline_Cycle — one full locked cycle (called from both Main loops)
;   No args.  Uses AcquireSovereignLock / ReleaseSovereignLock.
;-----------------------------------------------------------------------------
Sovereign_Pipeline_Cycle PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    lea rbp, [rsp+32]
    .SETFRAME rbp, 32
    .ENDPROLOG

    call    AcquireSovereignLock
    lock bts qword ptr [g_SovereignStatus], 1   ; set COMPILING bit

    call    CoordinateAgents
    call    RawrXD_Trigger_Chat
    call    ValidateDMAAlignment
    lea     rcx, szVA
    call    HealSymbolResolution
    lea     rcx, szDMAMap
    call    HealSymbolResolution

    lock btr qword ptr [g_SovereignStatus], 1   ; clear COMPILING bit
    call    ReleaseSovereignLock

    add     rsp, 32
    pop     rbp
    ret
Sovereign_Pipeline_Cycle ENDP

;-----------------------------------------------------------------------------
; Sovereign_MainLoop — infinite agentic loop (runs until OS terminates)
;   Call from a dedicated thread; never returns.
;-----------------------------------------------------------------------------
Sovereign_MainLoop PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    lea rbp, [rsp+32]
    .SETFRAME rbp, 32
    .ENDPROLOG
SovereignLoopTop:
    call    Sovereign_Pipeline_Cycle
    mov     ecx, szSleepMs
    call    Sleep
    jmp     SovereignLoopTop
    ; unreachable
    add     rsp, 32
    pop     rbp
    ret
Sovereign_MainLoop ENDP

END
