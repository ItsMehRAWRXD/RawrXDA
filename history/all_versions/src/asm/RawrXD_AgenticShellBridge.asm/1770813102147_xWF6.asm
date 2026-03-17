; =============================================================================
; RawrXD_AgenticShellBridge.asm — AI-Native Terminal Intelligence Layer
; Production bridge between ShellIntegration and Local LLM Inference
; =============================================================================
;
; Architecture:
;   - Consumes ShellInteg output ring buffer (lock-free consumer)
;   - Maintains rolling context window (last 4KB of terminal I/O)
;   - Async Ollama inference trigger on command failure (exit != 0)
;   - Command suggestion generator (natural language -> shell command)
;   - Auto-fix pipeline: failed command -> AI explanation -> suggested fix
;
; Protocol:
;   Context Ring: [Command][Output][ExitCode] x 32 slots (circular)
;   AI Triggers:
;     - ExitCode != 0  ->  Explain error + suggest fix
;     - "?" prefix     ->  Natural language query mode
;     - "@fix"         ->  Explicit fix request for last failure
;     - Idle > 2s      ->  Proactive suggestion based on CWD/git status
;
; Exports:
;   AgenticShell_Init           — Bind to ShellInteg instance, spawn AI worker
;   AgenticShell_Shutdown       — Drain context, terminate AI thread
;   AgenticShell_OnCommandEnd   — Called by ShellInteg post-execution (async AI)
;   AgenticShell_GetSuggestion  — Retrieve AI-generated command suggestion
;   AgenticShell_AcceptFix      — Apply AI-suggested fix to ShellInteg
;   AgenticShell_SetMode        — Toggle: 0=passive, 1=active, 2=aggressive
;   AgenticShell_GetStats       — Return AI subsystem performance counters
;   AgenticShell_SetModel       — Configure LLM model name for inference
;
; Pattern: PatchResult (RAX=0 success, RAX=NTSTATUS-style error on failure)
;          RDX = detail pointer or supplemental data
; Rule:    NO CRT, NO Qt, NO std::, NO exceptions, NO HEAP ALLOCATIONS
;          Lock-free context ring (SPSC: Shell produces, AI consumes)
;          NO SOURCE FILE IS TO BE SIMPLIFIED
; Build:   ml64.exe /c /Zi /Zd /Fo RawrXD_AgenticShellBridge.obj RawrXD_AgenticShellBridge.asm
; Link:    link RawrXD_ShellIntegration.obj RawrXD_AgenticShellBridge.obj kernel32.lib wininet.lib
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
; External Imports (from ShellIntegration module)
; =============================================================================
EXTERNDEF ShellInteg_GetCommandHistory:PROC
EXTERNDEF ShellInteg_GetStats:PROC
EXTERNDEF ShellInteg_IsAlive:PROC
EXTERNDEF ShellInteg_ExecuteCommand:PROC
EXTERNDEF ShellInteg_CompleteCommand:PROC

; =============================================================================
; Win32 API Imports (not in Common.inc)
; =============================================================================
EXTERNDEF CreateThread:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF CreateEventA:PROC
EXTERNDEF SetEvent:PROC
EXTERNDEF ResetEvent:PROC
EXTERNDEF InternetOpenA:PROC
EXTERNDEF InternetConnectA:PROC
EXTERNDEF HttpOpenRequestA:PROC
EXTERNDEF HttpSendRequestA:PROC
EXTERNDEF InternetReadFile:PROC
EXTERNDEF InternetCloseHandle:PROC
EXTERNDEF lstrlenA:PROC

; =============================================================================
; Constants
; =============================================================================

; Context Ring Configuration
CTX_RING_SLOTS          EQU 32
CTX_SLOT_SIZE           EQU 4096          ; 4KB per context entry
CTX_MASK                EQU (CTX_RING_SLOTS - 1)
CTX_CMD_MAX             EQU 512           ; Max command length within slot
CTX_OUT_MAX             EQU 3520          ; Remaining for output (4096-512-64 header)

; AI Trigger Modes
AI_MODE_PASSIVE         EQU 0             ; Only on explicit request
AI_MODE_ACTIVE          EQU 1             ; On non-zero exit codes
AI_MODE_AGGRESSIVE      EQU 2             ; Proactive suggestions

; Context Slot Flags
CTX_FLAG_NORMAL         EQU 0             ; No AI action
CTX_FLAG_FAILED         EQU 1             ; Command failed (exit != 0)
CTX_FLAG_AI_PENDING     EQU 2             ; Queued for AI processing
CTX_FLAG_AI_COMPLETE    EQU 3             ; AI has produced a suggestion
CTX_FLAG_AI_ERROR       EQU 4             ; AI inference failed

; HTTP/Ollama Configuration
INTERNET_OPEN_TYPE_DIRECT   EQU 1
INTERNET_SERVICE_HTTP       EQU 3
INTERNET_FLAG_RELOAD        EQU 80000000h
OLLAMA_PORT             EQU 11434
HTTP_TIMEOUT_MS         EQU 5000

; Buffer sizes
MAX_JSON_PAYLOAD        EQU 8192
MAX_SUGGESTION_LEN      EQU 512
MAX_MODEL_NAME_LEN      EQU 64
MAX_PROMPT_CONTEXT      EQU 2048          ; Max context sent to LLM

; WaitForSingleObject
WAIT_OBJECT_0           EQU 0
WAIT_TIMEOUT_VAL        EQU 00000102h
INFINITE_WAIT           EQU 0FFFFFFFFh

; Status Codes
AGENTSHELL_SUCCESS              EQU 0
AGENTSHELL_ERR_NOT_INIT         EQU 0C0000200h
AGENTSHELL_ERR_ALREADY_INIT     EQU 0C0000201h
AGENTSHELL_ERR_THREAD_FAIL      EQU 0C0000202h
AGENTSHELL_ERR_NET_FAIL         EQU 0C0000203h
AGENTSHELL_ERR_CONTEXT_FULL     EQU 0C0000204h
AGENTSHELL_ERR_INVALID_MODE     EQU 0C0000205h
AGENTSHELL_ERR_NULL_PARAM       EQU 0C0000206h
AGENTSHELL_ERR_NO_SUGGESTION    EQU 0C0000207h

