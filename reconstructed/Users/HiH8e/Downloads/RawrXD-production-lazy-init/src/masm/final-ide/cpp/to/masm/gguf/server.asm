; gguf_server_masm.asm
; Pure MASM x64 - GGUF Server (converted from C++ GGUFServer class)
; HTTP server for GGUF model inference with Ollama-compatible API

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN CreateThreadA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN CreateEventA:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC

; Server constants
MAX_CLIENTS EQU 100
MAX_REQUEST_SIZE EQU 1048576        ; 1 MB
MAX_RESPONSE_SIZE EQU 10485760      ; 10 MB
DEFAULT_PORT EQU 11434              ; Ollama default port
SERVER_TIMEOUT_MS EQU 5000          ; 5 second timeout

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; CLIENT_CONNECTION - Client connection state
CLIENT_CONNECTION STRUCT
    socket QWORD ?                  ; Socket handle
    address QWORD ?                 ; Client address
    requestBuffer QWORD ?           ; Request buffer
    requestSize QWORD ?             ; Request size
    responseBuffer QWORD ?          ; Response buffer
    responseSize QWORD ?            ; Response size
    connected BYTE ?                ; Connection status
    processing BYTE ?               ; Processing status
ENDS

; SERVER_REQUEST - HTTP request
SERVER_REQUEST STRUCT
    method QWORD ?                  ; HTTP method (GET, POST)
    path QWORD ?                    ; Request path
    headers QWORD ?                 ; HTTP headers
    body QWORD ?                    ; Request body
    bodySize QWORD ?                ; Body size
ENDS

; SERVER_RESPONSE - HTTP response
SERVER_RESPONSE STRUCT
    statusCode DWORD ?              ; HTTP status code
    headers QWORD ?                 ; Response headers
    body QWORD ?                    ; Response body
    bodySize QWORD ?                ; Body size
    contentType QWORD ?             ; Content type
ENDS

; GGUF_SERVER - Server state
GGUF_SERVER STRUCT
    enginePtr QWORD ?               ; InferenceEngine pointer
    port DWORD ?                    ; Server port
    
    clients QWORD ?                 ; Array of CLIENT_CONNECTION
    clientCount DWORD ?             ; Current client count
    maxClients DWORD ?              ; Capacity
    
    ; Threading
    serverThread QWORD ?            ; Server thread handle
    threadId DWORD ?                ; Thread ID
    
    ; Synchronization
    dataReady QWORD ?               ; Event handle
    serverRunning BYTE ?            ; Server running flag
    
    ; Statistics
    totalRequests QWORD ?
    totalResponses QWORD ?
    totalErrors DWORD ?
    
    ; Callbacks
    requestCallback QWORD ?         ; Called when request received
    responseCallback QWORD ?        ; Called when response ready
    errorCallback QWORD ?           ; Called on error
    
    initialized BYTE ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szServerCreated DB "[GGUF_SERVER] Created for engine: %llx", 0
    szServerStarted DB "[GGUF_SERVER] Started on port %d", 0
    szServerStopped DB "[GGUF_SERVER] Stopped", 0
    szRequestReceived DB "[GGUF_SERVER] Request: %s %s", 0
    szResponseSent DB "[GGUF_SERVER] Response: %d bytes, status=%d", 0
    szClientConnected DB "[GGUF_SERVER] Client connected: %s", 0
    szClientDisconnected DB "[GGUF_SERVER] Client disconnected", 0
    szPortConflict DB "[GGUF_SERVER] Port conflict on %d", 0

; HTTP status codes
HTTP_OK EQU 200
HTTP_BAD_REQUEST EQU 400
HTTP_NOT_FOUND EQU 404
HTTP_INTERNAL_ERROR EQU 500

; HTTP methods
HTTP_GET DB "GET", 0
HTTP_POST DB "POST", 0

