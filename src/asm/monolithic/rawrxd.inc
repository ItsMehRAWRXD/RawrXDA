; ═══════════════════════════════════════════════════════════════════
; rawrxd.inc — RawrXD Monolithic Kernel Unified Header
; ML64-only. No NASM, no FASM, no INVOKE, no sizeof, no addr.
; ═══════════════════════════════════════════════════════════════════
; Usage: INCLUDE rawrxd.inc  (at top of each monolithic module)
; This file is OPTIONAL — each module can declare inline instead.
; ═══════════════════════════════════════════════════════════════════

; ── Win32 API imports ────────────────────────────────────────────
; Kernel32
EXTERN GetModuleHandleW:PROC
EXTERN GetCommandLineW:PROC
EXTERN ExitProcess:PROC
EXTERN HeapCreate:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateFileW:PROC
EXTERN CreateFileMappingW:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CreatePipe:PROC
EXTERN CreateProcessW:PROC
EXTERN GetLastError:PROC

; User32
EXTERN RegisterClassExW:PROC
EXTERN CreateWindowExW:PROC
EXTERN GetMessageW:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageW:PROC
EXTERN DefWindowProcW:PROC
EXTERN PostQuitMessage:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN DestroyWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN GetClientRect:PROC
EXTERN FillRect:PROC
EXTERN SetScrollInfo:PROC
EXTERN GetScrollInfo:PROC

; Caret (user32)
EXTERN CreateCaret:PROC
EXTERN SetCaretPos:PROC
EXTERN ShowCaret:PROC
EXTERN DestroyCaret:PROC
EXTERN HideCaret:PROC

; GDI32
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN TextOutW:PROC
EXTERN SetTextColor:PROC
EXTERN SetBkColor:PROC
EXTERN SetBkMode:PROC
EXTERN CreateFontIndirectW:PROC
EXTERN SelectObject:PROC
EXTERN DeleteObject:PROC
EXTERN GetStockObject:PROC

; ── RawrXD cross-module exports ──────────────────────────────────
; main.asm
EXTERN g_hInstance:QWORD
EXTERN g_hHeap:QWORD
EXTERN g_cmdShow:DWORD

; beacon.asm
EXTERN BeaconRouterInit:PROC
EXTERN BeaconSend:PROC
EXTERN BeaconRecv:PROC
EXTERN RegisterAgent:PROC

; inference.asm
EXTERN InferenceEngineInit:PROC
EXTERN RunInference:PROC
EXTERN TokenGenerate:PROC
EXTERN ClearKVCache:PROC
<<<<<<< HEAD
EXTERN PruneKVCache:PROC
EXTERN UpdateImportanceScores:PROC
EXTERN GetTopKIndices:PROC
EXTERN g_modelbase:QWORD
EXTERN g_kv_len:QWORD

; inference.asm — Phase 7: KV-Cache Paging via SlotRing
EXTERN KVPage_Init:PROC
EXTERN KVPage_Grow:PROC
EXTERN KVPage_GetSegment:PROC
EXTERN KVPage_Shrink:PROC
EXTERN KVPage_PinWorkingSet:PROC

; inference.asm — Phase 8B: Paged KV Block Table (PagedAttention)
EXTERN KVBlock_Init:PROC
EXTERN KVBlock_AllocBlock:PROC
EXTERN KVBlock_FreeBlock:PROC
EXTERN KVBlock_MapToken:PROC
EXTERN KVBlock_GetTokenPtr:PROC
EXTERN KVBlock_ReuseBlock:PROC

; inference.asm — Phase 8C: Head-Major KV Layout
EXTERN KVHM_WriteToken:PROC
EXTERN KVHM_GetHeadPtr:PROC
=======
EXTERN g_modelbase:QWORD
>>>>>>> origin/main

; ui.asm
EXTERN UIMainLoop:PROC
EXTERN CreateEditorPane:PROC
EXTERN hMainWnd:QWORD

; agent.asm
EXTERN AgentCoreInit:PROC
EXTERN SpawnTask:PROC

; lsp.asm
EXTERN LSPBridgeInit:PROC
EXTERN LSPSendRequest:PROC
EXTERN LSPBridgeShutdown:PROC

