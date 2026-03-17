; =====================================================================
; NASM IDE Swarm Integration
; C-compatible interface for calling swarm agents from NASM
; =====================================================================

section .data
    ; Swarm API endpoints
    swarm_url       db "http://localhost:8888", 0
    dispatch_path   db "/dispatch", 0
    status_path     db "/status", 0
    
    ; HTTP headers
    content_type    db "Content-Type: application/json", 0
    
    ; JSON templates
    code_complete_json db '{"type":"code_complete","payload":{"task":"code_complete","code":"%s","cursor":%d}}', 0
    build_debug_json   db '{"type":"build_debug","payload":{"task":"build_debug","source":"%s"}}', 0
    status_json        db '{"type":"status_update","payload":{"task":"status_update","message":"%s","type":"%s"}}', 0
    
    ; Response buffer
    response_buffer resb 8192
    
section .text
    ; External C library functions
    extern sprintf
    extern curl_easy_init
    extern curl_easy_setopt
    extern curl_easy_perform
    extern curl_easy_cleanup
    extern curl_easy_getinfo
    extern curl_slist_append
    
    ; Swarm API functions
    global SwarmInit
    global SwarmShutdown
    global SwarmCodeComplete
    global SwarmBuildDebug
    global SwarmUpdateStatus
    global SwarmGetStatus
    global SwarmDetectToolchains

; =====================================================================
; Initialize Swarm Connection
; =====================================================================
SwarmInit:
    push rbp
    mov rbp, rsp
    
    ; Initialize curl
    call curl_easy_init
    mov [curl_handle], rax
    
    test rax, rax
    jz .failed
    
    ; Success
    mov eax, 1
    jmp .done
    
.failed:
    xor eax, eax
    
.done:
    pop rbp
    ret

; =====================================================================
; Shutdown Swarm Connection
; =====================================================================
SwarmShutdown:
    push rbp
    mov rbp, rsp
    
    ; Cleanup curl
    mov rcx, [curl_handle]
    test rcx, rcx
    jz .done
    
    call curl_easy_cleanup
    mov qword [curl_handle], 0
    
.done:
    pop rbp
    ret

; =====================================================================
; Request Code Completion from AI Agent
; Parameters: RCX = code string, RDX = cursor position
; Returns: RAX = response buffer pointer
; =====================================================================
SwarmCodeComplete:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Save parameters
    mov [rsp], rcx      ; code string
    mov [rsp + 8], rdx  ; cursor
    
    ; Format JSON payload
    lea rcx, [json_buffer]
    lea rdx, [code_complete_json]
    mov r8, [rsp]       ; code
    mov r9, [rsp + 8]   ; cursor
    call sprintf
    
    ; Send HTTP POST request
    lea rcx, [dispatch_path]
    lea rdx, [json_buffer]
    call SwarmHTTPPost
    
    ; Return response buffer
    lea rax, [response_buffer]
    
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Request Debug Build from Build Agent
; Parameters: RCX = source file path
; Returns: RAX = response buffer pointer
; =====================================================================
SwarmBuildDebug:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Format JSON payload
    lea rcx, [json_buffer]
    lea rdx, [build_debug_json]
    mov r8, [rsp]       ; source
    call sprintf
    
    ; Send HTTP POST request
    lea rcx, [dispatch_path]
    lea rdx, [json_buffer]
    call SwarmHTTPPost
    
    ; Return response
    lea rax, [response_buffer]
    
    add rsp, 32
    pop rbp
    ret

; =====================================================================
; Update Status Bar via Status Agent
; Parameters: RCX = message, RDX = type ("info", "success", "error")
; Returns: RAX = success (1) or failure (0)
; =====================================================================
SwarmUpdateStatus:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Save parameters
    mov [rsp], rcx      ; message
    mov [rsp + 8], rdx  ; type
    
    ; Format JSON payload
    lea rcx, [json_buffer]
    lea rdx, [status_json]
    mov r8, [rsp]       ; message
    mov r9, [rsp + 8]   ; type
    call sprintf
    
    ; Send HTTP POST request
    lea rcx, [dispatch_path]
    lea rdx, [json_buffer]
    call SwarmHTTPPost
    
    ; Check response
    test rax, rax
    setnz al
    movzx eax, al
    
    add rsp, 32
    pop rbp
    ret

