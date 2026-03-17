; ============================================================================
; RawrXD_PDBKernel.asm - Native Symbol & PE Engine (Phase 2.1)
; ============================================================================
.code

; EnumerateModules Callback: (BaseAddr, Size, PathPtr)
; RCX = CallbackProc

EXTERN GetModuleHandleA : PROC
EXTERN GetProcAddress : PROC

; ----------------------------------------------------------------------------
; rawrxd_find_export
; RCX = ModuleBase
; RDX = ExportName (String Ptr)
; Returns: Address of export or NULL
; ----------------------------------------------------------------------------
rawrxd_find_export PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; rsi = ModuleBase
    
    ; Parse DOS Header
    cmp word ptr [rsi], 5A4Dh ; 'MZ'
    jne L_fail
    
    mov eax, [rsi+3Ch]  ; efa_lfanew
    add rax, rsi        ; rax = NT Header
    mov rbx, rax
    
    cmp dword ptr [rbx], 00004550h ; 'PE\0\0'
    jne L_fail
    
    ; Optional Header -> DataDirectory[0] (Export)
    ; IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory[0] is at offset 136 (0x88)
    mov eax, [rbx+88h]  ; Export Directory RVA
    test eax, eax
    jz L_fail
    
    add rax, rsi        ; rax = IMAGE_EXPORT_DIRECTORY
    mov rdi, rax
    
    ; IMAGE_EXPORT_DIRECTORY layout:
    ; +24: NumberOfNames
    ; +28: AddressOfFunctions (RVA)
    ; +32: AddressOfNames (RVA)
    ; +36: AddressOfNameOrdinals (RVA)
    
    mov ecx, [rdi+18h]  ; NumberOfNames
    test ecx, ecx
    jz L_fail
    
    mov eax, [rdi+20h]  ; AddressOfNames RVA
    add rax, rsi        ; rax = Name Pointer Table
    
    ; [Binary search or linear scan fallback for spine]
    
L_fail:
    pop rdi
    pop rsi
    pop rbx
    xor rax, rax
    ret
rawrxd_find_export ENDP

END
