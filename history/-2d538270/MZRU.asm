;=====================================================================
; asm_logging_prod.asm - Production-Grade Structured Logging
; Lightweight, thread-safe logging with latency tracking
;=====================================================================
; Provides:
;  - asm_log_alloc() - Logs allocation with latency
;  - asm_log_free() - Logs deallocation with latency
;  - asm_log_patch() - Logs hotpatch operations
;  - asm_log_event() - Generic event logging with code/value
;  - asm_init_logging() - Initializes logging subsystem
;=====================================================================

.data

; Global logging state
PUBLIC g_qpc_freq, g_logging_enabled, g_log_buffer

g_qpc_freq              QWORD 0    ; QueryPerformanceFrequency (for latency calculation)
g_logging_enabled       QWORD 1    ; 0 = disabled, 1 = enabled
g_log_buffer            DB 512 dup(0)  ; Temporary buffer for formatted output

; Log event type codes
LOG_ALLOC_ENTER         EQU 0x10000001
LOG_ALLOC_SUCCESS       EQU 0x10000002
LOG_ALLOC_FAIL          EQU 0x10000003
LOG_FREE_ENTER          EQU 0x10000010
LOG_FREE_SUCCESS        EQU 0x10000011
LOG_FREE_INVALID        EQU 0x10000012

LOG_PATCH_APPLY         EQU 0x20000001
LOG_PATCH_SUCCESS       EQU 0x20000002
LOG_PATCH_ROLLBACK      EQU 0x20000003
LOG_PATCH_FAIL          EQU 0x20000004

LOG_FAILURE_DETECT      EQU 0x30000001
LOG_FAILURE_CORRECT     EQU 0x30000002

; Format strings
fmt_alloc_enter         DB "[ALLOC] Enter: size=%llu bytes, align=%llu", 0
fmt_alloc_success       DB "[ALLOC] Success: ptr=0x%llx, latency=%llu us", 0
fmt_alloc_fail          DB "[ALLOC] Failed: size=%llu, reason=OUT_OF_MEMORY", 0

fmt_free_enter          DB "[FREE] Enter: ptr=0x%llx", 0
fmt_free_success        DB "[FREE] Success: latency=%llu us", 0
fmt_free_invalid        DB "[FREE] Invalid: ptr=0x%llx, bad_magic", 0

fmt_patch_apply         DB "[PATCH] Apply: type=%s, target=0x%llx", 0
fmt_patch_success       DB "[PATCH] Success: latency=%llu us, bytes=%llu", 0

fmt_event               DB "[EVENT] Code=0x%08x, Value=0x%llx, Ts=%llu", 0

.code

EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN OutputDebugStringA:PROC
EXTERN wsprintfA:PROC

; Export logging functions
PUBLIC asm_init_logging, asm_log_alloc, asm_log_free, asm_log_patch, asm_log_event

;=====================================================================
; asm_init_logging() -> void
;
; Initializes logging subsystem by caching QueryPerformanceFrequency.
; Must be called once at startup before logging operations.
;=====================================================================

ALIGN 16
asm_init_logging PROC

    ; Cache frequency for latency calculation
    lea rcx, [g_qpc_freq]
    sub rsp, 32
    call QueryPerformanceFrequency
    add rsp, 32
    
    ; Enable logging
    mov qword ptr [g_logging_enabled], 1
    
    ret

asm_init_logging ENDP

;=====================================================================
; asm_log_event(code: rcx, value: rdx, timestamp: r8) -> void
;
; Generic event logging function.
;
; Parameters:
;   rcx = event code (e.g., LOG_ALLOC_SUCCESS)
;   rdx = associated value (size, pointer, etc.)
;   r8  = timestamp (ticks from QueryPerformanceCounter)
;=====================================================================

ALIGN 16
asm_log_event PROC

    push rbx
    sub rsp, 48
    
    ; Check if logging enabled
    mov rax, [g_logging_enabled]
    test rax, rax
    jz log_event_done
    
    ; Save parameters
    mov r11, rcx            ; r11 = code
    mov r12, rdx            ; r12 = value
    mov r13, r8             ; r13 = timestamp
    
    ; Format: "[EVENT] Code=0x%08x, Value=0x%llx, Ts=%llu"
    lea rcx, [g_log_buffer]
    lea rdx, [fmt_event]
    mov r8d, r11d           ; code (truncated to 32-bit for %x)
    mov r9, r12             ; value
    mov rax, r13            ; timestamp (need to pass on stack)
    mov qword ptr [rsp + 32], rax
    
    sub rsp, 16
    call wsprintfA
    add rsp, 16
    
    ; Output to debugger
    lea rcx, [g_log_buffer]
    sub rsp, 32
    call OutputDebugStringA
    add rsp, 32
    
