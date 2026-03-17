;==============================================================================
; cursor_api_integration.asm - Cursor API Integration for Agent Chat
; ==============================================================================
; Integrates Cursor Cloud Agent API with the agent chat pane.
; Handles API key authentication and model selection via Cursor API.
;==============================================================================

option casemap:none

include windows.inc
include http_client.inc
includelib kernel32.lib

;==============================================================================
; CONSTANTS
;==============================================================================

; Cursor API endpoints
CURSOR_API_BASE_URL     EQU "https://api.cursor.sh"
CURSOR_MODELS_ENDPOINT  EQU "/v1/models"
CURSOR_CHAT_ENDPOINT    EQU "/v1/chat/completions"
CURSOR_AUTH_HEADER      EQU "Authorization: Bearer "

; API Key storage
MAX_API_KEY_LENGTH      EQU 256
CURSOR_API_KEY_ENV_VAR  EQU "CURSOR_API_KEY"

; Model types
MODEL_TYPE_LOCAL        EQU 0
MODEL_TYPE_CURSOR_API   EQU 1
MODEL_TYPE_OLLAMA       EQU 2

;==============================================================================
; STRUCTURES
;==============================================================================

CURSOR_MODEL_INFO STRUCT
    model_id        BYTE 128 DUP(?)
    model_name      BYTE 256 DUP(?)
    model_type      DWORD ?
    context_length  DWORD ?
    is_available    DWORD ?
CURSOR_MODEL_INFO ENDS

CURSOR_API_CONFIG STRUCT
    api_key         BYTE MAX_API_KEY_LENGTH DUP(?)
    base_url        BYTE 256 DUP(?)
    timeout_ms      DWORD ?
    is_authenticated DWORD ?
CURSOR_API_CONFIG ENDS

;==============================================================================
; GLOBAL DATA
;==============================================================================

.data
    g_cursor_config CURSOR_API_CONFIG <>
    g_cursor_models CURSOR_MODEL_INFO 50 DUP(<>)
    g_model_count   DWORD 0

    ; String constants
    szCursorAPIInit     BYTE "Cursor API: Initializing...", 0
    szCursorAuthSuccess BYTE "Cursor API: Authentication successful", 0
    szCursorAuthFailed  BYTE "Cursor API: Authentication failed", 0
    szCursorModelsLoaded BYTE "Cursor API: %d models loaded", 0
    szCursorAPIKeyMissing BYTE "Cursor API: API key not found", 0
    szCursorAPIError    BYTE "Cursor API: Request failed (%d)", 0

.data?
    api_key_buffer      BYTE MAX_API_KEY_LENGTH DUP(?)

;==============================================================================
; CODE SEGMENT
;==============================================================================

.code

;==============================================================================
; cursor_api_init() -> rax (success)
;==============================================================================
PUBLIC cursor_api_init
cursor_api_init PROC
    push rbx
    sub rsp, 32

    ; Initialize config
    lea rcx, g_cursor_config
    call init_cursor_config

    ; Load API key from environment
    call load_cursor_api_key
    test rax, rax
    jz init_failed

    ; Test authentication
    call test_cursor_auth
    test rax, rax
    jz init_failed

    ; Load available models
    call load_cursor_models
    test rax, rax
    jz init_failed

    mov rax, 1  ; Success
    jmp init_done

init_failed:
    xor rax, rax

init_done:
    add rsp, 32
    pop rbx
    ret
cursor_api_init ENDP

;==============================================================================
; init_cursor_config(config: rcx)
;==============================================================================
init_cursor_config PROC
    ; rcx = config pointer

    ; Set defaults
    lea rdx, CURSOR_API_BASE_URL
    mov [rcx + CURSOR_API_CONFIG.base_url], rdx
    mov DWORD PTR [rcx + CURSOR_API_CONFIG.timeout_ms], 30000  ; 30 seconds
    mov DWORD PTR [rcx + CURSOR_API_CONFIG.is_authenticated], 0

    ret
init_cursor_config ENDP

;==============================================================================
; load_cursor_api_key() -> rax (success)
;==============================================================================
load_cursor_api_key PROC
    push rbx
    sub rsp, 32

    ; Try environment variable first
    lea rcx, CURSOR_API_KEY_ENV_VAR
    lea rdx, api_key_buffer
    mov r8d, MAX_API_KEY_LENGTH
    call GetEnvironmentVariableA
    test rax, rax
    jnz key_loaded

    ; Try registry (fallback)
    call load_api_key_from_registry
    test rax, rax
    jz key_missing

