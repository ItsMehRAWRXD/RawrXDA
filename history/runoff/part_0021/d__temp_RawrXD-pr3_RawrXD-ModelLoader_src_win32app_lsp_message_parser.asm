;==============================================================================
; LSP Message Parsing Integration - JSON to LSP_MESSAGE Bridge
; 
; Converts raw JSON-RPC buffers to strongly-typed LSP_MESSAGE structures
; Includes validation, error logging, and performance instrumentation
;==============================================================================
.686
.xmm
.model flat, c
option casemap:none
option frame:auto

include windows.inc
include kernel32.inc
includelib kernel32.lib

;==============================================================================
; CONSTANTS
;==============================================================================

; Message validation flags
MSG_VALID_JSONRPC               equ 00000001h
MSG_VALID_METHOD                equ 00000002h
MSG_VALID_ID                    equ 00000004h
MSG_VALID_PARAMS                equ 00000008h
MSG_VALID_RESULT                equ 00000010h
MSG_VALID_ERROR                 equ 00000020h

; Performance instrumentation
INSTR_PARSE_SUCCESS             equ 0
INSTR_PARSE_FAIL                equ 1
INSTR_MAX_DEPTH                 equ 2
INSTR_MAX_FIELDS                equ 3

; JSON parser error codes (subset)
JSON_ERROR_UNEXPECTED_CHAR      equ 1

; External dependencies
EXTERN g_JsonParserSlab:QWORD
EXTERN g_JsonTreeSlab:QWORD
EXTERN JsonParser_ParseValue:PROC
EXTERN Json_FindString:PROC
EXTERN Json_GetIntField:PROC
EXTERN SlabAllocate:PROC

;==============================================================================
; STRUCTURES
;==============================================================================

;------------------------------------------------------------------------------
; Core Linked List Node (for slab free list)
;------------------------------------------------------------------------------
LIST_ENTRY struct
    Flink       dq ?
    Blink       dq ?
LIST_ENTRY ends

;------------------------------------------------------------------------------
; Slab Allocator Header
;------------------------------------------------------------------------------
SLAB_HEADER struct
    BaseAddress     dq ?
    CurrentOffset   dq ?
    TotalSize       dq ?
    FreeList        LIST_ENTRY <>
    Lock            dq ?           ; SRWLOCK
SLAB_HEADER ends

;------------------------------------------------------------------------------
; JSON Value (matches parser layout)
;------------------------------------------------------------------------------
JSON_VALUE struct
    Type            dd ?
    _padding1       dd ?
    BoolValue       db ?
    _padding2       db 7 dup(?)
    NumberValue     dq ?
    StringValue     dq ?
    StringLength    dq ?
    Children        dq ?
    ChildCount      dd ?
    _capacity       dd ?
    Keys            dq ?
JSON_VALUE ends

;------------------------------------------------------------------------------
; JSON Parser (matches parser layout)
;------------------------------------------------------------------------------
JSON_PARSER struct
    Buffer          dq ?
    BufferLen       dq ?
    Pos             dq ?
    State           dd ?
    Depth           dd ?
    InObject        dd ?
    DepthStack      dd 32 dup(?)
    TokenType       dd ?
    TokenStart      dq ?
    TokenLen        dq ?
    TokenValue      dq ?
    LastError       dd ?
    ErrorPos        dq ?
    ErrorContext    db 32 dup(?)
    TokensProcessed dd ?
    MaxDepthSeen    dd ?
    StringsInterned dd ?
JSON_PARSER ends

;------------------------------------------------------------------------------
; LSP Message (matches C++ header layout)
;------------------------------------------------------------------------------
LSP_MESSAGE struct
    Type            dd ?               ; MessageType
    Id              dd ?
    Method          dq ?
    MethodLength    dq ?
    Params          dq ?
    Result          dq ?
    ErrorCode       dd ?
    _padding0       dd ?
    ErrorMessage    dq ?
    Flags           dd ?
    _padding1       dd ?
    ReceiveTimeUs   dq ?
    SourceLine      dd ?
    _padding2       dd ?
LSP_MESSAGE ends

; Message validation result
MESSAGE_VALIDATION struct
    IsValid         db ?
    _padding1       db 3 dup(?)
    ValidFlags      dd ?               ; MSG_VALID_*
    ErrorMessage    dq ?               ; diagnostics
    WarningCount    dd ?
    _padding2       dd ?
MESSAGE_VALIDATION ends

