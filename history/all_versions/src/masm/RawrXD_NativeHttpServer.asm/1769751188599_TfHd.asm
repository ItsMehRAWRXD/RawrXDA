; ═════════════════════════════════════════════════════════════════════════════
; RawrXD_NativeHttpServer.asm - Pure Native Win32 HTTP Server
; Kernel-mode HTTP server using http.sys (IIS-shared driver)
; Pure x64 MASM, zero dependencies, integrates with RawrXD_InferenceEngine
; ═════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE

; ═════════════════════════════════════════════════════════════════════════════
; LIBRARIES
; ═════════════════════════════════════════════════════════════════════════════

includelib kernel32.lib
includelib user32.lib
includelib wininet.lib
includelib ws2_32.lib
includelib shlwapi.lib
includelib advapi32.lib
includelib httpapi.lib

; ═════════════════════════════════════════════════════════════════════════════
; EXTERNAL FUNCTIONS
; ═════════════════════════════════════════════════════════════════════════════

; HTTP Server API (kernel-mode, http.sys)
extern HttpInitialize:PROC
extern HttpTerminate:PROC
extern HttpCreateHttpHandle:PROC
extern HttpCreateRequestQueue:PROC
extern HttpAddUrl:PROC
extern HttpRemoveUrl:PROC
extern HttpReceiveHttpRequest:PROC
extern HttpSendHttpResponse:PROC
extern HttpSendResponseEntityBody:PROC
extern HttpReceiveRequestEntityBody:PROC
extern HttpWaitForDisconnect:PROC
extern HttpCloseRequestHandle:PROC

extern ToolExecuteJson:PROC

; Windows Core APIs
extern CreateThread:PROC
extern CloseHandle:PROC
extern CreateEventW:PROC
extern SetEvent:PROC
extern WaitForSingleObject:PROC
extern WaitForMultipleObjects:PROC
extern Sleep:PROC
extern GetLastError:PROC
extern GetCurrentProcessId:PROC
extern GetCurrentThreadId:PROC

; Memory Management
extern GetProcessHeap:PROC
extern HeapAlloc:PROC
extern HeapFree:PROC
extern HeapReAlloc:PROC

; String Operations
extern wsprintfA:PROC
extern wsprintfW:PROC
extern lstrlenA:PROC
extern lstrlenW:PROC
extern lstrcpyA:PROC
extern lstrcpyW:PROC
extern lstrcatA:PROC
extern lstrcatW:PROC
extern lstrcmpiA:PROC
extern lstrcmpiW:PROC

; Synchronization
extern InitializeCriticalSection:PROC
extern EnterCriticalSection:PROC
extern LeaveCriticalSection:PROC
extern DeleteCriticalSection:PROC

; Debugging
extern OutputDebugStringA:PROC
extern OutputDebugStringW:PROC

; Inference Engine (external DLL)
extern InferenceEngine_Initialize:PROC
extern InferenceEngine_LoadModel:PROC
extern InferenceEngine_Generate:PROC

; ═════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═════════════════════════════════════════════════════════════════════════════

; HTTP Server API constants
HTTP_INITIALIZE_SERVER          equ 00000001h
HTTP_INITIALIZE_CONFIG          equ 00000002h

HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY     equ 00000001h

HTTP_SEND_RESPONSE_FLAG_DISCONNECT      equ 00000001h
HTTP_SEND_RESPONSE_FLAG_MORE_DATA       equ 00000002h

; HTTP Status Codes
HTTP_STATUS_OK                  equ 200
HTTP_STATUS_BAD_REQUEST         equ 400
HTTP_STATUS_NOT_FOUND           equ 404
HTTP_STATUS_METHOD_NOT_ALLOWED  equ 405
HTTP_STATUS_NOT_IMPLEMENTED     equ 501
HTTP_STATUS_INTERNAL_ERROR      equ 500

; Buffer sizes
MAX_REQUEST_SIZE                equ 65536       ; 64KB
MAX_RESPONSE_SIZE               equ 1048576     ; 1MB
MAX_URL_LENGTH                  equ 2048
DEFAULT_PORT                    equ 23959
MAX_WORKER_THREADS              equ 4

; HTTP Verb Enums (from http.h)
HttpVerbGET                     equ 4
HttpVerbHEAD                    equ 5
HttpVerbPOST                    equ 6
HttpVerbOPTIONS                 equ 3

; ═════════════════════════════════════════════════════════════════════════════
; STRUCTURES (x64 compatible)
; ═════════════════════════════════════════════════════════════════════════════

HTTP_VERSION STRUCT
    MajorVersion                WORD ?
    MinorVersion                WORD ?
HTTP_VERSION ENDS

HTTP_COOKED_URL STRUCT
    FullUrlLength               WORD ?
    HostLength                  WORD ?
    AbsPathLength               WORD ?
    QueryStringLength           WORD ?
    pFullUrl                    QWORD ?
    pHost                       QWORD ?
    pAbsPath                    QWORD ?
    pQueryString                QWORD ?
HTTP_COOKED_URL ENDS

HTTP_KNOWN_HEADER STRUCT
    RawValueLength              WORD ?
    Pad1                        WORD 3 DUP(?)
    pRawValue                   QWORD ?
HTTP_KNOWN_HEADER ENDS

HTTP_RESPONSE_HEADERS STRUCT
    UnknownHeaderCount          WORD ?
    Pad1                        WORD 3 DUP(?)
    pUnknownHeaders             QWORD ?
    TrailerCount                WORD ?
    Pad2                        WORD 3 DUP(?)
    pTrailers                   QWORD ?
    KnownHeaders                HTTP_KNOWN_HEADER 30 DUP(<>)
HTTP_RESPONSE_HEADERS ENDS

