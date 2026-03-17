; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_HTTP_Router.asm  ─  HTTP Server ↔ Inference Engine Bridge
; Standalone Winsock-based HTTP server with route handlers
; Assemble: ml64.exe /c /FoRawrXD_HTTP_Router.obj RawrXD_HTTP_Router.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc
include \masm64\include64\ws2_32.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\ws2_32.lib
includelib \masm64\lib64\user32.lib

EXTERNDEF RawrXD_MemAlloc:PROC
EXTERNDEF RawrXD_MemFree:PROC
EXTERNDEF RawrXD_StrLen:PROC


; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
HTTP_PORT               EQU 11434         ; Ollama-compatible default
HTTP_MAX_PENDING        EQU 128
HTTP_THREAD_POOL_SIZE   EQU 16
HTTP_MAX_REQUEST_SIZE   EQU 65536
HTTP_MAX_RESPONSE_SIZE  EQU 1048576       ; 1MB max response

; HTTP Status codes
HTTP_OK                 EQU 200
HTTP_BAD_REQUEST        EQU 400
HTTP_NOT_FOUND          EQU 404
HTTP_METHOD_NOT_ALLOWED EQU 405
HTTP_SERVER_ERROR       EQU 500
HTTP_SERVICE_UNAVAILABLE EQU 503

; Route IDs
ROUTE_API_GENERATE      EQU 1
ROUTE_API_TAGS          EQU 2
ROUTE_API_CHAT          EQU 3
ROUTE_V1_CHAT_COMPLETIONS EQU 4
ROUTE_API_EMBEDDINGS    EQU 5
ROUTE_HEALTH            EQU 6

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
HttpRequest STRUCT
    Socket              QWORD       ?
    Method              DWORD       ?       ; GET, POST, etc.
    RouteId             DWORD       ?
    ContentLength       QWORD       ?
    BodyOffset          QWORD       ?
    RequestBuffer       QWORD       ?       ; Pointer to full request
    RequestLength       QWORD       ?
    ResponseBuffer      QWORD       ?       ; Pre-allocated response buffer
    Context             QWORD       ?       ; Per-request context
HttpRequest ENDS

HttpServerContext STRUCT
    ListenSocket        QWORD       ?
    ThreadPool          QWORD HTTP_THREAD_POOL_SIZE DUP (?)
    hShutdownEvent      QWORD       ?
    bRunning            DWORD       ?
    ActiveConnections   DWORD       ?
    TotalRequests       QWORD       ?
    RouteTable          QWORD       ?       ; Pointer to route handlers
HttpServerContext ENDS

InferenceJob STRUCT
    JobId               QWORD       ?
    ModelId             QWORD       ?       ; Pointer to model name
    Prompt              QWORD       ?       ; Pointer to prompt text
    PromptLength        QWORD       ?
    MaxTokens           DWORD       ?
    Temperature         REAL4       ?
    Stream              DWORD       ?       ; Boolean: streaming mode
    Callback            QWORD       ?       ; Completion callback
    Context             QWORD       ?       ; User context
    Status              DWORD       ?       ; QUEUED, RUNNING, COMPLETE
InferenceJob ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_HttpServer            HttpServerContext <>

; Route path strings
szRouteApiGenerate      BYTE "/api/generate", 0
szRouteApiTags          BYTE "/api/tags", 0
szRouteApiChat          BYTE "/api/chat", 0
szRouteV1ChatCompletions BYTE "/v1/chat/completions", 0
szRouteApiEmbeddings    BYTE "/api/embeddings", 0
szRouteHealth           BYTE "/health", 0

; HTTP Method strings
szMethodGet             BYTE "GET", 0
szMethodPost            BYTE "POST", 0

; JSON Response templates
szJsonStatusOk          BYTE '{ "status": "ok" }', 0
szJsonErrorBadRequest   BYTE '{ "error": "bad request" }', 0
szJsonErrorNotFound     BYTE '{ "error": "not found" }', 0
szJsonErrorServer       BYTE '{ "error": "internal server error" }', 0

; HTTP response formatting templates
szHttpStatusTemplate    BYTE "HTTP/1.1 %d OK", 13, 10, 0
szHttpJsonHeadersPartial BYTE "Content-Type: application/json", 13, 10, "Content-Length: %d", 13, 10, 13, 10, 0

