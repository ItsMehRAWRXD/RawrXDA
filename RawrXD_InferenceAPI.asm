;=============================================================================
; RawrXD_InferenceAPI.asm — HTTP Bridge to RawrEngine/Ollama (Hybrid: real + sim fallback)
; Pure x64 MASM, zero CRT dependencies
; Exports: RawrXD_Tokenizer_Init, RawrXD_Inference_Init, RawrXD_Inference_Generate
; On Init: tries WinHTTP connect with 5s timeout; on failure returns 1 but sets g_hHttpConnect=0.
; On Generate: if g_hHttpConnect==0, emits simulation tokens; else streams from Ollama.
; Telemetry: [MODE] real | [MODE] simulation (Ollama unavailable)
;=============================================================================
OPTION CASEMAP:NONE

;-----------------------------------------------------------------------------
; WinHTTP Imports
;-----------------------------------------------------------------------------
EXTERN WinHttpOpen:PROC
EXTERN WinHttpConnect:PROC
EXTERN WinHttpOpenRequest:PROC
EXTERN WinHttpAddRequestHeaders:PROC
EXTERN WinHttpSendRequest:PROC
EXTERN WinHttpReceiveResponse:PROC
EXTERN WinHttpQueryDataAvailable:PROC
EXTERN WinHttpReadData:PROC
EXTERN WinHttpCloseHandle:PROC
EXTERN WinHttpSetOption:PROC

;-----------------------------------------------------------------------------
; Win32 Imports
;-----------------------------------------------------------------------------
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN GetProcessHeap:PROC
EXTERN GetTickCount:PROC
EXTERN OutputDebugStringA:PROC

;-----------------------------------------------------------------------------
; Constants
;-----------------------------------------------------------------------------
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY     EQU 0
WINHTTP_FLAG_ESCAPE_PERCENT           EQU 00000004h
WINHTTP_FLAG_BYPASS_PROXY_CACHE       EQU 04000000h
WINHTTP_OPTION_CONNECT_TIMEOUT        EQU 3
CONNECT_TIMEOUT_MS                    EQU 5000
GENERIC_WRITE                         EQU 40000000h

HTTP_STATUS_OK                        EQU 200