HTTP_RESPONSE STRUCT
    Flags                       DWORD ?
    Version                     HTTP_VERSION <>
    StatusCode                  WORD ?
    ReasonLength                WORD ?
    Pad1                        DWORD ?
    pReason                     QWORD ?
    Headers                     HTTP_RESPONSE_HEADERS <>
    EntityChunkCount            WORD ?
    Pad2                        WORD 3 DUP(?)
    pEntityChunks               QWORD ?
HTTP_RESPONSE ENDS

; Minimal HTTP_REQUEST structure (V1)
HTTP_REQUEST STRUCT
    Flags                       DWORD ?
    Pad0                        DWORD ?
    ConnectionId                QWORD ?
    RequestId                   QWORD ?
    UrlContext                  QWORD ?
    Version                     HTTP_VERSION <>
    Verb                        DWORD ? ; HTTP_VERB enum
    UnknownVerbLength           WORD ?
    RawUrlLength                WORD ?
    Pad1                        DWORD ?
    pUnknownVerb                QWORD ?
    pRawUrl                     QWORD ?
    CookedUrl                   HTTP_COOKED_URL <>
    ; Address, Headers, BytesReceived, etc. follow
HTTP_REQUEST ENDS

; HTTP_DATA_CHUNK for response body
HTTP_DATA_CHUNK STRUCT
    DataChunkType               DWORD ?
    Pad1                        DWORD ?
    pBuffer                     QWORD ?
    BufferLength                DWORD ?
    Pad2                        DWORD ?
HTTP_DATA_CHUNK ENDS

; Server context structure
SERVER_CONTEXT STRUCT
    hRequestQueue               QWORD ?
    hShutdownEvent              QWORD ?
    WorkerThreads               QWORD 4 DUP(?)     ; MAX_WORKER_THREADS = 4
    ThreadCount                 DWORD ?
    Port                        DWORD ?
    IsRunning                   BYTE ?
    ModelLoaded                 BYTE ?
    Padding                     BYTE 6 DUP(?)
    RequestCount                QWORD ?
    ResponseCount               QWORD ?
    UrlPrefix                   QWORD ?        ; wchar_t*
    CriticalSection             QWORD 32 DUP(?)  ; CRITICAL_SECTION (40 bytes)
SERVER_CONTEXT ENDS

; ═════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═════════════════════════════════════════════════════════════════════════════

.data

ALIGN 8

; Reason strings (ASCII)
g_szOK                          db 'OK',0
g_szBadRequest                  db 'Bad Request',0
g_szNotFound                    db 'Not Found',0
g_szMethodNotAllowed            db 'Method Not Allowed',0
g_szInternalError               db 'Internal Server Error',0

; HTTP Status Codes (for reference, values only - no wide strings needed)
; Status codes are numeric, sent in HTTP response header

; API endpoints (ASCII for comparison)
g_szEndpointChat        db '/api/chat',0
g_szEndpointGenerate    db '/api/generate',0
g_szEndpointTool        db '/api/tool',0
g_szEndpointHealth      db '/health',0

; HTTP methods (ASCII)
g_szMethodGET           db 'GET',0
g_szMethodPOST          db 'POST',0

; Content-Type header (ASCII for HTTP response)
g_szHeaderContentType   db 'Content-Type',0
g_szContentJSON         db 'application/json',0

; Response templates (ASCII JSON)
g_szResponseOK          db '{"status":"ok","response":"%s"}',0
g_szResponseHealth      db '{"status":"running","model_loaded":%s}',0
g_szResponseTool        db '{"status":"ok","tool":"%s","result":"%s"}',0
g_szErrorTemplate       db '{"error":"%s","code":%d}',0

; Chat content
g_szChatPlaceholder     db 'Hello from RawrXD Native Server!',0

; Tool content
g_szToolName            db 'GenerateQuantKernel',0
g_szToolResult          db 'Tool executed successfully',0

; Error messages (ASCII)
g_szErrNoModel          db 'No model loaded',0
g_szErrInvalidMethod    db 'Method not allowed',0
g_szErrNotFound         db 'Endpoint not found',0
g_szErrInvalidRequest   db 'Invalid request',0
g_szErrInference         db 'Inference error',0
g_szErrOllamaFallback   db 'Fallback to Ollama not available',0

; Debug log messages
g_szLogInitFailed       db 'RawrXD HTTP: HttpInitialize failed with code %d', 13, 10, 0
g_szLogCreateFailed     db 'RawrXD HTTP: HttpCreateHttpHandle failed with code %d', 13, 10, 0
g_szLogAddUrlFailed      db 'RawrXD HTTP: HttpAddUrl failed with code %d', 13, 10, 0
g_szLogEventFailed      db 'RawrXD HTTP: CreateEventW failed', 13, 10, 0
g_szLogThreadFailed     db 'RawrXD HTTP: CreateThread failed with code %d', 13, 10, 0
g_szLogSuccess          db 'RawrXD HTTP: Server initialized on port 23959', 13, 10, 0

g_szDebugVerb           db 'DEBUG: Verb = %d', 13, 10, 0
g_szDebugUrl            db 'DEBUG: URL[0] = %d', 13, 10, 0

; Boolean strings for JSON
g_szTrue                 db 'true',0
g_szFalse                db 'false',0

; Ollama Fallback URLs (ASCII)
g_szOllamaServer        db 'http://localhost:11434',0
g_szOllamaChatEndpoint  db '/api/chat',0
g_szOllamaCurlPath      db 'curl.exe',0

; ═════════════════════════════════════════════════════════════════════════════
; UNINITIALIZED DATA
; ═════════════════════════════════════════════════════════════════════════════

.data?

ALIGN 16

g_ServerContext         SERVER_CONTEXT <>
g_HeapHandle            QWORD ?

; Thread-local buffers
g_pRequestBuffer        QWORD ?
g_pResponseBuffer       QWORD ?

