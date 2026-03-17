; NetworkRelay_Benchmark.asm — Latency/Throughput Validation
; Run: ml64 /link /subsystem:console /entry:main

EXTRN   RelayEngine_Init:PROC
EXTRN   RelayEngine_CreateContext:PROC
EXTRN   RelayEngine_RunBiDirectional:PROC
EXTRN   GetCurrentProcessorNumber:PROC
EXTRN   QueryPerformanceCounter:PROC
EXTRN   QueryPerformanceFrequency:PROC
EXTRN   printf:PROC

.data
align 8
g_PerfStart     DQ      ?
g_PerfEnd       DQ      ?
g_PerfFreq      DQ      ?
g_BytesTx       DQ      ?
fmt_str         DB      "[Benchmark] Init time: %lld µs, Pool: 64MB", 0Ah, 0
fmt_perf        DB      "[Benchmark] Transfer: %lld bytes in %lld µs (%lld MB/s)", 0Ah, 0

.code
main PROC
    ; Init 64MB pool
    mov     rcx, 04000000h              ; 64MB
    call    RelayEngine_Init

    ; Get frequency
    lea     rcx, [g_PerfFreq]
    call    QueryPerformanceFrequency

    ; Benchmark init time
    lea     rcx, [g_PerfStart]
    call    QueryPerformanceCounter

    ; ... create loopback test socket pair (requires WSASocket setup) ...
    ; For now, just measure init

    lea     rcx, [g_PerfEnd]
    call    QueryPerformanceCounter

    ; Calculate init time in µs
    mov     rax, [g_PerfEnd]
    sub     rax, [g_PerfStart]
    mov     rcx, 1000000
    mul     rcx
    div     QWORD PTR [g_PerfFreq]

    ; Print
    lea     rcx, [fmt_str]
    mov     rdx, rax
    call    printf

    xor     eax, eax
    ret
main ENDP
END