; ╔══════════════════════════════════════════════════════════════════════════════╗
; ║ soft_throttle_dispatch.asm - PWM-Based Thermal Throttling Dispatch Loop     ║
; ║                                                                              ║
; ║ Purpose: Implement variable-delay soft throttling for NVMe I/O using        ║
; ║          PAUSE instruction PWM to regulate data throughput                  ║
; ║                                                                              ║
; ║ Author: RawrXD IDE Team                                                      ║
; ║ Version: 1.2.0                                                               ║
; ║ Target: x86-64 / AMD64 / Intel EM64T                                        ║
; ╚══════════════════════════════════════════════════════════════════════════════╝

; ═══════════════════════════════════════════════════════════════════════════════
; Build: ml64 /c /Fo soft_throttle_dispatch.obj soft_throttle_dispatch.asm
; ═══════════════════════════════════════════════════════════════════════════════

; ═══════════════════════════════════════════════════════════════════════════════
; Control Block Offsets (must match SovereignControlBlock.h)
; ═══════════════════════════════════════════════════════════════════════════════

SCB_MAGIC               EQU 0
SCB_VERSION             EQU 4
SCB_SEQUENCE            EQU 8
SCB_TIMESTAMP           EQU 16
SCB_BURST_MODE          EQU 24
SCB_ACTIVE_DRIVES       EQU 28
SCB_NVME_TEMPS          EQU 64      ; Array of 5 doubles (40 bytes)
SCB_GPU_TEMP            EQU 104
SCB_CPU_TEMP            EQU 112
SCB_THERMAL_THRESHOLD   EQU 120
SCB_THROTTLE            EQU 128
SCB_THROTTLE_PERCENT    EQU 128     ; int32 at offset 0 of throttle struct
SCB_THROTTLE_FLAGS      EQU 132     ; uint32 at offset 4
SCB_PAUSE_LOOP_COUNT    EQU 136     ; int32 at offset 8
SCB_SOFT_THROTTLE       EQU 144     ; double
SCB_HEADROOM            EQU 152     ; double
SCB_PREDICTION_VALID    EQU 160     ; int32
SCB_DRIVE_CMD           EQU 192
SCB_SELECTED_DRIVE      EQU 192     ; int32 at offset 0 of drive cmd
SCB_PREVIOUS_DRIVE      EQU 196     ; int32 at offset 4
SCB_DRIVE_HEADROOM      EQU 200     ; double at offset 8
SCB_AUTH                EQU 320
SCB_SESSION_KEY         EQU 320     ; uint64 at offset 0 of auth

; Burst mode values
BURST_SOVEREIGN_MAX     EQU 0
BURST_THERMAL_GOVERNED  EQU 1
BURST_ADAPTIVE_HYBRID   EQU 2

; Throttle flags
FLAG_SOFT_THROTTLE      EQU 01h
FLAG_HARD_THROTTLE      EQU 02h
FLAG_EMERGENCY_STOP     EQU 04h
FLAG_PREDICTIVE         EQU 08h
FLAG_DRIVE_SWITCH       EQU 10h
FLAG_SESSION_VALID      EQU 20h

; Magic value for validation
SCB_MAGIC_VALUE         EQU 052415752h  ; 'RAWR'

.DATA
    ALIGN 8
    XorState    dq 0                   ; Entropy state for fast jitter

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Fast Entropy Seed (RDRAND once) and XorShift64* generator
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC SeedXorEntropy
PUBLIC GenerateEntropy64

SeedXorEntropy PROC
    ; Seed XorState using hardware RNG (retry until carry set)
@@retry_seed:
    rdrand  rax
    jnc     @@retry_seed
    mov     XorState, rax
    ret
SeedXorEntropy ENDP

GenerateEntropy64 PROC
    ; XorShift64* (Marsaglia) – ~10–15 cycles
    mov     rax, XorState
    test    rax, rax
    jnz     @@seeded
    ; Fallback seed if caller did not prime (use TSC)
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     XorState, rax

@@seeded:

    mov     rdx, rax
    shr     rdx, 12
    xor     rax, rdx                 ; x ^= x >> 12

    mov     rdx, rax
    shl     rdx, 25
    xor     rax, rdx                 ; x ^= x << 25

    mov     rdx, rax
    shr     rdx, 27
    xor     rax, rdx                 ; x ^= x >> 27

    mov     XorState, rax

    mov     rdx, 2545F4914F6CDD1Dh   ; Multiply by Marsaglia constant
    mul     rdx                      ; Lower 64 bits returned in RAX
    ret
GenerateEntropy64 ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Validate Control Block
; ═══════════════════════════════════════════════════════════════════════════════
; bool validateControlBlock(void* controlBlock)
; RCX = control block pointer
; Returns: 1 if valid, 0 if invalid
; ═══════════════════════════════════════════════════════════════════════════════

