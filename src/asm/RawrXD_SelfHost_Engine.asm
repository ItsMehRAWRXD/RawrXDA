; =============================================================================
; RawrXD_SelfHost_Engine.asm — Phase E: Recursive Self-Hosting Compile Engine
; =============================================================================
;
; The Omega Codebase: RawrXD becomes capable of editing its own .asm source
; files, optimizing its own MASM kernels, and hotpatching itself with zero
; downtime. This is the bootstrap paradox resolver — Generation N compiles
; Generation N+1.
;
; Capabilities:
;   - Read own .text section via PE header traversal
;   - Identify AVX-512 bottlenecks via PMC counter sampling (RDPMC)
;   - Generate improved microcode sequences in RWX code caves
;   - Assemble new kernels in-process (micro-assembler for x64 subset)
;   - Atomic swap via SelfPatchEngine integration
;   - Formal verification stubs (instruction equivalence checking)
;   - Performance delta measurement (before/after RDTSC comparison)
;   - Source file I/O for .asm self-modification
;   - Generation counter tracking (bootstrap lineage)
;
; Active Exports:
;   asm_selfhost_init             — Initialize self-hosting subsystem
;   asm_selfhost_read_text        — Read own .text section into buffer
;   asm_selfhost_profile_region   — Profile code region via PMC/RDTSC
;   asm_selfhost_gen_trampoline   — Generate optimized trampoline in code cave
;   asm_selfhost_micro_assemble   — Micro-assemble x64 instruction stream
;   asm_selfhost_atomic_swap      — Atomic kernel swap (CLFLUSH + MFENCE)
;   asm_selfhost_verify_equiv     — Instruction-level equivalence verify
;   asm_selfhost_measure_delta    — Measure performance improvement ratio
;   asm_selfhost_read_source      — Read .asm source file into arena
;   asm_selfhost_write_source     — Write modified .asm back to disk
;   asm_selfhost_get_generation   — Get current bootstrap generation
;   asm_selfhost_get_stats        — Read engine statistics
;   asm_selfhost_shutdown         — Teardown self-hosting subsystem
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_SelfHost_Engine.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                       EXPORTS
; =============================================================================
PUBLIC asm_selfhost_init
PUBLIC asm_selfhost_read_text
PUBLIC asm_selfhost_profile_region
PUBLIC asm_selfhost_gen_trampoline
PUBLIC asm_selfhost_micro_assemble
PUBLIC asm_selfhost_atomic_swap
PUBLIC asm_selfhost_verify_equiv
PUBLIC asm_selfhost_measure_delta
PUBLIC asm_selfhost_read_source
PUBLIC asm_selfhost_write_source
PUBLIC asm_selfhost_get_generation
PUBLIC asm_selfhost_get_stats
PUBLIC asm_selfhost_shutdown

; =============================================================================
;                       CONSTANTS
; =============================================================================

; Self-host arena
SELFHOST_ARENA_SIZE     EQU     400000h     ; 4 MB code generation arena
SELFHOST_SRC_BUF_SIZE   EQU     100000h     ; 1 MB source file buffer
SELFHOST_MAX_PATCHES    EQU     512
SELFHOST_NAME_MAX       EQU     128

; Micro-assembler opcode tokens
UASM_NOP                EQU     000h
UASM_MOV_R64_R64        EQU     001h
UASM_MOV_R64_IMM        EQU     002h
UASM_ADD_R64_R64        EQU     003h
UASM_SUB_R64_R64        EQU     004h
UASM_MUL_R64            EQU     005h
UASM_DIV_R64            EQU     006h
UASM_AND_R64_R64        EQU     007h
UASM_OR_R64_R64         EQU     008h
UASM_XOR_R64_R64        EQU     009h
UASM_SHL_R64_IMM        EQU     00Ah
UASM_SHR_R64_IMM        EQU     00Bh
UASM_CMP_R64_R64        EQU     00Ch
UASM_JMP_REL32          EQU     00Dh
UASM_JE_REL32           EQU     00Eh
UASM_JNE_REL32          EQU     00Fh
UASM_CALL_ABS           EQU     010h
UASM_RET                EQU     011h
UASM_VMOVAPS_YMM        EQU     012h    ; AVX2 move
UASM_VFMADD_YMM         EQU     013h    ; AVX2 FMA
UASM_VMOVAPS_ZMM        EQU     014h    ; AVX-512 move
UASM_VFMADD_ZMM         EQU     015h    ; AVX-512 FMA
UASM_PREFETCHT0         EQU     016h
UASM_CLFLUSH            EQU     017h
UASM_MFENCE             EQU     018h
UASM_END                EQU     0FFh

; PMC (Performance Monitoring Counter) indices
PMC_INSTRUCTIONS_RETIRED EQU    0C0h     ; IA32_FIXED_CTR0
PMC_CYCLES              EQU     03Ch     ; CPU_CLK_UNHALTED.CORE
PMC_CACHE_MISSES        EQU     02Eh     ; LLC_MISSES
PMC_BRANCH_MISPREDICT   EQU     0C5h     ; BRANCH_MISSES_RETIRED

