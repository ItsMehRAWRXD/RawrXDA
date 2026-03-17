;==============================================================================
; error_handler.asm - Production-Ready Centralized Error Handler for RawrXD
; ==============================================================================
; PHASE 4: ERROR HANDLING ENHANCEMENT
; Implements comprehensive error handling as per AI Toolkit Production Readiness:
; - Centralized exception capture
; - Structured error logging
; - Error recovery mechanisms
; - Resource cleanup on error
; - Error rate tracking and alerting
; - Integration with all Priority 1 modules
;==============================================================================

option casemap:none

; ============================================================================
; Local logging levels (kept consistent with the broader system)
; ============================================================================
LOG_LEVEL_DEBUG             EQU 0
LOG_LEVEL_INFO              EQU 1
LOG_LEVEL_WARN              EQU 2
LOG_LEVEL_ERROR             EQU 3

;==============================================================================
; ERROR CONSTANTS
;==============================================================================
ERROR_SEVERITY_INFO        EQU 0
ERROR_SEVERITY_WARNING     EQU 1
ERROR_SEVERITY_ERROR       EQU 2
ERROR_SEVERITY_CRITICAL    EQU 3
ERROR_SEVERITY_FATAL       EQU 4

ERROR_CATEGORY_MEMORY      EQU 0
ERROR_CATEGORY_IO          EQU 1
ERROR_CATEGORY_NETWORK     EQU 2
ERROR_CATEGORY_VALIDATION  EQU 3
ERROR_CATEGORY_LOGIC       EQU 4
ERROR_CATEGORY_SYSTEM      EQU 5

ERROR_RECOVERY_RETRY       EQU 0
ERROR_RECOVERY_SKIP        EQU 1
ERROR_RECOVERY_ABORT       EQU 2
ERROR_RECOVERY_FALLBACK    EQU 3

MAX_ERROR_MESSAGE          EQU 512
MAX_ERROR_STACK            EQU 32
ERROR_RATE_WINDOW          EQU 60000  ; 60 seconds

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN GetLastError:PROC
EXTERN SetLastError:PROC
EXTERN GetTickCount64:PROC
EXTERN OutputDebugStringA:PROC
EXTERN ExitProcess:PROC
EXTERN wsprintfA:PROC
EXTERN lstrlenA:PROC
EXTERN Logger_LogStructured:PROC
EXTERN Config_IsProduction:PROC
EXTERN AddVectoredExceptionHandler:PROC
EXTERN RemoveVectoredExceptionHandler:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC

;==============================================================================
; STRUCTURES
;==============================================================================
ERROR_CONTEXT STRUCT
    error_code          DWORD ?
    severity            DWORD ?
    category            DWORD ?
    timestamp           QWORD ?
    message_ptr         QWORD ?
    file_ptr            QWORD ?
    line_number         DWORD ?
    recovery_action     DWORD ?
    data1               QWORD ?
    data2               QWORD ?
ERROR_CONTEXT ENDS

ERROR_STATS STRUCT
    total_errors        QWORD ?
    errors_info         QWORD ?
    errors_warning      QWORD ?
    errors_error        QWORD ?
    errors_critical     QWORD ?
    errors_fatal        QWORD ?
    last_error_time     QWORD ?
    errors_last_minute  QWORD ?
    error_rate          QWORD ?     ; Errors per minute
ERROR_STATS ENDS

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data?
    g_error_stats       ERROR_STATS <>
    g_error_stack       ERROR_CONTEXT MAX_ERROR_STACK DUP (<>)
    g_error_stack_ptr   QWORD ?
    g_error_buffer      BYTE MAX_ERROR_MESSAGE DUP (?)
    g_error_initialized DWORD ?
    g_veh_handle        QWORD ?

.data
    hexDigits           BYTE "0123456789ABCDEF", 0

    ; Fixed-format output line; hex spans overwritten at runtime
    vehMsg              BYTE "VEH: code=0x"
