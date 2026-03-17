; RawrXD_PulseScoreEngine.asm
; Pulse Effectiveness Scoring for Agentic Self-Optimization
; Pure MASM64 | Zero Dependencies | Cycle-Level Attribution

; ════════════════════════════════════════════════════════════════
; CONFIGURATION
; ════════════════════════════════════════════════════════════════
MAX_PULSE_SAMPLES       equ 64      ; Fixed-size ring buffer (audit-friendly)
PULSE_CYCLE_THRESHOLD   equ 5000    ; Cycles above which pulse is "expensive"
PULSE_DEMOTE_THRESHOLD  equ 3       ; Consecutive expensive pulses before demotion

; ════════════════════════════════════════════════════════════════
; PULSE SCORE ENTRY STRUCTURE
; ════════════════════════════════════════════════════════════════
PULSE_SCORE_ENTRY struct
    StartCycles     dq ?            ; TSC at pulse start
    EndCycles       dq ?            ; TSC at pulse end
    QueueDelta      dd ?            ; Change in pending commands (signed)
    MemDelta        dd ?            ; Change in available memory (signed)
    ResultCode      dd ?            ; 0=success, nonzero=failure
    Reserved        dd ?            ; Alignment padding
PULSE_SCORE_ENTRY ends

; ════════════════════════════════════════════════════════════════
; GLOBAL STATE
; ════════════════════════════════════════════════════════════════
.data?
PulseScoreRingBuffer    PULSE_SCORE_ENTRY MAX_PULSE_SAMPLES dup(<>)
PulseScoreIndex         dd 0
PulseExpensiveCount     dd 0        ; Consecutive expensive pulses
PulseTotalCount         dq 0        ; Total pulses recorded
PulseTotalCycles        dq 0        ; Cumulative cycles across all pulses
PulseSkipFlag           db 0        ; 1 = skip next pulse (throttling)

; ════════════════════════════════════════════════════════════════
; MACROS
; ════════════════════════════════════════════════════════════════

; RecordPulseScore: Store a pulse score entry in the ring buffer
; Arguments: start (qword), end (qword), qdelta (dword), mdelta (dword), result (dword)
RecordPulseScore MACRO start_cycles, end_cycles, qdelta, mdelta, result
    LOCAL indexReg, offsetReg
    push rax
    push rbx
    push rcx

    ; Get current index and wrap
    mov eax, PulseScoreIndex
    and eax, MAX_PULSE_SAMPLES - 1

    ; Calculate offset into ring buffer
    imul ebx, eax, SIZEOF PULSE_SCORE_ENTRY
    lea rcx, PulseScoreRingBuffer
    add rcx, rbx

    ; Store entry
    mov rax, start_cycles
    mov [rcx].PULSE_SCORE_ENTRY.StartCycles, rax
    mov rax, end_cycles
    mov [rcx].PULSE_SCORE_ENTRY.EndCycles, rax
    mov eax, qdelta
    mov [rcx].PULSE_SCORE_ENTRY.QueueDelta, eax
    mov eax, mdelta
    mov [rcx].PULSE_SCORE_ENTRY.MemDelta, eax
    mov eax, result
    mov [rcx].PULSE_SCORE_ENTRY.ResultCode, eax

    ; Increment index
    inc PulseScoreIndex

    ; Update totals
    inc PulseTotalCount
    mov rax, end_cycles
    sub rax, start_cycles
    add PulseTotalCycles, rax

    pop rcx
    pop rbx
    pop rax
ENDM

; ════════════════════════════════════════════════════════════════
; PROCEDURES
; ════════════════════════════════════════════════════════════════
.code

; CheckPulseEffectiveness: Returns 1 if pulse should run, 0 if skip
; Evaluates recent pulse performance and throttles if needed
CheckPulseEffectiveness PROC
    push rbx
    push rcx
    push rdx

    ; If skip flag is set, clear it and return 0
    cmp PulseSkipFlag, 1
    jne @check_expensive
    mov PulseSkipFlag, 0
    xor eax, eax
    jmp @done

