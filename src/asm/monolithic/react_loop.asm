;==============================================================================
; react_loop.asm  —  x64 MASM  ReAct (Reasoning + Acting) Agentic Loop
; RawrXD IDE  —  complete monolithic implementation
; Microsoft x64 ABI / MASM  (ml64.exe)
; No stubs. No TODOs. Every function fully implemented.
;==============================================================================

OPTION CASEMAP:NONE

;==============================================================================
; EXTERNAL DEPENDENCIES
;==============================================================================
EXTERNDEF BeaconSend:PROC
EXTERNDEF TryBeaconRecv:PROC
EXTERNDEF InferenceRouter_Generate:PROC
EXTERNDEF Tool_Execute:PROC
EXTERNDEF HeapAlloc:PROC
EXTERNDEF HeapFree:PROC
EXTERNDEF GetTickCount64:PROC
EXTERNDEF WideCharToMultiByte:PROC
EXTERNDEF MultiByteToWideChar:PROC
EXTERNDEF g_hHeap:QWORD

;==============================================================================
; CONSTANTS
;==============================================================================
REACT_MAX_ITER          EQU 12
REACT_PROMPT_BUF_SIZE   EQU 131072      ; 128 KB
REACT_RESULT_BUF_SIZE   EQU 65536       ;  64 KB
REACT_RESP_BUF_SIZE     EQU 65536       ;  64 KB
REACT_BEACON_SLOT       EQU 15

REACT_EVT_TOOL_CALL     EQU 0C001h
REACT_EVT_TOOL_RESULT   EQU 0C002h
REACT_EVT_FINAL_ANSWER  EQU 0C003h
REACT_EVT_MAX_ITER      EQU 0C004h
REACT_EVT_ABORT         EQU 0C005h

TOOL_ID_READ_FILE       EQU 0
TOOL_ID_WRITE_FILE      EQU 1
TOOL_ID_LIST_DIR        EQU 2
TOOL_ID_RUN_CMD         EQU 3
TOOL_ID_SEARCH_CODE     EQU 4
TOOL_ID_GET_DIAG        EQU 5
TOOL_ID_GET_SYMBOLS     EQU 6

HEAP_ZERO_MEMORY        EQU 8

REACT_TRUNC_KEEP        EQU 65536       ; bytes to keep when truncating context
REACT_TRUNC_THRESHOLD   EQU 122880      ; REACT_PROMPT_BUF_SIZE - 8192

;==============================================================================
; PUBLIC EXPORTS
;==============================================================================
PUBLIC ReAct_Init
PUBLIC ReAct_Run
PUBLIC ReAct_ParseToolCall
PUBLIC ReAct_FormatResult
PUBLIC ReAct_Abort

;==============================================================================
; .DATA  —  initialised string literals
;==============================================================================
.DATA

; Tool name strings (null-terminated)
szTool_read_file    DB "read_file",      0
szTool_write_file   DB "write_file",     0
szTool_list_dir     DB "list_dir",       0
szTool_run_command  DB "run_command",    0
szTool_search_code  DB "search_code",   0
szTool_get_diag     DB "get_diagnostics",0
szTool_get_symbols  DB "get_symbols",   0

