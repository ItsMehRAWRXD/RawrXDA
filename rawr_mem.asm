; =========================
; FILE: rawr_mem.asm  (SYSTEM 1: memory)  -- heap + aligned + arena
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

; Struct offsets
base_offset equ 0
reserve_sz_offset equ 8
commit_sz_offset equ 16
bump_offset_offset equ 24
pagesz_offset equ 32
_pad_offset equ 36

RAWR_SHADOWSPACE        equ 20h

RAWR_PROLOGUE macro locals_bytes:req
    sub     rsp, (RAWR_SHADOWSPACE + locals_bytes + 8)
endm

RAWR_EPILOGUE macro locals_bytes:req
    add     rsp, (RAWR_SHADOWSPACE + locals_bytes + 8)
    ret
endm

RAWR_SAVE_NONVOL macro
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
endm

RAWR_RESTORE_NONVOL macro
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
endm

RAWR_ALIGN_UP macro regX:req, regA:req, tmp:req
    ; tmp = a-1
    mov     tmp, regA
    dec     tmp
    add     regX, tmp
    not     tmp
    and     regX, tmp
endm

.data
public g_rawr_heap
g_rawr_heap dq 0

.code

public rawr_heap_init
rawr_heap_init proc
    RAWR_PROLOGUE 0
    mov     rax, g_rawr_heap
    test    rax, rax
    jne     _ok
    mov     r8, rdx
    mov     rdx, rcx
    xor     ecx, ecx
    call    HeapCreate
    test    rax, rax
    je      _fail
    mov     g_rawr_heap, rax
_ok:mov eax,1
    RAWR_EPILOGUE 0
_fail:
    xor eax,eax
    RAWR_EPILOGUE 0
rawr_heap_init endp

public rawr_heap_destroy
rawr_heap_destroy proc
    RAWR_PROLOGUE 0
    mov     rax, g_rawr_heap
    test    rax, rax
    je      _done
    mov     rcx, rax
    call    HeapDestroy
    mov     g_rawr_heap, 0
_done:
    RAWR_EPILOGUE 0
rawr_heap_destroy endp

public rawr_heap_alloc
rawr_heap_alloc proc
    RAWR_PROLOGUE 0
    mov     r9d, edx
    mov     r8, rcx
    mov     rax, g_rawr_heap
    test    rax, rax
    jne     _have
    call    GetProcessHeap
    mov     g_rawr_heap, rax
_have:
    mov     rcx, rax
    xor     edx, edx
    test    r9d, r9d
    jz      _call
    or      edx, HEAP_ZERO_MEMORY
_call:
    call    HeapAlloc
    RAWR_EPILOGUE 0
rawr_heap_alloc endp

public rawr_heap_free
rawr_heap_free proc
    RAWR_PROLOGUE 0
    test    rcx, rcx
    jz      _ok
    mov     r8, rcx
    mov     rax, g_rawr_heap
    test    rax, rax
    jne     _have
    call    GetProcessHeap
    mov     g_rawr_heap, rax
_have:
    mov     rcx, rax
    xor     edx, edx
    call    HeapFree
    test    eax, eax
    jz      _fail
_ok:mov eax,1
    RAWR_EPILOGUE 0
_fail:
    xor eax,eax
    RAWR_EPILOGUE 0
rawr_heap_free endp

public rawr_aligned_alloc
rawr_aligned_alloc proc
    RAWR_PROLOGUE 0
    mov     r9, rdx
    cmp     r9, 8
    jb      _bad
    mov     r10, r9
    dec     r10
    test    r9, r10
    jne     _bad
    mov     r11, rcx
    add     r11, r9
    add     r11, 8
    mov     rcx, r11
    mov     edx, r8d
    call    rawr_heap_alloc
    test    rax, rax
    jz      _bad
    mov     r12, rax
    lea     r13, [r12 + 8]
    mov     r14, r9
    dec     r14
    add     r13, r14
    not     r14
    and     r13, r14
    mov     qword ptr [r13 - 8], r12
    mov     rax, r13
    RAWR_EPILOGUE 0
_bad:
    xor     rax, rax
    RAWR_EPILOGUE 0