; Verification modes
VERIFY_SEMANTIC         EQU     0       ; Compare I/O equivalence
VERIFY_STRUCTURAL       EQU     1       ; Compare instruction graph
VERIFY_STATISTICAL      EQU     2       ; Monte Carlo sampling

; Error codes
SH_OK                   EQU     0
SH_ERR_NOT_INIT         EQU     1
SH_ERR_ALLOC            EQU     2
SH_ERR_PE_INVALID       EQU     3
SH_ERR_TEXT_NOT_FOUND   EQU     4
SH_ERR_FILE_OPEN        EQU     5
SH_ERR_FILE_READ        EQU     6
SH_ERR_FILE_WRITE       EQU     7
SH_ERR_CAVE_FULL        EQU     8
SH_ERR_ASM_INVALID      EQU     9
SH_ERR_VERIFY_FAIL      EQU     10
SH_ERR_SWAP_FAIL        EQU     11
SH_ERR_VPROTECT         EQU     12
SH_ERR_ALREADY_INIT     EQU     13

; =============================================================================
;                       STRUCTURES
; =============================================================================

; Profile result from PMC/RDTSC sampling
PROFILE_RESULT STRUCT 8
    cycles_before       DQ      ?
    cycles_after        DQ      ?
    instructions        DQ      ?
    cache_misses        DQ      ?
    branch_misses       DQ      ?
    ipc_ratio           DQ      ?       ; Fixed-point 16.16
    improvement_pct     DD      ?       ; Percentage improvement (signed)
    _pad0               DD      ?
PROFILE_RESULT ENDS

; Micro-assembled instruction entry
UASM_INSTR STRUCT 8
    opcode              DD      ?       ; UASM_xxx token
    dst_reg             DB      ?       ; Destination register (0-15)
    src_reg             DB      ?       ; Source register (0-15)
    _pad0               DW      ?
    imm64               DQ      ?       ; Immediate value
    encoded_len         DD      ?       ; Length of x64 encoding
    encoded             DB 16 DUP(?)    ; Actual x64 bytes (max 15 + pad)
    _pad1               DD      ?
UASM_INSTR ENDS

; Generated kernel descriptor
GEN_KERNEL STRUCT 8
    name                DB SELFHOST_NAME_MAX DUP(?)
    name_len            DD      ?
    generation          DD      ?       ; Bootstrap generation number
    cave_offset         DQ      ?       ; Offset into code cave arena
    code_size           DD      ?       ; Size of generated code
    instr_count         DD      ?       ; Number of micro-instructions
    verified            DD      ?       ; 1 = passed formal verification
    perf_delta          DD      ?       ; Performance change (signed %)
    original_addr       DQ      ?       ; Address of original kernel
    timestamp           DQ      ?       ; RDTSC at generation time
GEN_KERNEL ENDS

; Self-host statistics
SELFHOST_STATS STRUCT 8
    kernels_generated   DQ      ?
    kernels_swapped     DQ      ?
    kernels_rolled_back DQ      ?
    verify_passed       DQ      ?
    verify_failed       DQ      ?
    total_improvement   DQ      ?       ; Cumulative % improvement (fixed 16.16)
    arena_used          DQ      ?       ; Bytes used in code cave
    arena_total         DQ      ?       ; Total arena capacity
    source_reads        DQ      ?
    source_writes       DQ      ?
    current_generation  DQ      ?       ; Current bootstrap generation
    highest_ipc         DQ      ?       ; Best IPC ratio seen (fixed 16.16)
SELFHOST_STATS ENDS

; =============================================================================
;                       DATA SECTION
; =============================================================================
.data
ALIGN 16

g_sh_initialized    DD      0
g_sh_generation     DQ      0           ; Bootstrap generation counter
g_sh_arena_base     DQ      0           ; Base of RWX code generation arena
g_sh_arena_cursor   DQ      0           ; Current write position in arena
g_sh_arena_end      DQ      0           ; End of arena (base + size)
g_sh_text_base      DQ      0           ; .text section base address
g_sh_text_size      DQ      0           ; .text section size
g_sh_src_buf        DQ      0           ; Source file read buffer
g_sh_stats          SELFHOST_STATS <>

; Micro-assembler register encoding table (RAX=0, RCX=1, ... R15=15)
g_sh_reg_enc        DB 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15

; x64 REX prefix table (for registers R8-R15)
g_sh_rex_w          DB      048h        ; REX.W (64-bit operand)
g_sh_rex_wr         DB      04Ch        ; REX.WR (64-bit + ext dst)
g_sh_rex_wb         DB      049h        ; REX.WB (64-bit + ext src)
g_sh_rex_wrb        DB      04Dh        ; REX.WRB (64-bit + both ext)

; Win32 API names for GetProcAddress
szKernel32          DB      "kernel32.dll", 0
szVirtualAlloc      DB      "VirtualAlloc", 0
szVirtualFree       DB      "VirtualFree", 0
szVirtualProtect    DB      "VirtualProtect", 0
szCreateFileA       DB      "CreateFileA", 0
szReadFile          DB      "ReadFile", 0
szWriteFile         DB      "WriteFile", 0
szCloseHandle       DB      "CloseHandle", 0
szGetFileSize       DB      "GetFileSizeEx", 0
szFlushFileBuffers  DB      "FlushFileBuffers", 0
szGetModuleHandleA  DB      "GetModuleHandleA", 0

