; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm  ─  Atomic Model State Management
; Assemble: ml64.exe /c /FoRawrXD_Model_StateMachine.obj RawrXD_Model_StateMachine.asm
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

EXTERNDEF RawrXD_StrCmp:PROC


; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
MODEL_STATE_UNLOADED    EQU 0
MODEL_STATE_LOADING     EQU 1
MODEL_STATE_READY       EQU 2
MODEL_STATE_STREAMING   EQU 3
MODEL_STATE_UNLOADING   EQU 4
MODEL_STATE_ERROR       EQU 5

MODEL_MAX_INSTANCES     EQU 40      ; Support 40 concurrent models

; Transition validation matrix
; Row = current state, Col = target state, 1 = valid
TRANSITION_MATRIX       EQU 1 SHL (MODEL_STATE_UNLOADED * 8 + MODEL_STATE_LOADING) OR \
                            1 SHL (MODEL_STATE_LOADING * 8 + MODEL_STATE_READY) OR \
                            1 SHL (MODEL_STATE_LOADING * 8 + MODEL_STATE_ERROR) OR \
                            1 SHL (MODEL_STATE_READY * 8 + MODEL_STATE_STREAMING) OR \
                            1 SHL (MODEL_STATE_READY * 8 + MODEL_STATE_UNLOADING) OR \
                            1 SHL (MODEL_STATE_STREAMING * 8 + MODEL_STATE_READY) OR \
                            1 SHL (MODEL_STATE_STREAMING * 8 + MODEL_STATE_UNLOADING) OR \
                            1 SHL (MODEL_STATE_UNLOADING * 8 + MODEL_STATE_UNLOADED) OR \
                            1 SHL (MODEL_STATE_ERROR * 8 + MODEL_STATE_UNLOADING)

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
ModelInstance STRUCT
    Magic               DWORD       ?       ; 0x44415752 'RAWD'
    State               DWORD       ?       ; Current state
    RefCount            DWORD       ?       ; Reference count
    Lock                DWORD       ?       ; Spinlock
    hModel              QWORD       ?       ; Handle to loaded model
    ModelPath           QWORD       ?       ; Pointer to path string
    ModelSize           QWORD       ?       ; Bytes in VRAM
    LastUsedTick        QWORD       ?       ; For LRU
    LoadTick            QWORD       ?       ; When loaded
    StreamCount         DWORD       ?       ; Active streams
    ErrorCode           DWORD       ?       ; Last error
    Reserved            DWORD 4 DUP (?)     ; Padding
ModelInstance ENDS

ModelRegistry STRUCT
    Instances           ModelInstance MODEL_MAX_INSTANCES DUP (<>)
    GlobalLock          DWORD       ?
    ActiveCount         DWORD       ?
    TotalLoadedBytes    QWORD       ?
    StateChangeCallbacks QWORD 6 DUP (?)    ; Per-state callbacks
ModelRegistry ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_ModelRegistry         ModelRegistry <>

; State name strings (for logging)
szStateNames            QWORD OFFSET szStateUnloaded, OFFSET szStateLoading
                        QWORD OFFSET szStateReady, OFFSET szStateStreaming
                        QWORD OFFSET szStateUnloading, OFFSET szStateError

szStateUnloaded         BYTE "UNLOADED", 0
szStateLoading          BYTE "LOADING", 0
szStateReady            BYTE "READY", 0
szStateStreaming        BYTE "STREAMING", 0
szStateUnloading        BYTE "UNLOADING", 0
szStateError            BYTE "ERROR", 0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Initialize
; Initializes model registry
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Initialize PROC FRAME
    push rdi
    
    ; Zero registry
    lea rdi, g_ModelRegistry
    mov ecx, (SIZEOF ModelRegistry) / 8
    xor eax, eax
    rep stosq
    
    ; Initialize spinlocks
    mov ecx, MODEL_MAX_INSTANCES
    lea rdx, g_ModelRegistry.Instances
    
@init_loop:
    mov [rdx].ModelInstance.Magic, 052444157h  ; 'RAWD'
    mov [rdx].ModelInstance.State, MODEL_STATE_UNLOADED
    mov [rdx].ModelInstance.RefCount, 0
    mov [rdx].ModelInstance.Lock, 0
    add rdx, SIZEOF ModelInstance
    dec ecx
    jnz @init_loop
    
    pop rdi
    ret
ModelState_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_AcquireInstance
; Finds or creates model instance, increments ref count
;
; Parameters:
;   RCX = Model path string
;
; Returns:
;   RAX = Pointer to ModelInstance, NULL if full
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_AcquireInstance PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    
    mov r12, rcx                    ; Save path
    
    ; Try to find existing
    call FindModelByPath
    test rax, rax
    jnz @found_existing
    
    ; Allocate new slot
    call FindFreeSlot
    test rax, rax
    jz @acquire_fail
    
    mov rbx, rax                    ; RBX = new instance
    
    ; Initialize
    mov [rbx].ModelInstance.ModelPath, r12
    
    ; Attempt atomic ref count increment from 0 to 1
