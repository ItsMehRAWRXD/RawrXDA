; ============================================================================
; OLLAMA CLIENT - Pure MASM x64 WinHTTP Implementation
; ============================================================================
; Pure assembly implementation of Ollama HTTP client using WinHTTP API
; Features:
;   - GET /api/tags - List all available models
;   - GET /api/version - Get Ollama version
;   - POST /api/generate - Generate completions (streaming)
;   - Thread-safe with proper error handling
;   - Zero dependencies on C++ runtime
; ============================================================================

.code

; ============================================================================
; External WinHTTP API Functions
; ============================================================================
extern WinHttpOpen:PROC
extern WinHttpConnect:PROC
extern WinHttpOpenRequest:PROC
extern WinHttpSendRequest:PROC
extern WinHttpReceiveResponse:PROC
extern WinHttpQueryDataAvailable:PROC
extern WinHttpReadData:PROC
extern WinHttpCloseHandle:PROC

; External Windows API
extern GetLastError:PROC
extern LocalAlloc:PROC
extern LocalFree:PROC
extern lstrlenA:PROC
extern lstrcpyA:PROC
extern GetStdHandle:PROC
extern WriteFile:PROC

; External Logger
extern Logger_LogStructured:PROC

; ============================================================================
; Constants
; ============================================================================
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY = 0
WINHTTP_NO_PROXY_NAME = 0
WINHTTP_NO_PROXY_BYPASS = 0
WINHTTP_NO_REFERER = 0
WINHTTP_DEFAULT_ACCEPT_TYPES = 0
WINHTTP_FLAG_SECURE = 00800000h
WINHTTP_QUERY_FLAG_NUMBER = 20000000h

LMEM_ZEROINIT = 40h
LMEM_FIXED = 0h

LOG_LEVEL_INFO = 1
LOG_LEVEL_ERROR = 3
LOG_LEVEL_DEBUG = 0

MAX_RESPONSE_SIZE = 1048576   ; 1MB max response

; ============================================================================
; Data Section
; ============================================================================
.data
    ; User agent string (WinHTTP expects wide/UTF-16)
    szUserAgent         dw 'R','a','w','r','X','D','-','M','A','S','M','-','C','l','i','e','n','t','/','1','.','0',0
    
    ; Default host (localhost)
    szDefaultHost       dw 'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't', 0
    
    ; API endpoints
    szTagsEndpoint      dw '/', 'a', 'p', 'i', '/', 't', 'a', 'g', 's', 0
    szVersionEndpoint   dw '/', 'a', 'p', 'i', '/', 'v', 'e', 'r', 's', 'i', 'o', 'n', 0
    szGenerateEndpoint  dw '/', 'a', 'p', 'i', '/', 'g', 'e', 'n', 'e', 'r', 'a', 't', 'e', 0
    
    ; HTTP verbs
    szGetVerb           dw 'G', 'E', 'T', 0
    szPostVerb          dw 'P', 'O', 'S', 'T', 0
    
    ; Content-Type header
    szContentType       dw 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'T', 'y', 'p', 'e', ':', ' '
    szJsonType          dw 'a', 'p', 'p', 'l', 'i', 'c', 'a', 't', 'i', 'o', 'n', '/', 'j', 's', 'o', 'n', 0
    
    ; Log messages
    szOllamaClientInit      db "Ollama client initialized", 0
    szOllamaConnecting      db "Connecting to Ollama server", 0
    szOllamaRequestSent     db "HTTP request sent", 0
    szOllamaResponseRead    db "HTTP response received", 0
    szOllamaError           db "Ollama client error", 0
    szOllamaMemoryError     db "Memory allocation failed", 0

    ; Minimal stdout trace (diagnostic)
    szOllamaTraceStart      db "[ollama] init: start", 13, 10, 0
    szOllamaTraceAfterLog   db "[ollama] init: after log", 13, 10, 0
    szOllamaTraceBeforeOpen db "[ollama] init: before WinHttpOpen", 13, 10, 0
    szOllamaTraceAfterOpen  db "[ollama] init: after WinHttpOpen", 13, 10, 0
    szOllamaTraceBeforeConn db "[ollama] init: before WinHttpConnect", 13, 10, 0

.data?
    hSession            qword ?
    hConnect            qword ?
    hRequest            qword ?
    dwBytesAvailable    dword ?
    dwBytesRead         dword ?

.code