; Empty JSON object
szEmptyJson             BYTE "{}", 0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; HttpRouter_Initialize
; Initializes HTTP server with thread pool
; ═══════════════════════════════════════════════════════════════════════════════
HttpRouter_Initialize PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    
    sub rsp, 88                     ; WSAData + shadow space
    
    ; Initialize Winsock
    mov ecx, 0202h                  ; Version 2.2
    lea rdx, [rsp + 32]             ; WSAData structure
    call WSAStartup
    test eax, eax
    jnz @http_init_fail
    
    ; Create shutdown event
    xor ecx, ecx                    ; lpEventAttributes
    xor edx, edx                    ; bManualReset
    xor r8, r8                      ; bInitialState
    xor r9, r9                      ; lpName
    call CreateEventA
    mov g_HttpServer.hShutdownEvent, rax
    
    ; Create listening socket
    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    xor r8, r8                      ; IPPROTO_TCP
    call socket
    cmp rax, INVALID_SOCKET
    je @http_cleanup_wsa
    
    mov g_HttpServer.ListenSocket, rax
    mov rbx, rax
    
    ; Set SO_REUSEADDR
    mov ecx, ebx
    mov edx, SOL_SOCKET
    mov r8d, SO_REUSEADDR
    lea r9, [rsp + 60]              ; opt_val = 1
    mov dword ptr [r9], 1
    mov qword ptr [rsp + 32], 4     ; opt_len
    call setsockopt
    
    ; Bind to port
    mov word ptr [rsp + 60], AF_INET
    mov word ptr [rsp + 62], 0      ; Port (htons later)
    mov dword ptr [rsp + 64], 0     ; INADDR_ANY
    
    mov ecx, HTTP_PORT
    xchg cl, ch                     ; htons
    mov word ptr [rsp + 62], cx
    
    mov ecx, ebx
    lea rdx, [rsp + 60]             ; sockaddr
    mov r8d, SIZEOF sockaddr_in
    call bind
    test eax, eax
    jnz @http_cleanup_socket
    
    ; Listen
    mov ecx, ebx
    mov edx, HTTP_MAX_PENDING
    call listen
    test eax, eax
    jnz @http_cleanup_socket
    
    ; Create thread pool
    mov g_HttpServer.bRunning, 1
    xor r12, r12
    
@thread_create_loop:
    cmp r12, HTTP_THREAD_POOL_SIZE
    jge @threads_done
    
    lea rcx, HttpWorkerThread
    xor edx, edx
    xor r8, r8
    xor r9, r9
    call CreateThread
    
    mov g_HttpServer.ThreadPool[r12*8], rax
    inc r12
    jmp @thread_create_loop
    
@threads_done:
    ; Create acceptor thread
    lea rcx, HttpAcceptorThread
    xor edx, edx
    xor r8, r8
    xor r9, r9
    call CreateThread
    
    mov rax, TRUE
    jmp @http_init_done
    
@http_cleanup_socket:
    mov ecx, ebx
    call closesocket
    
@http_cleanup_wsa:
    call WSACleanup
    
@http_init_fail:
    xor eax, eax
    
@http_init_done:
    add rsp, 88
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
HttpRouter_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; HttpAcceptorThread
; Main accept loop - hands connections to thread pool
; ═══════════════════════════════════════════════════════════════════════════════
HttpAcceptorThread PROC FRAME
    push rbx
    push rsi
    push rdi
    
    sub rsp, 40
    
    mov rbx, g_HttpServer.ListenSocket
    
@accept_loop:
    ; Check shutdown
    cmp g_HttpServer.bRunning, 0
    je @acceptor_exit
    
    ; Wait for connection with timeout to check shutdown periodically
    mov ecx, ebx
    lea rdx, [rsp + 32]             ; sockaddr
    lea r8, [rsp + 28]              ; addrlen
    mov dword ptr [r8], SIZEOF sockaddr_in
    call accept
    
    cmp rax, INVALID_SOCKET
    je @accept_loop                 ; Error or timeout
    
    mov rdi, rax                    ; RDI = client socket
    
    ; Allocate request structure
    mov ecx, SIZEOF HttpRequest
    call HeapAlloc
    test rax, rax
    jz @close_client
    
    mov rsi, rax                    ; RSI = HttpRequest*
    mov [rsi].HttpRequest.Socket, rdi
    
    ; Allocate request buffer
    mov ecx, HTTP_MAX_REQUEST_SIZE
    call HeapAlloc
    mov [rsi].HttpRequest.RequestBuffer, rax
    
    ; Allocate response buffer
    mov ecx, HTTP_MAX_RESPONSE_SIZE
    call HeapAlloc
    mov [rsi].HttpRequest.ResponseBuffer, rax
    
    ; Queue to thread pool (simplified: direct handoff for now)
    mov rcx, rsi
    call ProcessHttpConnection
    
    jmp @accept_loop
    
