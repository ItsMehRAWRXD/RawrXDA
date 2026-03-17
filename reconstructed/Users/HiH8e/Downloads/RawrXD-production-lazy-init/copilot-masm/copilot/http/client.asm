;======================================================================
; copilot_http_client.asm - GitHub Copilot HTTPS Client (MASM x64)
;======================================================================
INCLUDE windows.inc
INCLUDE winhttp.inc
INCLUDE bcrypt.inc
INCLUDELIB winhttp.lib
INCLUDELIB bcrypt.lib

.CONST
COPILOT_API_ENDPOINT   DB "https://api.githubcopilot.com",0
COPILOT_CHAT_ENDPOINT  DB "/chat/completions",0
COPILOT_AUTH_HEADER    DB "Authorization: Bearer ",0
USER_AGENT             DB "GitHubCopilot/1.0.0 (RawrXD-IDE)",0
CONTENT_TYPE_JSON      DB "Content-Type: application/json",0

.DATA?
hSession               QWORD ?
hConnect               QWORD ?
hRequest               QWORD ?
pJwtToken              QWORD ?
cbJwtToken             DWORD ?
bcryptAlgHandle        QWORD ?
bcryptHashHandle       QWORD ?

.CODE

CopilotHttp_Init PROC
    LOCAL hHeap:QWORD
    
    ; Create WinHTTP session
    lea rcx, USER_AGENT
    xor rdx, rdx              ; Access type (default)
    xor r8, r8                ; Proxy
    xor r9, r9                ; Proxy bypass
    call WinHttpOpen
    test rax, rax
    jz @error
    mov hSession, rax
    
    ; Set timeouts
    mov rcx, hSession
    mov rdx, 30000            ; Resolve timeout (ms)
    mov r8, 30000             ; Connect timeout
    mov r9, 30000             ; Send timeout
    push 60000                ; Receive timeout
    call WinHttpSetTimeouts
    add rsp, 8
    
    ; Initialize BCrypt for JWT validation
    lea rcx, bcryptAlgHandle
    lea rdx, BCRYPT_SHA256_ALGORITHM
    xor r8, r8
    xor r9, r9
    call BCryptOpenAlgorithmProvider
    cmp rax, STATUS_SUCCESS
    jne @error
    
    mov rax, 1                ; SUCCESS
    ret
    
@error:
    xor rax, rax
    ret
CopilotHttp_Init ENDP

CopilotHttp_Connect PROC
    LOCAL port:DWORD
    
    mov rcx, hSession
    lea rdx, COPILOT_API_ENDPOINT
    mov r8d, 443              ; HTTPS port
    xor r9, r9                ; Reserved
    call WinHttpConnect
    test rax, rax
    jz @error
    mov hConnect, rax
    
    mov rax, 1
    ret
    
@error:
    xor rax, rax
    ret
CopilotHttp_Connect ENDP

CopilotHttp_SendChatRequest PROC \
        pJsonBody:QWORD, \
        cbJsonBody:QWORD
    
    LOCAL hHeap:QWORD
    LOCAL pHeaders:QWORD
    
    ; Create request handle
    mov rcx, hConnect
    lea rdx, COPILOT_CHAT_ENDPOINT
    mov r8d, 4                ; POST method
    xor r9, r9                ; Version (HTTP/1.1)
    push 0                    ; Accept types
    push 0                    ; Referrer
    call WinHttpOpenRequest
    test rax, rax
    jz @error
    mov hRequest, rax
    
    ; Add headers
    invoke GetProcessHeap
    mov hHeap, rax
    mov rcx, rax
    mov rdx, HEAP_ZERO_MEMORY
    mov r8d, 4096
    call HeapAlloc
    mov pHeaders, rax
    
    ; Build Authorization header
    lea rcx, [rax]
    lea rdx, COPILOT_AUTH_HEADER
    call lstrcpyA
    mov rcx, rax
    add rcx, rax
    mov rdx, pJwtToken
    call lstrcatA
    
    ; Add headers to request
    mov rcx, hRequest
    mov rdx, pHeaders
    mov r8, -1                ; Headers length (auto)
    mov r9d, WINHTTP_ADDREQ_FLAG_ADD
    call WinHttpAddRequestHeaders
    
    ; Send request
    mov rcx, hRequest
    mov rdx, pJsonBody
    mov r8, cbJsonBody
    mov r9d, cbJsonBody
    call WinHttpSendRequest
    test rax, rax
    jz @error
    
    ; Receive response (streaming)
    invoke CopilotHttp_ReceiveStream, hRequest
    
    ret
    
@error:
    xor rax, rax
    ret
CopilotHttp_SendChatRequest ENDP

CopilotHttp_ReceiveStream PROC hRequest:QWORD
    LOCAL buffer[1024]:BYTE
    LOCAL dwRead:DWORD
    
    .repeat
        lea rcx, buffer
        mov edx, SIZEOF buffer
        lea r8, dwRead
        xor r9, r9
        call WinHttpReadData
        test rax, rax
        jz @done
        
        ; Process chunk (token)
        lea rcx, buffer
        mov edx, dwRead
        invoke ChatStream_OnToken, rcx, rdx
    .until rax == 0
    
@done:
    ret
CopilotHttp_ReceiveStream ENDP

CopilotHttp_SetJwtToken PROC pToken:QWORD, cbToken:DWORD
    mov pJwtToken, pToken
    mov cbJwtToken, cbToken
    ret
CopilotHttp_SetJwtToken ENDP

CopilotHttp_Cleanup PROC
    .if hRequest != 0
        mov rcx, hRequest
        call WinHttpCloseHandle
    .endif
    
    .if hConnect != 0
        mov rcx, hConnect
        call WinHttpCloseHandle
    .endif
    
    .if hSession != 0
        mov rcx, hSession
        call WinHttpCloseHandle
    .endif
    
    .if bcryptAlgHandle != 0
        mov rcx, bcryptAlgHandle
        call BCryptCloseAlgorithmProvider
    .endif
    
    ret
CopilotHttp_Cleanup ENDP

END