validateControlBlock PROC
    test    rcx, rcx
    jz      @@invalid
    
    ; Check magic number
    mov     eax, [rcx + SCB_MAGIC]
    cmp     eax, SCB_MAGIC_VALUE
    jne     @@invalid
    
    ; Valid
    mov     eax, 1
    ret
    
@@invalid:
    xor     eax, eax
    ret
validateControlBlock ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Soft Throttle with PAUSE Instruction
; ═══════════════════════════════════════════════════════════════════════════════
; void applySoftThrottle(int pauseCount)
; ECX = number of PAUSE iterations
; 
; The PAUSE instruction is a hint to the processor that a spin-wait loop is
; being executed. It provides better power efficiency and reduces memory
; ordering violations that could cause pipeline flushes.
; ═══════════════════════════════════════════════════════════════════════════════

applySoftThrottle PROC
    test    ecx, ecx
    jle     @@no_throttle

    ; Add small jitter (0-15) using fast entropy to smooth spin-waits
    push    rax
    call    GenerateEntropy64
    and     eax, 0Fh
    add     ecx, eax
    pop     rax
    
    ; Execute PAUSE loop
@@pause_loop:
    pause                           ; ~10-140 cycles depending on CPU
    dec     ecx
    jnz     @@pause_loop
    
@@no_throttle:
    ret
applySoftThrottle ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PWM Throttle - Duty Cycle Based Throttling
; ═══════════════════════════════════════════════════════════════════════════════
; void applyPWMThrottle(int throttlePercent, int cycleCount)
; ECX = throttle percentage (0-100)
; EDX = number of PWM cycles to execute
;
; Uses PWM pattern to regulate throughput:
; - throttlePercent determines ratio of PAUSE to NOP
; - Higher percentage = more PAUSE = slower throughput
; ═══════════════════════════════════════════════════════════════════════════════

applyPWMThrottle PROC
    push    rbx
    push    r12
    push    r13
    
    mov     r12d, ecx               ; Throttle percent
    mov     r13d, edx               ; Cycle count
    
    ; Clamp throttle to 0-100
    test    r12d, r12d
    jns     @@clamp_upper
    xor     r12d, r12d
    jmp     @@pwm_start
@@clamp_upper:
    cmp     r12d, 100
    jle     @@pwm_start
    mov     r12d, 100
    
@@pwm_start:
    ; Each cycle: if (iteration % 100) < throttlePercent then PAUSE
    xor     ebx, ebx                ; Iteration counter
    
@@pwm_cycle:
    cmp     ebx, r13d
    jge     @@pwm_done
    
    ; Calculate position in duty cycle (iteration % 100)
    mov     eax, ebx
    xor     edx, edx
    mov     ecx, 100
    div     ecx                     ; EDX = remainder (0-99)
    
    ; If remainder < throttlePercent, do PAUSE (throttle active)
    cmp     edx, r12d
    jge     @@pwm_nop
    
    pause                           ; Throttle cycle
    jmp     @@pwm_next
    
@@pwm_nop:
    nop                             ; Active cycle (no delay)
    
@@pwm_next:
    inc     ebx
    jmp     @@pwm_cycle
    
@@pwm_done:
    pop     r13
    pop     r12
    pop     rbx
    ret
applyPWMThrottle ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Dispatch Loop - Main Thermal Control Loop
; ═══════════════════════════════════════════════════════════════════════════════
; int dispatchLoop(void* controlBlock, int maxIterations)
; RCX = control block pointer
; EDX = maximum iterations (0 = infinite)
; Returns: Exit reason (0 = normal, 1 = emergency, 2 = invalid)
;
; This is the main kernel loop that reads throttle commands from shared memory
; and applies appropriate throttling using PAUSE instruction PWM.
; ═══════════════════════════════════════════════════════════════════════════════

dispatchLoop PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    mov     r12, rcx                ; Control block pointer
    mov     r13d, edx               ; Max iterations
    xor     r14d, r14d              ; Current iteration
    
    ; Validate control block
    mov     rcx, r12
    call    validateControlBlock
    test    eax, eax
    jz      @@dispatch_invalid
    
@@dispatch_main:
    ; Check iteration limit
    test    r13d, r13d
    jz      @@dispatch_continue     ; 0 = infinite
    cmp     r14d, r13d
    jge     @@dispatch_normal_exit
    
@@dispatch_continue:
    ; Read throttle flags
    mov     eax, [r12 + SCB_THROTTLE_FLAGS]
    mov     r15d, eax               ; Save flags
    
    ; Check for emergency stop
    test    eax, FLAG_EMERGENCY_STOP
    jnz     @@dispatch_emergency
    
    ; Check burst mode
    mov     eax, [r12 + SCB_BURST_MODE]
    cmp     eax, BURST_SOVEREIGN_MAX
    je      @@dispatch_sovereign
    cmp     eax, BURST_THERMAL_GOVERNED
    je      @@dispatch_governed
    ; Default: BURST_ADAPTIVE_HYBRID
    jmp     @@dispatch_adaptive
    