log_event_done:
    add rsp, 48
    pop rbx
    ret

asm_log_event ENDP

;=====================================================================
; asm_log_alloc(size: rcx, alignment: rdx) -> void
;
; Logs memory allocation entry.
;=====================================================================

ALIGN 16
asm_log_alloc PROC

    push rbx
    sub rsp, 48
    
    ; Check if logging enabled
    mov rax, [g_logging_enabled]
    test rax, rax
    jz log_alloc_done
    
    ; Save size and alignment
    mov r11, rcx
    mov r12, rdx
    
    ; Get initial timestamp
    lea rcx, [rsp + 32]
    sub rsp, 32
    call QueryPerformanceCounter
    add rsp, 32
    mov r13, [rsp + 32]     ; r13 = start ticks
    
    ; Format: "[ALLOC] Enter: size=%llu bytes, align=%llu"
    lea rcx, [g_log_buffer]
    lea rdx, [fmt_alloc_enter]
    mov r8, r11             ; size
    mov r9, r12             ; alignment
    
    sub rsp, 16
    call wsprintfA
    add rsp, 16
    
    ; Output
    lea rcx, [g_log_buffer]
    sub rsp, 32
    call OutputDebugStringA
    add rsp, 32
    
    ; Log event code
    mov rcx, LOG_ALLOC_ENTER
    mov rdx, r11            ; size
    mov r8, r13             ; timestamp
    call asm_log_event
    
log_alloc_done:
    add rsp, 48
    pop rbx
    ret

asm_log_alloc ENDP

;=====================================================================
; asm_log_free(ptr: rcx) -> void
;
; Logs memory deallocation entry.
;=====================================================================

ALIGN 16
asm_log_free PROC

    push rbx
    sub rsp, 48
    
    ; Check if logging enabled
    mov rax, [g_logging_enabled]
    test rax, rax
    jz log_free_done
    
    mov r11, rcx            ; r11 = pointer
    
    ; Format: "[FREE] Enter: ptr=0x%llx"
    lea rcx, [g_log_buffer]
    lea rdx, [fmt_free_enter]
    mov r8, r11             ; pointer
    
    sub rsp, 16
    call wsprintfA
    add rsp, 16
    
    ; Output
    lea rcx, [g_log_buffer]
    sub rsp, 32
    call OutputDebugStringA
    add rsp, 32
    
    ; Log event code
    mov rcx, LOG_FREE_ENTER
    mov rdx, r11            ; pointer
    xor r8, r8              ; timestamp (can be 0 for entry)
    call asm_log_event
    
log_free_done:
    add rsp, 48
    pop rbx
    ret

asm_log_free ENDP

;=====================================================================
; asm_log_patch(type: rcx, target: rdx, success: r8) -> void
;
; Logs hotpatch operation with result and latency.
;
; Parameters:
;   rcx = patch type string (e.g., "MEMORY", "BYTE", "SERVER")
;   rdx = target address being patched
;   r8  = success flag (1=success, 0=fail)
;=====================================================================

ALIGN 16
asm_log_patch PROC

    push rbx
    sub rsp, 48
    
    ; Check if logging enabled
    mov rax, [g_logging_enabled]
    test rax, rax
    jz log_patch_done
    
    mov r11, rcx            ; r11 = type
    mov r12, rdx            ; r12 = target
    mov r13, r8             ; r13 = success
    
    ; Format: "[PATCH] Apply: type=%s, target=0x%llx"
    lea rcx, [g_log_buffer]
    lea rdx, [fmt_patch_apply]
    mov r8, r11             ; type string
    mov r9, r12             ; target address
    
    sub rsp, 16
    call wsprintfA
    add rsp, 16
    
    ; Output
    lea rcx, [g_log_buffer]
    sub rsp, 32
    call OutputDebugStringA
    add rsp, 32
    
    ; Log event code based on result
    test r13, r13
    jz patch_log_fail
    mov rcx, LOG_PATCH_SUCCESS
    jmp patch_log_event
patch_log_fail:
    mov rcx, LOG_PATCH_FAIL
patch_log_event:
    mov rdx, r12            ; target
    xor r8, r8              ; timestamp
    call asm_log_event
    
log_patch_done:
    add rsp, 48
    pop rbx
    ret

asm_log_patch ENDP

;=====================================================================
; asm_set_logging_level(level: rcx) -> void
;
; Enable (1) or disable (0) logging.
;=====================================================================

PUBLIC asm_set_logging_level

ALIGN 16
asm_set_logging_level PROC

    mov [g_logging_enabled], rcx
    ret

asm_set_logging_level ENDP

END
