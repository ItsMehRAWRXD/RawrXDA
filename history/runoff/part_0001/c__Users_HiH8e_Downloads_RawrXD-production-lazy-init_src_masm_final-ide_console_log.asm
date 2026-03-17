;=====================================================================
; console_log.asm - Simple Console Logging for MASM64
;=====================================================================
option casemap:none
include windows.inc
includelib kernel32.lib
includelib user32.lib

STD_OUTPUT_HANDLE EQU -11
GENERIC_WRITE      EQU 40000000h
FILE_APPEND_DATA   EQU 00000004h
FILE_SHARE_READ    EQU 00000001h
OPEN_ALWAYS        EQU 4
FILE_ATTRIBUTE_NORMAL EQU 80h
FILE_END           EQU 2
INVALID_HANDLE_VALUE EQU -1
MB_OK              EQU 0

EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN lstrlenA:PROC
EXTERN CreateFileA:PROC
EXTERN SetFilePointerEx:PROC
EXTERN GetLastError:PROC
EXTERN MessageBoxA:PROC
EXTERN FlushFileBuffers:PROC

PUBLIC triage_once

.data
    hStdOut         QWORD 0
    hLogFile        QWORD 0
    triage_once     BYTE 0
    write_err_once  BYTE 0
    newline         BYTE 13,10,0
    szLogFileName   BYTE "C:\\Users\\HiH8e\\run.log",0
    szLogInitMsg    BYTE "[log] console_log_init",13,10,0
    szLogFailMsg    BYTE "[log] file open failed, err=",0
    szWriteFailMsg  BYTE "[log] WriteFile failed err=",0
    szLogOkMsg      BYTE "[log] file open ok",13,10,0
    szMsgTitle      BYTE "console_log_init",0

.code

PUBLIC console_log_init
console_log_init PROC
    sub rsp, 40
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    ; Start file logger early and emit marker
    call file_log_init
    ; Inline first write to guarantee we have at least one marker even if helpers fail
    cmp hLogFile, 0
    je cli_skip_inline
    lea rcx, szLogInitMsg
    call lstrlenA
    mov rcx, hLogFile
    lea rdx, szLogInitMsg
    mov r8, rax
    lea r9, [rsp + 16]
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    mov rcx, hLogFile
    call FlushFileBuffers

cli_skip_inline:

    ; One-time MessageBox for triage: show CreateFile status
    cmp BYTE PTR triage_once, 0
    jne cli_after_msg
    mov BYTE PTR triage_once, 1
    cmp hLogFile, 0
    je cli_show_fail
    xor rcx, rcx
    lea rdx, szLogOkMsg
    lea r8, szMsgTitle
    mov r9d, MB_OK
    call MessageBoxA
    jmp cli_after_msg

cli_show_fail:
    ; Build simple failure string: prefix + decimal error
    call GetLastError
    mov ecx, eax                     ; error code in ecx for ntoa
    ; crude itoa to stack buffer
    lea rdx, [rsp]                   ; reuse shadow for temp digits
    call utoa32_inline
    ; log prefix
    lea rcx, szLogFailMsg
    call file_log_append
    ; log error digits
    lea rcx, [rsp]
    call file_log_append
    ; show MessageBox with digits
    xor rcx, rcx
    lea rdx, [rsp]
    lea r8, szMsgTitle
    mov r9d, MB_OK
    call MessageBoxA

cli_after_msg:
    add rsp, 40
    ret
console_log_init ENDP

;---------------------------------------------------------------------
; file_log_init() -> void
; Opens run.log for append and seeks to end. Safe to call multiple times.
;---------------------------------------------------------------------
PUBLIC file_log_init
file_log_init PROC
    sub rsp, 72                            ; align stack for Win64 calls
    lea rcx, szLogFileName                 ; lpFileName
    mov rdx, GENERIC_WRITE                 ; dwDesiredAccess
    mov r8d, FILE_SHARE_READ               ; dwShareMode
    xor r9d, r9d                           ; lpSecurityAttributes
    mov QWORD PTR [rsp + 32], OPEN_ALWAYS  ; 5th: creation disposition
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL ; 6th: flags/attrs
    mov QWORD PTR [rsp + 48], 0            ; 7th: hTemplateFile
    call CreateFileA
    mov hLogFile, rax
    cmp rax, INVALID_HANDLE_VALUE
    jne flog_seek
    mov hLogFile, 0
    jmp flog_done