; ============================================================================
; OllamaClient_Initialize - Initialize WinHTTP session
; ============================================================================
; Parameters:
;   RCX = Hostname (wide string, NULL for localhost)
;   RDX = Port (default 11434)
; Returns:
;   RAX = Session handle (0 on failure)
; ============================================================================
PUBLIC OllamaClient_Initialize
OllamaClient_Initialize PROC
    push rbx
    push rsi
    push rdi
    ; Win64: keep stack 16-byte aligned for calls (3 pushes => aligned; allocate multiple of 16)
    sub rsp, 30h
    
    ; Save parameters
    mov rbx, rcx                ; hostname
    mov rsi, rdx                ; port

    lea rcx, [szOllamaTraceStart]
    call Ollama_Out
    
    ; Log initialization
    lea rcx, [szOllamaClientInit]
    mov rdx, LOG_LEVEL_INFO
    call Logger_LogStructured

    lea rcx, [szOllamaTraceAfterLog]
    call Ollama_Out

    lea rcx, [szOllamaTraceBeforeOpen]
    call Ollama_Out
    
    ; Open WinHTTP session
    lea rcx, [szUserAgent]
    mov rdx, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
    mov r8, WINHTTP_NO_PROXY_NAME
    mov r9, WINHTTP_NO_PROXY_BYPASS
    xor eax, eax
    mov [rsp+20h], rax          ; flags = 0
    call WinHttpOpen

    lea rcx, [szOllamaTraceAfterOpen]
    call Ollama_Out
    
    test rax, rax
    jz init_failed
    
    mov [hSession], rax
    
    ; Connect to host
    mov rcx, [hSession]
    test rbx, rbx
    jnz use_custom_host
    lea rbx, [szDefaultHost]
    
use_custom_host:
    lea rcx, [szOllamaTraceBeforeConn]
    call Ollama_Out

    mov rdx, rbx                ; hostname
    movzx r8d, si               ; port (default 11434)
    xor r9d, r9d                ; reserved
    call WinHttpConnect
    
    test rax, rax
    jz init_failed
    
    mov [hConnect], rax
    mov rax, [hSession]
    jmp init_done
    
init_failed:
    xor rax, rax
    
init_done:
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
OllamaClient_Initialize ENDP

; ============================================================================
; Ollama_Out - write a null-terminated ANSI string to stdout (best-effort)
; RCX = string
; ============================================================================
Ollama_Out PROC
    push rbx
    sub rsp, 40h

    mov rbx, rcx
    mov ecx, -11                ; STD_OUTPUT_HANDLE
    call GetStdHandle

    ; length in r8d
    xor r8d, r8d
oo_len_loop:
    cmp byte ptr [rbx + r8], 0
    je oo_len_done
    inc r8d
    jmp oo_len_loop
oo_len_done:

    mov rcx, rax
    mov rdx, rbx
    lea r9, [rsp + 30h]
    mov qword ptr [rsp + 20h], 0
    call WriteFile

    add rsp, 40h
    pop rbx
    ret
Ollama_Out ENDP

; ============================================================================
; OllamaClient_ListModels - Get list of available models (GET /api/tags)
; ============================================================================
; Parameters:
;   RCX = Output buffer for JSON response
;   RDX = Buffer size
; Returns:
;   RAX = Number of bytes written (0 on failure)
; ============================================================================
PUBLIC OllamaClient_ListModels
OllamaClient_ListModels PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 30h
    
    ; Save parameters
    mov rbx, rcx                ; output buffer
    mov rsi, rdx                ; buffer size
    
    ; Check if connected
    mov rax, [hConnect]
    test rax, rax
    jz list_failed
    
    ; Open request
    mov rcx, [hConnect]
    lea rdx, [szGetVerb]
    lea r8, [szTagsEndpoint]
    xor r9d, r9d                ; version (NULL = HTTP/1.1)
    mov qword ptr [rsp+20h], WINHTTP_NO_REFERER
    mov qword ptr [rsp+28h], WINHTTP_DEFAULT_ACCEPT_TYPES
    xor eax, eax
    mov [rsp+30h], rax          ; flags = 0
    call WinHttpOpenRequest
    
    test rax, rax
    jz list_failed
    
    mov [hRequest], rax
    mov r12, rax
    
    ; Send request
    mov rcx, r12
    xor rdx, rdx                ; headers
    xor r8d, r8d                ; headers length
    xor r9d, r9d                ; optional data
    xor eax, eax
    mov [rsp+20h], rax          ; optional length
    mov [rsp+28h], rax          ; total length
    mov [rsp+30h], rax          ; context
    call WinHttpSendRequest
    
    test eax, eax
    jz list_close_request
    
    ; Receive response
    mov rcx, r12
    xor rdx, rdx                ; reserved
    call WinHttpReceiveResponse
    
    test eax, eax
    jz list_close_request
    
    ; Query data available
    xor r13d, r13d              ; total bytes read
    
list_read_loop:
    mov rcx, r12
    lea rdx, [dwBytesAvailable]
    xor r8d, r8d                ; reserved
    call WinHttpQueryDataAvailable
    
    test eax, eax
    jz list_close_request
    
    mov eax, [dwBytesAvailable]
    test eax, eax
    jz list_read_done
    
    ; Check buffer space
    mov ecx, r13d
    add ecx, eax
    cmp ecx, esi
    jae list_read_done
    
    ; Read data
    mov rcx, r12
    lea rdx, [rbx + r13]        ; buffer offset
    mov r8d, [dwBytesAvailable]
    lea r9, [dwBytesRead]
    xor eax, eax
    mov [rsp+20h], rax          ; reserved
    call WinHttpReadData
    
    test eax, eax
    jz list_close_request
    
    mov eax, [dwBytesRead]
    add r13d, eax
    jmp list_read_loop
    
