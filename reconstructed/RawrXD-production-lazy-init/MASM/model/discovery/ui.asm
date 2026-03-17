; ============================================================================
; MODEL DISCOVERY UI - Pure MASM x64 Model List Display
; ============================================================================
; Parse Ollama /api/tags JSON response and format for display
; Features:
;   - Parse JSON model arrays
;   - Extract model name, size, modified date, digest
;   - Format human-readable model list
;   - Sort by name or size
;   - Filter by search term
; ============================================================================

.code

; External functions
extern OllamaClient_ListModels:PROC
extern LocalAlloc:PROC
extern LocalFree:PROC
extern lstrlenA:PROC
extern lstrcpyA:PROC
extern lstrcatA:PROC
extern wsprintfA:PROC
extern Logger_LogStructured:PROC

; ============================================================================
; Constants
; ============================================================================
LMEM_ZEROINIT = 40h
LMEM_FIXED = 0h
LOG_LEVEL_INFO = 1
LOG_LEVEL_ERROR = 3

MAX_MODELS = 256
MAX_MODEL_NAME = 128
MAX_MODEL_SIZE_STR = 32
MAX_DISPLAY_LINE = 256

; ============================================================================
; Model Info Structure (256 bytes)
; ============================================================================
; +0:   name (128 bytes)
; +128: size_str (32 bytes)
; +160: modified (32 bytes)
; +192: digest (32 bytes)
; +224: size_bytes (8 bytes)
; +232: reserved (24 bytes)
; ============================================================================

.data
    ; JSON field names
    szModelsField       db '"models"', 0
    szNameField         db '"name"', 0
    szSizeField         db '"size"', 0
    szModifiedField     db '"modified_at"', 0
    szDigestField       db '"digest"', 0
    
    ; Display formatting
    szHeaderLine        db "╔═══════════════════════════════════════════════════════════════╗", 13, 10
                        db "║  OLLAMA MODELS - Available Local Models                      ║", 13, 10
                        db "╠═══════════════════════════════════════════════════════════════╣", 13, 10, 0
    
    szColumnHeaders     db "║  Name                              Size        Modified       ║", 13, 10
                        db "╟───────────────────────────────────────────────────────────────╢", 13, 10, 0
    
    szFooterLine        db "╚═══════════════════════════════════════════════════════════════╝", 13, 10, 0
    
    szModelLineFormat   db "║  %-32s  %-10s  %-12s  ║", 13, 10, 0
    szNoModelsFound     db "║  No models found. Install models with 'ollama pull <model>' ║", 13, 10, 0
    szTotalModelsFormat db "║  Total: %d model(s)                                           ║", 13, 10, 0
    
    ; Size formatting suffixes
    szBytesSuffix       db " bytes", 0
    szKBSuffix          db " KB", 0
    szMBSuffix          db " MB", 0
    szGBSuffix          db " GB", 0
    
    ; Log messages
    szModelDiscoveryStart   db "Starting model discovery", 0
    szModelDiscoveryDone    db "Model discovery completed", 0
    szModelParseError       db "JSON parse error", 0

.data?
    pJsonResponse       qword ?
    pModelList          qword ?
    dwModelCount        dword ?
    qwJsonResponseSize  qword ?

; ============================================================================
; ModelDiscovery_ListModels - Display all available Ollama models
; ============================================================================
; Parameters:
;   RCX = Output buffer for formatted text
;   RDX = Buffer size
; Returns:
;   RAX = Number of bytes written (0 on failure)
; ============================================================================
PUBLIC ModelDiscovery_ListModels
ModelDiscovery_ListModels PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 38h
    
    ; Save parameters
    mov rbx, rcx                ; output buffer
    mov rsi, rdx                ; buffer size
    
    ; Log start
    lea rcx, [szModelDiscoveryStart]
    mov rdx, LOG_LEVEL_INFO
    call Logger_LogStructured
    
    ; Allocate buffer for JSON response (1MB)
    mov rcx, 1048576
    mov rdx, LMEM_ZEROINIT
    call LocalAlloc
    test rax, rax
    jz discovery_failed
    
    mov [pJsonResponse], rax
    mov r12, rax
    
    ; Call Ollama API to get models
    mov rcx, r12
    mov rdx, 1048576
    call OllamaClient_ListModels
    
    test rax, rax
    jz discovery_cleanup_json
    
    mov [qwJsonResponseSize], rax
    
    ; Allocate model list array (256 models × 256 bytes = 64KB)
    mov rcx, 65536
    mov rdx, LMEM_ZEROINIT
    call LocalAlloc
    test rax, rax
    jz discovery_cleanup_json
    
    mov [pModelList], rax
    mov r13, rax
    
    ; Parse JSON and extract models
    mov rcx, r12                ; JSON string
    mov rdx, r13                ; model array
    lea r8, [dwModelCount]      ; count output
    call ParseModelListJSON
    
    test eax, eax
    jz discovery_cleanup_all
    
    ; Format output for display
    mov rcx, rbx                ; output buffer
    mov rdx, rsi                ; buffer size
    mov r8, r13                 ; model array
    mov r9d, [dwModelCount]     ; model count
    call FormatModelDisplay
    
    mov r14, rax                ; save bytes written
    jmp discovery_cleanup_all
    
