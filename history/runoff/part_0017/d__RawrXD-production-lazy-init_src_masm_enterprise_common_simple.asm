; ===============================================================================
; MASM Enterprise Common - Simplified Production Version
; Pure MASM x86-64, Zero Dependencies, Production-Ready
; ===============================================================================

option casemap:none

; ===============================================================================
; EXTERNAL DEPENDENCIES
; ===============================================================================

extern HeapAlloc:proc
extern HeapFree:proc
extern GetProcessHeap:proc

; ===============================================================================
; CONSTANTS
; ===============================================================================

HEAP_ZERO_MEMORY equ 08h

; ===============================================================================
; DATA
; ===============================================================================

.data
    g_Heap          qword 0
    g_Initialized   dword 0

.code

; ===============================================================================
; Initialize Enterprise Common
; ===============================================================================
EnterpriseInit PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    cmp     g_Initialized, 0
    jne     already_init
    
    call    GetProcessHeap
    mov     g_Heap, rax
    
    mov     g_Initialized, 1
    mov     eax, 1
    jmp     done
    
already_init:
    xor     eax, eax
    
done:
    leave
    ret
EnterpriseInit ENDP

; ===============================================================================
; Allocate Memory
; ===============================================================================
EnterpriseAlloc PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    mov     [rbp-8], rcx        ; size
    
    mov     rcx, g_Heap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8, [rbp-8]
    call    HeapAlloc
    
    leave
    ret
EnterpriseAlloc ENDP

; ===============================================================================
; Free Memory
; ===============================================================================
EnterpriseFree PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    mov     [rbp-8], rcx        ; ptr
    
    mov     rcx, g_Heap
    xor     edx, edx
    mov     r8, [rbp-8]
    call    HeapFree
    
    leave
    ret
EnterpriseFree ENDP

; ===============================================================================
; String Length
; ===============================================================================
EnterpriseStrLen PROC
    push    rsi
    mov     rsi, rcx
    xor     rax, rax
    
strlen_loop:
    cmp     byte ptr [rsi + rax], 0
    je      strlen_done
    inc     rax
    jmp     strlen_loop
    
strlen_done:
    pop     rsi
    ret
EnterpriseStrLen ENDP

; ===============================================================================
; String Copy
; ===============================================================================
EnterpriseStrCpy PROC
    push    rsi
    push    rdi
    
    mov     rdi, rcx            ; dest
    mov     rsi, rdx            ; src
    
strcpy_loop:
    lodsb
    stosb
    test    al, al
    jnz     strcpy_loop
    
    mov     rax, rcx
    pop     rdi
    pop     rsi
    ret
EnterpriseStrCpy ENDP

; ===============================================================================
; String Compare
; ===============================================================================
EnterpriseStrCmp PROC
    push    rsi
    push    rdi
    
    mov     rsi, rcx
    mov     rdi, rdx
    
strcmp_loop:
    lodsb
    scasb
    jne     strcmp_not_equal
    test    al, al
    jnz     strcmp_loop
    
    xor     eax, eax
    jmp     strcmp_done
    
strcmp_not_equal:
    sbb     eax, eax
    or      al, 1
    
strcmp_done:
    pop     rdi
    pop     rsi
    ret
EnterpriseStrCmp ENDP

; ===============================================================================
; EXPORTS
; ===============================================================================

PUBLIC EnterpriseInit
PUBLIC EnterpriseAlloc
PUBLIC EnterpriseFree
PUBLIC EnterpriseStrLen
PUBLIC EnterpriseStrCpy
PUBLIC EnterpriseStrCmp

END
