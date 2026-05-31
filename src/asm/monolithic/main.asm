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
EXTERN ASTIndexer_Initialize:PROC
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
EXTERN InferenceRouter_Generate:PROC

; PE Writer — Final Directive (Non-Stubbed)

EXTERN Emit_DOSHeader:PROC
EXTERN Emit_NTHeaders:PROC
EXTERN Emit_SectionHeaders:PROC
EXTERN Emit_ImportTable:PROC
EXTERN Emit_RelocTable:PROC
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
EXTERN BeaconSend:PROC
EXTERN g_routerBackend:DWORD
EXTERN g_vocabLoaded:DWORD
EXTERN g_vocabCount:DWORD
EXTERN RunInference:PROC
EXTERN SubmitInference_Fixed:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapCreate:PROC
EXTERN GetModuleHandleW:PROC
EXTERN GetCommandLineW:PROC
EXTERN ExitProcess:PROC
EXTERN GetTickCount64:PROC
EXTERN WriteFile:PROC
EXTERN CreateFileA:PROC
EXTERN DeleteFileA:PROC
EXTERN FlushFileBuffers:PROC
EXTERN CloseHandle:PROC
EXTERN GetStdHandle:PROC
EXTERN WideCharToMultiByte:PROC
EXTERN CommandLineToArgvW:PROC
EXTERN lstrcmpiW:PROC
EXTERN RawrXD_RunExternalTestsW:PROC
EXTERN RawrXD_HealBuild:PROC
EXTERN Tool_Init:PROC
EXTERN Tool_Execute:PROC

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
g_agentPromptMode dd 0               ; 1 = --agent-prompt mode
g_pHealBuildCmd dq 0                 ; pointer to build command arg  (wide)
g_pHealBuildSrc dq 0                 ; pointer to source root arg    (wide)
g_testMode    dd 0                   ; 1 = --run-tests mode
g_pTestRunner dq 0                   ; pointer to --test-runner arg (wide)
g_pTestArgs   dq 0                   ; pointer to --test-args arg   (wide)

.data
align 8
g_benchBuf    db 256 dup(0)         ; output buffer for benchmark results
g_agentPromptUtf8 db 4096 dup(0)
g_agentRespBuf    db 4096 dup(0)
g_agentToolJson   db 4096 dup(0)
g_agentWriteCount dd 0
g_agentEmitPtr    dq 0
g_agentEmitLen    dd 0
g_agentExitCode   dd 0
g_agentFileHandle dq 0
g_agentPhase      db 'P','0',13,10,0

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
szAgentPrompt dw '-','-','a','g','e','n','t','-','p','r','o','m','p','t',0
; Default LSP server path (wide, can be overridden by --lsp <path>)
szLSPDefault  dw 'c','l','a','n','g','d','.','e','x','e',0
; Benchmark output template (narrow for WriteFile)
szBenchHdr    db "RawrXD Benchmark Results",13,10
              db "========================",13,10,0
szTPS         db "Tokens/sec: ",0
szBootTime    db "Boot-to-inference: ",0
szMs          db " ms",13,10,0
szNewline     db 13,10,0
szAgentEmpty  db '{"agent":"RawrXD","status":"degraded","reason":"empty_response","response":""}',0
szAgentEmptyNoVocabLoaded db '{"agent":"RawrXD","status":"degraded","reason":"vocab_loaded=0","response":""}',0
szAgentEmptyNoVocabCount  db '{"agent":"RawrXD","status":"degraded","reason":"vocab_count=0","response":""}',0
szAgentEmptyVocabReady    db '{"agent":"RawrXD","status":"ready","reason":"vocab_ready=1_empty","response":""}',0
szAgentFailed db "agent: failed",0
szAgentOutFile db "D:\\rawrxd\\rawrxd_agent_smoke.txt",0
szAgentPhaseFile db "D:\\rawrxd\\rawrxd_agent_phase.txt",0
szAgentPromptFallback dw 's','m','o','k','e',0