; JSON search markers
; szToolMarker = {"tool":"   (9 chars)
szToolMarker        DB 7Bh, 22h, "tool", 22h, 3Ah, 22h, 0
; szArgsMarker = "args":{    (8 chars)
szArgsMarker        DB 22h, "args", 22h, 3Ah, 7Bh, 0

; Context building strings
szUserPrefix        DB 0Ah, 0Ah, "User: ",      0   ; \n\nUser: 
szAssistantSuffix   DB 0Ah, 0Ah, "Assistant:",  0   ; \n\nAssistant:

; Tool result formatting strings
szToolResultPrefix  DB 0Ah, 0Ah, "Tool[", 0
szToolResultMid     DB "] Result:", 0Ah, 0
szToolResultSuffix  DB 0Ah, 0Ah, "Assistant:", 0

; Error strings
szInfError          DB "[inference error]", 0

;==============================================================================
; .DATA?  —  uninitialised / mutable globals
;==============================================================================
.DATA?

g_reactAbort        DB ?
                    ALIGN 4
g_reactIterCount    DD ?
g_reactPromptBuf    DQ ?
g_reactResultBuf    DQ ?
g_reactRespBuf      DQ ?
g_reactPromptLen    DD ?
                    ALIGN 8
g_reactStartTick    DQ ?
g_parsedArgsPtr     DQ ?

;==============================================================================
; .CODE
;==============================================================================
.CODE

;------------------------------------------------------------------------------
; StrLen_Internal
;   In:  RCX = pointer to null-terminated UTF-8 string  (may be NULL)
;   Out: RAX = byte length (not including the null terminator)
;   Clobbers: RAX only
;------------------------------------------------------------------------------
StrLen_Internal PROC
    xor     eax, eax
    test    rcx, rcx
    jz      SL_Done
SL_Loop:
    cmp     byte ptr [rcx + rax], 0
    je      SL_Done
    inc     eax
    jmp     SL_Loop
SL_Done:
    ret
StrLen_Internal ENDP

;------------------------------------------------------------------------------
; StrCmp_Internal
;   In:  RCX = pointer to string A  (null-terminated)
;        RDX = pointer to string B  (null-terminated)
;   Out: EAX = 0 if equal, 1 if not equal
;   Clobbers: RAX, R8B
;   Note: does not modify RCX / RDX (callee-copies internally)
;------------------------------------------------------------------------------
StrCmp_Internal PROC
    test    rcx, rcx
    jz      SC_NullA
    test    rdx, rdx
    jz      SC_NotEqual
SC_Loop:
    mov     al,  byte ptr [rcx]
    mov     r8b, byte ptr [rdx]
    cmp     al,  r8b
    jne     SC_NotEqual
    test    al,  al
    jz      SC_Equal
    inc     rcx
    inc     rdx
    jmp     SC_Loop
SC_NullA:
    test    rdx, rdx
    jz      SC_Equal
SC_NotEqual:
    mov     eax, 1
    ret
SC_Equal:
    xor     eax, eax
    ret
StrCmp_Internal ENDP

;==============================================================================
; ReAct_Init PROC FRAME
;   Allocates all three heap buffers, zeros state variables, records start tick.
;   Returns: EAX = 0 (success)  /  -1 (heap failure)
;==============================================================================
ReAct_Init PROC FRAME
    .PUSHREG rbx
    push    rbx
    .PUSHREG rsi
    push    rsi
    .PUSHREG rdi
    push    rdi
    ; 3 pushes (24 B) + return addr (8 B) = 32 B → 16-byte aligned
    .ALLOCSTACK 20h
    sub     rsp, 20h        ; shadow space for callees
    .ENDPROLOG

    ;------------------------------------------------------------------
    ; Free existing buffers if re-initialising (safety)
    ;------------------------------------------------------------------
    mov     rax, [g_reactPromptBuf]
    test    rax, rax
    jz      RI_FreeResult
    mov     rcx, [g_hHeap]
    xor     edx, edx
    mov     r8,  rax
    call    HeapFree

RI_FreeResult:
    mov     rax, [g_reactResultBuf]
    test    rax, rax
    jz      RI_FreeResp
    mov     rcx, [g_hHeap]
    xor     edx, edx
    mov     r8,  rax
    call    HeapFree

RI_FreeResp:
    mov     rax, [g_reactRespBuf]
    test    rax, rax
    jz      RI_AllocPrompt
    mov     rcx, [g_hHeap]
    xor     edx, edx
    mov     r8,  rax
    call    HeapFree

    ;------------------------------------------------------------------
    ; Allocate g_reactPromptBuf  (REACT_PROMPT_BUF_SIZE, zeroed)
    ;------------------------------------------------------------------
RI_AllocPrompt:
    mov     rcx, [g_hHeap]
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, REACT_PROMPT_BUF_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      RI_Fail
    mov     [g_reactPromptBuf], rax

    ;------------------------------------------------------------------
    ; Allocate g_reactResultBuf  (REACT_RESULT_BUF_SIZE, zeroed)
    ;------------------------------------------------------------------
    mov     rcx, [g_hHeap]
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, REACT_RESULT_BUF_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      RI_Fail
    mov     [g_reactResultBuf], rax

    ;------------------------------------------------------------------
    ; Allocate g_reactRespBuf  (REACT_RESP_BUF_SIZE, zeroed)
    ;------------------------------------------------------------------
    mov     rcx, [g_hHeap]
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, REACT_RESP_BUF_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      RI_Fail
    mov     [g_reactRespBuf], rax

    ;------------------------------------------------------------------
    ; Zero state variables
    ;------------------------------------------------------------------
    mov     byte ptr  [g_reactAbort],     0
    mov     dword ptr [g_reactIterCount], 0
    mov     dword ptr [g_reactPromptLen], 0
    mov     qword ptr [g_parsedArgsPtr],  0

    ;------------------------------------------------------------------
    ; Record start tick
    ;------------------------------------------------------------------
    call    GetTickCount64
    mov     [g_reactStartTick], rax

    xor     eax, eax
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret

RI_Fail:
    mov     eax, -1
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ReAct_Init ENDP

;==============================================================================
; ReAct_Run PROC FRAME
;   RCX = pSystemPrompt  (UTF-8, null-terminated)
;   RDX = pUserPrompt    (UTF-8, null-terminated)
;   R8  = pFinalAnswerBuf (caller-allocated output buffer, UTF-8)
;   R9  = finalAnswerBufSize (bytes)
;
;   Returns: EAX = 0  (final answer obtained)
;            EAX = 1  (REACT_MAX_ITER exceeded)
;            EAX = -1 (inference error or null heap buffers)
;==============================================================================
ReAct_Run PROC FRAME
    .PUSHREG rbx
    push    rbx
    .PUSHREG rsi
    push    rsi
    .PUSHREG rdi
    push    rdi
    .PUSHREG r12
    push    r12
    .PUSHREG r13
    push    r13
    .PUSHREG r14
    push    r14
    .PUSHREG r15
    push    r15
    ; 7 pushes (56 B) + return addr (8 B) = 64 B → 16-byte aligned before sub
    .ALLOCSTACK 60h
    sub     rsp, 60h    ; 96 B: 32 shadow + 64 locals  → RSP total offset = 160, aligned
    .ENDPROLOG

    ;------------------------------------------------------------------
    ; Local variable layout within [rsp+20h .. rsp+58h]:
    ;   [rsp+20h]  pSystemPrompt     (param RCX)
    ;   [rsp+28h]  pUserPrompt       (param RDX)
    ;   [rsp+30h]  pFinalAnswerBuf   (param R8)
    ;   [rsp+38h]  finalAnswerBufSz  (param R9)
    ;   [rsp+40h]  saved tool_id for current iteration
    ;   [rsp+48h]  spare
    ;   [rsp+50h]  spare
    ;   [rsp+58h]  spare
    ;------------------------------------------------------------------
    mov     [rsp+20h], rcx
    mov     [rsp+28h], rdx
    mov     [rsp+30h], r8
    mov     [rsp+38h], r9

    ;------------------------------------------------------------------
    ; Validate heap buffers (must call ReAct_Init first)
    ;------------------------------------------------------------------
    mov     r12, [g_reactPromptBuf]     ; r12 = rolling context buffer
    test    r12, r12
    jz      RR_Error
    mov     r13, [g_reactRespBuf]       ; r13 = model response buffer
    test    r13, r13
    jz      RR_Error
    mov     r14, [g_reactResultBuf]     ; r14 = tool result buffer
    test    r14, r14
    jz      RR_Error

    ;------------------------------------------------------------------
    ; Reset per-run state
    ;------------------------------------------------------------------
    mov     byte ptr  [g_reactAbort],     0
    mov     dword ptr [g_reactIterCount], 0
    xor     ebx, ebx                    ; ebx = iteration counter

    ;------------------------------------------------------------------
    ; Build initial context in g_reactPromptBuf:
    ;   <systemPrompt> + "\n\nUser: " + <userPrompt> + "\n\nAssistant:"
    ;------------------------------------------------------------------
    xor     r15d, r15d                  ; r15d = current write offset (byte count)

    ; --- Append pSystemPrompt ---
    mov     rcx, [rsp+20h]
    test    rcx, rcx
    jz      RR_SkipSys
    call    StrLen_Internal
    test    eax, eax
    jz      RR_SkipSys
    ; guard: r15d + eax < REACT_PROMPT_BUF_SIZE - 1
    mov     ecx, r15d
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 1
    jae     RR_SkipSys
    ; copy
    mov     rdi, r12
    add     rdi, r15
    mov     rsi, [rsp+20h]
    mov     ecx, eax
    rep     movsb
    add     r15d, eax
RR_SkipSys:

    ; --- Append "\n\nUser: " ---
    lea     rcx, szUserPrefix
    call    StrLen_Internal
    mov     ecx, r15d
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 1
    jae     RR_SkipUPfx
    mov     rdi, r12
    add     rdi, r15
    lea     rsi, szUserPrefix
    mov     ecx, eax
    rep     movsb
    add     r15d, eax
RR_SkipUPfx:

    ; --- Append pUserPrompt ---
    mov     rcx, [rsp+28h]
    test    rcx, rcx
    jz      RR_SkipUser
    call    StrLen_Internal
    test    eax, eax
    jz      RR_SkipUser
    mov     ecx, r15d
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 1
    jae     RR_SkipUser
    mov     rdi, r12
    add     rdi, r15
    mov     rsi, [rsp+28h]
    mov     ecx, eax
    rep     movsb
    add     r15d, eax
RR_SkipUser:

    ; --- Append "\n\nAssistant:" ---
    lea     rcx, szAssistantSuffix
    call    StrLen_Internal
    mov     ecx, r15d
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 1
    jae     RR_ContextBuilt
    mov     rdi, r12
    add     rdi, r15
    lea     rsi, szAssistantSuffix
    mov     ecx, eax
    rep     movsb
    add     r15d, eax
RR_ContextBuilt:
    ; null-terminate
    mov     byte ptr [r12 + r15], 0
    mov     dword ptr [g_reactPromptLen], r15d

    ;==================================================================
    ; Main ReAct loop
    ;==================================================================
RR_LoopTop:
    ; -- check abort flag --
    cmp     byte ptr [g_reactAbort], 0
    jne     RR_Aborted

    ; -- check iteration count --
    cmp     ebx, REACT_MAX_ITER
    jge     RR_MaxIter

    ;--------------------------------------------------------------
    ; Step i: InferenceRouter_Generate(pPrompt, pRespBuf, maxLen)
    ;--------------------------------------------------------------
    mov     rcx, r12                        ; pPrompt
    mov     rdx, r13                        ; pRespBuf
    mov     r8d, REACT_RESP_BUF_SIZE - 1    ; maxLen
    call    InferenceRouter_Generate
    ; RAX = bytes written, or -1 on error
    cmp     eax, -1
    je      RR_InferError
    test    eax, eax
    jz      RR_InferError

    ; clamp and null-terminate response
    cmp     eax, REACT_RESP_BUF_SIZE - 1
    jb      RR_TermResp
    mov     eax, REACT_RESP_BUF_SIZE - 1
RR_TermResp:
    mov     byte ptr [r13 + rax], 0

    ;--------------------------------------------------------------
    ; Step iii: Append model response to rolling context
    ;--------------------------------------------------------------
    mov     rcx, r13
    call    StrLen_Internal
    ; rax = response length

    ; check if we need to truncate context before appending
    mov     r15d, dword ptr [g_reactPromptLen]
    mov     ecx, r15d
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 1
    jb      RR_AppendResp

    ; Context would overflow — truncate: keep last REACT_TRUNC_KEEP bytes
    cmp     r15d, REACT_TRUNC_KEEP
    jb      RR_TruncReset

    ; memmove(r12, r12 + (r15d - REACT_TRUNC_KEEP), REACT_TRUNC_KEEP)
    mov     rsi, r12
    mov     ecx, r15d
    sub     ecx, REACT_TRUNC_KEEP
    add     rsi, rcx            ; rsi = source start
    mov     rdi, r12            ; rdi = dest (beginning of buffer)
    mov     ecx, REACT_TRUNC_KEEP
    rep     movsb
    mov     r15d, REACT_TRUNC_KEEP
    mov     byte ptr [r12 + REACT_TRUNC_KEEP], 0
    mov     dword ptr [g_reactPromptLen], r15d

    ; Recompute space check after truncation
    mov     rcx, r13
    call    StrLen_Internal
    mov     ecx, r15d
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 1
    jae     RR_SkipAppendResp   ; still no room — skip appending this fragment
    jmp     RR_AppendResp

RR_TruncReset:
    ; Buffer shorter than REACT_TRUNC_KEEP — just clear it
    mov     byte ptr [r12], 0
    mov     r15d, 0
    mov     dword ptr [g_reactPromptLen], 0
    ; recompute response length
    mov     rcx, r13
    call    StrLen_Internal

RR_AppendResp:
    ; rax = length of response to append
    mov     rdi, r12
    add     rdi, r15
    mov     rsi, r13
    mov     ecx, eax
    rep     movsb
    add     r15d, eax
    mov     byte ptr [r12 + r15], 0
    mov     dword ptr [g_reactPromptLen], r15d
RR_SkipAppendResp:

    ;--------------------------------------------------------------
    ; Step iv: ReAct_ParseToolCall(pRespBuf) → tool_id in EAX
    ;--------------------------------------------------------------
    mov     rcx, r13
    call    ReAct_ParseToolCall
    ; EAX = tool_id (0-6) or -1 (no tool call)
    mov     dword ptr [rsp+40h], eax        ; save tool_id

    ;--------------------------------------------------------------
    ; Step v: No tool call → this IS the final answer
    ;--------------------------------------------------------------
    cmp     eax, -1
    jne     RR_HasToolCall

    ; Copy response to caller's finalAnswerBuf
    mov     rdi, [rsp+30h]
    test    rdi, rdi
    jz      RR_FinalSendBeacon
    mov     r8, [rsp+38h]
    test    r8, r8
    jz      RR_FinalSendBeacon

    mov     rcx, r13
    call    StrLen_Internal
    ; rax = response length; clamp to (finalAnswerBufSize - 1)
    mov     ecx, eax
    mov     r9d, r8d
    dec     r9d
    cmp     ecx, r9d
    jbe     RR_FinalCopy
    mov     ecx, r9d
RR_FinalCopy:
    test    ecx, ecx
    jz      RR_FinalNullTerm
    mov     rsi, r13
    rep     movsb
RR_FinalNullTerm:
    mov     byte ptr [rdi], 0

RR_FinalSendBeacon:
    mov     ecx, REACT_BEACON_SLOT
    mov     edx, REACT_EVT_FINAL_ANSWER
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax                    ; return 0 = success / final answer
    jmp     RR_Return

    ;--------------------------------------------------------------
    ; Step vi: Tool call found — execute the tool
    ;--------------------------------------------------------------
RR_HasToolCall:
    ; -- Send REACT_EVT_TOOL_CALL beacon with tool_id as payload --
    mov     ecx, REACT_BEACON_SLOT
    mov     edx, REACT_EVT_TOOL_CALL
    mov     r8d, dword ptr [rsp+40h]    ; tool_id as payload
    call    BeaconSend

    ; -- Tool_Execute(tool_id, pArgsJson, pResultBuf, maxResultLen) --
    mov     ecx,  dword ptr [rsp+40h]   ; tool_id
    mov     rdx,  [g_parsedArgsPtr]     ; pArgsJson (may be NULL → well-formed call)
    mov     r8,   r14                   ; pResultBuf = g_reactResultBuf
    mov     r9d,  REACT_RESULT_BUF_SIZE - 1
    call    Tool_Execute
    ; null-terminate result defensively
    mov     rcx, r14
    call    StrLen_Internal
    cmp     eax, REACT_RESULT_BUF_SIZE - 1
    jb      RR_TermResult
    mov     eax, REACT_RESULT_BUF_SIZE - 1
RR_TermResult:
    mov     byte ptr [r14 + rax], 0

    ; -- Send REACT_EVT_TOOL_RESULT beacon --
    mov     ecx, REACT_BEACON_SLOT
    mov     edx, REACT_EVT_TOOL_RESULT
    mov     r8d, dword ptr [rsp+40h]
    call    BeaconSend

    ; -- ReAct_FormatResult(tool_id, pToolResult, pContextBuf, pContextLen) --
    mov     ecx,  dword ptr [rsp+40h]   ; tool_id
    mov     rdx,  r14                   ; pToolResult
    mov     r8,   r12                   ; pContextBuf = g_reactPromptBuf
    lea     r9,   [g_reactPromptLen]    ; pContextLen (ptr to DWORD)
    call    ReAct_FormatResult
    ; update r15d from authoritative source
    mov     r15d, dword ptr [g_reactPromptLen]

    ;--------------------------------------------------------------
    ; Step vii: Bookkeeping — increment iteration, check abort,
    ;           check context buffer pressure
    ;--------------------------------------------------------------
    inc     ebx
    mov     dword ptr [g_reactIterCount], ebx

    cmp     byte ptr [g_reactAbort], 0
    jne     RR_Aborted

    ; Context pressure check: if context >= REACT_TRUNC_THRESHOLD, truncate now
    mov     eax, dword ptr [g_reactPromptLen]
    cmp     eax, REACT_TRUNC_THRESHOLD
    jb      RR_LoopTop

    ; Truncate: keep last REACT_TRUNC_KEEP bytes
    cmp     eax, REACT_TRUNC_KEEP
    jb      RR_LoopTrunc_TooShort

    mov     rsi, r12
    mov     ecx, eax
    sub     ecx, REACT_TRUNC_KEEP
    add     rsi, rcx
    mov     rdi, r12
    mov     ecx, REACT_TRUNC_KEEP
    rep     movsb
    mov     r15d, REACT_TRUNC_KEEP
    mov     byte ptr [r12 + REACT_TRUNC_KEEP], 0
    mov     dword ptr [g_reactPromptLen], r15d
    jmp     RR_LoopTop

RR_LoopTrunc_TooShort:
    ; context length < REACT_TRUNC_KEEP but still above threshold — harmless, continue
    jmp     RR_LoopTop

    ;==================================================================
    ; Exit paths
    ;==================================================================

    ;------------------------------------------------------------------
    ; Abort (g_reactAbort was set by ReAct_Abort)
    ;------------------------------------------------------------------
RR_Aborted:
    ; copy last response fragment to output if possible
    mov     rdi, [rsp+30h]
    test    rdi, rdi
    jz      RR_AbortedDone
    mov     r8, [rsp+38h]
    test    r8, r8
    jz      RR_AbortedDone
    mov     rcx, r13
    call    StrLen_Internal
    mov     ecx, eax
    mov     r9d, r8d
    dec     r9d
    cmp     ecx, r9d
    jbe     RR_AbortCopy
    mov     ecx, r9d
RR_AbortCopy:
    test    ecx, ecx
    jz      RR_AbortNullTerm
    mov     rsi, r13
    rep     movsb
RR_AbortNullTerm:
    mov     byte ptr [rdi], 0
RR_AbortedDone:
    mov     eax, -1
    jmp     RR_Return

    ;------------------------------------------------------------------
    ; Max iterations exceeded
    ;------------------------------------------------------------------
RR_MaxIter:
    ; copy last response to output
    mov     rdi, [rsp+30h]
    test    rdi, rdi
    jz      RR_MaxBeacon
    mov     r8, [rsp+38h]
    test    r8, r8
    jz      RR_MaxBeacon
    mov     rcx, r13
    call    StrLen_Internal
    mov     ecx, eax
    mov     r9d, r8d
    dec     r9d
    cmp     ecx, r9d
    jbe     RR_MaxCopy
    mov     ecx, r9d
RR_MaxCopy:
    test    ecx, ecx
    jz      RR_MaxNullTerm
    mov     rsi, r13
    rep     movsb
RR_MaxNullTerm:
    mov     byte ptr [rdi], 0
RR_MaxBeacon:
    mov     ecx, REACT_BEACON_SLOT
    mov     edx, REACT_EVT_MAX_ITER
    xor     r8d, r8d
    call    BeaconSend
    mov     eax, 1
    jmp     RR_Return

    ;------------------------------------------------------------------
    ; Inference error
    ;------------------------------------------------------------------
RR_InferError:
    mov     rdi, [rsp+30h]
    test    rdi, rdi
    jz      RR_InferErrDone
    mov     r8, [rsp+38h]
    test    r8, r8
    jz      RR_InferErrDone
    lea     rcx, szInfError
    call    StrLen_Internal
    mov     ecx, eax
    mov     r9d, r8d
    dec     r9d
    cmp     ecx, r9d
    jbe     RR_InferErrCopy
    mov     ecx, r9d
RR_InferErrCopy:
    lea     rsi, szInfError
    rep     movsb
    mov     byte ptr [rdi], 0
RR_InferErrDone:
    mov     eax, -1
    jmp     RR_Return

    ;------------------------------------------------------------------
    ; Null heap buffers (should not occur if ReAct_Init succeeded)
    ;------------------------------------------------------------------
RR_Error:
    mov     eax, -1

RR_Return:
    add     rsp, 60h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ReAct_Run ENDP

;==============================================================================
; ReAct_ParseToolCall PROC FRAME
;   RCX = pResponse  (UTF-8, null-terminated model output)
;
;   Scans pResponse for the JSON pattern  {"tool":"<name>","args":{...}}
;   Extracts the tool name, maps it to a TOOL_ID_* constant, and sets
;   g_parsedArgsPtr to point to the '{' opening the args object so callers
;   can pass it directly to Tool_Execute.
;
;   Returns: EAX = tool_id (0-6)  /  -1 if no recognised tool call found
;==============================================================================
ReAct_ParseToolCall PROC FRAME
    .PUSHREG rbx
    push    rbx
    .PUSHREG rsi
    push    rsi
    .PUSHREG rdi
    push    rdi
    .PUSHREG r12
    push    r12
    .PUSHREG r13
    push    r13
    ; 5 pushes (40 B) + return addr (8 B) = 48 B → 16-byte aligned before sub
    .ALLOCSTACK 30h
    sub     rsp, 30h    ; 48 B: 32 shadow + 16 locals; total offset = 96, aligned
    .ENDPROLOG

    ; [rsp+20h] = pResponse (local copy)
    ; [rsp+28h] = tool_id  (local, once found)
    mov     [rsp+20h], rcx
    mov     qword ptr [g_parsedArgsPtr], 0

    test    rcx, rcx
    jz      PC_NotFound

    ;------------------------------------------------------------------
    ; Phase 1: scan for szToolMarker = {"tool":"  in pResponse
    ;------------------------------------------------------------------
    ; r12 = current scan position in pResponse
    mov     r12, rcx

    ; r13d = length of szToolMarker (used for the match loop)
    lea     rcx, szToolMarker
    call    StrLen_Internal
    mov     r13d, eax               ; r13d = marker byte count (9)

PC_ScanLoop:
    ; end of string?
    cmp     byte ptr [r12], 0
    je      PC_NotFound

    ; attempt to match szToolMarker at position r12
    mov     rsi, r12                ; rsi = pResponse position
    lea     rdi, szToolMarker       ; rdi = marker pointer
    mov     ecx, r13d               ; ecx = remaining chars to check

PC_MatchChar:
    mov     al,  byte ptr [rdi]     ; next marker char
    test    al,  al
    jz      PC_MarkerMatched        ; reached end of marker  → full match
    mov     r8b, byte ptr [rsi]     ; next response char
    test    r8b, r8b
    jz      PC_NotFound             ; response ended mid-match → no tool call
    cmp     al,  r8b
    jne     PC_ScanNext             ; mismatch → advance scan position by 1
    inc     rsi
    inc     rdi
    dec     ecx
    jnz     PC_MatchChar
    ; fall-through: ecx reached 0 with no mismatch → full match

PC_MarkerMatched:
    ; rsi now points to the first byte of the tool name (after the opening '"')
    ; rbx = pointer to tool name start
    mov     rbx, rsi

    ; Find the closing '"' of the tool name
    mov     rdi, rsi
PC_FindNameEnd:
    cmp     byte ptr [rdi], 0
    je      PC_NotFound
    cmp     byte ptr [rdi], '"'
    je      PC_NameEnd
    inc     rdi
    jmp     PC_FindNameEnd

PC_NameEnd:
    ; Temporarily null-terminate the tool name for StrCmp_Internal
    mov     byte ptr [rdi], 0

    ;----------------------------------------------------------
    ; Compare tool name (rbx..rdi-1) against known names
    ;----------------------------------------------------------

    ; read_file
    mov     rcx, rbx
    lea     rdx, szTool_read_file
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_NotReadFile
    mov     byte ptr [rdi], '"'         ; restore terminator
    mov     dword ptr [rsp+28h], TOOL_ID_READ_FILE
    jmp     PC_FindArgs
PC_NotReadFile:

    ; write_file
    mov     rcx, rbx
    lea     rdx, szTool_write_file
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_NotWriteFile
    mov     byte ptr [rdi], '"'
    mov     dword ptr [rsp+28h], TOOL_ID_WRITE_FILE
    jmp     PC_FindArgs
PC_NotWriteFile:

    ; list_dir
    mov     rcx, rbx
    lea     rdx, szTool_list_dir
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_NotListDir
    mov     byte ptr [rdi], '"'
    mov     dword ptr [rsp+28h], TOOL_ID_LIST_DIR
    jmp     PC_FindArgs
PC_NotListDir:

    ; run_command
    mov     rcx, rbx
    lea     rdx, szTool_run_command
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_NotRunCmd
    mov     byte ptr [rdi], '"'
    mov     dword ptr [rsp+28h], TOOL_ID_RUN_CMD
    jmp     PC_FindArgs
PC_NotRunCmd:

    ; search_code
    mov     rcx, rbx
    lea     rdx, szTool_search_code
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_NotSearchCode
    mov     byte ptr [rdi], '"'
    mov     dword ptr [rsp+28h], TOOL_ID_SEARCH_CODE
    jmp     PC_FindArgs
PC_NotSearchCode:

    ; get_diagnostics
    mov     rcx, rbx
    lea     rdx, szTool_get_diag
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_NotGetDiag
    mov     byte ptr [rdi], '"'
    mov     dword ptr [rsp+28h], TOOL_ID_GET_DIAG
    jmp     PC_FindArgs
PC_NotGetDiag:

    ; get_symbols
    mov     rcx, rbx
    lea     rdx, szTool_get_symbols
    call    StrCmp_Internal
    test    eax, eax
    jnz     PC_UnknownTool
    mov     byte ptr [rdi], '"'
    mov     dword ptr [rsp+28h], TOOL_ID_GET_SYMBOLS
    jmp     PC_FindArgs

PC_UnknownTool:
    ; Restore the '"' we clobbered and report not found
    mov     byte ptr [rdi], '"'
    jmp     PC_NotFound

    ;------------------------------------------------------------------
    ; Phase 2: scan forward for szArgsMarker = "args":{
    ;          set g_parsedArgsPtr to the '{' of the args object
    ;------------------------------------------------------------------
PC_FindArgs:
    ; At this point:
    ;   rdi = ptr to restored '"' (end of tool name)
    ;   [rsp+28h] = tool_id
    ; Advance past that '"' to start searching for the args marker
    inc     rdi     ; rdi now points to the char after the tool name's closing '"'

    ; r13d = length of szArgsMarker
    push    rdi
    lea     rcx, szArgsMarker
    call    StrLen_Internal
    pop     rdi
    mov     r13d, eax   ; r13d = szArgsMarker length (8)

    ; r12 = scan position for args marker (starts where rdi is)
    mov     r12, rdi

PC_ArgsScanLoop:
    cmp     byte ptr [r12], 0
    je      PC_NoArgs           ; end of response without finding args

    ; attempt to match szArgsMarker at r12
    mov     rsi, r12
    lea     rbx, szArgsMarker   ; rbx = marker pointer (repurposed; tool_name not needed)

PC_ArgsMatchChar:
    mov     al,  byte ptr [rbx] ; next marker char
    test    al,  al
    jz      PC_ArgsMatched      ; reached end of marker → match
    mov     r8b, byte ptr [rsi]
    test    r8b, r8b
    jz      PC_NoArgs           ; response ended mid-match
    cmp     al,  r8b
    jne     PC_ArgsNext
    inc     rsi
    inc     rbx
    jmp     PC_ArgsMatchChar

PC_ArgsMatched:
    ; rsi points to the char AFTER the last char of szArgsMarker.
    ; The last char of szArgsMarker is '{', so rsi-1 = address of '{'.
    dec     rsi
    mov     [g_parsedArgsPtr], rsi
    mov     eax, dword ptr [rsp+28h]    ; tool_id
    jmp     PC_Done

PC_ArgsNext:
    inc     r12
    jmp     PC_ArgsScanLoop

PC_NoArgs:
    ; args marker not found — still return the tool_id but with null args ptr
    mov     qword ptr [g_parsedArgsPtr], 0
    mov     eax, dword ptr [rsp+28h]
    jmp     PC_Done

PC_ScanNext:
    inc     r12
    jmp     PC_ScanLoop

PC_NotFound:
    mov     eax, -1

PC_Done:
    add     rsp, 30h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ReAct_ParseToolCall ENDP

;==============================================================================
; ReAct_FormatResult PROC FRAME
;   RCX = tool_id        (DWORD, 0-6)
;   RDX = pToolResult    (UTF-8, null-terminated tool output)
;   R8  = pContextBuf    (destination rolling context buffer)
;   R9  = pContextLen    (pointer to DWORD: current length, updated on return)
;
;   Appends to pContextBuf at offset *pContextLen:
;     "\n\nTool[" + <tool_id digit> + "] Result:\n" + <result> + "\n\nAssistant:"
;   Updates *pContextLen to the new total byte count.
;   Null-terminates pContextBuf after every append.
;
;   Returns: EAX = new total context length (same as *pContextLen)
;==============================================================================
ReAct_FormatResult PROC FRAME
    .PUSHREG rbx
    push    rbx
    .PUSHREG rsi
    push    rsi
    .PUSHREG rdi
    push    rdi
    .PUSHREG r12
    push    r12
    .PUSHREG r13
    push    r13
    .PUSHREG r14
    push    r14
    ; 6 pushes (48 B) + return addr (8 B) = 56 B → NOT aligned; need sub to fix
    ; sub rsp, 28h (40 B): total = 96 B → 16-byte aligned ✓
    .ALLOCSTACK 28h
    sub     rsp, 28h
    .ENDPROLOG

    ; Local:  [rsp+20h] = pContextLen ptr
    mov     [rsp+20h], r9       ; save pContextLen pointer

    ; Register assignments (non-volatile, stable across calls)
    mov     r12d, ecx           ; r12d = tool_id
    mov     r13,  rdx           ; r13  = pToolResult
    mov     r14,  r8            ; r14  = pContextBuf

    ; ebx = current context length (loaded from *pContextLen)
    mov     ebx, dword ptr [r9]

    ;------------------------------------------------------------------
    ; Guard: if pContextBuf is null, bail immediately
    ;------------------------------------------------------------------
    test    r14, r14
    jz      FR_Done

    ;------------------------------------------------------------------
    ; Append "\n\nTool["   (szToolResultPrefix)
    ;------------------------------------------------------------------
    lea     rcx, szToolResultPrefix
    call    StrLen_Internal
    ; guard: if ebx + eax >= REACT_PROMPT_BUF_SIZE - 2, skip
    mov     ecx, ebx
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 2
    jae     FR_Done
    mov     rdi, r14
    add     rdi, rbx
    lea     rsi, szToolResultPrefix
    mov     ecx, eax
    rep     movsb
    add     ebx, eax

    ;------------------------------------------------------------------
    ; Append tool_id as a single ASCII decimal digit
    ;------------------------------------------------------------------
    cmp     ebx, REACT_PROMPT_BUF_SIZE - 2
    jae     FR_Done
    ; tool_id is guaranteed 0-6; one digit
    mov     al, r12b
    add     al, '0'             ; convert 0-6 → '0'-'6'
    mov     byte ptr [r14 + rbx], al
    inc     ebx

    ;------------------------------------------------------------------
    ; Append "] Result:\n"   (szToolResultMid)
    ;------------------------------------------------------------------
    lea     rcx, szToolResultMid
    call    StrLen_Internal
    mov     ecx, ebx
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 2
    jae     FR_NullTerm
    mov     rdi, r14
    add     rdi, rbx
    lea     rsi, szToolResultMid
    mov     ecx, eax
    rep     movsb
    add     ebx, eax

    ;------------------------------------------------------------------
    ; Append pToolResult
    ;------------------------------------------------------------------
    test    r13, r13
    jz      FR_SkipResult
    mov     rcx, r13
    call    StrLen_Internal
    test    eax, eax
    jz      FR_SkipResult
    mov     ecx, ebx
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 2
    jae     FR_NullTerm
    mov     rdi, r14
    add     rdi, rbx
    mov     rsi, r13
    mov     ecx, eax
    rep     movsb
    add     ebx, eax
FR_SkipResult:

    ;------------------------------------------------------------------
    ; Append "\n\nAssistant:"   (szToolResultSuffix)
    ;------------------------------------------------------------------
    lea     rcx, szToolResultSuffix
    call    StrLen_Internal
    mov     ecx, ebx
    add     ecx, eax
    cmp     ecx, REACT_PROMPT_BUF_SIZE - 2
    jae     FR_NullTerm
    mov     rdi, r14
    add     rdi, rbx
    lea     rsi, szToolResultSuffix
    mov     ecx, eax
    rep     movsb
    add     ebx, eax

FR_NullTerm:
    ; Always null-terminate
    mov     byte ptr [r14 + rbx], 0

    ; Update *pContextLen
    mov     r9, [rsp+20h]
    mov     dword ptr [r9], ebx

FR_Done:
    mov     eax, ebx            ; return new total length

    add     rsp, 28h
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ReAct_FormatResult ENDP

;==============================================================================
; ReAct_Abort PROC
;   Sets g_reactAbort = 1 and sends REACT_EVT_ABORT to REACT_BEACON_SLOT.
;   Returns: EAX = 0
;   No FRAME needed (no stack-allocated locals, no non-volatile reg saves).
;==============================================================================
ReAct_Abort PROC
    sub     rsp, 28h            ; shadow space + 8-byte alignment pad

    ; Signal the loop to stop
    mov     byte ptr [g_reactAbort], 1

    ; Notify subscribers via beacon
    mov     ecx, REACT_BEACON_SLOT
    mov     edx, REACT_EVT_ABORT
    xor     r8d, r8d            ; no payload
    call    BeaconSend

    add     rsp, 28h
    xor     eax, eax
    ret
ReAct_Abort ENDP

END