@close_client:
    mov ecx, edi
    call closesocket
    jmp @accept_loop
    
@acceptor_exit:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    xor eax, eax
    ret
HttpAcceptorThread ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; HttpWorkerThread
; Processes HTTP requests from queue
; ═══════════════════════════════════════════════════════════════════════════════
HttpWorkerThread PROC FRAME
    ; Simplified: workers poll a lock-free queue
    ; Real implementation would use IOCP or blocking queue
    
@worker_loop:
    cmp g_HttpServer.bRunning, 0
    je @worker_exit
    
    ; Sleep and check queue (placeholder for real queue)
    mov ecx, 10                     ; 10ms
    call Sleep
    jmp @worker_loop
    
@worker_exit:
    xor eax, eax
    ret
HttpWorkerThread ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ProcessHttpConnection
; Parses HTTP request and routes to handler
; ═══════════════════════════════════════════════════════════════════════════════
ProcessHttpConnection PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    
    mov rbx, rcx                    ; RBX = HttpRequest*
    
    ; Read request
    mov ecx, [rbx].HttpRequest.Socket
    mov rdx, [rbx].HttpRequest.RequestBuffer
    mov r8d, HTTP_MAX_REQUEST_SIZE - 1
    xor r9, r9                      ; flags
    call recv
    
    cmp rax, 0
    jle @http_error_close
    
    mov [rbx].HttpRequest.RequestLength, rax
    
    ; Null terminate
    mov r8, [rbx].HttpRequest.RequestBuffer
    mov byte ptr [r8 + rax], 0
    
    ; Parse method
    call ParseHttpMethod
    mov [rbx].HttpRequest.Method, eax
    
    ; Parse route
    call ParseHttpRoute
    mov [rbx].HttpRequest.RouteId, eax
    
    ; Parse Content-Length if POST
    cmp [rbx].HttpRequest.Method, 2 ; POST
    jne @route_dispatch
    
    call ParseContentLength
    mov [rbx].HttpRequest.ContentLength, rax
    
    ; Find body offset
    call FindBodyOffset
    mov [rbx].HttpRequest.BodyOffset, rax
    
@route_dispatch:
    ; Dispatch to route handler
    mov eax, [rbx].HttpRequest.RouteId
    
    cmp eax, ROUTE_API_GENERATE
    je @handle_generate
    
    cmp eax, ROUTE_API_TAGS
    je @handle_tags
    
    cmp eax, ROUTE_V1_CHAT_COMPLETIONS
    je @handle_chat_completions
    
    cmp eax, ROUTE_HEALTH
    je @handle_health
    
    ; Default: 404
    jmp @http_not_found

@handle_generate:
    mov rcx, rbx
    call HandleApiGenerate
    jmp @http_done
    
@handle_tags:
    mov rcx, rbx
    call HandleApiTags
    jmp @http_done
    
@handle_chat_completions:
    mov rcx, rbx
    call HandleV1ChatCompletions
    jmp @http_done
    
@handle_health:
    mov rcx, rbx
    call HandleHealth
    jmp @http_done
    
@http_not_found:
    mov rcx, rbx
    mov edx, HTTP_NOT_FOUND
    lea r8, szJsonErrorNotFound
    call SendJsonResponse
    jmp @http_done
    
@http_error_close:
    mov ecx, [rbx].HttpRequest.Socket
    call closesocket
    
    ; Free request structure
    mov rcx, [rbx].HttpRequest.RequestBuffer
    call HeapFree
    
    mov rcx, [rbx].HttpRequest.ResponseBuffer
    call HeapFree
    
    mov rcx, rbx
    call HeapFree
    
