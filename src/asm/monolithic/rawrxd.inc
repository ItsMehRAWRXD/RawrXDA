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
EXTERN g_modelbase:QWORD

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

; model_loader.asm
EXTERN LoadModel:PROC
EXTERN GetTensor:PROC
EXTERN UnloadModel:PROC
EXTERN ModelLoaderInit:PROC
EXTERN HotSwapModel:PROC
EXTERN GetCurrentModelPath:PROC

; beacon.asm (X+4)
EXTERN TryBeaconRecv:PROC

; agent.asm (X+4)
EXTERN ProcessOneAgentMessage:PROC

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
; End of rawrxd.inc — 7 modules, 1 EXE, zero deps.
; ═══════════════════════════════════════════════════════════════════
