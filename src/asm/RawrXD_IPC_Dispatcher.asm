; RawrXD_IPC_Dispatcher.asm - High-speed assembly router
; Part of RawrXD Win32IDE v14.7.3
; Optimized for zero-latency UI-to-Engine communication

.code

; External bridges (implemented in other MASM files or C++ bridges)
extern RawrXD_Disasm_HandleReq : proc
extern RawrXD_Symbol_HandleReq : proc
extern RawrXD_Module_HandleReq : proc

; Message Types from rawrxd_ipc_protocol.h
REQ_DISASM   equ 1001h
REQ_SYMBOL   equ 1002h
REQ_IMPORTS  equ 1004h

RawrXD_DispatchIPC proc frame
    ; rcx = pData (pointer to message buffer)
    ; rdx = size  (size of buffer)
    
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 32
    .allocstack 32
    .endprolog

    ; 1. Validate size
    test rdx, rdx
    jz @exit

    ; 2. Extract MessageType (First 2 bytes of the buffer)
    ; (Simplified for v14.7.3: we expect the raw struct)
    movzx eax, word ptr [rcx]
    
    cmp eax, REQ_DISASM
    je @handle_disasm
    
    cmp eax, REQ_SYMBOL
    je @handle_symbol
    
    cmp eax, REQ_IMPORTS
    je @handle_modules

    jmp @exit

@handle_disasm:
    call RawrXD_Disasm_HandleReq
    jmp @exit

@handle_symbol:
    call RawrXD_Symbol_HandleReq
    jmp @exit

@handle_modules:
    call RawrXD_Module_HandleReq
    jmp @exit

@exit:
    add rsp, 32
    pop rbp
    ret
RawrXD_DispatchIPC endp

end
