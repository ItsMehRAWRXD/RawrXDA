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
;  15. CLI parse             — --bench, --model, --prompt, --build flags
;  16. UIMainLoop            — window + message pump (blocks until WM_QUIT)
;      OR --build            — WritePEFile + SavePEToDisk (sovereign PE emit)

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
EXTERN SwarmNet_Init:PROC
EXTERN Consensus_Init:PROC
EXTERN Batch_Init:PROC
EXTERN StressTest_Run:PROC
EXTERN StressTest_LogStats:PROC
EXTERN StreamLoaderInit:PROC
EXTERN StreamMapModel:PROC
EXTERN WebView2Init:PROC
EXTERN ExtHostInit:PROC
EXTERN Mesh_Init:PROC

; PE Writer — Final Directive (Non-Stubbed)

EXTERN Emit_DOSHeader:PROC
EXTERN Emit_NTHeaders:PROC
EXTERN Emit_SectionHeaders:PROC
EXTERN Emit_ImportTable:PROC
EXTERN Emit_RelocTable:PROC
EXTERN Emit_Payload:PROC
EXTERN SavePEToDisk:PROC
EXTERN WritePEFile:PROC
EXTERN g_peBuffer:QWORD
EXTERN g_cursor:QWORD
EXTERN g_peSize:QWORD        ; PE file byte count — set by Emit_* path or WritePEFile

; Phase 9B: Async pager
EXTERN AsyncPage_Init:PROC
EXTERN AsyncPage_Shutdown:PROC

; Phase 9C: Ollama inference client
EXTERN OllamaClient_Init:PROC
EXTERN OllamaClient_Shutdown:PROC

; Phase A: Inference Router (sovereign backend selection)
EXTERN InferenceRouter_Init:PROC
EXTERN RunInference:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapCreate:PROC
EXTERN GetModuleHandleW:PROC
EXTERN GetCommandLineW:PROC
EXTERN ExitProcess:PROC
EXTERN GetTickCount64:PROC
EXTERN WriteFile:PROC
EXTERN GetStdHandle:PROC
EXTERN CommandLineToArgvW:PROC
EXTERN lstrcmpiW:PROC
EXTERN RawrXD_RunExternalTestsW:PROC
EXTERN RawrXD_HealBuild:PROC

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
g_buildMode   dd 0                  ; 1 = --build mode (PE writer)
g_pModelPath  dq 0                  ; pointer to --model argument (wide)
g_pPrompt     dq 0                  ; pointer to --prompt argument (wide)
g_pLSPPath    dq 0                  ; pointer to --lsp argument (wide), defaults to szLSPDefault
g_cmdLineW    dq 0                  ; raw GetCommandLineW result
g_benchStart  dq 0                  ; GetTickCount64 at bench start
g_benchTokens dd 0                  ; tokens generated in bench
g_hStdOut     dq 0                  ; stdout handle for bench output
g_multiNode   dd 0                  ; 1 = --multi-node mode
g_stressMode  dd 0                  ; 1 = --stress mode
g_healBuildMode dd 0                 ; 1 = --heal-build mode
g_pHealBuildCmd dq 0                 ; pointer to build command arg  (wide)
g_pHealBuildSrc dq 0                 ; pointer to source root arg    (wide)
g_testMode    dd 0                   ; 1 = --run-tests mode
g_pTestRunner dq 0                   ; pointer to --test-runner arg (wide)
g_pTestArgs   dq 0                   ; pointer to --test-args arg   (wide)

.data
align 8
g_benchBuf    db 256 dup(0)         ; output buffer for benchmark results

.const
szClassName   db "RawrXD_Monolithic",0
szTitle       db "RawrXD IDE",0
; CLI flag strings (wide for wcscmp)
szBench       dw '-','-','b','e','n','c','h',0
szBenchmark   dw '-','-','b','e','n','c','h','m','a','r','k',0
szModel       dw '-','-','m','o','d','e','l',0
szPrompt      dw '-','-','p','r','o','m','p','t',0
szNodes       dw '-','-','m','u','l','t','i','-','n','o','d','e',0
szBuild       dw '-','-','b','u','i','l','d',0
szStress      dw '-','-','s','t','r','e','s','s',0
szLSP         dw '-','-','l','s','p',0
szRunTests    dw '-','-','r','u','n','-','t','e','s','t','s',0
szTestRunner  dw '-','-','t','e','s','t','-','r','u','n','n','e','r',0
szTestArgs    dw '-','-','t','e','s','t','-','a','r','g','s',0
szAutofix     dw '-','-','a','u','t','o','f','i','x',0
szHealBuild   dw '-','-','h','e','a','l','-','b','u','i','l','d',0
szBuildCmd    dw '-','-','b','u','i','l','d','-','c','o','m','m','a','n','d',0
szWorkspace   dw '-','-','w','o','r','k','s','p','a','c','e','-','r','o','o','t',0
; Default LSP server path (wide, can be overridden by --lsp <path>)
szLSPDefault  dw 'c','l','a','n','g','d','.','e','x','e',0
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

    ; Parse CLI before any optional subsystem startup.
    ; Interactive mode stays on the minimal stable path and only
    ; explicit headless modes initialize heavier subsystems.
    call    ParseCommandLine

    ; 17. Branch: build mode, benchmark mode, or interactive UI
    cmp     g_buildMode, 0
    jne     @build_self

    cmp     g_stressMode, 0
    jne     @stress_test

    cmp     g_testMode, 0
    jne     @run_tests

    cmp     g_healBuildMode, 0
    jne     @heal_build

    cmp     g_benchMode, 0
    jne     @benchmark

    ; ── Interactive mode ──────────────────────────────────────────
    ; Bring up full IDE subsystems on the canonical runtime path.
    ; This keeps ghost-text + router + debug/lsp/task surfaces aligned
    ; with parity expectations instead of UI-only startup.
    call    BeaconRouterInit
    call    InferenceEngineInit
    call    StreamLoaderInit
    call    ModelLoaderInit
    call    InferenceRouter_Init
    call    AgentCoreInit
    call    LSPBridgeInit
    call    DAP_Init
    call    Test_Init
    call    Task_Init
    call    ExtHostInit
    call    WebView2Init
    call    UIMainLoop
    xor     eax, eax
    jmp     @exit

    ; ── Stress test mode (--stress) ───────────────────────────