; Offsets in HTTP response JSON: {"response":"TOKEN","done":false}
JSON_RESPONSE_PREFIX                  EQU 14  ; len of: {"response":"

;-----------------------------------------------------------------------------
; Public Exports
;-----------------------------------------------------------------------------
PUBLIC RawrXD_Tokenizer_Init
PUBLIC RawrXD_Tokenizer_Encode
PUBLIC RawrXD_Tokenizer_Decode
PUBLIC RawrXD_Inference_Init
PUBLIC RawrXD_Inference_Generate
PUBLIC RawrXD_Inference_GetMode
PUBLIC g_hHttpSession
PUBLIC g_hHttpConnect
PUBLIC g_InferenceModeSim

;=============================================================================
; .DATA — Global state (16-byte aligned)
;=============================================================================
.data
ALIGN 16

g_hHttpSession     QWORD 0
g_hHttpConnect     QWORD 0
g_hRequest         QWORD 0
g_PromptBuffer     QWORD 0
g_TokenCount       QWORD 0
g_InferenceModeSim DWORD 0       ; 0=real (Ollama), 1=simulation fallback
g_ConnectTimeoutMs DWORD CONNECT_TIMEOUT_MS

; HTTP Configuration
szUserAgent        DB "RawrXD-Inference/1.0",0
szHost             DB "127.0.0.1",0
szEndpoint         DB "/api/generate",0
szPost             DB "POST",0
szContentType      DB "Content-Type: application/json",13,10,0

; JSON Request Template: {"model":"phi-3-mini","prompt":"USER_PROMPT_HERE","stream":true,"num_predict":512}
szJsonPrefix       DB '{"model":"phi-3-mini","prompt":"',0
szJsonSuffix       DB '","stream":true,"num_predict":512}',0

; Response parsing strings
szResponseField    DB '"response":"',0          ; Find this in JSON stream
szDoneField        DB '"done":',0              ; Check for end marker

; Debug / telemetry messages
szInitOk           DB "[INFERENCE] Connected to RawrEngine on 127.0.0.1:11434",13,10,0
szModeReal         DB "[MODE] real (Ollama)",13,10,0
szModeSim          DB "[MODE] simulation (Ollama unavailable)",13,10,0
szInitFail         DB "[INFERENCE] Failed to connect to RawrEngine",13,10,0
szGenStart         DB "[INFERENCE] Generating tokens...",13,10,0
szSimPrefix        DB "[sim] ",0
szSimFallback      DB "Ollama unreachable. Running in simulation mode.",13,10,0

;=============================================================================
; .CODE
;=============================================================================
.code

;-----------------------------------------------------------------------------
; RawrXD_Tokenizer_Init — No-op for HTTP API (server tokenizes)
; RCX = vocab path (ignored)
; Returns: RAX = 1 (success)
;-----------------------------------------------------------------------------
RawrXD_Tokenizer_Init PROC
    mov rax, 1
    ret
RawrXD_Tokenizer_Init ENDP

;-----------------------------------------------------------------------------
; RawrXD_Tokenizer_Encode — Stub for ChatService (server tokenizes; encode no-op)
; RCX = hTokenizer, RDX = text, R8 = text_len, R9 = out_tokens (ptr to write count + ids)
; Writes dummy token count so pipeline continues; returns token count.
;-----------------------------------------------------------------------------
RawrXD_Tokenizer_Encode PROC
    test r9, r9
    jz enc_out
    mov qword ptr [r9], 1
    mov qword ptr [r9+8], 0
    mov qword ptr [r9+16], 0
enc_out:
    mov eax, 1
    ret
RawrXD_Tokenizer_Encode ENDP

;-----------------------------------------------------------------------------
; RawrXD_Tokenizer_Decode — Stub for StreamRenderer (no-op; server stream is already text)
; RCX = token_ids, RDX = count, R8 = output_buffer
; Returns: 0 (no bytes decoded)
;-----------------------------------------------------------------------------
RawrXD_Tokenizer_Decode PROC
    xor eax, eax
    ret
RawrXD_Tokenizer_Decode ENDP

;-----------------------------------------------------------------------------
; RawrXD_Inference_GetMode — Returns 0 = real (Ollama), 1 = simulation
;-----------------------------------------------------------------------------
RawrXD_Inference_GetMode PROC
    mov eax, dword ptr [g_InferenceModeSim]
    ret
RawrXD_Inference_GetMode ENDP

;-----------------------------------------------------------------------------
; RawrXD_Inference_Init — Initialize WinHTTP session to RawrEngine (hybrid: real or sim)
; RCX = model path (ignored), RDX = tokenizer handle (ignored)
; Sets connect timeout 5s. On connect failure: g_hHttpConnect=0, g_InferenceModeSim=1, still returns 1.
; Returns: RAX = 1 always (caller can always call Generate; Generate uses sim if connect failed).
;-----------------------------------------------------------------------------
RawrXD_Inference_Init PROC FRAME
    push rbp
    .PUSHREG rbp
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG

    ; Ensure sim flag and connect handle cleared
    xor eax, eax
    mov [g_InferenceModeSim], eax
    mov [g_hHttpConnect], rax

    ; WinHttpOpen(szUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0)
    lea rcx, [szUserAgent]
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call WinHttpOpen

    test rax, rax
    jz init_fail_graceful

    mov [g_hHttpSession], rax

    ; WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, 4)
    mov r9d, 4
    lea r8, [g_ConnectTimeoutMs]
    mov edx, WINHTTP_OPTION_CONNECT_TIMEOUT
    mov rcx, rax
    call WinHttpSetOption

    ; WinHttpConnect(hSession, szHost, 11434, 0)
    mov rcx, [g_hHttpSession]
    lea rdx, [szHost]
    mov r8d, 11434
    xor r9d, r9d
    call WinHttpConnect

    test rax, rax
    jz init_fail_graceful

    mov [g_hHttpConnect], rax
    lea rcx, [szInitOk]
    call OutputDebugStringA
    lea rcx, [szModeReal]
    call OutputDebugStringA
    mov rax, 1
    jmp init_done

