; ============================================================================
; gui_factory_integration.asm - Minimal queue management for GUI actions
; Provides Queue_AddTask / Queue_GetItem / Queue_GetCount used by UI
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
EXTERN lstrcpyA:PROC

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC Queue_AddTask
PUBLIC Queue_GetItem
PUBLIC Queue_GetCount

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
MAX_QUEUE_TASKS     equ 128
TASK_TEXT_BYTES     equ 256
NULL                equ 0

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------
.data
queueWriteIndex     dd 0
queueCount          dd 0
queueStorage        db MAX_QUEUE_TASKS * TASK_TEXT_BYTES dup(0)

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

Queue_AddTask PROC pszSpec:QWORD
    ; Returns RAX=1 if enqueued, 0 if full
    cmp queueCount, MAX_QUEUE_TASKS
    jae @queue_full

    ; Compute destination pointer
    mov eax, queueWriteIndex
    imul eax, TASK_TEXT_BYTES
    lea rdx, queueStorage
    add rdx, rax

    ; Copy text (bounded by TASK_TEXT_BYTES-1)
    mov rcx, pszSpec
    call lstrcpyA

    ; Advance counters
    inc queueCount
    inc queueWriteIndex
    cmp queueWriteIndex, MAX_QUEUE_TASKS
    jb @store_done
    mov queueWriteIndex, 0

@store_done:
    mov rax, 1
    ret

@queue_full:
    xor rax, rax
    ret
Queue_AddTask ENDP

Queue_GetItem PROC index:QWORD
    ; Returns RAX pointer to task text or 0 if out of range
    mov rax, index
    cmp eax, queueCount
    jae @not_found

    imul eax, TASK_TEXT_BYTES
    lea rdx, queueStorage
    add rdx, rax
    mov rax, rdx
    ret

@not_found:
    xor rax, rax
    ret
Queue_GetItem ENDP

Queue_GetCount PROC
    mov eax, queueCount
    ret
Queue_GetCount ENDP

END