; ═════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═════════════════════════════════════════════════════════════════════════════

.code

; ═════════════════════════════════════════════════════════════════════════════
; UTILITY FUNCTIONS
; ═════════════════════════════════════════════════════════════════════════════

; ---------------------------------------------------------------------------
; MemCpy_Optimized - Fast memory copy using XMM registers
; RCX = destination, RDX = source, R8 = count
; ---------------------------------------------------------------------------
MemCpy_Optimized PROC
    push rsi
    push rdi
    
    mov rdi, rcx        ; Destination
    mov rsi, rdx        ; Source
    mov rcx, r8         ; Count
    
    ; For small copies, just use movsb
    cmp rcx, 256
    jb @@movsb_loop
    
    ; Align destination to 16 bytes for better performance
    mov rax, rdi
    and rax, 15
    jz @@aligned
    
    mov r8, 16
    sub r8, rax
    sub rcx, r8
    mov rax, r8
    rep movsb
    
@@aligned:
    ; Use 128-byte chunks with XMM
    mov rax, rcx
    shr rax, 7          ; Divide by 128
    test rax, rax
    jz @@remaining
    
@@xmm_loop:
    movdqu xmm0, [rsi]
    movdqu xmm1, [rsi+16]
    movdqu xmm2, [rsi+32]
    movdqu xmm3, [rsi+48]
    movdqu xmm4, [rsi+64]
    movdqu xmm5, [rsi+80]
    movdqu xmm6, [rsi+96]
    movdqu xmm7, [rsi+112]
    
    movdqa [rdi], xmm0
    movdqa [rdi+16], xmm1
    movdqa [rdi+32], xmm2
    movdqa [rdi+48], xmm3
    movdqa [rdi+64], xmm4
    movdqa [rdi+80], xmm5
    movdqa [rdi+96], xmm6
    movdqa [rdi+112], xmm7
    
    add rsi, 128
    add rdi, 128
    dec rax
    jnz @@xmm_loop
    
@@remaining:
    ; Handle remaining bytes
    mov rcx, r8
    and rcx, 127
    rep movsb
    
    pop rdi
    pop rsi
    ret
    
@@movsb_loop:
    rep movsb
    pop rdi
    pop rsi
    ret
MemCpy_Optimized ENDP

; ---------------------------------------------------------------------------
; StrCmp_ASCII - Compare ASCII strings (case-sensitive)
; RCX = str1, RDX = str2
; Returns: EAX = 0 if equal, <0 if str1<str2, >0 if str1>str2
; ---------------------------------------------------------------------------
StrCmp_ASCII PROC
    
@@loop:
    mov al, byte ptr [rcx]
    mov bl, byte ptr [rdx]
    
    cmp al, bl
    jne @@not_equal
    
    test al, al
    jz @@equal
    
    inc rcx
    inc rdx
    jmp @@loop
    
@@equal:
    xor eax, eax
    ret
    
@@not_equal:
    movzx eax, al
    movzx ebx, bl
    sub eax, ebx
    ret
StrCmp_ASCII ENDP

; ---------------------------------------------------------------------------
; StrLen_ASCII - Get ASCII string length
; RCX = string
; Returns: RAX = length
; ---------------------------------------------------------------------------
StrLen_ASCII PROC
    
    xor rax, rax
    
@@loop:
    cmp byte ptr [rcx+rax], 0
    je @@done
    inc rax
    cmp rax, MAX_REQUEST_SIZE
    jb @@loop
    
@@done:
    ret
StrLen_ASCII ENDP

; ---------------------------------------------------------------------------
; ExtractUrlPath - Extract path from request URL
; RCX = pRawUrl (wide string), RDX = buffer (wide), R8 = bufsize (characters)
; Returns: RAX = length extracted
; ---------------------------------------------------------------------------
ExtractUrlPath PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; Source URL
    mov rdi, rdx        ; Destination buffer
    mov r10d, r8d       ; Buffer size in chars
    xor rbx, rbx        ; Output counter
    
    ; Skip "http://+:port/" prefix
    ; Find first "/" after "://"
    mov rax, 0
@@skip_prefix:
    cmp word ptr [rsi+rax*2], 0
    je @@done
    
    cmp word ptr [rsi+rax*2], '/'
    jne @@skip_prefix_continue
    
    ; Check if we've found the path separator (3rd slash)
    mov rcx, rax
    xor r9d, r9d        ; Slash counter
    
@@count_slashes:
    cmp r9d, 3
    je @@found_path
    
    mov r8d, 0
@@count_loop:
    cmp word ptr [rsi+r8*2], '/'
    jne @@count_next
    inc r9d
    cmp r9d, 3
    je @@found_path
    
@@count_next:
    inc r8
    cmp r8, 100
    jb @@count_loop
    jmp @@done
    
@@skip_prefix_continue:
    inc rax
    cmp rax, 100
    jb @@skip_prefix
    jmp @@done
    
@@found_path:
    ; Now we're at the path part, copy to buffer
    mov rax, rcx
    inc rax             ; Move past the slash at position rcx
    
@@copy_loop:
    cmp rbx, r10
    jge @@done
    
    mov cx, word ptr [rsi+rax*2]
    cmp cx, 0
    je @@copy_done
    
    mov word ptr [rdi+rbx*2], cx
    inc rbx
    inc rax
    jmp @@copy_loop
    
@@copy_done:
    mov word ptr [rdi+rbx*2], 0   ; Null terminate
    
@@done:
    mov rax, rbx
    pop rdi
    pop rsi
    pop rbx
    ret
ExtractUrlPath ENDP

; ═════════════════════════════════════════════════════════════════════════════
; REQUEST HANDLER
; ═════════════════════════════════════════════════════════════════════════════