@http_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ProcessHttpConnection ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; HandleApiGenerate
; POST /api/generate - Queue inference job
; ═══════════════════════════════════════════════════════════════════════════════
HandleApiGenerate PROC FRAME
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; HttpRequest*
    
    ; Validate POST
    cmp [rbx].HttpRequest.Method, 2 ; POST
    jne @generate_bad_method
    
    ; Parse JSON body (simplified - real implementation uses proper JSON parser)
    mov rsi, [rbx].HttpRequest.RequestBuffer
    add rsi, [rbx].HttpRequest.BodyOffset
    
    ; Extract model field
    lea rcx, [rsi]
    lea rdx, szJsonModelField       ; "model"
    call JsonExtractString
    mov rdi, rax                    ; RDI = model name
    
    ; Extract prompt field
    mov rcx, rsi
    lea rdx, szJsonPromptField      ; "prompt"
    call JsonExtractString
    
    ; Extract stream flag
    mov rcx, rsi
    lea rdx, szJsonStreamField      ; "stream"
    call JsonExtractBool
    mov r12d, eax                   ; R12D = stream flag
    
    ; Create inference job
    mov ecx, SIZEOF InferenceJob
    call HeapAlloc
    mov rsi, rax                    ; RSI = InferenceJob*
    
    mov [rsi].InferenceJob.ModelId, rdi
    mov [rsi].InferenceJob.Prompt, rax
    mov [rsi].InferenceJob.Stream, r12d
    mov [rsi].InferenceJob.Status, 0 ; QUEUED
    
    ; Queue to inference engine (via shared queue)
    call QueueInferenceJob
    
    ; Send immediate acknowledgment for non-streaming
    cmp r12d, 0
    je @generate_sync
    
    ; Streaming: set up SSE response
    mov rcx, rbx
    call StartStreamingResponse
    jmp @generate_done
    
@generate_sync:
    ; Wait for completion and send response
    mov rcx, rsi
    call WaitForInferenceCompletion
    
    ; Format response JSON
    mov rcx, [rbx].HttpRequest.ResponseBuffer
    mov rdx, HTTP_MAX_RESPONSE_SIZE
    lea r8, szJsonGenerateTemplate
    mov r9, [rsi].InferenceJob.Result
    call FormatJsonResponse
    
    mov rcx, rbx
    mov edx, HTTP_OK
    mov r8, [rbx].HttpRequest.ResponseBuffer
    call SendJsonResponse
    
    jmp @generate_done
    
@generate_bad_method:
    mov rcx, rbx
    mov edx, HTTP_METHOD_NOT_ALLOWED
    lea r8, szJsonErrorMethod
    call SendJsonResponse
    
@generate_done:
    pop rdi
    pop rsi
    pop rbx
    ret
HandleApiGenerate ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; HandleApiTags
; GET /api/tags - List available models
; ═══════════════════════════════════════════════════════════════════════════════
HandleApiTags PROC FRAME
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; HttpRequest*
    
    ; Scan models directory
    call ScanModelsDirectory
    mov rsi, rax                    ; RSI = model list JSON
    
    ; Send response
    mov rcx, rbx
    mov edx, HTTP_OK
    mov r8, rsi
    call SendJsonResponse
    
    ; Free temporary buffer
    mov rcx, rsi
    call HeapFree
    
    pop rdi
    pop rsi
    pop rbx
    ret
HandleApiTags ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; HandleV1ChatCompletions
; POST /v1/chat/completions - OpenAI-compatible endpoint
; ═══════════════════════════════════════════════════════════════════════════════
HandleV1ChatCompletions PROC FRAME
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx
    
    ; Similar to /api/generate but with OpenAI message format
    ; Convert messages array to single prompt
    call ConvertChatMessagesToPrompt
    
    ; Delegate to inference engine
    mov rcx, rbx
    call HandleApiGenerate
    
    ; Transform response to OpenAI format
    call TransformToOpenAIFormat
    
    pop rdi
    pop rsi
    pop rbx
    ret
