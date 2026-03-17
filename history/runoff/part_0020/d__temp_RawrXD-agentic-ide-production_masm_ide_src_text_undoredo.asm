; ============================================================================
; File 24: text_undoredo.asm - Undo/Redo with intelligent coalescing
; ============================================================================
; Purpose: Command pattern history with 500ms coalescing + bounded memory
; Uses: Command stack, timestamp tracking, FIFO eviction, 64MB cap
; Functions: Init, PushCommand, Undo, Redo, CoalesceCommand, TrimHistory
; ============================================================================

.code

; CONSTANTS
; ============================================================================

UNDOSTACK_MAX_MEMORY    equ 67108864  ; 64MB
UNDOSTACK_COALESCE_MS   equ 500       ; milliseconds
UNDOSTACK_MAX_COMMANDS  equ 10000     ; absolute limit

; INITIALIZATION
; ============================================================================

UndoStack_Init PROC USES rbx rcx rdx rsi rdi
    ; Returns: UndoStack* in rax
    ; { commands[], commandCount, undoIndex, maxMemory, currentMemory, lastCommandTime, mutex }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax  ; process heap
    
    ; Allocate main struct (96 bytes)
    mov rdx, 0
    mov r8, 96
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = UndoStack*
    
    ; Allocate command array (10000 commands × ~128 bytes = 1.28MB)
    mov r8, 1310720  ; 10000 * 128 bytes
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rcx + 0], rax  ; commands array
    
    ; Initialize fields
    mov rcx, rbx
    mov qword ptr [rcx + 8], 0   ; commandCount = 0
    mov qword ptr [rcx + 16], 0  ; undoIndex = 0
    mov qword ptr [rcx + 24], UNDOSTACK_MAX_MEMORY  ; maxMemory
    mov qword ptr [rcx + 32], 0  ; currentMemory
    mov qword ptr [rcx + 40], 0  ; lastCommandTime
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    mov rax, rbx
    ret
UndoStack_Init ENDP

; PUSH COMMAND (Main Insert Point)
; ============================================================================

UndoStack_PushCommand PROC USES rbx rcx rdx rsi rdi r8 r9 undoStack:PTR DWORD, command:PTR DWORD
    ; undoStack = UndoStack*
    ; command = { type, position, length, data, timestamp }
    ; Returns: 1 if successful, 0 if failed
    
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, undoStack
    mov rsi, command
    
    ; Check if we can coalesce with previous command
    mov rax, [rcx + 8]  ; commandCount
    cmp rax, 0
    je @push_no_coalesce
    
    ; Get current time
    sub rsp, 40
    call QueryPerformanceCounter
    add rsp, 40
    mov r8, rax  ; r8 = current time
    
    mov r9, [rcx + 40]  ; lastCommandTime
    sub r8, r9
    
    ; Check if within coalesce window (rough estimate: time delta < threshold)
    ; In real impl would convert to milliseconds
    cmp r8, UNDOSTACK_COALESCE_MS
    jg @push_no_coalesce
    
    ; Attempt coalesce
    mov rax, [rcx + 8]
    dec rax
    mov rdx, [rcx + 0]  ; commands array
    lea rdi, [rdx + rax*128]  ; last command
    
    call UndoStack_CoalesceCommand
    cmp rax, 1
    je @push_done
    
@push_no_coalesce:
    ; Add new command
    mov rcx, undoStack
    mov rax, [rcx + 8]  ; commandCount
    cmp rax, UNDOSTACK_MAX_COMMANDS
    jge @push_overflow
    
    ; Store new command
    mov rsi, [rcx + 0]  ; commands array
    lea rdi, [rsi + rax*128]  ; new command slot
    
    mov rsi, command
    mov rcx, 16  ; copy 128 bytes (16 qwords)
    rep movsq
    
    ; Update counts
    mov rcx, undoStack
    mov rax, [rcx + 8]
    inc rax
    mov [rcx + 8], rax  ; commandCount++
    
    ; Update memory usage
    mov rsi, command
    mov rax, [rsi + 24]  ; command.dataLength
    add rax, 128  ; struct overhead
    mov rdx, [rcx + 32]
    add rdx, rax
    mov [rcx + 32], rdx  ; currentMemory += size
    
    ; Get timestamp
    sub rsp, 40
    call QueryPerformanceCounter
    add rsp, 40
    mov [rcx + 40], rax  ; lastCommandTime
    
    ; Check memory limit
    mov rsi, [rcx + 24]  ; maxMemory
    cmp rdx, rsi
    jle @push_done
    
    ; Trim history if over limit
    call UndoStack_TrimHistory
    
@push_done:
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
    
@push_overflow:
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 0
    ret
UndoStack_PushCommand ENDP

; COALESCE COMMANDS
; ============================================================================