vehMsgCodeHex          BYTE "00000000"
vehMsgAddrPart         BYTE " addr=0x"
vehMsgAddrHex          BYTE "0000000000000000", 13, 10, 0

    ; Error severity strings
    szSeverityInfo      BYTE "INFO", 0
    szSeverityWarning   BYTE "WARNING", 0
    szSeverityError     BYTE "ERROR", 0
    szSeverityCritical  BYTE "CRITICAL", 0
    szSeverityFatal     BYTE "FATAL", 0
    
    ; Error category strings
    szCategoryMemory    BYTE "MEMORY", 0
    szCategoryIO        BYTE "I/O", 0
    szCategoryNetwork   BYTE "NETWORK", 0
    szCategoryValidation BYTE "VALIDATION", 0
    szCategoryLogic     BYTE "LOGIC", 0
    szCategorySystem    BYTE "SYSTEM", 0
    
    ; Error message format
    szErrorFormat       BYTE "[%s] [%s] Code:%d - %s", 13, 10, 0
    szRateAlert         BYTE "HIGH ERROR RATE DETECTED: %d errors/min", 13, 10, 0
    szFatalError        BYTE "FATAL ERROR: Terminating process", 13, 10, 0
    
    ; Default error messages
    szErrMemoryAlloc    BYTE "Memory allocation failed", 0
    szErrFileOpen       BYTE "Failed to open file", 0
    szErrNetworkTimeout BYTE "Network operation timed out", 0
    szErrInvalidParam   BYTE "Invalid parameter", 0
    szErrLogicFailure   BYTE "Logic error", 0
    szErrSystemCall     BYTE "System call failed", 0
    
    ; Initialization messages
    szErrorInitStart    BYTE "Error handler initialization starting", 0
    szErrorInitSuccess  BYTE "Error handler initialization successful", 0

;==============================================================================
; CODE SEGMENT
;==============================================================================
.code

PUBLIC WriteHex32
PUBLIC WriteHex64

;==============================================================================
; WriteHex32
; eax = value
; rdx = dest (8 bytes)
;==============================================================================
WriteHex32 PROC
    push rbx
    lea r9, hexDigits
    mov ebx, eax
    mov ecx, 8

wh32_loop:
    mov eax, ebx
    shr eax, 28
    and eax, 0Fh
    mov al, byte ptr [r9 + rax]
    mov byte ptr [rdx], al
    inc rdx
    shl ebx, 4
    dec ecx
    jnz wh32_loop

    pop rbx
    ret
WriteHex32 ENDP

;==============================================================================
; WriteHex64
; rax = value
; rdx = dest (16 bytes)
;==============================================================================
WriteHex64 PROC
    push rbx
    lea r9, hexDigits
    mov rbx, rax
    mov ecx, 16

wh64_loop:
    mov rax, rbx
    shr rax, 60
    and eax, 0Fh
    mov al, byte ptr [r9 + rax]
    mov byte ptr [rdx], al
    inc rdx
    shl rbx, 4
    dec ecx
    jnz wh64_loop

    pop rbx
    ret
WriteHex64 ENDP

;==============================================================================
; Vectored exception handler
; rcx = PEXCEPTION_POINTERS
;==============================================================================
VehHandler PROC
    push rbx
    push rsi
    sub rsp, 40h

    mov rsi, rcx                    ; ExceptionPointers
    mov rbx, qword ptr [rsi]        ; ExceptionRecord

    ; Patch in exception code
    mov eax, dword ptr [rbx]        ; ExceptionCode
    lea rdx, vehMsgCodeHex
    call WriteHex32

    ; Patch in exception address
    mov rax, qword ptr [rbx + 10h]  ; ExceptionAddress
    lea rdx, vehMsgAddrHex
    call WriteHex64

    ; Write line to stdout
    mov ecx, -11
    call GetStdHandle

    ; Compute length
    lea rbx, vehMsg
    xor r8d, r8d
veh_len_loop:
    cmp byte ptr [rbx + r8], 0
    je veh_len_done
    inc r8d
    jmp veh_len_loop
veh_len_done:

    mov rcx, rax
    mov rdx, rbx
    lea r9, [rsp + 30h]
    mov qword ptr [rsp + 20h], 0
    call WriteFile

    ; EXCEPTION_CONTINUE_SEARCH
    xor eax, eax

    add rsp, 40h
    pop rsi
    pop rbx
    ret
VehHandler ENDP