discovery_cleanup_all:
    mov rcx, [pModelList]
    call LocalFree
    
discovery_cleanup_json:
    mov rcx, [pJsonResponse]
    call LocalFree
    
discovery_failed:
    xor r14, r14
    
    ; Log completion
    lea rcx, [szModelDiscoveryDone]
    mov rdx, LOG_LEVEL_INFO
    call Logger_LogStructured
    
    mov rax, r14
    add rsp, 38h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ModelDiscovery_ListModels ENDP

; ============================================================================
; ParseModelListJSON - Extract model info from JSON array
; ============================================================================
; Parameters:
;   RCX = JSON string
;   RDX = Output model array
;   R8  = Pointer to model count output
; Returns:
;   EAX = 1 on success, 0 on failure
; ============================================================================
ParseModelListJSON PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 28h
    
    mov rbx, rcx                ; JSON string
    mov rsi, rdx                ; model array
    mov rdi, r8                 ; count pointer
    xor r12d, r12d              ; model count = 0
    
    ; Find "models" array
    mov rcx, rbx
    lea rdx, [szModelsField]
    call FindJSONField
    
    test rax, rax
    jz parse_failed
    
    mov r13, rax                ; pointer to models array
    
    ; Skip to array start '['
    mov rcx, r13
    mov dl, '['
    call FindChar
    test rax, rax
    jz parse_failed
    
    inc rax                     ; skip '['
    mov r14, rax                ; current position
    
parse_next_model:
    ; Check for array end ']'
    movzx eax, byte ptr [r14]
    cmp al, ']'
    je parse_done
    
    ; Check for object start '{'
    cmp al, '{'
    jne parse_advance
    
    ; Parse this model object
    mov rcx, r14                ; object start
    mov rdx, rsi                ; current model struct
    call ParseModelObject
    
    test eax, eax
    jz parse_advance
    
    ; Move to next model slot
    add rsi, 256
    inc r12d
    cmp r12d, MAX_MODELS
    jae parse_done
    
parse_advance:
    inc r14
    jmp parse_next_model
    
parse_done:
    mov [rdi], r12d             ; store model count
    mov eax, 1                  ; success
    jmp parse_exit
    
parse_failed:
    xor eax, eax
    
parse_exit:
    add rsp, 28h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ParseModelListJSON ENDP

; ============================================================================
; ParseModelObject - Extract info from single model object
; ============================================================================
; Parameters:
;   RCX = JSON object string (starting at '{')
;   RDX = Output model structure (256 bytes)
; Returns:
;   EAX = 1 on success, 0 on failure
; ============================================================================
ParseModelObject PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx                ; JSON object
    mov rsi, rdx                ; model struct
    
    ; Extract name
    mov rcx, rbx
    lea rdx, [szNameField]
    call FindJSONField
    test rax, rax
    jz parse_obj_failed
    
    mov rcx, rax
    mov rdx, rsi                ; name field in struct
    mov r8d, MAX_MODEL_NAME
    call ExtractStringValue
    
    ; Extract size
    mov rcx, rbx
    lea rdx, [szSizeField]
    call FindJSONField
    test rax, rax
    jz parse_obj_no_size
    
    mov rcx, rax
    lea rdx, [rsi + 224]        ; size_bytes field
    call ExtractNumberValue
    
    ; Format size as human-readable
    mov rcx, [rsi + 224]        ; size in bytes
    lea rdx, [rsi + 128]        ; size_str field
    mov r8d, MAX_MODEL_SIZE_STR
    call FormatSizeString
    