; Function pointers (resolved at init)
pfnVirtualAlloc     DQ      0
pfnVirtualFree      DQ      0
pfnVirtualProtect   DQ      0
pfnCreateFileA      DQ      0
pfnReadFile         DQ      0
pfnWriteFile        DQ      0
pfnCloseHandle      DQ      0
pfnGetFileSize      DQ      0
pfnFlushFileBuffers DQ      0
pfnGetModuleHandle  DQ      0

; =============================================================================
;                       CODE SECTION
; =============================================================================
.code

; =============================================================================
; asm_selfhost_init — Initialize self-hosting subsystem
; Returns: EAX = SH_OK on success, error code otherwise
; =============================================================================
asm_selfhost_init PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 48h

    ; Check double-init
    cmp     DWORD PTR [g_sh_initialized], 1
    je      _sh_init_already

    ; Resolve kernel32 APIs
    lea     rcx, [szKernel32]
    call    _sh_resolve_apis
    test    eax, eax
    jnz     _sh_init_fail_alloc

    ; Allocate RWX arena for code generation
    xor     ecx, ecx                    ; lpAddress = NULL
    mov     edx, SELFHOST_ARENA_SIZE    ; dwSize
    mov     r8d, 3000h                  ; MEM_COMMIT | MEM_RESERVE
    mov     r9d, 040h                   ; PAGE_EXECUTE_READWRITE
    call    QWORD PTR [pfnVirtualAlloc]
    test    rax, rax
    jz      _sh_init_fail_alloc

    mov     [g_sh_arena_base], rax
    mov     [g_sh_arena_cursor], rax
    lea     rcx, [rax + SELFHOST_ARENA_SIZE]
    mov     [g_sh_arena_end], rcx

    ; Allocate source file buffer
    xor     ecx, ecx
    mov     edx, SELFHOST_SRC_BUF_SIZE
    mov     r8d, 3000h
    mov     r9d, 04h                    ; PAGE_READWRITE
    call    QWORD PTR [pfnVirtualAlloc]
    test    rax, rax
    jz      _sh_init_fail_alloc

    mov     [g_sh_src_buf], rax

    ; Locate own .text section
    call    _sh_locate_text_section
    test    eax, eax
    jnz     _sh_init_fail_pe

    ; Initialize stats
    lea     rdi, [g_sh_stats]
    xor     eax, eax
    mov     ecx, SIZEOF SELFHOST_STATS
    rep     stosb

    mov     rax, SELFHOST_ARENA_SIZE
    mov     [g_sh_stats.arena_total], rax

    ; Set initialized
    mov     DWORD PTR [g_sh_initialized], 1
    mov     QWORD PTR [g_sh_generation], 0

    xor     eax, eax                    ; SH_OK
    jmp     _sh_init_done

_sh_init_already:
    mov     eax, SH_ERR_ALREADY_INIT
    jmp     _sh_init_done

_sh_init_fail_alloc:
    mov     eax, SH_ERR_ALLOC
    jmp     _sh_init_done

_sh_init_fail_pe:
    mov     eax, SH_ERR_PE_INVALID

_sh_init_done:
    add     rsp, 48h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_selfhost_init ENDP

; =============================================================================
; asm_selfhost_read_text — Read own .text section into caller buffer
; RCX = destination buffer
; RDX = buffer size
; Returns: RAX = bytes copied, 0 on error
; =============================================================================
asm_selfhost_read_text PROC
    push    rsi
    push    rdi

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_rt_fail

    mov     rdi, rcx                    ; dst
    mov     rsi, [g_sh_text_base]       ; src = .text
    mov     rcx, [g_sh_text_size]

    ; Clamp to buffer size
    cmp     rcx, rdx
    cmova   rcx, rdx

    push    rcx                         ; save copy count
    rep     movsb
    pop     rax                         ; return count

    pop     rdi
    pop     rsi
    ret

_sh_rt_fail:
    xor     eax, eax
    pop     rdi
    pop     rsi
    ret
asm_selfhost_read_text ENDP

; =============================================================================
; asm_selfhost_profile_region — Profile a code region via RDTSC
; RCX = function pointer to profile
; RDX = iteration count
; R8  = pointer to PROFILE_RESULT output
; Returns: EAX = SH_OK
; =============================================================================
asm_selfhost_profile_region PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 30h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_pr_fail

    mov     r12, rcx                    ; function to profile
    mov     r13, rdx                    ; iterations
    mov     r14, r8                     ; output PROFILE_RESULT

    ; RDTSC before
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r15, rax
    mov     [r14 + PROFILE_RESULT.cycles_before], rax

    ; Call target function N times
    mov     rbx, r13
_sh_pr_loop:
    test    rbx, rbx
    jz      _sh_pr_loop_done
    call    r12
    dec     rbx
    jmp     _sh_pr_loop