;==============================================================================
; PUBLIC: ErrorHandler_Initialize() -> eax
; Initializes the centralized error handling system
;==============================================================================
PUBLIC ErrorHandler_Initialize
ALIGN 16
ErrorHandler_Initialize PROC
    PUSH rbx
    SUB rsp, 32
    
    ; Check if already initialized
    MOV eax, DWORD PTR [g_error_initialized]
    TEST eax, eax
    JNZ init_already_done
    
    ; Log initialization start
    LEA rcx, [szErrorInitStart]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Zero out error stats
    LEA rbx, [g_error_stats]
    XOR rax, rax
    MOV QWORD PTR [rbx + ERROR_STATS.total_errors], rax
    MOV QWORD PTR [rbx + ERROR_STATS.errors_info], rax
    MOV QWORD PTR [rbx + ERROR_STATS.errors_warning], rax
    MOV QWORD PTR [rbx + ERROR_STATS.errors_error], rax
    MOV QWORD PTR [rbx + ERROR_STATS.errors_critical], rax
    MOV QWORD PTR [rbx + ERROR_STATS.errors_fatal], rax
    MOV QWORD PTR [rbx + ERROR_STATS.last_error_time], rax
    MOV QWORD PTR [rbx + ERROR_STATS.errors_last_minute], rax
    MOV QWORD PTR [rbx + ERROR_STATS.error_rate], rax
    
    ; Initialize error stack
    LEA rax, [g_error_stack]
    MOV QWORD PTR [g_error_stack_ptr], rax
    
    ; Mark as initialized
    MOV DWORD PTR [g_error_initialized], 1

    ; Install a vectored exception handler for last-chance diagnostics (best-effort)
    MOV rax, QWORD PTR [g_veh_handle]
    TEST rax, rax
    JNZ veh_done
    mov ecx, 1
    lea rdx, VehHandler
    call AddVectoredExceptionHandler
    mov QWORD PTR [g_veh_handle], rax
veh_done:
    
    ; Log initialization success
    LEA rcx, [szErrorInitSuccess]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    MOV eax, 1
    JMP init_done
    
init_already_done:
    MOV eax, 1
    
init_done:
    ADD rsp, 32
    POP rbx
    RET
ErrorHandler_Initialize ENDP

;==============================================================================
; PUBLIC: ErrorHandler_Capture(error_code: ecx, severity: edx, category: r8d,
;                              message: r9) -> void
; Captures an error with full context and logging
;==============================================================================
PUBLIC ErrorHandler_Capture
ALIGN 16
ErrorHandler_Capture PROC
    PUSH rbx
    PUSH rsi
    PUSH rdi
    PUSH r12
    PUSH r13
    PUSH r14
    SUB rsp, 128
    
    ; Save parameters
    MOV r12d, ecx           ; error_code
    MOV r13d, edx           ; severity
    MOV r14d, r8d           ; category
    MOV rsi, r9             ; message
    
    ; Get current timestamp
    CALL GetTickCount64
    MOV rdi, rax            ; timestamp
    
    ; Update global statistics
    LEA rbx, [g_error_stats]
    LOCK INC QWORD PTR [rbx + ERROR_STATS.total_errors]
    
    ; Update severity-specific counter
    CMP r13d, ERROR_SEVERITY_INFO
    JE update_info
    CMP r13d, ERROR_SEVERITY_WARNING
    JE update_warning
    CMP r13d, ERROR_SEVERITY_ERROR
    JE update_error
    CMP r13d, ERROR_SEVERITY_CRITICAL
    JE update_critical
    CMP r13d, ERROR_SEVERITY_FATAL
    JE update_fatal
    JMP update_done
    
update_info:
    LOCK INC QWORD PTR [rbx + ERROR_STATS.errors_info]
    JMP update_done
    
update_warning:
    LOCK INC QWORD PTR [rbx + ERROR_STATS.errors_warning]
    JMP update_done
    
update_error:
    LOCK INC QWORD PTR [rbx + ERROR_STATS.errors_error]
    JMP update_done
    
update_critical:
    LOCK INC QWORD PTR [rbx + ERROR_STATS.errors_critical]
    JMP update_done
    
update_fatal:
    LOCK INC QWORD PTR [rbx + ERROR_STATS.errors_fatal]
    
update_done:
    ; Update error rate tracking
    MOV rax, QWORD PTR [rbx + ERROR_STATS.last_error_time]
    TEST rax, rax
    JZ first_error
    
    ; Calculate time since last error
    MOV rcx, rdi
    SUB rcx, rax
    CMP rcx, ERROR_RATE_WINDOW
    JG reset_rate
    
    ; Within window, increment count
    LOCK INC QWORD PTR [rbx + ERROR_STATS.errors_last_minute]
    JMP rate_updated
    
reset_rate:
    ; Outside window, reset count
    MOV QWORD PTR [rbx + ERROR_STATS.errors_last_minute], 1
    JMP rate_updated
    
first_error:
    MOV QWORD PTR [rbx + ERROR_STATS.errors_last_minute], 1
    
rate_updated:
    ; Update last error timestamp
    MOV QWORD PTR [rbx + ERROR_STATS.last_error_time], rdi
    
    ; Calculate error rate (errors per minute)
    MOV rax, QWORD PTR [rbx + ERROR_STATS.errors_last_minute]
    MOV rcx, 60000
    MUL rcx
    MOV rcx, rdi
    MOV rax, QWORD PTR [rbx + ERROR_STATS.last_error_time]
    SUB rcx, rax
    TEST rcx, rcx
    JZ skip_rate_calc
    DIV rcx
    MOV QWORD PTR [rbx + ERROR_STATS.error_rate], rax
    