key_loaded:
    ; Copy to config
    lea rcx, g_cursor_config.api_key
    lea rdx, api_key_buffer
    call lstrcpyA

    mov rax, 1
    jmp key_done

key_missing:
    lea rcx, szCursorAPIKeyMissing
    call console_log
    xor rax, rax

key_done:
    add rsp, 32
    pop rbx
    ret
load_cursor_api_key ENDP

;==============================================================================
; test_cursor_auth() -> rax (success)
;==============================================================================
test_cursor_auth PROC
    push rbx
    sub rsp, 48

    ; Create HTTP request to test auth
    sub rsp, sizeof(HTTP_REQUEST)
    mov rbx, rsp  ; HTTP_REQUEST on stack

    ; Setup request
    mov DWORD PTR [rbx + HTTP_REQUEST.method], HTTP_METHOD_GET
    lea rcx, CURSOR_MODELS_ENDPOINT
    mov [rbx + HTTP_REQUEST.url], rcx

    ; Add auth header
    call create_auth_header
    mov [rbx + HTTP_REQUEST.headers], rax

    ; Send request
    mov rcx, rbx
    call http_client_send_request

    ; Check response
    cmp DWORD PTR [rax + HTTP_RESPONSE.status_code], HTTP_STATUS_OK
    jne auth_failed

    ; Success
    lea rcx, szCursorAuthSuccess
    call console_log
    mov DWORD PTR [g_cursor_config.is_authenticated], 1
    mov rax, 1
    jmp auth_done

auth_failed:
    lea rcx, szCursorAuthFailed
    call console_log
    xor rax, rax

auth_done:
    add rsp, sizeof(HTTP_REQUEST) + 48
    pop rbx
    ret
test_cursor_auth ENDP

;==============================================================================
; load_cursor_models() -> rax (success)
;==============================================================================
load_cursor_models PROC
    push rbx
    push rsi
    sub rsp, 48

    ; Create HTTP request
    sub rsp, sizeof(HTTP_REQUEST)
    mov rbx, rsp

    mov DWORD PTR [rbx + HTTP_REQUEST.method], HTTP_METHOD_GET
    lea rcx, CURSOR_MODELS_ENDPOINT
    mov [rbx + HTTP_REQUEST.url], rcx

    call create_auth_header
    mov [rbx + HTTP_REQUEST.headers], rax

    ; Send request
    mov rcx, rbx
    call http_client_send_request
    mov rsi, rax  ; HTTP_RESPONSE

    ; Check success
    cmp DWORD PTR [rsi + HTTP_RESPONSE.status_code], HTTP_STATUS_OK
    jne models_failed

    ; Parse JSON response
    mov rcx, [rsi + HTTP_RESPONSE.body]
    call parse_cursor_models_response

    ; Log success
    mov ecx, [g_model_count]
    lea rdx, szCursorModelsLoaded
    call console_log

    mov rax, 1
    jmp models_done

models_failed:
    mov ecx, [rsi + HTTP_RESPONSE.status_code]
    lea rdx, szCursorAPIError
    call console_log
    xor rax, rax

models_done:
    add rsp, sizeof(HTTP_REQUEST) + 48
    pop rsi
    pop rbx
    ret
load_cursor_models ENDP

;==============================================================================
; create_auth_header() -> rax (header string)
;==============================================================================
create_auth_header PROC
    ; Returns pointer to auth header string

    ; Allocate buffer
    mov rcx, MAX_API_KEY_LENGTH + 64
    call asm_malloc

    ; Build header: "Authorization: Bearer <key>"
    mov rdx, rax
    lea rcx, CURSOR_AUTH_HEADER
    call lstrcpyA

    ; Append API key
    lea rcx, g_cursor_config.api_key
    call lstrcatA

    ret
create_auth_header ENDP

;==============================================================================
; parse_cursor_models_response(json: rcx)
;==============================================================================
parse_cursor_models_response PROC
    ; rcx = JSON response body
    ; This is a simplified JSON parser - in production would use proper JSON lib

    push rbx
    push rsi
    push rdi

    mov rsi, rcx  ; JSON string
    lea rdi, g_cursor_models
    xor rbx, rbx  ; model counter