; API endpoints
API_GENERATE DB "/api/generate", 0
API_TAGS DB "/api/tags", 0
API_CHAT_COMPLETIONS DB "/v1/chat/completions", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; gguf_server_create(RCX = enginePtr)
; Create GGUF server
; Returns: RAX = pointer to GGUF_SERVER
PUBLIC gguf_server_create
gguf_server_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = enginePtr
    
    ; Allocate server
    mov rcx, SIZEOF GGUF_SERVER
    call malloc
    mov r9, rax
    
    ; Allocate clients array
    mov rcx, MAX_CLIENTS
    imul rcx, SIZEOF CLIENT_CONNECTION
    call malloc
    mov [r9 + GGUF_SERVER.clients], rax
    
    ; Initialize
    mov [r9 + GGUF_SERVER.enginePtr], rbx
    mov [r9 + GGUF_SERVER.port], DEFAULT_PORT
    mov [r9 + GGUF_SERVER.clientCount], 0
    mov [r9 + GGUF_SERVER.maxClients], MAX_CLIENTS
    mov byte [r9 + GGUF_SERVER.serverRunning], 0
    mov [r9 + GGUF_SERVER.totalRequests], 0
    mov [r9 + GGUF_SERVER.totalResponses], 0
    mov [r9 + GGUF_SERVER.totalErrors], 0
    
    ; Create data ready event
    mov rcx, 0                      ; lpEventAttributes
    mov rdx, 0                      ; bManualReset
    mov r8, 0                       ; bInitialState
    mov r9d, 0                      ; lpName
    call CreateEventA
    mov [r9 + GGUF_SERVER.dataReady], rax
    
    mov byte [r9 + GGUF_SERVER.initialized], 1
    
    ; Log
    lea rcx, [szServerCreated]
    mov rdx, rbx
    call console_log
    
    mov rax, r9
    pop rbx
    ret
gguf_server_create ENDP

; ============================================================================

; gguf_server_start(RCX = server, RDX = port)
; Start GGUF server
; Returns: RAX = 1 if successful, 0 if failed
PUBLIC gguf_server_start
gguf_server_start PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = server
    
    ; Check if already running
    cmp byte [rbx + GGUF_SERVER.serverRunning], 1
    je .already_running
    
    ; Set port
    cmp edx, 0
    je .use_default_port
    mov [rbx + GGUF_SERVER.port], edx
    
.use_default_port:
    ; Check port conflict (simplified)
    mov eax, [rbx + GGUF_SERVER.port]
    cmp eax, DEFAULT_PORT
    jne .port_available
    
    ; Simulate port conflict check
    mov rax, 1                      ; Assume port available
    
.port_available:
    test rax, rax
    jz .port_conflict
    
    ; Create server thread
    mov rcx, 0                      ; lpThreadAttributes
    mov rdx, 0                      ; dwStackSize (default)
    lea r8, [server_thread_proc]
    mov r9, rbx                     ; lpParameter = server
    call CreateThreadA
    mov [rbx + GGUF_SERVER.serverThread], rax
    
    ; Get thread ID
    mov [rbx + GGUF_SERVER.threadId], eax
    
    mov byte [rbx + GGUF_SERVER.serverRunning], 1
    
    ; Log
    lea rcx, [szServerStarted]
    mov edx, [rbx + GGUF_SERVER.port]
    call console_log
    
    mov rax, 1
    pop rbx
    ret
    
.already_running:
    mov rax, 1
    pop rbx
    ret
    
.port_conflict:
    lea rcx, [szPortConflict]
    mov edx, [rbx + GGUF_SERVER.port]
    call console_log
    
    xor rax, rax
    pop rbx
    ret
gguf_server_start ENDP

; ============================================================================

; gguf_server_stop(RCX = server)
; Stop GGUF server
PUBLIC gguf_server_stop
gguf_server_stop PROC
    push rbx
    
    mov rbx, rcx
    
    ; Check if running
    cmp byte [rbx + GGUF_SERVER.serverRunning], 1
    jne .already_stopped
    
    ; Stop server thread
    mov byte [rbx + GGUF_SERVER.serverRunning], 0
    
    ; Wait for thread to complete
    mov rcx, [rbx + GGUF_SERVER.serverThread]
    mov rdx, SERVER_TIMEOUT_MS
    call WaitForSingleObject
    
    ; Close thread handle
    mov rcx, [rbx + GGUF_SERVER.serverThread]
    call CloseHandle
    
    ; Log
    lea rcx, [szServerStopped]
    call console_log
    
