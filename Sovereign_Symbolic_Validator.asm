OPTION CASEMAP:NONE

; ==============================================================================
; Sovereign Symbolic Validator
; Semantic guardrail for ghost text: validates tokens against live PE exports.
; ==============================================================================

; Exports:
;   VALIDATE_TOKEN_SIMD  RCX=token, RDX=symbol, R8=length(1..32) -> RAX=1/0
;   FNV1A_64            RCX=token, RDX=length(1..32) -> RAX=hash
;   BUILD_SYMBOL_HASH_TABLE RCX=module base -> RAX=inserted symbol count
;   RESOLVE_SYMBOL_HASH RCX=token, RDX=length -> RAX=symbol address/0
;   RESOLVE_SYMBOL_FROM_PE RCX=module base, RDX=token, R8=length -> RAX=addr/0

; PE offsets
DOS_e_lfanew                EQU 3Ch
NT_ExportDirectoryRva       EQU 88h
EXP_NumberOfNames           EQU 18h
EXP_AddressOfFunctions      EQU 1Ch
EXP_AddressOfNames          EQU 20h
EXP_AddressOfNameOrdinals   EQU 24h

; Hash table constants
HASH_TABLE_SIZE             EQU 8192
HASH_TABLE_MASK             EQU 8191
HASH_SLOT_HASH              EQU 0
HASH_SLOT_NAMEPTR           EQU 8
HASH_SLOT_FUNCPTR           EQU 16
HASH_SLOT_SIZE              EQU 24

.DATA
ALIGN 16
g_HashModule        QWORD 0
g_HashCount         QWORD 0
g_HashTable         BYTE HASH_TABLE_SIZE*HASH_SLOT_SIZE DUP(0)

.CODE

; ------------------------------------------------------------------------------
; STRNLEN32
; RCX = pointer to ASCII symbol
; Returns EAX = length capped at 32
; ------------------------------------------------------------------------------
STRNLEN32 PROC FRAME
    .endprolog
    xor eax, eax
len_loop:
    cmp eax, 32
    jae len_done
    cmp BYTE PTR [rcx+rax], 0
    je len_done
    inc eax
    jmp len_loop
len_done:
    ret
STRNLEN32 ENDP

; ------------------------------------------------------------------------------
; BYTE_COMPARE_EXACT
; RCX = token pointer
; RDX = symbol pointer
; R8  = token length (1..32)
; Returns EAX = 1 if first length bytes match and both tails are null, else 0
; ------------------------------------------------------------------------------
BYTE_COMPARE_EXACT PROC FRAME
    .endprolog
    test r8, r8
    jz bcmp_no

    xor r9d, r9d
bcmp_loop:
    cmp r9, r8
    jae bcmp_tail
    mov al, BYTE PTR [rcx+r9]
    mov dl, BYTE PTR [rdx+r9]
    cmp al, dl
    jne bcmp_no
    inc r9
    jmp bcmp_loop

bcmp_tail:
    cmp BYTE PTR [rcx+r8], 0
    jne bcmp_no
    cmp BYTE PTR [rdx+r8], 0
    jne bcmp_no
    mov eax, 1
    ret

bcmp_no:
    xor eax, eax
    ret
BYTE_COMPARE_EXACT ENDP

; ------------------------------------------------------------------------------
; VALIDATE_TOKEN_SIMD
; RCX = token buffer
; RDX = symbol buffer
; R8  = length (1..32)
; Returns RAX = 1 if exact match for first length bytes, else 0
; ------------------------------------------------------------------------------
PUBLIC VALIDATE_TOKEN_SIMD
VALIDATE_TOKEN_SIMD PROC FRAME
    .endprolog

    test r8, r8
    jz simd_no_match
    cmp r8, 32
    ja simd_no_match

    ; For short tokens, use exact byte loop to avoid unsafe 32-byte overread.
    cmp r8, 32
    jne simd_scalar_path

    ; For 32-byte tokens, avoid AVX load when buffer crosses a page boundary.
    mov r9, rcx
    and r9, 0FFFh
    cmp r9, 0FE0h
    ja simd_scalar_path

    mov r9, rdx
    and r9, 0FFFh
    cmp r9, 0FE0h
    ja simd_scalar_path

    ; 32-byte vector compare
    vmovdqu ymm0, YMMWORD PTR [rcx]
    vmovdqu ymm1, YMMWORD PTR [rdx]
    vpcmpeqb ymm2, ymm0, ymm1
    vpmovmskb eax, ymm2
    cmp eax, 0FFFFFFFFh
    jne simd_no_match
    mov eax, 1
    vzeroupper
    ret

