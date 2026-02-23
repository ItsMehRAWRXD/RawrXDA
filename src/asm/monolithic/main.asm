; RawrXD Monolithic Kernel — Main Entry
; Assembles: ml64 /c /Fo main.obj main.asm
; Links: link main.obj inference.obj ui.obj beacon.obj lsp.obj agent.obj model_loader.obj ...
;
; Canonical bootstrap order:
;   1. HeapCreate            — 64MB arena
;   2. BeaconRouterInit      — ring buffers, slots 0-15
;   3. InferenceEngineInit   — AVX detection, KV cache
;   4. StreamLoaderInit      — VEH + LRU for demand-paged GGUF
;   5. ModelLoaderInit       — X+4 SRWLOCK for hotswap
;   6. LSPBridgeInit         — stub (no child process)
;   7. AgentCoreInit         — registers agent, allocates task queue
;   8. DAP_Init              — debug adapter protocol engine
;   9. Test_Init             — test explorer tree + state
;  10. Task_Init             — task runner configs + ring buffer
;  11. Swarm_Init            — multi-GPU Vulkan orchestrator (graceful fallback)
;  12. SwarmCoord_Init       — beacon-based work distribution
;  13. ExtHostInit           — extension host (sandboxed DLL loader)
;  14. WebView2Init          — WebView2 shell (graceful GDI fallback)
;  15. CLI parse             — --bench, --model, --prompt flags
;  16. UIMainLoop            — window + message pump (blocks until WM_QUIT)

EXTERN InferenceEngineInit:PROC
EXTERN UIMainLoop:PROC
EXTERN BeaconRouterInit:PROC
EXTERN AgentCoreInit:PROC
EXTERN LSPBridgeInit:PROC
EXTERN ModelLoaderInit:PROC
EXTERN DAP_Init:PROC
EXTERN Test_Init:PROC
EXTERN Task_Init:PROC
EXTERN Swarm_Init:PROC
EXTERN SwarmCoord_Init:PROC
EXTERN StreamLoaderInit:PROC
EXTERN StreamMapModel:PROC
EXTERN WebView2Init:PROC
EXTERN ExtHostInit:PROC
EXTERN RunInference:PROC
EXTERN HeapCreate:PROC
EXTERN GetModuleHandleW:PROC
EXTERN GetCommandLineW:PROC
EXTERN ExitProcess:PROC
EXTERN GetTickCount64:PROC
EXTERN WriteFile:PROC
EXTERN GetStdHandle:PROC
EXTERN CommandLineToArgvW:PROC
EXTERN lstrcmpiW:PROC

PUBLIC WinMain
PUBLIC WinMainCRTStartup
PUBLIC g_hInstance
PUBLIC g_hHeap
PUBLIC g_cmdShow
PUBLIC g_benchMode
PUBLIC g_pModelPath
PUBLIC g_pPrompt

.data
align 16
g_hInstance   dq 0
g_hHeap       dq 0
g_cmdShow     dd 0
g_benchMode   dd 0                  ; 1 = --benchmark mode
g_pModelPath  dq 0                  ; pointer to --model argument (wide)
g_pPrompt     dq 0                  ; pointer to --prompt argument (wide)
g_cmdLineW    dq 0                  ; raw GetCommandLineW result
g_benchStart  dq 0                  ; GetTickCount64 at bench start
g_benchTokens dd 0                  ; tokens generated in bench
g_hStdOut     dq 0                  ; stdout handle for bench output

.data?
align 8
g_benchBuf    db 256 dup(?)         ; output buffer for benchmark results

.const
szClassName   db "RawrXD_Monolithic",0
szTitle       db "RawrXD IDE",0
; CLI flag strings (wide for wcscmp)
szBench       dw '-','-','b','e','n','c','h',0
szBenchmark   dw '-','-','b','e','n','c','h','m','a','r','k',0
szModel       dw '-','-','m','o','d','e','l',0
szPrompt      dw '-','-','p','r','o','m','p','t',0
; Benchmark output template (narrow for WriteFile)
szBenchHdr    db "RawrXD Benchmark Results",13,10
              db "========================",13,10,0
szTPS         db "Tokens/sec: ",0
szBootTime    db "Boot-to-inference: ",0
szMs          db " ms",13,10,0
szNewline     db 13,10,0

STD_OUTPUT_HANDLE equ -11