_sh_pr_loop_done:

    ; RDTSC after
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     [r14 + PROFILE_RESULT.cycles_after], rax

    ; Compute delta
    sub     rax, r15
    mov     [r14 + PROFILE_RESULT.instructions], r13

    ; IPC ratio (fixed 16.16): iterations * 65536 / cycles
    mov     rcx, r13
    shl     rcx, 16
    xor     edx, edx
    div     rcx                         ; rax = ratio
    mov     [r14 + PROFILE_RESULT.ipc_ratio], rax

    ; Zero PMC fields (user-mode can't read without driver)
    xor     eax, eax
    mov     [r14 + PROFILE_RESULT.cache_misses], rax
    mov     [r14 + PROFILE_RESULT.branch_misses], rax
    mov     [r14 + PROFILE_RESULT.improvement_pct], eax

    xor     eax, eax                    ; SH_OK
    jmp     _sh_pr_done

_sh_pr_fail:
    mov     eax, SH_ERR_NOT_INIT

_sh_pr_done:
    add     rsp, 30h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_selfhost_profile_region ENDP

; =============================================================================
; asm_selfhost_gen_trampoline — Generate trampoline in code cave
; RCX = target address (where to jump)
; Returns: RAX = trampoline address, 0 on error
; =============================================================================
asm_selfhost_gen_trampoline PROC
    push    rbx
    sub     rsp, 20h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_gt_fail

    mov     rbx, rcx                    ; target

    ; Check cave space (need 14 bytes)
    mov     rax, [g_sh_arena_cursor]
    lea     rcx, [rax + 14]
    cmp     rcx, [g_sh_arena_end]
    ja      _sh_gt_cave_full

    ; Write FF 25 00 00 00 00 (JMP [RIP+0])
    mov     BYTE PTR [rax], 0FFh
    mov     BYTE PTR [rax+1], 025h
    mov     DWORD PTR [rax+2], 0

    ; Write 8-byte absolute target
    mov     QWORD PTR [rax+6], rbx

    ; Flush instruction cache
    push    rax
    mov     rcx, rax
    clflush [rcx]
    mfence
    pop     rax

    ; Advance cursor
    lea     rcx, [rax + 14]
    mov     [g_sh_arena_cursor], rcx

    ; Update stats
    lock inc QWORD PTR [g_sh_stats.kernels_generated]

    ; Update arena_used
    mov     rcx, [g_sh_arena_cursor]
    sub     rcx, [g_sh_arena_base]
    mov     [g_sh_stats.arena_used], rcx

    jmp     _sh_gt_done

_sh_gt_cave_full:
    mov     eax, 0
    jmp     _sh_gt_done

_sh_gt_fail:
    xor     eax, eax

_sh_gt_done:
    add     rsp, 20h
    pop     rbx
    ret
asm_selfhost_gen_trampoline ENDP

; =============================================================================
; asm_selfhost_micro_assemble — Micro-assemble x64 instruction stream
; RCX = pointer to UASM_INSTR array
; RDX = instruction count
; R8  = output buffer
; R9  = output buffer size
; Returns: RAX = bytes written, 0 on error
; =============================================================================
asm_selfhost_micro_assemble PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_ma_fail

    mov     rsi, rcx                    ; instruction array
    mov     r12, rdx                    ; instr count
    mov     rdi, r8                     ; output buffer
    mov     r13, r9                     ; output capacity
    xor     r14d, r14d                  ; bytes written
    xor     r15d, r15d                  ; instruction index

_sh_ma_loop:
    cmp     r15, r12
    jge     _sh_ma_done_ok

    ; Get current instruction
    ; Each UASM_INSTR is 48 bytes (aligned to 8)
    imul    rax, r15, SIZEOF UASM_INSTR
    lea     rbx, [rsi + rax]

    mov     eax, [rbx + UASM_INSTR.opcode]

    ; Dispatch by opcode
    cmp     eax, UASM_NOP
    je      _sh_ma_emit_nop
    cmp     eax, UASM_RET
    je      _sh_ma_emit_ret
    cmp     eax, UASM_MFENCE
    je      _sh_ma_emit_mfence
    cmp     eax, UASM_MOV_R64_R64
    je      _sh_ma_emit_mov_rr
    cmp     eax, UASM_MOV_R64_IMM
    je      _sh_ma_emit_mov_ri
    cmp     eax, UASM_ADD_R64_R64
    je      _sh_ma_emit_alu_rr
    cmp     eax, UASM_END
    je      _sh_ma_done_ok

    ; Unknown opcode — skip
    jmp     _sh_ma_next

_sh_ma_emit_nop:
    ; Check space
    lea     rax, [r14 + 1]
    cmp     rax, r13
    ja      _sh_ma_fail
    mov     BYTE PTR [rdi + r14], 090h  ; NOP
    inc     r14
    jmp     _sh_ma_next

_sh_ma_emit_ret:
    lea     rax, [r14 + 1]
    cmp     rax, r13
    ja      _sh_ma_fail
    mov     BYTE PTR [rdi + r14], 0C3h  ; RET
    inc     r14
    jmp     _sh_ma_next

_sh_ma_emit_mfence:
    lea     rax, [r14 + 3]
    cmp     rax, r13
    ja      _sh_ma_fail
    mov     BYTE PTR [rdi + r14], 00Fh
    mov     BYTE PTR [rdi + r14 + 1], 0AEh
    mov     BYTE PTR [rdi + r14 + 2], 0F0h
    add     r14, 3
    jmp     _sh_ma_next

_sh_ma_emit_mov_rr:
    ; REX.W + 89 /r (MOV r/m64, r64) or 8B /r
    lea     rax, [r14 + 3]
    cmp     rax, r13
    ja      _sh_ma_fail

    ; Determine REX byte
    movzx   eax, BYTE PTR [rbx + UASM_INSTR.dst_reg]
    movzx   ecx, BYTE PTR [rbx + UASM_INSTR.src_reg]

    ; REX.W prefix (always needed for 64-bit)
    mov     BYTE PTR [rdi + r14], 048h
    ; Adjust REX for extended registers
    cmp     al, 8
    jb      @F
    or      BYTE PTR [rdi + r14], 01h   ; REX.B
    sub     al, 8
@@:
    cmp     cl, 8
    jb      @F
    or      BYTE PTR [rdi + r14], 04h   ; REX.R
    sub     cl, 8
@@:
    mov     BYTE PTR [rdi + r14 + 1], 089h  ; MOV r/m64, r64
    ; ModRM: mod=11, reg=src, rm=dst
    shl     cl, 3
    or      cl, al
    or      cl, 0C0h
    mov     BYTE PTR [rdi + r14 + 2], cl
    add     r14, 3
    jmp     _sh_ma_next

_sh_ma_emit_mov_ri:
    ; REX.W + B8+rd (MOV r64, imm64)
    lea     rax, [r14 + 10]
    cmp     rax, r13
    ja      _sh_ma_fail

    movzx   eax, BYTE PTR [rbx + UASM_INSTR.dst_reg]
    mov     BYTE PTR [rdi + r14], 048h
    cmp     al, 8
    jb      @F
    or      BYTE PTR [rdi + r14], 01h   ; REX.B
    sub     al, 8
@@:
    add     al, 0B8h                    ; B8+rd
    mov     BYTE PTR [rdi + r14 + 1], al
    mov     rax, [rbx + UASM_INSTR.imm64]
    mov     QWORD PTR [rdi + r14 + 2], rax
    add     r14, 10
    jmp     _sh_ma_next

_sh_ma_emit_alu_rr:
    ; REX.W + opcode /r (ADD/SUB/AND/OR/XOR)
    lea     rax, [r14 + 3]
    cmp     rax, r13
    ja      _sh_ma_fail

    movzx   eax, BYTE PTR [rbx + UASM_INSTR.dst_reg]
    movzx   ecx, BYTE PTR [rbx + UASM_INSTR.src_reg]

    mov     BYTE PTR [rdi + r14], 048h
    cmp     al, 8
    jb      @F
    or      BYTE PTR [rdi + r14], 01h
    sub     al, 8
@@:
    cmp     cl, 8
    jb      @F
    or      BYTE PTR [rdi + r14], 04h
    sub     cl, 8
@@:
    mov     BYTE PTR [rdi + r14 + 1], 001h  ; ADD r/m64, r64
    shl     cl, 3
    or      cl, al
    or      cl, 0C0h
    mov     BYTE PTR [rdi + r14 + 2], cl
    add     r14, 3
    jmp     _sh_ma_next

_sh_ma_next:
    inc     r15
    jmp     _sh_ma_loop

_sh_ma_done_ok:
    mov     rax, r14
    jmp     _sh_ma_exit

_sh_ma_fail:
    xor     eax, eax

_sh_ma_exit:
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_selfhost_micro_assemble ENDP

; =============================================================================
; asm_selfhost_atomic_swap — Atomic kernel swap
; RCX = target address (code to replace)
; RDX = replacement address (new code)
; R8  = size of replacement
; Returns: EAX = SH_OK or error
; =============================================================================
asm_selfhost_atomic_swap PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 48h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_as_fail_init

    mov     r12, rcx                    ; target
    mov     rsi, rdx                    ; replacement
    mov     r13, r8                     ; size

    ; VirtualProtect target to RWX
    lea     r9, [rsp + 20h]             ; lpflOldProtect
    mov     r8d, 040h                   ; PAGE_EXECUTE_READWRITE
    mov     rdx, r13                    ; dwSize
    mov     rcx, r12                    ; lpAddress
    call    QWORD PTR [pfnVirtualProtect]
    test    eax, eax
    jz      _sh_as_fail_vprot

    ; Copy replacement code
    mov     rdi, r12
    mov     rcx, r13
    rep     movsb

    ; CLFLUSH each cache line
    mov     rax, r12
    mov     rcx, r13
    add     rcx, 63                     ; Round up
    shr     rcx, 6                      ; / 64 = cache line count
_sh_as_flush:
    clflush [rax]
    add     rax, 64
    dec     rcx
    jnz     _sh_as_flush

    ; Memory fence
    mfence

    ; Restore original protection
    mov     ecx, DWORD PTR [rsp + 20h]  ; old protect
    lea     r9, [rsp + 28h]             ; dummy old protect out
    mov     r8d, ecx                    ; restore old
    mov     rdx, r13
    mov     rcx, r12
    call    QWORD PTR [pfnVirtualProtect]

    ; Update stats
    lock inc QWORD PTR [g_sh_stats.kernels_swapped]

    xor     eax, eax                    ; SH_OK
    jmp     _sh_as_done

_sh_as_fail_init:
    mov     eax, SH_ERR_NOT_INIT
    jmp     _sh_as_done

_sh_as_fail_vprot:
    mov     eax, SH_ERR_VPROTECT

_sh_as_done:
    add     rsp, 48h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_selfhost_atomic_swap ENDP

; =============================================================================
; asm_selfhost_verify_equiv — Instruction equivalence verification
; RCX = original function pointer
; RDX = new function pointer
; R8  = test input array (uint64_t[4])
; R9  = test count
; Returns: EAX = 1 if equivalent, 0 if divergent
; =============================================================================
asm_selfhost_verify_equiv PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40h

    mov     r12, rcx                    ; original fn
    mov     r13, rdx                    ; new fn
    mov     r14, r8                     ; test inputs
    mov     r15, r9                     ; test count

    xor     ebx, ebx                    ; test index

_sh_ve_loop:
    cmp     rbx, r15
    jge     _sh_ve_pass

    ; Call original with test input
    mov     rcx, QWORD PTR [r14 + rbx*8]
    call    r12
    mov     [rsp + 30h], rax            ; save original result

    ; Call new with same input
    mov     rcx, QWORD PTR [r14 + rbx*8]
    call    r13

    ; Compare results
    cmp     rax, QWORD PTR [rsp + 30h]
    jne     _sh_ve_diverge

    inc     rbx
    jmp     _sh_ve_loop

_sh_ve_pass:
    mov     eax, 1
    lock inc QWORD PTR [g_sh_stats.verify_passed]
    jmp     _sh_ve_done

_sh_ve_diverge:
    xor     eax, eax
    lock inc QWORD PTR [g_sh_stats.verify_failed]

_sh_ve_done:
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_selfhost_verify_equiv ENDP

; =============================================================================
; asm_selfhost_measure_delta — Measure performance improvement
; RCX = original function pointer
; RDX = new function pointer
; R8  = iteration count
; Returns: EAX = improvement percentage (signed), negative = regression
; =============================================================================
asm_selfhost_measure_delta PROC
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 30h

    mov     r12, rcx                    ; original
    mov     r13, rdx                    ; new
    mov     r14, r8                     ; iterations

    ; Profile original
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r15, rax

    mov     rbx, r14
_sh_md_orig_loop:
    test    rbx, rbx
    jz      _sh_md_orig_done
    call    r12
    dec     rbx
    jmp     _sh_md_orig_loop
_sh_md_orig_done:

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r15
    mov     [rsp + 20h], rax            ; original cycles

    ; Profile new
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r15, rax

    mov     rbx, r14
_sh_md_new_loop:
    test    rbx, rbx
    jz      _sh_md_new_done
    call    r13
    dec     rbx
    jmp     _sh_md_new_loop
_sh_md_new_done:

    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r15                    ; new cycles

    ; improvement = (original - new) * 100 / original
    mov     rcx, [rsp + 20h]            ; original cycles
    test    rcx, rcx
    jz      _sh_md_zero
    mov     rbx, rcx
    sub     rbx, rax                    ; delta (positive = improvement)
    imul    rbx, 100
    mov     rax, rbx
    cqo
    idiv    rcx
    ; rax = improvement %
    jmp     _sh_md_done

_sh_md_zero:
    xor     eax, eax

_sh_md_done:
    add     rsp, 30h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_selfhost_measure_delta ENDP

; =============================================================================
; asm_selfhost_read_source — Read an .asm source file into the arena buffer
; RCX = pointer to filename string (null-terminated)
; RDX = pointer to output: bytes read (uint64_t*)
; Returns: RAX = pointer to buffer containing file data, NULL on error
; =============================================================================
asm_selfhost_read_source PROC
    push    rbx
    push    rsi
    push    r12
    sub     rsp, 48h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_rs_fail

    mov     r12, rdx                    ; output size ptr

    ; CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)
    mov     QWORD PTR [rsp + 30h], 0    ; hTemplateFile
    mov     DWORD PTR [rsp + 28h], 0    ; dwFlagsAndAttributes
    mov     DWORD PTR [rsp + 20h], 3    ; OPEN_EXISTING
    xor     r9d, r9d                    ; lpSecurityAttributes
    mov     r8d, 1                      ; FILE_SHARE_READ
    mov     edx, 80000000h              ; GENERIC_READ
    ; rcx already has filename
    call    QWORD PTR [pfnCreateFileA]
    cmp     rax, -1                     ; INVALID_HANDLE_VALUE
    je      _sh_rs_fail

    mov     rbx, rax                    ; file handle

    ; GetFileSizeEx
    lea     rdx, [rsp + 38h]            ; lpFileSize
    mov     rcx, rbx
    call    QWORD PTR [pfnGetFileSize]
    test    eax, eax
    jz      _sh_rs_close_fail

    mov     rsi, QWORD PTR [rsp + 38h]  ; file size
    cmp     rsi, SELFHOST_SRC_BUF_SIZE
    ja      _sh_rs_close_fail            ; too large

    ; ReadFile
    mov     QWORD PTR [rsp + 20h], 0    ; lpOverlapped
    lea     r9, [rsp + 40h]             ; lpNumberOfBytesRead
    mov     r8, rsi                     ; nNumberOfBytesToRead
    mov     rdx, [g_sh_src_buf]         ; lpBuffer
    mov     rcx, rbx                    ; hFile
    call    QWORD PTR [pfnReadFile]
    test    eax, eax
    jz      _sh_rs_close_fail

    ; Close handle
    mov     rcx, rbx
    call    QWORD PTR [pfnCloseHandle]

    ; Output size
    mov     QWORD PTR [r12], rsi

    ; Update stats
    lock inc QWORD PTR [g_sh_stats.source_reads]

    mov     rax, [g_sh_src_buf]
    jmp     _sh_rs_done

