; ═══════════════════════════════════════════════════════════════════
; ollama_client.asm — Phase 9C: Ollama Inference Client Wiring
;
; Connects the RawrXD inference engine to Ollama's REST API for
; real token generation using the locally-running 70B model.
;
; Architecture:
;   1. OllamaClient_Init    — WSAStartup, resolve host
;   2. OllamaClient_Generate — POST /api/generate, stream tokens
;   3. OllamaClient_Chat    — POST /api/chat (multi-turn)
;   4. OllamaClient_Shutdown — Cleanup
;
; Wire points:
;   - TokenGenerate calls OllamaClient_Generate when model is "ollama"
;   - The proxy (ollama_sovereign_proxy.asm) is optional — this client
;     can go direct to Ollama (localhost:11434) or through the proxy (11435)
;
; Exports:
;   OllamaClient_Init
;   OllamaClient_Generate
;   OllamaClient_Chat
;   OllamaClient_Abort
;   OllamaClient_SetModel
;   OllamaClient_Shutdown
;   OllamaClient_IsConnected
; ═══════════════════════════════════════════════════════════════════

EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN WSAStartup:PROC
EXTERN WSACleanup:PROC
EXTERN socket:PROC
EXTERN connect:PROC
EXTERN send:PROC
EXTERN recv:PROC
EXTERN closesocket:PROC
EXTERN inet_addr:PROC
EXTERN htons:PROC
EXTERN GetTickCount64:PROC

PUBLIC OllamaClient_Init
PUBLIC OllamaClient_Generate
PUBLIC OllamaClient_Chat
PUBLIC OllamaClient_Abort
PUBLIC OllamaClient_SetModel
PUBLIC OllamaClient_Shutdown
PUBLIC OllamaClient_IsConnected

; ── Constants ────────────────────────────────────────────────────
OLLAMA_DEFAULT_PORT equ 11434
OLLAMA_PROXY_PORT   equ 11435
RECV_BUF_SIZE       equ 65536
SEND_BUF_SIZE       equ 8192
MODEL_NAME_MAX      equ 128

; Socket constants
AF_INET             equ 2
SOCK_STREAM         equ 1
IPPROTO_TCP         equ 6
INVALID_SOCKET      equ -1

; MEM flags
MEM_COMMIT_C        equ 1000h
MEM_RESERVE_C       equ 2000h
MEM_RELEASE_C       equ 8000h
PAGE_RW             equ 4

.data
align 8
; Connection state
g_ollamaSocket  dq INVALID_SOCKET
g_ollamaPort    dw OLLAMA_DEFAULT_PORT
g_ollamaReady   dd 0
g_ollamaAbort   dd 0

; Model name (null-terminated, default)
g_ollamaModel   db 'huihui_ai/llama3.3-abliterated:latest', 0
                db (MODEL_NAME_MAX - 39) dup(0)

; Connection target
g_ollamaAddr    db '127.0.0.1', 0

; HTTP request template
g_httpPost      db 'POST /api/generate HTTP/1.1', 0Dh, 0Ah
                db 'Host: localhost', 0Dh, 0Ah
                db 'Content-Type: application/json', 0Dh, 0Ah
                db 'Content-Length: '
g_httpPostLen   equ $ - g_httpPost
g_httpCRLF      db 0Dh, 0Ah, 0Dh, 0Ah
g_httpCRLFLen   equ $ - g_httpCRLF

; Chat endpoint template
g_httpChatPost  db 'POST /api/chat HTTP/1.1', 0Dh, 0Ah
                db 'Host: localhost', 0Dh, 0Ah
                db 'Content-Type: application/json', 0Dh, 0Ah
                db 'Content-Length: '
g_httpChatLen   equ $ - g_httpChatPost

; JSON templates (will be composed at runtime)
g_jsonPrefix    db '{"model":"', 0
g_jsonPrompt    db '","prompt":"', 0
g_jsonStream    db '","stream":false}', 0
g_jsonChatPre   db '","messages":[{"role":"user","content":"', 0
g_jsonChatSuf   db '"}],"stream":false}', 0

; Telemetry
g_ollamaReqs    dq 0                ; Total requests sent
g_ollamaTokens  dq 0                ; Total tokens received
g_ollamaErrors  dq 0                ; Total errors
g_ollamaLastMs  dq 0                ; Last request latency (ms)

.data?
align 8
g_wsaData       db 408 dup(?)       ; WSADATA struct
g_recvBuf       db RECV_BUF_SIZE dup(?)
g_sendBuf       db SEND_BUF_SIZE dup(?)