; ═══════════════════════════════════════════════════════════════════════════════
; SOVEREIGN MAX Mode - Minimal throttling
; ═══════════════════════════════════════════════════════════════════════════════
@@dispatch_sovereign:
    ; Only throttle if predictive warning is active
    test    r15d, FLAG_PREDICTIVE
    jz      @@dispatch_no_throttle
    
    ; Check if prediction indicates imminent thermal issue
    mov     eax, [r12 + SCB_PREDICTION_VALID]
    test    eax, eax
    jz      @@dispatch_no_throttle
    
    ; Light throttle: 10% of commanded
    mov     eax, [r12 + SCB_PAUSE_LOOP_COUNT]
    shr     eax, 3                  ; Divide by 8 (~12.5%)
    mov     ecx, eax
    call    applySoftThrottle
    jmp     @@dispatch_next
    
; ═══════════════════════════════════════════════════════════════════════════════
; THERMAL GOVERNED Mode - Maximum throttling
; ═══════════════════════════════════════════════════════════════════════════════
@@dispatch_governed:
    ; Always apply full throttle
    mov     ecx, [r12 + SCB_PAUSE_LOOP_COUNT]
    test    ecx, ecx
    jz      @@dispatch_baseline_throttle
    call    applySoftThrottle
    jmp     @@dispatch_next
    
@@dispatch_baseline_throttle:
    ; Baseline throttle even if no command
    mov     ecx, 50                 ; Minimum 50 PAUSE cycles
    call    applySoftThrottle
    jmp     @@dispatch_next
    
; ═══════════════════════════════════════════════════════════════════════════════
; ADAPTIVE HYBRID Mode - Smart throttling based on thermal headroom
; ═══════════════════════════════════════════════════════════════════════════════
@@dispatch_adaptive:
    ; Check if soft throttle flag is set
    test    r15d, FLAG_SOFT_THROTTLE
    jz      @@dispatch_check_headroom
    
    ; Apply commanded throttle
    mov     ecx, [r12 + SCB_PAUSE_LOOP_COUNT]
    test    ecx, ecx
    jz      @@dispatch_check_headroom
    call    applySoftThrottle
    jmp     @@dispatch_next
    
@@dispatch_check_headroom:
    ; Load thermal headroom (double at SCB_HEADROOM)
    movsd   xmm0, qword ptr [r12 + SCB_HEADROOM]
    
    ; Convert to integer for comparison (truncate)
    cvttsd2si eax, xmm0
    
    ; Headroom-based throttle calculation
    ; < 5°C headroom: heavy throttle
    ; 5-10°C: moderate throttle
    ; 10-20°C: light throttle
    ; > 20°C: no throttle
    
    cmp     eax, 5
    jl      @@dispatch_heavy
    cmp     eax, 10
    jl      @@dispatch_moderate
    cmp     eax, 20
    jl      @@dispatch_light
    jmp     @@dispatch_no_throttle
    
@@dispatch_heavy:
    mov     ecx, 500                ; 500 PAUSE cycles
    call    applySoftThrottle
    jmp     @@dispatch_next
    
@@dispatch_moderate:
    mov     ecx, 200                ; 200 PAUSE cycles
    call    applySoftThrottle
    jmp     @@dispatch_next
    
@@dispatch_light:
    mov     ecx, 50                 ; 50 PAUSE cycles
    call    applySoftThrottle
    jmp     @@dispatch_next
    
@@dispatch_no_throttle:
    ; No throttle needed, but yield to prevent busy-loop
    pause
    
@@dispatch_next:
    ; Check for drive switch pending
    test    r15d, FLAG_DRIVE_SWITCH
    jz      @@dispatch_inc
    
    ; Acknowledge drive switch by brief pause
    mov     ecx, 20
    call    applySoftThrottle
    
@@dispatch_inc:
    inc     r14d
    jmp     @@dispatch_main
    
; ═══════════════════════════════════════════════════════════════════════════════
; Exit paths
; ═══════════════════════════════════════════════════════════════════════════════
@@dispatch_normal_exit:
    xor     eax, eax                ; Return 0 = normal
    jmp     @@dispatch_done
    
@@dispatch_emergency:
    mov     eax, 1                  ; Return 1 = emergency
    jmp     @@dispatch_done
    
@@dispatch_invalid:
    mov     eax, 2                  ; Return 2 = invalid control block
    