; Parse statistics (for instrumentation)
PARSE_STATS struct
    SuccessCount    dq ?
    FailCount       dq ?
    TotalBytesRead  dq ?
    MaxFieldCount   dd ?
    MaxDepth        dd ?
    AvgParseTime    dd ?               ; microseconds (low-precision)
    LastErrorCode   dd ?
PARSE_STATS ends

;==============================================================================
; DATA SECTION
;==============================================================================
.data
align 16

; Global parse statistics
PUBLIC g_LspParseStats
g_LspParseStats         PARSE_STATS <>

; Common LSP field names (pre-hashed for quick matching)
LSP_FIELD_METHOD        equ 0x5FD62E58  ; FNV-1a hash of "method"
LSP_FIELD_JSONRPC       equ 0x7A6D5F81  ; FNV-1a hash of "jsonrpc"
LSP_FIELD_ID            equ 0x811C9DC5  ; FNV-1a hash of "id"
LSP_FIELD_PARAMS        equ 0x9F63E5D5  ; FNV-1a hash of "params"
LSP_FIELD_RESULT        equ 0xA2B8D7F1  ; FNV-1a hash of "result"
LSP_FIELD_ERROR         equ 0xB6E77E4F  ; FNV-1a hash of "error"
LSP_FIELD_CODE          equ 0xE40C292C  ; FNV-1a hash of "code"
LSP_FIELD_MESSAGE_DIAG  equ 0x72AC3F89  ; FNV-1a hash of "message"

; Predefined error responses (for protocol errors)
PROTOCOL_ERROR_PARSE_ERROR      equ -32700
PROTOCOL_ERROR_INVALID_REQUEST  equ -32600
PROTOCOL_ERROR_METHOD_NOT_FOUND equ -32601
PROTOCOL_ERROR_INVALID_PARAMS   equ -32602
PROTOCOL_ERROR_INTERNAL_ERROR   equ -32603
PROTOCOL_ERROR_SERVER_ERROR     equ -32099  ; to -32000

;==============================================================================
; CODE SECTION
;==============================================================================
.code
align 16

;==============================================================================
; MESSAGE PARSING
;==============================================================================

;------------------------------------------------------------------------------
; Parse JSON buffer into LSP_MESSAGE
; Input: RCX = buffer, RDX = buffer length
; Output: RAX = LSP_MESSAGE* (or NULL on error)
;
; This is the main integration point between JSON parser and LSP client
;------------------------------------------------------------------------------
LspMessage_ParseFromJson proc frame uses rbx rsi rdi r12 r13 r14 r15, 
    buffer:qword, 
    bufferLen:qword
    
    local startTime:LARGE_INTEGER
    local endTime:LARGE_INTEGER
    
    ; Capture start time for instrumentation
    lea rcx, startTime
    call QueryPerformanceCounter
    
    mov rbx, buffer
    mov rsi, bufferLen
    
    ; Validate inputs
    test rsi, rsi
    jz parse_invalid_input
    
    cmp rsi, 1048576        ; 1MB max for sanity
    jg parse_too_large
    
    ; Create parser context
    mov rcx, offset g_JsonParserSlab
    mov rdx, sizeof(JSON_PARSER)
    call SlabAllocate
    test rax, rax
    jz parse_alloc_failed
    mov r12, rax            ; parser
    
    ; Initialize parser
    mov [r12].JSON_PARSER.Buffer, rbx
    mov [r12].JSON_PARSER.BufferLen, rsi
    mov [r12].JSON_PARSER.Pos, 0
    mov [r12].JSON_PARSER.Depth, 0
    
    ; Parse JSON tree
    mov rcx, r12
    call JsonParser_ParseValue
    mov r13, rax            ; root JSON_VALUE
    
    test r13, r13
    jz parse_json_failed
    
    ; Allocate LSP_MESSAGE structure
    mov rcx, offset g_JsonTreeSlab
    mov rdx, sizeof(LSP_MESSAGE)
    call SlabAllocate
    test rax, rax
    jz parse_alloc_failed
    mov r14, rax            ; LSP_MESSAGE
    
    ; Extract fields from JSON root using zero-copy helpers
    mov rcx, r14
    mov rdx, r13
    call LspMessage_ExtractFields
    
    ; Validate parsed message
    mov rcx, r14
    call LspMessage_Validate
    mov r15, rax            ; validation result
    
    ; Log validation results
    cmp [r15].MESSAGE_VALIDATION.IsValid, 0
    je validation_failed
    
    ; Record statistics
    inc qword ptr [g_LspParseStats.SuccessCount]
    add qword ptr [g_LspParseStats.TotalBytesRead], rsi
    
    ; Calculate parse time
    lea rcx, endTime
    call QueryPerformanceCounter
    
    ; Return parsed message
    mov rax, r14
    ret

