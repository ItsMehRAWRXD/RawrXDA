; ═══════════════════════════════════════════════════════════════════
; stream_bench.asm — Phase 5: Paging vs Full-Load Benchmark
; Measures: Reserve latency, Prefetch throughput, Eviction speed
; ═══════════════════════════════════════════════════════════════════

INCLUDE rawrxd.inc

EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN wsprintfW:PROC
EXTERN MessageBoxW:PROC

PUBLIC RunStreamBenchmark

.data
szBenchTitle    dw 'P','h','a','s','e',' ','5',':',' ','S','t','r','e','a','m',' ','B','e','n','c','h',0
szBenchFmt      dw 'V','M','M',' ','R','e','s','e','r','v','e',':',' ','%','l','u',' ','u','s',13,10
                dw 'P','r','e','f','e','t','c','h',' ','(','1','M','B',')',':',' ','%','l','u',' ','u','s',13,10
                dw 'E','v','i','c','t',':',' ','%','l','u',' ','u','s',0

; Benchmark parameters
VMM_BENCH_SIZE  equ 100000h        ; 1MB reserve for bench
PREFETCH_SIZE   equ 100000h        ; 1MB prefetch

.data?
qFreq           dq ?
qT0             dq ?
qT1             dq ?
qT2             dq ?
qT3             dq ?
qT4             dq ?
szBuf           dw 512 dup(?)

.code

; ────────────────────────────────────────────────────────────────
; RunStreamBenchmark — Measure VMM reserve / prefetch / evict
; ────────────────────────────────────────────────────────────────
RunStreamBenchmark PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    ; Get frequency
    lea     rcx, qFreq
    call    QueryPerformanceFrequency

    ; ── Bench 1: VMM Reserve ──────────────────────────────────
    lea     rcx, qT0
    call    QueryPerformanceCounter

    mov     rcx, VMM_BENCH_SIZE
    call    StreamLoaderInit

    lea     rcx, qT1
    call    QueryPerformanceCounter

    ; ── Bench 2: Prefetch (commit + fault pages) ──────────────
    lea     rcx, qT2
    call    QueryPerformanceCounter

    mov     rcx, g_modelbase
    mov     rdx, PREFETCH_SIZE
    call    StreamPrefetch

    lea     rcx, qT3
    call    QueryPerformanceCounter

    ; ── Bench 3: Evict (decommit) ─────────────────────────────
    mov     rcx, g_modelbase
    mov     rdx, PREFETCH_SIZE
    call    StreamEvictCold

    lea     rcx, qT4
    call    QueryPerformanceCounter

    ; ── Calculate latencies ───────────────────────────────────
    ; Reserve latency (us)
    mov     rax, qT1
    sub     rax, qT0
    imul    rax, 1000000
    xor     rdx, rdx
    div     qFreq
    mov     r8, rax                 ; reserve_us

    ; Prefetch latency (us)
    mov     rax, qT3
    sub     rax, qT2
    imul    rax, 1000000
    xor     rdx, rdx
    div     qFreq
    mov     r9, rax                 ; prefetch_us

    ; Evict latency (us)
    mov     rax, qT4
    sub     rax, qT3
    imul    rax, 1000000
    xor     rdx, rdx
    div     qFreq
    mov     [rsp+20h], rax          ; evict_us (5th arg)

    ; ── Format and display ────────────────────────────────────
    lea     rcx, szBuf
    lea     rdx, szBenchFmt
    ; R8 = reserve, R9 = prefetch, [rsp+20h] = evict
    call    wsprintfW

    xor     rcx, rcx
    lea     rdx, szBuf
    lea     r8, szBenchTitle
    xor     r9, r9
    call    MessageBoxW

    add     rsp, 48h
    pop     rsi
    pop     rbx
    ret
RunStreamBenchmark ENDP

END