UndoStack_CoalesceCommand PROC USES rbx rcx rdx rsi rdi r8 r9 lastCommand:PTR DWORD, newCommand:PTR DWORD
    ; lastCommand = previous command
    ; newCommand = new command
    ; Returns: 1 if coalesced, 0 if kept separate
    
    mov rcx, lastCommand
    mov rsi, newCommand
    
    ; Get types
    mov eax, dword ptr [rcx + 0]  ; lastCommand.type
    mov edx, dword ptr [rsi + 0]  ; newCommand.type
    
    ; Both must be INSERT or both DELETE at sequential positions
    cmp eax, edx
    jne @coalesce_fail
    
    ; Check position continuity
    ; lastPosition + lastLength == newPosition
    mov r8, [rcx + 8]   ; lastCommand.position
    mov r9, [rcx + 16]  ; lastCommand.length
    add r8, r9
    
    mov r10, [rsi + 8]  ; newCommand.position
    cmp r8, r10
    jne @coalesce_fail
    
    ; Coalesce: append new data to last command
    mov r8, [rcx + 16]  ; lastLength
    mov r9, [rsi + 16]  ; newLength
    
    mov r10, [rcx + 24] ; lastData ptr
    mov r11, [rsi + 24] ; newData ptr
    
    ; Append newData to lastData
    mov rdi, r10
    add rdi, r8
    mov rsi, r11
    mov rcx, r9
    rep movsb
    
    ; Update lastCommand.length
    mov rcx, lastCommand
    mov r8, [rcx + 16]
    add r8, r9
    mov [rcx + 16], r8
    
    mov rax, 1  ; success
    ret
    
@coalesce_fail:
    mov rax, 0
    ret
UndoStack_CoalesceCommand ENDP

; UNDO
; ============================================================================

UndoStack_Undo PROC USES rbx rcx rdx rsi rdi undoStack:PTR DWORD
    ; undoStack = UndoStack*
    ; Returns: command* in rax (or NULL if at beginning)
    
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, undoStack
    mov rax, [rcx + 16]  ; undoIndex
    cmp rax, 0
    je @undo_at_start
    
    dec rax
    mov [rcx + 16], rax  ; undoIndex--
    
    ; Return command at current undoIndex
    mov rsi, [rcx + 0]   ; commands array
    lea rax, [rsi + rax*128]
    
    jmp @undo_exit
    
@undo_at_start:
    mov rax, 0
    
@undo_exit:
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
UndoStack_Undo ENDP

; REDO
; ============================================================================

UndoStack_Redo PROC USES rbx rcx rdx rsi rdi undoStack:PTR DWORD
    ; undoStack = UndoStack*
    ; Returns: command* in rax (or NULL if at end)
    
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, undoStack
    mov rax, [rcx + 16]  ; undoIndex
    mov rdx, [rcx + 8]   ; commandCount
    
    cmp rax, rdx
    jge @redo_at_end
    
    ; Return command at current undoIndex, then increment
    mov rsi, [rcx + 0]   ; commands array
    lea rax, [rsi + rax*128]
    
    mov rdx, [rcx + 16]
    inc rdx
    mov [rcx + 16], rdx  ; undoIndex++
    
    jmp @redo_exit
    
@redo_at_end:
    mov rax, 0
    
@redo_exit:
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
UndoStack_Redo ENDP

; REMOVE OLDEST COMMAND
; ============================================================================

UndoStack_RemoveOldest PROC USES rbx rcx rdx rsi rdi undoStack:PTR DWORD
    ; Remove first command and shift all others down
    ; Returns: bytes freed in rax
    
    mov rcx, undoStack
    mov rsi, [rcx + 0]  ; commands array
    
    ; Get size of first command
    mov rax, [rsi + 24]  ; first command data length
    add rax, 128        ; struct overhead
    
    mov r8, [rcx + 8]  ; commandCount
    dec r8
    
    ; Shift all commands down by one slot
    mov rdi, rsi
    mov rsi, [rsi + 128]  ; start of next command
    mov rcx, r8
    imul rcx, 128       ; total bytes to move
    rep movsb
    
    ; Update counts
    mov rcx, undoStack
    mov r8, [rcx + 8]
    dec r8
    mov [rcx + 8], r8   ; commandCount--
    
    mov rdx, [rcx + 32]
    sub rdx, rax
    mov [rcx + 32], rdx ; currentMemory -= freed
    
    mov rdx, [rcx + 16]
    dec rdx
    mov [rcx + 16], rdx ; undoIndex-- (stay valid)
    
    ret
UndoStack_RemoveOldest ENDP

; TRIM HISTORY
; ============================================================================

UndoStack_TrimHistory PROC USES rbx rcx rdx rsi rdi r8 undoStack:PTR DWORD
    ; Remove oldest commands until memory is under limit
    ; Returns: bytes freed in rax
    
    mov rcx, undoStack
    xor rax, rax  ; total freed
    
@trim_loop:
    mov rdx, [rcx + 32]  ; currentMemory
    mov rsi, [rcx + 24]  ; maxMemory
    cmp rdx, rsi
    jle @trim_done
    
    ; Remove oldest
    call UndoStack_RemoveOldest
    
    add rax, rax  ; accumulate freed
    
    mov rcx, undoStack
    jmp @trim_loop
    
@trim_done:
    ret
UndoStack_TrimHistory ENDP

; GET MEMORY USAGE
; ============================================================================

UndoStack_GetMemoryUsage PROC USES rbx rcx rdx undoStack:PTR DWORD
    ; undoStack = UndoStack*
    ; Returns: memory used in rax
    
    mov rcx, undoStack
    mov rax, [rcx + 32]  ; currentMemory
    ret
UndoStack_GetMemoryUsage ENDP

; CLEAR HISTORY
; ============================================================================

UndoStack_Clear PROC USES rbx rcx rdx rsi rdi undoStack:PTR DWORD
    ; Clear all history
    
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, undoStack
    mov qword ptr [rcx + 8], 0   ; commandCount = 0
    mov qword ptr [rcx + 16], 0  ; undoIndex = 0
    mov qword ptr [rcx + 32], 0  ; currentMemory = 0
    
    mov rcx, undoStack
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
UndoStack_Clear ENDP

end
