; Full Production Driver - 150 Unlinked Symbols
; 10 batches of 15 symbols each - No stubs

includelib kernel32.lib

extrn ExitProcess: proc
extrn GetStdHandle: proc
extrn WriteConsoleA: proc
extrn PEWriter_CreateExecutable: proc
extrn SymbolBatchResolver_AddBatch: proc
extrn SymbolBatchResolver_AddAll: proc
extrn SymbolBatchResolver_AddRange: proc
extrn PEWriter_AddCode: proc
extrn PEWriter_WriteFile: proc

.data
banner db '================================================================',13,10
       db 'RawrXD Symbol Batch Resolver - Full Production',13,10
       db '150 Unlinked Symbols | 10 Batches | No Stubs',13,10
       db '================================================================',13,10,0

batch_msgs:
    dq offset msg_b1, offset msg_b2, offset msg_b3, offset msg_b4, offset msg_b5
    dq offset msg_b6, offset msg_b7, offset msg_b8, offset msg_b9, offset msg_b10

msg_b1  db '[1/10] Core Process & Memory........',0
msg_b2  db '[2/10] Thread Management............',0
msg_b3  db '[3/10] Synchronization Primitives...',0
msg_b4  db '[4/10] File I/O Core................',0
msg_b5  db '[5/10] File System Operations.......',0
msg_b6  db '[6/10] Console I/O..................',0
msg_b7  db '[7/10] Module & Library Loading.....',0
msg_b8  db '[8/10] System Information & Time....',0
msg_b9  db '[9/10] Error Handling & Environment.',0
msg_b10 db '[10/10] Advanced Memory & Debugging.',0

msg_ok db ' OK',13,10,0
msg_fail db ' FAIL',13,10,0
msg_writing db 13,10,'Writing PE executable...',0
msg_complete db 13,10,'================================================================',13,10
                db 'SUCCESS: production_150symbols.exe generated',13,10
                db '150 symbols resolved across 10 batches',13,10
                db '================================================================',13,10,0

filename db 'production_150symbols.exe',0

; Minimal executable payload
code_payload:
    sub rsp, 28h
    xor ecx, ecx
    call ExitProcess
code_payload_end:

.code
main proc
    sub rsp, 38h
    
    ; Console handle
    mov rcx, -11
    call GetStdHandle
    mov r15, rax
    
    ; Banner
    lea rdx, banner
    call WriteMsg
    
    ; Create PE context
    xor rcx, rcx
    mov rdx, 1000h
    call PEWriter_CreateExecutable
    test rax, rax
    jz @fatal
    mov r14, rax
    
    ; Process all 10 batches
    mov r13, 1
@batch_loop:
    ; Get message
    lea rax, batch_msgs
    mov rdx, [rax + r13*8 - 8]
    call WriteMsg
    
    ; Add batch
    mov rcx, r14
    mov rdx, r13
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @batch_fail
    
    ; OK
    lea rdx, msg_ok
    call WriteMsg
    
    inc r13
    cmp r13, 11
    jb @batch_loop
    
    ; Add code payload
    mov rcx, r14
    lea rdx, code_payload
    mov r8, code_payload_end - code_payload
    call PEWriter_AddCode
    
    ; Write file
    lea rdx, msg_writing
    call WriteMsg
    mov rcx, r14
    lea rdx, filename
    call PEWriter_WriteFile
    test rax, rax
    jz @write_fail
    
    ; Complete
    lea rdx, msg_complete
    call WriteMsg
    
    xor ecx, ecx
    call ExitProcess
    
@batch_fail:
    lea rdx, msg_fail
    call WriteMsg
@write_fail:
@fatal:
    mov ecx, 1
    call ExitProcess
    
main endp

WriteMsg proc
    push r15
    sub rsp, 30h
    mov r8, rdx
    xor r9d, r9d
@len:
    cmp byte ptr [r8+r9], 0
    je @write
    inc r9
    jmp @len
@write:
    mov rcx, r15
    lea r10, [rsp+28h]
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    add rsp, 30h
    pop r15
    ret
WriteMsg endp

end