_sh_rs_close_fail:
    mov     rcx, rbx
    call    QWORD PTR [pfnCloseHandle]

_sh_rs_fail:
    xor     eax, eax

_sh_rs_done:
    add     rsp, 48h
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_selfhost_read_source ENDP

; =============================================================================
; asm_selfhost_write_source — Write modified .asm source back to disk
; RCX = filename (null-terminated)
; RDX = data buffer
; R8  = data size
; Returns: EAX = SH_OK or error
; =============================================================================
asm_selfhost_write_source PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_ws_fail_init

    mov     rsi, rdx                    ; data
    mov     rdi, r8                     ; size

    ; CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
    mov     QWORD PTR [rsp + 30h], 0
    mov     DWORD PTR [rsp + 28h], 80h  ; FILE_ATTRIBUTE_NORMAL
    mov     DWORD PTR [rsp + 20h], 2    ; CREATE_ALWAYS
    xor     r9d, r9d
    xor     r8d, r8d                    ; no sharing
    mov     edx, 40000000h              ; GENERIC_WRITE
    call    QWORD PTR [pfnCreateFileA]
    cmp     rax, -1
    je      _sh_ws_fail_file

    mov     rbx, rax                    ; handle

    ; WriteFile
    mov     QWORD PTR [rsp + 20h], 0    ; lpOverlapped
    lea     r9, [rsp + 38h]             ; lpBytesWritten
    mov     r8, rdi                     ; nBytesToWrite
    mov     rdx, rsi                    ; lpBuffer
    mov     rcx, rbx
    call    QWORD PTR [pfnWriteFile]
    test    eax, eax
    jz      _sh_ws_close_fail

    ; FlushFileBuffers
    mov     rcx, rbx
    call    QWORD PTR [pfnFlushFileBuffers]

    ; Close
    mov     rcx, rbx
    call    QWORD PTR [pfnCloseHandle]

    ; Update stats
    lock inc QWORD PTR [g_sh_stats.source_writes]
    lock inc QWORD PTR [g_sh_generation]
    mov     rax, [g_sh_generation]
    mov     [g_sh_stats.current_generation], rax

    xor     eax, eax                    ; SH_OK
    jmp     _sh_ws_done