; Context Slot Offsets (within 4096-byte slot)
CTX_OFF_CMD             EQU 0             ; [0-511]:   Command text (null-padded)
CTX_OFF_EXITCODE        EQU 512           ; [512-515]: Exit code (DWORD)
CTX_OFF_FLAGS           EQU 516           ; [516-519]: Flags (DWORD)
CTX_OFF_TIMESTAMP       EQU 520           ; [520-527]: Timestamp (QWORD)
CTX_OFF_OUTLEN          EQU 528           ; [528-531]: Output length (DWORD)
CTX_OFF_DURATION        EQU 532           ; [532-535]: Duration ms (DWORD)
CTX_OFF_PADDING         EQU 536           ; [536-575]: Reserved
CTX_OFF_OUTPUT          EQU 576           ; [576-4095]: Output text

; =============================================================================
;              Cache-Line Padded Counters Segment
; =============================================================================

AGENTSHELL_CL SEGMENT ALIGN(16) 'DATA'

    PUBLIC g_AS_MetricStart
    g_AS_MetricStart        dq 04147454E544149h     ; "AGENTAI" sentinel
                            db 56 dup(0)

    PUBLIC g_AS_Counter_CtxCaptured
    g_AS_Counter_CtxCaptured    dq 0
                                db 56 dup(0)

    PUBLIC g_AS_Counter_AIRequests
    g_AS_Counter_AIRequests     dq 0
                                db 56 dup(0)

    PUBLIC g_AS_Counter_AISuccess
    g_AS_Counter_AISuccess      dq 0
                                db 56 dup(0)

    PUBLIC g_AS_Counter_AIFailures
    g_AS_Counter_AIFailures     dq 0
                                db 56 dup(0)

    PUBLIC g_AS_Counter_BytesSentToAI
    g_AS_Counter_BytesSentToAI  dq 0
                                db 56 dup(0)

    PUBLIC g_AS_Counter_BytesFromAI
    g_AS_Counter_BytesFromAI    dq 0
                                db 56 dup(0)

    PUBLIC g_AS_MetricEnd
    g_AS_MetricEnd          dq 0454E44414745h       ; "ENDAGE" sentinel
                            db 56 dup(0)

AGENTSHELL_CL ENDS

; =============================================================================
;                          Data Segment
; =============================================================================
.data

    ; =========================================================================
    ; Initialization state
    ; =========================================================================
    ALIGN 8
    g_AS_Initialized        dq  0
    g_AS_ShutdownRequested  dq  0       ; Signal AI thread to exit
    g_AS_Mode               dd  AI_MODE_ACTIVE
    g_AS_ModePad            dd  0

    ; =========================================================================
    ; Thread handles
    ; =========================================================================
    ALIGN 8
    g_AS_hAIThread          dq  0
    g_AS_hEvent             dq  0       ; Manual-reset event for new data

    ; =========================================================================
    ; Context Ring Buffer (SPSC: Shell produces at head, AI consumes at tail)
    ; =========================================================================
    ALIGN 4096
    g_ContextRing           db (CTX_RING_SLOTS * CTX_SLOT_SIZE) dup(0)

    ALIGN 8
    g_ContextHead           dq  0       ; Write index (atomic xadd by producer)
    g_ContextTail           dq  0       ; Read index (AI consumer only)

    ; =========================================================================
    ; AI suggestion output (double-buffered: write slot + read slot)
    ; =========================================================================
    ALIGN 8
    g_AS_SuggestionReady    dq  0       ; 1 = suggestion available for read
    g_AS_SuggestionBufA     db MAX_SUGGESTION_LEN dup(0)
    g_AS_SuggestionBufB     db MAX_SUGGESTION_LEN dup(0)
    g_AS_ActiveBuf          dq  0       ; 0 = A is readable, 1 = B is readable

    ; =========================================================================
    ; JSON payload scratch (only used by AI thread — no concurrency concern)
    ; =========================================================================
    ALIGN 8
    g_AS_JsonPayload        db MAX_JSON_PAYLOAD dup(0)

    ; =========================================================================
    ; HTTP response scratch (only used by AI thread)
    ; =========================================================================
    ALIGN 8
    g_AS_HttpResponse       db MAX_JSON_PAYLOAD dup(0)

    ; =========================================================================
    ; Model configuration
    ; =========================================================================
    ALIGN 8
    g_AS_ModelName          db "phi3", 0
                            db (MAX_MODEL_NAME_LEN - 5) dup(0)

    ; =========================================================================
    ; WinInet handles (AI thread only)
    ; =========================================================================
    ALIGN 8
    g_AS_hInternet          dq  0
    g_AS_hConnect           dq  0

    ; =========================================================================
    ; String Constants
    ; =========================================================================

    ; Ollama endpoint
    szAS_OllamaHost         db "127.0.0.1", 0
    szAS_UserAgent          db "RawrXD-AgenticShell/1.0", 0
    szAS_ApiPath            db "/api/generate", 0
    szAS_HttpVerb           db "POST", 0
    szAS_ContentType        db "Content-Type: application/json", 0Dh, 0Ah, 0

    ; JSON template fragments (built manually — no CRT sprintf)
    szAS_JsonModelOpen      db '{"model":"', 0
    szAS_JsonPromptOpen     db '","prompt":"', 0
    szAS_JsonStreamFalse    db '","stream":false}', 0

    ; Prompt prefixes
    szAS_PromptExplain      db "Explain this shell error concisely and show the corrected command. "
                            db "Command: ", 0
    szAS_PromptExplainOut   db " Output: ", 0
    szAS_PromptExplainExit  db " ExitCode: ", 0
    szAS_PromptSuggest      db "Suggest a shell command for: ", 0
    szAS_PromptFix          db "Fix this command: ", 0

    ; Log/status strings
    szAS_InitOk             db "[AgenticShell] AI subsystem initialized", 0Dh, 0Ah, 0
    szAS_ShutdownOk         db "[AgenticShell] AI subsystem shutdown", 0Dh, 0Ah, 0
    szAS_AIQuerySent        db "[AgenticShell] AI query dispatched", 0Dh, 0Ah, 0
    szAS_AIQueryFail        db "[AgenticShell] AI query failed", 0Dh, 0Ah, 0
    szAS_CtxCaptured        db "[AgenticShell] Context captured for AI", 0Dh, 0Ah, 0

    ; Scratch for integer-to-ascii
    g_AS_ItoBuf             db 32 dup(0)

; =============================================================================
;                           Code Segment
; =============================================================================
.code

; =============================================================================
; AgenticShell_Init — Initialize AI shell bridge
;
; Spawns background worker thread, prepares context ring, opens WinInet.
;
; RCX = Mode (AI_MODE_PASSIVE/ACTIVE/AGGRESSIVE)
; RDX = Model name string (NULL = default "phi3")
; Returns: RAX = 0 on success, AGENTSHELL_ERR_* on failure
; =============================================================================
PUBLIC AgenticShell_Init
AgenticShell_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 192
    .allocstack 192
    .endprolog

    ; Guard: already initialized?
    cmp     qword ptr [g_AS_Initialized], 1
    jne     @as_init_go
    mov     eax, AGENTSHELL_ERR_ALREADY_INIT
    jmp     @as_init_exit
