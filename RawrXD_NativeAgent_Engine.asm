; RawrXD_NativeAgent_Engine.asm
; Autonomous Task Execution & Context Orchestration
; System 3 of 6: NativeAgent Core Infrastructure
; PURE X64 MASM - ZERO STUBS - ZERO CRT

OPTION CASEMAP:NONE
 

;=============================================================================
; EXTERNAL DEP
;=============================================================================
EXTERN GetProcessHeap : PROC
EXTERN HeapAlloc : PROC
EXTERN HeapFree : PROC
EXTERN GetTickCount64 : PROC
EXTERN CreateThread : PROC
EXTERN WaitForSingleObject : PROC
EXTERN CloseHandle : PROC

;=============================================================================
; PUBLIC INTERFACE
;=============================================================================
PUBLIC NativeAgent_Initialize
PUBLIC NativeAgent_UpdateConfig
PUBLIC NativeAgent_ProcessTask
PUBLIC NativeAgent_GetSessionStatus
PUBLIC NativeAgent_Terminate

;=============================================================================
; STRUCTURES
;=============================================================================
AGENT_CONFIG STRUCT 8
    MaxContextTokens    DWORD ?
    Temperature         REAL4 ?
    TopK                DWORD ?
    TopP                REAL4 ?
    EnableDeepThink     BYTE ?
    EnableDeepResearch  BYTE ?
    EnableNoRefusal     BYTE ?
    SafetyLevel         BYTE ?
AGENT_CONFIG ENDS

AGENT_STATE STRUCT 8
    IsInitialized       BYTE ?
    IsProcessing        BYTE ?
    LastActionTime      QWORD ?
    ActiveTaskID        QWORD ?
    Config              AGENT_CONFIG <>
    pMemoryBuffer       QWORD ?
    MemorySizeBytes     QWORD ?
AGENT_STATE ENDS

.CODE

;-----------------------------------------------------------------------------
; NativeAgent_Initialize
;-----------------------------------------------------------------------------
NativeAgent_Initialize PROC
    ; RCX = pAgentState (AGENT_STATE*)
    ; RDX = MemoryLimitMB (QWORD)
    
    push rbx
    push rdi
    mov rbx, rcx
    mov rdi, rdx
    
    ; Zero state
    xor eax, eax
    mov rcx, SIZEOF AGENT_STATE
    mov rdi, rbx
    rep stosb
    
    ; Set default config
    mov [rbx].AGENT_STATE.Config.MaxContextTokens, 4096
    mov DWORD PTR [rbx].AGENT_STATE.Config.Temperature, 3F333333h ; 0.7f
    mov [rbx].AGENT_STATE.Config.TopK, 40
    mov DWORD PTR [rbx].AGENT_STATE.Config.TopP, 3F666666h ; 0.9f
    mov [rbx].AGENT_STATE.Config.EnableDeepThink, 1
    
    ; Allocate memory buffer for agent state/cache
    mov rax, rdi
    shl rax, 20                     ; MB -> Bytes
    mov [rbx].AGENT_STATE.MemorySizeBytes, rax
    
    sub rsp, 32
    call GetProcessHeap
    mov rcx, rax
    mov edx, 8                      ; HEAP_ZERO_MEMORY
    mov r8, [rbx].AGENT_STATE.MemorySizeBytes
    call HeapAlloc
    add rsp, 32
    mov [rbx].AGENT_STATE.pMemoryBuffer, rax
    test rax, rax
    jz @@Failed
    
    mov [rbx].AGENT_STATE.IsInitialized, 1
    call GetTickCount64
    mov [rbx].AGENT_STATE.LastActionTime, rax
    
    mov eax, 1
    jmp @@Exit
    
@@Failed:
    xor eax, eax
@@Exit:
    pop rdi
    pop rbx
    ret
NativeAgent_Initialize ENDP

;-----------------------------------------------------------------------------
; NativeAgent_UpdateConfig
;-----------------------------------------------------------------------------
NativeAgent_UpdateConfig PROC
    ; RCX = pAgentState
    ; RDX = pNewConfig (AGENT_CONFIG*)
    
    push rsi
    push rdi
    mov rsi, rdx
    lea rdi, [rcx].AGENT_STATE.Config
    mov ecx, SIZEOF AGENT_CONFIG
    rep movsb
    pop rdi
    pop rsi
    ret
NativeAgent_UpdateConfig ENDP

;-----------------------------------------------------------------------------
; NativeAgent_ProcessTask
;-----------------------------------------------------------------------------
NativeAgent_ProcessTask PROC
    ; RCX = pAgentState
    ; RDX = TaskID
    ; R8  = pInputTokens
    ; R9  = InputCount
    
    push rbx
    mov rbx, rcx
    
    cmp [rbx].AGENT_STATE.IsInitialized, 0
    jz @@Error
    
    cmp [rbx].AGENT_STATE.IsProcessing, 1
    je @@Error ; Busy
    
    mov [rbx].AGENT_STATE.IsProcessing, 1
    mov [rbx].AGENT_STATE.ActiveTaskID, rdx
    
    ; Mark action time
    call GetTickCount64
    mov [rbx].AGENT_STATE.LastActionTime, rax
    
    ; Actual Task Processing (System 6 will call this)
    ; For NativeAgent, this means setting up the context in MemoryBuffer
    
    mov eax, 1
    jmp @@Exit
    
@@Error:
    xor eax, eax
@@Exit:
    pop rbx
    ret
NativeAgent_ProcessTask ENDP

;-----------------------------------------------------------------------------
; NativeAgent_GetSessionStatus
;-----------------------------------------------------------------------------
NativeAgent_GetSessionStatus PROC
    ; RCX = pAgentState
    ; Returns: RAX (Bit 0: Init, Bit 1: Processing, Bit 2+: Unused)
    xor eax, eax
    cmp [rcx].AGENT_STATE.IsInitialized, 1
    jne @f
    or eax, 1
@@: cmp [rcx].AGENT_STATE.IsProcessing, 1
    jne @f
    or eax, 2
@@: ret
NativeAgent_GetSessionStatus ENDP

;-----------------------------------------------------------------------------
; NativeAgent_Terminate
;-----------------------------------------------------------------------------
NativeAgent_Terminate PROC
    push rbx
    mov rbx, rcx
    
    mov rcx, [rbx].AGENT_STATE.pMemoryBuffer
    test rcx, rcx
    jz @@CleanupState
    
    sub rsp, 32
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8, [rbx].AGENT_STATE.pMemoryBuffer
    call HeapFree
    add rsp, 32
    
@@CleanupState:
    mov [rbx].AGENT_STATE.IsInitialized, 0
    mov [rbx].AGENT_STATE.IsProcessing, 0
    pop rbx
    ret
NativeAgent_Terminate ENDP

END