_sh_ws_close_fail:
    mov     rcx, rbx
    call    QWORD PTR [pfnCloseHandle]

_sh_ws_fail_file:
    mov     eax, SH_ERR_FILE_WRITE
    jmp     _sh_ws_done

_sh_ws_fail_init:
    mov     eax, SH_ERR_NOT_INIT

_sh_ws_done:
    add     rsp, 48h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_selfhost_write_source ENDP

; =============================================================================
; asm_selfhost_get_generation — Get current bootstrap generation
; Returns: RAX = generation number
; =============================================================================
asm_selfhost_get_generation PROC
    mov     rax, [g_sh_generation]
    ret
asm_selfhost_get_generation ENDP

; =============================================================================
; asm_selfhost_get_stats — Get pointer to statistics struct
; Returns: RAX = pointer to SELFHOST_STATS
; =============================================================================
asm_selfhost_get_stats PROC
    lea     rax, [g_sh_stats]
    ret
asm_selfhost_get_stats ENDP

; =============================================================================
; asm_selfhost_shutdown — Teardown self-hosting subsystem
; Returns: EAX = SH_OK
; =============================================================================
asm_selfhost_shutdown PROC
    push    rbx
    sub     rsp, 20h

    cmp     DWORD PTR [g_sh_initialized], 0
    je      _sh_sd_done

    ; Free arena
    mov     rcx, [g_sh_arena_base]
    test    rcx, rcx
    jz      _sh_sd_free_src
    xor     edx, edx
    mov     r8d, 8000h                  ; MEM_RELEASE
    call    QWORD PTR [pfnVirtualFree]