; model_loader.asm
EXTERN LoadModel:PROC
EXTERN GetTensor:PROC
EXTERN UnloadModel:PROC
EXTERN ModelLoaderInit:PROC
EXTERN HotSwapModel:PROC
EXTERN GetCurrentModelPath:PROC
EXTERN GetModelLoadTimestamp:PROC
EXTERN ValidateModelCompat:PROC

; beacon.asm (X+4)
EXTERN TryBeaconRecv:PROC

; agent.asm (X+4)
EXTERN ProcessOneAgentMessage:PROC

; dap.asm (X+5 LSP subsystem)
EXTERN DAP_Init:PROC
EXTERN DAP_Launch:PROC
EXTERN DAP_Step:PROC
EXTERN DAP_StackTrace:PROC
EXTERN DAP_Evaluate:PROC

; testing.asm (X+5 LSP subsystem)
EXTERN Test_Init:PROC
EXTERN Test_AddNode:PROC
EXTERN Test_Discover:PROC
EXTERN Test_Run:PROC
EXTERN Test_GetResults:PROC

; tasks.asm (X+5 LSP subsystem)
EXTERN Task_Init:PROC
EXTERN Task_Run:PROC
EXTERN Task_Kill:PROC
EXTERN Task_GetOutput:PROC
EXTERN Task_GetCount:PROC

; swarm.asm (X+5 Distributed Swarm)
EXTERN Swarm_Init:PROC
EXTERN Swarm_EnumGPUs:PROC
EXTERN Swarm_AllocShard:PROC
EXTERN Swarm_DispatchCompute:PROC
EXTERN Swarm_P2PCopy:PROC
EXTERN Swarm_Shutdown:PROC
EXTERN Swarm_GetDeviceCount:PROC
EXTERN g_swarmDeviceCount:DWORD
EXTERN g_swarmReady:DWORD

; swarm_coordinator.asm (X+5 Distributed Swarm)
EXTERN SwarmCoord_Init:PROC
EXTERN SwarmCoord_DistributeWork:PROC
EXTERN SwarmCoord_AllReduce:PROC
EXTERN SwarmCoord_GetWorkerStatus:PROC
EXTERN SwarmCoord_Shutdown:PROC
EXTERN g_coordReady:DWORD
EXTERN g_workerCount:DWORD

<<<<<<< HEAD
; swarm_network.asm (X+5 Sovereign Expansion → Phase 14 Cross-Node)
EXTERN SwarmNet_Init:PROC
EXTERN SwarmNet_Listen:PROC
EXTERN SwarmNet_Connect:PROC
EXTERN SwarmNet_BroadcastDiscovery:PROC
EXTERN SwarmNet_SendShard:PROC
EXTERN SwarmNet_RecvShard:PROC
EXTERN SwarmNet_SyncKVCache:PROC
EXTERN SwarmNet_ExchangeLoadInfo:PROC
EXTERN SwarmNet_Shutdown:PROC
EXTERN g_netReady:DWORD
EXTERN g_remoteCount:DWORD

; swarm_consensus.asm (X+5 Sovereign Expansion)
EXTERN Consensus_Init:PROC
EXTERN Consensus_Propose:PROC
EXTERN Consensus_Vote:PROC
EXTERN Consensus_GetState:PROC
EXTERN g_consensusTerm:QWORD
EXTERN g_consensusLeader:DWORD

; batch_decoder.asm (X+5 Parallel Throughput)
EXTERN Batch_Init:PROC
EXTERN Batch_AddSlot:PROC
EXTERN Batch_Step:PROC
EXTERN Batch_GetOutput:PROC

; work_steal.asm (Phase 14: Autonomous Shard Migration)
EXTERN WorkSteal_Init:PROC
EXTERN WorkSteal_IdleProbe:PROC
EXTERN WorkSteal_AtomicClaim:PROC
EXTERN WorkSteal_MigrateShard:PROC
EXTERN WorkSteal_GetStats:PROC
EXTERN WorkSteal_SelectVictim:PROC
EXTERN WorkSteal_Shutdown:PROC
EXTERN WorkSteal_CrossNodeProbe:PROC
EXTERN WorkSteal_ExchangeAllLoads:PROC
EXTERN g_stealReady:DWORD
EXTERN g_totalSteals:QWORD
EXTERN g_totalMigrations:QWORD
EXTERN g_stealFailures:QWORD
EXTERN g_crossNodeSteals:QWORD

