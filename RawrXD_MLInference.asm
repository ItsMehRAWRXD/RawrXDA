;=============================================================================
; RawrXD_MLInference.asm
; Real model inference backend — HTTP to RawrEngine on localhost:23959
; Pure x64 MASM, no CRT, WinHTTP-based token streaming
;
; PUBLIC EXPORTS:
;   - MLInference_Init()
;   - MLInference_Query(prompt, bufferOut, bufferSize)
;   - MLInference_GetTokensProcessed()
;   - MLInference_GetLatency()
;   - MLInference_Close()
;=============================================================================
OPTION CASEMAP:NONE

;-----------------------------------------------------------------------------
; Windows API imports (WinHTTP)
;-----------------------------------------------------------------------------
EXTERN  WinHttpOpen          :PROC
EXTERN  WinHttpConnect       :PROC
EXTERN  WinHttpOpenRequest   :PROC
EXTERN  WinHttpSendRequest   :PROC
EXTERN  WinHttpReceiveResponse :PROC
EXTERN  WinHttpReadData      :PROC
EXTERN  WinHttpCloseHandle   :PROC
EXTERN  GetTickCount         :PROC
EXTERN  OutputDebugStringA   :PROC
EXTERN  wsprintfA            :PROC
EXTERN  lstrcpyA             :PROC
EXTERN  lstrcatA             :PROC
EXTERN  lstrlenA             :PROC

;-----------------------------------------------------------------------------
; Public exports
;-----------------------------------------------------------------------------
PUBLIC MLInference_Init
PUBLIC MLInference_Query
PUBLIC MLInference_GetTokensProcessed
PUBLIC MLInference_GetLatency
PUBLIC MLInference_Close
PUBLIC g_MLInferenceReady
PUBLIC g_TokenCount
PUBLIC g_LatencyMs
PUBLIC g_LastResponseSize

;=============================================================================
; .DATA — ML inference state
;=============================================================================
.data
ALIGN 16

g_MLInferenceReady      QWORD   0       ; 1 = initialized
g_TokenCount            QWORD   0       ; tokens generated
g_LatencyMs             QWORD   0       ; last inference latency (ms)
g_LastResponseSize      QWORD   0       ; last response byte count
g_HTTPSession           QWORD   0       ; WinHttpOpen handle
g_HTTPConnection        QWORD   0       ; WinHttpConnect handle

; RawrEngine endpoint
szUserAgent             DB      "RawrXD-ML/1.0",0
szHost                  DB      "localhost",0
szPath                  DB      "/api/infer",0
usPort                  WORD    23959

; HTTP headers for JSON POST
szContentType           DB      "Content-Type: application/json",13,10,0
szContentLength         DB      "Content-Length: %d",13,10,0

; JSON request template: {"prompt": "...", "n_tokens": 512}
szJSONTemplate          DB      '{"prompt": "',0
szJSONMid               DB      '", "n_tokens": 512}',0

; Response buffer (4 KB for token stream)
ML_RESPONSE_BUFFER_SIZE EQU 4096
g_ResponseBuffer        DB      ML_RESPONSE_BUFFER_SIZE DUP(0)

; Timing
g_InferenceStart        QWORD   0

;=============================================================================
; .CODE
;=============================================================================
.code

;-----------------------------------------------------------------------------
; MLInference_Init() — Initialize WinHTTP session
; Returns: rax = 1 if success, 0 if failed
;-----------------------------------------------------------------------------
MLInference_Init PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    sub rsp, 40
    .ALLOCSTACK 40
    .ENDPROLOG

    ; Check if already initialized
    lea rax, [g_MLInferenceReady]
    cmp qword ptr [rax], 1
    je init_done

    ; WinHttpOpen(szUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0)
    lea rcx, [szUserAgent]
    mov rdx, 1              ; WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    xor r8, r8
    xor r9, r9
    sub rsp, 8
    mov qword ptr [rsp], 0  ; flags
    call WinHttpOpen
    add rsp, 8

    test rax, rax
    jz init_fail

    lea rbx, [g_HTTPSession]
    mov [rbx], rax

    ; WinHttpConnect(hSession, szHost, PORT_23959, 0)
    mov rcx, rax            ; hSession
    lea rdx, [szHost]
    mov r8w, 23959          ; port
    xor r9, r9              ; flags
    call WinHttpConnect

    test rax, rax
    jz init_fail_close_session

    lea rbx, [g_HTTPConnection]
    mov [rbx], rax

    ; Success
    lea rax, [g_MLInferenceReady]
    mov qword ptr [rax], 1

init_done:
    mov rax, 1
    jmp init_exit

init_fail_close_session:
    mov rcx, [g_HTTPSession]
    call WinHttpCloseHandle
    xor rax, rax

init_fail:
    xor rax, rax

init_exit:
    add rsp, 40
    pop rbx
    pop rbp
    ret
ENDP

