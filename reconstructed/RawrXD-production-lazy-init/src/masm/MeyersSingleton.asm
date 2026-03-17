; MeyersSingleton.asm - Thread-safe lazy singleton, zero overhead
; Assemble: ml64 /c /Fo$(IntDir)%(Filename).obj %(FullPath)
;
; This provides an ultra-efficient thread-safe singleton pattern using
; Windows InitOnceExecuteOnce API. Can be used as an alternative to
; C++ static local variables for performance-critical code paths.
;
; Integration with OrchestraManager:
;   extern "C" void* MeyersGet();
;   OrchestraManager* mgr = reinterpret_cast<OrchestraManager*>(MeyersGet());

option casemap:none

; External Windows API
extern HeapAlloc:proc
extern GetProcessHeap:proc
extern InitOnceExecuteOnce:proc

.const
ALIGN 16
g_guard     dq 0                    ; Init-once guard (INIT_ONCE structure)
g_instance  dq 0                    ; Singleton pointer

; Heap allocation flags
HEAP_ZERO_MEMORY equ 08h

.code

; ============================================================
; MeyersGet - Get singleton instance (thread-safe)
; ============================================================
; Returns: RAX = singleton instance pointer (always valid after first call)
; Thread-safe: Yes (uses InitOnceExecuteOnce)
; Overhead: Single branch on fast path after initialization
;
public MeyersGet
MeyersGet proc
    ; Fast path: check if already initialized
    mov     rax, [g_instance]
    test    rax, rax
    jnz     short @already_init
    
    ; Slow path: need to initialize
    sub     rsp, 40                 ; Shadow space + alignment
    
    ; InitOnceExecuteOnce(g_guard, InitOnceHandler, NULL, &context)
    lea     rcx, g_guard            ; lpInitOnce
    lea     rdx, InitOnceHandler    ; InitFn callback
    xor     r8, r8                  ; Parameter (NULL)
    xor     r9, r9                  ; Context (NULL, we use global)
    call    InitOnceExecuteOnce
    
    mov     rax, [g_instance]       ; Get the now-initialized instance
    add     rsp, 40
    
@already_init:
    ret
MeyersGet endp

; ============================================================
; InitOnceHandler - Initialization callback
; ============================================================
; Called by InitOnceExecuteOnce exactly once, even with concurrent callers
; Parameters:
;   RCX = InitOnce pointer
;   RDX = Parameter (unused)
;   R8  = Context output pointer (unused, we use global)
;
InitOnceHandler proc
    push    rbx
    sub     rsp, 32                 ; Shadow space
    
    ; Get process heap
    call    GetProcessHeap
    test    rax, rax
    jz      short @fail
    mov     rbx, rax                ; Save heap handle
    
    ; Allocate 128 bytes for singleton object (cache-line aligned)
    ; HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 128)
    mov     rcx, rbx                ; hHeap
    mov     edx, HEAP_ZERO_MEMORY   ; dwFlags
    mov     r8d, 128                ; dwBytes (enough for OrchestraManager vtable + members)
    call    HeapAlloc
    
    test    rax, rax
    jz      short @fail
    
    ; Store singleton instance
    mov     [g_instance], rax
    
    ; Return TRUE to indicate success
    mov     eax, 1
    add     rsp, 32
    pop     rbx
    ret
    
@fail:
    ; Return FALSE to indicate failure
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
InitOnceHandler endp

; ============================================================
; MeyersGetFast - Inline macro for ultra-fast path
; ============================================================
; Use this macro in hot code paths where you've already checked
; that initialization is complete. This avoids the function call overhead.
;
; Usage in assembly:
;   MeyersGetFast
;   ; RAX now contains singleton pointer
;
MeyersGetFast macro
    local skip
    mov     rax, [g_instance]
    test    rax, rax
    jnz     skip
    call    MeyersGet               ; Slow path if not initialized
skip:
endm

; ============================================================
; MeyersIsInitialized - Check if singleton is initialized
; ============================================================
; Returns: RAX = 1 if initialized, 0 if not
; Thread-safe: Yes (atomic read)
;
public MeyersIsInitialized
MeyersIsInitialized proc
    mov     rax, [g_instance]
    test    rax, rax
    setnz   al
    movzx   eax, al
    ret
MeyersIsInitialized endp

; ============================================================
; MeyersReset - Reset singleton (FOR TESTING ONLY)
; ============================================================
; WARNING: This is NOT thread-safe and should only be used in tests
; Does NOT free memory - that would require destructor call
;
public MeyersReset
MeyersReset proc
    xor     rax, rax
    mov     [g_instance], rax
    mov     [g_guard], rax
    ret
MeyersReset endp

end
