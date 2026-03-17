;=====================================================================
; asm_sync.inc - x64 MASM Thread Synchronization Prototypes
;=====================================================================

; Mutexes
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC
EXTERN asm_mutex_destroy:PROC

; Events
EXTERN asm_event_create:PROC
EXTERN asm_event_set:PROC
EXTERN asm_event_reset:PROC
EXTERN asm_event_wait:PROC
EXTERN asm_event_destroy:PROC

; Semaphores
EXTERN asm_semaphore_create:PROC
EXTERN asm_semaphore_wait:PROC
EXTERN asm_semaphore_release:PROC
EXTERN asm_semaphore_destroy:PROC

; Atomic Operations
EXTERN asm_atomic_increment:PROC
EXTERN asm_atomic_decrement:PROC
EXTERN asm_atomic_add:PROC
EXTERN asm_atomic_cmpxchg:PROC
EXTERN asm_atomic_xchg:PROC