init_fail_graceful:
    ; Close session if we opened one
    mov rcx, [g_hHttpSession]
    test rcx, rcx
    jz init_after_close
    call WinHttpCloseHandle
    xor rax, rax
    mov [g_hHttpSession], rax
init_after_close:
    mov [g_hHttpConnect], rax
    mov dword ptr [g_InferenceModeSim], 1
    lea rcx, [szInitFail]
    call OutputDebugStringA
    lea rcx, [szModeSim]
    call OutputDebugStringA
    mov rax, 1                         ; Return non-zero so Chat_Init keeps going; Generate will use sim

init_done:
    add rsp, 48
    pop rbp
    ret
RawrXD_Inference_Init ENDP

;-----------------------------------------------------------------------------
; RawrXD_Inference_Generate — Stream tokens from RawrEngine
; RCX = model handle (ignored)
; RDX = prompt string (UTF-8)
; R8  = max buffer size (token buffer capacity)
; R9  = output token buffer (where to write tokens)
; 
; Returns: RAX = total bytes written to buffer
;
; Streams tokens from RawrEngine API and appends to output buffer in real-time
;-----------------------------------------------------------------------------
RawrXD_Inference_Generate PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    push r12
    .PUSHREG r12
    push r13
    .PUSHREG r13
    sub rsp, 256
    .ALLOCSTACK 256
    .ENDPROLOG

    mov r12, r9                       ; r12 = output buffer
    mov r13, r8                       ; r13 = buffer size
    mov rsi, rdx                      ; rsi = prompt
    xor rbx, rbx                      ; rbx = bytes written

    ; Hybrid: if no HTTP connection, run simulation path
    cmp qword ptr [g_hHttpConnect], 0
    jne do_http_gen
    ; --- Simulation path: write fallback message to buffer ---
    lea rsi, [szSimFallback]
sim_copy_loop:
    cmp rbx, r13
    jae sim_done
    mov al, byte ptr [rsi]
    test al, al
    jz sim_done
    mov byte ptr [r12+rbx], al
    inc rbx
    inc rsi
    jmp sim_copy_loop
sim_done:
    cmp rbx, r13
    jae sim_null
    mov byte ptr [r12+rbx], 0
sim_null:
    mov rax, rbx
    jmp gen_done

do_http_gen:
    lea rcx, [szGenStart]
    call OutputDebugStringA

    ; Build JSON request in [rsp]
    lea rdi, [rsp+80]                 ; JSON build area
    lea rsi, [szJsonPrefix]
    xor rcx, rcx
    
    ; Copy prefix: {"model":"phi-3-mini","prompt":"
gen_copy_prefix:
    mov al, byte ptr [rsi+rcx]
    test al, al
    jz gen_prefix_done
    mov byte ptr [rdi+rcx], al
    inc rcx
    jmp gen_copy_prefix

gen_prefix_done:
    ; Append user prompt (with basic quote escaping)
    mov rsi, rdx                      ; rdx = original prompt
    mov r10, rcx                      ; r10 = prefix length
    
gen_copy_prompt:
    mov al, byte ptr [rsi]
    test al, al
    jz gen_prompt_done
    
    cmp al, '"'
    jne gen_no_escape_quote
    mov byte ptr [rdi+r10], '\'
    inc r10
    mov al, '"'
    
gen_no_escape_quote:
    mov byte ptr [rdi+r10], al
    inc r10
    inc rsi
    jmp gen_copy_prompt

gen_prompt_done:
    ; Append suffix: ","stream":true,"num_predict":512}
    lea rsi, [szJsonSuffix]
    xor rcx, rcx
    
gen_copy_suffix:
    mov al, byte ptr [rsi+rcx]
    lea rax, [rdi+r10]
    mov byte ptr [rax+rcx], al
    test al, al
    jz gen_suffix_done
    inc rcx
    jmp gen_copy_suffix

gen_suffix_done:
    mov r11, r10                      ; r11 = JSON length with suffix
    add r11, rcx

    ; WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, NULL, NULL, WINHTTP_FLAG_BYPASS_PROXY_CACHE)
    mov r9, WINHTTP_FLAG_BYPASS_PROXY_CACHE
    xor r8d, r8d
    xor edx, edx
    lea rcx, [szEndpoint]
    mov edx, 1                        ; POST verb (L"POST")
    mov rcx, [g_hHttpConnect]
    call WinHttpOpenRequest

    test rax, rax
    jz gen_fail

    mov [g_hRequest], rax

    ; WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)
    mov r9, 20000000h                 ; ADDREQ_FLAG_ADD
    mov r8, -1                        ; auto strlen
    lea rdx, [szContentType]
    mov rcx, rax
    call WinHttpAddRequestHeaders

    ; WinHttpSendRequest(hRequest, NULL, 0, pJsonRequest, dwJsonLength, dwJsonLength, 0)
    xor r9d, r9d                      ; context
    mov r8, r11                       ; dwTotalLength = JSON length
    lea rcx, [rsp+80]                 ; pData
    xor edx, edx                      ; dwReadDataLength
    mov rcx, [g_hRequest]
    call WinHttpSendRequest

    test eax, eax
    jz gen_fail

    ; WinHttpReceiveResponse(hRequest, NULL)
    xor edx, edx
    mov rcx, [g_hRequest]
    call WinHttpReceiveResponse

    test eax, eax
    jz gen_fail

    ; Now stream NDJSON responses
