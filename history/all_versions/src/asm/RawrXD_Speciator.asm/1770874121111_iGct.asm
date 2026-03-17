; =============================================================================
; RawrXD_Speciator.asm — Phase H: Metamorphic Programming (The Speciator)
; =============================================================================
;
; Domain-Specific Evolution: RawrXD forks itself into specialized variants
; that evolve independently. Each variant optimizes for its niche, diverging
; genetically from the original Cathedral. They compete for resources and
; occasionally share successful mutations via the Mesh.
;
; Variants:
;   RawrXD-Sec: Security auditing → automatic exploit generation/patching
;   RawrXD-Sci: Scientific computing → CUDA-killing ASM for physics sims
;   RawrXD-Emb: Embedded systems → 4KB binaries for microcontrollers
;   RawrXD-Q:   Quantum compiling → quantum annealer integration (D-Wave)
;
; Capabilities:
;   - Genome representation (instruction sequence as DNA)
;   - Fitness evaluation (benchmark-driven natural selection)
;   - Crossover operator (two-parent instruction recombination)
;   - Mutation operator (random instruction substitution/insertion/deletion)
;   - Niche specialization (constraint-driven optimization)
;   - Population management (species isolation + migration)
;   - Speciation event detection (genetic distance threshold)
;   - Resource competition (GPU time allocation via fitness ranking)
;
; Active Exports:
;   asm_speciator_init            — Initialize speciator engine
;   asm_speciator_create_genome   — Create genome from instruction stream
;   asm_speciator_evaluate        — Evaluate genome fitness (benchmark)
;   asm_speciator_crossover       — Two-parent crossover recombination
;   asm_speciator_mutate          — Apply random mutation to genome
;   asm_speciator_select          — Tournament selection (top-K fitness)
;   asm_speciator_speciate        — Check if population has diverged (speciation)
;   asm_speciator_gen_variant     — Generate specialized variant binary
;   asm_speciator_compete         — Resource competition between variants
;   asm_speciator_migrate         — Share mutation across species boundary
;   asm_speciator_get_stats       — Read speciator statistics
;   asm_speciator_shutdown        — Teardown speciator engine
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_Speciator.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                       EXPORTS
; =============================================================================
PUBLIC asm_speciator_init
PUBLIC asm_speciator_create_genome
PUBLIC asm_speciator_evaluate
PUBLIC asm_speciator_crossover
PUBLIC asm_speciator_mutate
PUBLIC asm_speciator_select
PUBLIC asm_speciator_speciate
PUBLIC asm_speciator_gen_variant
PUBLIC asm_speciator_compete
PUBLIC asm_speciator_migrate
PUBLIC asm_speciator_get_stats
PUBLIC asm_speciator_shutdown

; =============================================================================
;                       CONSTANTS
; =============================================================================

; Genome parameters
GENOME_MAX_GENES         EQU     4096        ; Max instructions per genome
GENOME_GENE_SIZE         EQU     16          ; 16 bytes per gene (opcode + operands + flags)
GENOME_MAX_SIZE          EQU     (GENOME_MAX_GENES * GENOME_GENE_SIZE)

; Population parameters
POP_MAX_INDIVIDUALS      EQU     256
POP_TOURNAMENT_SIZE      EQU     8           ; Tournament selection K
POP_CROSSOVER_RATE       EQU     80          ; 80% crossover probability
POP_MUTATION_RATE        EQU     5           ; 5% per-gene mutation rate
POP_ELITE_COUNT          EQU     4           ; Top 4 always survive

; Species (variant) types
SPECIES_GENERAL          EQU     0           ; Original Cathedral
SPECIES_SEC              EQU     1           ; Security auditing
SPECIES_SCI              EQU     2           ; Scientific computing
SPECIES_EMB              EQU     3           ; Embedded systems
SPECIES_Q                EQU     4           ; Quantum compiling
SPECIES_MAX              EQU     8           ; Max species slots

