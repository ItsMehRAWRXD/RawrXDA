; ==============================================================================
; Sovereign Core Engine - Self-Hosted Position-Independent Instrumentation
; ==============================================================================
; The "Golden Ratio" of assembly: Zero imports, zero dependencies.
; Uses GS register for PEB access, walks export tables by hash,
; installs hardware breakpoints via direct DR access.
;
; Architecture:
;   - Position-Independent Code (PIC)
;   - PEB-based module resolution
;   - ROR13 hash-based API lookup
;   - Direct debug register manipulation
;   - No IAT, no imports, no placeholders
;
; Exports:
;   ENGINE_ENTRY        - Unified initialization
;   RESOLVE_API         - Hash-based API resolution
;   ENABLE_HARDWARE_BP  - Direct DR0/DR7 manipulation
;   PEB_WALK            - Module enumeration
;   HASH_STRING         - ROR13 hash algorithm
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Constants
; ==============================================================================
PEB_LDR_OFFSET          equ 18h
PEB_LIST_OFFSET         equ 20h
LDR_BASE_OFFSET         equ 30h
LDR_NAME_OFFSET         equ 58h

DOS_LFANEW              equ 3Ch
NT_EXPORT_RVA           equ 88h

EXPORT_NAMES_RVA        equ 20h
EXPORT_ORDINALS_RVA     equ 24h
EXPORT_FUNCS_RVA        equ 1Ch

; ==============================================================================
; Data Section (Minimal - only hash table)
; ==============================================================================
.data
align 16
; Pre-computed ROR13 hashes for common APIs
HASH_GetProcAddress     dd 07C0DFA8Ah
HASH_LoadLibraryA       dd 08A8B4036h
HASH_VirtualProtect     dd 079370C3Ah
HASH_ExitProcess        dd 0273C9658h

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; HASH_STRING - ROR13 hash algorithm
; ==============================================================================
; Input:  RCX = Pointer to null-terminated ASCII string
; Output: RAX = ROR13 hash value
; Clobbers: RAX, RCX, RDX
; ==============================================================================
HASH_STRING proc
    xor eax, eax            ; Initialize hash = 0
    mov rdx, 0Dh            ; ROR13 constant
    
hash_loop:
    movzx r8d, byte ptr [rcx]  ; Load character
    test r8d, r8d
    jz hash_done            ; Null terminator
    
    ; ROR eax, 13
    ror eax, 13
    add eax, r8d            ; Add character to hash
    
    inc rcx
    jmp hash_loop
    
hash_done:
    ret
HASH_STRING endp

; ==============================================================================
; PEB_WALK - Enumerate loaded modules via PEB
; ==============================================================================
; Input:  RCX = Pointer to wide-char module name (or 0 for first module)
; Output: RAX = Module base address, or 0 if not found
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; ==============================================================================
PEB_WALK proc
    push rbx
    push rsi
    push rdi
    
    ; Get PEB from GS segment (x64)
    mov rax, gs:[60h]
    mov rax, [rax + PEB_LDR_OFFSET]   ; PEB->Ldr
    mov rax, [rax + PEB_LIST_OFFSET]    ; Ldr->InMemoryOrderModuleList
    mov rbx, rax                        ; Save head for loop detection
    
    ; If RCX is 0, return first module (the EXE itself)
    test rcx, rcx
    jz return_first_module
    
    ; Walk the list
walk_loop:
    ; Get module name
    mov rdx, [rax + LDR_NAME_OFFSET]    ; UNICODE_STRING.Buffer
    test rdx, rdx
    jz next_module
    
    ; Compare wide strings (case-insensitive)
    mov rsi, rcx                        ; Target name
    mov rdi, rdx                        ; Current module name
    
compare_chars:
    movzx r8d, word ptr [rsi]
    movzx r9d, word ptr [rdi]
    
    ; Case-insensitive comparison
    or r8d, 20h
    or r9d, 20h
    
    cmp r8d, r9d
    jne next_module
    
    ; Check for end of string
    test r8d, r8d
    jz found_module
    
    add rsi, 2
    add rdi, 2
    jmp compare_chars
    
next_module:
    mov rax, [rax]                      ; Flink to next entry
    cmp rax, rbx                        ; Back to head?
    je module_not_found
    jmp walk_loop
    
return_first_module:
    mov rax, [rax + LDR_BASE_OFFSET]    ; DllBase
    jmp peb_walk_exit
    
