// ==============================================================================
// GenesisP0_AiBackendBridge.asm — Raw HTTP/JSON to Ollama in MASM64
// Exports: Genesis_AiBackendBridge_Init, Genesis_AiBackendBridge_SendPrompt, Genesis_AiBackendBridge_StreamResponse
// ==============================================================================
OPTION DOTNAME
EXTERN WinHttpOpen:PROC, WinHttpConnect:PROC, WinHttpOpenRequest:PROC
EXTERN WinHttpSendRequest:PROC, WinHttpReceiveResponse:PROC
EXTERN WinHttpReadData:PROC, WinHttpCloseHandle:PROC
EXTERN RtlZeroMemory:PROC, ExitProcess:PROC

.data
    align 8
    g_hSession          DQ 0
    g_hConnect          DQ 0
    g_ollamaHost        DB "localhost", 0
    g_ollamaPort        DW 11434
    
    szJsonTemplate      DB '{"model":"phi3:mini","prompt":"', 0
    szJsonEnd           DB '","stream":true}', 0
    szPost              DB "POST", 0
    szApiGenerate       DB "/api/generate", 0
    szContentType       DB "Content-Type: application/json", 0

.code
; ------------------------------------------------------------------------------
; Genesis_AiBackendBridge_Init — Initialize WinHTTP session to Ollama
; ------------------------------------------------------------------------------
Genesis_AiBackendBridge_Init PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    ; WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0)
    xor ecx, ecx                    ; User agent (would be wide string in real impl)
    xor edx, edx                    ; Access type
    xor r8, r8                      ; Proxy
    xor r9, r9                      ; ProxyBypass
    mov [rsp+32], r9                ; Flags
    call WinHttpOpen
    mov g_hSession, rax
    
    test rax, rax
    jz _ai_init_fail
    
    ; Connect to localhost:11434
    mov rcx, rax
    lea rdx, g_ollamaHost
    movzx r8, word ptr g_ollamaPort
    xor r9, r9                      ; Reserved
    call WinHttpConnect
    mov g_hConnect, rax
    
    xor rax, rax                    ; Success
    jmp _ai_init_exit
    
_ai_init_fail:
    mov rax, 1
    
_ai_init_exit:
    mov rsp, rbp
    pop rbp
    ret
Genesis_AiBackendBridge_Init ENDP

; ------------------------------------------------------------------------------
; Genesis_AiBackendBridge_SendPrompt — POST to /api/generate
; RCX = prompt string, RDX = responseBuffer, R8 = bufferSize
; ------------------------------------------------------------------------------
Genesis_AiBackendBridge_SendPrompt PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 88
    
    ; Open request
    mov r9, rcx                     ; Save prompt
    mov rcx, g_hConnect
    lea rdx, szPost
    lea r8, szApiGenerate
    xor rax, rax
    mov [rsp+32], rax               ; Version
    mov [rsp+40], rax               ; Referer
    mov [rsp+48], rax               ; Accept types
    mov [rsp+56], rax               ; Flags
    call WinHttpOpenRequest
    
    ; Send request (simplified)
    mov rcx, rax                    ; Request handle
    xor edx, edx                    ; Headers
    xor r8, r8                      ; Headers length
    mov r9, r9                      ; Optional data (prompt)
    mov [rsp+32], r9                ; Optional length
    mov [rsp+40], r9                ; Total length
    call WinHttpSendRequest
    
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
Genesis_AiBackendBridge_SendPrompt ENDP

; ------------------------------------------------------------------------------
; Genesis_AiBackendBridge_StreamResponse — Read streaming chunks
; RCX = callbackProc (void* data, size_t len)
; ------------------------------------------------------------------------------
Genesis_AiBackendBridge_StreamResponse PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    ; Implementation would read chunks and invoke callback
    xor rax, rax
    
    mov rsp, rbp
    pop rbp
    ret
Genesis_AiBackendBridge_StreamResponse ENDP

END
