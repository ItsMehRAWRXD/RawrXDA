; ═══════════════════════════════════════════════════════════════════
; bridge.asm — Ghost-text bridge: InferenceRouter ↔ UI pipeline
;
; Connects the UI's ghost-text overlay with sovereign inference.
; Bridge_RequestSuggestion fires an async generate via the router,
; which selects LOCAL (RunInference) or OLLAMA (HTTP) backend.
; Result delivered via Bridge_OnSuggestionReady (in ui.asm).
;
; Architecture:
;   1. UI types text → idle timer fires → calls Bridge_RequestSuggestion
;   2. Bridge extracts context around cursor, builds prompt
;   3. InferenceRouter_Generate dispatches to best backend
;   4. On completion, worker calls Bridge_OnSuggestionReady (in ui.asm)
;   5. UI paints ghost text on next WM_PAINT
;   6. Tab → Bridge_SubmitCompletion (insert text)
;   7. Esc → Bridge_ClearSuggestion (dismiss)
;   8. New keystroke → Bridge_AbortInference (cancel pending)
; ═══════════════════════════════════════════════════════════════════

PUBLIC Bridge_SubmitCompletion
PUBLIC Bridge_GetSuggestionText
PUBLIC Bridge_ClearSuggestion
PUBLIC Bridge_RequestSuggestion
PUBLIC Bridge_AbortInference

; ── Win32 imports ────────────────────────────────────────────────
EXTERN CreateThread:PROC
EXTERN CloseHandle:PROC
EXTERN WideCharToMultiByte:PROC
EXTERN MultiByteToWideChar:PROC
EXTERN PostMessageW:PROC
EXTERN Sleep:PROC

; ── Inference Router (replaces direct Ollama) ────────────────────
EXTERN InferenceRouter_Generate:PROC
EXTERN InferenceRouter_Abort:PROC
EXTERN InferenceRouter_Init:PROC
EXTERN g_routerReady:DWORD
EXTERN g_routerAbort:DWORD

; ── Agentic ReAct loop + tool registry ───────────────────────────
EXTERN Tool_Init:PROC
EXTERN ReAct_Init:PROC
EXTERN ReAct_Run:PROC
EXTERN RTP_AgentLoop_Run:PROC
EXTERN RTP_BuildContextBlob:PROC
EXTERN RTP_StreamParser_Reset:PROC
EXTERN RTP_StreamParser_PushByte:PROC
EXTERN RTP_StreamParser_GetPacket:PROC
EXTERN RTP_DispatchPacket:PROC
EXTERN RTP_EncodeToolResultFrame:PROC

; ── UI callback (defined in ui.asm) ──────────────────────────────
EXTERN Bridge_OnSuggestionReady:PROC

; ── UI globals (defined in ui.asm) ───────────────────────────────
EXTERN g_ghostActive:BYTE
EXTERN g_ghostTextBuffer:WORD
EXTERN g_ghostTextLen:DWORD
EXTERN g_ghostLineCount:DWORD
EXTERN g_inferencePending:BYTE
EXTERN hMainWnd:QWORD
EXTERN g_rlhfAcceptCount:DWORD
EXTERN g_rlhfRejectCount:DWORD

; ── Beacon ───────────────────────────────────────────────────────

; ── Forward declarations ─────────────────────────────────────────
Bridge_WorkerThread PROTO
EXTERN BeaconSend:PROC

; ── Constants ────────────────────────────────────────────────────
BRIDGE_BEACON_SLOT      equ 14
BRIDGE_EVT_REQUEST      equ 0B1h
BRIDGE_EVT_COMPLETE     equ 0B2h
BRIDGE_EVT_CLEAR        equ 0B3h
BRIDGE_EVT_ACCEPT       equ 0B4h
BRIDGE_EVT_ROUTER_FAIL  equ 0B5h

MAX_PROMPT_BYTES        equ 4096     ; UTF-8 prompt buffer
MAX_RESPONSE_BYTES      equ 2048     ; UTF-8 response buffer
MAX_CONTEXT_CHARS       equ 512      ; WCHAR context window
MAX_RTP_PACKET_BYTES    equ 8192
MAX_RTP_RESULT_BYTES    equ 8192