flog_seek:
    ; Move pointer to end for append semantics
    mov rcx, rax                           ; hFile
    xor rdx, rdx                           ; liDistanceToMove.QuadPart = 0
    xor r8, r8                             ; lpNewFilePointer = NULL
    mov r9d, FILE_END                      ; dwMoveMethod = FILE_END
    call SetFilePointerEx

flog_done:
    add rsp, 72
    ret
file_log_init ENDP

;---------------------------------------------------------------------
; file_log_append(msg: rcx) -> void
; Writes message to run.log if available.
;---------------------------------------------------------------------
PUBLIC file_log_append
file_log_append PROC
    push rbx
    sub rsp, 48

    mov rbx, rcx
    test rbx, rbx
    jz flog_append_done

    ; Ensure log file is open
    cmp hLogFile, 0
    jne flog_have_handle
    call file_log_init

flog_have_handle:
    cmp hLogFile, 0
    je flog_append_done

    ; Get length
    mov rcx, rbx
    call lstrlenA

    ; WriteFile(hLogFile, buf, len, &written, NULL)
    mov rcx, hLogFile
    mov rdx, rbx
    mov r8, rax
    lea r9, [rsp + 40]
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    test eax, eax
    jnz flog_flush

    ; Emit a one-time MessageBox if WriteFile fails so we see the error code
    cmp BYTE PTR write_err_once, 0
    jne flog_append_done
    mov BYTE PTR write_err_once, 1
    call GetLastError
    mov ecx, eax
    lea rdx, [rsp + 16]               ; reuse shadow for decimal buffer
    call utoa32_inline
    xor rcx, rcx
    lea rdx, [rsp + 16]
    lea r8, szMsgTitle
    mov r9d, MB_OK
    call MessageBoxA
    jmp flog_append_done

flog_flush:
    ; Best-effort flush so data hits disk before any crash
    mov rcx, hLogFile
    call FlushFileBuffers

flog_append_done:
    add rsp, 48
    pop rbx
    ret
file_log_append ENDP

;---------------------------------------------------------------------
; utoa32_inline(ecx=val, rdx=buf) -> buf written with ASCII digits
; Minimal helper for error display; writes null-terminated string.
;---------------------------------------------------------------------
utoa32_inline PROC
    push rbx
    sub rsp, 32

    mov ebx, ecx                 ; value
    lea r8, [rsp+16]             ; temp buffer for reverse digits
    xor r9d, r9d                 ; digit count

utoa_div_loop:
    mov eax, ebx
    xor edx, edx
    mov r10d, 10
    div r10d                     ; eax=quot, edx=rem
    add dl, '0'
    mov [r8 + r9], dl            ; store digit
    inc r9d
    mov ebx, eax
    test ebx, ebx
    jnz utoa_div_loop

    ; reverse into output buffer (rdx points to caller buffer)
    xor r11d, r11d               ; output index
utoa_rev_loop:
    dec r9d
    mov al, [r8 + r9]
    mov [rdx + r11], al
    inc r11d
    test r9d, r9d
    jnz utoa_rev_loop
    mov byte ptr [rdx + r11], 0

    add rsp, 32
    pop rbx
    ret
utoa32_inline ENDP

PUBLIC console_log
console_log PROC
    ; rcx = string pointer
    push rbx
    sub rsp, 48
    
    mov rbx, rcx
    test rbx, rbx
    jz log_done
    
    ; Get length
    mov rcx, rbx
    call lstrlenA
    
    ; WriteFile(hStdOut, buf, len, &written, NULL)
    mov rcx, hStdOut
    mov rdx, rbx
    mov r8, rax
    lea r9, [rsp + 40]          ; lpNumberOfBytesWritten (use space above shadow)
    mov QWORD PTR [rsp + 32], 0 ; lpOverlapped (5th param)
    call WriteFile

    ; Also append to run.log for headless runs
    mov rcx, rbx
    call file_log_append
    
log_done:
    add rsp, 48
    pop rbx
    ret
console_log ENDP

END
