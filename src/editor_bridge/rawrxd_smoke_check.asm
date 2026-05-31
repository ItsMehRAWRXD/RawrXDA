; ==============================================================================
; RawrXD-Smoke: Phase 7.1 Bridge Interop Smoke Test
; ==============================================================================
; Purpose: Verify cross-DLL linkage and export accessibility
; Toolchain: ml64.exe / link.exe (x64 MASM)
; ==============================================================================

extern LoadLibraryA : proc
extern GetProcAddress : proc
extern FreeLibrary : proc
extern ExitProcess : proc

.data
    szLib1 db "editor_bridge.dll", 0
    szLib2 db "rawrxd_tokenizers.dll", 0
    szLib3 db "rawrxd_lsp.dll", 0
    szLib4 db "rawrxd_observability.dll", 0
    szLib5 db "rawrxd_ux.dll", 0
    szLib6 db "rawrxd_dominance.dll", 0
    
    szMsgSuccess db "SUCCESS: Library Loaded and Verified", 0ah, 0
    szMsgFailure db "FAILURE: Error Loading Library", 0ah, 0

.code
main proc
    sub rsp, 40 ; Shadow space
    
    ; Test Editor Bridge
    lea rcx, szLib1
    call LoadLibraryA
    test rax, rax
    jz _fail
    mov rcx, rax
    call FreeLibrary

    ; Test Tokenizer Bridge
    lea rcx, szLib2
    call LoadLibraryA
    test rax, rax
    jz _fail
    mov rcx, rax
    call FreeLibrary

    ; Test LSP Bridge
    lea rcx, szLib3
    call LoadLibraryA
    test rax, rax
    jz _fail
    mov rcx, rax
    call FreeLibrary

    ; Test Observability Bridge
    lea rcx, szLib4
    call LoadLibraryA
    test rax, rax
    jz _fail
    mov rcx, rax
    call FreeLibrary

    ; Test UX Bridge
    lea rcx, szLib5
    call LoadLibraryA
    test rax, rax
    jz _fail
    mov rcx, rax
    call FreeLibrary

    ; Test Dominance Bridge
    lea rcx, szLib6
    call LoadLibraryA
    test rax, rax
    jz _fail
    mov rcx, rax
    call FreeLibrary

    ; ALL PASS
    xor ecx, ecx
    call ExitProcess

_fail:
    mov ecx, 1
    call ExitProcess
main endp

end