@as_init_go:

    ; Validate mode
    cmp     ecx, AI_MODE_AGGRESSIVE
    ja      @as_init_bad_mode
    mov     dword ptr [g_AS_Mode], ecx

    ; Clear shutdown flag
    mov     qword ptr [g_AS_ShutdownRequested], 0

    ; Set model if provided
    test    rdx, rdx
    jz      @as_init_default_model
    mov     rsi, rdx
    lea     rdi, g_AS_ModelName
    mov     ecx, MAX_MODEL_NAME_LEN - 1
@as_copy_model:
    lodsb
    stosb
    test    al, al
    jz      @as_model_done
    loop    @as_copy_model
    mov     byte ptr [rdi], 0       ; Force null terminate
@as_model_done:
@as_init_default_model:

    ; Zero context ring head/tail
    mov     qword ptr [g_ContextHead], 0
    mov     qword ptr [g_ContextTail], 0
    mov     qword ptr [g_AS_SuggestionReady], 0

    ; Zero performance counters
    mov     qword ptr [g_AS_Counter_CtxCaptured], 0
    mov     qword ptr [g_AS_Counter_AIRequests], 0
    mov     qword ptr [g_AS_Counter_AISuccess], 0
    mov     qword ptr [g_AS_Counter_AIFailures], 0
    mov     qword ptr [g_AS_Counter_BytesSentToAI], 0
    mov     qword ptr [g_AS_Counter_BytesFromAI], 0

    ; ---- Create manual-reset event for signaling AI thread ----
    xor     ecx, ecx                ; lpEventAttributes = NULL
    mov     edx, 1                  ; bManualReset = TRUE
    xor     r8d, r8d                ; bInitialState = FALSE
    xor     r9d, r9d                ; lpName = NULL
    sub     rsp, 32
    call    CreateEventA
    add     rsp, 32
    test    rax, rax
    jz      @as_init_fail
    mov     qword ptr [g_AS_hEvent], rax

    ; ---- Open WinInet session (persistent for thread lifetime) ----
    lea     rcx, szAS_UserAgent
    mov     edx, INTERNET_OPEN_TYPE_DIRECT
    xor     r8, r8                  ; lpszProxy
    xor     r9, r9                  ; lpszProxyBypass
    sub     rsp, 40
    mov     qword ptr [rsp + 32], 0 ; dwFlags
    call    InternetOpenA
    add     rsp, 40
    test    rax, rax
    jz      @as_init_fail_event
    mov     qword ptr [g_AS_hInternet], rax

    ; ---- InternetConnect to Ollama ----
    mov     rcx, rax                ; hInternet
    lea     rdx, szAS_OllamaHost
    mov     r8d, OLLAMA_PORT
    xor     r9, r9                  ; lpszUserName
    sub     rsp, 48
    mov     qword ptr [rsp + 32], 0 ; lpszPassword
    mov     qword ptr [rsp + 40], INTERNET_SERVICE_HTTP
    call    InternetConnectA
    add     rsp, 48
    test    rax, rax
    jz      @as_init_fail_inet
    mov     qword ptr [g_AS_hConnect], rax

    ; ---- Spawn AI worker thread ----
    xor     ecx, ecx                ; lpThreadAttributes
    xor     edx, edx                ; dwStackSize (default 1MB)
    lea     r8, as_AIWorkerThread   ; lpStartAddress
    xor     r9, r9                  ; lpParameter
    sub     rsp, 48
    mov     qword ptr [rsp + 32], 0 ; dwCreationFlags
    mov     qword ptr [rsp + 40], 0 ; lpThreadId
    call    CreateThread
    add     rsp, 48
    test    rax, rax
    jz      @as_init_fail_connect
    mov     qword ptr [g_AS_hAIThread], rax

    ; ---- Mark initialized ----
    mov     qword ptr [g_AS_Initialized], 1

    ; Debug log
    lea     rcx, szAS_InitOk
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

    xor     eax, eax
    jmp     @as_init_exit

@as_init_bad_mode:
    mov     eax, AGENTSHELL_ERR_INVALID_MODE
    jmp     @as_init_exit

@as_init_fail_connect:
    mov     rcx, qword ptr [g_AS_hConnect]
    sub     rsp, 32
    call    InternetCloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hConnect], 0

@as_init_fail_inet:
    mov     rcx, qword ptr [g_AS_hInternet]
    sub     rsp, 32
    call    InternetCloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hInternet], 0

@as_init_fail_event:
    mov     rcx, qword ptr [g_AS_hEvent]
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hEvent], 0

@as_init_fail:
    mov     eax, AGENTSHELL_ERR_THREAD_FAIL

@as_init_exit:
    lea     rsp, [rbp]
    pop     rbp
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AgenticShell_Init ENDP

; =============================================================================
; AgenticShell_Shutdown — Tear down AI shell bridge
;
; Signals worker thread to exit, waits 3 seconds, closes all handles.
;
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC AgenticShell_Shutdown
AgenticShell_Shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 64
    .allocstack 64
    .endprolog

    cmp     qword ptr [g_AS_Initialized], 0
    je      @as_shut_not_init

    ; Signal shutdown
    mov     qword ptr [g_AS_ShutdownRequested], 1

    ; Wake the AI thread
    mov     rcx, qword ptr [g_AS_hEvent]
    test    rcx, rcx
    jz      @as_shut_no_event
    sub     rsp, 32
    call    SetEvent
    add     rsp, 32
@as_shut_no_event:

    ; Wait for AI thread to exit (3 seconds)
    mov     rcx, qword ptr [g_AS_hAIThread]
    test    rcx, rcx
    jz      @as_shut_close_inet
    mov     edx, 3000
    sub     rsp, 32
    call    WaitForSingleObject
    add     rsp, 32

    ; Close thread handle
    mov     rcx, qword ptr [g_AS_hAIThread]
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hAIThread], 0

@as_shut_close_inet:
    ; Close WinInet handles
    mov     rcx, qword ptr [g_AS_hConnect]
    test    rcx, rcx
    jz      @as_shut_close_inet2
    sub     rsp, 32
    call    InternetCloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hConnect], 0

@as_shut_close_inet2:
    mov     rcx, qword ptr [g_AS_hInternet]
    test    rcx, rcx
    jz      @as_shut_close_event
    sub     rsp, 32
    call    InternetCloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hInternet], 0

