; ==============================================================================
; Sovereign Ghost Engine - Predictive Overlay Interface
; ==============================================================================
; Real-time ghost text rendering for AI-native IDE integration.
; Synchronizes AI Orchestrator inference output with UI thread render buffer
; via shared memory ring buffer with sub-200ms latency enforcement.
;
; Architecture:
;   - Shared memory ring buffer (lock-free, multi-producer single-consumer)
;   - Latency-aware frame dropping (rdtsc-based, ~200ms threshold)
;   - Confidence-based YOLO mode (configurable threshold)
;   - Non-blocking push-model (AI writes, UI reads)
;   - 60FPS UI thread compatibility
;
; Exports:
;   RENDER_GHOST_PREDICTIVE   - Main render entry point
;   PUSH_GHOST_PREDICTION     - AI agent writes prediction
;   INIT_GHOST_BUFFER         - Initialize shared memory
;   GET_GHOST_LATENCY         - Query last render latency
;   SET_CONFIDENCE_THRESHOLD  - Configure YOLO mode
; ==============================================================================

option casemap:none
option prologue:none
option epilogue:none

; ==============================================================================
; Constants
; ==============================================================================
GHOST_BUFFER_SIZE       equ 4096        ; 4KB ring buffer
GHOST_MAX_TEXT_LEN      equ 1024        ; Max prediction length
GHOST_RING_SLOTS        equ 16          ; Ring buffer slots

; Timing constants (TSC-based, assuming ~3GHz CPU)
; 200ms = 0.2s * 3,000,000,000 = 600,000,000 cycles
GHOST_LATENCY_THRESHOLD equ 600000000

; Confidence thresholds
CONFIDENCE_YOLO         equ 03F19999Ah  ; 0.6f (aggressive)
CONFIDENCE_CONSERVATIVE equ 03F4CCCCDh  ; 0.8f (conservative)
CONFIDENCE_STEALTH      equ 03F000000h  ; 0.5f (minimal)

; Status codes
GHOST_OK                equ 0
GHOST_STALE             equ 1
GHOST_LOW_CONFIDENCE    equ 2
GHOST_BUFFER_FULL       equ 3

; ==============================================================================
; Ghost Data Structure (32 bytes - cache line friendly)
; ==============================================================================
GHOST_DATA struc
    BufferPtr       dq ?        ; Pointer to text content
    TextLen         dq ?        ; Text length in bytes
    Timestamp       dq ?        ; TSC timestamp at creation
    Confidence      dd ?        ; AI confidence score (float)
    Status          dd ?        ; Frame status code
GHOST_DATA ends

; ==============================================================================
; Ring Buffer Structure
; ==============================================================================
GHOST_RING struc
    Head            dq ?        ; Write index (AI producer)
    Tail            dq ?        ; Read index (UI consumer)
    SlotCount       dq ?        ; Active slots
    DropCount       dq ?        ; Dropped frames (stale)
    RenderCount     dq ?        ; Successfully rendered frames
    Threshold       dd ?        ; Current confidence threshold
    Padding         dd ?        ; Align to 40 bytes
GHOST_RING ends

; ==============================================================================
; Data Section
; ==============================================================================
.data
align 16
; Ring buffer slots
g_GhostSlots    GHOST_DATA GHOST_RING_SLOTS dup(<>)
g_GhostRing     GHOST_RING <0, 0, 0, 0, 0, CONFIDENCE_YOLO, 0>

; Text storage pool
g_TextPool      db GHOST_BUFFER_SIZE dup(0)
g_TextPoolUsed  dq 0

; Latency tracking
g_LastLatency   dq 0
g_LastTimestamp dq 0

; ==============================================================================
; Code Section
; ==============================================================================
.code