rawr_aligned_alloc endp

public rawr_aligned_free
rawr_aligned_free proc
    RAWR_PROLOGUE 0
    test    rcx, rcx
    jz      _done
    mov     rax, qword ptr [rcx - 8]
    mov     rcx, rax
    call    rawr_heap_free
_done:
    RAWR_EPILOGUE 0
rawr_aligned_free endp

public rawr_arena_init
rawr_arena_init proc
    RAWR_PROLOGUE 50h
    RAWR_SAVE_NONVOL

    mov     r12, rcx
    mov     r13, rdx
    test    r12, r12
    jz      _fail

    lea     rcx, [rsp+20h]
    call    GetSystemInfo

    mov     eax, dword ptr [rsp+24h]
    mov     dword ptr [r12 + 32], eax
    mov     r14, rax
    test    r14, r14
    jnz     _min_ok
    mov     r14, 1000h
_min_ok:
    cmp     r13, r14
    jae     _reserve_ok
    mov     r13, r14
_reserve_ok:
    xor     rcx, rcx
    mov     rdx, r13
    mov     r8d, MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      _fail

    mov     [r12 + 0], rax
    mov     [r12 + 8], r13
    mov     qword ptr [r12 + 16], 0
    mov     qword ptr [r12 + 24], 0

    mov     eax, 1
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 50h

_fail:
    xor     eax, eax
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 50h
rawr_arena_init endp

public rawr_arena_alloc
rawr_arena_alloc proc
    RAWR_PROLOGUE 0
    RAWR_SAVE_NONVOL

    mov     r12, rcx      ; a*
    mov     r13, rdx      ; sz
    mov     r14, r8       ; align

    test    r12, r12
    jz      _oom

    mov     r15, [r12 + 0]
    test    r15, r15
    jz      _oom

    test    r14, r14
    jnz     _a_chk
    mov     r14, 1
_a_chk:
    mov     rax, r14
    dec     rax
    test    r14, rax
    jne     _oom

    mov     rbx, [r12 + 24]
    mov     rsi, rbx
    RAWR_ALIGN_UP rsi, r14, rax    ; aligned_off in RSI

    mov     rdi, rsi
    add     rdi, r13
    jc      _oom

    mov     rdx, [r12 + 8]
    cmp     rdi, rdx
    ja      _oom

    mov     r8, [r12 + 16]
    cmp     r8, rdi
    jae     _commit_ok

    mov     eax, dword ptr [r12 + 32]
    mov     r9, rax
    test    r9, r9
    jnz     _ps_ok
    mov     r9, 1000h
_ps_ok:
    mov     rbp, rdi
    RAWR_ALIGN_UP rbp, r9, rax     ; commit_needed in RBP

    mov     r10, rbp
    sub     r10, r8
    jz      _commit_ok

    mov     rcx, r15
    add     rcx, r8
    mov     rdx, r10
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      _oom

    mov     [r12 + 16], rbp

_commit_ok:
    mov     rax, r15
    add     rax, rsi
    mov     [r12 + 24], rdi

    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 0

_oom:
    xor     rax, rax
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 0
rawr_arena_alloc endp

public rawr_arena_reset
rawr_arena_reset proc
    RAWR_PROLOGUE 0
    test    rcx, rcx
    jz      _done
    mov     qword ptr [rcx + bump_offset_offset], 0
_done:
    RAWR_EPILOGUE 0
rawr_arena_reset endp

public rawr_arena_destroy
rawr_arena_destroy proc
    RAWR_PROLOGUE 0
    RAWR_SAVE_NONVOL
    mov     r12, rcx
    test    r12, r12
    jz      _done
    mov     rax, [r12 + 0]
    test    rax, rax
    jz      _zero
    mov     rcx, rax
    xor     rdx, rdx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
_zero:
    mov     qword ptr [r12 + 0], 0
    mov     qword ptr [r12 + 8], 0
    mov     qword ptr [r12 + 16], 0
    mov     qword ptr [r12 + 24], 0
    mov     dword ptr [r12 + 32], 0
_done:
    RAWR_RESTORE_NONVOL
    RAWR_EPILOGUE 0
rawr_arena_destroy endp

end