; WM_USER messages for thread→UI marshalling
WM_GHOST_TEXT_READY     equ 0500h    ; wParam=len, lParam=ptr to WCHAR buf

.data
align 8
; Current suggestion state
g_suggestionBuf     dw  1024 dup(0)  ; Wide-char suggestion text
g_suggestionLen     dd  0            ; Length in WCHARs
g_suggestionActive  dd  0            ; 1 = suggestion is showing
g_bridgeReady       dd  0            ; 1 = bridge initialized
szReActSystem       db "RawrXD sovereign coding agent. If a tool is required emit tool JSON and continue until final completion text.",0

; Request context (filled by RequestSuggestion, consumed by worker)
g_reqRow            dd  0
g_reqCol            dd  0
g_reqTextPtr        dq  0            ; Pointer to UI text buffer
g_reqPending        dd  0            ; 1 = worker is running

.data?
align 16
; UTF-8 conversion buffers (not in .data to avoid bloating .exe)
g_utf8Prompt        db MAX_PROMPT_BYTES dup(?)
g_utf8Response      db MAX_RESPONSE_BYTES dup(?)
g_wideResponse      dw MAX_RESPONSE_BYTES dup(?)  ; UTF-8→WCHAR result
g_contextBuf        dw MAX_CONTEXT_CHARS dup(?)    ; WCHAR context window
g_rtpPacketBuf      db MAX_RTP_PACKET_BYTES dup(?)
g_rtpResultFrame    db MAX_RTP_RESULT_BYTES dup(?)
g_rtpContextShadow  db 4096 dup(?)

.code

; ════════════════════════════════════════════════════════════════════
; Bridge_SubmitCompletion — Accept or reject the current suggestion
;   ECX = 1 → accepted (Tab), 0 → rejected (Esc)
;   Returns: EAX = 0
;
;   When accepted, the actual text insertion is handled by ui.asm's
;   ghost-accept code (@char_tab). This just clears bridge state
;   and signals via beacon.
; ════════════════════════════════════════════════════════════════════
Bridge_SubmitCompletion PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     ebx, ecx                    ; save accepted flag

    ; ── RLHF counter update ──
    test    ebx, ebx
    jz      @bsc_rlhf_reject
    lock inc dword ptr [g_rlhfAcceptCount]
    jmp     @bsc_rlhf_done
@bsc_rlhf_reject:
    lock inc dword ptr [g_rlhfRejectCount]
@bsc_rlhf_done:

    ; Clear bridge suggestion state
    mov     g_suggestionActive, 0
    mov     g_suggestionLen, 0

    ; Beacon: report accept/reject
    mov     ecx, BRIDGE_BEACON_SLOT
    test    ebx, ebx
    jz      @bsc_reject
    mov     edx, BRIDGE_EVT_ACCEPT
    jmp     @bsc_send
@bsc_reject:
    mov     edx, BRIDGE_EVT_CLEAR
@bsc_send:
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    add     rsp, 20h
    pop     rbx
    ret
Bridge_SubmitCompletion ENDP


; ════════════════════════════════════════════════════════════════════
; Bridge_GetSuggestionText — Return pointer to current suggestion
;   Returns: RAX = pointer to WCHAR suggestion text
;            EDX = length in WCHARs
; ════════════════════════════════════════════════════════════════════
Bridge_GetSuggestionText PROC
    lea     rax, g_suggestionBuf
    mov     edx, g_suggestionLen
    ret
Bridge_GetSuggestionText ENDP


; ════════════════════════════════════════════════════════════════════
; Bridge_ClearSuggestion — Dismiss the active ghost-text overlay
;   No args. Returns: EAX = 0
; ════════════════════════════════════════════════════════════════════
Bridge_ClearSuggestion PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Clear suggestion
    mov     g_suggestionActive, 0
    mov     g_suggestionLen, 0

    ; Beacon
    mov     ecx, BRIDGE_BEACON_SLOT
    mov     edx, BRIDGE_EVT_CLEAR
    xor     r8d, r8d
    call    BeaconSend

    xor     eax, eax
    add     rsp, 28h
    ret
Bridge_ClearSuggestion ENDP