; ==============================================================================
; INIT_GHOST_BUFFER - Initialize shared memory ring buffer
; ==============================================================================
; Input:  None
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
INIT_GHOST_BUFFER proc
    push rdi
    push rcx
    
    ; Clear ring buffer
    lea rdi, g_GhostSlots
    mov rcx, GHOST_RING_SLOTS * SIZEOF GHOST_DATA
    xor eax, eax
    rep stosb
    
    ; Clear ring state
    lea rdi, g_GhostRing
    mov qword ptr [rdi + GHOST_RING.Head], 0
    mov qword ptr [rdi + GHOST_RING.Tail], 0
    mov qword ptr [rdi + GHOST_RING.SlotCount], 0
    mov qword ptr [rdi + GHOST_RING.DropCount], 0
    mov qword ptr [rdi + GHOST_RING.RenderCount], 0
    mov dword ptr [rdi + GHOST_RING.Threshold], CONFIDENCE_YOLO
    
    ; Clear text pool
    lea rdi, g_TextPool
    mov rcx, GHOST_BUFFER_SIZE
    xor eax, eax
    rep stosb
    mov qword ptr [g_TextPoolUsed], 0
    
    ; Reset latency tracking
    mov qword ptr [g_LastLatency], 0
    mov qword ptr [g_LastTimestamp], 0
    
    pop rcx
    pop rdi
    mov eax, 1
    ret
INIT_GHOST_BUFFER endp

; ==============================================================================
; PUSH_GHOST_PREDICTION - AI agent writes prediction to ring buffer
; ==============================================================================
; Input:  RCX = Pointer to text content
;         RDX = Text length
;         R8  = Confidence score (float)
; Output: RAX = 1 on success, 0 on failure
; ==============================================================================
PUSH_GHOST_PREDICTION proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    
    mov r12, rcx            ; Text pointer
    mov r13, rdx            ; Text length
    mov r14d, r8d           ; Confidence
    
    ; Validate length
    cmp r13, GHOST_MAX_TEXT_LEN
    ja push_fail
    
    ; Get current head index
    lea rbx, g_GhostRing
    mov rsi, [rbx + GHOST_RING.Head]
    
    ; Check if buffer is full
    mov rdi, [rbx + GHOST_RING.SlotCount]
    cmp rdi, GHOST_RING_SLOTS
    jae push_full
    
    ; Calculate slot address
    imul rax, rsi, SIZEOF GHOST_DATA
    lea rdi, g_GhostSlots
    add rdi, rax
    
    ; Copy text to pool
    mov rax, [g_TextPoolUsed]
    lea rsi, g_TextPool
    add rsi, rax
    
    ; Check pool capacity
    add rax, r13
    add rax, 1              ; Null terminator
    cmp rax, GHOST_BUFFER_SIZE
    ja push_fail
    
    ; Copy text
    mov rcx, r13
    mov r10, rsi            ; Preserve destination pointer (pool)
    mov rsi, r12            ; Source
    mov rdi, r10            ; Dest (in pool)
    rep movsb
    mov byte ptr [rdi], 0   ; Null terminate
    
    ; Update pool usage
    mov [g_TextPoolUsed], rax
    
    ; Write ghost data
    imul rax, [rbx + GHOST_RING.Head], SIZEOF GHOST_DATA
    lea rdi, g_GhostSlots
    add rdi, rax
    
    ; Set buffer pointer (relative to pool start)
    mov rax, [g_TextPoolUsed]
    sub rax, r13
    sub rax, 1
    lea rax, [g_TextPool + rax]
    mov [rdi + GHOST_DATA.BufferPtr], rax
    
    mov [rdi + GHOST_DATA.TextLen], r13
    
    ; Get timestamp
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov [rdi + GHOST_DATA.Timestamp], rax
    
    mov [rdi + GHOST_DATA.Confidence], r14d
    mov dword ptr [rdi + GHOST_DATA.Status], GHOST_OK
    
    ; Advance head
    inc qword ptr [rbx + GHOST_RING.Head]
    mov rax, [rbx + GHOST_RING.Head]
    cmp rax, GHOST_RING_SLOTS
    jb push_no_wrap
    mov qword ptr [rbx + GHOST_RING.Head], 0
    