@@dispatch_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
dispatchLoop ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Single Dispatch Iteration
; ═══════════════════════════════════════════════════════════════════════════════
; int dispatchOnce(void* controlBlock)
; RCX = control block pointer
; Returns: Throttle amount applied
;
; Performs a single dispatch iteration - useful for polling-based integration
; ═══════════════════════════════════════════════════════════════════════════════

dispatchOnce PROC
    push    rbx
    
    mov     rbx, rcx
    
    ; Validate control block
    call    validateControlBlock
    test    eax, eax
    jz      @@once_invalid
    
    ; Read and apply throttle
    mov     eax, [rbx + SCB_THROTTLE_FLAGS]
    test    eax, FLAG_EMERGENCY_STOP
    jnz     @@once_emergency
    
    test    eax, FLAG_SOFT_THROTTLE
    jz      @@once_none
    
    mov     ecx, [rbx + SCB_PAUSE_LOOP_COUNT]
    mov     eax, ecx                ; Save for return
    test    ecx, ecx
    jz      @@once_none
    
    push    rax
    call    applySoftThrottle
    pop     rax
    
    pop     rbx
    ret
    
@@once_none:
    xor     eax, eax
    pop     rbx
    ret
    
@@once_emergency:
    mov     eax, -1                 ; Emergency indicator
    pop     rbx
    ret
    
@@once_invalid:
    mov     eax, -2                 ; Invalid indicator
    pop     rbx
    ret
dispatchOnce ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Get Current Throttle Level
; ═══════════════════════════════════════════════════════════════════════════════
; int getThrottleLevel(void* controlBlock)
; RCX = control block pointer
; Returns: Current throttle percentage (0-100)
; ═══════════════════════════════════════════════════════════════════════════════

getThrottleLevel PROC
    test    rcx, rcx
    jz      @@level_zero
    
    mov     eax, [rcx + SCB_THROTTLE_PERCENT]
    
    ; Clamp to 0-100
    test    eax, eax
    jns     @@level_check_max
    xor     eax, eax
    ret
    
@@level_check_max:
    cmp     eax, 100
    jle     @@level_done
    mov     eax, 100
    
@@level_done:
    ret
    
@@level_zero:
    xor     eax, eax
    ret
getThrottleLevel ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Set Throttle Level (for external control)
; ═══════════════════════════════════════════════════════════════════════════════
; void setThrottleLevel(void* controlBlock, int percent)
; RCX = control block pointer
; EDX = throttle percentage (0-100)
; ═══════════════════════════════════════════════════════════════════════════════

setThrottleLevel PROC
    test    rcx, rcx
    jz      @@set_done
    
    ; Clamp to 0-100
    test    edx, edx
    jns     @@set_check_max
    xor     edx, edx
    jmp     @@set_store
    
@@set_check_max:
    cmp     edx, 100
    jle     @@set_store
    mov     edx, 100
    
@@set_store:
    ; Store throttle percent
    mov     [rcx + SCB_THROTTLE_PERCENT], edx
    
    ; Calculate pause loop count (percent * 10)
    mov     eax, edx
    imul    eax, 10
    mov     [rcx + SCB_PAUSE_LOOP_COUNT], eax
    
    ; Set soft throttle flag if non-zero
    test    edx, edx
    jz      @@set_clear_flag
    
    or      dword ptr [rcx + SCB_THROTTLE_FLAGS], FLAG_SOFT_THROTTLE
    jmp     @@set_done
    
@@set_clear_flag:
    and     dword ptr [rcx + SCB_THROTTLE_FLAGS], NOT FLAG_SOFT_THROTTLE
    
@@set_done:
    ret
setThrottleLevel ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Trigger Emergency Stop
; ═══════════════════════════════════════════════════════════════════════════════
; void triggerEmergencyStop(void* controlBlock)
; RCX = control block pointer
; ═══════════════════════════════════════════════════════════════════════════════

triggerEmergencyStop PROC
    test    rcx, rcx
    jz      @@emer_done
    
    or      dword ptr [rcx + SCB_THROTTLE_FLAGS], FLAG_EMERGENCY_STOP
    mov     dword ptr [rcx + SCB_THROTTLE_PERCENT], 100
    mov     dword ptr [rcx + SCB_PAUSE_LOOP_COUNT], 1000
    
@@emer_done:
    ret
triggerEmergencyStop ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Clear Emergency Stop
; ═══════════════════════════════════════════════════════════════════════════════
; void clearEmergencyStop(void* controlBlock)
; RCX = control block pointer
; ═══════════════════════════════════════════════════════════════════════════════

clearEmergencyStop PROC
    test    rcx, rcx
    jz      @@clear_done
    
    and     dword ptr [rcx + SCB_THROTTLE_FLAGS], NOT FLAG_EMERGENCY_STOP
    
@@clear_done:
    ret
clearEmergencyStop ENDP

END