simd_scalar_path:
    mov r9, rcx
    mov r10, rdx
    mov rcx, r9
    mov rdx, r10
    call BYTE_COMPARE_EXACT
    ret

simd_no_match:
    xor eax, eax
    vzeroupper
    ret
VALIDATE_TOKEN_SIMD ENDP

; ------------------------------------------------------------------------------
; FNV1A_64
; RCX = string pointer, RDX = length
; Returns RAX = 64-bit FNV-1a hash (0 reserved -> remapped to 1)
; ------------------------------------------------------------------------------
PUBLIC FNV1A_64
FNV1A_64 PROC FRAME
    .endprolog
    mov rax, 14695981039346656037
    mov r8, 1099511628211

fnv_loop:
    test rdx, rdx
    jz fnv_done
    movzx r9, BYTE PTR [rcx]
    xor rax, r9
    imul rax, r8
    inc rcx
    dec rdx
    jmp fnv_loop

fnv_done:
    test rax, rax
    jnz fnv_exit
    mov rax, 1
fnv_exit:
    ret
FNV1A_64 ENDP

; ------------------------------------------------------------------------------
; BUILD_SYMBOL_HASH_TABLE
; RCX = module base
; Returns RAX = inserted symbol count
; ------------------------------------------------------------------------------
PUBLIC BUILD_SYMBOL_HASH_TABLE
BUILD_SYMBOL_HASH_TABLE PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rbx, rcx

    ; Clear table
    lea rdi, g_HashTable
    mov ecx, HASH_TABLE_SIZE*HASH_SLOT_SIZE/8
    xor eax, eax
    rep stosq

    mov QWORD PTR [g_HashModule], rbx
    mov QWORD PTR [g_HashCount], 0

    ; Parse export directory
    mov eax, DWORD PTR [rbx+DOS_e_lfanew]
    add rax, rbx
    mov eax, DWORD PTR [rax+NT_ExportDirectoryRva]
    test eax, eax
    jz build_done

    add rax, rbx
    mov r9, rax

    mov r10d, DWORD PTR [r9+EXP_NumberOfNames]
    test r10d, r10d
    jz build_done

    mov eax, DWORD PTR [r9+EXP_AddressOfNames]
    add rax, rbx
    mov r11, rax

    mov eax, DWORD PTR [r9+EXP_AddressOfNameOrdinals]
    add rax, rbx
    mov r12, rax

    mov eax, DWORD PTR [r9+EXP_AddressOfFunctions]
    add rax, rbx
    mov r13, rax

    xor r14d, r14d
build_loop:
    cmp r14d, r10d
    jae build_done

    ; name_ptr
    mov eax, DWORD PTR [r11+r14*4]
    add rax, rbx
    mov r15, rax

    ; length <= 32 for validator path
    mov rcx, r15
    call STRNLEN32
    test eax, eax
    jz build_next

    ; hash(name)
    mov rcx, r15
    mov edx, eax
    call FNV1A_64
    mov r8, rax

    ; function address
    movzx eax, WORD PTR [r12+r14*2]
    mov eax, DWORD PTR [r13+rax*4]
    add rax, rbx
    mov r9, rax

    ; open-address insert
    mov rdx, r8
    and rdx, HASH_TABLE_MASK
    xor esi, esi

insert_probe:
    cmp esi, HASH_TABLE_SIZE
    jae build_next

    imul rcx, rdx, HASH_SLOT_SIZE
    lea rcx, [g_HashTable+rcx]
    mov rax, QWORD PTR [rcx+HASH_SLOT_HASH]
    test rax, rax
    jz insert_here

    inc rdx
    and rdx, HASH_TABLE_MASK
    inc esi
    jmp insert_probe

insert_here:
    mov QWORD PTR [rcx+HASH_SLOT_HASH], r8
    mov QWORD PTR [rcx+HASH_SLOT_NAMEPTR], r15
    mov QWORD PTR [rcx+HASH_SLOT_FUNCPTR], r9
    inc QWORD PTR [g_HashCount]

build_next:
    inc r14d
    jmp build_loop

build_done:
    mov rax, QWORD PTR [g_HashCount]
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
BUILD_SYMBOL_HASH_TABLE ENDP

