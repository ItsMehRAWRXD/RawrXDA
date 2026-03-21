; Production Symbol Batch Driver
; Demonstrates unlinked symbol resolution in batches of 15
; No stubs - direct production implementation

includelib kernel32.lib

extrn ExitProcess: proc
extrn GetStdHandle: proc
extrn WriteConsoleA: proc
extrn GetProcessHeap: proc
extrn HeapAlloc: proc
extrn HeapFree: proc
extrn CreateFileA: proc
extrn WriteFile: proc
extrn CloseHandle: proc

extrn PEWriter_CreateExecutable: proc
extrn SymbolBatchResolver_AddBatch: proc
extrn SymbolBatchResolver_AddAll: proc
extrn PEWriter_AddCode: proc
extrn PEWriter_WriteFile: proc

.data
msg_start db 'Symbol Batch Resolver - Production Mode',13,10,0
msg_batch1 db 'Batch 1: Core Kernel32 (15 symbols)...',0
msg_batch2 db 'Batch 2: Extended Kernel32 (15 symbols)...',0
msg_batch3 db 'Batch 3: Memory & Process (15 symbols)...',0
msg_batch4 db 'Batch 4: File System (15 symbols)...',0
msg_batch5 db 'Batch 5: Console & Time (15 symbols)...',0
msg_ok db ' OK',13,10,0
msg_fail db ' FAILED',13,10,0
msg_writing db 'Writing PE with 75 resolved symbols...',0
msg_complete db 'Production executable generated: batch_resolved.exe',13,10,0
filename db 'batch_resolved.exe',0

; Minimal code payload
code_payload:
    mov rcx, 0
    call ExitProcess
code_payload_end:

.code
main proc
    sub rsp, 38h
    
    ; Get console
    mov rcx, -11
    call GetStdHandle
    mov rbx, rax
    
    ; Start message
    lea rdx, msg_start
    call WriteMsg
    
    ; Create PE context
    xor rcx, rcx
    mov rdx, 1000h
    call PEWriter_CreateExecutable
    test rax, rax
    jz @fail
    mov r12, rax
    
    ; Batch 1
    lea rdx, msg_batch1
    call WriteMsg
    mov rcx, r12
    mov rdx, 1
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @batch_fail
    lea rdx, msg_ok
    call WriteMsg
    
    ; Batch 2
    lea rdx, msg_batch2
    call WriteMsg
    mov rcx, r12
    mov rdx, 2
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @batch_fail
    lea rdx, msg_ok
    call WriteMsg
    
    ; Batch 3
    lea rdx, msg_batch3
    call WriteMsg
    mov rcx, r12
    mov rdx, 3
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @batch_fail
    lea rdx, msg_ok
    call WriteMsg
    
    ; Batch 4
    lea rdx, msg_batch4
    call WriteMsg
    mov rcx, r12
    mov rdx, 4
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @batch_fail
    lea rdx, msg_ok
    call WriteMsg
    
    ; Batch 5
    lea rdx, msg_batch5
    call WriteMsg
    mov rcx, r12
    mov rdx, 5
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @batch_fail
    lea rdx, msg_ok
    call WriteMsg
    
    ; Add code
    mov rcx, r12
    lea rdx, code_payload
    mov r8, code_payload_end - code_payload
    call PEWriter_AddCode
    
    ; Write file
    lea rdx, msg_writing
    call WriteMsg
    mov rcx, r12
    lea rdx, filename
    call PEWriter_WriteFile
    test rax, rax
    jz @batch_fail
    lea rdx, msg_ok
    call WriteMsg
    
    ; Complete
    lea rdx, msg_complete
    call WriteMsg
    
    xor ecx, ecx
    call ExitProcess
    
@batch_fail:
    lea rdx, msg_fail
    call WriteMsg
@fail:
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
