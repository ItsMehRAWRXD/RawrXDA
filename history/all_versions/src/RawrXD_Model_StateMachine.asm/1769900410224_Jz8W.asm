; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Model_StateMachine.asm
; Model lifecycle management: Loading, Unloading, State Transitions
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
STATE_UNLOADED          EQU 0
STATE_LOADING           EQU 1
STATE_READY             EQU 2
STATE_UNLOADING         EQU 3

MAX_MODELS              EQU 16

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════
ModelState STRUCT
    State               DWORD       ?
    RefCount            DWORD       ?
    ModelPath           QWORD       ?
    Handle              QWORD       ?
    LoadTime            QWORD       ?
ModelState ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
g_ModelStates           ModelState MAX_MODELS DUP (<>)
g_StateLock             DWORD       0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Initialize PROC FRAME
    push rbx
    
    ; Zero out table
    lea rcx, g_ModelStates
    mov rdx, SIZEOF ModelState * MAX_MODELS
    call RtlZeroMemory
    
    pop rbx
    ret
ModelState_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_Transition
; RCX = Model Index, RDX = New State
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_Transition PROC FRAME
    push rbx
    
    ; Bounds check
    cmp rcx, MAX_MODELS
    jge @invalid_index
    
    imul rcx, SIZEOF ModelState
    lea rbx, g_ModelStates[rcx]
    
    ; Set state atomically (simplified)
    mov [rbx].ModelState.State, edx
    
    mov eax, 1
    jmp @done
    
@invalid_index:
    xor eax, eax
    
@done:
    pop rbx
    ret
ModelState_Transition ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ModelState_AcquireInstance
; RCX = Model ID (Hash/Index)
; Returns RAX = Instance Handle or 0
; ═══════════════════════════════════════════════════════════════════════════════
ModelState_AcquireInstance PROC FRAME
    push rbx
    
    ; Mock lookup - assume ID is index
    cmp rcx, MAX_MODELS
    jge @fail_acquire
    
    imul rcx, SIZEOF ModelState
    lea rbx, g_ModelStates[rcx]
    
    cmp [rbx].ModelState.State, STATE_READY
    jne @fail_acquire
    
    lock inc dword ptr [rbx].ModelState.RefCount
    mov rax, [rbx].ModelState.Handle
    
    pop rbx
    ret
    
@fail_acquire:
    xor eax, eax
    pop rbx
    ret
ModelState_AcquireInstance ENDP

PUBLIC ModelState_Initialize
PUBLIC ModelState_Transition
PUBLIC ModelState_AcquireInstance

END