STD_OUTPUT_HANDLE equ -11
CP_UTF8           equ 65001
MAX_AGENT_IO_BYTES equ 4096
ROUTER_BEACON_SLOT equ 15
BACKEND_OLLAMA     equ 1
MAIN_EVT_VOCAB_LOADED equ 0D1h
MAIN_EVT_VOCAB_COUNT  equ 0D2h
GENERIC_WRITE      equ 40000000h
CREATE_ALWAYS      equ 2
FILE_ATTRIBUTE_NORMAL equ 80h

.code
; ────────────────────────────────────────────────────────────────
; WinMain — master init sequence
;   RCX = hInstance, RDX = hPrevInstance, R8 = lpCmdLine, R9D = nCmdShow
;   FRAME + shadow space for all callee calls.
; ────────────────────────────────────────────────────────────────
WinMain PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 38h
    .allocstack 38h
    .endprolog

    ; Save args into globals
    mov     g_hInstance, rcx
    mov     g_cmdShow, r9d
    mov     g_cmdLineW, r8          ; raw command line for ParseCommandLine

    ; Ultra-early startup marker for control-flow diagnostics.
    mov     ecx, 'W'
    call    AgentPhaseStamp

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

    ; Fallback: detect --agent-prompt in raw command line when argv parsing misses it.
    cmp     g_agentPromptMode, 0
    jne     @agent_mode_ready
    mov     rsi, g_cmdLineW
    test    rsi, rsi
    jz      @agent_mode_ready
    xor     edx, edx                ; match index into szAgentPrompt
@scan_agent_flag:
    mov     ax, word ptr [rsi]
    test    ax, ax
    je      @agent_mode_ready

    mov     cx, word ptr [szAgentPrompt + rdx*2]
    cmp     ax, cx
    je      @scan_match_advance

    mov     cx, word ptr [szAgentPrompt]
    cmp     ax, cx
    jne     @scan_match_reset
    mov     edx, 1
    jmp     @scan_step

@scan_match_reset:
    xor     edx, edx
    jmp     @scan_step

@scan_match_advance:
    inc     edx
    cmp     edx, 14                 ; len("--agent-prompt")
    jne     @scan_step
    mov     g_agentPromptMode, 1
    jmp     @agent_mode_ready

@scan_step:
    add     rsi, 2
    jmp     @scan_agent_flag

@agent_mode_ready:
    cmp     g_agentPromptMode, 0
    je      @agent_prompt_seed_done
    cmp     qword ptr [g_pPrompt], 0
    jne     @agent_prompt_seed_done
    lea     rax, szAgentPromptFallback
    mov     g_pPrompt, rax
@agent_prompt_seed_done:

    ; 17. Branch: build mode, benchmark mode, or interactive UI
    cmp     g_buildMode, 0
    jne     @build_self

    cmp     g_stressMode, 0
    jne     @stress_test

    cmp     g_testMode, 0
    jne     @run_tests

    cmp     g_healBuildMode, 0
    jne     @heal_build

    cmp     g_agentPromptMode, 0
    jne     @agent_prompt

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
    call    ASTIndexer_Initialize
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
    call    GetTickCount64
    mov     g_benchStart, rax

    ; If --model supplied, demand-page it
    mov     rcx, g_pModelPath
    test    rcx, rcx
    jz      @bench_infer
    call    StreamMapModel

@bench_infer:
    ; Headless safe mode: benchmark currently measures startup-only elapsed time.

@bench_done:

    ; Elapsed = GetTickCount64() - start
    call    GetTickCount64
    sub     rax, g_benchStart       ; rax = elapsed ms
    ; Exit with elapsed as exit code (useful for scripted bench)
    mov     ecx, eax
    call    ExitProcess

    ; ── Direct agent prompt mode (--agent-prompt --prompt <text>) ─────────
@agent_prompt:
    mov     r8, g_pPrompt
    test    r8, r8
    jz      @agent_fail

    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     g_hStdOut, rax

    mov     ecx, '1'
    call    AgentPhaseStamp

    ; Keep headless lane on the stable tool/react runtime boundary.
    ; Heavy inference/router startup is intentionally skipped here because
    ; direct startup has been unstable in agent-prompt smoke mode.

    ; If --model supplied, map it before generate.
    mov     rcx, g_pModelPath
    test    rcx, rcx
    jz      @agent_force_ollama
    call    StreamMapModel
    jmp     @agent_generate