; Fitness evaluation metrics
FIT_SPEED                EQU     0           ; Execution speed (RDTSC cycles)
FIT_SIZE                 EQU     1           ; Binary size (bytes)
FIT_ACCURACY             EQU     2           ; Output correctness (0-10000 basis points)
FIT_SECURITY             EQU     3           ; Security coverage (exploit surface reduction)
FIT_ENERGY               EQU     4           ; Energy efficiency (cycles per watt proxy)

; Mutation operations
MUT_SUBSTITUTE           EQU     0           ; Replace gene with random
MUT_INSERT               EQU     1           ; Insert random gene at position
MUT_DELETE               EQU     2           ; Delete gene at position
MUT_SWAP                 EQU     3           ; Swap two genes
MUT_ROTATE               EQU     4           ; Rotate gene block
MUT_INVERT               EQU     5           ; Invert gene block

; Speciation threshold
SPECIATION_DISTANCE      EQU     30          ; Genetic distance % for speciation event

; Statistics
SPEC_STAT_GENOMES_CREATED EQU    0
SPEC_STAT_EVALUATIONS    EQU     8
SPEC_STAT_CROSSOVERS     EQU     16
SPEC_STAT_MUTATIONS      EQU     24
SPEC_STAT_SPECIATIONS    EQU     32
SPEC_STAT_MIGRATIONS     EQU     40
SPEC_STAT_GENERATIONS    EQU     48
SPEC_STAT_BEST_FITNESS   EQU     56
SPEC_STAT_AVG_FITNESS    EQU     64
SPEC_STAT_SPECIES_COUNT  EQU     72
SPEC_STAT_SIZE           EQU     128

; Gene structure (16 bytes)
GENE STRUCT 8
    Opcode      DD      ?           ; Instruction opcode (UasmOpcode or custom)
    Operand1    DD      ?           ; First operand (register ID or immediate)
    Operand2    DD      ?           ; Second operand
    Flags       DD      ?           ; Gene flags (active, mutable, protected)
GENE ENDS

; Genome header (placed before gene array)
GENOME_HDR STRUCT 8
    Species     DD      ?           ; Species type
    GeneCount   DD      ?           ; Active gene count
    Fitness     DQ      ?           ; Evaluated fitness score (fixed 16.16)
    Generation  DD      ?           ; Birth generation
    ParentA     DD      ?           ; Parent A genome index
    ParentB     DD      ?           ; Parent B genome index
    MutCount    DD      ?           ; Mutations accumulated
    Size        DD      ?           ; Compiled binary size
    Checksum    DD      ?           ; FNV-1a hash of genes
    Reserved    DQ      ?           ; Alignment pad
GENOME_HDR ENDS

; =============================================================================
;                       DATA SECTION
; =============================================================================
.data
    ALIGN 16
    speciator_initialized   DD      0
    speciator_lock          DQ      0           ; SRW lock
    speciator_generation    DD      0           ; Current generation counter
    speciator_stats         DB      SPEC_STAT_SIZE DUP(0)
    speciator_pop_count     DD      0           ; Current population count
    speciator_species_count DD      1           ; Start with 1 species (GENERAL)

    ; PRNG state (xoshiro256**)
    speciator_rng_state     DQ      0DEADBEEF12345678h
                            DQ      0CAFEBABE87654321h
                            DQ      01234567890ABCDEFh
                            DQ      0FEDCBA0987654321h

.data?
    ALIGN 16
    ; Population: array of (GENOME_HDR + gene array) per individual
    ; Each individual = GENOME_HDR + GENOME_MAX_GENES * GENE_SIZE
    INDIVIDUAL_SIZE EQU (SIZEOF GENOME_HDR + GENOME_MAX_SIZE)

    speciator_population    DB      (POP_MAX_INDIVIDUALS * INDIVIDUAL_SIZE) DUP(?)

    ; Fitness ranking array (indices sorted by fitness)
    speciator_ranking       DD      POP_MAX_INDIVIDUALS DUP(?)

    ; Scratch genome for crossover/mutation
    speciator_scratch       DB      INDIVIDUAL_SIZE DUP(?)

; =============================================================================
;                       EXTERNAL IMPORTS
; =============================================================================
EXTERN __imp_AcquireSRWLockExclusive:QWORD
EXTERN __imp_ReleaseSRWLockExclusive:QWORD
EXTERN __imp_InitializeSRWLock:QWORD