@try_acquire:
    mov eax, [rbx].ModelInstance.RefCount
    test eax, eax
    jnz @slot_race                  ; Someone else got it
    
    mov edx, 1
    lock cmpxchg [rbx].ModelInstance.RefCount, edx
    jne @try_acquire
    
    mov rax, rbx
    jmp @acquire_done
    
@found_existing:
    mov rbx, rax
    lock inc [rbx].ModelInstance.RefCount
    
@slot_race:
    mov rax, rbx
    jmp @acquire_done
    
@acquire_fail:
    xor eax, eax
    
@acquire_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ModelState_AcquireInstance ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Transition
; Atomically transitions model state with validation
;
; Parameters:
;   RCX = ModelInstance*
;   RDX = Target state
;
; Returns:
;   RAX = TRUE if transition succeeded, FALSE if invalid
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Transition PROC FRAME
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; RBX = instance
    mov esi, edx                    ; ESI = target state
    
    ; Validate target state
    cmp esi, MODEL_STATE_ERROR
    ja @transition_invalid
    
    ; Acquire instance lock
    lea rcx, [rbx].ModelInstance.Lock
    call AcquireSpinlock
    
    ; Get current state
    mov edi, [rbx].ModelInstance.State
    
    ; Check transition validity
    mov eax, edi
    shl eax, 8
    or eax, esi                     ; EAX = (current << 8) | target
    
    mov ecx, eax
    mov eax, TRANSITION_MATRIX
    shr eax, cl
    and eax, 1
    jz @transition_invalid_locked
    
    ; Perform transition
    mov [rbx].ModelInstance.State, esi
    
    ; Update timestamps
    cmp esi, MODEL_STATE_READY
    jne @check_unload
    
    call GetTickCount64
    mov [rbx].ModelInstance.LoadTick, rax
    jmp @transition_ok
    
@check_unload:
    cmp esi, MODEL_STATE_UNLOADED
    jne @transition_ok
    
    ; Clear on unload
    mov [rbx].ModelInstance.ModelPath, 0
    mov [rbx].ModelInstance.hModel, 0
    
@transition_ok:
    ; Release lock
    mov dword ptr [rbx].ModelInstance.Lock, 0
    
    ; Notify callbacks
    mov ecx, edi                    ; Old state
    mov edx, esi                    ; New state
    mov r8, rbx
    call NotifyStateChange
    
    mov rax, TRUE
    jmp @transition_done
    
@transition_invalid_locked:
    mov dword ptr [rbx].ModelInstance.Lock, 0
    
@transition_invalid:
    xor eax, eax
    
@transition_done:
    pop rdi
    pop rsi
    pop rbx
    ret
ModelState_Transition ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_BeginStream
; Attempts to start streaming from READY state
;
; Parameters:
;   RCX = ModelInstance*
;
; Returns:
;   RAX = TRUE if stream started
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_BeginStream PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Must be in READY state
    cmp [rbx].ModelInstance.State, MODEL_STATE_READY
    jne @stream_fail
    
    ; Attempt transition to STREAMING
    mov rcx, rbx
    mov edx, MODEL_STATE_STREAMING
    call ModelState_Transition
    test al, al
    jz @stream_fail
    
    ; Increment active stream count
    lock inc [rbx].ModelInstance.StreamCount
    
    mov rax, TRUE
    jmp @stream_done
    
@stream_fail:
    xor eax, eax
    
@stream_done:
    pop rbx
    ret
ModelState_BeginStream ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_EndStream
; Decrements stream count, returns to READY if last stream
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_EndStream PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    lock dec [rbx].ModelInstance.StreamCount
    
    ; If no more streams, return to READY
    cmp [rbx].ModelInstance.StreamCount, 0
    jne @end_done
    
    mov rcx, rbx
    mov edx, MODEL_STATE_READY
    call ModelState_Transition
    
@end_done:
    pop rbx
    ret
ModelState_EndStream ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Release
; Decrements reference count, triggers unload if zero
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Release PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    lock dec [rbx].ModelInstance.RefCount
    jnz @release_done
    
    ; Last reference - initiate unload
    mov rcx, rbx
    mov edx, MODEL_STATE_UNLOADING
    call ModelState_Transition
    
    ; Schedule async unload
    call ScheduleModelUnload
    
@release_done:
    pop rbx
    ret
ModelState_Release ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper stubs
; ═══════════════════════════════════════════════════════════════════════════════
FindModelByPath PROC
    xor eax, eax
    ret
FindModelByPath ENDP

FindFreeSlot PROC
    xor eax, eax
    ret
FindFreeSlot ENDP

AcquireSpinlock PROC
    ret
AcquireSpinlock ENDP

NotifyStateChange PROC
    ret
NotifyStateChange ENDP

ScheduleModelUnload PROC
    ret
ScheduleModelUnload ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC ModelState_Initialize
PUBLIC ModelState_AcquireInstance
PUBLIC ModelState_Transition
PUBLIC ModelState_BeginStream
PUBLIC ModelState_EndStream
PUBLIC ModelState_Release

END