found_module:
    mov rax, [rax + LDR_BASE_OFFSET]    ; DllBase
    jmp peb_walk_exit
    
module_not_found:
    xor eax, eax
    
peb_walk_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
PEB_WALK endp

; ==============================================================================
; RESOLVE_API - Find function address by ROR13 hash
; ==============================================================================
; Input:  RCX = Module base address
;         RDX = ROR13 hash of function name
; Output: RAX = Function address, or 0 if not found
; Clobbers: RAX, RCX, RDX, R8, R9, R10, R11
; ==============================================================================
RESOLVE_API proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov r12, rcx            ; Module base
    mov r13, rdx            ; Target hash
    
    ; Get NT headers
    mov eax, [r12 + DOS_LFANEW]
    add rax, r12
    
    ; Get export directory
    mov eax, [rax + NT_EXPORT_RVA]
    test eax, eax
    jz api_not_found
    
    add rax, r12            ; Export directory VA
    mov rbx, rax
    
    ; Get export table pointers
    mov r8d, [rbx + EXPORT_NAMES_RVA]
    add r8, r12             ; Names table
    
    mov r9d, [rbx + EXPORT_ORDINALS_RVA]
    add r9, r12             ; Ordinals table
    
    mov r10d, [rbx + EXPORT_FUNCS_RVA]
    add r10, r12            ; Functions table
    
    ; Get number of names
    mov ecx, [rbx + 18h]    ; NumberOfNames
    test ecx, ecx
    jz api_not_found
    
    xor r11d, r11d          ; Index counter
    
search_loop:
    cmp r11d, ecx
    jae api_not_found
    
    ; Get name RVA
    mov eax, [r8 + r11*4]
    add rax, r12            ; Name VA
    
    ; Calculate hash
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    
    mov rcx, rax
    call HASH_STRING
    
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    
    ; Compare with target hash
    cmp eax, r13d
    je found_api
    
    inc r11d
    jmp search_loop
    
found_api:
    ; Get ordinal
    movzx eax, word ptr [r9 + r11*2]
    
    ; Get function RVA
    mov eax, [r10 + rax*4]
    add rax, r12            ; Function VA
    jmp api_exit
    
api_not_found:
    xor eax, eax
    
api_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RESOLVE_API endp

; ==============================================================================
; ENABLE_HARDWARE_BP - Direct debug register manipulation
; ==============================================================================
; Input:  RCX = Target address for breakpoint
;         RDX = Breakpoint index (0-3)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
ENABLE_HARDWARE_BP proc
    push rbx
    
    ; Validate index
    cmp rdx, 3
    ja bp_fail
    
    ; Set DR0-DR3 based on index
    cmp rdx, 0
    je set_dr0
    cmp rdx, 1
    je set_dr1
    cmp rdx, 2
    je set_dr2
    cmp rdx, 3
    je set_dr3
    jmp bp_fail
    
set_dr0:
    mov dr0, rcx
    jmp enable_bp
    
set_dr1:
    mov dr1, rcx
    jmp enable_bp
    
set_dr2:
    mov dr2, rcx
    jmp enable_bp
    
set_dr3:
    mov dr3, rcx
    jmp enable_bp
    
enable_bp:
    ; Read current DR7
    mov rax, dr7
    
    ; Enable local breakpoint based on index
    cmp rdx, 0
    je enable_l0
    cmp rdx, 1
    je enable_l1
    cmp rdx, 2
    je enable_l2
    cmp rdx, 3
    je enable_l3
    
enable_l0:
    or rax, 00000001h       ; L0 = 1
    jmp write_dr7
    
enable_l1:
    or rax, 00000004h       ; L1 = 1
    jmp write_dr7
    
enable_l2:
    or rax, 00000010h       ; L2 = 1
    jmp write_dr7
    
enable_l3:
    or rax, 00000040h       ; L3 = 1
    jmp write_dr7
    
write_dr7:
    mov dr7, rax
    
    mov eax, 1
    jmp bp_exit
    
bp_fail:
    xor eax, eax
    
bp_exit:
    pop rbx
    ret
ENABLE_HARDWARE_BP endp