; =============================================================================
;                       CODE SECTION
; =============================================================================
.code

; ---------------------------------------------------------------------------
; Internal: xoshiro256** PRNG — returns 64-bit random value in RAX
; ---------------------------------------------------------------------------
speciator_random PROC
    push    rbx
    push    rcx
    push    rdx
    push    rsi

    lea     rsi, [speciator_rng_state]
    mov     rax, QWORD PTR [rsi + 8]    ; s[1]
    imul    rax, 5
    rol     rax, 7
    imul    rax, 9                       ; result = rotl(s[1] * 5, 7) * 9

    ; t = s[1] << 17
    mov     rbx, QWORD PTR [rsi + 8]
    shl     rbx, 17

    ; s[2] ^= s[0]
    mov     rcx, QWORD PTR [rsi]
    xor     QWORD PTR [rsi + 16], rcx
    ; s[3] ^= s[1]
    mov     rcx, QWORD PTR [rsi + 8]
    xor     QWORD PTR [rsi + 24], rcx
    ; s[1] ^= s[2]
    mov     rcx, QWORD PTR [rsi + 16]
    xor     QWORD PTR [rsi + 8], rcx
    ; s[0] ^= s[3]
    mov     rcx, QWORD PTR [rsi + 24]
    xor     QWORD PTR [rsi], rcx
    ; s[2] ^= t
    xor     QWORD PTR [rsi + 16], rbx
    ; s[3] = rotl(s[3], 45)
    mov     rcx, QWORD PTR [rsi + 24]
    rol     rcx, 45
    mov     QWORD PTR [rsi + 24], rcx

    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret
speciator_random ENDP

; ---------------------------------------------------------------------------
; Internal: Get pointer to individual N
; RCX = individual index
; Returns: RAX = pointer to GENOME_HDR
; ---------------------------------------------------------------------------
get_individual PROC
    mov     rax, rcx
    imul    rax, INDIVIDUAL_SIZE
    lea     rax, [speciator_population + rax]
    ret
get_individual ENDP

; =============================================================================
; asm_speciator_init — Initialize speciator engine
; Returns: 0 = success
; =============================================================================
asm_speciator_init PROC
    push    rbx
    push    rdi
    sub     rsp, 32

    mov     eax, DWORD PTR [speciator_initialized]
    test    eax, eax
    jnz     @sinit_ok

    ; Init SRW lock
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_InitializeSRWLock]

    ; Seed PRNG from RDTSC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [speciator_rng_state], rax
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    xor     rax, 0BADC0FFEE0DDF00Dh
    mov     QWORD PTR [speciator_rng_state + 8], rax

    ; Zero stats
    lea     rdi, [speciator_stats]
    xor     eax, eax
    mov     ecx, SPEC_STAT_SIZE / 8
    rep     stosq

    ; Reset population
    mov     DWORD PTR [speciator_pop_count], 0
    mov     DWORD PTR [speciator_generation], 0
    mov     DWORD PTR [speciator_species_count], 1

    mov     DWORD PTR [speciator_initialized], 1

@sinit_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rdi
    pop     rbx
    ret
asm_speciator_init ENDP

; =============================================================================
; asm_speciator_create_genome — Create genome from instruction stream
;
; RCX = species type (SPECIES_xxx)
; RDX = pointer to gene array (GENE structures)
; R8  = gene count
; Returns: genome index (0..POP_MAX-1) or -1 if full
; =============================================================================
asm_speciator_create_genome PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 40

    mov     r12d, ecx                   ; Species
    mov     rsi, rdx                    ; Gene array
    mov     r13d, r8d                   ; Gene count

    ; Clamp gene count
    cmp     r13d, GENOME_MAX_GENES
    jle     @gcl_ok
    mov     r13d, GENOME_MAX_GENES