=======
>>>>>>> origin/main
; stream_loader.asm — Tier 1: demand-paged GGUF VMM
EXTERN StreamLoaderInit:PROC
EXTERN StreamMapModel:PROC
EXTERN StreamUnmap:PROC
EXTERN StreamPrefetch:PROC
EXTERN StreamEvictCold:PROC
EXTERN StreamGetStats:PROC

; stream_loader.asm — Tier 2: per-tensor slot-ring paging (200B+)
EXTERN SlotRing_Init:PROC
EXTERN SlotRing_Destroy:PROC
EXTERN SlotRing_Attach:PROC
EXTERN SlotRing_Detach:PROC
EXTERN SlotRing_GetTensor:PROC
EXTERN SlotRing_Tick:PROC
EXTERN SlotRing_SetBudget:PROC
EXTERN SlotRing_GetSlotStats:PROC
EXTERN SlotRing_EvictAll:PROC

<<<<<<< HEAD
; slot_ring.asm — Phase 6: Prefetch + Pin + Tiered Eviction
EXTERN SlotRing_Pin:PROC
EXTERN SlotRing_Unpin:PROC
EXTERN SlotRing_Prefetch:PROC
EXTERN SlotRing_BatchEvict:PROC
EXTERN SlotRing_PredictivePrefetch:PROC
EXTERN SlotRing_ClockEvict:PROC

=======
>>>>>>> origin/main
; webview2_shell.asm (P1 — WebView2 Monaco embedding)
EXTERN WebView2Init:PROC
EXTERN WebView2Navigate:PROC
EXTERN WebView2PostMessage:PROC
EXTERN WebView2Resize:PROC
EXTERN WebView2IsReady:PROC
EXTERN WebView2Shutdown:PROC
EXTERN WebView2CreateEnvironment:PROC
EXTERN WebView2NavigateToString:PROC
EXTERN WebView2ExecuteScript:PROC

; extension_host.asm (P2 — sandboxed DLL extension loader)
EXTERN ExtHostInit:PROC
EXTERN ExtHostLoad:PROC
EXTERN ExtHostUnload:PROC
EXTERN ExtHostSendMessage:PROC
EXTERN ExtHostGetCount:PROC
EXTERN ExtHostShutdown:PROC

; main.asm (CLI globals)
EXTERN g_benchMode:DWORD
EXTERN g_pModelPath:QWORD
EXTERN g_pPrompt:QWORD

<<<<<<< HEAD
EXTERN SlotRing_GetSlotStats:PROC
EXTERN SlotRing_EvictAll:PROC

; stress_test.asm (X+5 Sovereign Expansion)
EXTERN StressTest_Run:PROC
EXTERN StressTest_LogStats:PROC

; ── Phase 9A: SIMD/AVX-512 Attention Kernels (simd_kernels.asm) ──
EXTERN SIMD_RMSNorm:PROC
EXTERN SIMD_Softmax:PROC
EXTERN SIMD_DotProduct:PROC
EXTERN SIMD_ScaledDotBatch:PROC
EXTERN SIMD_VAccumulate:PROC
EXTERN SIMD_MatVecQ4:PROC
EXTERN SIMD_RoPE:PROC
EXTERN SIMD_SiLU:PROC

; ── Phase 9B: Async Pager Thread (async_pager.asm) ──
EXTERN AsyncPage_Init:PROC
EXTERN AsyncPage_Shutdown:PROC
EXTERN AsyncPage_Submit:PROC
EXTERN AsyncPage_Poll:PROC
EXTERN AsyncPage_Flush:PROC
EXTERN AsyncPage_GetStats:PROC

; ── Phase 9C: Ollama Inference Client (ollama_client.asm) ──
EXTERN OllamaClient_Init:PROC
EXTERN OllamaClient_Generate:PROC
EXTERN OllamaClient_Chat:PROC
EXTERN OllamaClient_Abort:PROC
EXTERN OllamaClient_SetModel:PROC
EXTERN OllamaClient_Shutdown:PROC
EXTERN OllamaClient_IsConnected:PROC