; ════════════════════════════════════════════════════════════════════
; Bridge_RequestSuggestion — Fire async completion request
;   RCX = pTextBuf   (WCHAR* to full editor text buffer)
;   EDX = cursorRow
;   R8D = cursorCol
;   Returns: EAX = 0 (request queued) or -1 (busy/unavailable)
;
;   Extracts ~512 chars of context around cursor, converts to UTF-8,
;   spawns a worker thread that calls OllamaClient_Generate2, then
;   posts the result back to UI thread via Bridge_OnSuggestionReady.
; ════════════════════════════════════════════════════════════════════
Bridge_RequestSuggestion PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Lazy init of tool registry + ReAct runtime
    cmp     g_bridgeReady, 1
    je      @brs_ready
    call    Tool_Init
    call    ReAct_Init
    test    eax, eax
    jnz     @brs_busy
    mov     g_bridgeReady, 1

@brs_ready:

    ; Bail if already pending
    cmp     g_reqPending, 1
    je      @brs_busy

    ; Ensure router is initialized (lazy init with retry on first completion request)
    cmp     g_routerReady, 0
    jne     @brs_router_ready

    ; Retry router init up to 3 times with 100ms backoff
    xor     ebx, ebx                     ; ebx = retry counter
@brs_router_retry:
    cmp     ebx, 3
    jge     @brs_router_failed
    call    InferenceRouter_Init
    cmp     g_routerReady, 0
    jne     @brs_router_ready
    ; Backoff: Sleep(100 * (retry + 1))
    push    rbx
    lea     ecx, [ebx + 1]
    imul    ecx, 100                     ; 100ms, 200ms, 300ms
    sub     rsp, 20h
    call    Sleep
    add     rsp, 20h
    pop     rbx
    inc     ebx
    jmp     @brs_router_retry

@brs_router_failed:
    ; Router failed after 3 retries — send status beacon so UI can show warning
    mov     ecx, BRIDGE_BEACON_SLOT
    mov     edx, BRIDGE_EVT_ROUTER_FAIL
    mov     r8d, 3                       ; retry count
    call    BeaconSend
    jmp     @brs_busy

@brs_router_ready:

    ; Abort any previous inference still draining
    mov     g_routerAbort, 0

    ; Save request parameters
    mov     g_reqTextPtr, rcx
    mov     g_reqRow, edx
    mov     g_reqCol, r8d

    ; Mark pending + inference pending in UI
    mov     g_reqPending, 1
    mov     byte ptr [g_inferencePending], 1

    ; Beacon
    mov     ecx, BRIDGE_BEACON_SLOT
    mov     edx, BRIDGE_EVT_REQUEST
    xor     r8d, r8d
    call    BeaconSend

    ; Spawn worker thread
    xor     ecx, ecx                     ; lpThreadAttributes = NULL
    xor     edx, edx                     ; dwStackSize = default
    lea     r8, Bridge_WorkerThread      ; lpStartAddress
    xor     r9d, r9d                     ; lpParameter = NULL
    mov     dword ptr [rsp+20h], 0       ; dwCreationFlags = 0
    mov     qword ptr [rsp+28h], 0       ; lpThreadId = NULL
    call    CreateThread
    test    rax, rax
    jz      @brs_thread_fail

    ; Close thread handle (fire-and-forget)
    mov     rcx, rax
    call    CloseHandle

    xor     eax, eax                     ; 0 = queued
    jmp     @brs_ret

@brs_thread_fail:
    mov     g_reqPending, 0
    mov     byte ptr [g_inferencePending], 0

@brs_busy:
    mov     eax, -1

@brs_ret:
    add     rsp, 30h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Bridge_RequestSuggestion ENDP


; ════════════════════════════════════════════════════════════════════
; Bridge_WorkerThread — Background thread: context→prompt→infer
;   Called by CreateThread. RCX = lpParameter (unused).
;   Reads g_reqTextPtr/Row/Col, builds UTF-8 prompt, calls
;   InferenceRouter_Generate (LOCAL or OLLAMA backend),
;   converts response to WCHAR, calls Bridge_OnSuggestionReady.
;   FRAME: 2 pushes + 40h alloc
; ════════════════════════════════════════════════════════════════════
Bridge_WorkerThread PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 50h
    .allocstack 50h
    .endprolog

    ; ── Step 1: Extract context window ──
    mov     rsi, g_reqTextPtr            ; WCHAR* to full text
    test    rsi, rsi
    jz      @bwt_done

    ; Calculate rough char offset: walk lines to find cursor position
    ; For simplicity: scan from start, count newlines until we reach g_reqRow
    xor     ecx, ecx                     ; line counter
    xor     edx, edx                     ; char offset