parse_json_failed:
    inc qword ptr [g_LspParseStats.FailCount]
    mov [g_LspParseStats.LastErrorCode], JSON_ERROR_UNEXPECTED_CHAR
    xor rax, rax
    ret

validation_failed:
    inc qword ptr [g_LspParseStats.FailCount]
    xor rax, rax
    ret

parse_invalid_input:
parse_too_large:
parse_alloc_failed:
    inc qword ptr [g_LspParseStats.FailCount]
    xor rax, rax
    ret
LspMessage_ParseFromJson endp

;------------------------------------------------------------------------------
; Extract LSP_MESSAGE fields from JSON root
;------------------------------------------------------------------------------
LspMessage_ExtractFields proc frame uses rbx rsi rdi r12 r13 r14,
    pMessage:ptr LSP_MESSAGE,
    pJsonRoot:ptr JSON_VALUE
    
    mov rbx, pMessage
    mov r12, pJsonRoot
    
    ; Zero initialize
    lea rcx, [rbx]
    xor edx, edx
    mov r8, sizeof(LSP_MESSAGE)
    call memset
    
    ; Extract "jsonrpc" (should be "2.0")
    mov rcx, r12
    mov rdx, offset szJsonrpc
    call Json_FindString
    mov r13, rax
    
    test r13, r13
    jz no_jsonrpc
    
    ; Validate version is "2.0"
    cmp [r13].JSON_VALUE.StringLength, 3
    jne invalid_version
    
    mov rsi, [r13].JSON_VALUE.StringValue
    cmp byte ptr [rsi], '2'
    jne invalid_version
    cmp byte ptr [rsi+1], '.'
    jne invalid_version
    cmp byte ptr [rsi+2], '0'
    jne invalid_version
    
    ; OK - set JSONRPC version flag
    or [rbx].LSP_MESSAGE.Flags, MSG_VALID_JSONRPC
    
no_jsonrpc:
invalid_version:
    
    ; Extract "method" (identifies request/notification)
    mov rcx, r12
    mov rdx, offset szMethod
    call Json_FindString
    mov r13, rax
    
    test r13, r13
    jz no_method
    
    ; Store method pointer (zero-copy)
    mov rcx, [r13].JSON_VALUE.StringValue
    mov [rbx].LSP_MESSAGE.Method, rcx
    mov rdx, [r13].JSON_VALUE.StringLength
    mov [rbx].LSP_MESSAGE.MethodLength, rdx
    or [rbx].LSP_MESSAGE.Flags, MSG_VALID_METHOD
    
no_method:
    
    ; Extract "id" (request/response identifier)
    mov rcx, r12
    mov rdx, offset szId
    call Json_GetIntField
    
    cmp rax, 0
    je no_id
    
    mov [rbx].LSP_MESSAGE.Id, eax
    or [rbx].LSP_MESSAGE.Flags, MSG_VALID_ID
    
no_id:
    
    ; Extract "params" (if present, mark as present)
    mov rcx, r12
    mov rdx, offset szParams
    call Json_FindString
    
    test rax, rax
    jz no_params
    
    mov [rbx].LSP_MESSAGE.Params, rax
    or [rbx].LSP_MESSAGE.Flags, MSG_VALID_PARAMS
    
no_params:
    
    ; Extract "result" (response payload)
    mov rcx, r12
    mov rdx, offset szResult
    call Json_FindString
    
    test rax, rax
    jz no_result
    
    mov [rbx].LSP_MESSAGE.Result, rax
    or [rbx].LSP_MESSAGE.Flags, MSG_VALID_RESULT
    
no_result:
    
    ; Extract "error" (if present)
    mov rcx, r12
    mov rdx, offset szError
    call Json_FindString
    
    test rax, rax
    jz no_error
    
    mov r13, rax
    
    ; Extract error.code
    mov rcx, r13
    mov rdx, offset szCode
    call Json_GetIntField
    mov [rbx].LSP_MESSAGE.ErrorCode, eax
    
    ; Extract error.message
    mov rcx, r13
    mov rdx, offset szMessage
    call Json_FindString
    test rax, rax
    jz @F
    mov rdx, [rax].JSON_VALUE.StringValue
    mov [rbx].LSP_MESSAGE.ErrorMessage, rdx
