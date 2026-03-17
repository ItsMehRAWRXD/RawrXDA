; RawrXD_PE_Importer.asm
; MASM64 kernel to walk the IMAGE_IMPORT_DESCRIPTOR array.
; callback(PSTR dllName, PSTR funcName, PVOID context)

.code

; IMAGE_IMPORT_DESCRIPTOR Structure offsets
; OriginalFirstThunk equ 0   ; RVA to IAT (Import Address Table) - Hints/Names
; TimeDateStamp      equ 4
; ForwarderChain     equ 8
; NameRva            equ 12  ; RVA to DLL Name
; FirstThunk         equ 16  ; RVA to IAT

; RCX = ImageBase
; RDX = ImportDirectoryVA (RVA)
; R8  = Callback Address
; R9  = Context

RawrXD_WalkImports PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 32

    mov r12, rcx        ; r12 = ImageBase
    mov r13, rdx        ; r13 = ImportDescriptor RVA
    mov r14, r8         ; r14 = Callback
    mov r15, r9         ; r15 = Context

    add r13, r12        ; r13 = VA of first Import Descriptor

next_descriptor:
    mov eax, dword ptr [r13 + 12] ; Name RVA
    test eax, eax
    jz done_imports

    lea rbx, [r12 + rax]          ; rbx = VA of DLL Name string

    ; Get Thunk (OriginalFirstThunk if present, else FirstThunk)
    mov eax, dword ptr [r13]      ; OriginalFirstThunk
    test eax, eax
    jnz use_oft
    mov eax, dword ptr [r13 + 16] ; FirstThunk
use_oft:
    lea rsi, [r12 + rax]          ; rsi = VA of Thunk Array (IMAGE_THUNK_DATA64)

next_thunk:
    mov rax, [rsi]                ; Get thunk value
    test rax, rax                 ; End of thunk array?
    jz skip_descriptor

    ; Check if Import by Ordinal (Bit 63 set)
    mov r8, 8000000000000000h
    test rax, r8
    jnz skip_thunk                ; We only want names for this bridge

    ; rax is RVA to IMAGE_IMPORT_BY_NAME
    ; Offset 0: Hint (WORD)
    ; Offset 2: Name (STRING)
    lea rdi, [r12 + rax + 2]      ; rdi = VA of Function Name string

    ; Invoke Callback(dllName, funcName, context)
    ; RCX=rbx, RDX=rdi, R8=r15
    mov rcx, rbx
    mov rdx, rdi
    mov r8, r15
    call r14

    add rsi, 8                    ; Next thunk (64-bit)
    jmp next_thunk

skip_thunk:
    add rsi, 8
    jmp next_thunk

skip_descriptor:
    add r13, 20                   ; sizeof(IMAGE_IMPORT_DESCRIPTOR) = 20
    jmp next_descriptor

done_imports:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_WalkImports ENDP

END
