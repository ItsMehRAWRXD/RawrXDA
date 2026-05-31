; =============================================================================
; RawrXD_QueryEngine.asm - Streaming LLM Response Parser
; Based on Claude Code's QueryEngine (46K lines in leak)
; =============================================================================

OPTION CASemap:NONE
OPTION WIN64:3

INCLUDE \masm64\include64\win64.inc

; Parser states
PARSE_STATE_INIT            EQU 0
PARSE_STATE_READING         EQU 1
PARSE_STATE_TOOL_START      EQU 2
PARSE_STATE_CONTENT         EQU 4

; Buffer sizes
STREAM_BUFFER_SIZE          EQU 65536
MAX_TOOL_ARGS_SIZE          EQU 32768
MAX_CONTENT_SIZE            EQU 1048576

; Structures
QUERY_PARSER_CONTEXT STRUCT
    state               DWORD ?
    nestingDepth        DWORD ?
    braceCount          DWORD ?
    bracketCount        DWORD ?
    streamBuffer        QWORD ?
    bufferWritePos      DWORD ?
    bufferReadPos       DWORD ?
    bufferSize          DWORD ?
    currentToken        DWORD ?
    tokenBuffer         QWORD ?
    tokenLength         DWORD ?
    toolName            BYTE 128 DUP(?)
    toolArgsBuffer      QWORD ?
    toolArgsLength      DWORD ?
    toolArgsCapacity    DWORD ?
    contentBuffer       QWORD ?
    contentLength       DWORD ?
    contentCapacity     DWORD ?
    thinkingBuffer      QWORD ?
    thinkingLength      DWORD ?
    isThinking          BYTE ?
    onContent           QWORD ?
    onToolCall          QWORD ?
    onThinking          QWORD ?
    onError             QWORD ?
    onComplete          QWORD ?
    userData            QWORD ?
QUERY_PARSER_CONTEXT ENDS

RETRY_CONTEXT STRUCT
    attemptNumber       DWORD ?
    maxAttempts         DWORD ?
    baseDelayMs         DWORD ?
    currentDelayMs      DWORD ?
    lastError           DWORD ?
    exponentialBackoff  BYTE ?
    jitterEnabled       BYTE ?
RETRY_CONTEXT ENDS

QUERY_ENGINE_CONTEXT STRUCT
    hSession            QWORD ?
    hConnect            QWORD ?
    hRequest            QWORD ?
    parser              QUERY_PARSER_CONTEXT <>
    retry               RETRY_CONTEXT <>
    startTime           QWORD ?
    firstTokenTime      QWORD ?
    totalTokens         DWORD ?
    toolCalls           DWORD ?
    modelName           BYTE 64 DUP(?)
    apiKey              BYTE 128 DUP(?)
    endpoint            BYTE 256 DUP(?)
    timeoutMs           DWORD ?
    maxTokens           DWORD ?
    temperature         REAL4 ?
QUERY_ENGINE_CONTEXT ENDS

.CODE

; =============================================================================
; QueryEngine_Initialize - Initialize streaming query engine
; =============================================================================
QueryEngine_Initialize PROC FRAME
    LOCAL hHeap:QWORD
    push rbx
    push rdi
    mov rbx, rcx                    ; Context
    
    ; Zero context structure
    mov rdi, rbx
    mov rcx, (SIZEOF QUERY_ENGINE_CONTEXT / 8) + 1
    xor rax, rax
    rep stosq
    
    ; Initial retry config
    mov [rbx].QUERY_ENGINE_CONTEXT.retry.maxAttempts, 5
    mov [rbx].QUERY_ENGINE_CONTEXT.retry.baseDelayMs, 1000
    mov [rbx].QUERY_ENGINE_CONTEXT.retry.exponentialBackoff, 1
    
    ; Setup default timeout
    mov [rbx].QUERY_ENGINE_CONTEXT.timeoutMs, 30000
    
    mov rax, TRUE
    pop rdi
    pop rbx
    ret
QueryEngine_Initialize ENDP

PUBLIC QueryEngine_Initialize

END
