;=====================================================================
; asm_log.asm - Lightweight Structured Logger (Pure MASM x64)
;=====================================================================
; Provides timestamped debug logging using OutputDebugStringA.
; - asm_log_init(): initializes frequency cache (optional)
; - asm_log(msg):   emits "[tick] msg" with QueryPerformanceCounter ticks
;=====================================================================

EXTERN OutputDebugStringA:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN wsprintfA:PROC

.data
LOG_ENABLED      EQU 1

g_qpf           dq 0
fmt_log         db "[%llu] %s",13,10,0
; Shared buffer (single-threaded assumption for fast path)
g_logbuf        db 512 dup(0)

.code

; Export logging functions
PUBLIC asm_log_init, asm_log

;---------------------------------------------------------------------
; asm_log_init() -> void
; Caches QueryPerformanceFrequency for potential duration math.
;---------------------------------------------------------------------
ALIGN 16
asm_log_init PROC
    sub rsp, 40
    lea rcx, g_qpf
    call QueryPerformanceFrequency
    add rsp, 40
    ret
asm_log_init ENDP

;---------------------------------------------------------------------
; asm_log(msg: rcx) -> void
; Formats "[ticks] msg" and emits via OutputDebugStringA.
; Clobbers volatile registers only.
;---------------------------------------------------------------------
ALIGN 16
asm_log PROC
IF LOG_ENABLED
    ; Save message pointer
    mov r11, rcx

    ; Shadow + local storage for QPC value
    sub rsp, 56                ; 32 shadow + 24 local (align stack)
    lea rcx, [rsp + 32]        ; rcx = &local_qword
    call QueryPerformanceCounter
    mov rax, [rsp + 32]        ; rax = ticks

    ; wsprintfA(buf, fmt, ticks, msg)
    lea rcx, g_logbuf          ; rcx = buffer
    lea rdx, fmt_log           ; rdx = fmt
    mov r8,  rax               ; r8  = ticks
    mov r9,  r11               ; r9  = msg
    call wsprintfA

    ; OutputDebugStringA(buffer)
    lea rcx, g_logbuf
    call OutputDebugStringA

    add rsp, 56
ENDIF
    ret
asm_log ENDP

END