;-----------------------------------------------------------------------------
; MLInference_Query(rcx=prompt_ptr, rdx=output_buffer, r8=buffer_size)
; Returns: rax = bytes written to output_buffer, 0 if failed
;
; Performs HTTP POST to RawrEngine, captures JSON response as plain text
;-----------------------------------------------------------------------------
MLInference_Query PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG

    ; rcx = prompt_ptr, rdx = output_buffer, r8 = buffer_size
    mov rsi, rcx            ; rsi = prompt_ptr
    mov rdi, rdx            ; rdi = output_buffer
    mov r10, r8             ; r10 = buffer_size

    ; Check if initialized
    cmp qword ptr [g_MLInferenceReady], 1
    jne query_fail

    ; Record start time
    call GetTickCount
    lea rbx, [g_InferenceStart]
    mov [rbx], rax

    ; Build JSON request: {"prompt": "<prompt>", "n_tokens": 512}
    ; Store at [rsp+0] with max 1024 bytes
    lea rcx, [g_ResponseBuffer + ML_RESPONSE_BUFFER_SIZE]  ; temp build area
    mov byte ptr [rcx], 0

    ; Copy template start: {"prompt": "
    lea rax, [szJSONTemplate]
    call lstrcpyA            ; doesn't use params correctly in asm, manual copy

    ; Manually build JSON
    mov rax, rcx
    lea rdx, [szJSONTemplate]
    xor r10, r10
copy_start_loop:
    mov bl, byte ptr [rdx + r10]
    test bl, bl
    jz copy_start_done
    mov byte ptr [rax + r10], bl
    inc r10
    jmp copy_start_loop

copy_start_done:
    ; Append user prompt
    mov r11, r10
copy_prompt_loop:
    mov bl, byte ptr [rsi + r11 - r10]
    test bl, bl
    jz copy_prompt_done
    mov byte ptr [rcx + r11], bl
    inc r11
    jmp copy_prompt_loop

copy_prompt_done:
    ; Append JSON end
    lea rax, [szJSONMid]
    xor r10, r10
copy_end_loop:
    mov bl, byte ptr [rax + r10]
    mov byte ptr [rcx + r11 + r10], bl
    test bl, bl
    jz copy_end_done
    inc r10
    jmp copy_end_loop

copy_end_done:
    ; rcx now contains full JSON request
    ; Post it via WinHttpOpenRequest
    mov r9, [g_HTTPConnection]
    mov r8, 3               ; WINHTTP_VERB_POST
    xor rdx, rdx
    lea r10, [szPath]

    ; WinHttpOpenRequest(hConnect, "POST", "/api/infer", NULL, NULL, NULL, flags)
    ; For simplicity: use WINHTTP_FLAG_BYPASS_PROXY_CACHE
    mov rcx, r9
    mov rdx, r8
    lea r8, [szPath]
    xor r9, r9              ; accept types
    xor r10, r10            ; referrer
    xor r11, r11            ; extra headers
    sub rsp, 8
    mov r12, 0x4000000     ; WINHTTP_FLAG_BYPASS_PROXY_CACHE
    mov qword ptr [rsp], r12
    call WinHttpOpenRequest
    add rsp, 8

    test rax, rax
    jz query_fail

    ; Store request handle
    mov r12, rax            ; r12 = request handle

    ; WinHttpSendRequest(hRequest, NULL, 0, data, data_len, data_len, 0)
    mov rcx, rax
    xor rdx, rdx
    xor r8, r8
    mov r9, rcx             ; JSON data (rcx still points to data)
    sub rsp, 16
    mov qword ptr [rsp], rax        ; data_len
    mov qword ptr [rsp + 8], 0      ; reserved
    call WinHttpSendRequest
    add rsp, 16

    ; WinHttpReceiveResponse
    mov rcx, r12
    xor rdx, rdx
    call WinHttpReceiveResponse

    ; WinHttpReadData(hRequest, buffer, size, bytes_read)
    mov rcx, r12
    lea rdx, [g_ResponseBuffer]
    mov r8, ML_RESPONSE_BUFFER_SIZE
    lea r9, [g_ResponseBuffer + ML_RESPONSE_BUFFER_SIZE]
    call WinHttpReadData

    ; rax = bytes read
    mov r11, rax

    ; Copy to output buffer
    mov rcx, rdi            ; rdx = r9 (copy to rdi)
    lea rsi, [g_ResponseBuffer]
    xor r10, r10
copy_response_loop:
    cmp r10, r11
    jge copy_response_done
    cmp r10, r8             ; don't overflow
    jge copy_response_done
    mov bl, byte ptr [rsi + r10]
    mov byte ptr [rcx + r10], bl
    inc r10
    jmp copy_response_loop

copy_response_done:
    ; Record response size and latency
    lea rax, [g_LastResponseSize]
    mov [rax], r11

    call GetTickCode
    sub rax, [g_InferenceStart]
    lea rbx, [g_LatencyMs]
    mov [rbx], rax

    ; Increment token count (estimate from response size)
    mov rax, r11
    shr rax, 3              ; divide by 8 (rough token estimate)
    lea rbx, [g_TokenCount]
    add [rbx], rax

    ; Close handles
    mov rcx, r12
    call WinHttpCloseHandle

    mov rax, r11            ; return bytes read
    jmp query_exit

query_fail:
    xor rax, rax

query_exit:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
ENDP

;-----------------------------------------------------------------------------
; MLInference_GetTokensProcessed() — Return g_TokenCount
;-----------------------------------------------------------------------------
MLInference_GetTokensProcessed PROC
    lea rax, [g_TokenCount]
    mov rax, [rax]
    ret
ENDP

;-----------------------------------------------------------------------------
; MLInference_GetLatency() — Return g_LatencyMs
;-----------------------------------------------------------------------------
MLInference_GetLatency PROC
    lea rax, [g_LatencyMs]
    mov rax, [rax]
    ret
ENDP

;-----------------------------------------------------------------------------
; MLInference_Close() — Cleanup HTTP handles
;-----------------------------------------------------------------------------
MLInference_Close PROC
    ; Close connection
    mov rcx, [g_HTTPConnection]
    test rcx, rcx
    jz close_session
    call WinHttpCloseHandle

close_session:
    ; Close session
    mov rcx, [g_HTTPSession]
    test rcx, rcx
    jz close_exit
    call WinHttpCloseHandle

close_exit:
    xor rax, rax
    ret
ENDP

END