; ── Phase A: Inference Router (inference_router.asm) ──
; Sovereign backend selection: LOCAL → OLLAMA → fallback
EXTERN InferenceRouter_Init:PROC
EXTERN InferenceRouter_Generate:PROC
EXTERN InferenceRouter_Abort:PROC
EXTERN InferenceRouter_SetBackend:PROC
EXTERN InferenceRouter_GetStats:PROC
EXTERN InferenceRouter_LoadVocab:PROC
EXTERN g_routerBackend:DWORD
EXTERN g_routerAbort:DWORD

; bridge.asm — Ghost-text bridge
EXTERN Bridge_SubmitCompletion:PROC
EXTERN Bridge_GetSuggestionText:PROC
EXTERN Bridge_ClearSuggestion:PROC
EXTERN Bridge_RequestSuggestion:PROC
EXTERN Bridge_AbortInference:PROC

=======
>>>>>>> origin/main
; ── Win32 Constants ──────────────────────────────────────────────
; Window Styles
WS_OVERLAPPEDWINDOW equ 0CF0000h
WS_VISIBLE          equ 10000000h
WS_CHILD            equ 40000000h
WS_VSCROLL          equ 200000h
WS_EX_CLIENTEDGE    equ 200h

; Class Styles
CS_HREDRAW           equ 2
CS_VREDRAW           equ 1

; Messages (note: MSG is reserved in ML64 — use WINMSG)
WM_CREATE            equ 1
WM_DESTROY           equ 2
WM_PAINT             equ 0Fh
WM_CLOSE             equ 10h
WM_COMMAND           equ 111h
WM_KEYDOWN           equ 100h
WM_CHAR              equ 102h

; CreateWindow
CW_USEDEFAULT        equ 80000000h
COLOR_WINDOW         equ 5
SW_SHOWDEFAULT       equ 10

; File I/O
GENERIC_READ         equ 80000000h
GENERIC_WRITE        equ 40000000h
FILE_SHARE_READ      equ 1
OPEN_EXISTING        equ 3
CREATE_ALWAYS        equ 2
PAGE_READONLY        equ 2
FILE_MAP_READ        equ 4

; Process
STARTF_USESTDHANDLES equ 100h
CREATE_NO_WINDOW     equ 8000000h

; Memory
MEM_COMMIT           equ 1000h
MEM_RESERVE          equ 2000h
MEM_RELEASE          equ 8000h
PAGE_READWRITE       equ 4

; GGUF
GGUF_MAGIC           equ 046554747h
Q4_0_ID              equ 2

; ── Structs (ML64 safe names) ────────────────────────────────────
; PAINTSTRUCT → PAINTST  (PAINTSTRUCT is fine in ML64, but PAINTST
;   is shorter and avoids any future collision)
PAINTST STRUCT
    hdc             dq ?
    fErase          dd ?
    rc_left         dd ?
    rc_top          dd ?
    rc_right        dd ?
    rc_bottom       dd ?
    reserved        db 32 dup(?)
PAINTST ENDS

; MSG → WINMSG  (MSG is reserved in ML64)
WINMSG STRUCT
    hwnd            dq ?
    message         dd ?
    wParam          dq ?
    lParam          dq ?
    time            dd ?
    pt_x            dd ?
    pt_y            dd ?
WINMSG ENDS

; WNDCLASSEXW (cbSize = 80 on x64, hardcode — no sizeof in ML64)
WNDCLASSEXW STRUCT
    cbSize          dd ?
    style           dd ?
    lpfnWndProc     dq ?
    cbClsExtra      dd ?
    cbWndExtra      dd ?
    hInstance       dq ?
    hIcon           dq ?
    hCursor         dq ?
    hbrBackground   dq ?
    lpszMenuName    dq ?
    lpszClassName   dq ?
    hIconSm         dq ?
WNDCLASSEXW ENDS

; WNDCLASSEXW.cbSize — hardcoded constant for x64
WNDCLASSEXW_SIZE    equ 80

; ── Beacon constants ─────────────────────────────────────────────
BEACON_SLOTS         equ 16
RING_SIZE            equ 100000h

; Agent IDs
AGENT_ID_CORE        equ 0A1h
AGENT_ID_INFERENCE   equ 0A2h
AGENT_ID_UI          equ 0A3h
AGENT_ID_LSP         equ 0A4h

; ═══════════════════════════════════════════════════════════════════
; End of rawrxd.inc — 15 modules, 1 EXE, zero deps.
; ═══════════════════════════════════════════════════════════════════
