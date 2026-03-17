; ============================================================================
; RawrXD_PE_Exporter.asm - Export Table Walker (Phase 2.3)
; ============================================================================
.code

; ----------------------------------------------------------------------------
; RawrXD_WalkExports
; RCX = Module Base Address (u64)
; RDX = Callback function: void callback(u64 rva, const char* name, void* context)
; R8  = Context
; ----------------------------------------------------------------------------
public RawrXD_WalkExports
RawrXD_WalkExports PROC
    jmp rawrxd_walk_export_table
RawrXD_WalkExports ENDP

; ----------------------------------------------------------------------------
; rawrxd_walk_export_table
; RCX = Module Base Address (u64)
; RDX = Callback function: void callback(u64 rva, const char* name, void* context)
; R8  = Context
; ----------------------------------------------------------------------------
rawrxd_walk_export_table PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                ; r12 = Module Base
    mov r13, rdx                ; r13 = Callback
    mov r14, r8                 ; r14 = Context (saved in R14)
    
    ; 1. Verify DOS Header
    cmp word ptr [r12], 05A4Dh  ; 'MZ'
    jne L_exit
    
    ; 2. Find NT Headers
    mov eax, dword ptr [r12 + 3Ch] ; NT Header RVA
    lea rbx, [r12 + rax]        ; rbx = NT Headers
    cmp dword ptr [rbx], 00004550h ; 'PE\0\0'
    jne L_exit
    
    ; 3. Data Directory Index for EXPORT (Index 0)
    ; Offset 0x88 from NT Header (x64 Image Optional Header)
    mov eax, [rbx + 088h]        ; eax = Export Directory RVA
    test eax, eax
    jz L_exit
    
    lea r9, [r12 + rax]         ; r9 = IMAGE_EXPORT_DIRECTORY
    
    ; 4. Export Table Parsing
    ; Offsets from IMAGE_EXPORT_DIRECTORY:
    ; 0x18: NumberOfNames
    ; 0x1C: AddressOfFunctions RVA
    ; 0x20: AddressOfNames RVA
    ; 0x24: AddressOfNameOrdinals RVA
    
    mov r15d, [r9 + 018h]       ; r15d = Number of named exports
    mov eax, [r9 + 020h]        ; eax = AddressOfNames RVA
    lea rbx, [r12 + rax]        ; rbx = Pointer to Names array
    
    xor rsi, rsi                ; rsi = Name Index
    
L_export_loop:
    cmp esi, r15d
    jge L_done
    
    ; Get Name RVA from Names Array
    mov edi, [rbx + rsi * 4]    ; edi = Name RVA
    lea rdx, [r12 + rdi]        ; rdx = Pointer to Name (UTF-8/ASCII)
    
    ; Get Ordinal to find function RVA
    mov eax, [r9 + 024h]        ; eax = AddressOfNameOrdinals RVA
    lea r11, [r12 + rax]        ; r11 = Ordinal array
    movzx eax, word ptr [r11 + rsi * 2] ; eax = Ordinal
    
    mov r11d, [r9 + 01Ch]       ; r11d = AddressOfFunctions RVA
    lea r11, [r12 + r11]        ; r11 = Function array
    mov ecx, dword ptr [r11 + rax * 4] ; ecx = Function RVA!
    
    ; Prepare callback parameters: 
    ; RCX = RVA, RDX = name_ptr, R8 = context
    ; (RCX is already ECX via mov ecx, ...)
    mov r8, r14                 ; r8 = context
    
    sub rsp, 32
    call r13                    ; callback(rva, name_ptr, context)
    add rsp, 32
    
    inc esi
    jmp L_export_loop

L_done:
L_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
rawrxd_walk_export_table ENDP

END
