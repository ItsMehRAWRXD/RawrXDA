extern ExitProcess:proc
extern GetStdHandle:proc
extern WriteConsoleA:proc
extern PeWriter_CreateMinimalExe:proc

.data
start_msg db "Starting PE emission...", 10, 0
start_msg_len equ $ - start_msg
succ_msg db "Finished PE emission successfully.", 10, 0
succ_msg_len equ $ - succ_msg

.code
main proc
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
    ; Print start
    mov rcx, -11 ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx, rax
    lea rdx, start_msg
    mov r8, start_msg_len
    lea r9, [rbp-8]
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA

    call PeWriter_CreateMinimalExe
    mov qword ptr [rbp-10h], rax ; Save return code

    ; Print success
    mov rcx, -11
    call GetStdHandle
    mov rcx, rax
    lea rdx, succ_msg
    mov r8, succ_msg_len
    lea r9, [rbp-8]
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA

    mov rcx, qword ptr [rbp-10h]
    call ExitProcess
main endp
end
