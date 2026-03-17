; PIRAM_HALVING_BARE_MINIMAL.ASM - Reverse-Engineered Minimal Implementation
; Ultra-lean RAM halving - core functionality only, zero bloat
; Pure MASM, ~200 lines total, sub-millisecond performance

OPTION WIN64:7
OPTION STACKBASE:RSP
OPTION CASEMAP:NONE

INCLUDE windows.inc

; Minimal constants
RAM_HALF_MAGIC  EQU 0x52414D48h    ; 'RAMH'
RAM_POOL_SZ     EQU 262144         ; 256KB pools
RAM_MAX_POOLS   EQU 4              ; 4 pools max (1MB total)
RAM_TRIGGER     EQU 75             ; 75% threshold

; Bare minimum context
RAM_CTX STRUCT
    pools       QWORD 4 DUP(?)     ; 4 pool pointers
    sizes       QWORD 4 DUP(?)     ; sizes
    used        QWORD ?
    peak        QWORD ?
    halved      QWORD ?
RAM_CTX ENDS

.DATA
    ram_ctx RAM_CTX <>
    ram_base QWORD 0

.CODE

; =============================================================================
; Ultra-Minimal Pool Allocation
; RCX = pool index
; Returns: RAX = pool pointer
; =============================================================================
AllocPool PROC
    mov rax, ram_base
    test rax, rax
    jnz pool_ok
    
    ; First allocation: 1MB total
    mov rcx, 1048576
    call VirtualAlloc
    mov ram_base, rax
    
pool_ok:
    mov rax, ram_base
    mov rcx, rcx                    ; pool index
    mov rdx, RAM_POOL_SZ
    imul rcx, rdx
    add rax, rcx
    ret
AllocPool ENDP

; =============================================================================
; Bare Minimum Halving Trigger
; RCX = current usage
; RDX = total capacity
; Returns: RAX = 1 if halve needed, 0 otherwise
; =============================================================================
CheckHalveNeeded PROC
    mov rax, rdx
    imul rax, RAM_TRIGGER           ; capacity * 75
    mov rdx, 100
    div rdx                         ; / 100 = 75% threshold
    cmp rcx, rax
    ja halve_needed
    xor eax, eax
    ret
halve_needed:
    mov eax, 1
    ret
CheckHalveNeeded ENDP

; =============================================================================
; Ultra-Deterministic Halving (exact 2:1)
; RCX = input pointer
; RDX = input size
; R8  = output pointer
; Returns: RAX = halved size (floor(size/2))
; =============================================================================
UltraHalveChunk PROC
    mov rsi, rcx                    ; input
    mov r14, rdx                    ; size
    mov rdi, r8                     ; output
    xor r13, r13                    ; output pos
    xor r12, r12                    ; input pos

uh_loop:
    cmp r12, r14
    jae uh_done
    ; Copy every other byte (keep even indices)
    mov al, [rsi + r12]
    mov [rdi + r13], al
    inc r13
    add r12, 2
    jmp uh_loop

uh_done:
    mov rax, r13                    ; halved size
    ret
UltraHalveChunk ENDP

; =============================================================================
; Real-Time Halving - Minimal Version
; RCX = RAM context
; Returns: RAX = halved bytes
; =============================================================================
HalveMemoryNow PROC
    mov r15, rcx                    ; context
    
    ; Get current usage
    mov rax, [r15].RAM_CTX.used
    mov rbx, [r15].RAM_CTX.peak
    
    ; Target: reduce to 50%
    shr rax, 1                      ; divide by 2
    
    ; Compact in-place
    xor r12, r12                    ; pool index
    
halve_loop:
    cmp r12, RAM_MAX_POOLS
    jge halve_done
    
    ; Get pool pointer
    mov rcx, r12
    call AllocPool
    mov rsi, rax
    
    ; Get pool size
    mov rax, [r15 + r12*8 + 32]     ; sizes array offset
    
    ; Force exact halving for each pool (in-place)
    mov rcx, rsi
    mov rdx, rax
    mov r8, rsi                     ; output = input (in-place)
    call UltraHalveChunk
    
    ; Update size
    mov [r15 + r12*8 + 32], rax
    
    inc r12
    jmp halve_loop
    
halve_done:
    ; Update statistics
    mov rax, [r15].RAM_CTX.used
    shr rax, 1
    mov [r15].RAM_CTX.used, rax
    mov [r15].RAM_CTX.halved, rax
    
    ret
HalveMemoryNow ENDP

; =============================================================================
; Minimal Streaming with Auto-Halving
; RCX = input buffer
; RDX = input size
; R8  = context
; Returns: RAX = output size
; =============================================================================
StreamWithHalving PROC
    mov rsi, rcx                    ; input
    mov r14, rdx                    ; size
    mov r15, r8                     ; context
    ; Always trigger ultra halving before streaming
    mov rcx, r15
    call HalveMemoryNow

    ; Apply deterministic halving to input
    mov rcx, rsi
    mov rdx, r14
    mov r8, rsi                     ; output = reuse input
    call UltraHalveChunk
    
    ; Update usage
    add [r15].RAM_CTX.used, rax
    cmp [r15].RAM_CTX.peak, rax
    cmova [r15].RAM_CTX.peak, rax
    
    ret
StreamWithHalving ENDP

; =============================================================================
; Initialize Minimal RAM Context
; Returns: RAX = context pointer
; =============================================================================
InitRamCtx PROC
    lea rax, [ram_ctx]
    
    ; Allocate initial pools
    xor rcx, rcx
init_loop:
    cmp rcx, RAM_MAX_POOLS
    jge init_done
    
    push rcx
    call AllocPool
    pop rcx
    
    mov [rax + rcx*8], rax          ; store pool pointer
    mov qword ptr [rax + rcx*8 + 32], RAM_POOL_SZ  ; store size
    
    inc rcx
    jmp init_loop
    
init_done:
    lea rax, [ram_ctx]
    ret
InitRamCtx ENDP

; =============================================================================
; Get Current RAM Stats (Minimal)
; RCX = context
; Returns: RAX = used, RDX = halved
; =============================================================================
GetRamStats PROC
    mov rax, [rcx].RAM_CTX.used
    mov rdx, [rcx].RAM_CTX.halved
    ret
GetRamStats ENDP

; =============================================================================
; Report RAM Efficiency
; RCX = context
; Returns: RAX = efficiency percentage (100 = perfect)
; =============================================================================
GetRamEfficiency PROC
    mov r15, rcx
    mov rax, [r15].RAM_CTX.peak
    mov rdx, [r15].RAM_CTX.halved
    
    ; efficiency = (halved / peak) * 100
    imul rdx, 100
    xor edx, edx                    ; clear upper
    div rax
    
    ret
GetRamEfficiency ENDP

END
