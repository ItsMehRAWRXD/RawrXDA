; Test: real MainDispatcher code with LogMessage
includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib
includelib shell32.lib

extern GetCommandLineA: proc
extern GetStdHandle: proc
extern WriteConsoleA: proc
extern ExitProcess: proc
extern lstrlenA: proc
extern CreateFileA: proc
extern SetFilePointer: proc
extern WriteFile: proc
extern CloseHandle: proc
extern GetEnvironmentVariableA: proc
extern GetLocalTime: proc
extern wsprintfA: proc
extern GetTickCount64: proc
extern lstrcmpA: proc

STD_OUTPUT_HANDLE equ -11
GENERIC_WRITE equ 40000000h
FILE_ATTRIBUTE_NORMAL equ 80h

.data
    hStdOut dq 0
    hLogFile dq 0
    dwWritten dd 0
    szLogFile db "rawrxd_test.log",0
    szLogLine db 512 dup(0)
    szLogFormat db "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s", 13, 10, 0
    szLevelInfo db "INFO", 0
    szLevelDebug db "DEBUG", 0
    stLocalTime dw 8 dup(0)
    szBanner db "RawrXD Test - v1.0",13,10,0
    szMsgDone db "All logging steps passed!",13,10,0
    InputBuffer db 256 dup(0)

.code

Print PROC
    push rbx
    push r12
    push r14
    sub rsp, 30h
    mov r12, rcx
    call lstrlenA
    mov r14d, eax
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax
    mov rcx, rbx
    mov rdx, r12
    mov r8d, r14d
    lea r9, dwWritten
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    add rsp, 30h
    pop r14
    pop r12
    pop rbx
    ret
Print ENDP

LogMessage PROC
    ; rcx = level string, rdx = message string
    push rbx
    push rsi
    push rdi
    sub rsp, 90h

    mov rbx, rcx
    mov rsi, rdx

    ; 1. Get current time
    lea rcx, stLocalTime
    call GetLocalTime

    ; 2. Format log line
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

    ; 3. Print to console
    lea rcx, szLogLine
    call Print

    ; 4. Write to file if open
    mov rcx, hLogFile
    cmp rcx, -1
    je no_file
    cmp rcx, 0
    je no_file
    
    lea rcx, szLogLine
    call lstrlenA
    mov r8, rax
    
    mov rcx, hLogFile
    lea rdx, szLogLine
    lea r9, InputBuffer
    mov qword ptr [rsp+20h], 0
    call WriteFile

no_file:
    add rsp, 90h
    pop rdi
    pop rsi
    pop rbx
    ret
LogMessage ENDP

LogInfo PROC
    mov rdx, rcx
    lea rcx, szLevelInfo
    jmp LogMessage
LogInfo ENDP

LogDebug PROC
    mov rdx, rcx
    lea rcx, szLevelDebug
    jmp LogMessage
LogDebug ENDP

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

MainDispatcher PROC
    push r15
    sub rsp, 40h

    call InitializeLogging
    
    lea rcx, szBanner
    call LogInfo
    
    call GetCommandLineA
    mov r15, rax
    mov rcx, rax
    call LogDebug

    lea rcx, szMsgDone
    call Print

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
