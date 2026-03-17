; ═══════════════════════════════════════════════════════════════════
; stress_harness.asm — Minimal entry point for Phase 14 stress test
;   Debug version with step-by-step breadcrumbs
; ═══════════════════════════════════════════════════════════════════

PUBLIC WinMainCRTStartup
PUBLIC g_hHeap
PUBLIC g_hInstance

EXTERN ExitProcess:PROC
EXTERN GetModuleHandleW:PROC
EXTERN HeapCreate:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC

EXTERN BeaconRouterInit:PROC
EXTERN SwarmNet_Init:PROC
EXTERN StressTest_Run:PROC
EXTERN StressTest_LogStats:PROC

.data
align 8
g_hHeap      dq 0
g_hInstance  dq 0
g_hStdout    dq 0
g_wr         dd 0

szS1  db "[1] Heap+Beacon OK",13,10,0
szS2  db "[2] SwarmNet OK",13,10,0
szS3  db "[3] Entering StressTest_Run",13,10,0
szS4  db "[4] StressTest_Run done",13,10,0
szS5  db "[5] LogStats done",13,10,0

.code

WinMainCRTStartup PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; Get stdout handle
    mov     rcx, -11
    call    GetStdHandle
    mov     g_hStdout, rax
    mov     rbx, rax

    ; GetModuleHandleW(NULL)
    xor     rcx, rcx
    call    GetModuleHandleW
    mov     g_hInstance, rax

    ; HeapCreate(0, 4MB, 0)
    xor     ecx, ecx
    mov     edx, 400000h
    xor     r8d, r8d
    call    HeapCreate
    test    rax, rax
    jz      @fail
    mov     g_hHeap, rax

    ; BeaconRouterInit
    call    BeaconRouterInit

    ; === Print "[1]" ===
    mov     rcx, rbx
    lea     rdx, szS1
    mov     r8d, 20
    lea     r9, g_wr
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; SwarmNet_Init (non-fatal)
    call    SwarmNet_Init

    ; === Print "[2]" ===
    mov     rcx, rbx
    lea     rdx, szS2
    mov     r8d, 17
    lea     r9, g_wr
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; === Print "[3]" ===
    mov     rcx, rbx
    lea     rdx, szS3
    mov     r8d, 29
    lea     r9, g_wr
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; Run the stress test
    call    StressTest_Run

    ; === Print "[4]" ===
    mov     rcx, rbx
    lea     rdx, szS4
    mov     r8d, 25
    lea     r9, g_wr
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; Print results
    call    StressTest_LogStats

    ; === Print "[5]" ===
    mov     rcx, rbx
    lea     rdx, szS5
    mov     r8d, 21
    lea     r9, g_wr
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    ; ExitProcess(0)
    xor     ecx, ecx
    call    ExitProcess

@fail:
    mov     ecx, 1
    call    ExitProcess

WinMainCRTStartup ENDP

END