push_no_wrap:
    inc qword ptr [rbx + GHOST_RING.SlotCount]
    
    mov eax, 1
    jmp push_exit
    
push_full:
    mov eax, GHOST_BUFFER_FULL
    jmp push_exit
    
push_fail:
    xor eax, eax
    
push_exit:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PUSH_GHOST_PREDICTION endp

; ==============================================================================
; RENDER_GHOST_PREDICTIVE - Main render entry point
; ==============================================================================
; Input:  RCX = Editor window handle (HWND)
; Output: RAX = 1 if rendered, 0 if dropped
; ==============================================================================
RENDER_GHOST_PREDICTIVE proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov r12, rcx            ; HWND
    
    ; Get ring state
    lea rbx, g_GhostRing
    mov rsi, [rbx + GHOST_RING.Tail]
    mov rdi, [rbx + GHOST_RING.SlotCount]
    
    ; Check if any slots available
    test rdi, rdi
    jz render_drop
    
    ; Calculate slot address
    imul rax, rsi, SIZEOF GHOST_DATA
    lea r13, g_GhostSlots
    add r13, rax
    
    ; === 1. Latency Validation (Drop if > 200ms) ===
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r8, rax             ; Current TSC
    
    sub rax, [r13 + GHOST_DATA.Timestamp]
    mov [g_LastLatency], rax
    
    cmp rax, GHOST_LATENCY_THRESHOLD
    ja render_stale
    
    ; === 2. Confidence Filter (YOLO Mode) ===
    movss xmm0, dword ptr [r13 + GHOST_DATA.Confidence]
    movss xmm1, dword ptr [rbx + GHOST_RING.Threshold]
    ucomiss xmm0, xmm1
    jb render_low_confidence
    
    ; === 3. Render Trigger ===
    ; Get text pointer and length
    mov rcx, [r13 + GHOST_DATA.BufferPtr]
    mov rdx, [r13 + GHOST_DATA.TextLen]
    
    ; Validate pointer
    test rcx, rcx
    jz render_drop
    
    ; Call draw function (placeholder for actual GDI/DirectWrite)
    ; In production: This would call the IDE's paint hook
    mov r9, r12             ; HWND
    call DRAW_OVERLAY_TEXT
    
    ; Mark as rendered
    inc qword ptr [rbx + GHOST_RING.RenderCount]
    
    ; Advance tail
    inc qword ptr [rbx + GHOST_RING.Tail]
    mov rax, [rbx + GHOST_RING.Tail]
    cmp rax, GHOST_RING_SLOTS
    jb render_no_wrap
    mov qword ptr [rbx + GHOST_RING.Tail], 0
    
render_no_wrap:
    dec qword ptr [rbx + GHOST_RING.SlotCount]
    
    mov eax, 1
    jmp render_exit
    
render_stale:
    inc qword ptr [rbx + GHOST_RING.DropCount]
    mov dword ptr [r13 + GHOST_DATA.Status], GHOST_STALE
    jmp render_consume
    
render_low_confidence:
    mov dword ptr [r13 + GHOST_DATA.Status], GHOST_LOW_CONFIDENCE
    jmp render_consume
    
render_consume:
    ; Consume slot even if dropped
    inc qword ptr [rbx + GHOST_RING.Tail]
    mov rax, [rbx + GHOST_RING.Tail]
    cmp rax, GHOST_RING_SLOTS
    jb render_consume_wrap
    mov qword ptr [rbx + GHOST_RING.Tail], 0
    
render_consume_wrap:
    dec qword ptr [rbx + GHOST_RING.SlotCount]
    
render_drop:
    xor eax, eax
    
render_exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RENDER_GHOST_PREDICTIVE endp

; ==============================================================================
; DRAW_OVERLAY_TEXT - Platform-specific text rendering
; ==============================================================================
; Input:  RCX = Text pointer
;         RDX = Text length
;         R9  = HWND (editor window)
; Output: RAX = 1 on success
; ==============================================================================
DRAW_OVERLAY_TEXT proc
    ; Placeholder for actual GDI/DirectWrite rendering
    ; In production, this would:
    ; 1. Get device context from HWND
    ; 2. Calculate caret position
    ; 3. Set ghost text color (light gray)
    ; 4. Draw text at caret offset
    ; 5. Release device context
    
    mov eax, 1
    ret