@as_shut_close_event:
    mov     rcx, qword ptr [g_AS_hEvent]
    test    rcx, rcx
    jz      @as_shut_done
    sub     rsp, 32
    call    CloseHandle
    add     rsp, 32
    mov     qword ptr [g_AS_hEvent], 0

@as_shut_done:
    mov     qword ptr [g_AS_Initialized], 0

    lea     rcx, szAS_ShutdownOk
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

    xor     eax, eax
    jmp     @as_shut_exit

@as_shut_not_init:
    mov     eax, AGENTSHELL_ERR_NOT_INIT

@as_shut_exit:
    add     rsp, 64
    pop     rbx
    ret
AgenticShell_Shutdown ENDP

; =============================================================================
; AgenticShell_OnCommandEnd — Called when a command completes
;
; Captures command + output + exit code into the context ring.
; If mode >= ACTIVE and exit code != 0, signals AI thread.
;
; RCX = Exit code (DWORD, zero-extended)
; RDX = Pointer to output buffer
; R8  = Output length (bytes)
; R9  = Pointer to command string (null-terminated)
; Returns: RAX = 0
; =============================================================================
PUBLIC AgenticShell_OnCommandEnd
AgenticShell_OnCommandEnd PROC FRAME
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
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 128
    .allocstack 128
    .endprolog

    ; Guard: initialized?
    cmp     qword ptr [g_AS_Initialized], 0
    je      @as_oce_exit_ok

    mov     r12d, ecx               ; r12d = exit code
    mov     r13, rdx                ; r13 = output ptr
    mov     r14, r8                 ; r14 = output len
    mov     r15, r9                 ; r15 = command ptr

    ; ---- Acquire context slot (atomic increment) ----
    mov     rax, 1
    lock xadd qword ptr [g_ContextHead], rax
    ; rax = old head value = our reserved slot index
    and     eax, CTX_MASK
    mov     ebx, eax                ; ebx = slot index

    ; Calculate slot address
    mov     ecx, CTX_SLOT_SIZE
    imul    rax, rcx                ; rax = slot offset
    lea     rdi, g_ContextRing
    add     rdi, rax                ; rdi = slot base pointer
    mov     rbx, rdi                ; rbx = save slot base

    ; ---- Zero the entire slot ----
    push    rdi
    xor     eax, eax
    mov     ecx, CTX_SLOT_SIZE
    rep stosb
    pop     rdi

    ; ---- Fill command text ----
    test    r15, r15
    jz      @as_oce_no_cmd
    push    rdi
    mov     rsi, r15
    mov     ecx, CTX_CMD_MAX - 1
@as_oce_copy_cmd:
    lodsb
    test    al, al
    jz      @as_oce_cmd_done
    stosb
    loop    @as_oce_copy_cmd
@as_oce_cmd_done:
    mov     byte ptr [rdi], 0       ; Null-terminate
    pop     rdi
@as_oce_no_cmd:

    ; ---- Fill exit code ----
    mov     dword ptr [rbx + CTX_OFF_EXITCODE], r12d

    ; ---- Fill timestamp ----
    sub     rsp, 32
    call    GetTickCount64
    add     rsp, 32
    mov     qword ptr [rbx + CTX_OFF_TIMESTAMP], rax

    ; ---- Fill output (truncated to CTX_OUT_MAX) ----
    test    r13, r13
    jz      @as_oce_no_output
    mov     rsi, r13
    lea     rdi, [rbx + CTX_OFF_OUTPUT]
    mov     rcx, r14
    cmp     rcx, CTX_OUT_MAX - 1
    jbe     @as_oce_out_fits
    mov     rcx, CTX_OUT_MAX - 1
@as_oce_out_fits:
    mov     dword ptr [rbx + CTX_OFF_OUTLEN], ecx
    rep movsb
    mov     byte ptr [rdi], 0       ; Null-terminate
@as_oce_no_output:

    ; ---- Set flags based on exit code and mode ----
    cmp     dword ptr [g_AS_Mode], AI_MODE_PASSIVE
    je      @as_oce_passive

    test    r12d, r12d              ; Exit code == 0?
    jz      @as_oce_passive

    ; Non-zero exit + ACTIVE or AGGRESSIVE mode: mark for AI
    mov     dword ptr [rbx + CTX_OFF_FLAGS], CTX_FLAG_AI_PENDING
    lock inc qword ptr [g_AS_Counter_CtxCaptured]

    ; Debug log
    lea     rcx, szAS_CtxCaptured
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

    ; Signal AI thread
    mov     rcx, qword ptr [g_AS_hEvent]
    test    rcx, rcx
    jz      @as_oce_exit_ok
    sub     rsp, 32
    call    SetEvent
    add     rsp, 32
    jmp     @as_oce_exit_ok

@as_oce_passive:
    mov     dword ptr [rbx + CTX_OFF_FLAGS], CTX_FLAG_NORMAL

@as_oce_exit_ok:
    xor     eax, eax

    lea     rsp, [rbp]
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AgenticShell_OnCommandEnd ENDP

; =============================================================================
; AgenticShell_GetSuggestion — Retrieve AI-generated suggestion
;
; RCX = Output buffer pointer (at least MAX_SUGGESTION_LEN bytes)
; RDX = Buffer capacity
; Returns: RAX = string length (0 if no suggestion), or error code
; =============================================================================
PUBLIC AgenticShell_GetSuggestion
AgenticShell_GetSuggestion PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    cmp     qword ptr [g_AS_Initialized], 0
    je      @as_gs_not_init

    test    rcx, rcx
    jz      @as_gs_null
    test    rdx, rdx
    jz      @as_gs_null

    ; Check if a suggestion is ready (atomic check-and-clear)
    xor     eax, eax
    mov     r8, 1
    lock cmpxchg qword ptr [g_AS_SuggestionReady], r8
    ; If old value was 0, no suggestion available
    test    eax, eax
    jz      @as_gs_none

    ; Determine which buffer is readable
    mov     rax, qword ptr [g_AS_ActiveBuf]
    test    rax, rax
    jz      @as_gs_use_A
    lea     rsi, g_AS_SuggestionBufB
    jmp     @as_gs_copy
@as_gs_use_A:
    lea     rsi, g_AS_SuggestionBufA

@as_gs_copy:
    mov     rdi, rcx                ; Output buffer
    ; Find suggestion length (manual strlen)
    xor     eax, eax
    mov     r8, rsi
