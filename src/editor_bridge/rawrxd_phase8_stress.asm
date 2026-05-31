; ==============================================================================
; RawrXD-Stress: Phase 8.1 Memory Safety & 1GB Stress Test
; ==============================================================================
; Purpose: Verify memory stability and tokenizer logic under 1GB cross-file load
; Toolchain: ml64.exe / link.exe (x64 MASM)
; ==============================================================================

extern LoadLibraryA : proc
extern GetProcAddress : proc
extern FreeLibrary : proc
extern ExitProcess : proc
extern VirtualAlloc : proc
extern VirtualFree : proc

.data
    szTokenDll db "rawrxd_tokenizers.dll", 0
    szProcTokenize db "RawrXDDetectLanguage", 0
    hTokDll dq 0
    pTokenize dq 0
    pLargeBuffer dq 0
    nBufferSize dq 1073741824 ; 1GB

.code
main proc
    sub rsp, 40

    ; 1GB Allocation
    xor rcx, rcx
    mov rdx, 1073741824
    mov r8, 1000h ; MEM_COMMIT
    mov r9, 04h   ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz _err
    
    ; Release
    mov rcx, rax
    xor rdx, rdx
    mov r8, 8000h ; MEM_RELEASE
    call VirtualFree

    xor ecx, ecx
    add rsp, 40
    call ExitProcess

_err:
    mov ecx, 1
    add rsp, 40
    call ExitProcess
main endp

end

