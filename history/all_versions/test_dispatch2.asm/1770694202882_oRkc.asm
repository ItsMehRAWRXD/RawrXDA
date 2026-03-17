; Test: simulate the real MainDispatcher init calls one by one
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
    szLogFile db "rawrxd.log",0
    szMsg1 db "Step 1: Print works",13,10,0
    szMsg2 db "Step 2: InitializeLogging done",13,10,0
    szMsg3 db "Step 3: GetCommandLineA done",13,10,0
    szMsg4 db "Step 4: ParseCLIArgs done, mode=",0
    szMsgEnd db "All steps passed!",13,10,0
    szCLICompile db "-compile",0
    szCLIEncrypt db "-encrypt",0
    CLI_ArgBuffer db 2048 dup(0)
    szDigit db "0",13,10,0

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

InitializeLogging PROC
    sub rsp, 48h
    lea rcx, szLogFile
    mov rdx, GENERIC_WRITE
    mov r8, 1
    xor r9, r9
    mov qword ptr [rsp+20h], 4 ; OPEN_ALWAYS
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    mov hLogFile, rax
    add rsp, 48h
    ret
InitializeLogging ENDP

CopyString PROC
    push rbx
    push r12
    mov rbx, rcx
    mov r12, rdx
copy_loop:
    movzx rax, byte ptr [rbx]
    mov byte ptr [r12], al
    test al, al
    jz copy_done
    inc rbx
    inc r12
    jmp copy_loop
copy_done:
    pop r12
    pop rbx
    ret
CopyString ENDP

; Simplified ParseCLIArgs - just find -compile or -encrypt
ParseCLIArgs PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 28h

    mov r12, rcx        ; save arg buffer ptr
    
    ; Skip to first space (past exe name)
    xor r15, r15         ; argc = 0
    xor r13, r13         ; mode = 0
    
find_space:
    movzx rax, byte ptr [r12]
    test al, al
    jz cli_no_args
    cmp al, ' '
    je found_space
    inc r12
    jmp find_space
found_space:
    ; Skip spaces
    inc r12
    movzx rax, byte ptr [r12]
    cmp al, ' '
    je found_space
    test al, al
    jz cli_no_args
    
    ; r12 now points to the argument
    mov rcx, r12
    lea rdx, szCLICompile
    call lstrcmpA
    test eax, eax
    jz found_compile
    
    mov rcx, r12
    lea rdx, szCLIEncrypt
    call lstrcmpA
    test eax, eax
    jz found_encrypt
    
    ; Unknown arg
    xor r13, r13
    jmp cli_done

found_compile:
    mov r13, 1
    jmp cli_done
found_encrypt:
    mov r13, 2
    jmp cli_done
cli_no_args:
    xor r13, r13
cli_done:
    mov rax, r13
    add rsp, 28h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
ParseCLIArgs ENDP

MainDispatcher PROC
    push r15
    sub rsp, 40h

    ; Step 1: Print
    lea rcx, szMsg1
    call Print
    
    ; Step 2: InitializeLogging
    call InitializeLogging
    lea rcx, szMsg2
    call Print
    
    ; Step 3: GetCommandLineA
    call GetCommandLineA
    mov r15, rax
    lea rcx, szMsg3
    call Print
    
    ; Step 4: CopyString + ParseCLIArgs
    mov rcx, r15
    lea rdx, CLI_ArgBuffer
    call CopyString
    
    lea rcx, CLI_ArgBuffer
    call ParseCLIArgs
    ; rax = mode
    
    ; Print result
    add al, '0'
    mov byte ptr [szDigit], al
    lea rcx, szMsg4
    call Print
    lea rcx, szDigit
    call Print
    
    lea rcx, szMsgEnd
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