@as_gs_len:
    cmp     byte ptr [r8 + rax], 0
    je      @as_gs_len_done
    inc     eax
    cmp     eax, edx                ; Don't exceed capacity
    jb      @as_gs_len
@as_gs_len_done:
    mov     ecx, eax                ; ecx = bytes to copy
    push    rax                     ; save length
    rep movsb
    mov     byte ptr [rdi], 0       ; Null-terminate
    pop     rax

    ; Clear suggestion-ready flag (allow new suggestion)
    mov     qword ptr [g_AS_SuggestionReady], 0

    jmp     @as_gs_exit

@as_gs_none:
    xor     eax, eax                ; No suggestion
    jmp     @as_gs_exit

@as_gs_not_init:
    mov     eax, AGENTSHELL_ERR_NOT_INIT
    jmp     @as_gs_exit

@as_gs_null:
    mov     eax, AGENTSHELL_ERR_NULL_PARAM

@as_gs_exit:
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
AgenticShell_GetSuggestion ENDP

; =============================================================================
; AgenticShell_AcceptFix — Apply AI-suggested fix by submitting to shell
;
; Reads the current suggestion and passes it to ShellInteg_ExecuteCommand.
;
; Returns: RAX = 0 on success, error if no suggestion or not init
; =============================================================================
PUBLIC AgenticShell_AcceptFix
AgenticShell_AcceptFix PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     qword ptr [g_AS_Initialized], 0
    je      @as_af_not_init

    ; Check which buffer has the suggestion
    mov     rax, qword ptr [g_AS_ActiveBuf]
    test    rax, rax
    jz      @as_af_buf_A
    lea     rcx, g_AS_SuggestionBufB
    jmp     @as_af_check_empty
@as_af_buf_A:
    lea     rcx, g_AS_SuggestionBufA

@as_af_check_empty:
    cmp     byte ptr [rcx], 0
    je      @as_af_no_sugg

    ; Submit to ShellInteg
    xor     edx, edx                ; auto-detect length
    sub     rsp, 32
    call    ShellInteg_ExecuteCommand
    add     rsp, 32
    ; Return whatever ShellInteg returns
    jmp     @as_af_exit

@as_af_no_sugg:
    mov     eax, AGENTSHELL_ERR_NO_SUGGESTION
    jmp     @as_af_exit

@as_af_not_init:
    mov     eax, AGENTSHELL_ERR_NOT_INIT

@as_af_exit:
    add     rsp, 48
    pop     rbx
    ret
AgenticShell_AcceptFix ENDP

; =============================================================================
; AgenticShell_SetMode — Change AI trigger mode
;
; ECX = New mode (AI_MODE_PASSIVE/ACTIVE/AGGRESSIVE)
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC AgenticShell_SetMode
AgenticShell_SetMode PROC
    cmp     ecx, AI_MODE_AGGRESSIVE
    ja      @as_sm_fail
    mov     dword ptr [g_AS_Mode], ecx
    xor     eax, eax
    ret
@as_sm_fail:
    mov     eax, AGENTSHELL_ERR_INVALID_MODE
    ret
AgenticShell_SetMode ENDP

; =============================================================================
; AgenticShell_SetModel — Configure LLM model name
;
; RCX = Model name string pointer (null-terminated)
; Returns: RAX = 0 on success
; =============================================================================
PUBLIC AgenticShell_SetModel
AgenticShell_SetModel PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    test    rcx, rcx
    jz      @as_smod_null
    cmp     byte ptr [rcx], 0
    je      @as_smod_null

    mov     rsi, rcx
    lea     rdi, g_AS_ModelName
    mov     ecx, MAX_MODEL_NAME_LEN - 1
@as_smod_copy:
    lodsb
    stosb
    test    al, al
    jz      @as_smod_done
    loop    @as_smod_copy
    mov     byte ptr [rdi], 0       ; Force null
@as_smod_done:
    xor     eax, eax
    jmp     @as_smod_exit

@as_smod_null:
    mov     eax, AGENTSHELL_ERR_NULL_PARAM

@as_smod_exit:
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
AgenticShell_SetModel ENDP

; =============================================================================
; AgenticShell_GetStats — Return AI subsystem performance counters
;
; RCX = Output buffer (7 QWORDs = 56 bytes minimum)
;   [0] = CtxCaptured
;   [1] = AIRequests
;   [2] = AISuccess
;   [3] = AIFailures
;   [4] = BytesSentToAI
;   [5] = BytesFromAI
;   [6] = CurrentMode
; Returns: RAX = 0
; =============================================================================
PUBLIC AgenticShell_GetStats
AgenticShell_GetStats PROC
    test    rcx, rcx
    jz      @as_gst_null

    mov     rax, qword ptr [g_AS_Counter_CtxCaptured]
    mov     qword ptr [rcx], rax
    mov     rax, qword ptr [g_AS_Counter_AIRequests]
    mov     qword ptr [rcx + 8], rax
    mov     rax, qword ptr [g_AS_Counter_AISuccess]
    mov     qword ptr [rcx + 16], rax
    mov     rax, qword ptr [g_AS_Counter_AIFailures]
    mov     qword ptr [rcx + 24], rax
    mov     rax, qword ptr [g_AS_Counter_BytesSentToAI]
    mov     qword ptr [rcx + 32], rax
    mov     rax, qword ptr [g_AS_Counter_BytesFromAI]
    mov     qword ptr [rcx + 40], rax
    mov     eax, dword ptr [g_AS_Mode]
    mov     qword ptr [rcx + 48], rax

    xor     eax, eax
    ret

@as_gst_null:
    mov     eax, AGENTSHELL_ERR_NULL_PARAM
    ret
AgenticShell_GetStats ENDP

; =============================================================================
;                     Internal: AI Worker Thread
; =============================================================================

; -----------------------------------------------------------------------------
; as_AIWorkerThread — Background thread for Ollama inference
;
; Loop:
;   1. WaitForSingleObject on event (2s timeout for proactive checks)
;   2. Scan context ring for AI_PENDING slots
;   3. Build JSON prompt, POST to Ollama /api/generate
;   4. Parse response, store suggestion
;   5. Mark slot as AI_COMPLETE
;
; Terminates when g_AS_ShutdownRequested != 0
; -----------------------------------------------------------------------------
as_AIWorkerThread PROC FRAME
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
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 640
    .allocstack 640
    .endprolog

