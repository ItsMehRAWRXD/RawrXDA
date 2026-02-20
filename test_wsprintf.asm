; Test 3: wsprintfA + full LogMessage pattern from real code
; Also tests POLYMACRO and CANARY patterns

extern GetStdHandle: proc
extern WriteConsoleA: proc
extern ExitProcess: proc
extern GetCommandLineA: proc
extern CreateFileA: proc
extern CloseHandle: proc
extern GetLocalTime: proc
extern wsprintfA: proc
extern lstrlenA: proc
extern WriteFile: proc

STD_OUTPUT_HANDLE equ -11
GENERIC_WRITE equ 40000000h
FILE_ATTRIBUTE_NORMAL equ 80h

POLYMACRO MACRO
    nop
    xchg rax, rax
    mov r15, r15
    lea rax, [rax]
ENDM

.data
    szMsg1 db "Step 1: MainDispatcher entered", 13, 10, 0
    szMsg2 db "Step 2: InitializeLogging OK", 13, 10, 0
    szMsg3 db "Step 3: LogMessage OK", 13, 10, 0
    szMsg4 db "Step 4: Exiting cleanly", 13, 10, 0
    szLogFile db "test_log3.txt", 0
    dwWritten dd 0
    hLogFile dq 0
    
    szLogLine db 512 dup(0)
    szTimestamp db 64 dup(0)
    szLogFormat db "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s", 13, 10, 0
    szLevelInfo db "INFO", 0
    szLevelDebug db "DEBUG", 0
    stLocalTime dw 8 dup(0)
    InputBuffer db 256 dup(0)
    szBanner db 13,10,"Test Banner",13,10,0

.code

Print PROC
    push rbx
    sub rsp, 30h
    mov rbx, rcx
    
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    mov rdx, rbx
    call lstrlenA
    ; oops lstrlenA takes one arg and returns length
    ; Let me redo
    add rsp, 30h
    pop rbx
    ret
Print ENDP

; Simpler Print using lstrlenA
SimplePrint PROC
    push rbx
    push rdi
    sub rsp, 38h
    mov rbx, rcx    ; save string
    
    ; Get length
    mov rcx, rbx
    call lstrlenA
    mov rdi, rax     ; save length
    
    ; Get stdout
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    
    ; WriteConsoleA
    mov rcx, rax
    mov rdx, rbx
    mov r8, rdi
    lea r9, dwWritten
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    
    add rsp, 38h
    pop rdi
    pop rbx
    ret
SimplePrint ENDP

InitializeLogging PROC
    sub rsp, 48h
    lea rcx, szLogFile
    mov rdx, GENERIC_WRITE
    mov r8, 1
    xor r9, r9
    mov qword ptr [rsp+20h], 4
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    mov hLogFile, rax
    add rsp, 48h
    ret
InitializeLogging ENDP

LogMessage PROC
    ; rcx = level string, rdx = message string
    push rbx
    push rsi
    push rdi
    sub rsp, 90h

    mov rbx, rcx
    mov rsi, rdx

    lea rcx, stLocalTime
    call GetLocalTime

    lea rcx, szLogLine
    lea rdx, szLogFormat
    movzx r8, word ptr [stLocalTime]
    movzx r9, word ptr [stLocalTime + 2]
    
    movzx rax, word ptr [stLocalTime + 6]
    mov qword ptr [rsp + 20h], rax
    movzx rax, word ptr [stLocalTime + 8]
    mov qword ptr [rsp + 28h], rax
    movzx rax, word ptr [stLocalTime + 10]
    mov qword ptr [rsp + 30h], rax
    movzx rax, word ptr [stLocalTime + 12]
    mov qword ptr [rsp + 38h], rax
    mov qword ptr [rsp + 40h], rbx
    mov qword ptr [rsp + 48h], rsi
    call wsprintfA

    lea rcx, szLogLine
    call SimplePrint

    add rsp, 90h
    pop rdi
    pop rsi
    pop rbx
    ret
LogMessage ENDP

LogInfo PROC
    lea rcx, szLevelInfo
    ; rdx already has old rcx (message) — wait no, rcx = message on entry
    ; Let me fix: LogInfo takes rcx = message
    mov rdx, rcx
    lea rcx, szLevelInfo
    jmp LogMessage
LogInfo ENDP

LogDebug PROC
    mov rdx, rcx
    lea rcx, szLevelDebug
    jmp LogMessage
LogDebug ENDP

MainDispatcher PROC
    push r15
    POLYMACRO
    sub rsp, 40h

    call InitializeLogging
    
    lea rcx, szBanner
    call LogInfo
    
    call GetCommandLineA
    mov r15, rax
    mov rcx, rax
    call LogDebug

    lea rcx, szMsg3
    call SimplePrint

    ; Close log
    mov rcx, hLogFile
    cmp rcx, -1
    je skip_close
    cmp rcx, 0
    je skip_close
    call CloseHandle
skip_close:
    lea rcx, szMsg4
    call SimplePrint
    
    add rsp, 40h
    pop r15
    ret
MainDispatcher ENDP

_start_entry PROC
    sub rsp, 28h
    call MainDispatcher
    xor ecx, ecx
    call ExitProcess
_start_entry ENDP

END
