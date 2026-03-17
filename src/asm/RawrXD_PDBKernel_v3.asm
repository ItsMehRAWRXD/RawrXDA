; ============================================================================
; RawrXD_PDBKernel_v2.asm - Optimized Export Resolver (Phase 2.3)
; Fast Binary Search across IMAGE_EXPORT_DIRECTORY
; ============================================================================

.code

PUBLIC rawrxd_find_export

; ----------------------------------------------------------------------------
; rawrxd_find_export
; RCX = Base Address of Module
; RDX = Pointer to Null-Terminated Export Name (String)
; Returns: RAX = Absolute Address of Export (0 if not found)
; ----------------------------------------------------------------------------
rawrxd_find_export PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12, rcx                ; r12 = Module Base
    mov r13, rdx                ; r13 = Target Name

    ; 1. Verify DOS Header (MZ)
    cmp word ptr [r12], 5A4Dh
    jne L_error

    ; 2. Find NT Header (PE)
    mov eax, [r12 + 03Ch]       ; e_lfanew
    mov r14, r12
    add r14, rax                ; r14 = NT Header
    cmp dword ptr [r14], 00004550h ; 'PE\0\0'
    jne L_error

    ; 3. Data Directory -> Export Directory (Entry 0)
    ; OptionalHeader starts at +0x18 in NT64, DataDir starts at +0x70 in OptionalHeader
    ; Total offset from NT Header = 0x88
    mov eax, [r14 + 088h]       ; VirtualAddress of Export Dir
    test eax, eax
    jz L_error
    mov r15, r12
    add r15, rax                ; r15 = IMAGE_EXPORT_DIRECTORY

    ; 4. Load Export Directory Metadata
    mov esi, [r15 + 018h]       ; esi = NumberOfNames
    test esi, esi
    jz L_error

    mov edi, [r15 + 020h]       ; Names RVA
    mov rdi, r12
    add rdi, r12                ; Fix: rdi = r12 + AddressOfNames
    ; Cleanup addressing logic for rdi
    mov edi, [r15 + 020h]
    mov rdi, r12
    add rdi, rax                ; wait, let's use a cleaner approach

    ; Resetting rdi properly
    mov eax, [r15 + 020h]
    lea rdi, [r12 + rax]        ; rdi = AddressOfNames (Array of RVAs)

    ; 5. Binary Search Initiation
    xor r8, r8                  ; r8 = Low index
    mov r9, rsi                 ; r9 = High index
    dec r9

L_binary_search_loop:
    cmp r8, r9
    jg L_error                  ; Not found

    ; mid = (low + high) / 2
    mov r10, r8
    add r10, r9
    shr r10, 1                  ; r10 = Mid index

    ; Get mid string
    mov eax, [rdi + r10 * 4]    ; RVA of mid string
    lea rsi, [r12 + rax]        ; rsi = Mid string addr
    mov rdx, r13                ; rdx = Target string
    
    ; strcmp loop
L_strcmp_loop:
    mov al, [rsi]
    mov bl, [rdx]
    
    ; Compare case-sensitive
    cmp al, bl
    jne L_strcmp_diff
    
    test al, al                 ; End of both strings?
    jz L_match_found            ; Exact match found
    
    inc rsi
    inc rdx
    jmp L_strcmp_loop

L_strcmp_diff:
    ; al (mid) vs bl (target)
    jb L_adjust_low             ; mid < target
    ; mid > target
    mov r9, r10
    dec r9
    jmp L_binary_search_loop

L_adjust_low:
    mov r8, r10
    inc r8
    jmp L_binary_search_loop

L_match_found:
    ; Found at mid index (r10)
    ; 6. Get Name Ordinal
    mov eax, [r15 + 024h]       ; AddressOfNameOrdinals RVA
    lea rsi, [r12 + rax]
    movzx eax, word ptr [rsi + r10 * 2] ; eax = Ordinal (Index into AddressOfFunctions)

    ; 7. Get Function RVA
    mov ecx, [r15 + 01Ch]       ; AddressOfFunctions RVA
    lea rsi, [r12 + rcx]
    mov eax, [rsi + rax * 4]    ; eax = Function RVA

    ; Finalization (Note: Forwarding check skipped for maximum performance/zero-bloat)
    add rax, r12                ; Absolute address
    jmp L_exit

L_error:
    xor rax, rax

L_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
rawrxd_find_export ENDP

END