@ai_worker_loop:
    ; ---- Check shutdown ----
    cmp     qword ptr [g_AS_ShutdownRequested], 0
    jne     @ai_worker_exit

    ; ---- Wait for event or 2-second timeout ----
    mov     rcx, qword ptr [g_AS_hEvent]
    mov     edx, 2000               ; 2s timeout (for proactive mode)
    sub     rsp, 32
    call    WaitForSingleObject
    add     rsp, 32

    ; Check shutdown again after wake
    cmp     qword ptr [g_AS_ShutdownRequested], 0
    jne     @ai_worker_exit

    ; Reset event if it was signaled
    cmp     eax, WAIT_OBJECT_0
    jne     @ai_scan_slots
    mov     rcx, qword ptr [g_AS_hEvent]
    sub     rsp, 32
    call    ResetEvent
    add     rsp, 32

@ai_scan_slots:
    ; ---- Scan from tail to head for pending slots ----
    mov     rax, qword ptr [g_ContextTail]
    mov     rcx, qword ptr [g_ContextHead]
    cmp     rax, rcx
    jae     @ai_worker_loop         ; Tail caught up to head — no work

@ai_process_slot:
    ; Load slot address
    mov     rax, qword ptr [g_ContextTail]
    and     eax, CTX_MASK
    mov     ecx, CTX_SLOT_SIZE
    imul    rax, rcx
    lea     rsi, g_ContextRing
    add     rsi, rax                ; rsi = slot base

    ; Check if AI_PENDING
    cmp     dword ptr [rsi + CTX_OFF_FLAGS], CTX_FLAG_AI_PENDING
    jne     @ai_advance_tail

    ; ---- Build error explanation prompt ----
    ; Target: inactive suggestion buffer (write to opposite of g_AS_ActiveBuf)
    mov     rax, qword ptr [g_AS_ActiveBuf]
    test    rax, rax
    jz      @ai_write_B
    lea     r12, g_AS_SuggestionBufA
    jmp     @ai_have_write_buf
@ai_write_B:
    lea     r12, g_AS_SuggestionBufB
@ai_have_write_buf:

    ; Build JSON payload into g_AS_JsonPayload
    lea     rdi, g_AS_JsonPayload
    call    as_BuildPromptJson      ; rsi = slot, rdi = output JSON buf
    ; Returns: rdi = past-end of JSON, rax = JSON string length
    mov     r13, rax                ; r13 = JSON payload length

    ; Count AI request
    lock inc qword ptr [g_AS_Counter_AIRequests]

    ; ---- HTTP POST to Ollama ----
    lea     rcx, g_AS_JsonPayload   ; Payload pointer
    mov     rdx, r13                ; Payload length
    mov     r8, r12                 ; Output suggestion buffer
    mov     r9d, MAX_SUGGESTION_LEN ; Output capacity
    call    as_PostToOllama
    test    eax, eax
    jnz     @ai_query_failed

    ; ---- Success: flip active buffer, mark ready ----
    mov     rax, qword ptr [g_AS_ActiveBuf]
    xor     rax, 1                  ; Flip 0<->1
    mov     qword ptr [g_AS_ActiveBuf], rax
    mov     qword ptr [g_AS_SuggestionReady], 1

    ; Mark slot as AI_COMPLETE
    mov     dword ptr [rsi + CTX_OFF_FLAGS], CTX_FLAG_AI_COMPLETE
    lock inc qword ptr [g_AS_Counter_AISuccess]

    lea     rcx, szAS_AIQuerySent
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32
    jmp     @ai_advance_tail

@ai_query_failed:
    mov     dword ptr [rsi + CTX_OFF_FLAGS], CTX_FLAG_AI_ERROR
    lock inc qword ptr [g_AS_Counter_AIFailures]

    lea     rcx, szAS_AIQueryFail
    sub     rsp, 32
    call    OutputDebugStringA
    add     rsp, 32

@ai_advance_tail:
    inc     qword ptr [g_ContextTail]

    ; Check if more to process
    mov     rax, qword ptr [g_ContextTail]
    cmp     rax, qword ptr [g_ContextHead]
    jb      @ai_process_slot

    jmp     @ai_worker_loop

@ai_worker_exit:
    xor     eax, eax
    lea     rsp, [rbp]
    pop     rbp
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
as_AIWorkerThread ENDP