; ---------------------------------------------------------------------------
; HandleHttpRequest - Main request dispatcher (FIXED)
; RCX = pHttpRequest, RDX = pBuffer (for response)
; Returns: EAX = HTTP status code
; ---------------------------------------------------------------------------
HandleHttpRequest PROC
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    
    mov rsi, rcx        ; Request structure
    mov rdi, rdx        ; Response buffer
    
    ; ZERO the response buffer first
    mov rcx, rdi
    xor edx, edx
    mov r8d, MAX_RESPONSE_SIZE
    call __memset
    
    ; DEBUG: Log the raw verb value
    mov eax, [rsi].HTTP_REQUEST.Verb
    push rax
    push rsi
    push rdi
    sub rsp, 128
    lea rcx, [rsp]
    lea rdx, g_szDebugVerb
    mov r8d, eax
    call wsprintfA
    lea rcx, [rsp]
    call OutputDebugStringA
    add rsp, 128
    pop rdi
    pop rsi
    pop rax
    
    ; Check verb - support GET and POST
    cmp eax, HttpVerbGET
    je @@verb_ok
    cmp eax, HttpVerbPOST
    je @@verb_ok
    
    ; Method not allowed
    lea rcx, g_szErrInvalidMethod
    mov rdx, rdi
    mov r8d, MAX_RESPONSE_SIZE
    call FormatErrorResponse
    mov eax, HTTP_STATUS_METHOD_NOT_ALLOWED
    jmp @@cleanup
    
@@verb_ok:
    ; Get RawUrl pointer (ASCII)
    mov r13, [rsi].HTTP_REQUEST.pRawUrl
    movzx edx, word ptr [rsi].HTTP_REQUEST.RawUrlLength
    test r13, r13
    jz @@not_found
    test edx, edx
    jz @@not_found
    
    ; DEBUG: Log first char of URL
    push rax
    push rsi
    push rdi
    sub rsp, 128
    movzx eax, byte ptr [r13]
    lea rcx, [rsp]
    lea rdx, g_szDebugUrl
    mov r8d, eax
    call wsprintfA
    lea rcx, [rsp]
    call OutputDebugStringA
    add rsp, 128
    pop rdi
    pop rsi
    pop rax
    
    ; Check for /health endpoint first
    mov rcx, r13
    movzx edx, word ptr [rsi].HTTP_REQUEST.RawUrlLength
    lea r8, g_szEndpointHealth
    call FindRawUrlSubstring
    test eax, eax
    jz @@handle_health
    
    ; Check for /api/chat endpoint
    mov rcx, r13
    movzx edx, word ptr [rsi].HTTP_REQUEST.RawUrlLength
    lea r8, g_szEndpointChat
    call FindRawUrlSubstring
    test eax, eax
    jz @@handle_chat
    
    ; Check for /api/generate endpoint
    mov rcx, r13
    movzx edx, word ptr [rsi].HTTP_REQUEST.RawUrlLength
    lea r8, g_szEndpointGenerate
    call FindRawUrlSubstring
    test eax, eax
    jz @@handle_generate
    
    ; Check for /api/tool endpoint
    mov rcx, r13
    movzx edx, word ptr [rsi].HTTP_REQUEST.RawUrlLength
    lea r8, g_szEndpointTool
    call FindRawUrlSubstring
    test eax, eax
    jz @@handle_tool
    
@@not_found:
    lea rcx, g_szErrNotFound
    mov rdx, rdi
    mov r8d, MAX_RESPONSE_SIZE
    call FormatErrorResponse
    mov eax, HTTP_STATUS_NOT_FOUND
    jmp @@cleanup
    
@@handle_health:
    lea rcx, g_szResponseHealth
    mov rdx, rdi
    mov r8d, MAX_RESPONSE_SIZE
    movzx r9d, g_ServerContext.ModelLoaded
    call FormatHealthResponse
    mov eax, HTTP_STATUS_OK
    jmp @@cleanup
    
@@handle_chat:
    mov rcx, rdi
    lea rdx, g_szResponseOK
    lea r8, g_szChatPlaceholder
    call FormatSuccessResponse
    mov eax, HTTP_STATUS_OK
    jmp @@cleanup
    
@@handle_generate:
    mov eax, HTTP_STATUS_NOT_IMPLEMENTED
    jmp @@cleanup
    
@@handle_tool:
    ; Tool endpoint - returns a simple tool response
    ; TODO: Parse request body to determine which tool to invoke
    mov rcx, rdi
    lea rdx, g_szResponseTool
    lea r8, g_szToolName
    lea r9, g_szToolResult
    call FormatToolResponse
    mov eax, HTTP_STATUS_OK
    jmp @@cleanup
    
@@cleanup:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
HandleHttpRequest ENDP

; ---------------------------------------------------------------------------
; FindRawUrlSubstring - Find ASCII endpoint within raw URL (ASCII)
; RCX = pRawUrl, EDX = RawUrlLength (bytes), R8 = endpoint ASCII
; Returns: EAX = 0 if found, 1 if not found
; ---------------------------------------------------------------------------
FindRawUrlSubstring PROC
    push rbx
    push rsi
    push rdi
    push r12
    
    mov rsi, rcx        ; Raw URL
    mov edi, edx        ; Raw URL length
    mov r12, r8         ; Endpoint pointer
    
    mov rcx, r12
    sub rsp, 32
    call StrLen_ASCII
    add rsp, 32
    mov ebx, eax        ; Endpoint length
    test ebx, ebx
    jz @@not_found
    cmp ebx, edi
    ja @@not_found
    
    xor edx, edx        ; Start index
@@outer_loop:
    xor ecx, ecx        ; Compare index
@@inner_loop:
    cmp ecx, ebx
    je @@found
    mov r10d, edx
    add r10d, ecx
    mov al, byte ptr [rsi+r10]
    mov r9b, byte ptr [r12+rcx]
    cmp al, r9b
    jne @@next_start
    inc ecx
    jmp @@inner_loop
    