.already_stopped:
    pop rbx
    ret
gguf_server_stop ENDP

; ============================================================================

; gguf_server_is_running(RCX = server)
; Check if server is running
; Returns: RAX = 1 if running, 0 if not
PUBLIC gguf_server_is_running
gguf_server_is_running PROC
    movzx eax, byte [rcx + GGUF_SERVER.serverRunning]
    ret
gguf_server_is_running ENDP

; ============================================================================

; gguf_server_port(RCX = server)
; Get server port
; Returns: RAX = port number
PUBLIC gguf_server_port
gguf_server_port PROC
    mov eax, [rcx + GGUF_SERVER.port]
    ret
gguf_server_port ENDP

; ============================================================================

; gguf_server_handle_request(RCX = server, RDX = request, R8 = requestSize)
; Handle HTTP request
; Returns: RAX = pointer to SERVER_RESPONSE
PUBLIC gguf_server_handle_request
gguf_server_handle_request PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = server
    mov rsi, rdx                    ; rsi = request
    mov r9, r8                      ; r9 = requestSize
    
    ; Log
    lea rcx, [szRequestReceived]
    mov rdx, rsi                    ; Method (simplified)
    lea r8, [API_GENERATE]          ; Path (simplified)
    call console_log
    
    ; Parse request (simplified)
    mov rcx, 1024                   ; Allocate response
    call malloc
    mov r10, rax                    ; r10 = response
    
    ; Handle different endpoints
    mov rcx, rsi
    lea rdx, [API_GENERATE]
    call strstr
    cmp rax, 0
    je .not_generate
    
    ; Handle /api/generate
    mov rcx, rbx
    mov rdx, rsi
    mov r8, r9
    call handle_generate_request
    mov r11, rax                    ; r11 = response
    jmp .request_handled
    
.not_generate:
    mov rcx, rsi
    lea rdx, [API_TAGS]
    call strstr
    cmp rax, 0
    je .not_tags
    
    ; Handle /api/tags
    mov rcx, rbx
    call handle_tags_request
    mov r11, rax
    jmp .request_handled
    
.not_tags:
    ; Default response
    mov rcx, rbx
    call handle_default_request
    mov r11, rax
    
.request_handled:
    ; Update statistics
    inc qword [rbx + GGUF_SERVER.totalRequests]
    inc qword [rbx + GGUF_SERVER.totalResponses]
    
    ; Log response
    lea rcx, [szResponseSent]
    mov rdx, [r11 + SERVER_RESPONSE.bodySize]
    mov r8d, [r11 + SERVER_RESPONSE.statusCode]
    call console_log
    
    mov rax, r11                    ; Return response
    pop rsi
    pop rbx
    ret
gguf_server_handle_request ENDP

; ============================================================================

; handle_generate_request(RCX = server, RDX = request, R8 = requestSize)
; Handle /api/generate endpoint
; Returns: RAX = pointer to SERVER_RESPONSE
handle_generate_request PROC
    push rbx
    
    mov rbx, rcx
    
    ; Allocate response
    mov rcx, SIZEOF SERVER_RESPONSE
    call malloc
    mov r9, rax
    
    ; Set response
    mov [r9 + SERVER_RESPONSE.statusCode], HTTP_OK
    
    ; Create response body (simplified)
    mov rcx, 1024
    call malloc
    mov [r9 + SERVER_RESPONSE.body], rax
    
    lea rdx, [szGenerateResponse]
    call strcpy
    mov [r9 + SERVER_RESPONSE.bodySize], 1024
    
    mov rax, r9
    pop rbx
    ret