@agent_force_ollama:
    mov     dword ptr [g_routerBackend], BACKEND_OLLAMA

@agent_generate:
    mov     ecx, '2'
    call    AgentPhaseStamp

    mov     rcx, g_pPrompt
    lea     rdx, g_agentRespBuf
    mov     r8d, MAX_AGENT_IO_BYTES - 1
    call    AgentEntry_Safe
    test    eax, eax
    jl      @agent_fail
    jz      @agent_empty
    cmp     byte ptr [g_agentRespBuf], 0
    je      @agent_empty
    mov     ebx, eax
    jmp     @agent_write

@agent_write:
    mov     rcx, g_hStdOut
    lea     rdx, g_agentRespBuf
    mov     r8d, ebx
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    mov     rcx, g_hStdOut
    lea     rdx, szNewline
    mov     r8d, 2
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    xor     eax, eax
    lea     rdx, g_agentRespBuf
    mov     r8d, ebx
    jmp     @agent_emit_file

@agent_empty:
    mov     ecx, 'E'
    call    AgentPhaseStamp

    ; Last-chance dispatch: route through bridge agent runtime before degraded output.
    lea     rcx, g_agentPromptUtf8
    xor     edx, edx
    lea     r8,  g_agentRespBuf
    mov     r9d, MAX_AGENT_IO_BYTES - 1
    call    SubmitInference_Fixed
    test    eax, eax
    jne     @agent_empty_fallback
    cmp     byte ptr [g_agentRespBuf], 0
    je      @agent_empty_fallback
    xor     ebx, ebx
@agent_retry_len:
    cmp     byte ptr [g_agentRespBuf + rbx], 0
    je      @agent_write
    inc     ebx
    cmp     ebx, MAX_AGENT_IO_BYTES - 1
    jb      @agent_retry_len

@agent_empty_fallback:
    lea     rdx, szAgentEmpty
    mov     r8d, SIZEOF szAgentEmpty - 1
    cmp     g_vocabLoaded, 0
    jne     @agent_empty_check_count
    lea     rdx, szAgentEmptyNoVocabLoaded
    mov     r8d, SIZEOF szAgentEmptyNoVocabLoaded - 1
    jmp     @agent_empty_emit

@agent_empty_check_count:
    cmp     g_vocabCount, 0
    jne     @agent_empty_vocab_ready
    lea     rdx, szAgentEmptyNoVocabCount
    mov     r8d, SIZEOF szAgentEmptyNoVocabCount - 1
    jmp     @agent_empty_emit

@agent_empty_vocab_ready:
    lea     rdx, szAgentEmptyVocabReady
    mov     r8d, SIZEOF szAgentEmptyVocabReady - 1

@agent_empty_emit:
    mov     qword ptr [g_agentEmitPtr], rdx
    mov     dword ptr [g_agentEmitLen], r8d

    mov     rcx, g_hStdOut
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    mov     rcx, g_hStdOut
    lea     rdx, szNewline
    mov     r8d, 2
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    xor     eax, eax
    mov     rdx, qword ptr [g_agentEmitPtr]
    mov     r8d, dword ptr [g_agentEmitLen]
    jmp     @agent_emit_file

@agent_fail:
    mov     ecx, 'X'
    call    AgentPhaseStamp

    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     g_hStdOut, rax

    mov     rcx, g_hStdOut
    lea     rdx, szAgentFailed
    mov     r8d, 13
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    mov     rcx, g_hStdOut
    lea     rdx, szNewline
    mov     r8d, 2
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    xor     eax, eax
    lea     rdx, szAgentFailed
    mov     r8d, 13
    jmp     @agent_emit_file