gen_stream_loop:
    ; WinHttpQueryDataAvailable(hRequest, pdwSize)
    lea rdx, [rsp+40]
    mov rcx, [g_hRequest]
    call WinHttpQueryDataAvailable

    test eax, eax
    jz gen_fail

    mov ecx, dword ptr [rsp+40]
    test ecx, ecx
    jz gen_stream_done                ; No more data

    ; Cap to 4KB per read
    cmp ecx, 4096
    jle gen_read_size_ok
    mov ecx, 4096

gen_read_size_ok:
    ; WinHttpReadData(hRequest, pBuffer, dwSize, pdwRead)
    lea r9, [rsp+48]
    mov r8d, ecx
    lea rdx, [rsp+120]                ; read buffer
    mov rcx, [g_hRequest]
    call WinHttpReadData

    mov ecx, dword ptr [rsp+48]      ; bytes read
    test ecx, ecx
    jz gen_stream_done

    ; Parse NDJSON line: {"response":"TOKEN",...}
    lea rsi, [rsp+120]
    lea rdi, [rsp+120]
    add rdi, rcx                      ; end of buffer

gen_parse_loop:
    cmp rsi, rdi
    jae gen_stream_loop

    ; Find "response":"
    mov al, byte ptr [rsi]
    cmp al, '"'
    jne gen_parse_next
    
    cmp dword ptr [rsi+1], 'sser'     ; respo (little endian)
    jne gen_parse_next

    add rsi, 12                       ; skip "response":"

gen_token_copy:
    cmp rsi, rdi
    jae gen_token_done

    mov al, byte ptr [rsi]
    
    cmp al, '"'
    je gen_token_done
    
    cmp al, '\'
    jne gen_no_escape_token
    inc rsi                           ; skip escape char
    jmp gen_token_copy

gen_no_escape_token:
    ; Cap buffer overflow
    cmp rbx, r13
    jae gen_token_done
    
    mov byte ptr [r12+rbx], al
    inc rbx
    inc rsi
    jmp gen_token_copy

gen_token_done:
    ; Skip closing quote
    cmp rsi, rdi
    jae gen_stream_loop
    cmp byte ptr [rsi], '"'
    jne gen_stream_loop
    inc rsi
    jmp gen_parse_loop

gen_parse_next:
    inc rsi
    jmp gen_parse_loop

gen_stream_done:
    ; Null terminate output
    cmp rbx, r13
    jae gen_close_request
    mov byte ptr [r12+rbx], 0

    mov rax, rbx
    jmp gen_close_request

gen_fail:
    xor rax, rax

gen_close_request:
    mov rcx, [g_hRequest]
    test rcx, rcx
    jz gen_done
    call WinHttpCloseHandle

gen_done:
    add rsp, 256
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_Inference_Generate ENDP

END