.code

; ────────────────────────────────────────────────────────────────
; OllamaClient_Init — Initialize WinSock and verify Ollama
;   ECX = port (0 = use default 11434)
;   Returns: EAX = 0 success, -1 failure
; ────────────────────────────────────────────────────────────────
OllamaClient_Init PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Set port
    test    ecx, ecx
    jz      @oci_default_port
    mov     g_ollamaPort, cx
    jmp     @oci_wsa
@oci_default_port:
    mov     g_ollamaPort, OLLAMA_DEFAULT_PORT

@oci_wsa:
    ; WSAStartup(0x0202, &wsaData)
    mov     cx, 0202h
    lea     rdx, g_wsaData
    call    WSAStartup
    test    eax, eax
    jnz     @oci_fail

    ; Test connection to Ollama
    call    OllamaClient_Connect
    test    eax, eax
    jnz     @oci_fail

    ; Close test socket
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET

    mov     g_ollamaReady, 1
    mov     g_ollamaAbort, 0
    xor     eax, eax
    jmp     @oci_ret

@oci_fail:
    mov     eax, -1

@oci_ret:
    add     rsp, 30h
    pop     rbx
    ret
OllamaClient_Init ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_Connect — Internal: open TCP to Ollama
;   Returns: EAX = 0 success, -1 failure
; ────────────────────────────────────────────────────────────────
OllamaClient_Connect PROC FRAME
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    ; socket(AF_INET, SOCK_STREAM, 0)
    mov     ecx, AF_INET
    mov     edx, SOCK_STREAM
    xor     r8d, r8d
    call    socket
    cmp     rax, INVALID_SOCKET
    je      @occ_fail
    mov     g_ollamaSocket, rax

    ; Build sockaddr_in on stack
    mov     word ptr [rsp + 20h], AF_INET      ; sin_family
    movzx   ecx, g_ollamaPort
    call    htons
    mov     word ptr [rsp + 22h], ax           ; sin_port
    lea     rcx, g_ollamaAddr
    call    inet_addr
    mov     dword ptr [rsp + 24h], eax         ; sin_addr
    mov     qword ptr [rsp + 28h], 0           ; sin_zero

    ; connect
    mov     rcx, g_ollamaSocket
    lea     rdx, [rsp + 20h]
    mov     r8d, 16
    call    connect
    test    eax, eax
    jnz     @occ_close_fail

    xor     eax, eax
    jmp     @occ_ret

@occ_close_fail:
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET

@occ_fail:
    mov     eax, -1

@occ_ret:
    add     rsp, 38h
    ret
OllamaClient_Connect ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_Generate — Send prompt, receive generated text
;   RCX = pPrompt    (char* UTF-8 prompt, null-terminated)
;   RDX = pOutBuf    (char* output buffer for response text)
;   R8D = outBufSize (max bytes in output buffer)
;   Returns: EAX = bytes written to pOutBuf, or -1 on error
;
;   Forwards directly to OllamaClient_Generate2 which has proper
;   register save/restore. This wrapper exists for ABI compatibility.
; ────────────────────────────────────────────────────────────────
OllamaClient_Generate PROC
    ; Tail-call to Generate2 — all args are already in correct registers
    jmp     OllamaClient_Generate2