; =============================================================================
; as_BuildPromptJson — Construct JSON payload for Ollama /api/generate
;
; Input:
;   RSI = ContextSlot pointer (read-only)
;   RDI = Output buffer (g_AS_JsonPayload, MAX_JSON_PAYLOAD)
; Output:
;   RDI = past-end of JSON string
;   RAX = total JSON length
; Clobbers: RCX, RDX, R8, R9
;
; Produces: {"model":"<model>","prompt":"<context>","stream":false}
; =============================================================================
as_BuildPromptJson PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     r12, rdi                ; r12 = start of output

    ; ---- {"model":" ----
    lea     rbx, szAS_JsonModelOpen
    call    as_strcpy_advance       ; Copies string, advances rdi

    ; ---- model name ----
    lea     rbx, g_AS_ModelName
    call    as_strcpy_advance

    ; ---- ","prompt":" ----
    lea     rbx, szAS_JsonPromptOpen
    call    as_strcpy_advance

    ; ---- Prompt prefix: error explanation ----
    lea     rbx, szAS_PromptExplain
    call    as_json_escape_copy

    ; ---- Command text from slot [rsi + CTX_OFF_CMD] ----
    lea     rbx, [rsi + CTX_OFF_CMD]
    call    as_json_escape_copy

    ; ---- Output label ----
    lea     rbx, szAS_PromptExplainOut
    call    as_json_escape_copy

    ; ---- Output text from slot (truncated to MAX_PROMPT_CONTEXT) ----
    lea     rbx, [rsi + CTX_OFF_OUTPUT]
    mov     ecx, dword ptr [rsi + CTX_OFF_OUTLEN]
    cmp     ecx, MAX_PROMPT_CONTEXT
    jbe     @bp_out_ok
    mov     ecx, MAX_PROMPT_CONTEXT
@bp_out_ok:
    call    as_json_escape_copy_n

    ; ---- Exit code label ----
    lea     rbx, szAS_PromptExplainExit
    call    as_json_escape_copy

    ; ---- Exit code as decimal ----
    mov     eax, dword ptr [rsi + CTX_OFF_EXITCODE]
    call    as_uint32_to_buf        ; Writes decimal into rdi, advances rdi

    ; ---- ","stream":false} ----
    lea     rbx, szAS_JsonStreamFalse
    call    as_strcpy_advance

    ; Calculate total length
    mov     rax, rdi
    sub     rax, r12                ; rax = total JSON length

    ; Track bytes sent
    lock add qword ptr [g_AS_Counter_BytesSentToAI], rax

    add     rsp, 32
    pop     r12
    pop     rbx
    ret
as_BuildPromptJson ENDP

; =============================================================================
; as_PostToOllama — HTTP POST JSON payload, read response into suggestion buf
;
; RCX = JSON payload pointer
; RDX = JSON payload length
; R8  = Suggestion output buffer pointer
; R9  = Suggestion output capacity
; Returns: EAX = 0 on success, non-zero on failure
; =============================================================================
as_PostToOllama PROC FRAME
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
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 256
    .allocstack 256
    .endprolog

    mov     r12, rcx                ; r12 = payload
    mov     r13, rdx                ; r13 = payload length
    mov     r14, r8                 ; r14 = suggestion buffer
    mov     r15, r9                 ; r15 = suggestion capacity

    ; ---- HttpOpenRequest (POST to /api/generate) ----
    mov     rcx, qword ptr [g_AS_hConnect]
    test    rcx, rcx
    jz      @po_fail

    lea     rdx, szAS_HttpVerb      ; "POST"
    lea     r8, szAS_ApiPath        ; "/api/generate"
    xor     r9, r9                  ; lpszVersion (HTTP/1.1)
    sub     rsp, 64
    mov     qword ptr [rsp + 32], 0         ; lpszReferer
    mov     qword ptr [rsp + 40], 0         ; lpszAcceptTypes
    mov     dword ptr [rsp + 48], INTERNET_FLAG_RELOAD
    mov     dword ptr [rsp + 52], 0         ; padding
    mov     qword ptr [rsp + 56], 0         ; dwContext
    call    HttpOpenRequestA
    add     rsp, 64
    test    rax, rax
    jz      @po_fail
    mov     rbx, rax                ; rbx = hRequest

    ; ---- HttpSendRequest with JSON body ----
    mov     rcx, rbx                ; hRequest
    lea     rdx, szAS_ContentType   ; Headers
    mov     r8, -1                  ; Auto-detect header length
    mov     r9, r12                 ; lpOptional = JSON payload
    sub     rsp, 40
    mov     qword ptr [rsp + 32], r13   ; dwOptionalLength
    call    HttpSendRequestA
    add     rsp, 40
    test    eax, eax
    jz      @po_fail_close

    ; ---- Read response into g_AS_HttpResponse ----
    lea     rdi, g_AS_HttpResponse
    xor     r13, r13                ; Total bytes read

@po_read_loop:
    mov     rcx, rbx                ; hRequest
    mov     rdx, rdi                ; Buffer
    mov     r8d, 4096               ; Chunk size
    lea     r9, [rbp - 16]          ; &bytesRead (fixed stack slot)
    mov     dword ptr [rbp - 16], 0
    sub     rsp, 32
    call    InternetReadFile
    add     rsp, 32
    test    eax, eax
    jz      @po_read_done           ; Error or done

    mov     eax, dword ptr [rbp - 16]
    test    eax, eax
    jz      @po_read_done           ; EOF

    add     rdi, rax
    add     r13, rax

    ; Bounds check: don't overflow g_AS_HttpResponse
    cmp     r13, MAX_JSON_PAYLOAD - 1
    jae     @po_read_done
    jmp     @po_read_loop

@po_read_done:
    mov     byte ptr [rdi], 0       ; Null-terminate response

    ; Track bytes received
    lock add qword ptr [g_AS_Counter_BytesFromAI], r13

    ; ---- Parse "response":"..." from JSON into suggestion buffer ----
    lea     rsi, g_AS_HttpResponse
    mov     rdi, r14                ; Suggestion output buffer
    mov     rcx, r15                ; Capacity
    call    as_ParseOllamaResponse

    ; ---- Close request handle ----
    mov     rcx, rbx
    sub     rsp, 32
    call    InternetCloseHandle
    add     rsp, 32

    xor     eax, eax                ; Success
    jmp     @po_exit

@po_fail_close:
    mov     rcx, rbx
    sub     rsp, 32
    call    InternetCloseHandle
    add     rsp, 32

@po_fail:
    mov     eax, 1

@po_exit:
    lea     rsp, [rbp]
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
as_PostToOllama ENDP

; =============================================================================
; as_ParseOllamaResponse — Extract "response" field value from JSON
;
; Scans for "response":"<value>" pattern and copies <value> into output buffer.
; Handles JSON escapes: \n -> newline, \\ -> backslash, \" -> quote.
;
; RSI = Input JSON string (null-terminated)
; RDI = Output buffer
; RCX = Output capacity
; Returns: RAX = output length (0 if field not found)
; =============================================================================
as_ParseOllamaResponse PROC
    push    rbx
    push    r12
    push    r13

    mov     r12, rdi                ; r12 = output start
    mov     r13, rcx                ; r13 = capacity
    xor     ebx, ebx                ; ebx = output length

    ; Scan for "response" key
@opr_scan:
    lodsb
    test    al, al
    jz      @opr_not_found

    cmp     al, '"'
    jne     @opr_scan

    ; Check if next chars are: response"
    cmp     byte ptr [rsi], 'r'
    jne     @opr_scan
    cmp     byte ptr [rsi + 1], 'e'
    jne     @opr_scan
    cmp     byte ptr [rsi + 2], 's'
    jne     @opr_scan
    cmp     byte ptr [rsi + 3], 'p'
    jne     @opr_scan
    cmp     byte ptr [rsi + 4], 'o'
    jne     @opr_scan
    cmp     byte ptr [rsi + 5], 'n'
    jne     @opr_scan
    cmp     byte ptr [rsi + 6], 's'
    jne     @opr_scan
    cmp     byte ptr [rsi + 7], 'e'
    jne     @opr_scan
    add     rsi, 8                  ; Skip "response"

    ; Skip ":" and whitespace to opening quote
@opr_find_colon:
    lodsb
    cmp     al, '"'
    je      @opr_copy_value
    test    al, al
    jz      @opr_not_found
    jmp     @opr_find_colon

    ; Copy value, handling JSON escape sequences
@opr_copy_value:
    cmp     ebx, r13d
    jae     @opr_truncated          ; Output full

    lodsb
    test    al, al
    jz      @opr_done

    cmp     al, '"'                 ; Unescaped quote = end of value
    je      @opr_done

    cmp     al, '\'
    jne     @opr_literal

    ; Handle escape sequence
    lodsb
    cmp     al, 'n'
    je      @opr_esc_newline
    cmp     al, '\'
    je      @opr_literal
    cmp     al, '"'
    je      @opr_literal
    cmp     al, 't'
    je      @opr_esc_tab
    ; Unknown escape: emit backslash + char
    mov     byte ptr [rdi], '\'
    inc     rdi
    inc     ebx
    jmp     @opr_literal

@opr_esc_newline:
    mov     al, 0Ah
    jmp     @opr_literal
@opr_esc_tab:
    mov     al, 09h

@opr_literal:
    mov     byte ptr [rdi], al
    inc     rdi
    inc     ebx
    jmp     @opr_copy_value

@opr_truncated:
@opr_done:
    mov     byte ptr [rdi], 0       ; Null-terminate
    mov     eax, ebx
    jmp     @opr_exit

@opr_not_found:
    mov     byte ptr [r12], 0       ; Empty output
    xor     eax, eax

@opr_exit:
    pop     r13
    pop     r12
    pop     rbx
    ret
as_ParseOllamaResponse ENDP

; =============================================================================
;                    Internal String Helpers (Zero CRT)
; =============================================================================

; -----------------------------------------------------------------------------
; as_strcpy_advance — Copy null-terminated string from [rbx] to [rdi]
;                     Advances rdi past the last written byte (before null)
;                     Does NOT write null terminator
; Preserves: rsi, r12-r15
; -----------------------------------------------------------------------------
as_strcpy_advance PROC
    push    rax
    push    rcx
    mov     rcx, rbx
@sa_loop:
    mov     al, byte ptr [rcx]
    test    al, al
    jz      @sa_done
    mov     byte ptr [rdi], al
    inc     rcx
    inc     rdi
    jmp     @sa_loop
@sa_done:
    pop     rcx
    pop     rax
    ret
as_strcpy_advance ENDP

; -----------------------------------------------------------------------------
; as_json_escape_copy — Copy string from [rbx] to [rdi] with JSON escaping
;                       Escapes: " -> \", \ -> \\, \n -> \\n, \r -> \\r
;                       Advances rdi
; Preserves: rsi, r12-r15
; -----------------------------------------------------------------------------
as_json_escape_copy PROC
    push    rax
    push    rcx
    mov     rcx, rbx
@jec_loop:
    mov     al, byte ptr [rcx]
    test    al, al
    jz      @jec_done

    cmp     al, '"'
    je      @jec_esc_quote
    cmp     al, '\'
    je      @jec_esc_backslash
    cmp     al, 0Ah                 ; \n
    je      @jec_esc_newline
    cmp     al, 0Dh                 ; \r
    je      @jec_esc_cr
    cmp     al, 09h                 ; \t
    je      @jec_esc_tab

    ; Normal character
    mov     byte ptr [rdi], al
    inc     rdi
    inc     rcx
    jmp     @jec_loop

@jec_esc_quote:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], '"'
    add     rdi, 2
    inc     rcx
    jmp     @jec_loop