; ------------------------------------------------------------------------------
; RESOLVE_SYMBOL_HASH
; RCX = token pointer, RDX = token length
; Returns RAX = function address or 0
; ------------------------------------------------------------------------------
PUBLIC RESOLVE_SYMBOL_HASH
RESOLVE_SYMBOL_HASH PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rsi, rcx
    mov edi, edx

    mov rcx, rsi
    mov edx, edi
    call FNV1A_64
    mov r8, rax

    mov r9, r8
    and r9, HASH_TABLE_MASK
    xor ebx, ebx

lookup_probe:
    cmp ebx, HASH_TABLE_SIZE
    jae lookup_not_found

    imul rcx, r9, HASH_SLOT_SIZE
    lea rcx, [g_HashTable+rcx]

    mov rax, QWORD PTR [rcx+HASH_SLOT_HASH]
    test rax, rax
    jz lookup_not_found
    cmp rax, r8
    jne lookup_next

    ; Hash matched; verify token bytes to reject collisions
    mov r10, QWORD PTR [rcx+HASH_SLOT_NAMEPTR]
    mov r11, QWORD PTR [rcx+HASH_SLOT_FUNCPTR]
    mov rcx, rsi
    mov rdx, r10
    mov r8d, edi
    call VALIDATE_TOKEN_SIMD
    test eax, eax
    jz lookup_next

    mov rax, r11
    jmp lookup_done

lookup_next:
    inc r9
    and r9, HASH_TABLE_MASK
    inc ebx
    jmp lookup_probe

lookup_not_found:
    xor eax, eax

lookup_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
RESOLVE_SYMBOL_HASH ENDP

; ------------------------------------------------------------------------------
; RESOLVE_SYMBOL_FROM_PE
; RCX = module base
; RDX = token pointer (ASCII)
; R8  = token length (1..32)
; Returns RAX = symbol address or 0
; ------------------------------------------------------------------------------
PUBLIC RESOLVE_SYMBOL_FROM_PE
RESOLVE_SYMBOL_FROM_PE PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rbx, rcx                    ; module base
    mov rsi, rdx                    ; token ptr
    mov edi, r8d                    ; token length (dword)

    ; Build table lazily per module, then do hashed lookup.
    cmp QWORD PTR [g_HashModule], rbx
    jne rebuild_table
    cmp QWORD PTR [g_HashCount], 0
    jne lookup_hash

rebuild_table:
    mov rcx, rbx
    call BUILD_SYMBOL_HASH_TABLE

lookup_hash:
    mov rcx, rsi
    mov edx, edi
    call RESOLVE_SYMBOL_HASH
    test rax, rax
    jnz resolve_done

    ; Validate DOS/NT export directory
    mov eax, DWORD PTR [rbx+DOS_e_lfanew]
    add rax, rbx
    mov eax, DWORD PTR [rax+NT_ExportDirectoryRva]
    test eax, eax
    jz resolve_not_found

    add rax, rbx                    ; export directory VA
    mov r9, rax

    mov r10d, DWORD PTR [r9+EXP_NumberOfNames]
    test r10d, r10d
    jz resolve_not_found

    mov eax, DWORD PTR [r9+EXP_AddressOfNames]
    add rax, rbx
    mov r11, rax                    ; names table

    mov eax, DWORD PTR [r9+EXP_AddressOfNameOrdinals]
    add rax, rbx
    mov r12, rax                    ; ordinals table

    mov eax, DWORD PTR [r9+EXP_AddressOfFunctions]
    add rax, rbx
    mov r13, rax                    ; functions table

    xor r14d, r14d
resolve_loop:
    cmp r14d, r10d
    jae resolve_not_found

    ; name_ptr = module + names[i]
    mov eax, DWORD PTR [r11+r14*4]
    add rax, rbx
    mov r15, rax

    ; quick length check to avoid unnecessary SIMD compares
    mov rcx, r15
    call STRNLEN32
    cmp eax, edi
    jne resolve_next

    ; SIMD token compare
    mov rcx, rsi
    mov rdx, r15
    mov r8d, edi
    call VALIDATE_TOKEN_SIMD
    test eax, eax
    jz resolve_next

    ; Resolve function VA by ordinal
    movzx eax, WORD PTR [r12+r14*2]
    mov eax, DWORD PTR [r13+rax*4]
    add rax, rbx
    jmp resolve_done

resolve_next:
    inc r14d
    jmp resolve_loop

resolve_not_found:
    xor eax, eax

resolve_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
RESOLVE_SYMBOL_FROM_PE ENDP

END