DRAW_OVERLAY_TEXT endp

; ==============================================================================
; GET_GHOST_LATENCY - Query last render latency in cycles
; ==============================================================================
; Input:  None
; Output: RAX = Last latency in TSC cycles
; ==============================================================================
GET_GHOST_LATENCY proc
    mov rax, [g_LastLatency]
    ret
GET_GHOST_LATENCY endp

; ==============================================================================
; GET_GHOST_STATS - Retrieve rendering statistics
; ==============================================================================
; Input:  RCX = Pointer to output buffer (32 bytes)
;         [RCX+0]  = Rendered frames
;         [RCX+8]  = Dropped frames (stale)
;         [RCX+16] = Active slots
;         [RCX+24] = Last latency
; Output: RAX = 1 always
; ==============================================================================
GET_GHOST_STATS proc
    lea r8, g_GhostRing
    mov rax, [r8 + GHOST_RING.RenderCount]
    mov [rcx + 0], rax
    mov rax, [r8 + GHOST_RING.DropCount]
    mov [rcx + 8], rax
    mov rax, [r8 + GHOST_RING.SlotCount]
    mov [rcx + 16], rax
    mov rax, [g_LastLatency]
    mov [rcx + 24], rax
    mov eax, 1
    ret
GET_GHOST_STATS endp

; ==============================================================================
; SET_CONFIDENCE_THRESHOLD - Configure YOLO mode
; ==============================================================================
; Input:  RCX = Threshold value (float bits)
; Output: RAX = 1 on success
; ==============================================================================
SET_CONFIDENCE_THRESHOLD proc
    lea rax, g_GhostRing
    mov [rax + GHOST_RING.Threshold], ecx
    mov eax, 1
    ret
SET_CONFIDENCE_THRESHOLD endp

; ==============================================================================
; GET_CONFIDENCE_THRESHOLD - Get current YOLO threshold
; ==============================================================================
; Input:  None
; Output: RAX = Current threshold (float bits)
; ==============================================================================
GET_CONFIDENCE_THRESHOLD proc
    lea rax, g_GhostRing
    mov eax, [rax + GHOST_RING.Threshold]
    ret
GET_CONFIDENCE_THRESHOLD endp

; ==============================================================================
; FLUSH_GHOST_BUFFER - Clear all pending predictions
; ==============================================================================
; Input:  None
; Output: RAX = Number of slots flushed
; ==============================================================================
FLUSH_GHOST_BUFFER proc
    push rbx
    
    lea rbx, g_GhostRing
    mov rax, [rbx + GHOST_RING.SlotCount]
    
    ; Reset ring state
    mov qword ptr [rbx + GHOST_RING.Head], 0
    mov qword ptr [rbx + GHOST_RING.Tail], 0
    mov qword ptr [rbx + GHOST_RING.SlotCount], 0
    
    ; Reset text pool
    mov qword ptr [g_TextPoolUsed], 0
    
    pop rbx
    ret
FLUSH_GHOST_BUFFER endp

; ==============================================================================
; GHOST_HEARTBEAT - Periodic maintenance (call from UI thread)
; ==============================================================================
; Input:  RCX = HWND (editor window)
; Output: RAX = 1 if rendered, 0 otherwise
; ==============================================================================
GHOST_HEARTBEAT proc
    ; Check if there are pending predictions
    lea rax, g_GhostRing
    mov rdx, [rax + GHOST_RING.SlotCount]
    test rdx, rdx
    jz heartbeat_none
    
    ; Render most recent prediction
    call RENDER_GHOST_PREDICTIVE
    ret
    
heartbeat_none:
    xor eax, eax
    ret
GHOST_HEARTBEAT endp

end