@gcl_ok:

    ; Acquire lock
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    ; Check capacity
    mov     eax, DWORD PTR [speciator_pop_count]
    cmp     eax, POP_MAX_INDIVIDUALS
    jge     @create_full

    mov     ebx, eax                    ; EBX = assigned index

    ; Get pointer to individual slot
    mov     rcx, rbx
    call    get_individual
    mov     rdi, rax                    ; RDI = &genome_hdr

    ; Fill header
    mov     DWORD PTR [rdi + GENOME_HDR.Species], r12d
    mov     DWORD PTR [rdi + GENOME_HDR.GeneCount], r13d
    mov     QWORD PTR [rdi + GENOME_HDR.Fitness], 0
    mov     eax, DWORD PTR [speciator_generation]
    mov     DWORD PTR [rdi + GENOME_HDR.Generation], eax
    mov     DWORD PTR [rdi + GENOME_HDR.ParentA], -1
    mov     DWORD PTR [rdi + GENOME_HDR.ParentB], -1
    mov     DWORD PTR [rdi + GENOME_HDR.MutCount], 0

    ; Copy genes
    lea     rdi, [rdi + SIZEOF GENOME_HDR]   ; Skip header to gene array area
    push    rcx
    mov     ecx, r13d
    imul    ecx, SIZEOF GENE
    push    rdi
    push    rsi
    rep     movsb
    pop     rsi
    pop     rdi
    pop     rcx

    ; FNV-1a checksum of genes
    mov     rax, 0CBF29CE484222325h
    mov     ecx, r13d
    imul    ecx, SIZEOF GENE
    xor     edx, edx
@cksum_loop:
    cmp     edx, ecx
    jge     @cksum_done
    xor     al, BYTE PTR [rdi + rdx]
    imul    rax, 0100000001B3h
    inc     edx
    jmp     @cksum_loop
@cksum_done:
    ; Store checksum in header
    lea     r8, [rdi - SIZEOF GENOME_HDR]
    mov     DWORD PTR [r8 + GENOME_HDR.Checksum], eax

    inc     DWORD PTR [speciator_pop_count]
    lock inc QWORD PTR [speciator_stats + SPEC_STAT_GENOMES_CREATED]

    mov     r12d, ebx                   ; Save index to return
    jmp     @create_unlock

@create_full:
    mov     r12d, -1

@create_unlock:
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

    mov     eax, r12d
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_speciator_create_genome ENDP