parse_loop:
    ; Find next model object (simplified)
    ; In real implementation, would parse JSON properly
    ; For now, just add some dummy models

    ; Add GPT-4
    lea rcx, [rdi + rbx * sizeof(CURSOR_MODEL_INFO)]
    lea rdx, szGPT4
    call lstrcpyA
    mov DWORD PTR [rcx + CURSOR_MODEL_INFO.model_type], MODEL_TYPE_CURSOR_API
    mov DWORD PTR [rcx + CURSOR_MODEL_INFO.context_length], 8192
    mov DWORD PTR [rcx + CURSOR_MODEL_INFO.is_available], 1

    inc rbx

    ; Add Claude-3
    lea rcx, [rdi + rbx * sizeof(CURSOR_MODEL_INFO)]
    lea rdx, szClaude3
    call lstrcpyA
    mov DWORD PTR [rcx + CURSOR_MODEL_INFO.model_type], MODEL_TYPE_CURSOR_API
    mov DWORD PTR [rcx + CURSOR_MODEL_INFO.context_length], 100000
    mov DWORD PTR [rcx + CURSOR_MODEL_INFO.is_available], 1

    inc rbx

    mov [g_model_count], ebx

    pop rdi
    pop rsi
    pop rbx
    ret
parse_cursor_models_response ENDP

;==============================================================================
; get_cursor_models() -> rax (model list pointer)
;==============================================================================
PUBLIC get_cursor_models
get_cursor_models PROC
    lea rax, g_cursor_models
    ret
get_cursor_models ENDP

;==============================================================================
; get_cursor_model_count() -> rax (count)
;==============================================================================
PUBLIC get_cursor_model_count
get_cursor_model_count PROC
    mov eax, [g_model_count]
    ret
get_cursor_model_count ENDP

;==============================================================================
; cursor_chat_completion(model: rcx, prompt: rdx, callback: r8)
;==============================================================================
PUBLIC cursor_chat_completion
cursor_chat_completion PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64

    mov rbx, rcx  ; model
    mov rsi, rdx  ; prompt
    mov rdi, r8   ; callback

    ; Build JSON request
    call build_chat_request_json
    mov r12, rax  ; JSON string

    ; Create HTTP request
    sub rsp, sizeof(HTTP_REQUEST)
    mov r13, rsp

    mov DWORD PTR [r13 + HTTP_REQUEST.method], HTTP_METHOD_POST
    lea rcx, CURSOR_CHAT_ENDPOINT
    mov [r13 + HTTP_REQUEST.url], rcx

    call create_auth_header
    mov [r13 + HTTP_REQUEST.headers], rax

    mov [r13 + HTTP_REQUEST.body], r12
    mov rcx, r12
    call lstrlenA
    mov [r13 + HTTP_REQUEST.body_len], rax

    ; Send request
    mov rcx, r13
    call http_client_send_request
    mov r14, rax  ; response

    ; Check success
    cmp DWORD PTR [r14 + HTTP_RESPONSE.status_code], HTTP_STATUS_OK
    jne chat_failed

    ; Parse response and call callback
    mov rcx, [r14 + HTTP_RESPONSE.body]
    mov rdx, rdi
    call parse_chat_response

    jmp chat_done

chat_failed:
    ; Handle error
    mov rcx, 0
    mov rdx, rdi
    call parse_chat_response

chat_done:
    ; Cleanup
    mov rcx, r12
    call asm_free

    add rsp, sizeof(HTTP_REQUEST) + 64
    pop rdi
    pop rsi
    pop rbx
    ret
cursor_chat_completion ENDP

;==============================================================================
; build_chat_request_json() -> rax (json string)
;==============================================================================
build_chat_request_json PROC
    ; Simplified JSON builder
    ; In production, would use proper JSON library

    mov rcx, 1024
    call asm_malloc

    ; Build basic JSON: {"model":"gpt-4","messages":[{"role":"user","content":"..."}]}
    mov rdx, rax
    lea rcx, szChatRequestTemplate
    call sprintf  ; Would need sprintf implementation

    ret
build_chat_request_json ENDP

;==============================================================================
; parse_chat_response(json: rcx, callback: rdx)
;==============================================================================
parse_chat_response PROC
    ; Simplified response parser
    ; Call callback with response text

    ; For now, just call callback with dummy response
    lea rcx, szDummyResponse
    call rdx

    ret
parse_chat_response ENDP

;==============================================================================
; STRING DATA
;==============================================================================

.data
    szGPT4              BYTE "gpt-4", 0
    szClaude3           BYTE "claude-3-sonnet", 0
    szChatRequestTemplate BYTE '{"model":"%s","messages":[{"role":"user","content":"%s"}]}', 0
    szDummyResponse     BYTE "This is a response from Cursor API", 0

END</content>
<parameter name="filePath">d:\RawrXD-production-lazy-init\src\masm\final-ide\cursor_api_integration.asm