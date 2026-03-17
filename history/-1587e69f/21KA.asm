; Test 2: Simulate the real _start_entry + MainDispatcher pattern
; Test stack alignment and basic API calls

extern GetStdHandle: proc
extern WriteConsoleA: proc
extern ExitProcess: proc
extern GetCommandLineA: proc
extern CreateFileA: proc
extern CloseHandle: proc

STD_OUTPUT_HANDLE equ -11
GENERIC_WRITE equ 40000000h
FILE_ATTRIBUTE_NORMAL equ 80h

.data
    szMsg1 db "Step 1: MainDispatcher entered", 13, 10, 0
    szMsg2 db "Step 2: InitializeLogging OK", 13, 10, 0
    szMsg3 db "Step 3: GetCommandLineA OK", 13, 10, 0
    szMsg4 db "Step 4: Exiting cleanly", 13, 10, 0
    szLogFile db "test_log.txt", 0
    dwWritten dd 0
    hLogFile dq 0

.code

Print PROC
    ; rcx = string pointer
    push rbx
    push rdi
    sub rsp, 38h       ; 2 pushes(16) + 38h(56) = 72. entry is 8mod16, so 8+72=80=0mod16
    mov rbx, rcx
    
    ; Get length manually
    xor rax, rax
    mov rdi, rbx
count_loop:
    cmp byte ptr [rdi], 0
    je count_done
    inc rax
    inc rdi
    jmp count_loop
count_done:
    mov r8, rax    ; length in r8 (3rd param)
    
    ; Save length
    mov qword ptr [rsp+20h], r8
    
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    
    mov r8, qword ptr [rsp+20h]  ; restore length
    mov rcx, rax           ; handle
    mov rdx, rbx           ; buffer
    lea r9, dwWritten
    mov qword ptr [rsp+20h], 0   ; reserved (5th param)
    call WriteConsoleA
    
    add rsp, 38h
    pop rdi
    pop rbx
    ret
Print ENDP

InitializeLogging PROC
    sub rsp, 48h     ; entry is 8mod16, 8+48h(72) = 80=0mod16 before calls
    lea rcx, szLogFile
    mov rdx, GENERIC_WRITE
    mov r8, 1             ; FILE_SHARE_READ
    xor r9, r9
    mov qword ptr [rsp+20h], 4     ; OPEN_ALWAYS
    mov qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    mov hLogFile, rax
    add rsp, 48h
    ret
InitializeLogging ENDP

MainDispatcher PROC
    push r15
    sub rsp, 40h     ; 1 push(8) + 40h(64) = 72. entry is 8mod16, so 8+72=80=0mod16
    
    lea rcx, szMsg1
    call Print
    
    call InitializeLogging
    
    lea rcx, szMsg2
    call Print
    
    call GetCommandLineA
    mov r15, rax
    
    lea rcx, szMsg3
    call Print
    
    ; Close log file
    mov rcx, hLogFile
    cmp rcx, -1
    je skip_close
    cmp rcx, 0
    je skip_close
    call CloseHandle
skip_close:
    
    lea rcx, szMsg4
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