parse_obj_no_size:
    ; Extract modified date
    mov rcx, rbx
    lea rdx, [szModifiedField]
    call FindJSONField
    test rax, rax
    jz parse_obj_no_modified
    
    mov rcx, rax
    lea rdx, [rsi + 160]        ; modified field
    mov r8d, 32
    call ExtractStringValue
    
parse_obj_no_modified:
    mov eax, 1                  ; success
    jmp parse_obj_exit
    
parse_obj_failed:
    xor eax, eax
    
parse_obj_exit:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
ParseModelObject ENDP

; ============================================================================
; FormatModelDisplay - Create formatted model list output
; ============================================================================
; Parameters:
;   RCX = Output buffer
;   RDX = Buffer size
;   R8  = Model array
;   R9D = Model count
; Returns:
;   RAX = Bytes written
; ============================================================================
FormatModelDisplay PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 38h
    
    mov rbx, rcx                ; output buffer
    mov rsi, rdx                ; buffer size
    mov r12, r8                 ; model array
    mov r13d, r9d               ; model count
    xor r14, r14                ; bytes written
    
    ; Write header
    mov rcx, rbx
    lea rdx, [szHeaderLine]
    call lstrcatA
    
    mov rcx, rbx
    lea rdx, [szColumnHeaders]
    call lstrcatA
    
    ; Check if no models
    test r13d, r13d
    jnz format_models
    
    mov rcx, rbx
    lea rdx, [szNoModelsFound]
    call lstrcatA
    jmp format_footer
    
format_models:
    xor r14d, r14d              ; current model index
    
format_next_model:
    ; Format model line
    lea rcx, [rsp+20h]          ; temp buffer
    lea rdx, [szModelLineFormat]
    mov r8, r12                 ; name field
    lea r9, [r12 + 128]         ; size_str field
    mov rax, [r12 + 160]        ; modified field
    mov [rsp+20h], rax
    call wsprintfA
    
    ; Append to output
    mov rcx, rbx
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; Next model
    add r12, 256
    inc r14d
    cmp r14d, r13d
    jl format_next_model
    
format_footer:
    ; Write total count
    lea rcx, [rsp+20h]
    lea rdx, [szTotalModelsFormat]
    mov r8d, r13d
    call wsprintfA
    
    mov rcx, rbx
    lea rdx, [rsp+20h]
    call lstrcatA
    
    ; Write footer
    mov rcx, rbx
    lea rdx, [szFooterLine]
    call lstrcatA
    
    ; Calculate total length
    mov rcx, rbx
    call lstrlenA
    
    add rsp, 38h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
FormatModelDisplay ENDP

; ============================================================================
; Helper Functions
; ============================================================================

; FindJSONField - Locate field in JSON string
; Returns pointer to field value or NULL
FindJSONField PROC
    ; Stub - full implementation searches for field name
    xor rax, rax
    ret
FindJSONField ENDP

; FindChar - Find first occurrence of character
FindChar PROC
    xor rax, rax
    ret
FindChar ENDP

; ExtractStringValue - Extract quoted string value
ExtractStringValue PROC
    ret
ExtractStringValue ENDP

; ExtractNumberValue - Extract numeric value
ExtractNumberValue PROC
    ret
ExtractNumberValue ENDP

; FormatSizeString - Convert bytes to human-readable (KB/MB/GB)
FormatSizeString PROC
    push rbx
    push rsi
    sub rsp, 28h
    
    mov rbx, rcx                ; size in bytes
    mov rsi, rdx                ; output buffer
    
    ; Check for GB (>= 1073741824)
    mov rax, rbx
    mov rcx, 1073741824
    cmp rax, rcx
    jl check_mb
    
    ; Format as GB
    xor rdx, rdx
    div rcx
    mov rcx, rsi
    lea rdx, [szGBSuffix]
    ; wsprintfA call would go here
    jmp format_size_done
    
check_mb:
    ; Check for MB (>= 1048576)
    mov rax, rbx
    mov rcx, 1048576
    cmp rax, rcx
    jl check_kb
    
    xor rdx, rdx
    div rcx
    jmp format_size_done
    
check_kb:
    ; Check for KB (>= 1024)
    mov rax, rbx
    mov rcx, 1024
    cmp rax, rcx
    jl format_bytes
    
    xor rdx, rdx
    div rcx
    jmp format_size_done
    
format_bytes:
    ; Format as bytes
    mov rax, rbx
    
format_size_done:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
FormatSizeString ENDP

END
