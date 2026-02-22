; RawrXD Monolithic Kernel — Main Entry
; Assembles: ml64 /c /Fo main.obj main.asm
; Links: link main.obj inference.obj ui.obj beacon.obj lsp.obj agent.obj model_loader.obj ...
;
; Canonical bootstrap order:
;   1. HeapCreate            — 64MB arena
;   2. BeaconRouterInit      — ring buffers, slots 0-15
;   3. InferenceEngineInit   — AVX detection, KV cache
;   4. ModelLoaderInit       — X+4 SRWLOCK for hotswap
;   5. LSPBridgeInit         — stub (no child process)
;   6. AgentCoreInit         — registers agent, allocates task queue
;   7. DAP_Init              — debug adapter protocol engine
;   8. Test_Init             — test explorer tree + state
;   9. Task_Init             — task runner configs + ring buffer
;  10. UIMainLoop            — window + message pump (blocks until WM_QUIT)

EXTERN InferenceEngineInit:PROC
EXTERN UIMainLoop:PROC
EXTERN BeaconRouterInit:PROC
EXTERN AgentCoreInit:PROC
EXTERN LSPBridgeInit:PROC
EXTERN ModelLoaderInit:PROC
EXTERN DAP_Init:PROC
EXTERN Test_Init:PROC
EXTERN Task_Init:PROC
EXTERN HeapCreate:PROC
EXTERN GetModuleHandleW:PROC
EXTERN GetCommandLineW:PROC
EXTERN ExitProcess:PROC

PUBLIC WinMain
PUBLIC WinMainCRTStartup
PUBLIC g_hInstance
PUBLIC g_hHeap
PUBLIC g_cmdShow

.data
align 16
g_hInstance   dq 0
g_hHeap       dq 0
g_cmdShow     dd 0

.const
szClassName   db "RawrXD_Monolithic",0
szTitle       db "RawrXD IDE",0

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

    ; 1. HeapCreate(flags=0, initCommit=4MB, maxSize=64MB)
    xor     ecx, ecx
    mov     edx, 400000h
    mov     r8d, 4000000h
    call    HeapCreate
    test    rax, rax
    jz      @fail
    mov     g_hHeap, rax

    ; 2. Beacon first — all other modules signal through this
    call    BeaconRouterInit

    ; 3. Inference engine (depends on heap, uses beacon slot 2)
    call    InferenceEngineInit

    ; 4. LSP bridge (stub — no child process unless path provided)
    xor     rcx, rcx
    call    LSPBridgeInit

    ; 6. Agent core (depends on Inference + Beacon)
    call    AgentCoreInit

    ; 7. DAP engine (debug adapter protocol)
    call    DAP_Init

    ; 8. Test explorer (test discovery + execution)
    call    Test_Init

    ; 9. Task runner (task configs + process management)
    call    Task_Init

    ; 10. UI last — blocks on message pump, does not return until WM_QUIT
    call    UIMainLoop

    ; Normal exit
    xor     eax, eax
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