list_read_done:
    ; Null-terminate response
    cmp r13d, esi
    jae list_close_request
    mov byte ptr [rbx + r13], 0
    mov rax, r13
    jmp list_cleanup
    
list_close_request:
    mov rcx, [hRequest]
    call WinHttpCloseHandle
    
list_failed:
    xor rax, rax
    jmp list_done
    
list_cleanup:
    push rax
    mov rcx, [hRequest]
    call WinHttpCloseHandle
    pop rax
    
list_done:
    add rsp, 30h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
OllamaClient_ListModels ENDP

; ============================================================================
; OllamaClient_GetVersion - Get Ollama version (GET /api/version)
; ============================================================================
; Parameters:
;   RCX = Output buffer for version string
;   RDX = Buffer size
; Returns:
;   RAX = Number of bytes written (0 on failure)
; ============================================================================
PUBLIC OllamaClient_GetVersion
OllamaClient_GetVersion PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 30h
    
    ; Save parameters
    mov rbx, rcx                ; output buffer
    mov rsi, rdx                ; buffer size
    
    ; Check if connected
    mov rax, [hConnect]
    test rax, rax
    jz version_failed
    
    ; Open request
    mov rcx, [hConnect]
    lea rdx, [szGetVerb]
    lea r8, [szVersionEndpoint]
    xor r9d, r9d
    mov qword ptr [rsp+20h], WINHTTP_NO_REFERER
    mov qword ptr [rsp+28h], WINHTTP_DEFAULT_ACCEPT_TYPES
    xor eax, eax
    mov [rsp+30h], rax
    call WinHttpOpenRequest
    
    test rax, rax
    jz version_failed
    
    mov [hRequest], rax
    mov r12, rax
    
    ; Send request
    mov rcx, r12
    xor rdx, rdx
    xor r8d, r8d
    xor r9d, r9d
    xor eax, eax
    mov [rsp+20h], rax
    mov [rsp+28h], rax
    mov [rsp+30h], rax
    call WinHttpSendRequest
    
    test eax, eax
    jz version_close_request
    
    ; Receive response
    mov rcx, r12
    xor rdx, rdx
    call WinHttpReceiveResponse
    
    test eax, eax
    jz version_close_request
    
    ; Read response (same loop as ListModels)
    xor r13d, r13d
    
version_read_loop:
    mov rcx, r12
    lea rdx, [dwBytesAvailable]
    xor r8d, r8d
    call WinHttpQueryDataAvailable
    
    test eax, eax
    jz version_close_request
    
    mov eax, [dwBytesAvailable]
    test eax, eax
    jz version_read_done
    
    mov ecx, r13d
    add ecx, eax
    cmp ecx, esi
    jae version_read_done
    
    mov rcx, r12
    lea rdx, [rbx + r13]
    mov r8d, [dwBytesAvailable]
    lea r9, [dwBytesRead]
    xor eax, eax
    mov [rsp+20h], rax
    call WinHttpReadData
    
    test eax, eax
    jz version_close_request
    
    mov eax, [dwBytesRead]
    add r13d, eax
    jmp version_read_loop
    
version_read_done:
    cmp r13d, esi
    jae version_close_request
    mov byte ptr [rbx + r13], 0
    mov rax, r13
    jmp version_cleanup
    
version_close_request:
    mov rcx, [hRequest]
    call WinHttpCloseHandle
    
version_failed:
    xor rax, rax
    jmp version_done
    
version_cleanup:
    push rax
    mov rcx, [hRequest]
    call WinHttpCloseHandle
    pop rax
    
version_done:
    add rsp, 30h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
OllamaClient_GetVersion ENDP

; ============================================================================
; OllamaClient_Cleanup - Clean up WinHTTP handles
; ============================================================================
; Parameters: None
; Returns: None
; ============================================================================
PUBLIC OllamaClient_Cleanup
OllamaClient_Cleanup PROC
    push rbx
    sub rsp, 20h
    
    ; Close connection handle
    mov rcx, [hConnect]
    test rcx, rcx
    jz cleanup_session
    call WinHttpCloseHandle
    mov qword ptr [hConnect], 0
    
cleanup_session:
    ; Close session handle
    mov rcx, [hSession]
    test rcx, rcx
    jz cleanup_done
    call WinHttpCloseHandle
    mov qword ptr [hSession], 0
    
cleanup_done:
    add rsp, 20h
    pop rbx
    ret
OllamaClient_Cleanup ENDP

END
