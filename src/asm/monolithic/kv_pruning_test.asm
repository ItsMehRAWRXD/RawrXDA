; ═══════════════════════════════════════════════════════════════════
; kv_pruning_test.asm — Latency Benchmarking for KV-Cache Pruning
; Validates Phase 4A (AVX-512) vs Standard x64 performance.
; ═══════════════════════════════════════════════════════════════════

INCLUDE rawrxd.inc

; External from inference.asm (ensure these are PUBLIC in inference.asm)
EXTERN g_kv_len:QWORD
EXTERN g_hasAVX512:DWORD

; Win32 Timing
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN wsprintfW:PROC
EXTERN MessageBoxW:PROC

.data
szTestTitle     dw 'R','a','w','r','X','D',' ','P','r','u','n','i','n','g',' ','B','e','n','c','h','m','a','r','k',0
szResultFmt     dw 'P','r','u','n','e',' ','(','T','a','i','l',' ','S','h','i','f','t',')',':',' ','%','l','u',' ','u','s',13,10
                dw 'S','c','o','r','e',' ','U','p','d','a','t','e',':',' ','%','l','u',' ','u','s',13,10
                dw 'A','V','X','-','5','1','2',' ','S','t','a','t','u','s',':',' ','%','s',0
szEnabled       dw 'E','N','A','B','L','E','D',0
szDisabled      dw 'D','I','S','A','B','L','E','D',0

.data?
qFreq           dq ?
qStart          dq ?
qEnd            dq ?
qScoreStart     dq ?
qScoreEnd       dq ?
szBuffer        dw 256 dup(?)
mockAttn        dd 100 dup(?)           ; Mock attention weights

.code

; ─── Main Benchmarking Entry ──────────────────────────────────────
RunPruningBenchmark PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    ; 1. Prepare frequency
    lea     rcx, qFreq
    call    QueryPerformanceFrequency

    ; 2. Initialize Inference Engine (Detects AVX-512)
    call    InferenceEngineInit

    ; 3. Setup Mock Data (Fill 100 tokens, each 512 bytes)
    ; In a real scenario, this would be VirtualAlloc'd, but here we use g_kvcache
    mov     g_kv_len, 100

    ; 4. Benchmark: Prune Tail (Keep 50, discard 50 from head)
    lea     rcx, qStart
    call    QueryPerformanceCounter

    ; REPEAT 1000 times for stable metric
    mov     rbx, 1000
@bench_loop:
    mov     g_kv_len, 100           ; Reset len for each iteration
    mov     rcx, 50                 ; keepCount
    mov     edx, 1                  ; flags = keep tail
    call    PruneKVCache
    dec     rbx
    jnz     @bench_loop

    lea     rcx, qEnd
    call    QueryPerformanceCounter

    ; 4a. Benchmark: Score Update Latency
    lea     rcx, qScoreStart
    call    QueryPerformanceCounter
    
    mov     rbx, 1000
@score_bench_loop:
    lea     rcx, mockAttn
    mov     rdx, 100                ; numActive
    call    UpdateImportanceScores
    dec     rbx
    jnz     @score_bench_loop
    
    lea     rcx, qScoreEnd
    call    QueryPerformanceCounter

    ; 5. Calculate Latencies (Delta / Freq / 1000 iter)
    ; Pruning Latency
    mov     rax, qEnd
    sub     rax, qStart             ; Total ticks
    imul    rax, 1000000            ; Convert to microseconds
    xor     rdx, rdx
    div     qFreq
    xor     rdx, rdx
    mov     rcx, 1000
    div     rcx                     ; Microseconds per call
    mov     r8, rax                 ; R8 = pruning latency

    ; Scoring Latency
    mov     rax, qScoreEnd
    sub     rax, qScoreStart
    imul    rax, 1000000
    xor     rdx, rdx
    div     qFreq
    xor     rdx, rdx
    mov     rcx, 1000
    div     rcx
    mov     r9, rax                 ; R9 = scoring latency

    ; 6. Format Result
    lea     rcx, szBuffer
    lea     rdx, szResultFmt
    ; R8 = Pruning, R9 = Scoring
    ; We need to push the 5th argument (AVX status string) to stack for wsprintfW
    mov     eax, g_hasAVX512
    test    eax, eax
    jz      @no_avx_str
    lea     rax, szEnabled
    mov     [rsp+20h], rax          ; 5th arg
    jmp     @do_fmt
@no_avx_str:
    lea     rax, szDisabled
    mov     [rsp+20h], rax

@do_fmt:
    call    wsprintfW

    ; 7. Display
    xor     rcx, rcx
    lea     rdx, szBuffer
    lea     r8, szTestTitle
    xor     r9, r9
    call    MessageBoxW

    add     rsp, 40h
    pop     rbx
    ret
RunPruningBenchmark ENDP

END
