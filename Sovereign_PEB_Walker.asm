; ==============================================================================
; Sovereign PEB Walker - Stealth Module Discovery
; ==============================================================================
; Zero-dependency bootstrapping via Process Environment Block (PEB) walking.
; Locates kernel32.dll, ntdll.dll, and any loaded module without API calls.
; Bypasses EDR/AV hooks on GetModuleHandle/GetProcAddress.
;
; Architecture:
;   - PEB traversal via GS:[60h]
;   - InMemoryOrderModuleList linked-list walk
;   - Wide-char string comparison (case-insensitive)
;   - Export Directory Table traversal for function resolution
;
; Exports:
;   GET_MODULE_BASE     - Find module base by name
;   GET_PROC_ADDRESS    - Resolve function by hash or name
;   WALK_MODULE_LIST    - Enumerate all loaded modules
;   HASH_STRING         - ROR13 hash for stealth comparison
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Constants
; ==============================================================================
PEB_OFFSET              equ 60h
PEB_LDR_DATA_OFFSET     equ 18h
INMEMORY_ORDER_LIST     equ 20h
LDR_ENTRY_BASE          equ 20h
LDR_ENTRY_DLL_BASE      equ 30h
LDR_ENTRY_BASE_NAME     equ 58h
LDR_ENTRY_FULL_NAME     equ 48h

EXPORT_DIR_OFFSET       equ 88h
EXPORT_NAMES_RVA        equ 20h
EXPORT_ORDINALS_RVA     equ 24h
EXPORT_FUNCS_RVA        equ 1Ch

; ==============================================================================
; Data Section
; ==============================================================================
.data
align 16
g_Kernel32Base dq 0
g_NtdllBase  dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; HASH_STRING - ROR13 hash for stealth string comparison
; ==============================================================================
; Input:  RCX = Pointer to wide-char string
; Output: RAX = 32-bit hash value
; Clobbers: RAX, RCX, RDX
; ==============================================================================
HASH_STRING proc
    xor eax, eax
    mov edx, 13               ; ROR shift amount
    
hash_loop:
    movzx r8d, word ptr [rcx]
    test r8d, r8d
    jz hash_done
    
    ; ROR13 + add
    ror eax, 13
    add eax, r8d
    
    add rcx, 2                ; Next wide-char
    jmp hash_loop
    
hash_done:
    ret
HASH_STRING endp

; ==============================================================================
; HASH_STRING_A - ANSI version of ROR13 hash
; ==============================================================================
; Input:  RCX = Pointer to ANSI string
; Output: RAX = 32-bit hash value
; ==============================================================================
HASH_STRING_A proc
    xor eax, eax
    
hash_a_loop:
    movzx r8d, byte ptr [rcx]
    test r8d, r8d
    jz hash_a_done
    
    ; Convert to lowercase
    cmp r8d, 'A'
    jb hash_a_skip_lower
    cmp r8d, 'Z'
    ja hash_a_skip_lower
    or r8d, 20h               ; ToLower
    
hash_a_skip_lower:
    ror eax, 13
    add eax, r8d
    inc rcx
    jmp hash_a_loop
    
hash_a_done:
    ret
HASH_STRING_A endp

; ==============================================================================
; GET_MODULE_BASE - Find module base address by wide-char name
; ==============================================================================
; Input:  RCX = Pointer to wide-char module name (e.g., L"kernel32.dll")
; Output: RAX = Module base address or 0 if not found
; Clobbers: RAX, RCX, RDX, R8, R9, R10
; ==============================================================================
GET_MODULE_BASE proc
    push rbx
    push rsi
    push rdi
    
    ; Get PEB
    mov rax, gs:[PEB_OFFSET]  ; TEB->PEB
    mov rax, [rax + PEB_LDR_DATA_OFFSET]  ; PEB->Ldr
    mov rax, [rax + INMEMORY_ORDER_LIST]  ; Ldr->InMemoryOrderModuleList.Flink
    
    ; Save target name
    mov rdi, rcx
    
walk_list:
    ; Get current entry's base name
    mov rsi, [rax + LDR_ENTRY_BASE_NAME]  ; UNICODE_STRING.Buffer
    test rsi, rsi
    jz next_module
    
    ; Compare strings (case-insensitive wide-char)
    mov r8, rdi             ; Target name
    mov r9, rsi             ; Current module name
    
compare_loop:
    movzx r10d, word ptr [r8]
    movzx r11d, word ptr [r9]
    
    ; Convert to lowercase for comparison
    cmp r10d, 'A'
    jb skip_lower1
    cmp r10d, 'Z'
    ja skip_lower1
    or r10d, 20h
    
skip_lower1:
    cmp r11d, 'A'
    jb skip_lower2
    cmp r11d, 'Z'
    ja skip_lower2
    or r11d, 20h
    
skip_lower2:
    cmp r10d, r11d
    jne next_module         ; Mismatch
    
    test r10d, r10d
    jz found_match          ; End of string = match
    
    add r8, 2
    add r9, 2
    jmp compare_loop
    
