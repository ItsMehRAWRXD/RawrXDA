; Simple test for batch resolver
includelib kernel32.lib

extrn ExitProcess: proc
extrn GetStdHandle: proc
extrn WriteConsoleA: proc
extrn PEWriter_CreateExecutable: proc
extrn PEWriter_AddImport: proc
extrn PEWriter_AddCode: proc
extrn PEWriter_WriteFile: proc

.data
msg_test db 'Testing PE Writer...',13,10,0
msg_ok db 'OK',13,10,0
msg_fail db 'FAIL',13,10,0
dll_name db 'kernel32.dll',0
func_name db 'ExitProcess',0
filename db 'test_simple.exe',0

code_buf:
    sub rsp, 28h
    xor ecx, ecx
    call ExitProcess
code_buf_end:

.code
main proc
    sub rsp, 38h
    
    mov rcx, -11
    call GetStdHandle
    mov rbx, rax
    
    ; Test message
    lea rdx, msg_test
    call WriteMsg
    
    ; Create PE
    xor rcx, rcx
    mov rdx, 1000h
    call PEWriter_CreateExecutable
    test rax, rax
    jz @fail
    mov r14, rax
    
    ; Add import
    mov rcx, r14
    lea rdx, dll_name
    lea r8, func_name
    call PEWriter_AddImport
    test rax, rax
    jz @fail
    
    ; Add code
    mov rcx, r14
    lea rdx, code_buf
    mov r8, code_buf_end - code_buf
    call PEWriter_AddCode
    test rax, rax
    jz @fail
    
    ; Write file
    mov rcx, r14
    lea rdx, filename
    call PEWriter_WriteFile
    test rax, rax
    jz @fail
    
    lea rdx, msg_ok
    call WriteMsg
    xor ecx, ecx
    call ExitProcess
    
@fail:
    lea rdx, msg_fail
    call WriteMsg
    mov ecx, 1
    call ExitProcess
    
main endp

WriteMsg proc
    push rbx
    sub rsp, 30h
    mov r8, rdx
    xor r9d, r9d
@len:
    cmp byte ptr [r8+r9], 0
    je @write
    inc r9
    jmp @len
@write:
    mov rcx, rbx
    lea r10, [rsp+28h]
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    add rsp, 30h
    pop rbx
    ret
WriteMsg endp

end