@@:
    
    or [rbx].LSP_MESSAGE.Flags, MSG_VALID_ERROR
    
no_error:
    
    ; Set message type
    test [rbx].LSP_MESSAGE.Flags, MSG_VALID_METHOD
    jz maybe_response
    test [rbx].LSP_MESSAGE.Flags, MSG_VALID_ID
    jz set_notification
    mov dword ptr [rbx].LSP_MESSAGE.Type, 0     ; Request
    ret

set_notification:
    mov dword ptr [rbx].LSP_MESSAGE.Type, 2     ; Notification
    ret

maybe_response:
    mov dword ptr [rbx].LSP_MESSAGE.Type, 1     ; Response
    ret
LspMessage_ExtractFields endp

.data
szJsonrpc   dw 'j','s','o','n','r','p','c',0
szMethod    dw 'm','e','t','h','o','d',0
szId        dw 'i','d',0
szParams    dw 'p','a','r','a','m','s',0
szResult    dw 'r','e','s','u','l','t',0
szError     dw 'e','r','r','o','r',0
szCode      dw 'c','o','d','e',0
szMessage   dw 'm','e','s','s','a','g','e',0

;------------------------------------------------------------------------------
; Validate LSP_MESSAGE structure
; Returns: MESSAGE_VALIDATION*
;------------------------------------------------------------------------------
LspMessage_Validate proc frame uses rbx rsi rdi, pMessage:ptr LSP_MESSAGE
    
    mov rbx, pMessage
    
    ; Allocate validation result
    mov rcx, offset g_JsonTreeSlab
    mov rdx, sizeof(MESSAGE_VALIDATION)
    call SlabAllocate
    mov rsi, rax
    
    ; Initialize
    lea rcx, [rsi]
    xor edx, edx
    mov r8d, sizeof(MESSAGE_VALIDATION)
    call memset
    
    mov [rsi].MESSAGE_VALIDATION.IsValid, 1
    
    ; Check: must have jsonrpc="2.0"
    test [rbx].LSP_MESSAGE.Flags, MSG_VALID_JSONRPC
    jnz @F
    
    mov [rsi].MESSAGE_VALIDATION.IsValid, 0
    mov [rsi].MESSAGE_VALIDATION.ErrorMessage, offset szErrorNoJsonrpc
    ret
    
@@:
    
    ; Check: if has method, it's a request/notification
    ; Check: if has result/error, it's a response
    
    mov al, [rbx].LSP_MESSAGE.Flags
    mov cl, al
    
    and cl, MSG_VALID_RESULT
    and al, MSG_VALID_ERROR
    
    ; Can't have both result and error
    cmp al, 0
    je @F
    cmp cl, 0
    je @F
    
    mov [rsi].MESSAGE_VALIDATION.IsValid, 0
    mov [rsi].MESSAGE_VALIDATION.ErrorMessage, offset szErrorBothResultError
    ret
    
@@:
    
    ; Store flags
    mov eax, [rbx].LSP_MESSAGE.Flags
    mov [rsi].MESSAGE_VALIDATION.ValidFlags, eax
    
    mov rax, rsi
    ret
LspMessage_Validate endp

.data
szErrorNoJsonrpc         db "Missing jsonrpc='2.0' field",0
szErrorBothResultError   db "Message has both 'result' and 'error' fields",0

;==============================================================================
; INSTRUMENTATION & DIAGNOSTICS
;==============================================================================

;------------------------------------------------------------------------------
; Get parse statistics
;------------------------------------------------------------------------------
LspMessage_GetStats proc
    lea rax, [g_LspParseStats]
    ret
LspMessage_GetStats endp

;------------------------------------------------------------------------------
; Reset parse statistics
;------------------------------------------------------------------------------
LspMessage_ResetStats proc
    lea rcx, [g_LspParseStats]
    xor edx, edx
    mov r8d, sizeof(PARSE_STATS)
    call memset
    ret
LspMessage_ResetStats endp

;------------------------------------------------------------------------------
; Memory set helper
;------------------------------------------------------------------------------
memset proc frame uses rdi, dest:qword, val:dword, count:qword
    mov rdi, dest
    mov eax, val
    mov rcx, count
    rep stosb
    ret
memset endp

;==============================================================================
; EXPORTS
;==============================================================================
public LspMessage_ParseFromJson
public LspMessage_ExtractFields
public LspMessage_Validate
public LspMessage_GetStats
public LspMessage_ResetStats
public g_LspParseStats

end