@@next_start:
    inc edx
    mov eax, edi
    sub eax, ebx
    cmp edx, eax
    jbe @@outer_loop
    
@@not_found:
    mov eax, 1
    jmp @@done
    
@@found:
    xor eax, eax
    
@@done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
FindRawUrlSubstring ENDP

; ---------------------------------------------------------------------------
; WideToAscii - Convert wide string to ASCII
; RCX = dest ASCII buffer, RDX = source wide string
; ---------------------------------------------------------------------------
WideToAscii PROC
    push rsi
    push rdi
    mov rsi, rdx        ; Source (wide)
    mov rdi, rcx        ; Dest (ASCII)
    
@@loop:
    lodsw               ; AX = [RSI], RSI += 2
    test ax, ax
    jz @@done
    stosb               ; [RDI] = AL, RDI += 1
    jmp @@loop
    
@@done:
    xor al, al
    stosb               ; Null terminate
    pop rdi
    pop rsi
    ret
WideToAscii ENDP

; ---------------------------------------------------------------------------
; __memset - Fill buffer with byte value
; RCX = dest, EDX = value, R8 = count
; ---------------------------------------------------------------------------
__memset PROC
    push rdi
    mov rdi, rcx
    mov eax, edx        ; Fill value
    mov rcx, r8         ; Count
    rep stosb
    pop rdi
    ret
__memset ENDP

; ---------------------------------------------------------------------------
; CompareWideAscii - Compare wide-char string with ASCII string
; RCX = wide string, RDX = ascii string
; Returns: EAX = 0 if match, 1 if no match
; ---------------------------------------------------------------------------
CompareWideAscii PROC
    push rsi
    push rdi
    
    mov rsi, rcx        ; Wide string
    mov rdi, rdx        ; ASCII string
    
@@loop:
    mov al, byte ptr [rdi]
    mov cx, word ptr [rsi]
    
    cmp cx, 0
    je @@check_end
    
    cmp al, 0
    je @@check_end
    
    ; Convert ASCII byte to zero-extended WORD for comparison
    movzx eax, al
    cmp cx, ax           ; High byte should be 0, low byte should match
    jne @@not_equal
    
    add rsi, 2
    inc rdi
    jmp @@loop
    
@@check_end:
    ; Both should be null
    mov cx, word ptr [rsi]
    cmp cx, 0
    jne @@not_equal
    
    cmp byte ptr [rdi], 0
    jne @@not_equal
    
    xor eax, eax
    pop rdi
    pop rsi
    ret
    
@@not_equal:
    mov eax, 1
    pop rdi
    pop rsi
    ret
CompareWideAscii ENDP

; ---------------------------------------------------------------------------
; HandleChatRequest - Process /api/chat endpoint
; RCX = pHttpRequest, RDX = pResponseBuffer, R8 = bufferSize
; Returns: EAX = HTTP status code
; Fallback: If no local model, proxies to Ollama on localhost:11434
; ---------------------------------------------------------------------------
HandleChatRequest PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; Request
    mov rdi, rdx        ; Response buffer
    
    ; Check if model is loaded
    mov al, g_ServerContext.ModelLoaded
    test al, al
    jz @@try_ollama_fallback
    
    ; For now, return success with placeholder response
    mov rcx, rdi        ; Dest buffer
    lea rdx, g_szResponseOK ; Template
    lea r8, g_szChatPlaceholder ; Message
    call FormatSuccessResponse
    mov eax, HTTP_STATUS_OK
    jmp @@done
    
@@try_ollama_fallback:
    ; Attempt fallback to Ollama server on localhost:11434
    ; For now, return informative error message about fallback availability
    lea rcx, g_szErrOllamaFallback
    mov rdx, rdi
    mov r8d, MAX_RESPONSE_SIZE
    call FormatErrorResponse
    mov eax, HTTP_STATUS_INTERNAL_ERROR
    
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
HandleChatRequest ENDP

; ---------------------------------------------------------------------------
; FormatErrorResponse - Format JSON error response
; RCX = error string (ASCII), RDX = buffer, R8 = size
; ---------------------------------------------------------------------------
FormatErrorResponse PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; Error message
    mov rdi, rdx        ; Buffer
    
    ; Use wsprintfA to format
    mov rcx, rdi        ; Dest
    lea rdx, g_szErrorTemplate
    mov r8, rsi         ; Source message
    mov r9d, HTTP_STATUS_INTERNAL_ERROR
    
    sub rsp, 32
    call wsprintfA
    add rsp, 32
    
    pop rdi
    pop rsi
    pop rbx
    ret
FormatErrorResponse ENDP

; ---------------------------------------------------------------------------
; FormatSuccessResponse - Format success JSON response
; RCX = template, RDX = buffer, R8 = message string
; ---------------------------------------------------------------------------
FormatSuccessResponse PROC
    push rbx
    
    ; wsprintfA(dest, format, arg1)
    ; RCX = dest, RDX = format, R8 = arg1
    
    sub rsp, 32
    call wsprintfA
    add rsp, 32
    
    pop rbx
    ret
FormatSuccessResponse ENDP

; ---------------------------------------------------------------------------
; FormatHealthResponse - Format health check response
; RCX = template, RDX = buffer, R8 = size, R9 = modelLoaded
; ---------------------------------------------------------------------------
FormatHealthResponse PROC
    push rbx
    push r12
    push r13
    
    mov r12, rcx        ; Template
    mov r13, rdx        ; Buffer
    mov ebx, r8d        ; Size
    
    ; Convert boolean to string pointer for %s
    test r9d, r9d
    jz @@use_false
    lea r8, g_szTrue
    jmp @@format

@@use_false:
    lea r8, g_szFalse
    