OllamaClient_Generate ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_Generate2 — Fixed version with proper register save
;   RCX = pPrompt    (char* UTF-8 prompt, null-terminated)
;   RDX = pOutBuf    (char* output buffer for response text)
;   R8D = outBufSize (max bytes in output buffer)
;   Returns: EAX = bytes written to pOutBuf, or -1 on error
; ────────────────────────────────────────────────────────────────
OllamaClient_Generate2 PROC FRAME
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
    sub     rsp, 58h
    .allocstack 58h
    .endprolog

    ; Save all params to stack slots
    mov     [rsp + 50h], rcx        ; pPrompt
    mov     [rsp + 48h], rdx        ; pOutBuf
    mov     [rsp + 40h], r8         ; outBufSize (as qword)

    ; Check ready
    cmp     g_ollamaReady, 1
    jne     @g2_fail

    ; Record start time
    call    GetTickCount64
    mov     r15, rax                ; startTick

    ; ── Build JSON body ──
    lea     rbx, g_sendBuf
    xor     r12d, r12d              ; offset into sendBuf

    ; {"model":"
    lea     rsi, g_jsonPrefix
    lea     rdi, [rbx]
    call    @g2_append

    ; <model_name>
    lea     rsi, g_ollamaModel
    call    @g2_append

    ; ","prompt":"
    lea     rsi, g_jsonPrompt
    call    @g2_append

    ; <prompt_text>
    mov     rsi, [rsp + 50h]        ; pPrompt
    call    @g2_append

    ; ","stream":false}
    lea     rsi, g_jsonStream
    call    @g2_append

    ; r12d = JSON body length
    mov     r13d, r12d              ; bodyLen

    ; ── Build HTTP request in recvBuf (reuse as scratch) ──
    ; We need: "POST /api/generate HTTP/1.1\r\nHost: ...\r\nContent-Length: <len>\r\n\r\n<body>"
    lea     rdi, g_recvBuf
    xor     r12d, r12d              ; offset

    ; Copy HTTP headers
    lea     rsi, g_httpPost
    mov     ecx, g_httpPostLen
    call    @g2_copy_n

    ; Convert bodyLen to ASCII and append
    mov     eax, r13d
    call    @g2_itoa                ; appends decimal digits

    ; \r\n\r\n
    lea     rsi, g_httpCRLF
    mov     ecx, g_httpCRLFLen
    call    @g2_copy_n

    ; Append JSON body
    lea     rsi, g_sendBuf
    mov     ecx, r13d
    call    @g2_copy_n

    ; r12d = total HTTP request length
    mov     r14d, r12d

    ; ── Connect to Ollama ──
    call    OllamaClient_Connect
    test    eax, eax
    jnz     @g2_fail

    ; ── Send request ──
    mov     rcx, g_ollamaSocket
    lea     rdx, g_recvBuf
    mov     r8d, r14d               ; total request bytes
    xor     r9d, r9d                ; flags
    call    send
    cmp     eax, -1
    je      @g2_close_fail

    ; ── Receive response ──
    ; Read into sendBuf (we no longer need it)
    lea     r12, g_sendBuf
    xor     r13d, r13d              ; total received

@g2_recv_loop:
    cmp     g_ollamaAbort, 1
    je      @g2_close_fail

    mov     rcx, g_ollamaSocket
    lea     rdx, [r12 + r13]
    mov     r8d, SEND_BUF_SIZE
    sub     r8d, r13d
    jle     @g2_recv_done           ; buffer full
    xor     r9d, r9d
    call    recv
    cmp     eax, 0
    jle     @g2_recv_done           ; connection closed or error
    add     r13d, eax
    jmp     @g2_recv_loop

@g2_recv_done:
    ; Close socket
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET

    ; Null-terminate response
    mov     byte ptr [r12 + r13], 0

    ; ── Parse JSON: find "response":" and extract value ──
    ; Search for "response":" in the received data
    lea     rcx, [r12]
    mov     edx, r13d
    call    @g2_extract_response    ; RAX = ptr to response text, ECX = length

    test    rax, rax
    jz      @g2_fail

    ; Copy to output buffer
    mov     rsi, rax
    mov     rdi, [rsp + 48h]        ; pOutBuf
    mov     edx, ecx                ; response length
    mov     r8d, dword ptr [rsp + 40h]  ; outBufSize
    dec     r8d                     ; leave room for null terminator

    ; Cap to outBufSize
    cmp     edx, r8d
    jbe     @g2_copy_ok
    mov     edx, r8d
@g2_copy_ok:
    mov     ecx, edx
    rep     movsb
    mov     byte ptr [rdi], 0       ; null terminate

    ; Update telemetry
    call    GetTickCount64
    sub     rax, r15
    mov     g_ollamaLastMs, rax
    inc     qword ptr [g_ollamaReqs]

    mov     eax, edx                ; return bytes written
    jmp     @g2_ret

@g2_close_fail:
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET
    inc     qword ptr [g_ollamaErrors]

@g2_fail:
    mov     eax, -1

@g2_ret:
    add     rsp, 58h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ── Internal helpers ──

@g2_append:
    ; Append null-terminated RSI to RBX at offset R12D
@g2a_loop:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @g2a_done
    mov     byte ptr [rbx + r12], al
    inc     r12d
    inc     rsi
    jmp     @g2a_loop
@g2a_done:
    ret

@g2_copy_n:
    ; Copy ECX bytes from RSI to RDI at offset R12D
    push    rcx
@g2cn_loop:
    test    ecx, ecx
    jz      @g2cn_done
    movzx   eax, byte ptr [rsi]
    mov     byte ptr [rdi + r12], al
    inc     rsi
    inc     r12d
    dec     ecx
    jmp     @g2cn_loop
@g2cn_done:
    pop     rcx
    ret

@g2_itoa:
    ; Convert EAX (unsigned) to decimal ASCII, append to RDI at R12D
    push    rbx
    mov     ebx, eax
    ; Stack-based digit extraction
    xor     ecx, ecx                ; digit count
@itoa_push:
    xor     edx, edx
    mov     eax, ebx
    mov     r8d, 10
    div     r8d                     ; eax = quotient, edx = remainder
    mov     ebx, eax
    add     dl, '0'
    push    rdx
    inc     ecx
    test    ebx, ebx
    jnz     @itoa_push
@itoa_pop:
    test    ecx, ecx
    jz      @itoa_done
    pop     rax
    mov     byte ptr [rdi + r12], al
    inc     r12d
    dec     ecx
    jmp     @itoa_pop
@itoa_done:
    pop     rbx
    ret

@g2_extract_response:
    ; Find "response":" in buffer at RCX, length EDX
    ; Returns: RAX = ptr to response text (after opening quote)
    ;          ECX = length (up to closing unescaped quote)
    ;          RAX = 0 if not found
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     edi, edx

    ; Search for "response":"
    ; Pattern: 22h 72h 65h 73h 70h 6Fh 6Eh 73h 65h 22h 3Ah 22h
    ; = "response":"
    xor     ecx, ecx
@er_scan:
    cmp     ecx, edi
    jge     @er_notfound
    cmp     byte ptr [rsi + rcx], '"'
    jne     @er_next
    ; Check "response":"
    lea     rax, [rsi + rcx]
    cmp     byte ptr [rax + 1], 'r'
    jne     @er_next
    cmp     byte ptr [rax + 2], 'e'
    jne     @er_next
    cmp     byte ptr [rax + 3], 's'
    jne     @er_next
    cmp     byte ptr [rax + 4], 'p'
    jne     @er_next
    cmp     byte ptr [rax + 5], 'o'
    jne     @er_next
    cmp     byte ptr [rax + 6], 'n'
    jne     @er_next
    cmp     byte ptr [rax + 7], 's'
    jne     @er_next
    cmp     byte ptr [rax + 8], 'e'
    jne     @er_next
    cmp     byte ptr [rax + 9], '"'
    jne     @er_next
    cmp     byte ptr [rax + 10], ':'
    jne     @er_next
    cmp     byte ptr [rax + 11], '"'
    jne     @er_next

    ; Found! Response text starts at rax + 12
    lea     rax, [rax + 12]

    ; Find closing unescaped quote
    xor     edx, edx
@er_end:
    cmp     byte ptr [rax + rdx], 0
    je      @er_found
    cmp     byte ptr [rax + rdx], '"'
    jne     @er_continue
    ; Check if escaped (preceded by backslash)
    test    edx, edx
    jz      @er_found               ; first char = quote → empty
    cmp     byte ptr [rax + rdx - 1], '\'
    je      @er_continue             ; escaped quote, skip
    jmp     @er_found
@er_continue:
    inc     edx
    jmp     @er_end

@er_found:
    mov     ecx, edx                ; length of response text
    pop     rdi
    pop     rsi
    ret

@er_next:
    inc     ecx
    jmp     @er_scan

@er_notfound:
    xor     eax, eax
    pop     rdi
    pop     rsi
    ret

OllamaClient_Generate2 ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_Chat — Multi-turn chat via /api/chat
;   RCX = pMessage   (char* UTF-8 user message, null-terminated)
;   RDX = pOutBuf    (char* output buffer)
;   R8D = outBufSize
;   Returns: EAX = bytes written, or -1 on error
; ────────────────────────────────────────────────────────────────
OllamaClient_Chat PROC FRAME
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
    sub     rsp, 58h
    .allocstack 58h
    .endprolog

    ; Save params
    mov     [rsp + 50h], rcx        ; pMessage
    mov     [rsp + 48h], rdx        ; pOutBuf
    mov     dword ptr [rsp + 40h], r8d  ; outBufSize

    cmp     g_ollamaReady, 1
    jne     @chat_fail

    call    GetTickCount64
    mov     r15, rax

    ; Build JSON: {"model":"<model>","messages":[{"role":"user","content":"<msg>"}],"stream":false}
    lea     rbx, g_sendBuf
    xor     r12d, r12d

    lea     rsi, g_jsonPrefix       ; {"model":"
    call    @chat_append
    lea     rsi, g_ollamaModel
    call    @chat_append
    lea     rsi, g_jsonChatPre      ; ","messages":[{"role":"user","content":"
    call    @chat_append
    mov     rsi, [rsp + 50h]        ; user message
    call    @chat_append
    lea     rsi, g_jsonChatSuf      ; "}],"stream":false}
    call    @chat_append

    mov     r13d, r12d              ; bodyLen

    ; Build HTTP request
    lea     rdi, g_recvBuf
    xor     r12d, r12d

    ; HTTP headers
    lea     rsi, g_httpChatPost
    mov     ecx, g_httpChatLen
    call    @chat_copy_n

    ; Content-Length
    mov     eax, r13d
    call    @chat_itoa

    ; \r\n\r\n
    lea     rsi, g_httpCRLF
    mov     ecx, g_httpCRLFLen
    call    @chat_copy_n

    ; Body
    lea     rsi, g_sendBuf
    mov     ecx, r13d
    call    @chat_copy_n

    mov     r14d, r12d              ; total request length

    ; Connect
    call    OllamaClient_Connect
    test    eax, eax
    jnz     @chat_fail

    ; Send
    mov     rcx, g_ollamaSocket
    lea     rdx, g_recvBuf
    mov     r8d, r14d
    xor     r9d, r9d
    call    send
    cmp     eax, -1
    je      @chat_close_fail

    ; Receive
    lea     r12, g_sendBuf
    xor     r13d, r13d

@chat_recv:
    mov     rcx, g_ollamaSocket
    lea     rdx, [r12 + r13]
    mov     r8d, SEND_BUF_SIZE
    sub     r8d, r13d
    jle     @chat_recv_done
    xor     r9d, r9d
    call    recv
    cmp     eax, 0
    jle     @chat_recv_done
    add     r13d, eax
    jmp     @chat_recv

@chat_recv_done:
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET
    mov     byte ptr [r12 + r13], 0

    ; Parse — look for "content":" field (chat response format)
    ; Similar to generate but field is "content" not "response"
    ; For simplicity, also accept "response" (Ollama uses both)
    lea     rcx, [r12]
    mov     edx, r13d
    call    @chat_extract           ; RAX = ptr, ECX = len

    test    rax, rax
    jz      @chat_fail

    mov     rsi, rax
    mov     rdi, [rsp + 48h]
    mov     edx, ecx
    mov     r8d, dword ptr [rsp + 40h]
    dec     r8d
    cmp     edx, r8d
    jbe     @chat_copy_ok
    mov     edx, r8d
@chat_copy_ok:
    mov     ecx, edx
    rep     movsb
    mov     byte ptr [rdi], 0

    call    GetTickCount64
    sub     rax, r15
    mov     g_ollamaLastMs, rax
    inc     qword ptr [g_ollamaReqs]

    mov     eax, edx
    jmp     @chat_ret

@chat_close_fail:
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET
    inc     qword ptr [g_ollamaErrors]

@chat_fail:
    mov     eax, -1

@chat_ret:
    add     rsp, 58h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@chat_append:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @ca_done
    mov     byte ptr [rbx + r12], al
    inc     r12d
    inc     rsi
    jmp     @chat_append
@ca_done:
    ret

@chat_copy_n:
    push    rcx
@ccn_loop:
    test    ecx, ecx
    jz      @ccn_done
    movzx   eax, byte ptr [rsi]
    mov     byte ptr [rdi + r12], al
    inc     rsi
    inc     r12d
    dec     ecx
    jmp     @ccn_loop
@ccn_done:
    pop     rcx
    ret

@chat_itoa:
    push    rbx
    mov     ebx, eax
    xor     ecx, ecx
@ci_push:
    xor     edx, edx
    mov     eax, ebx
    mov     r8d, 10
    div     r8d
    mov     ebx, eax
    add     dl, '0'
    push    rdx
    inc     ecx
    test    ebx, ebx
    jnz     @ci_push
@ci_pop:
    test    ecx, ecx
    jz      @ci_done
    pop     rax
    mov     byte ptr [rdi + r12], al
    inc     r12d
    dec     ecx
    jmp     @ci_pop
@ci_done:
    pop     rbx
    ret

@chat_extract:
    ; Search for "content":" or "response":" in buffer
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     edi, edx
    xor     ecx, ecx
@ce_scan:
    cmp     ecx, edi
    jge     @ce_nf
    cmp     byte ptr [rsi + rcx], '"'
    jne     @ce_next
    ; Check "content":"
    lea     rax, [rsi + rcx]
    cmp     byte ptr [rax + 1], 'c'
    jne     @ce_try_resp
    cmp     byte ptr [rax + 2], 'o'
    jne     @ce_next
    cmp     byte ptr [rax + 3], 'n'
    jne     @ce_next
    cmp     byte ptr [rax + 4], 't'
    jne     @ce_next
    cmp     byte ptr [rax + 5], 'e'
    jne     @ce_next
    cmp     byte ptr [rax + 6], 'n'
    jne     @ce_next
    cmp     byte ptr [rax + 7], 't'
    jne     @ce_next
    cmp     byte ptr [rax + 8], '"'
    jne     @ce_next
    cmp     byte ptr [rax + 9], ':'
    jne     @ce_next
    cmp     byte ptr [rax + 10], '"'
    jne     @ce_next
    lea     rax, [rax + 11]
    jmp     @ce_find_end

@ce_try_resp:
    cmp     byte ptr [rax + 1], 'r'
    jne     @ce_next
    cmp     byte ptr [rax + 2], 'e'
    jne     @ce_next
    cmp     byte ptr [rax + 3], 's'
    jne     @ce_next
    cmp     byte ptr [rax + 4], 'p'
    jne     @ce_next
    cmp     byte ptr [rax + 5], 'o'
    jne     @ce_next
    cmp     byte ptr [rax + 6], 'n'
    jne     @ce_next
    cmp     byte ptr [rax + 7], 's'
    jne     @ce_next
    cmp     byte ptr [rax + 8], 'e'
    jne     @ce_next
    cmp     byte ptr [rax + 9], '"'
    jne     @ce_next
    cmp     byte ptr [rax + 10], ':'
    jne     @ce_next
    cmp     byte ptr [rax + 11], '"'
    jne     @ce_next
    lea     rax, [rax + 12]

@ce_find_end:
    xor     edx, edx
@ce_end_loop:
    cmp     byte ptr [rax + rdx], 0
    je      @ce_found
    cmp     byte ptr [rax + rdx], '"'
    jne     @ce_cont
    test    edx, edx
    jz      @ce_found
    cmp     byte ptr [rax + rdx - 1], '\'
    je      @ce_cont
    jmp     @ce_found
@ce_cont:
    inc     edx
    jmp     @ce_end_loop

@ce_found:
    mov     ecx, edx
    pop     rdi
    pop     rsi
    ret

@ce_next:
    inc     ecx
    jmp     @ce_scan

@ce_nf:
    xor     eax, eax
    pop     rdi
    pop     rsi
    ret

OllamaClient_Chat ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_Abort — Signal any in-progress request to abort
; ────────────────────────────────────────────────────────────────
OllamaClient_Abort PROC
    mov     g_ollamaAbort, 1
    ret
OllamaClient_Abort ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_SetModel — Change the model name
;   RCX = pModelName (char* null-terminated, e.g. "llama3.3:70b")
; ────────────────────────────────────────────────────────────────
OllamaClient_SetModel PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    lea     rdx, g_ollamaModel
    xor     eax, eax
@sm_loop:
    cmp     eax, MODEL_NAME_MAX - 1
    jae     @sm_term
    movzx   r8d, byte ptr [rcx + rax]
    test    r8b, r8b
    jz      @sm_term
    mov     byte ptr [rdx + rax], r8b
    inc     eax
    jmp     @sm_loop
@sm_term:
    mov     byte ptr [rdx + rax], 0
    add     rsp, 28h
    ret
OllamaClient_SetModel ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_Shutdown — Cleanup WinSock
; ────────────────────────────────────────────────────────────────
OllamaClient_Shutdown PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    cmp     g_ollamaReady, 0
    je      @ocs_done

    ; Close any open socket
    cmp     g_ollamaSocket, INVALID_SOCKET
    je      @ocs_wsa
    mov     rcx, g_ollamaSocket
    call    closesocket
    mov     g_ollamaSocket, INVALID_SOCKET

@ocs_wsa:
    call    WSACleanup
    mov     g_ollamaReady, 0

@ocs_done:
    add     rsp, 28h
    ret
OllamaClient_Shutdown ENDP

; ────────────────────────────────────────────────────────────────
; OllamaClient_IsConnected — Quick check
;   Returns: EAX = 1 if initialized and Ollama reachable, 0 otherwise
; ────────────────────────────────────────────────────────────────
OllamaClient_IsConnected PROC
    mov     eax, g_ollamaReady
    ret
OllamaClient_IsConnected ENDP

END