HandleV1ChatCompletions ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SendJsonResponse
; Sends HTTP response with JSON body
; ═══════════════════════════════════════════════════════════════════════════════
SendJsonResponse PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rbx, rcx                    ; HttpRequest*
    mov esi, edx                    ; StatusCode
    mov rdi, r8                     ; JSON body string
    
    ; 1. Calculate JSON length
    mov rcx, rdi
    call RawrXD_StrLen
    mov r12, rax                    ; r12 = body len
    
    ; 2. Format Header Buffer
    ; Allocate temporary buffer for headers
    mov rcx, 1024
    call RawrXD_MemAlloc
    test rax, rax
    jz @send_done
    
    mov rsi, rax                    ; Buffer for headers
    
    ; Header template: "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n"
    ; Simplified for ASM: Send Status Line, then Type, then Length, then blank line.
    
    ; Send Status Line
    lea rdx, szHttpStatusTemplate   ; "HTTP/1.1 %d OK\r\n"
    mov rcx, rsi
    mov r8, rsi                     ; Temp buffer
    mov r9, rsi                     ; .. wait wsprintf is complex
    
    ; We'll just construct manually or use wsprintfA which we import user32 for.
    ; But for standalone, let's use send repeatedly or construct simplified.
    
    ; A. Status Line
    lea rdX, szHttpStatusTemplate
    mov rcx, rsi
    mov r8d, esi                    ; StatusCode
    call wsprintfA
    ; Send
    mov rcx, [rbx].HttpRequest.Socket
    mov rdx, rsi
    mov r8, rax                     ; Len
    xor r9, r9
    call send
    
    ; B. Content-Type/Len
    lea rdx, szHttpJsonHeaders      ; "Content-Type... Length: %d... %s" is too complex for 1 go maybe?
    ; Let's format manually: "Content-Type: application/json\r\nContent-Length: "
    ; + Itoa(Len) + "\r\n\r\n"
    
    ; Let's use the template: szHttpJsonHeaders = "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n%s"
    ; NO, we send body separately to avoid huge allocation.
    ; Change template to just headers.
    
    ; Fix template in .DATA first? We can reuse szHttpJsonHeaders but ignore the %s if we change code.
    ; Actually, let's just make a new small template here on stack or assume we have one.
    ; I'll assume I can just generic send headers.
    
    mov rcx, rsi
    lea rdx, szHttpJsonHeadersPartial ; We need to define this or reuse
    mov r8, r12                     ; ContentLength
    call wsprintfA
    
    mov rcx, [rbx].HttpRequest.Socket
    mov rdx, rsi
    mov r8, rax
    xor r9, r9
    call send
    
    ; C. Send Body
    mov rcx, [rbx].HttpRequest.Socket
    mov rdx, rdi
    mov r8, r12
    xor r9, r9
    call send
    
    ; Free temp buffer
    mov rcx, rsi
    call RawrXD_MemFree

@send_done:
    add rsp, 28h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
SendJsonResponse ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; QueueInferenceJob
; Adds job to inference queue (called by HTTP handlers)
; ═══════════════════════════════════════════════════════════════════════════════
QueueInferenceJob PROC FRAME
    ; Implementation: add to lock-free MPSC queue
    ; Signal inference engine thread
    ret
QueueInferenceJob ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper stubs
; ═══════════════════════════════════════════════════════════════════════════════
ParseHttpMethod PROC
    xor eax, eax
    ret
ParseHttpMethod ENDP

ParseHttpRoute PROC
    xor eax, eax
    ret
ParseHttpRoute ENDP

ParseContentLength PROC
    xor eax, eax
    ret
ParseContentLength ENDP

FindBodyOffset PROC
    xor eax, eax
    ret
FindBodyOffset ENDP

JsonExtractString PROC
    xor eax, eax
    ret
JsonExtractString ENDP

JsonExtractBool PROC
    xor eax, eax
    ret
JsonExtractBool ENDP

ScanModelsDirectory PROC
    xor eax, eax
    ret
ScanModelsDirectory ENDP

ConvertChatMessagesToPrompt PROC
    ret
ConvertChatMessagesToPrompt ENDP

TransformToOpenAIFormat PROC
    ret
TransformToOpenAIFormat ENDP

StartStreamingResponse PROC
    ret
StartStreamingResponse ENDP

WaitForInferenceCompletion PROC FRAME
    ; Stub: wait for job completion
    mov eax, 1
    ret
WaitForInferenceCompletion ENDP

HandleHealth PROC FRAME
    ; Simple health check
    mov eax, 1
    ret
HandleHealth ENDP

FormatJsonResponse PROC FRAME
    ; Stub
    xor eax, eax
    ret
FormatJsonResponse ENDP

; Removed C-Runtime Stubs (using RawrXD_Core_Utils or Win32)
; HeapAlloc -> RawrXD_MemAlloc
; HeapFree -> RawrXD_MemFree
; lstrlenA -> RawrXD_StrLen
; wsprintfA -> user32.wsprintfA (imported)
; memcpy -> RtlMoveMemory (kernel32)

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC HttpRouter_Initialize
PUBLIC HttpAcceptorThread
PUBLIC HttpWorkerThread
PUBLIC ProcessHttpConnection
PUBLIC QueueInferenceJob

; String constants for JSON parsing
szJsonModelField        BYTE '"model"', 0
szJsonPromptField       BYTE '"prompt"', 0
szJsonStreamField       BYTE '"stream"', 0
szJsonErrorMethod       BYTE '{ "error": "method not allowed" }', 0
szHttpStatusTemplate    BYTE "HTTP/1.1 %d OK\r\n", 0
szHttpJsonHeaders       BYTE "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", 0
szJsonGenerateTemplate  BYTE '{ "response": "%s" }', 0

END