; ==============================================================================
; DISABLE_HARDWARE_BP - Remove hardware breakpoint
; ==============================================================================
; Input:  RCX = Breakpoint index (0-3)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
DISABLE_HARDWARE_BP proc
    ; Validate index
    cmp rcx, 3
    ja disable_fail
    
    ; Read current DR7
    mov rax, dr7
    
    ; Disable local breakpoint based on index
    cmp rcx, 0
    je disable_l0
    cmp rcx, 1
    je disable_l1
    cmp rcx, 2
    je disable_l2
    cmp rcx, 3
    je disable_l3
    
disable_l0:
    and rax, NOT 00000001h
    xor rdx, rdx
    mov dr0, rdx
    jmp write_disable
    
disable_l1:
    and rax, NOT 00000004h
    xor rdx, rdx
    mov dr1, rdx
    jmp write_disable
    
disable_l2:
    and rax, NOT 00000010h
    xor rdx, rdx
    mov dr2, rdx
    jmp write_disable
    
disable_l3:
    and rax, NOT 00000040h
    xor rdx, rdx
    mov dr3, rdx
    jmp write_disable
    
write_disable:
    mov dr7, rax
    
    mov eax, 1
    ret
    
disable_fail:
    xor eax, eax
    ret
DISABLE_HARDWARE_BP endp

; ==============================================================================
; ENGINE_ENTRY - Unified initialization
; ==============================================================================
; Input:  None
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
ENGINE_ENTRY proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; 1. Locate kernel32.dll via PEB walk
    lea rcx, kernel32_name
    call PEB_WALK
    test rax, rax
    jz engine_fail
    mov r12, rax            ; kernel32 base
    
    ; 2. Resolve GetProcAddress by hash
    mov rcx, r12
    mov edx, HASH_GetProcAddress
    call RESOLVE_API
    test rax, rax
    jz engine_fail
    mov r13, rax            ; GetProcAddress
    
    ; 3. Resolve LoadLibraryA by hash
    mov rcx, r12
    mov edx, HASH_LoadLibraryA
    call RESOLVE_API
    test rax, rax
    jz engine_fail
    mov r14, rax            ; LoadLibraryA
    
    ; 4. Resolve VirtualProtect by hash
    mov rcx, r12
    mov edx, HASH_VirtualProtect
    call RESOLVE_API
    test rax, rax
    jz engine_fail
    mov r15, rax            ; VirtualProtect
    
    ; 5. Resolve ExitProcess by hash
    mov rcx, r12
    mov edx, HASH_ExitProcess
    call RESOLVE_API
    test rax, rax
    jz engine_fail
    
    ; Engine initialized successfully
    mov eax, 1
    jmp engine_exit
    
engine_fail:
    xor eax, eax
    
engine_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
    
    ; Wide-char string for "kernel32.dll"
kernel32_name:
    dw 'k','e','r','n','e','l','3','2','.','d','l','l',0
    
ENGINE_ENTRY endp

; ==============================================================================
; GET_PEB_BASE - Get PEB base address
; ==============================================================================
; Input:  None
; Output: RAX = PEB base address
; ==============================================================================
GET_PEB_BASE proc
    mov rax, gs:[60h]
    ret
GET_PEB_BASE endp

; ==============================================================================
; GET_MODULE_HASH - Get module base by name hash
; ==============================================================================
; Input:  RCX = ROR13 hash of module name
; Output: RAX = Module base, or 0 if not found
; ==============================================================================
GET_MODULE_HASH proc
    push rbx
    push rsi
    push rdi
    push r12
    
    mov r12, rcx            ; Target hash
    
    ; Get PEB
    call GET_PEB_BASE
    mov rax, [rax + PEB_LDR_OFFSET]
    mov rax, [rax + PEB_LIST_OFFSET]
    mov rbx, rax            ; Save head
    
module_hash_loop:
    ; Get module name
    mov rdx, [rax + LDR_NAME_OFFSET]
    test rdx, rdx
    jz next_hash_module
    
    ; Calculate hash of module name
    push rax
    push rbx
    
    mov rcx, rdx
    call HASH_STRING
    
    pop rbx
    pop rax
    
    ; Compare hashes
    cmp eax, r12d
    je found_hash_module
    
next_hash_module:
    mov rax, [rax]
    cmp rax, rbx
    jne module_hash_loop
    
    xor eax, eax
    jmp get_module_exit
    
found_hash_module:
    mov rax, [rax + LDR_BASE_OFFSET]
    
get_module_exit:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GET_MODULE_HASH endp

end