@agent_emit_file:
    mov     qword ptr [g_agentEmitPtr], rdx
    mov     dword ptr [g_agentEmitLen], r8d
    mov     dword ptr [g_agentExitCode], eax

    ; Force a fresh file timestamp/write path for smoke harness.
    lea     rcx, szAgentOutFile
    call    DeleteFileA

    lea     rcx, szAgentOutFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], CREATE_ALWAYS
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+30h], 0
    call    CreateFileA
    cmp     rax, -1
    jne     @agent_emit_opened
    mov     dword ptr [g_agentExitCode], 2
    jmp     @agent_emit_done

@agent_emit_opened:

    mov     qword ptr [g_agentFileHandle], rax
    mov     rcx, rax
    mov     rdx, qword ptr [g_agentEmitPtr]
    mov     r8d, dword ptr [g_agentEmitLen]
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    test    eax, eax
    jne     @agent_emit_flush
    mov     dword ptr [g_agentExitCode], 3
    jmp     @agent_emit_close

@agent_emit_flush:
    mov     rcx, qword ptr [g_agentFileHandle]
    call    FlushFileBuffers

    mov     eax, dword ptr [g_agentWriteCount]
    cmp     eax, dword ptr [g_agentEmitLen]
    je      @agent_emit_close
    mov     dword ptr [g_agentExitCode], 4

@agent_emit_close:

    mov     rcx, qword ptr [g_agentFileHandle]
    call    CloseHandle

@agent_emit_done:
    mov     eax, dword ptr [g_agentExitCode]
    jmp     @exit

@exit:
    lea     rsp, [rbp]
    pop     r12
    pop     rsi
    pop     rbx
    pop     rbp
    ret
@fail:
    mov     eax, 1
    lea     rsp, [rbp]
    pop     r12
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WinMain ENDP

; Writes a single checkpoint marker to a sidecar file for crash-boundary triage.
; CL = phase code byte
AgentPhaseStamp PROC FRAME
    push    rbx
    .pushreg rbx
    push    rbp
    .pushreg rbp
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    ; Normalize stack alignment for WinAPI calls regardless of caller alignment.
    mov     rbp, rsp
    and     rsp, -16
    sub     rsp, 40h

    mov     byte ptr [g_agentPhase + 1], cl

    lea     rcx, szAgentPhaseFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], CREATE_ALWAYS
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+30h], 0
    call    CreateFileA
    cmp     rax, -1
    je      @aps_done

    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, g_agentPhase
    mov     r8d, 4
    lea     r9, g_agentWriteCount
    mov     qword ptr [rsp+20h], 0
    call    WriteFile

    mov     rcx, rbx
    call    CloseHandle

@aps_done:
    mov     rsp, rbp
    add     rsp, 20h
    pop     rbp
    pop     rbx
    ret
AgentPhaseStamp ENDP

; Headless-safe inference entry: converts UTF-16 prompt to UTF-8 and routes
; through the inference router using a stable call boundary.
AgentEntry_Safe PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48h
    .allocstack 48h
    .endprolog

    mov     rbx, rcx                            ; prompt (UTF-16)
    mov     rsi, rdx                            ; output buffer
    mov     rdi, r8                             ; output size

    mov     ecx, '3'
    call    AgentPhaseStamp

    test    rbx, rbx
    jz      @aes_fail
    test    rsi, rsi
    jz      @aes_fail
    test    rdi, rdi
    jz      @aes_fail

    mov     ecx, CP_UTF8                        ; CodePage
    xor     edx, edx                            ; dwFlags
    mov     r8,  rbx                            ; lpWideCharStr
    mov     r9d, -1                             ; cchWideChar = null-terminated
    lea     rax, g_agentPromptUtf8
    mov     qword ptr [rsp+20h], rax            ; lpMultiByteStr
    mov     dword ptr [rsp+28h], MAX_AGENT_IO_BYTES - 1 ; cbMultiByte
    mov     qword ptr [rsp+30h], 0              ; lpDefaultChar
    mov     qword ptr [rsp+38h], 0              ; lpUsedDefaultChar
    call    WideCharToMultiByte
    test    eax, eax
    jle     @aes_fail

    mov     ecx, '4'
    call    AgentPhaseStamp

    ; Direct headless tool lane: "Read <path>" -> Tool_Execute(read_file)
    mov     ecx, '5'
    call    AgentPhaseStamp
    call    Tool_Init
    mov     ecx, '6'
    call    AgentPhaseStamp

    lea     r10, g_agentPromptUtf8
    mov     al, byte ptr [r10 + 0]
    cmp     al, 'R'
    je      @aes_check_read_1
    cmp     al, 'r'
    jne     @aes_fallback_submit