@@format:
    mov rcx, r13        ; Dest
    mov rdx, r12        ; Format
    ; r8 already has string pointer
    
    sub rsp, 32
    call wsprintfA
    add rsp, 32
    
    pop r13
    pop r12
    pop rbx
    ret
FormatHealthResponse ENDP

; ---------------------------------------------------------------------------
; FormatToolResponse - Format tool execution response
; RCX = dest buffer, RDX = template, R8 = tool name, R9 = tool result
; ---------------------------------------------------------------------------
FormatToolResponse PROC
    push rbx
    push r12
    push r13
    
    mov r12, rcx        ; Dest
    mov r13, rdx        ; Template
    
    ; wsprintfA(dest, template, tool_name, result)
    ; RCX = dest, RDX = format, R8 = arg1, R9 = arg2
    
    mov rcx, r12        ; Dest
    mov rdx, r13        ; Format (template)
    ; R8 already has tool_name
    ; R9 already has tool_result
    
    sub rsp, 32
    call wsprintfA
    add rsp, 32
    
    pop r13
    pop r12
    pop rbx
    ret
FormatToolResponse ENDP

; ---------------------------------------------------------------------------
; ProxyToOllama - Fallback proxy to Ollama server on localhost:11434
; RCX = endpoint (e.g., "/api/chat"), RDX = request body (JSON), R8 = response buffer
; Returns: EAX = HTTP status code from Ollama, or 503 if Ollama unavailable
; Uses: curl.exe to proxy requests to Ollama
; ---------------------------------------------------------------------------
ProxyToOllama PROC
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    
    mov rsi, rcx        ; Endpoint
    mov rdi, rdx        ; Request body
    mov r12, r8         ; Response buffer
    
    ; Check if Ollama is running by attempting connection
    ; For now, return 503 Service Unavailable
    ; Full implementation would use curl/WinINet to proxy the request
    
    ; Construct error message indicating Ollama fallback
    mov rcx, r12        ; Dest buffer
    lea rdx, g_szErrorTemplate
    lea r8, g_szErrOllamaFallback
    mov r9d, 503        ; Service Unavailable
    
    sub rsp, 32
    call wsprintfA
    add rsp, 32
    
    mov eax, 503        ; HTTP_STATUS_SERVICE_UNAVAILABLE
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
ProxyToOllama ENDP

; ═════════════════════════════════════════════════════════════════════════════
; WORKER THREAD
; ═════════════════════════════════════════════════════════════════════════════

; ---------------------------------------------------------------------------
; WorkerThreadProc - HTTP request processing loop
; RCX = pServerContext (SERVER_CONTEXT*)
; ---------------------------------------------------------------------------
WorkerThreadProc PROC
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov r15, rcx        ; Server context
    
    ; Allocate request and response buffers
    sub rsp, 32
    mov rcx, g_HeapHandle
    xor edx, edx        ; Flags
    mov r8d, MAX_REQUEST_SIZE
    call HeapAlloc
    add rsp, 32
    test rax, rax
    jz @@thread_exit
    mov r12, rax        ; Request buffer
    
    sub rsp, 32
    mov rcx, g_HeapHandle
    xor edx, edx
    mov r8d, MAX_RESPONSE_SIZE
    call HeapAlloc
    add rsp, 32
    test rax, rax
    jz @@free_request
    mov r13, rax        ; Response buffer
    
    ; Thread main loop
@@request_loop:
    ; Check for shutdown event
    sub rsp, 32
    mov rcx, [r15].SERVER_CONTEXT.hShutdownEvent
    xor edx, edx        ; WAIT_TIMEOUT = 0
    call WaitForSingleObject
    add rsp, 32
    cmp eax, 0          ; WAIT_OBJECT_0 = shutdown requested
    je @@thread_cleanup
    
    ; Receive HTTP request from kernel
    sub rsp, 64
    mov rcx, [r15].SERVER_CONTEXT.hRequestQueue
    xor rdx, rdx        ; RequestId (0 = any)
    mov r8d, HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY
    mov r9, r12         ; pHttpRequest buffer
    mov qword ptr [rsp + 32], MAX_REQUEST_SIZE
    mov qword ptr [rsp + 40], 0
    mov qword ptr [rsp + 48], 0
    call HttpReceiveHttpRequest
    add rsp, 64
    
    ; Check if request received successfully
    test eax, eax
    jnz @@request_loop
    
    ; Handle the request
    mov rcx, r12        ; Request
    mov rdx, r13        ; Response buffer
    call HandleHttpRequest
    mov r14d, eax       ; Save status code
    
    ; Send HTTP response
    sub rsp, 1024
    mov rdi, rsp
    add rdi, 128         ; pResponse starts after shadow space
    
    ; Zero out response structure
    push rdi
    mov rcx, 160        ; 160 * 4 = 640 bytes
    xor eax, eax
    rep stosd
    pop rdi
    
    mov [rdi].HTTP_RESPONSE.StatusCode, r14w
    mov word ptr [rdi].HTTP_RESPONSE.Version.MajorVersion, 1
    mov word ptr [rdi].HTTP_RESPONSE.Version.MinorVersion, 1
    
    ; Set reason
    cmp r14w, 200
    jne @@not_OK
    lea rax, g_szOK
    jmp @@set_reason
@@not_OK:
    lea rax, g_szInternalError
@@set_reason:
    mov [rdi].HTTP_RESPONSE.pReason, rax
    sub rsp, 32
    mov rcx, rax
    call StrLen_ASCII
    add rsp, 32
    mov [rdi].HTTP_RESPONSE.ReasonLength, ax

    ; Set body
    sub rsp, 32
    mov rcx, r13
    call StrLen_ASCII
    add rsp, 32
    test rax, rax
    jz @@send_raw
    
    mov rbx, rax
    lea r8, [rsp + 800]
    mov [r8].HTTP_DATA_CHUNK.DataChunkType, 0  ; HttpDataChunkFromMemory
    mov [r8].HTTP_DATA_CHUNK.pBuffer, r13
    mov [r8].HTTP_DATA_CHUNK.BufferLength, ebx
    
    mov [rdi].HTTP_RESPONSE.EntityChunkCount, 1
    lea rax, [rsp + 800]
    mov [rdi].HTTP_RESPONSE.pEntityChunks, rax
    
