.code
extern ExitProcess: proc
extern GetStdHandle: proc
extern WriteFile: proc

.data
    sov_msg db "RAWRXD SOVEREIGN MODE: AIR-GAP ACTIVE", 0Dh, 0Ah, 0
    sov_len equ $ - sov_msg
    kill_msg db "NETWORK KILL-SWITCH: ENGAGED (0.0.0.0)", 0Dh, 0Ah, 0
    kill_len equ $ - kill_msg

.code
SovereignModeMain proc
    sub rsp, 48            ; 40 for shadow + 8 for alignment

    ; Get stdout handle
    mov rcx, -11           ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax           ; Save handle

    ; Write Status Message
    mov rcx, rbx           ; hFile
    lea rdx, sov_msg       ; lpBuffer
    mov r8, sov_len        ; nNumberOfBytesToWrite (back to r8 to be safe)
    lea r9, [rsp+32]       ; lpNumberOfBytesWritten
    mov qword ptr [rsp+40], 0 ; lpOverlapped
    call WriteFile

    ; Write Kill-Switch Message
    mov rcx, rbx
    lea rdx, kill_msg
    mov r8, kill_len
    lea r9, [rsp+32]
    mov qword ptr [rsp+40], 0
    call WriteFile

    ; Exit
    xor rcx, rcx
    call ExitProcess
SovereignModeMain endp
end