@bwt_find_row:
    cmp     ecx, g_reqRow
    jge     @bwt_found_row
    cmp     word ptr [rsi + rdx*2], 0
    je      @bwt_found_row               ; end of text
    cmp     word ptr [rsi + rdx*2], 0Ah
    jne     @bwt_fr_next
    inc     ecx
@bwt_fr_next:
    inc     edx
    jmp     @bwt_find_row

@bwt_found_row:
    ; edx = char offset of start of cursor line
    add     edx, g_reqCol                ; edx = total char offset at cursor

    ; Context window: take last MAX_CONTEXT_CHARS chars before cursor
    mov     ebx, edx                     ; ebx = cursor offset
    mov     eax, edx
    sub     eax, MAX_CONTEXT_CHARS
    jns     @bwt_ctx_start
    xor     eax, eax                     ; clamp to start
@bwt_ctx_start:
    ; eax = start offset, ebx = end offset (cursor pos)
    mov     ecx, ebx
    sub     ecx, eax                     ; ecx = context length in WCHARs
    test    ecx, ecx
    jz      @bwt_done

    ; ── Step 2: Convert context WCHAR → UTF-8 ─────────────────────
    ; WideCharToMultiByte(CP_UTF8, 0, pWide, wideLen, pMB, mbLen, NULL, NULL)
    mov     dword ptr [rsp+38h], 0       ; lpUsedDefaultChar = NULL
    mov     qword ptr [rsp+30h], 0       ; lpDefaultChar = NULL
    mov     dword ptr [rsp+28h], MAX_PROMPT_BYTES ; cbMultiByte
    mov     dword ptr [rsp+20h], 0       ; placeholder (5th param)
    mov     dword ptr [rsp+40h], ecx     ; save wideLen in safe stack slot
    lea     r9, g_utf8Prompt             ; lpMultiByteStr (4th param)
    mov     r8d, ecx                     ; cchWideChar (3rd param)
    lea     rdx, [rsi + rax*2]           ; lpWideCharStr (2nd param, offset by start)
    mov     ecx, 65001                   ; CP_UTF8 (1st param)
    ; Fixup: p5=mbLen, p6=NULL, p7=NULL
    ; WideCharToMultiByte has 8 params on x64:
    ;   rcx=CodePage, rdx=dwFlags, r8=lpWide, r9=cchWide,
    ;   [rsp+20h]=lpMB, [rsp+28h]=cbMB, [rsp+30h]=lpDefault, [rsp+38h]=lpUsed
    ; Re-arrange:
    mov     dword ptr [rsp+20h], 0       ; will fix below
    ; Actually the signature is:
    ;   WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar,
    ;                       lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar)
    ; So: rcx=65001, edx=0, r8=pWide, r9d=wideLen,
    ;     [rsp+20h]=pMB, [rsp+28h]=mbLen, [rsp+30h]=NULL, [rsp+38h]=NULL
    mov     r9d, r8d                     ; cchWideChar → r9
    lea     r8, [rsi + rax*2]            ; lpWideCharStr → r8
    xor     edx, edx                     ; dwFlags = 0
    lea     rax, g_utf8Prompt
    mov     [rsp+20h], rax               ; lpMultiByteStr
    mov     dword ptr [rsp+28h], MAX_PROMPT_BYTES ; cbMultiByte
    mov     qword ptr [rsp+30h], 0       ; lpDefaultChar = NULL
    mov     qword ptr [rsp+38h], 0       ; lpUsedDefaultChar = NULL
    call    WideCharToMultiByte
    test    eax, eax
    jz      @bwt_done
    mov     ebx, eax                     ; ebx = UTF-8 byte count

    ; Null-terminate UTF-8 prompt
    lea     rsi, g_utf8Prompt
    mov     byte ptr [rsi + rbx], 0

    ; ── Step 2.5: Build binary RTP context blob for model interface ─────────
    lea     rcx, g_rtpContextShadow
    mov     edx, 4096
    lea     r8, [rsp+4Ch]
    call    RTP_BuildContextBlob

    ; ── Step 3: RTP Agent Loop (plan→execute→verify), then ReAct fallback ──
    ; RTP_AgentLoop_Run(userPrompt, outBuf, outCap, maxIters)
    lea     rcx, g_utf8Prompt
    lea     rdx, g_utf8Response
    mov     r8d, MAX_RESPONSE_BYTES - 1
    mov     r9d, 4
    call    RTP_AgentLoop_Run
    cmp     eax, 0
    jl      @bwt_react_path

    ; Agent loop success: compute response byte length
    lea     rsi, g_utf8Response
    xor     ebx, ebx
