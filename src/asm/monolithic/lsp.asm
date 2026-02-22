; RawrXD LSP Bridge — JSON-RPC over stdin/stdout of LSP subprocess

EXTERN CreatePipe:PROC
EXTERN CreateProcessW:PROC
EXTERN CloseHandle:PROC

PUBLIC LSPBridgeInit
PUBLIC LSPSendRequest

.data?
hLSPInRead    dq ?
hLSPInWrite   dq ?
hLSPOutRead   dq ?
hLSPOutWrite  dq ?

.const
STARTF_USESTDHANDLES equ 100h
CREATE_NO_WINDOW     equ 8000000h

.code
LSPBridgeInit PROC
    ; RCX = lspPath (optional; can be 0 for stub)
    test    rcx, rcx
    jz      @stub_ok
    ; TODO: CreatePipe, CreateProcessW, handle inheritance
@stub_ok:
    xor     eax, eax
    ret
LSPBridgeInit ENDP

LSPSendRequest PROC
    ; RCX=method, RDX=params
    xor     eax, eax
    ret
LSPSendRequest ENDP

END