next_module:
    mov rax, [rax]          ; Flink to next entry
    cmp rax, gs:[PEB_OFFSET]; Back to head?
    jne walk_list
    
    ; Not found
    xor eax, eax
    jmp module_exit
    
found_match:
    ; Return DllBase
    mov rax, [rax + LDR_ENTRY_DLL_BASE]
    
module_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
GET_MODULE_BASE endp

; ==============================================================================
; GET_PROC_ADDRESS - Resolve function by ANSI name hash
; ==============================================================================
; Input:  RCX = Module base address
;         RDX = Function name hash (ROR13)
; Output: RAX = Function address or 0 if not found
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; ==============================================================================
GET_PROC_ADDRESS proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    
    mov r12, rcx            ; Module base
    mov r13, rdx            ; Target hash
    
    ; Get DOS header
    cmp word ptr [r12], 5A4Dh  ; "MZ"
    jne proc_not_found
    
    ; Get NT headers
    mov eax, [r12 + 3Ch]    ; e_lfanew
    add rax, r12            ; NT Header
    
    ; Validate PE signature
    cmp dword ptr [rax], 4550h  ; "PE\0\0"
    jne proc_not_found
    
    ; Get export directory
    mov ebx, [rax + EXPORT_DIR_OFFSET]  ; Export Directory RVA
    test ebx, ebx
    jz proc_not_found
    add rbx, r12            ; Export Directory VA
    
    ; Get export tables
    mov r14d, [rbx + EXPORT_NAMES_RVA]    ; AddressOfNames RVA
    add r14, r12
    mov esi, [rbx + EXPORT_ORDINALS_RVA]  ; AddressOfNameOrdinals RVA
    add rsi, r12
    mov edi, [rbx + EXPORT_FUNCS_RVA]     ; AddressOfFunctions RVA
    add rdi, r12
    
    ; Get number of names
    mov ecx, [rbx + 18h]    ; NumberOfNames
    test ecx, ecx
    jz proc_not_found
    
    xor r8d, r8d            ; Name index
    
search_exports:
    cmp r8d, ecx
    jge proc_not_found
    
    ; Get name RVA
    mov r9d, [r14 + r8 * 4] ; AddressOfNames[index]
    add r9, r12             ; Name string VA
    
    ; Hash the function name
    mov rcx, r9
    call HASH_STRING_A
    
    ; Compare with target hash
    cmp eax, r13d
    je found_proc
    
    inc r8d
    jmp search_exports
    
found_proc:
    ; Get ordinal
    movzx r10d, word ptr [rsi + r8 * 2] ; AddressOfNameOrdinals[index]
    
    ; Get function RVA
    mov r11d, [rdi + r10 * 4]         ; AddressOfFunctions[ordinal]
    add r11, r12                        ; Function VA
    
    mov rax, r11
    jmp proc_exit
    
proc_not_found:
    xor eax, eax
    
proc_exit:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GET_PROC_ADDRESS endp

; ==============================================================================
; WALK_MODULE_LIST - Enumerate all loaded modules
; ==============================================================================
; Input:  RCX = Callback function pointer
;         RDX = User context pointer
; Output: RAX = Number of modules enumerated
; ==============================================================================
WALK_MODULE_LIST proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    xor r13d, r13d          ; Count
    mov r12, rcx            ; Callback
    mov rdi, rdx            ; Context
    
    ; Get PEB
    mov rax, gs:[PEB_OFFSET]
    mov rax, [rax + PEB_LDR_DATA_OFFSET]
    mov rax, [rax + INMEMORY_ORDER_LIST]
    
walk_loop:
    ; Get module base
    mov rbx, [rax + LDR_ENTRY_DLL_BASE]
    test rbx, rbx
    jz walk_next
    
    ; Get module name
    mov rsi, [rax + LDR_ENTRY_BASE_NAME]
    
    ; Call callback with: RCX=base, RDX=name, R8=context
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call r12
    
    inc r13d
    
walk_next:
    mov rax, [rax]          ; Flink
    cmp rax, gs:[PEB_OFFSET]
    jne walk_loop
    
    mov eax, r13d
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
WALK_MODULE_LIST endp

; ==============================================================================
; INIT_STEALTH_BASES - Initialize common module bases
; ==============================================================================
; Input:  None
; Output: RAX = 1 on success
; ==============================================================================
INIT_STEALTH_BASES proc
    push rcx
    push rdx
    
    ; Get kernel32.dll base
    lea rcx, kernel32_name
    call GET_MODULE_BASE
    mov [g_Kernel32Base], rax
    
    ; Get ntdll.dll base
    lea rcx, ntdll_name
    call GET_MODULE_BASE
    mov [g_NtdllBase], rax
    
    pop rdx
    pop rcx
    mov eax, 1
    ret
    
kernel32_name dw 'k','e','r','n','e','l','3','2','.','d','l','l',0
ntdll_name    dw 'n','t','d','l','l','.','d','l','l',0
    
INIT_STEALTH_BASES endp

end