@aes_check_read_1:
    mov     al, byte ptr [r10 + 1]
    cmp     al, 'e'
    jne     @aes_fallback_submit
    mov     al, byte ptr [r10 + 2]
    cmp     al, 'a'
    jne     @aes_fallback_submit
    mov     al, byte ptr [r10 + 3]
    cmp     al, 'd'
    jne     @aes_fallback_submit
    mov     al, byte ptr [r10 + 4]
    cmp     al, ' '
    jne     @aes_fallback_submit

    lea     r11, g_agentToolJson
    mov     byte ptr [r11 + 0], '{'
    mov     byte ptr [r11 + 1], '"'
    mov     byte ptr [r11 + 2], 'p'
    mov     byte ptr [r11 + 3], 'a'
    mov     byte ptr [r11 + 4], 't'
    mov     byte ptr [r11 + 5], 'h'
    mov     byte ptr [r11 + 6], '"'
    mov     byte ptr [r11 + 7], ':'
    mov     byte ptr [r11 + 8], '"'

    lea     r10, [r10 + 5]
    mov     r9d, 9
@aes_copy_path:
    cmp     r9d, 4092
    jae     @aes_finish_json
    mov     al, byte ptr [r10]
    test    al, al
    jz      @aes_finish_json
    cmp     al, '"'
    je      @aes_finish_json
    mov     byte ptr [r11 + r9], al
    inc     r10
    inc     r9d
    jmp     @aes_copy_path

@aes_finish_json:
    mov     byte ptr [r11 + r9], '"'
    inc     r9d
    mov     byte ptr [r11 + r9], '}'
    inc     r9d
    mov     byte ptr [r11 + r9], 0

    mov     ecx, '7'
    call    AgentPhaseStamp

    xor     ecx, ecx
    lea     rdx, g_agentToolJson
    mov     r8,  rsi
    mov     r9,  rdi
    call    Tool_Execute
    test    eax, eax
    jg      @aes_tool_ok

@aes_fallback_submit:
    mov     ecx, '9'
    call    AgentPhaseStamp

    lea     rcx, g_agentPromptUtf8
    xor     edx, edx
    mov     r8,  rsi
    mov     r9,  rdi
    call    SubmitInference_Fixed
    test    eax, eax
    jl      @aes_fail

    mov     ecx, 'A'
    call    AgentPhaseStamp

    xor     eax, eax
    jmp     @aes_len_loop

@aes_tool_ok:
    mov     ecx, '8'
    call    AgentPhaseStamp
    xor     eax, eax            ; AgentPhaseStamp (CloseHandle) clobbered eax; reset scan to byte 0

@aes_len_loop:
    cmp     byte ptr [rsi + rax], 0
    je      @aes_ret
    inc     eax
    cmp     eax, edi
    jb      @aes_len_loop
    jmp     @aes_ret

@aes_fail:
    mov     ecx, 'F'
    call    AgentPhaseStamp
    mov     eax, -1

@aes_ret:
    mov     ecx, 'R'
    call    AgentPhaseStamp

    add     rsp, 48h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AgentEntry_Safe ENDP

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
    ; ── try --agent-prompt ───────────────────────
    mov     rcx, r12
    lea     rdx, [szAgentPrompt]
    call    lstrcmpiW
    test    eax, eax
    jz      @pcl_agent_prompt
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

@pcl_agent_prompt:
    mov     g_agentPromptMode, 1
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

    ; Entry marker before any startup API calls.
    mov     ecx, 'C'
    call    AgentPhaseStamp

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