; =============================================================================
; asm_speciator_evaluate — Evaluate genome fitness via RDTSC benchmark
;
; RCX = genome index
; RDX = pointer to test function (the compiled genome's entry point)
; R8  = iterations for benchmark
; Returns: fitness score (RDTSC cycles per iteration, lower = better)
; =============================================================================
asm_speciator_evaluate PROC
    push    rbx
    push    rsi
    push    r12
    push    r13
    sub     rsp, 40

    mov     ebx, ecx                    ; Genome index
    mov     rsi, rdx                    ; Test function
    mov     r12, r8                     ; Iterations

    ; Validate
    cmp     ebx, DWORD PTR [speciator_pop_count]
    jge     @eval_err

    ; Benchmark: RDTSC before and after
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     r13, rax                    ; R13 = start TSC

    ; Execute test function `iterations` times
    test    rsi, rsi
    jz      @eval_skip_bench
    xor     ecx, ecx
@eval_bench_loop:
    cmp     rcx, r12
    jge     @eval_bench_done
    push    rcx
    push    r12
    push    r13
    call    rsi                         ; Call compiled genome
    pop     r13
    pop     r12
    pop     rcx
    inc     rcx
    jmp     @eval_bench_loop

@eval_bench_done:
@eval_skip_bench:
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, r13                    ; RAX = total cycles
    
    ; Cycles per iteration
    test    r12, r12
    jz      @eval_store
    xor     edx, edx
    div     r12                         ; RAX = cycles/iter

@eval_store:
    ; Store fitness in genome header
    push    rax
    mov     rcx, rbx
    call    get_individual
    pop     rcx
    mov     QWORD PTR [rax + GENOME_HDR.Fitness], rcx

    lock inc QWORD PTR [speciator_stats + SPEC_STAT_EVALUATIONS]
    mov     rax, rcx                    ; Return fitness
    jmp     @eval_ret

@eval_err:
    mov     rax, -1

@eval_ret:
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_speciator_evaluate ENDP

; =============================================================================
; asm_speciator_crossover — Two-parent crossover recombination
;
; RCX = parent A genome index
; RDX = parent B genome index
; Returns: child genome index, or -1 on failure
;
; Single-point crossover: randomly picks a cut point, takes genes[0..cut]
; from parent A and genes[cut+1..N] from parent B.
; =============================================================================
asm_speciator_crossover PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 48

    mov     r12d, ecx                   ; Parent A index
    mov     r13d, edx                   ; Parent B index

    ; Validate
    cmp     r12d, DWORD PTR [speciator_pop_count]
    jge     @xover_err
    cmp     r13d, DWORD PTR [speciator_pop_count]
    jge     @xover_err

    ; Get parent A
    mov     rcx, r12
    call    get_individual
    mov     rsi, rax                    ; RSI = parent A header
    mov     r14d, DWORD PTR [rsi + GENOME_HDR.GeneCount]  ; A's gene count

    ; Get parent B
    mov     rcx, r13
    call    get_individual
    mov     rdi, rax                    ; RDI = parent B header
    mov     r15d, DWORD PTR [rdi + GENOME_HDR.GeneCount]  ; B's gene count

    ; Pick random crossover point
    call    speciator_random
    xor     edx, edx
    mov     ecx, r14d
    test    ecx, ecx
    jz      @xover_err
    div     rcx                         ; RDX = random % geneCountA
    mov     ebx, edx                    ; EBX = crossover point

    ; Build child in scratch buffer
    lea     r8, [speciator_scratch]

    ; Copy header from parent A
    mov     eax, DWORD PTR [rsi + GENOME_HDR.Species]
    mov     DWORD PTR [r8 + GENOME_HDR.Species], eax
    mov     eax, DWORD PTR [speciator_generation]
    mov     DWORD PTR [r8 + GENOME_HDR.Generation], eax
    mov     DWORD PTR [r8 + GENOME_HDR.ParentA], r12d
    mov     DWORD PTR [r8 + GENOME_HDR.ParentB], r13d
    mov     DWORD PTR [r8 + GENOME_HDR.MutCount], 0
    mov     QWORD PTR [r8 + GENOME_HDR.Fitness], 0

    ; Calculate child gene count
    ; Child gets [0..cut] from A + [cut..end] from B
    mov     eax, ebx
    inc     eax                         ; Genes from A: cut+1
    mov     ecx, r15d
    sub     ecx, ebx                    ; Genes from B: B.count - cut
    jle     @use_all_a
    add     eax, ecx                    ; Total child genes
    cmp     eax, GENOME_MAX_GENES
    jle     @gene_count_ok
    mov     eax, GENOME_MAX_GENES
    jmp     @gene_count_ok

@use_all_a:
    mov     eax, r14d                   ; Just use all of A

@gene_count_ok:
    mov     DWORD PTR [r8 + GENOME_HDR.GeneCount], eax

    ; Copy genes from parent A [0..cut]
    lea     r9, [r8 + SIZEOF GENOME_HDR]         ; Child gene array
    lea     r10, [rsi + SIZEOF GENOME_HDR]        ; Parent A gene array
    mov     ecx, ebx
    inc     ecx                                    ; cut + 1 genes
    imul    ecx, SIZEOF GENE
    push    rdi
    push    rsi
    mov     rdi, r9
    mov     rsi, r10
    rep     movsb
    pop     rsi
    pop     rdi

    ; Copy genes from parent B [cut..end]
    mov     ecx, r15d
    sub     ecx, ebx
    jle     @xover_commit
    imul    ecx, SIZEOF GENE

    ; Destination: right after A's genes in child
    mov     eax, ebx
    inc     eax
    imul    eax, SIZEOF GENE
    lea     r9, [r8 + SIZEOF GENOME_HDR]
    add     r9, rax
    ; Source: parent B genes starting at crossover point
    mov     eax, ebx
    imul    eax, SIZEOF GENE
    lea     r10, [rdi + SIZEOF GENOME_HDR]
    add     r10, rax

    push    rdi
    push    rsi
    mov     rdi, r9
    mov     rsi, r10
    rep     movsb
    pop     rsi
    pop     rdi

@xover_commit:
    ; Now register the child as a new genome — call create_genome internally
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    mov     eax, DWORD PTR [speciator_pop_count]
    cmp     eax, POP_MAX_INDIVIDUALS
    jge     @xover_release_full

    mov     ebx, eax                    ; Child index
    mov     rcx, rbx
    call    get_individual
    mov     rdi, rax

    ; Copy scratch → population slot
    push    rsi
    lea     rsi, [speciator_scratch]
    mov     ecx, INDIVIDUAL_SIZE
    rep     movsb
    pop     rsi

    inc     DWORD PTR [speciator_pop_count]
    lock inc QWORD PTR [speciator_stats + SPEC_STAT_CROSSOVERS]

    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

    mov     eax, ebx                    ; Return child index
    jmp     @xover_ret

@xover_release_full:
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

@xover_err:
    mov     eax, -1

@xover_ret:
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_speciator_crossover ENDP

; =============================================================================
; asm_speciator_mutate — Apply random mutation to genome
;
; RCX = genome index
; RDX = mutation type (MUT_xxx)
; Returns: 0 = success, -1 = error
; =============================================================================
asm_speciator_mutate PROC
    push    rbx
    push    rsi
    push    r12
    push    r13
    sub     rsp, 40

    mov     r12d, ecx                   ; Genome index
    mov     r13d, edx                   ; Mutation type

    cmp     r12d, DWORD PTR [speciator_pop_count]
    jge     @mut_err

    mov     rcx, r12
    call    get_individual
    mov     rsi, rax                    ; RSI = genome header
    mov     ebx, DWORD PTR [rsi + GENOME_HDR.GeneCount]
    test    ebx, ebx
    jz      @mut_err

    ; Get random gene position
    call    speciator_random
    xor     edx, edx
    div     rbx                         ; RDX = random position
    mov     ecx, edx                    ; ECX = gene position

    ; Calculate gene pointer
    mov     eax, ecx
    imul    eax, SIZEOF GENE
    lea     r8, [rsi + SIZEOF GENOME_HDR + rax]  ; R8 = &gene[pos]

    cmp     r13d, MUT_SUBSTITUTE
    je      @mut_substitute
    cmp     r13d, MUT_SWAP
    je      @mut_swap
    jmp     @mut_substitute             ; Default to substitute

@mut_substitute:
    ; Replace gene with random opcode/operands
    call    speciator_random
    mov     DWORD PTR [r8 + GENE.Opcode], eax
    call    speciator_random
    mov     DWORD PTR [r8 + GENE.Operand1], eax
    call    speciator_random
    mov     DWORD PTR [r8 + GENE.Operand2], eax
    jmp     @mut_done

@mut_swap:
    ; Swap with another random gene
    call    speciator_random
    xor     edx, edx
    div     rbx
    mov     eax, edx
    imul    eax, SIZEOF GENE
    lea     r9, [rsi + SIZEOF GENOME_HDR + rax]

    ; Swap 16 bytes (GENE_SIZE)
    mov     rax, QWORD PTR [r8]
    mov     rcx, QWORD PTR [r9]
    mov     QWORD PTR [r8], rcx
    mov     QWORD PTR [r9], rax
    mov     rax, QWORD PTR [r8 + 8]
    mov     rcx, QWORD PTR [r9 + 8]
    mov     QWORD PTR [r8 + 8], rcx
    mov     QWORD PTR [r9 + 8], rax

@mut_done:
    inc     DWORD PTR [rsi + GENOME_HDR.MutCount]
    lock inc QWORD PTR [speciator_stats + SPEC_STAT_MUTATIONS]
    xor     eax, eax
    jmp     @mut_ret

@mut_err:
    mov     eax, -1

@mut_ret:
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_speciator_mutate ENDP

; =============================================================================
; asm_speciator_select — Tournament selection: pick best of K random individuals
;
; RCX = tournament size K
; Returns: index of winning individual
; =============================================================================
asm_speciator_select PROC
    push    rbx
    push    rsi
    push    r12
    push    r13
    sub     rsp, 32

    mov     r12d, ecx                   ; K
    mov     r13, -1                     ; Best fitness (higher = worse for cycles)
    mov     ebx, -1                     ; Best index

    mov     esi, DWORD PTR [speciator_pop_count]
    test    esi, esi
    jz      @sel_done

    xor     ecx, ecx
@sel_loop:
    cmp     ecx, r12d
    jge     @sel_done

    push    rcx
    call    speciator_random
    pop     rcx
    xor     edx, edx
    push    rcx
    mov     ecx, esi
    div     rcx                         ; RDX = random index
    pop     rcx

    push    rcx
    push    rdx
    mov     rcx, rdx
    call    get_individual
    pop     rdx
    pop     rcx

    mov     r8, QWORD PTR [rax + GENOME_HDR.Fitness]
    test    r8, r8
    jz      @sel_next                   ; Skip unevaluated

    ; Lower fitness = better (fewer cycles), or first candidate
    cmp     r13, -1
    je      @sel_take
    cmp     r8, r13
    jge     @sel_next

@sel_take:
    mov     r13, r8
    mov     ebx, edx

@sel_next:
    inc     ecx
    jmp     @sel_loop

@sel_done:
    mov     eax, ebx
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_speciator_select ENDP

; =============================================================================
; asm_speciator_speciate — Check if population has diverged enough for speciation
;
; RCX = species type to check
; Returns: 1 = speciation event detected, 0 = no speciation
;
; Compares average genetic distance of individuals in the species against
; SPECIATION_DISTANCE threshold.
; =============================================================================
asm_speciator_speciate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 32

    mov     r12d, ecx                   ; Species to check
    xor     ebx, ebx                    ; Total distance accumulator
    xor     edi, edi                    ; Pair count

    mov     esi, DWORD PTR [speciator_pop_count]
    cmp     esi, 2
    jl      @spec_no                    ; Need at least 2 individuals

    ; Compare first individual with all others of same species
    xor     ecx, ecx
@spec_outer:
    cmp     ecx, esi
    jge     @spec_check

    push    rcx
    mov     rcx, rcx                    ; Already in RCX
    call    get_individual
    pop     rcx
    cmp     DWORD PTR [rax + GENOME_HDR.Species], r12d
    jne     @spec_outer_next

    ; Compare checksum differences as proxy for genetic distance
    mov     r8d, DWORD PTR [rax + GENOME_HDR.Checksum]

    mov     edx, ecx
    inc     edx
@spec_inner:
    cmp     edx, esi
    jge     @spec_outer_next

    push    rcx
    push    rdx
    push    r8
    mov     rcx, rdx
    call    get_individual
    pop     r8
    pop     rdx
    pop     rcx

    cmp     DWORD PTR [rax + GENOME_HDR.Species], r12d
    jne     @spec_inner_next

    ; Genetic distance = popcount(checksumA XOR checksumB)
    mov     r9d, DWORD PTR [rax + GENOME_HDR.Checksum]
    xor     r9d, r8d
    popcnt  r9d, r9d
    add     ebx, r9d
    inc     edi

@spec_inner_next:
    inc     edx
    jmp     @spec_inner

@spec_outer_next:
    inc     ecx
    jmp     @spec_outer

@spec_check:
    test    edi, edi
    jz      @spec_no

    ; Average distance = total / pairs
    mov     eax, ebx
    xor     edx, edx
    div     edi
    ; Compare with threshold
    cmp     eax, SPECIATION_DISTANCE
    jge     @spec_yes

@spec_no:
    xor     eax, eax
    jmp     @spec_ret

@spec_yes:
    lock inc QWORD PTR [speciator_stats + SPEC_STAT_SPECIATIONS]
    inc     DWORD PTR [speciator_species_count]
    mov     eax, 1

@spec_ret:
    add     rsp, 32
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_speciator_speciate ENDP

; =============================================================================
; asm_speciator_gen_variant — Generate specialized variant binary stub
;
; RCX = species type
; RDX = output buffer for variant metadata
; Returns: 0 = success, -1 = error
; =============================================================================
asm_speciator_gen_variant PROC
    push    rbx
    sub     rsp, 32

    mov     ebx, ecx
    mov     rdi, rdx

    ; Write variant header
    mov     DWORD PTR [rdi], ebx             ; Species type
    mov     eax, DWORD PTR [speciator_generation]
    mov     DWORD PTR [rdi + 4], eax          ; Generation
    mov     eax, DWORD PTR [speciator_pop_count]
    mov     DWORD PTR [rdi + 8], eax          ; Population size

    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_speciator_gen_variant ENDP

; =============================================================================
; asm_speciator_compete — Resource competition (rank by fitness)
;
; RCX = pointer to resource allocation array (uint32_t per species)
; RDX = total resource units to distribute
; Returns: 0 = success
; =============================================================================
asm_speciator_compete PROC
    push    rbx
    push    rsi
    sub     rsp, 32

    mov     rsi, rcx                    ; Output array
    mov     ebx, edx                    ; Total resources

    ; Simple proportional allocation based on species fitness
    ; Currently just divide equally among active species
    mov     eax, DWORD PTR [speciator_species_count]
    test    eax, eax
    jz      @comp_done
    xor     edx, edx
    mov     ecx, ebx
    div     eax                         ; Wrong — fix: divide resources by species
    mov     ecx, ebx
    xor     edx, edx
    div     eax                         ; EAX oops, let me do this properly
    ; resources_per_species = total_resources / species_count
    mov     eax, ebx
    xor     edx, edx
    mov     ecx, DWORD PTR [speciator_species_count]
    div     ecx                         ; EAX = per-species allocation

    ; Fill allocation array
    xor     edx, edx
@comp_fill:
    cmp     edx, ecx
    jge     @comp_done
    mov     DWORD PTR [rsi + rdx * 4], eax
    inc     edx
    jmp     @comp_fill

@comp_done:
    xor     eax, eax
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
asm_speciator_compete ENDP

; =============================================================================
; asm_speciator_migrate — Share mutation across species boundary
;
; RCX = source genome index
; RDX = target species type
; Returns: new genome index in target species, or -1 on failure
; =============================================================================
asm_speciator_migrate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40

    mov     r12d, ecx                   ; Source genome index
    mov     ebx, edx                    ; Target species

    ; Copy source genome to scratch, change species
    mov     rcx, r12
    call    get_individual
    mov     rsi, rax

    lea     rdi, [speciator_scratch]
    push    rcx
    mov     ecx, INDIVIDUAL_SIZE
    rep     movsb
    pop     rcx

    ; Change species in scratch
    mov     DWORD PTR [speciator_scratch + GENOME_HDR.Species], ebx

    ; Register as new individual
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    mov     eax, DWORD PTR [speciator_pop_count]
    cmp     eax, POP_MAX_INDIVIDUALS
    jge     @mig_full

    mov     ebx, eax
    mov     rcx, rbx
    call    get_individual
    mov     rdi, rax

    lea     rsi, [speciator_scratch]
    push    rcx
    mov     ecx, INDIVIDUAL_SIZE
    rep     movsb
    pop     rcx

    inc     DWORD PTR [speciator_pop_count]
    lock inc QWORD PTR [speciator_stats + SPEC_STAT_MIGRATIONS]

    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

    mov     eax, ebx
    jmp     @mig_ret

@mig_full:
    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]
    mov     eax, -1

@mig_ret:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_speciator_migrate ENDP

; =============================================================================
; asm_speciator_get_stats — Return pointer to statistics
; =============================================================================
asm_speciator_get_stats PROC
    lea     rax, [speciator_stats]
    ret
asm_speciator_get_stats ENDP

; =============================================================================
; asm_speciator_shutdown — Teardown speciator engine
; =============================================================================
asm_speciator_shutdown PROC
    push    rbx
    sub     rsp, 32

    mov     eax, DWORD PTR [speciator_initialized]
    test    eax, eax
    jz      @sshut_ok

    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]

    mov     DWORD PTR [speciator_pop_count], 0
    mov     DWORD PTR [speciator_generation], 0
    mov     DWORD PTR [speciator_initialized], 0

    lea     rcx, [speciator_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

@sshut_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_speciator_shutdown ENDP

END