@@send_raw:
    mov rcx, [r15].SERVER_CONTEXT.hRequestQueue
    mov rdx, [r12].HTTP_REQUEST.RequestId
    xor r8d, r8d        ; Flags
    mov r9, rdi         ; pResponse
    
    xor rax, rax
    mov qword ptr [rsp + 32], rax
    mov qword ptr [rsp + 40], rax
    mov qword ptr [rsp + 48], rax
    mov qword ptr [rsp + 56], rax
    mov qword ptr [rsp + 64], rax
    mov qword ptr [rsp + 72], rax
    call HttpSendHttpResponse
    
    add rsp, 1024
    
    ; Continue main loop
    jmp @@request_loop
    
@@thread_cleanup:
    ; Free buffers (R13 followed by R12)
    sub rsp, 32
    mov rcx, g_HeapHandle
    xor edx, edx
    mov r8, r13
    call HeapFree
    add rsp, 32
    
@@free_request:
    sub rsp, 32
    mov rcx, g_HeapHandle
    xor edx, edx
    mov r8, r12
    call HeapFree
    add rsp, 32
    
@@thread_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
WorkerThreadProc ENDP

; ═════════════════════════════════════════════════════════════════════════════
; SERVER LIFECYCLE
; ═════════════════════════════════════════════════════════════════════════════

; ---------------------------------------------------------------------------
; HttpServer_Initialize - Start the HTTP server
; RCX = port number
; Returns: EAX = NO_ERROR (0) on success
; ---------------------------------------------------------------------------
HttpServer_Initialize PROC
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    
    mov r12d, ecx       ; Port number
    
    ; Get process heap
    sub rsp, 32
    call GetProcessHeap
    add rsp, 32
    mov g_HeapHandle, rax
    
    ; Initialize HTTP Server API (Version 1.0)
    ; HttpInitialize(HTTPAPI_VERSION Version, ULONG Flags, PVOID pReserved)
    mov ecx, 00000001h  ; MajorVersion = 1, MinorVersion = 0
    mov edx, 1          ; HTTP_INITIALIZE_SERVER
    xor r8, r8          ; pReserved
    sub rsp, 32
    call HttpInitialize
    add rsp, 32
    test eax, eax
    jnz @@init_failed_log
    
    ; Create request queue (Version 1.0)
    ; HttpCreateHttpHandle(PHANDLE pRequestQueueHandle, ULONG Reserved)
    lea rcx, [g_ServerContext.hRequestQueue]
    xor edx, edx        ; Reserved
    sub rsp, 32
    call HttpCreateHttpHandle
    add rsp, 32
    test eax, eax
    jnz @@create_failed_log
    
    ; Create shutdown event
    sub rsp, 32
    xor rcx, rcx        ; Security
    mov edx, 1          ; ManualReset = TRUE
    mov r8d, 0          ; InitialState = FALSE (not signaled)
    xor r9d, r9d        ; Name = NULL
    call CreateEventW
    add rsp, 32
    test rax, rax
    jz @@event_failed_log
    mov g_ServerContext.hShutdownEvent, rax
    
    ; Build URL prefix string - "http://+:PORT/"
    sub rsp, 128        ; Buffer for URL
    lea rcx, [rsp]
    mov word ptr [rcx], 'h'
    mov word ptr [rcx+2], 't'
    mov word ptr [rcx+4], 't'
    mov word ptr [rcx+6], 'p'
    mov word ptr [rcx+8], ':'
    mov word ptr [rcx+10], '/'
    mov word ptr [rcx+12], '/'
    mov word ptr [rcx+14], '+'
    mov word ptr [rcx+16], ':'
    
        ; Convert port to wide string "15099"
        mov word ptr [rcx+18], '1'
        mov word ptr [rcx+20], '5'
        mov word ptr [rcx+22], '0'
        mov word ptr [rcx+24], '9'
        mov word ptr [rcx+26], '9'
        mov word ptr [rcx+28], '/'
        mov word ptr [rcx+30], 0   ; Null terminate
    
    ; Add URL to HTTP server
    ; HttpAddUrl(HANDLE hRequestQueue, PCWSTR pFullyQualifiedUrl, PVOID pReserved)
    mov rcx, [g_ServerContext.hRequestQueue]
    lea rdx, [rsp]      ; URL prefix (wide string!)
    xor r8, r8          ; pReserved
    sub rsp, 32
    call HttpAddUrl
    add rsp, 32
    add rsp, 128        ; Clean up URL buffer
    test eax, eax
    jnz @@add_url_failed_log
    
    ; Initialize critical section for thread safety
    sub rsp, 32
    lea rcx, g_ServerContext.CriticalSection
    call InitializeCriticalSection
    add rsp, 32
    
    ; Store port number
    mov g_ServerContext.Port, r12d
    mov g_ServerContext.IsRunning, 1
    
    ; Create worker threads
    mov ebx, 0          ; Thread counter
    
@@create_threads:
    cmp ebx, MAX_WORKER_THREADS
    jge @@threads_created
    
    sub rsp, 64
    xor rcx, rcx        ; Security
    mov edx, 0          ; StackSize (default)
    lea r8, WorkerThreadProc
    lea r9, g_ServerContext
    mov qword ptr [rsp + 32], 0 ; CreationFlags
    mov qword ptr [rsp + 40], 0 ; lpThreadId
    call CreateThread
    add rsp, 64
    
    mov [g_ServerContext.WorkerThreads + rbx*8], rax
    test rax, rax
    jz @@thread_create_failed
    
    inc ebx
    jmp @@create_threads
    