skip_rate_calc:
    ; Check for high error rate (>10 errors/min = critical threshold)
    MOV rax, QWORD PTR [rbx + ERROR_STATS.errors_last_minute]
    CMP rax, 10
    JLE normal_rate
    
    ; High error rate detected - log alert
    LEA rcx, [g_error_buffer]
    LEA rdx, [szRateAlert]
    MOV r8, rax
    CALL wsprintfA
    
    LEA rcx, [g_error_buffer]
    CALL OutputDebugStringA
    
normal_rate:
    ; Format and log error message
    CALL ErrorHandler_FormatMessage
    
    ; Push error onto stack (if space available)
    MOV rax, QWORD PTR [g_error_stack_ptr]
    LEA rcx, [g_error_stack]
    LEA rdx, [g_error_stack + SIZE ERROR_CONTEXT * MAX_ERROR_STACK]
    CMP rax, rdx
    JGE stack_full
    
    ; Store error context on stack
    MOV DWORD PTR [rax + ERROR_CONTEXT.error_code], r12d
    MOV DWORD PTR [rax + ERROR_CONTEXT.severity], r13d
    MOV DWORD PTR [rax + ERROR_CONTEXT.category], r14d
    MOV QWORD PTR [rax + ERROR_CONTEXT.timestamp], rdi
    MOV QWORD PTR [rax + ERROR_CONTEXT.message_ptr], rsi
    
    ADD rax, SIZE ERROR_CONTEXT
    MOV QWORD PTR [g_error_stack_ptr], rax
    
stack_full:
    ; Handle fatal errors
    CMP r13d, ERROR_SEVERITY_FATAL
    JNE capture_done
    
    LEA rcx, [szFatalError]
    CALL OutputDebugStringA
    
    ; In production, we might want graceful shutdown
    CALL Config_IsProduction
    TEST rax, rax
    JZ capture_done
    
    ; Production: Exit gracefully with error code
    MOV ecx, r12d
    CALL ExitProcess
    
capture_done:
    ADD rsp, 128
    POP r14
    POP r13
    POP r12
    POP rdi
    POP rsi
    POP rbx
    RET
ErrorHandler_Capture ENDP

;==============================================================================
; PRIVATE: ErrorHandler_FormatMessage() -> void
; Formats error message using current error context
; Parameters: r12d=error_code, r13d=severity, r14d=category, rsi=message
;==============================================================================
ALIGN 16
ErrorHandler_FormatMessage PROC
    PUSH rbx
    PUSH r12
    PUSH r13
    PUSH r14
    PUSH rsi
    SUB rsp, 64
    
    ; Get severity string
    CALL ErrorHandler_GetSeverityString
    MOV [rsp + 32], rax     ; severity_str
    
    ; Get category string
    MOV ecx, r14d
    CALL ErrorHandler_GetCategoryString
    MOV [rsp + 40], rax     ; category_str
    
    ; Format message
    LEA rcx, [g_error_buffer]
    LEA rdx, [szErrorFormat]
    MOV r8, [rsp + 32]      ; severity
    MOV r9, [rsp + 40]      ; category
    MOV QWORD PTR [rsp + 48], r12   ; error_code
    MOV QWORD PTR [rsp + 56], rsi   ; message
    CALL wsprintfA
    
    ; Output to debug console
    LEA rcx, [g_error_buffer]
    CALL OutputDebugStringA
    
    ; Also log through structured logging
    LEA rcx, [g_error_buffer]
    MOV edx, r13d           ; Use severity as log level
    CALL Logger_LogStructured
    
    ADD rsp, 64
    POP rsi
    POP r14
    POP r13
    POP r12
    POP rbx
    RET
ErrorHandler_FormatMessage ENDP

;==============================================================================
; PRIVATE: ErrorHandler_GetSeverityString(severity: r13d) -> rax
; Returns pointer to severity string
;==============================================================================
ALIGN 16
ErrorHandler_GetSeverityString PROC
    CMP r13d, ERROR_SEVERITY_INFO
    JE sev_info
    CMP r13d, ERROR_SEVERITY_WARNING
    JE sev_warning
    CMP r13d, ERROR_SEVERITY_ERROR
    JE sev_error
    CMP r13d, ERROR_SEVERITY_CRITICAL
    JE sev_critical
    CMP r13d, ERROR_SEVERITY_FATAL
    JE sev_fatal
    
    LEA rax, [szSeverityError]
    RET
    