@check_expensive:
    ; Check if last N pulses were all expensive
    cmp PulseExpensiveCount, PULSE_DEMOTE_THRESHOLD
    jl @allow_pulse

    ; Too many expensive pulses - skip next one
    mov PulseSkipFlag, 1
    mov PulseExpensiveCount, 0      ; Reset counter
    xor eax, eax
    jmp @done

@allow_pulse:
    mov eax, 1

@done:
    pop rdx
    pop rcx
    pop rbx
    ret
CheckPulseEffectiveness ENDP

; UpdatePulseMetrics: Called after each pulse to update effectiveness tracking
; rcx = cycle_delta (qword)
UpdatePulseMetrics PROC
    push rbx

    ; Check if pulse was expensive
    cmp rcx, PULSE_CYCLE_THRESHOLD
    jle @not_expensive

    ; Expensive pulse - increment counter
    inc PulseExpensiveCount
    jmp @done

@not_expensive:
    ; Reset expensive counter on good pulse
    mov PulseExpensiveCount, 0

@done:
    pop rbx
    ret
UpdatePulseMetrics ENDP

; GetPulseStats: Returns average cycle count per pulse in rax
GetPulseStats PROC
    mov rax, PulseTotalCycles
    mov rcx, PulseTotalCount
    test rcx, rcx
    jz @zero_count

    xor edx, edx
    div rcx                         ; rax = average cycles
    ret

@zero_count:
    xor eax, eax
    ret
GetPulseStats ENDP

; GetPulseScoreEntry: Returns pointer to entry at index (rcx) in rax
; For telemetry/dashboard access
GetPulseScoreEntry PROC
    and ecx, MAX_PULSE_SAMPLES - 1
    imul eax, ecx, SIZEOF PULSE_SCORE_ENTRY
    lea rax, PulseScoreRingBuffer
    add rax, rcx
    ret
GetPulseScoreEntry ENDP

; GetPulseScoreIndex: Returns current ring buffer index in eax
GetPulseScoreIndex PROC
    mov eax, PulseScoreIndex
    ret
GetPulseScoreIndex ENDP

; RawrXD_RecordPulseScoreProc: Procedure wrapper for cross-module calls
; rcx = start_cycles, rdx = end_cycles
; r8d = queue_delta, r9d = mem_delta
; [rsp+28h] = result_code, [rsp+30h] = pulse_type
RawrXD_RecordPulseScoreProc PROC
    push rbx
    push rdi
    
    ; Get current index and wrap
    mov eax, PulseScoreIndex
    and eax, MAX_PULSE_SAMPLES - 1
    
    ; Calculate offset into ring buffer
    imul ebx, eax, SIZEOF PULSE_SCORE_ENTRY
    lea rdi, PulseScoreRingBuffer
    add rdi, rbx
    
    ; Store entry
    mov [rdi].PULSE_SCORE_ENTRY.StartCycles, rcx
    mov [rdi].PULSE_SCORE_ENTRY.EndCycles, rdx
    mov [rdi].PULSE_SCORE_ENTRY.QueueDelta, r8d
    mov [rdi].PULSE_SCORE_ENTRY.MemDelta, r9d
    
    mov eax, [rsp+40h]              ; result_code (accounting for pushes + ret addr)
    mov [rdi].PULSE_SCORE_ENTRY.ResultCode, eax
    
    ; Increment index
    inc PulseScoreIndex
    
    ; Update totals
    inc PulseTotalCount
    mov rax, rdx
    sub rax, rcx
    add PulseTotalCycles, rax
    
    pop rdi
    pop rbx
    ret
RawrXD_RecordPulseScoreProc ENDP

; ResetPulseScoring: Clears all pulse scoring state
ResetPulseScoring PROC
    push rdi
    push rcx

    ; Zero the ring buffer
    lea rdi, PulseScoreRingBuffer
    mov rcx, SIZEOF PULSE_SCORE_ENTRY * MAX_PULSE_SAMPLES
    xor eax, eax
    rep stosb

    ; Reset counters
    mov PulseScoreIndex, 0
    mov PulseExpensiveCount, 0
    mov PulseTotalCount, 0
    mov PulseTotalCycles, 0
    mov PulseSkipFlag, 0

    pop rcx
    pop rdi
    ret
ResetPulseScoring ENDP

END