@bwt_agent_strlen:
    cmp     byte ptr [rsi + rbx], 0
    je      @bwt_have_response
    inc     ebx
    cmp     ebx, MAX_RESPONSE_BYTES - 1
    jb      @bwt_agent_strlen
    mov     byte ptr [rsi + MAX_RESPONSE_BYTES - 1], 0
    mov     ebx, MAX_RESPONSE_BYTES - 1
    jmp     @bwt_have_response

@bwt_react_path:
    ; ── ReAct loop (tool-calling), fallback to direct infer ──
    ; ReAct_Run(system, userPrompt, outBuf, outBufSize)
    lea     rcx, szReActSystem
    lea     rdx, g_utf8Prompt
    lea     r8, g_utf8Response
    mov     r9d, MAX_RESPONSE_BYTES - 1
    call    ReAct_Run
    cmp     eax, 0
    jne     @bwt_fallback_infer

    ; ReAct success: compute response byte length
    lea     rsi, g_utf8Response
    xor     ebx, ebx
@bwt_react_strlen:
    cmp     byte ptr [rsi + rbx], 0
    je      @bwt_have_response
    inc     ebx
    cmp     ebx, MAX_RESPONSE_BYTES - 1
    jb      @bwt_react_strlen
    mov     byte ptr [rsi + MAX_RESPONSE_BYTES - 1], 0
    mov     ebx, MAX_RESPONSE_BYTES - 1
    jmp     @bwt_have_response

@bwt_fallback_infer:
    ; Fallback path: direct router completion
    lea     rcx, g_utf8Prompt
    lea     rdx, g_utf8Response
    mov     r8d, MAX_RESPONSE_BYTES - 1
    call    InferenceRouter_Generate
    cmp     eax, -1
    je      @bwt_infer_fail
    test    eax, eax
    jz      @bwt_infer_fail
    mov     ebx, eax                     ; ebx = response byte count
    jmp     @bwt_have_response

@bwt_infer_fail:
    ; Both ReAct and direct inference failed — send diagnostic beacon
    mov     ecx, BRIDGE_BEACON_SLOT
    mov     edx, BRIDGE_EVT_ROUTER_FAIL
    mov     r8d, 0FFh                    ; 0xFF = inference fail (not router init)
    call    BeaconSend
    jmp     @bwt_done

@bwt_have_response:

    ; Null-terminate response
    lea     rsi, g_utf8Response
    mov     byte ptr [rsi + rbx], 0

    ; ── Step 3.5: RTP binary tool-call path (stream parse + dispatch) ────
    ; If model emitted RTP packet bytes ("RTP!"), dispatch through RTP core
    ; and replace response buffer with dispatched tool result text.
    call    RTP_StreamParser_Reset
    lea     rsi, g_utf8Response
    xor     edi, edi                     ; index into response bytes
@bwt_rtp_scan_loop:
    cmp     edi, ebx
    jae     @bwt_after_rtp_scan
    movzx   ecx, byte ptr [rsi + rdi]
    call    RTP_StreamParser_PushByte
    cmp     eax, 1
    je      @bwt_rtp_packet_ready
    inc     edi
    jmp     @bwt_rtp_scan_loop