@@threads_created:
    mov g_ServerContext.ThreadCount, ebx
    
    ; Log success
    sub rsp, 32
    lea rcx, g_szLogSuccess
    call OutputDebugStringA
    add rsp, 32
    
    xor eax, eax        ; Return success
    jmp @@init_done

@@init_failed_log:
    mov ebx, eax        ; Save error code in EBX (non-volatile)
    mov r8d, ebx        ; arg 3 for wsprintfA
    sub rsp, 128
    lea rcx, [rsp + 64] ; Buffer
    lea rdx, g_szLogInitFailed
    call wsprintfA
    lea rcx, [rsp + 64]
    call OutputDebugStringA
    add rsp, 128
    mov eax, ebx
    jmp @@init_done

@@create_failed_log:
    mov ebx, eax        ; Save error code
    mov r8d, ebx
    sub rsp, 128
    lea rcx, [rsp + 64]
    lea rdx, g_szLogCreateFailed
    call wsprintfA
    lea rcx, [rsp + 64]
    call OutputDebugStringA
    add rsp, 128
    mov eax, ebx
    jmp @@init_done

@@event_failed_log:
    call GetLastError
    mov ebx, eax
    mov r8d, ebx
    sub rsp, 128
    lea rcx, [rsp + 64]
    lea rdx, g_szLogEventFailed
    call wsprintfA
    lea rcx, [rsp + 64]
    call OutputDebugStringA
    add rsp, 128
    mov eax, ebx
    jmp @@init_done

@@add_url_failed_log:
    mov ebx, eax
    mov r8d, ebx
    sub rsp, 128
    lea rcx, [rsp + 64]
    lea rdx, g_szLogAddUrlFailed
    call wsprintfA
    lea rcx, [rsp + 64]
    call OutputDebugStringA
    add rsp, 128
    mov eax, ebx
    jmp @@init_done

@@thread_create_failed:
    call GetLastError
    mov ebx, eax
    mov r8d, ebx
    sub rsp, 128
    lea rcx, [rsp + 64]
    lea rdx, g_szLogThreadFailed
    call wsprintfA
    lea rcx, [rsp + 64]
    call OutputDebugStringA
    add rsp, 128
    mov eax, ebx
    jmp @@init_done

@@init_done:
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
HttpServer_Initialize ENDP

; ---------------------------------------------------------------------------
; HttpServer_Shutdown - Stop the HTTP server
; Returns: EAX = NO_ERROR (0) on success
; ---------------------------------------------------------------------------
HttpServer_Shutdown PROC
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    
    mov g_ServerContext.IsRunning, 0
    
    ; Signal shutdown event
    sub rsp, 32
    mov rcx, g_ServerContext.hShutdownEvent
    call SetEvent
    add rsp, 32
    
    ; Wait for worker threads to finish
    mov ecx, g_ServerContext.ThreadCount
    xor ebx, ebx
    
@@wait_threads:
    cmp ebx, ecx
    jge @@threads_done
    
    sub rsp, 32
    mov rcx, [g_ServerContext.WorkerThreads + rbx*8]
    mov edx, 5000       ; 5 second timeout
    call WaitForSingleObject
    add rsp, 32
    
    ; Close thread handle
    sub rsp, 32
    mov rcx, [g_ServerContext.WorkerThreads + rbx*8]
    call CloseHandle
    add rsp, 32
    
    inc ebx
    jmp @@wait_threads
    
@@threads_done:
    ; Clean up HTTP API resources
    sub rsp, 32
    mov rcx, g_ServerContext.hRequestQueue
    call HttpRemoveUrl
    add rsp, 32
    
    sub rsp, 32
    mov rcx, g_ServerContext.hRequestQueue
    call CloseHandle
    add rsp, 32
    
    ; Terminate HTTP API
    sub rsp, 32
    call HttpTerminate
    add rsp, 32
    
    ; Delete critical section
    sub rsp, 32
    lea rcx, g_ServerContext.CriticalSection
    call DeleteCriticalSection
    add rsp, 32
    
    xor eax, eax
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
HttpServer_Shutdown ENDP

; ---------------------------------------------------------------------------
; HttpServer_IsRunning - Check if server is running
; Returns: AL = 1 if running, 0 if stopped
; ---------------------------------------------------------------------------
HttpServer_IsRunning PROC
    mov al, g_ServerContext.IsRunning
    ret
HttpServer_IsRunning ENDP

; ---------------------------------------------------------------------------
; HttpServer_LoadModel - Load inference model
; RCX = path to model file (ASCII)
; Returns: EAX = 0 on success
; ---------------------------------------------------------------------------
HttpServer_LoadModel PROC
    push rbx
    
    ; Call inference engine to load model
    call InferenceEngine_LoadModel
    test eax, eax
    jnz @@load_failed
    
    ; Mark model as loaded
    mov g_ServerContext.ModelLoaded, 1
    xor eax, eax
    jmp @@load_done
    
@@load_failed:
    mov eax, 1
    
@@load_done:
    pop rbx
    ret
HttpServer_LoadModel ENDP

; ---------------------------------------------------------------------------
; HttpServer_GetStatus - Get server status
; Returns: RAX = (RequestCount << 32) | ResponseCount
; ---------------------------------------------------------------------------
HttpServer_GetStatus PROC
    xor rax, rax                    ; Ensure RAX is clean
    mov rcx, g_ServerContext.RequestCount
    shl rcx, 32
    mov rdx, g_ServerContext.ResponseCount
    or rax, rcx
    or rax, rdx
    ret
HttpServer_GetStatus ENDP

END
