; ============================================================
; GenesisP0_AiBackendBridge.asm — Raw WinHTTP to Ollama backend
; Exports: AIBridge_Initialize, AIBridge_QueryModel, AIBridge_StreamResponse, AIBridge_Cleanup
; Zero dependencies beyond kernel32/winhttp
; WinHttpConnect expects LPCWSTR; use wstrServer for localhost.
; ============================================================
OPTION CASEMAP:NONE

EXTERN WinHttpOpen:PROC
EXTERN WinHttpConnect:PROC
EXTERN WinHttpOpenRequest:PROC
EXTERN WinHttpSendRequest:PROC
EXTERN WinHttpReceiveResponse:PROC
EXTERN WinHttpReadData:PROC
EXTERN WinHttpCloseHandle:PROC
EXTERN WinHttpQueryDataAvailable:PROC

PUBLIC AIBridge_Initialize
PUBLIC AIBridge_QueryModel
PUBLIC AIBridge_StreamResponse
PUBLIC AIBridge_Cleanup

.data
ALIGN 8
g_hSession      QWORD 0
g_hConnect      QWORD 0
g_hRequest      QWORD 0
szAgent         BYTE "RawrXD-Genesis/1.0",0
szServer        BYTE "localhost",0
; Wide "localhost" for WinHttpConnect (LPCWSTR)
wstrServer      WORD 006Ch, 006Fh, 0063h, 0061h, 006Ch, 0068h, 006Fh, 0073h, 0074h, 0
szPost          BYTE "POST",0
szEndpoint      BYTE "/api/generate",0
szContentType   BYTE "Content-Type: application/json",0
szJsonTemplate  BYTE '{"model":"%s","prompt":"%s","stream":true}',0
g_readBuffer    BYTE 4096 DUP(0)

.code
; ------------------------------------------------------------
; AIBridge_Initialize(const char* host, UINT16 port) -> BOOL
; host ignored in stub; uses localhost (wide).
; ------------------------------------------------------------
AIBridge_Initialize PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40h

    mov rsi, rcx                ; host
    mov di, dx                  ; port

    ; WinHttpOpen (user agent can be ANSI for Open)
    lea rcx, szAgent
    xor edx, edx                ; WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    xor r8d, r8d                ; Proxy name
    xor r9d, r9d                ; Proxy bypass
    mov QWORD PTR [rsp+20h], 0  ; Flags
    call WinHttpOpen
    test rax, rax
    jz @init_fail
    mov [g_hSession], rax

    ; WinHttpConnect — server name must be LPCWSTR
    mov rcx, rax
    lea rdx, wstrServer         ; localhost (wide)
    movzx r8d, di               ; Port
    mov r9d, 0                  ; Reserved
    call WinHttpConnect
    test rax, rax
    jz @init_fail_connect
    mov [g_hConnect], rax

    mov eax, 1
    jmp @init_done

@init_fail_connect:
    mov rcx, [g_hSession]
    call WinHttpCloseHandle
    mov [g_hSession], 0

@init_fail:
    xor eax, eax

@init_done:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
AIBridge_Initialize ENDP

; ------------------------------------------------------------
; AIBridge_QueryModel(const char* model, const char* prompt, char* outBuf, UINT32 bufSize) -> INT (bytes read)
; Stub: open request, send, receive, read loop; no sprintf — copy prompt only for stub.
; ------------------------------------------------------------
AIBridge_QueryModel PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 60h

    mov r12, rcx                ; model
    mov rsi, rdx                ; prompt
    mov rdi, r8                 ; outBuf
    mov ebx, r9d                ; bufSize

    ; OpenRequest
    mov rcx, [g_hConnect]
    lea rdx, szPost
    lea r8, szEndpoint
    xor r9d, r9d                ; Version
    mov QWORD PTR [rsp+20h], 0  ; Referer
    mov QWORD PTR [rsp+28h], 0  ; Accept types
    mov QWORD PTR [rsp+30h], 0  ; Flags
    call WinHttpOpenRequest
    test rax, rax
    jz @query_fail
    mov [g_hRequest], rax

    ; SendRequest — minimal stub body
    mov rcx, [g_hRequest]
    lea rdx, szContentType
    mov r8d, -1                 ; Length auto
    xor r9d, r9d                ; Optional body
    mov QWORD PTR [rsp+20h], 0
    mov QWORD PTR [rsp+28h], 0
    call WinHttpSendRequest
    test eax, eax
    jz @query_fail

    ; ReceiveResponse
    mov rcx, [g_hRequest]
    xor edx, edx
    call WinHttpReceiveResponse

    ; Read loop stub — return 0 bytes if no sprintf/body built
    xor r12d, r12d              ; Total bytes

@read_done:
    mov eax, r12d
    jmp @query_cleanup

@query_fail:
    mov eax, -1

@query_cleanup:
    mov rcx, [g_hRequest]
    test rcx, rcx
    jz @skip_close_req
    call WinHttpCloseHandle
    mov [g_hRequest], 0
@skip_close_req:

    add rsp, 60h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AIBridge_QueryModel ENDP

; ------------------------------------------------------------
; AIBridge_StreamResponse(void* callback) -> void
; Callback receives (const char* chunk, UINT32 len). Stub.
; ------------------------------------------------------------
AIBridge_StreamResponse PROC
    push rbx
    sub rsp, 28h

    add rsp, 28h
    pop rbx
    ret
AIBridge_StreamResponse ENDP

; ------------------------------------------------------------
; AIBridge_Cleanup() -> void
; ------------------------------------------------------------
AIBridge_Cleanup PROC
    push rbx
    sub rsp, 28h

    mov rcx, [g_hConnect]
    test rcx, rcx
    jz @skip_close_conn
    call WinHttpCloseHandle

@skip_close_conn:
    mov rcx, [g_hSession]
    test rcx, rcx
    jz @skip_close_sess
    call WinHttpCloseHandle

@skip_close_sess:
    xor eax, eax
    mov [g_hConnect], rax
    mov [g_hSession], rax

    add rsp, 28h
    pop rbx
    ret
AIBridge_Cleanup ENDP

END