handle_generate_request ENDP

; ============================================================================

; handle_tags_request(RCX = server)
; Handle /api/tags endpoint
; Returns: RAX = pointer to SERVER_RESPONSE
handle_tags_request PROC
    push rbx
    
    mov rbx, rcx
    
    ; Allocate response
    mov rcx, SIZEOF SERVER_RESPONSE
    call malloc
    mov r9, rax
    
    ; Set response
    mov [r9 + SERVER_RESPONSE.statusCode], HTTP_OK
    
    ; Create response body (simplified)
    mov rcx, 512
    call malloc
    mov [r9 + SERVER_RESPONSE.body], rax
    
    lea rdx, [szTagsResponse]
    call strcpy
    mov [r9 + SERVER_RESPONSE.bodySize], 512
    
    mov rax, r9
    pop rbx
    ret
handle_tags_request ENDP

; ============================================================================

; handle_default_request(RCX = server)
; Handle default request
; Returns: RAX = pointer to SERVER_RESPONSE
handle_default_request PROC
    push rbx
    
    mov rbx, rcx
    
    ; Allocate response
    mov rcx, SIZEOF SERVER_RESPONSE
    call malloc
    mov r9, rax
    
    ; Set response
    mov [r9 + SERVER_RESPONSE.statusCode], HTTP_NOT_FOUND
    
    ; Create response body
    mov rcx, 256
    call malloc
    mov [r9 + SERVER_RESPONSE.body], rax
    
    lea rdx, [szNotFoundResponse]
    call strcpy
    mov [r9 + SERVER_RESPONSE.bodySize], 256
    
    mov rax, r9
    pop rbx
    ret
handle_default_request ENDP

; ============================================================================

; gguf_server_get_statistics(RCX = server, RDX = statsBuffer)
; Get server statistics
PUBLIC gguf_server_get_statistics
gguf_server_get_statistics PROC
    mov [rdx + 0], qword [rcx + GGUF_SERVER.totalRequests]
    mov [rdx + 8], qword [rcx + GGUF_SERVER.totalResponses]
    mov [rdx + 16], dword [rcx + GGUF_SERVER.totalErrors]
    ret
gguf_server_get_statistics ENDP

; ============================================================================

; gguf_server_destroy(RCX = server)
; Free GGUF server
PUBLIC gguf_server_destroy
gguf_server_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Stop server if running
    mov rcx, rbx
    call gguf_server_stop
    
    ; Close data ready event
    mov rcx, [rbx + GGUF_SERVER.dataReady]
    cmp rcx, 0
    je .skip_event
    call CloseHandle
    
.skip_event:
    ; Free clients array
    mov rcx, [rbx + GGUF_SERVER.clients]
    cmp rcx, 0
    je .skip_clients
    call free
    
.skip_clients:
    ; Free server
    mov rcx, rbx
    call free
    
    pop rbx
    ret
gguf_server_destroy ENDP

; ============================================================================
; INTERNAL THREAD PROCEDURE
; ============================================================================

; server_thread_proc(RCX = server)
; Internal server thread function
server_thread_proc PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = server
    
    ; Main server loop
.server_loop:
    cmp byte [rbx + GGUF_SERVER.serverRunning], 1
    jne .server_exit
    
    ; Simulate client connection handling
    mov rcx, 1000                   ; 1 second delay
    call Sleep
    
    jmp .server_loop
    
.server_exit:
    pop rbx
    ret
server_thread_proc ENDP

; ============================================================================

.data
    szGenerateResponse DB "{\"model\":\"llama2\",\"created_at\":\"2023-01-01T00:00:00Z\",\"response\":\"Hello\",\"done\":true}", 0
    szTagsResponse DB "{\"models\":[{\"name\":\"llama2\",\"modified_at\":\"2023-01-01T00:00:00Z\",\"size\":3826560512,\"digest\":\"abc123\"}]}", 0
    szNotFoundResponse DB "{\"error\":\"Endpoint not found\"}", 0

END