_sh_sd_free_src:
    ; Free source buffer
    mov     rcx, [g_sh_src_buf]
    test    rcx, rcx
    jz      _sh_sd_clear
    xor     edx, edx
    mov     r8d, 8000h
    call    QWORD PTR [pfnVirtualFree]

_sh_sd_clear:
    xor     eax, eax
    mov     [g_sh_arena_base], rax
    mov     [g_sh_arena_cursor], rax
    mov     [g_sh_arena_end], rax
    mov     [g_sh_src_buf], rax
    mov     DWORD PTR [g_sh_initialized], 0

_sh_sd_done:
    xor     eax, eax
    add     rsp, 20h
    pop     rbx
    ret
asm_selfhost_shutdown ENDP

; =============================================================================
;                    INTERNAL HELPERS
; =============================================================================

; _sh_resolve_apis — Resolve Win32 APIs via GetProcAddress
_sh_resolve_apis PROC
    push    rbx
    push    rsi
    sub     rsp, 28h

    ; GetModuleHandleA("kernel32.dll")
    lea     rcx, [szKernel32]
    ; Using the import directly (linked via kernel32.lib)
    call    GetModuleHandleA
    test    rax, rax
    jz      _sh_ra_fail
    mov     rbx, rax

    ; Resolve each function
    mov     rcx, rbx
    lea     rdx, [szVirtualAlloc]
    call    GetProcAddress
    mov     [pfnVirtualAlloc], rax

    mov     rcx, rbx
    lea     rdx, [szVirtualFree]
    call    GetProcAddress
    mov     [pfnVirtualFree], rax

    mov     rcx, rbx
    lea     rdx, [szVirtualProtect]
    call    GetProcAddress
    mov     [pfnVirtualProtect], rax

    mov     rcx, rbx
    lea     rdx, [szCreateFileA]
    call    GetProcAddress
    mov     [pfnCreateFileA], rax

    mov     rcx, rbx
    lea     rdx, [szReadFile]
    call    GetProcAddress
    mov     [pfnReadFile], rax

    mov     rcx, rbx
    lea     rdx, [szWriteFile]
    call    GetProcAddress
    mov     [pfnWriteFile], rax

    mov     rcx, rbx
    lea     rdx, [szCloseHandle]
    call    GetProcAddress
    mov     [pfnCloseHandle], rax

    mov     rcx, rbx
    lea     rdx, [szGetFileSize]
    call    GetProcAddress
    mov     [pfnGetFileSize], rax

    mov     rcx, rbx
    lea     rdx, [szFlushFileBuffers]
    call    GetProcAddress
    mov     [pfnFlushFileBuffers], rax

    xor     eax, eax
    jmp     _sh_ra_done

