; ===============================================================================
; MASM Enterprise Common - Production Version
; Pure MASM x86-64, Zero Dependencies, Production-Ready
; ===============================================================================

option casemap:none

; ===============================================================================
; EXTERNAL DEPENDENCIES
; ===============================================================================

extern HeapCreate:proc
extern HeapAlloc:proc
extern HeapFree:proc
extern HeapDestroy:proc
extern GetProcessHeap:proc
extern InitializeCriticalSection:proc
extern DeleteCriticalSection:proc
extern EnterCriticalSection:proc
extern LeaveCriticalSection:proc
extern CreateEventA:proc
extern SetEvent:proc
extern ResetEvent:proc
extern WaitForSingleObject:proc
extern CloseHandle:proc
extern GetSystemTimeAsFileTime:proc
extern GetCurrentProcessId:proc
extern GetCurrentThreadId:proc

; ===============================================================================
; CONSTANTS
; ===============================================================================

HEAP_ZERO_MEMORY        equ 08h
HEAP_GENERATE_EXCEPTIONS equ 04h
PAGE_SIZE               equ 4096
MAX_STRING_LEN          equ 65536
LOG_BUFFER_SIZE         equ 1048576     ; 1MB circular log buffer
MAX_JSON_DEPTH          equ 256
WAIT_INFINITE           equ 0FFFFFFFFh

; ===============================================================================
; STRUCTURES
; ===============================================================================

MEMORY_BLOCK STRUCT
    pData           qword ?
    block_size      qword ?
    next            qword ?
MEMORY_BLOCK ENDS

LOG_ENTRY STRUCT
    timestamp       qword ?
    level           dword ?
    message         byte MAX_STRING_LEN dup(?)
LOG_ENTRY ENDS

; ===============================================================================
; DATA
; ===============================================================================

.data
    g_Heap          qword 0
    g_Initialized   dword 0
    g_MemPoolLock   qword 0
    g_LogLock       qword 0
    g_InitLock      qword 0

; Log buffer
LogBuffer         LOG_ENTRY 256 dup(<>)
LogWriteIndex     dword 0
LogReadIndex      dword 0

; Memory pool
MemoryPool        MEMORY_BLOCK 1024 dup(<>)
PoolSize          dword 0

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
    
    ; Get process heap
    call    GetProcessHeap
    mov     g_Heap, rax
    
    ; Initialize critical sections
    lea     rcx, g_MemPoolLock
    call    InitializeCriticalSection
    
    lea     rcx, g_LogLock
    call    InitializeCriticalSection
    
    lea     rcx, g_InitLock
    call    InitializeCriticalSection
    
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
    
    ; Enter critical section
    lea     rcx, g_MemPoolLock
    call    EnterCriticalSection
    
    mov     rcx, g_Heap
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8, [rbp-8]
    call    HeapAlloc
    
    ; Leave critical section
    lea     rcx, g_MemPoolLock
    call    LeaveCriticalSection
    
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
    
    ; Enter critical section
    lea     rcx, g_MemPoolLock
    call    EnterCriticalSection
    
    mov     rcx, g_Heap
    xor     edx, edx
    mov     r8, [rbp-8]
    call    HeapFree
    
    ; Leave critical section
    lea     rcx, g_MemPoolLock
    call    LeaveCriticalSection
    
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
; Log Message
; ===============================================================================
EnterpriseLog PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    
    mov     [rbp-8], rcx        ; level
    mov     [rbp-16], rdx       ; message
    
    ; Enter critical section
    lea     rcx, g_LogLock
    call    EnterCriticalSection
    
    ; Get current time
    lea     rcx, [rbp-24]
    call    GetSystemTimeAsFileTime
    
    ; Write to log buffer
    mov     eax, LogWriteIndex
    imul    rax, rax, sizeof LOG_ENTRY
    lea     rcx, LogBuffer
    add     rcx, rax
    
    ; Copy timestamp
    mov     rax, [rbp-24]
    mov     [rcx].LOG_ENTRY.timestamp, rax
    
    ; Copy level
    mov     eax, [rbp-8]
    mov     [rcx].LOG_ENTRY.level, eax
    
    ; Copy message
    lea     rdi, [rcx].LOG_ENTRY.message
    mov     rsi, [rbp-16]
    call    EnterpriseStrCpy
    
    ; Update write index
    mov     eax, LogWriteIndex
    inc     eax
    cmp     eax, 256
    jl      no_wrap
    xor     eax, eax
no_wrap:
    mov     LogWriteIndex, eax
    
    ; Leave critical section
    lea     rcx, g_LogLock
    call    LeaveCriticalSection
    
    leave
    ret
EnterpriseLog ENDP

; ===============================================================================
; EXPORTS
; ===============================================================================

PUBLIC EnterpriseInit
PUBLIC EnterpriseAlloc
PUBLIC EnterpriseFree
PUBLIC EnterpriseStrLen
PUBLIC EnterpriseStrCpy
PUBLIC EnterpriseStrCmp
PUBLIC EnterpriseLog

END