sev_info:
    LEA rax, [szSeverityInfo]
    RET
    
sev_warning:
    LEA rax, [szSeverityWarning]
    RET
    
sev_error:
    LEA rax, [szSeverityError]
    RET
    
sev_critical:
    LEA rax, [szSeverityCritical]
    RET
    
sev_fatal:
    LEA rax, [szSeverityFatal]
    RET
ErrorHandler_GetSeverityString ENDP

;==============================================================================
; PRIVATE: ErrorHandler_GetCategoryString(category: ecx) -> rax
; Returns pointer to category string
;==============================================================================
ALIGN 16
ErrorHandler_GetCategoryString PROC
    CMP ecx, ERROR_CATEGORY_MEMORY
    JE cat_memory
    CMP ecx, ERROR_CATEGORY_IO
    JE cat_io
    CMP ecx, ERROR_CATEGORY_NETWORK
    JE cat_network
    CMP ecx, ERROR_CATEGORY_VALIDATION
    JE cat_validation
    CMP ecx, ERROR_CATEGORY_LOGIC
    JE cat_logic
    CMP ecx, ERROR_CATEGORY_SYSTEM
    JE cat_system
    
    LEA rax, [szCategorySystem]
    RET
    
cat_memory:
    LEA rax, [szCategoryMemory]
    RET
    
cat_io:
    LEA rax, [szCategoryIO]
    RET
    
cat_network:
    LEA rax, [szCategoryNetwork]
    RET
    
cat_validation:
    LEA rax, [szCategoryValidation]
    RET
    
cat_logic:
    LEA rax, [szCategoryLogic]
    RET
    
cat_system:
    LEA rax, [szCategorySystem]
    RET
ErrorHandler_GetCategoryString ENDP

;==============================================================================
; PUBLIC: ErrorHandler_GetStats(stats_ptr: rcx) -> eax
; Retrieves current error statistics
;==============================================================================
PUBLIC ErrorHandler_GetStats
ALIGN 16
ErrorHandler_GetStats PROC
    TEST rcx, rcx
    JZ get_stats_fail
    
    ; Copy error stats to user buffer
    LEA rax, [g_error_stats]
    MOV rdx, SIZE ERROR_STATS
    
copy_loop:
    TEST rdx, rdx
    JZ get_stats_success
    
    MOV r8b, BYTE PTR [rax]
    MOV BYTE PTR [rcx], r8b
    INC rax
    INC rcx
    DEC rdx
    JMP copy_loop
    
get_stats_success:
    MOV eax, 1
    RET
    
get_stats_fail:
    XOR eax, eax
    RET
ErrorHandler_GetStats ENDP

;==============================================================================
; PUBLIC: ErrorHandler_Reset() -> void
; Resets error statistics and stack
;==============================================================================
PUBLIC ErrorHandler_Reset
ALIGN 16
ErrorHandler_Reset PROC
    ; Reset error stats
    LEA rax, [g_error_stats]
    XOR rcx, rcx
    MOV QWORD PTR [rax + ERROR_STATS.total_errors], rcx
    MOV QWORD PTR [rax + ERROR_STATS.errors_info], rcx
    MOV QWORD PTR [rax + ERROR_STATS.errors_warning], rcx
    MOV QWORD PTR [rax + ERROR_STATS.errors_error], rcx
    MOV QWORD PTR [rax + ERROR_STATS.errors_critical], rcx
    MOV QWORD PTR [rax + ERROR_STATS.errors_fatal], rcx
    MOV QWORD PTR [rax + ERROR_STATS.last_error_time], rcx
    MOV QWORD PTR [rax + ERROR_STATS.errors_last_minute], rcx
    MOV QWORD PTR [rax + ERROR_STATS.error_rate], rcx
    
    ; Reset error stack
    LEA rax, [g_error_stack]
    MOV QWORD PTR [g_error_stack_ptr], rax
    
    RET
ErrorHandler_Reset ENDP

;==============================================================================
; PUBLIC: ErrorHandler_Cleanup() -> void
; Cleanup error handling resources
;==============================================================================
PUBLIC ErrorHandler_Cleanup
ALIGN 16
ErrorHandler_Cleanup PROC
    sub rsp, 28h

    mov rax, QWORD PTR [g_veh_handle]
    test rax, rax
    jz veh_cleanup_done
    mov rcx, rax
    call RemoveVectoredExceptionHandler
    mov QWORD PTR [g_veh_handle], 0
veh_cleanup_done:

    CALL ErrorHandler_Reset
    MOV DWORD PTR [g_error_initialized], 0
    add rsp, 28h
    RET
ErrorHandler_Cleanup ENDP

END