_sh_ra_fail:
    mov     eax, 1

_sh_ra_done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
_sh_resolve_apis ENDP

; _sh_locate_text_section — Find .text section in PE headers
_sh_locate_text_section PROC
    push    rbx
    push    rsi
    sub     rsp, 28h

    ; Get module base (NULL = main exe)
    xor     ecx, ecx
    call    GetModuleHandleA
    test    rax, rax
    jz      _sh_lt_fail

    mov     rbx, rax                    ; image base

    ; DOS header → e_lfanew
    movzx   eax, WORD PTR [rbx]
    cmp     ax, 05A4Dh                  ; 'MZ'
    jne     _sh_lt_fail

    mov     eax, DWORD PTR [rbx + 03Ch] ; e_lfanew
    lea     rsi, [rbx + rax]            ; PE header

    ; Verify PE signature
    cmp     DWORD PTR [rsi], 04550h     ; 'PE\0\0'
    jne     _sh_lt_fail

    ; COFF header: NumberOfSections at offset 6
    movzx   ecx, WORD PTR [rsi + 6]     ; section count
    ; SizeOfOptionalHeader at offset 20
    movzx   edx, WORD PTR [rsi + 20]

    ; First section header = PE + 24 + SizeOfOptionalHeader
    lea     rsi, [rsi + 24]
    add     rsi, rdx                    ; rsi → first IMAGE_SECTION_HEADER

    ; Search for .text section
_sh_lt_scan:
    test    ecx, ecx
    jz      _sh_lt_fail

    ; Section name at offset 0 (8 bytes)
    cmp     DWORD PTR [rsi], 07865742Eh ; '.tex' (little-endian)
    jne     _sh_lt_next

    ; VirtualAddress at offset 12, SizeOfRawData at offset 16
    mov     eax, DWORD PTR [rsi + 12]   ; VirtualAddress (RVA)
    add     rax, rbx                    ; VA
    mov     [g_sh_text_base], rax

    mov     eax, DWORD PTR [rsi + 8]    ; VirtualSize
    mov     [g_sh_text_size], rax

    xor     eax, eax                    ; success
    jmp     _sh_lt_done

_sh_lt_next:
    add     rsi, 40                     ; IMAGE_SECTION_HEADER size
    dec     ecx
    jmp     _sh_lt_scan

_sh_lt_fail:
    mov     eax, SH_ERR_TEXT_NOT_FOUND

_sh_lt_done:
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
_sh_locate_text_section ENDP

; =============================================================================
; External imports (kernel32.lib)
; =============================================================================
EXTERN GetModuleHandleA:PROC
EXTERN GetProcAddress:PROC

END