@stress_test:
    call    SwarmNet_Init
    call    Consensus_Init
    call    Mesh_Init
    call    StressTest_Run
    call    StressTest_LogStats
    xor     eax, eax
    jmp     @exit

    ; ── External test runner mode (--run-tests) ───────────────
@run_tests:
    mov     rcx, g_pTestRunner
    mov     rdx, g_pTestArgs
    call    RawrXD_RunExternalTestsW
    jmp     @exit

    ; ── Build-self mode (--build) ──────────────────────────────
@build_self:
    call    WritePEFile
    test    rax, rax
    jz      @fail
    call    SavePEToDisk
    test    eax, eax
    jz      @fail
    xor     eax, eax
    jmp     @exit

    ; ── Heal-build mode (--autofix / --heal-build) ────────────
@heal_build:
    call    BeaconRouterInit
    call    InferenceEngineInit
    call    StreamLoaderInit
    call    ModelLoaderInit
    call    InferenceRouter_Init
    call    AgentCoreInit

    ; Load model if --model was supplied
    mov     rcx, g_pModelPath
    test    rcx, rcx
    jz      @hb_no_model
    call    StreamMapModel
@hb_no_model:

    ; Attempt build, diagnose errors, apply fix — up to 3 attempts
    mov     r12d, 3                      ; max attempts
@hb_retry:
    mov     rcx, g_pHealBuildCmd         ; build command (wide)
    mov     rdx, g_pHealBuildSrc         ; workspace root (wide)
    call    RawrXD_HealBuild
    test    eax, eax
    jz      @hb_done                     ; 0 = success
    dec     r12d
    jnz     @hb_retry
    mov     eax, 1                       ; exhausted retries
@hb_done:
    jmp     @exit

    ; ── Benchmark mode ────────────────────────────────────────
@benchmark:
    call    InferenceEngineInit
    call    StreamLoaderInit
    call    ModelLoaderInit
    call    InferenceRouter_Init
    call    GetTickCount64
    mov     g_benchStart, rax

    ; If --model supplied, demand-page it
    mov     rcx, g_pModelPath
    test    rcx, rcx
    jz      @bench_infer
    call    StreamMapModel

@bench_infer:
    mov     rcx, g_pPrompt               ; Pass --prompt text (or NULL)
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

    ; ── try --build ───────────────────────────
    mov     rcx, r12
    lea     rdx, [szBuild]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_build

    ; ── try --stress ──────────────────────────
    mov     rcx, r12
    lea     rdx, [szStress]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_stress

    ; ── try --run-tests ───────────────────────
    mov     rcx, r12
    lea     rdx, [szRunTests]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_run_tests

    ; ── try --test-runner ─────────────────────
    mov     rcx, r12
    lea     rdx, [szTestRunner]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_test_runner

    ; ── try --test-args ───────────────────────
    mov     rcx, r12
    lea     rdx, [szTestArgs]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_test_args

    ; ── try --lsp ─────────────────────────────
    mov     rcx, r12
    lea     rdx, [szLSP]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_lsp
    ; ── try --autofix ─────────────────────────────────────────
    mov     rcx, r12
    lea     rdx, [szAutofix]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_autofix

    ; ── try --heal-build ──────────────────────────────────────
    mov     rcx, r12
    lea     rdx, [szHealBuild]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_autofix

    ; ── try --build-command ───────────────────────────────────
    mov     rcx, r12
    lea     rdx, [szBuildCmd]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_build_cmd

    ; ── try --workspace-root ──────────────────────────────────
    mov     rcx, r12
    lea     rdx, [szWorkspace]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_workspace
    inc     ebx
    jmp     @pcl_loop

@pcl_bench:
    mov     g_benchMode, 1
    inc     ebx
    jmp     @pcl_loop

@pcl_stress:
    mov     g_stressMode, 1
    inc     ebx
    jmp     @pcl_loop

@pcl_run_tests:
    mov     g_testMode, 1
    inc     ebx
    jmp     @pcl_loop

@pcl_test_runner:
    mov     g_testMode, 1
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pTestRunner, rax
    inc     ebx
    jmp     @pcl_loop

@pcl_test_args:
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pTestArgs, rax
    inc     ebx
    jmp     @pcl_loop

@pcl_build:
    mov     g_buildMode, 1
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

@pcl_autofix:
    mov     g_healBuildMode, 1
    inc     ebx
    jmp     @pcl_loop

@pcl_build_cmd:
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pHealBuildCmd, rax
    inc     ebx
    jmp     @pcl_loop

@pcl_workspace:
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pHealBuildSrc, rax
    inc     ebx
    jmp     @pcl_loop

@pcl_lsp:
    inc     ebx
    cmp     ebx, edi
    jge     @pcl_done
    mov     rax, [rsi + rbx*8]
    mov     g_pLSPPath, rax
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