@bwt_rtp_packet_ready:
    ; Extract packet from parser into bridge-local packet buffer.
    lea     rcx, g_rtpPacketBuf
    mov     edx, MAX_RTP_PACKET_BYTES
    lea     r8, [rsp+40h]                ; out packet bytes
    call    RTP_StreamParser_GetPacket
    cmp     eax, 0
    jne     @bwt_after_rtp_scan

    ; Dispatch RTP packet to legacy tool execution path.
    lea     rcx, g_rtpPacketBuf
    mov     edx, dword ptr [rsp+40h]
    lea     r8,  g_utf8Response          ; result text -> response buffer
    mov     r9d, MAX_RESPONSE_BYTES - 1
    call    RTP_DispatchPacket
    mov     dword ptr [rsp+44h], eax     ; save dispatch status
    test    eax, eax
    jne     @bwt_after_rtp_scan

    ; Compute new response length after dispatch.
    lea     rsi, g_utf8Response
    xor     ebx, ebx
@bwt_rtp_strlen:
    cmp     byte ptr [rsi + rbx], 0
    je      @bwt_rtp_encode_result
    inc     ebx
    cmp     ebx, MAX_RESPONSE_BYTES - 1
    jb      @bwt_rtp_strlen
    mov     byte ptr [rsi + MAX_RESPONSE_BYTES - 1], 0
    mov     ebx, MAX_RESPONSE_BYTES - 1

@bwt_rtp_encode_result:
    ; Encode tool result frame (for model feedback/replay pipeline).
    ; call_id uses request row/col packed into 64-bit for now.
    mov     ecx, g_reqRow
    shl     rcx, 32
    mov     eax, g_reqCol
    mov     eax, eax
    or      rcx, rax
    mov     edx, dword ptr [rsp+44h]     ; status_code
    lea     r8, g_utf8Response           ; payload ptr
    mov     r9d, ebx                     ; payload bytes
    lea     rax, g_rtpResultFrame
    mov     [rsp+20h], rax               ; out_buf
    mov     dword ptr [rsp+28h], MAX_RTP_RESULT_BYTES ; out_cap
    lea     rax, [rsp+48h]
    mov     [rsp+30h], rax               ; out_written
    call    RTP_EncodeToolResultFrame
    ; Best-effort encode only; ignore failures in UI path.

@bwt_after_rtp_scan:

    ; ── Step 4: Convert response UTF-8 → WCHAR ────────────────────
    ; MultiByteToWideChar(CP_UTF8, 0, pMB, mbLen, pWide, wideLen)
    mov     ecx, 65001                   ; CP_UTF8
    xor     edx, edx                     ; dwFlags = 0
    lea     r8, g_utf8Response           ; lpMultiByteStr
    mov     r9d, ebx                     ; cbMultiByte
    lea     rax, g_wideResponse
    mov     [rsp+20h], rax               ; lpWideCharStr (5th param)
    mov     dword ptr [rsp+28h], MAX_RESPONSE_BYTES ; cchWideChar (6th param)
    call    MultiByteToWideChar
    test    eax, eax
    jz      @bwt_done

    ; ── Step 5: Store in suggestion buffer ─────────────────────────
    mov     ebx, eax                     ; ebx = WCHAR count
    mov     g_suggestionLen, eax
    mov     g_suggestionActive, 1

    lea     rsi, g_wideResponse
    lea     rdi, g_suggestionBuf
    mov     ecx, ebx
    rep     movsw

    ; ── Step 6: Deliver to UI via Bridge_OnSuggestionReady ─────────
    lea     rcx, g_suggestionBuf
    mov     edx, ebx
    call    Bridge_OnSuggestionReady

@bwt_done:
    ; Clear pending flag
    mov     g_reqPending, 0
    mov     byte ptr [g_inferencePending], 0
    mov     g_routerAbort, 0

    xor     eax, eax                     ; Thread exit code
    add     rsp, 50h
    pop     rsi
    pop     rbx
    ret
Bridge_WorkerThread ENDP


; ════════════════════════════════════════════════════════════════════
; Bridge_AbortInference — Cancel any pending inference from UI thread
;   Called when user types a new character while inference is pending.
;   Sets abort flags so the worker thread + router bail out early.
;   No args. Returns: nothing.
; ════════════════════════════════════════════════════════════════════
Bridge_AbortInference PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Signal abort to the inference router
    call    InferenceRouter_Abort

    ; Clear ghost text immediately
    mov     byte ptr [g_ghostActive], 0
    mov     g_suggestionLen, 0
    mov     g_suggestionActive, 0

    add     rsp, 28h
    ret
Bridge_AbortInference ENDP

END