@jec_esc_backslash:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], '\'
    add     rdi, 2
    inc     rcx
    jmp     @jec_loop

@jec_esc_newline:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], 'n'
    add     rdi, 2
    inc     rcx
    jmp     @jec_loop

@jec_esc_cr:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], 'r'
    add     rdi, 2
    inc     rcx
    jmp     @jec_loop

@jec_esc_tab:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], 't'
    add     rdi, 2
    inc     rcx
    jmp     @jec_loop

@jec_done:
    pop     rcx
    pop     rax
    ret
as_json_escape_copy ENDP

; -----------------------------------------------------------------------------
; as_json_escape_copy_n — Same as above but limited to ECX bytes
; RBX = source, RDI = dest, ECX = max bytes to read
; -----------------------------------------------------------------------------
as_json_escape_copy_n PROC
    push    rax
    push    rdx
    mov     edx, ecx                ; edx = remaining count
    mov     rcx, rbx                ; rcx = source ptr
@jecn_loop:
    test    edx, edx
    jz      @jecn_done

    mov     al, byte ptr [rcx]
    test    al, al
    jz      @jecn_done

    cmp     al, '"'
    je      @jecn_esc_q
    cmp     al, '\'
    je      @jecn_esc_bs
    cmp     al, 0Ah
    je      @jecn_esc_nl
    cmp     al, 0Dh
    je      @jecn_esc_cr

    mov     byte ptr [rdi], al
    inc     rdi
    inc     rcx
    dec     edx
    jmp     @jecn_loop

@jecn_esc_q:
    mov     word ptr [rdi], 225Ch   ; '\"' little-endian
    add     rdi, 2
    inc     rcx
    dec     edx
    jmp     @jecn_loop

@jecn_esc_bs:
    mov     word ptr [rdi], 5C5Ch   ; '\\' little-endian
    add     rdi, 2
    inc     rcx
    dec     edx
    jmp     @jecn_loop

@jecn_esc_nl:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], 'n'
    add     rdi, 2
    inc     rcx
    dec     edx
    jmp     @jecn_loop

@jecn_esc_cr:
    mov     byte ptr [rdi], '\'
    mov     byte ptr [rdi + 1], 'r'
    add     rdi, 2
    inc     rcx
    dec     edx
    jmp     @jecn_loop

@jecn_done:
    pop     rdx
    pop     rax
    ret
as_json_escape_copy_n ENDP

; -----------------------------------------------------------------------------
; as_uint32_to_buf — Convert unsigned 32-bit integer in EAX to decimal ASCII
;                    Writes directly into [rdi], advances rdi
; Preserves: rsi, rbx, r12-r15
; Clobbers: rax, rcx, rdx, r8
; -----------------------------------------------------------------------------
as_uint32_to_buf PROC
    push    rbx

    ; Handle zero case
    test    eax, eax
    jnz     @u2b_nonzero
    mov     byte ptr [rdi], '0'
    inc     rdi
    pop     rbx
    ret

@u2b_nonzero:
    ; Convert digits in reverse into stack scratch
    ; Max 10 digits for uint32
    mov     ebx, eax                ; save value
    lea     r8, [rdi + 10]          ; scratch end (within reasonable range)
    xor     ecx, ecx                ; digit count

@u2b_loop:
    test    ebx, ebx
    jz      @u2b_emit

    mov     eax, ebx
    xor     edx, edx
    mov     r8d, 10
    div     r8d                     ; eax = quotient, edx = remainder
    mov     ebx, eax
    add     dl, '0'

    ; Push digit
    push    rdx
    inc     ecx
    jmp     @u2b_loop

@u2b_emit:
    ; Pop digits in correct order
@u2b_pop:
    test    ecx, ecx
    jz      @u2b_done
    pop     rax
    mov     byte ptr [rdi], al
    inc     rdi
    dec     ecx
    jmp     @u2b_pop

@u2b_done:
    pop     rbx
    ret
as_uint32_to_buf ENDP

END