; =====================================================================
; Get Swarm Status
; Returns: RAX = response buffer pointer
; =====================================================================
SwarmGetStatus:
    push rbp
    mov rbp, rsp
    
    ; Send HTTP GET request
    lea rcx, [status_path]
    call SwarmHTTPGet
    
    ; Return response
    lea rax, [response_buffer]
    
    pop rbp
    ret

; =====================================================================
; Detect Toolchains via Detection Agent
; Returns: RAX = response buffer pointer with toolchain info
; =====================================================================
SwarmDetectToolchains:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Format JSON payload
    lea rcx, [json_buffer]
    lea rdx, [detect_json]
    call sprintf
    
    ; Send request
    lea rcx, [dispatch_path]
    lea rdx, [json_buffer]
    call SwarmHTTPPost
    
    lea rax, [response_buffer]
    
    add rsp, 32
    pop rbp
    ret

; =====================================================================
; Internal: HTTP POST Request
; Parameters: RCX = path, RDX = JSON data
; Returns: RAX = bytes received
; =====================================================================
SwarmHTTPPost:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov [rsp], rcx      ; path
    mov [rsp + 8], rdx  ; json data
    
    ; Build full URL
    lea rcx, [url_buffer]
    lea rdx, [swarm_url]
    call strcpy
    
    lea rcx, [url_buffer]
    mov rdx, [rsp]      ; path
    call strcat
    
    ; Setup curl
    mov rcx, [curl_handle]
    mov rdx, CURLOPT_URL
    lea r8, [url_buffer]
    call curl_easy_setopt
    
    ; Set POST data
    mov rcx, [curl_handle]
    mov rdx, CURLOPT_POSTFIELDS
    mov r8, [rsp + 8]   ; json data
    call curl_easy_setopt
    
    ; Set headers
    xor rcx, rcx
    lea rdx, [content_type]
    call curl_slist_append
    mov [headers], rax
    
    mov rcx, [curl_handle]
    mov rdx, CURLOPT_HTTPHEADER
    mov r8, [headers]
    call curl_easy_setopt
    
    ; Set write callback
    mov rcx, [curl_handle]
    mov rdx, CURLOPT_WRITEFUNCTION
    lea r8, [WriteCallback]
    call curl_easy_setopt
    
    ; Perform request
    mov rcx, [curl_handle]
    call curl_easy_perform
    
    ; Get bytes received
    mov rax, [bytes_received]
    
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Internal: HTTP GET Request
; Parameters: RCX = path
; Returns: RAX = bytes received
; =====================================================================
SwarmHTTPGet:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Build URL
    lea rcx, [url_buffer]
    lea rdx, [swarm_url]
    call strcpy
    
    lea rcx, [url_buffer]
    mov rdx, [rsp]
    call strcat
    
    ; Setup curl
    mov rcx, [curl_handle]
    mov rdx, CURLOPT_URL
    lea r8, [url_buffer]
    call curl_easy_setopt
    
    ; Perform request
    mov rcx, [curl_handle]
    call curl_easy_perform
    
    mov rax, [bytes_received]
    
    add rsp, 32
    pop rbp
    ret

; =====================================================================
; CURL Write Callback
; =====================================================================
WriteCallback:
    push rbp
    mov rbp, rsp
    
    ; RCX = data, RDX = size, R8 = nmemb, R9 = userdata
    imul rdx, r8        ; total size
    
    ; Copy to response buffer
    push rsi
    push rdi
    
    mov rsi, rcx
    lea rdi, [response_buffer]
    add rdi, [bytes_received]
    mov rcx, rdx
    rep movsb
    
    pop rdi
    pop rsi
    
    ; Update bytes received
    add [bytes_received], rdx
    mov rax, rdx
    
    pop rbp
    ret

section .bss
    curl_handle     resq 1
    headers         resq 1
    bytes_received  resq 1
    url_buffer      resb 512
    json_buffer     resb 4096

section .data
    detect_json     db '{"type":"detect_toolchain","payload":{"task":"detect"}}', 0
    
    ; CURL constants
    CURLOPT_URL             equ 10002
    CURLOPT_POSTFIELDS      equ 10015
    CURLOPT_HTTPHEADER      equ 10023
    CURLOPT_WRITEFUNCTION   equ 20011
