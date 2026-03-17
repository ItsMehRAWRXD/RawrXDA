; Minimal GGUF header reader test
option casemap:none

EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN ExitProcess:PROC

.const
GGUF_MAGIC32         EQU 46554747h
STD_OUTPUT_HANDLE    EQU 0FFFFFFFFFFFFFFF5h
GENERIC_READ         EQU 80000000h
FILE_SHARE_READ      EQU 1
OPEN_EXISTING        EQU 3
INVALID_HANDLE_VALUE EQU 0FFFFFFFFFFFFFFFFh

.data
default_input        DB "D:\RawrXD-ExecAI\test_small.gguf", 0
input_path           DB 260 DUP(0)
file_handle          DQ 0
stdout_handle        DQ 0
header_buffer        DB 24 DUP(0)
read_temp            DQ 0

msg_header           DB "=== Minimal GGUF Test ===", 13, 10, 0
msg_success          DB "SUCCESS: Header read", 13, 10, 0
msg_error_file       DB "ERROR: Cannot open file", 13, 10, 0
msg_error_read       DB "ERROR: Read failed", 13, 10, 0
msg_error_magic      DB "ERROR: Invalid GGUF magic", 13, 10, 0

.code

PrintString PROC
    sub rsp, 28h
    mov r8, rcx
    xor r9, r9
PS_L:
    cmp byte ptr [r8+r9], 0
    je PS_W
    inc r9
    cmp r9, 4096
    jb PS_L
PS_W:
    mov rcx, [stdout_handle]
    mov rdx, r8
    mov r8d, r9d
    lea r9, [rsp+30h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    add rsp, 28h
    ret
PrintString ENDP

main PROC
    sub rsp, 58h
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [stdout_handle], rax

    ; Print Banner
    lea rcx, msg_header
    call PrintString

    ; Open file
    lea rcx, default_input
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je E_F
    mov [file_handle], rax

    ; Read header
    mov rcx, [file_handle]
    lea rdx, header_buffer
    mov r8d, 24
    lea r9, read_temp
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz E_R
    
    ; Check magic
    mov eax, dword ptr [header_buffer]
    cmp eax, GGUF_MAGIC32
    jne E_M

    ; Success
    lea rcx, msg_success
    call PrintString
    mov rcx, [file_handle]
    call CloseHandle
    xor ecx, ecx
    call ExitProcess

E_F:
    lea rcx, msg_error_file
    call PrintString
    mov ecx, 1
    call ExitProcess
E_R:
    lea rcx, msg_error_read
    call PrintString
    mov ecx, 2
    call ExitProcess
E_M:
    lea rcx, msg_error_magic
    call PrintString
    mov ecx, 3
    call ExitProcess
main ENDP

END