.code
; ────────────────────────────────────────────────────────────────
; WinMain — master init sequence
;   RCX = hInstance, RDX = hPrevInstance, R8 = lpCmdLine, R9D = nCmdShow
;   FRAME + shadow space for all callee calls.
; ────────────────────────────────────────────────────────────────
WinMain PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    ; Save args into globals
    mov     g_hInstance, rcx
    mov     g_cmdShow, r9d
    mov     g_cmdLineW, r8          ; raw command line for ParseCommandLine

    ; 1. HeapCreate(flags=0, initCommit=4MB, maxSize=0 -> growable)
    xor     ecx, ecx
    mov     edx, 400000h
    xor     r8d, r8d
    call    HeapCreate
    test    rax, rax
    jz      @fail
    mov     g_hHeap, rax

    ; 2. Beacon first — all other modules signal through this
    call    BeaconRouterInit
    test    eax, eax
    jnz     @fail

    ; 3. Inference engine (depends on heap, uses beacon slot 2)
    call    InferenceEngineInit

    ; 4. Stream loader — VEH + LRU for demand-paged GGUF
    call    StreamLoaderInit

    ; 5. Model loader — SRWLOCK hot-swap
    call    ModelLoaderInit

    ; 6. LSP bridge (stub)
    xor     rcx, rcx
    call    LSPBridgeInit

    ; 7. Agent core (depends on Inference + Beacon)
    call    AgentCoreInit

    ; 8. DAP engine (debug adapter protocol)
    call    DAP_Init

    ; 9. Test explorer (test discovery + execution)
    call    Test_Init

    ; 10. Task runner (task configs + process management)
    call    Task_Init

    ; 11. Swarm — multi-GPU Vulkan orchestrator (graceful fallback)
    call    Swarm_Init

    ; 12. Swarm coordinator — beacon-based work distribution
    call    SwarmCoord_Init

    ; 13. Extension host — sandboxed DLL loader
    call    ExtHostInit

    ; 14. WebView2 shell — graceful GDI fallback if loader absent
    call    WebView2Init

    ; 15. Parse CLI flags: --bench, --model <path>, --prompt <text>
    call    ParseCommandLine

    ; 16. Branch: benchmark mode or interactive UI
    cmp     g_benchMode, 0
    jne     @benchmark

    ; ── Interactive mode ──────────────────────────────────────
    call    UIMainLoop
    xor     eax, eax
    jmp     @exit

    ; ── Benchmark mode ────────────────────────────────────────
@benchmark:
    call    GetTickCount64
    mov     g_benchStart, rax

    ; If --model supplied, demand-page it
    mov     rcx, g_pModelPath
    test    rcx, rcx
    jz      @bench_infer
    call    StreamMapModel

@bench_infer:
    call    RunInference

    ; Elapsed = GetTickCount64() - start
    call    GetTickCount64
    sub     rax, g_benchStart       ; rax = elapsed ms
    ; Exit with elapsed as exit code (useful for scripted bench)
    mov     ecx, eax
    call    ExitProcess

@exit:
    lea     rsp, [rbp]
    pop     rbp
    ret
@fail:
    mov     eax, 1
    lea     rsp, [rbp]
    pop     rbp
    ret
WinMain ENDP

; ────────────────────────────────────────────────────────────────
; ParseCommandLine — scan argv for --bench/--model/--prompt
;   Uses CommandLineToArgvW + lstrcmpiW for proper tokenisation.
;   Argv buffer intentionally leaked (g_pModelPath/g_pPrompt point into it).
; ────────────────────────────────────────────────────────────────
ParseCommandLine PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rcx, g_cmdLineW
    test    rcx, rcx
    jz      @pcl_done

    ; CommandLineToArgvW(lpCmdLine, &argc)
    lea     rdx, [rsp + 20h]        ; local: argc (dword)
    call    CommandLineToArgvW
    test    rax, rax
    jz      @pcl_done

    mov     rsi, rax                ; rsi = argv (LPWSTR*)
    mov     edi, dword ptr [rsp + 20h] ; edi = argc
    mov     ebx, 1                  ; skip argv[0] (program name)

@pcl_loop:
    cmp     ebx, edi
    jge     @pcl_done
    mov     r12, [rsi + rbx*8]      ; r12 = argv[i]

    ; ── try --bench ───────────────────────────
    mov     rcx, r12
    lea     rdx, [szBench]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_bench

    mov     rcx, r12
    lea     rdx, [szBenchmark]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_bench

    ; ── try --model ───────────────────────────
    mov     rcx, r12
    lea     rdx, [szModel]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_model

    ; ── try --prompt ──────────────────────────
    mov     rcx, r12
    lea     rdx, [szPrompt]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_prompt

    inc     ebx
    jmp     @pcl_loop

@pcl_bench:
    mov     g_benchMode, 1
    inc     ebx
    jmp     @pcl_loop

@pcl_model:
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pModelPath, rax
    inc     ebx
    jmp     @pcl_loop

@pcl_prompt:
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pPrompt, rax
    inc     ebx
    jmp     @pcl_loop

@pcl_done:
    lea     rsp, [rbp]
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
ParseCommandLine ENDP

; ────────────────────────────────────────────────────────────────
; WinMainCRTStartup — CRT-less entry point
;   Linker: /ENTRY:WinMainCRTStartup
;   Must preserve hInstance (rbx) across GetCommandLineW call.
; ────────────────────────────────────────────────────────────────
WinMainCRTStartup PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; GetModuleHandleW(NULL) → hInstance
    xor     rcx, rcx
    call    GetModuleHandleW
    mov     rbx, rax                ; save hInstance (rbx = non-volatile)

    ; GetCommandLineW() → lpCmdLine
    call    GetCommandLineW

    ; WinMain(hInstance, NULL, lpCmdLine, SW_SHOWDEFAULT)
    mov     rcx, rbx                ; hInstance
    xor     rdx, rdx                ; hPrevInstance = NULL
    mov     r8, rax                 ; lpCmdLine
    mov     r9d, 10                 ; SW_SHOWDEFAULT
    call    WinMain

    ; ExitProcess(exitCode)
    mov     ecx, eax
    call    ExitProcess
WinMainCRTStartup ENDP

END
