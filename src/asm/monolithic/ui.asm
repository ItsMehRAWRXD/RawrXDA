; ═══════════════════════════════════════════════════════════════════
; RawrXD UI Engine — Phase X+1: Custom Editor Surface
; Pure Win32 GDI. No Edit control. Own text buffer, cursor, paint.
; Exports: UIMainLoop, CreateEditorPane, hMainWnd
; ═══════════════════════════════════════════════════════════════════

EXTERN g_hInstance:QWORD
EXTERN GetModuleHandleW:PROC
EXTERN RegisterClassExW:PROC
EXTERN CreateWindowExW:PROC
EXTERN GetMessageW:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageW:PROC
EXTERN DefWindowProcW:PROC
EXTERN PostQuitMessage:PROC
EXTERN BeginPaint:PROC
EXTERN EndPaint:PROC
EXTERN InvalidateRect:PROC
EXTERN GetClientRect:PROC
EXTERN SetFocus:PROC
EXTERN TextOutW:PROC
EXTERN SetTextColor:PROC
EXTERN SetBkColor:PROC
EXTERN SetBkMode:PROC

; Phase 16: Multi-Node Visualizer Imports
EXTERN g_swarmDeviceCount:DWORD
EXTERN g_remoteCount:DWORD
EXTERN g_accumulatedSteps:DWORD

; Bridge exports from C++ (bridge_layer.cpp)
EXTERN Bridge_SubmitCompletion:PROC
EXTERN Bridge_ClearSuggestion:PROC
EXTERN Bridge_RequestSuggestion:PROC
EXTERN Bridge_AbortInference:PROC
EXTERN PostMessageW:PROC          ; Needed for deferred ghost text requests

; Phase E: QTG bridge (quantum_agent_orchestrator_thunks.cpp)
EXTERN UIBridge_GenerateFeature:PROC

EXTERN CreateFontIndirectW:PROC
EXTERN SelectObject:PROC
EXTERN DeleteObject:PROC
EXTERN FillRect:PROC
EXTERN GetStockObject:PROC
EXTERN CreateCaret:PROC
EXTERN SetCaretPos:PROC
EXTERN ShowCaret:PROC
EXTERN DestroyCaret:PROC
EXTERN HideCaret:PROC
EXTERN SetScrollInfo:PROC
EXTERN GetScrollInfo:PROC
EXTERN SetTimer:PROC
EXTERN KillTimer:PROC

; Clipboard
EXTERN OpenClipboard:PROC
EXTERN CloseClipboard:PROC
EXTERN EmptyClipboard:PROC
EXTERN SetClipboardData:PROC
EXTERN GetClipboardData:PROC
EXTERN GlobalAlloc:PROC
EXTERN GlobalLock:PROC
EXTERN GlobalUnlock:PROC
EXTERN GlobalFree:PROC
EXTERN GetKeyState:PROC

; Context menu
EXTERN CreatePopupMenu:PROC
EXTERN InsertMenuW:PROC
EXTERN TrackPopupMenu:PROC
EXTERN DestroyMenu:PROC
EXTERN GetCursorPos:PROC
EXTERN ScreenToClient:PROC
EXTERN LoadCursorW:PROC
EXTERN SetCursor:PROC
EXTERN AppendMenuW:PROC

; File I/O for PE Writer + Save/Open
EXTERN CreateFileW:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN MessageBoxA:PROC
EXTERN MessageBoxW:PROC

; PE Writer (in pe_writer.asm)
EXTERN SavePEToDisk:PROC
EXTERN GetFileSizeEx:PROC
EXTERN GetSaveFileNameW:PROC
EXTERN GetOpenFileNameW:PROC
EXTERN CreateSolidBrush:PROC
EXTERN SavePEToDisk:PROC

PUBLIC UIMainLoop
PUBLIC CreateEditorPane
PUBLIC hMainWnd
PUBLIC g_ghostActive
PUBLIC g_ghostTextBuffer
PUBLIC g_ghostTextLen
PUBLIC g_ghostLineCount
PUBLIC g_inferencePending

CS_HREDRAW         equ 2
CS_VREDRAW         equ 1
WS_OVERLAPPEDWINDOW equ 0CF0000h
WS_VISIBLE         equ 10000000h
WS_CHILD           equ 40000000h
WS_VSCROLL         equ 200000h
WS_EX_CLIENTEDGE   equ 200h
CW_USEDEFAULT      equ 80000000h
COLOR_WINDOW       equ 5
SW_SHOWDEFAULT     equ 10

WM_CREATE          equ 1
WM_DESTROY         equ 2
WM_SIZE            equ 5
WM_PAINT           equ 0Fh
WM_CLOSE           equ 10h
WM_SETFOCUS        equ 7
WM_KILLFOCUS       equ 8
WM_KEYDOWN         equ 100h
WM_CHAR            equ 102h
WM_MOUSEWHEEL      equ 20Ah
WM_VSCROLL         equ 114h
WM_ERASEBKGND      equ 14h
WM_USER            equ 400h
WM_USER_SUGGESTION_REQ equ (400h + 100h) ; 0x410
WM_TIMER           equ 113h

; Timer IDs for debounced inference
TIMER_GHOST_DEBOUNCE equ 1001       ; Idle timer for ghost text trigger
GHOST_DEBOUNCE_MS    equ 350        ; ms of idle before firing inference

VK_LEFT            equ 25h
VK_UP              equ 26h
VK_RIGHT           equ 27h
VK_DOWN            equ 28h
VK_HOME            equ 24h
VK_END             equ 23h
VK_DELETE          equ 2Eh
VK_PRIOR           equ 21h
VK_NEXT            equ 22h
VK_ESCAPE          equ 1Bh
VK_TAB             equ 09h
VK_BACK            equ 8
VK_SPACE           equ 20h
VK_CONTROL         equ 11h

WM_COMMAND         equ 111h
WM_RBUTTONUP       equ 205h
WM_LBUTTONDOWN     equ 201h
WM_SETCURSOR       equ 20h

TPM_LEFTALIGN      equ 0
TPM_RETURNCMD      equ 100h
MF_STRING          equ 0
MF_SEPARATOR       equ 800h
GMEM_MOVEABLE      equ 2
CF_UNICODETEXT     equ 0Dh
IDC_IBEAM          equ 32513

IDM_CUT            equ 1
IDM_COPY           equ 2
IDM_PASTE          equ 3
IDM_SELECTALL      equ 4
IDM_DELETE         equ 5
IDM_GENERATE_PE    equ 1001h
IDM_SAVE_PE        equ 1002h
IDM_GENERATE_FEATURE equ 1003h

SB_VERT            equ 1
SIF_ALL            equ 17h
SIF_POS            equ 4
SB_THUMBTRACK      equ 5
SB_LINEUP          equ 0
SB_LINEDOWN        equ 1
SB_PAGEUP          equ 2
SB_PAGEDOWN        equ 3

TRANSPARENT        equ 1
OPAQUE             equ 2
FW_NORMAL          equ 400
DEFAULT_CHARSET    equ 1
CLEARTYPE_QUALITY  equ 5
FF_MODERN          equ 30h
FIXED_PITCH        equ 1
NULL_BRUSH         equ 5

GUTTER_WIDTH       equ 50
CHAR_WIDTH         equ 8
LINE_HEIGHT        equ 18
TEXT_BUF_SIZE      equ 10000h
MAX_LINES          equ 4000
MAX_GHOST_LEN      equ 1024          ; Increased for multi-line
MAX_GHOST_LINES    equ 16            ; Max lines per suggestion
GHOST_TEXT_COLOR   equ 00808080h ; Grey RGB
SEL_BK_COLOR      equ 00D77800h ; Selection highlight (blue in BGR)
SEL_TEXT_COLOR    equ 00FFFFFFh ; White text on selection
GUTTER_TEXT_COLOR equ 00999999h ; Grey gutter line numbers
BG_COLOR          equ 001E1E1Eh ; Dark editor background (BGR)
GENERIC_READ      equ 80000000h
OPEN_EXISTING     equ 3
FILE_ATTRIBUTE_NORMAL equ 80h
CREATE_ALWAYS     equ 2

; Undo system constants
UNDO_MAX_ENTRIES  equ 64          ; Circular undo ring size
UNDO_BUF_SIZE     equ 512         ; Max chars per undo snapshot

;=============================================================================
; PE32+ Header Constants — RawrXD Sovereign PE Writer
;=============================================================================
IMAGE_DOS_SIGNATURE       EQU     5A4Dh          ; 'MZ'
IMAGE_DOS_HEADER_SIZE     EQU     64             ; 0x40 bytes
IMAGE_NT_SIGNATURE        EQU     00004550h      ; 'PE\0\0'
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 020Bh          ; PE32+ (x64)
IMAGE_FILE_MACHINE_AMD64  EQU     8664h
IMAGE_FILE_EXECUTABLE_IMAGE   EQU 0002h 
IMAGE_FILE_LARGE_ADDRESS_AWARE  EQU 0020h
IMAGE_SCN_CNT_CODE        EQU     000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA EQU  000000040h
IMAGE_SCN_MEM_EXECUTE       EQU   020000000h
IMAGE_SCN_MEM_READ          EQU   040000000h
IMAGE_SCN_MEM_WRITE         EQU   080000000h
IMAGE_DIRECTORY_ENTRY_IMPORT  EQU 1
FILE_ALIGNMENT            EQU     512            ; 0x200
SECTION_ALIGNMENT         EQU     4096           ; 0x1000

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

WINMSG STRUCT
    hwnd    dq ?
    message dd ?
    wParam  dq ?
    lParam  dq ?
    time    dd ?
    pt_x    dd ?
    pt_y    dd ?
WINMSG ENDS

PAINTST STRUCT
    hdc     dq ?
    fErase  dd ?
    rc_left dd ?
    rc_top  dd ?
    rc_right dd ?
    rc_bottom dd ?
    reserved db 32 dup(?)
PAINTST ENDS

LOGFONTW STRUCT
    lfHeight         dd ?
    lfWidth          dd ?
    lfEscapement     dd ?
    lfOrientation    dd ?
    lfWeight         dd ?
    lfItalic         db ?
    lfUnderline      db ?
    lfStrikeOut      db ?
    lfCharSet        db ?
    lfOutPrecision   db ?
    lfClipPrecision  db ?
    lfQuality        db ?
    lfPitchAndFamily db ?
    lfFaceName       dw 32 dup(?)
LOGFONTW ENDS

SCROLLINFO STRUCT
    cbSize    dd ?
    fMask     dd ?
    nMin      dd ?
    nMax      dd ?
    nPage     dd ?
    nPos      dd ?
    nTrackPos dd ?
SCROLLINFO ENDS

PUBLIC g_textBuf
PUBLIC g_totalChars
PUBLIC hMainWnd

PUBLIC g_lineOff
PUBLIC g_cursorLine
PUBLIC g_cursorCol

.data
align 8
hMainWnd      dq 0
hEditorFont   dq 0
g_hFont       dq 0
g_cursorLine  dd 0
g_cursorCol   dd 0
g_caretX      dd 0
g_scrollY     dd 0
g_clientW     dd 960
g_clientH     dd 600
g_totalChars  dd 0
g_lineCount   dd 1
g_selStart    dd -1          ; selection anchor (char index), -1 = none
g_selEnd      dd -1          ; selection end (char index)
g_hasFocus    dd 0
g_ghostTimerActive dd 0      ; 1 = debounce timer is running
align 8
g_lineOff     dd MAX_LINES dup(0)

;=============================================================================
; Ghost Text State (Multi-line)
;=============================================================================
align 16
g_ghostTextBuffer      dw  MAX_GHOST_LEN DUP(0)   ; Raw suggestion WCHARs
g_ghostLineOffsets     dd  MAX_GHOST_LINES DUP(0) ; 0-based char offsets for each line
g_ghostLineLengths     dd  MAX_GHOST_LINES DUP(0) ; length of each line in ghost
g_ghostLineCount       dd  0                        ; Current number of lines
g_ghostTextLen         dd  0                        ; Total WCHARs in ghost buffer
g_ghostActive          db  0                        ; 0=inactive, 1=active
                       db  0                        ; padding
g_ghostCursorRow       dd  0                        ; Current Row
g_ghostCursorCol       dd  0                        ; Current Col
g_inferencePending     db  0                        ; Request in flight
align 8
g_hGhostFont           dq  0                        ; Italic font for ghost overlay

;=============================================================================
; Phase 4B: Neural Pruning Scorer State
;=============================================================================
MAX_CANDIDATES     equ  8            ; Max candidate completions to rank
MAX_CANDIDATE_LEN  equ  256          ; Max WCHARs per candidate
SCORE_THRESHOLD    equ  40           ; Minimum score (0-255) to accept (fixed-point)

PUBLIC g_candidates
PUBLIC g_candidateLens
PUBLIC g_candidateScores
PUBLIC g_candidateCount
PUBLIC g_bestCandidate
PUBLIC g_rlhfAcceptCount
PUBLIC g_rlhfRejectCount
align 16
g_candidates       dw  MAX_CANDIDATES * MAX_CANDIDATE_LEN DUP(0)
g_candidateLens    dd  MAX_CANDIDATES DUP(0)
g_candidateScores  dd  MAX_CANDIDATES DUP(0)  ; Fixed-point 8.24 scores
g_candidateCount   dd  0
g_bestCandidate    dd  0                      ; Index of highest-scored candidate
g_rlhfAcceptCount  dd  0                      ; Total accepted suggestions (RLHF)
g_rlhfRejectCount  dd  0                      ; Total rejected suggestions (RLHF)


;=============================================================================
; Undo Ring Buffer
;=============================================================================
UNDO_ENTRY STRUCT
    undoType       dd  ?         ; 0=insert, 1=delete, 2=replace
    charOffset     dd  ?         ; Position in buffer where edit occurred
    charCount      dd  ?         ; Number of chars affected
    cursorLine     dd  ?         ; Cursor line before edit
    cursorCol      dd  ?         ; Cursor col before edit
    pad            dd  ?         ; Alignment padding
UNDO_ENTRY ENDS

align 16
g_undoRing    UNDO_ENTRY UNDO_MAX_ENTRIES DUP(<>)
g_undoData    dw  UNDO_MAX_ENTRIES * UNDO_BUF_SIZE DUP(0)  ; Deleted text storage
g_undoHead    dd  0             ; Next write slot
g_undoCount   dd  0             ; Valid entries in ring

;=============================================================================
; File Save/Open State
;=============================================================================
align 8
g_filePath    dw  260 DUP(0)    ; Current file path (MAX_PATH WCHARs)
g_filePathSet dd  0             ; 1 if a file path is active
g_hBgBrush    dq  0             ; Background brush handle

; OPENFILENAMEW structure (152 bytes for x64)
align 8
g_ofn         db  152 DUP(0)    ; OPENFILENAMEW struct
g_ofnFilter   dw  'A','l','l',' ','F','i','l','e','s',0,'*','.','*',0
              dw  'A','S','M',0,'*','.','a','s','m',0
              dw  0  ; double-null terminator

; Handles — g_hInstance is EXTERN from main.asm
; g_hFont                dq  0                        ; Editor system font (COMMENTED OUT TO AVOID REDEFINITION)
; hEditorFont            dq  0                        ; Compatibility handle (COMMENTED OUT TO AVOID REDEFINITION)
; hMainWnd               dq  0                        ; Window handle (COMMENTED OUT TO AVOID REDEFINITION)
;=============================================================================
; PE32+ Embedded Binary Templates — Corrected RawrXD Sovereign PE Writer
;=============================================================================
ALIGN 16
g_dosHeader:
    dw      IMAGE_DOS_SIGNATURE                  ; e_magic  = 'MZ'
    dw      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    dd      IMAGE_DOS_HEADER_SIZE                ; e_lfanew = 0x40 (PE sig right after DOS hdr)

g_ntHeaders:
    dd      IMAGE_NT_SIGNATURE                   ; Signature = 'PE\0\0'
    ; ── COFF File Header (20 bytes) ──
    dw      IMAGE_FILE_MACHINE_AMD64             ; Machine
    dw      3                                    ; NumberOfSections
    dd      0, 0, 0                              ; TimeDateStamp, PtrToSymTab, NumSymbols
    dw      0F0h                                 ; SizeOfOptionalHeader = 240 (PE32+)
    dw      IMAGE_FILE_EXECUTABLE_IMAGE OR IMAGE_FILE_LARGE_ADDRESS_AWARE
    ; ── Optional Header PE32+ (240 bytes) ──
    dw      IMAGE_NT_OPTIONAL_HDR64_MAGIC        ; Magic = 0x020B
    db      14, 30                               ; LinkerVersion
    dd      0, 0, 0                              ; SizeOfCode, InitData, UninitData
    dd      000001000h                           ; AddressOfEntryPoint
    dd      000001000h                           ; BaseOfCode
    dq      0000000140000000h                    ; ImageBase
    dd      SECTION_ALIGNMENT                    ; SectionAlignment = 0x1000
    dd      FILE_ALIGNMENT                       ; FileAlignment    = 0x200
    dw      6, 0, 0, 0, 6, 0                    ; OS/Image/Subsystem versions
    dd      0                                    ; Win32VersionValue
    dd      000004000h                           ; SizeOfImage
    dd      000000200h                           ; SizeOfHeaders (aligned to FileAlignment)
    dd      0                                    ; CheckSum
    dw      3                                    ; Subsystem = WINDOWS_CUI (console)
    dw      0                                    ; DllCharacteristics
    dq      00100000h, 00001000h                 ; StackReserve, StackCommit
    dq      00100000h, 00001000h                 ; HeapReserve, HeapCommit
    dd      0                                    ; LoaderFlags
    dd      16                                   ; NumberOfRvaAndSizes

g_dataDirectory:
    ; 16 data directory entries (RVA, Size pairs) = 128 bytes
    dd      0, 0                                 ; [0]  Export
    dd      000003000h, 40                       ; [1]  Import (RVA=0x3000, Size=40)
    dd      0, 0                                 ; [2]  Resource
    dd      0, 0                                 ; [3]  Exception
    dd      0, 0                                 ; [4]  Security
    dd      0, 0                                 ; [5]  BaseReloc
    dd      0, 0                                 ; [6]  Debug
    dd      0, 0                                 ; [7]  Architecture
    dd      0, 0                                 ; [8]  GlobalPtr
    dd      0, 0                                 ; [9]  TLS
    dd      0, 0                                 ; [10] LoadConfig
    dd      0, 0                                 ; [11] BoundImport
    dd      000003048h, 32                       ; [12] IAT (RVA=0x3048, Size=32)
    dd      0, 0                                 ; [13] DelayImport
    dd      0, 0                                 ; [14] CLR
    dd      0, 0                                 ; [15] Reserved

g_sectionHeaders:
    db      '.text', 0, 0, 0
    dd      000001000h                           ; VirtualSize
    dd      000001000h                           ; VirtualAddress
    dd      000000200h                           ; SizeOfRawData
    dd      000000200h                           ; PointerToRawData
    dd      0, 0
    dw      0, 0
    dd      IMAGE_SCN_CNT_CODE OR IMAGE_SCN_MEM_EXECUTE OR IMAGE_SCN_MEM_READ
    db      '.data', 0, 0, 0
    dd      000001000h
    dd      000002000h
    dd      000000200h
    dd      000000400h
    dd      0, 0
    dw      0, 0
    dd      IMAGE_SCN_CNT_INITIALIZED_DATA OR IMAGE_SCN_MEM_READ OR IMAGE_SCN_MEM_WRITE
    db      '.idata', 0, 0
    dd      000001000h
    dd      000003000h
    dd      000000200h
    dd      000000600h
    dd      0, 0
    dw      0, 0
    dd      IMAGE_SCN_CNT_INITIALIZED_DATA OR IMAGE_SCN_MEM_READ OR IMAGE_SCN_MEM_WRITE

; ── .idata section content (copied to file offset 0x600, RVA 0x3000) ──
; Layout within .idata (3 imports from kernel32.dll):
;   0x00: Import Dir Entry 0               (20 bytes)
;   0x14: Import Dir Entry 1 (null term)   (20 bytes)
;   0x28: ILT[0] GetStdHandle              (8 bytes)
;   0x30: ILT[1] WriteFile                 (8 bytes)
;   0x38: ILT[2] ExitProcess               (8 bytes)
;   0x40: ILT[3] terminator                (8 bytes)
;   0x48: IAT[0] GetStdHandle (patched)    (8 bytes)
;   0x50: IAT[1] WriteFile    (patched)    (8 bytes)
;   0x58: IAT[2] ExitProcess  (patched)    (8 bytes)
;   0x60: IAT[3] terminator                (8 bytes)
;   0x68: Hint + "GetStdHandle\0" + pad    (16 bytes)
;   0x78: Hint + "WriteFile\0"             (12 bytes)
;   0x84: Hint + "ExitProcess\0"           (14 bytes)
;   0x92: "kernel32.dll\0"                 (13 bytes)
g_importTable:
    ; Import Directory Entry 0 — kernel32.dll
    dd      000003028h                           ; OriginalFirstThunk (ILT at +0x28)
    dd      0                                    ; TimeDateStamp
    dd      0                                    ; ForwarderChain
    dd      000003092h                           ; Name RVA (+0x92)
    dd      000003048h                           ; FirstThunk (IAT at +0x48)
    ; Import Directory Entry 1 (null terminator)
    dd      0, 0, 0, 0, 0
    ; ILT (Import Lookup Table) at .idata+0x28
    dq      000003068h                           ; [0] → GetStdHandle
    dq      000003078h                           ; [1] → WriteFile
    dq      000003084h                           ; [2] → ExitProcess
    dq      0                                    ; [3] Terminator
    ; IAT (Import Address Table) at .idata+0x48
    dq      000003068h                           ; [0] GetStdHandle (loader patches)
    dq      000003078h                           ; [1] WriteFile    (loader patches)
    dq      000003084h                           ; [2] ExitProcess  (loader patches)
    dq      0                                    ; [3] Terminator
    ; Hint/Name: GetStdHandle at .idata+0x68 (16 bytes)
    dw      0
    db      'GetStdHandle', 0, 0
    ; Hint/Name: WriteFile at .idata+0x78 (12 bytes)
    dw      0
    db      'WriteFile', 0
    ; Hint/Name: ExitProcess at .idata+0x84 (14 bytes)
    dw      0
    db      'ExitProcess', 0
    ; DLL name at .idata+0x92
    db      'kernel32.dll', 0
    ALIGN   4

; ── .data section payload (copied to file offset 0x400, RVA 0x2000) ──
g_dataPayload:
    db      'RawrXD PE32+ Emitter OK', 0Dh, 0Ah ; 25 bytes at RVA 0x2000
    ALIGN   8

; ── .text section content (copied to file offset 0x200, RVA 0x1000) ──
; 64-byte entry: GetStdHandle(-11) → WriteFile(stdout,msg,25) → ExitProcess(0)
;   sub rsp, 38h           48 83 EC 38           (4b, 0x1000)
;   mov ecx, -11           B9 F5 FF FF FF        (5b, 0x1004) STD_OUTPUT_HANDLE
;   call [rip+0x2039]      FF 15 39 20 00 00     (6b, 0x1009) → IAT[0] 0x3048
;   mov rcx, rax           48 89 C1              (3b, 0x100F)
;   lea rdx, [rip+0x0FE7]  48 8D 15 E7 0F 00 00  (7b, 0x1012) → .data 0x2000
;   mov r8d, 19h           41 B8 19 00 00 00     (6b, 0x1019) 25 bytes
;   lea r9, [rsp+28h]      4C 8D 4C 24 28        (5b, 0x101F) &written
;   mov [rsp+20h], 0       48 C7 44 24 20 0..0   (9b, 0x1024) lpOverlapped
;   call [rip+0x201D]      FF 15 1D 20 00 00     (6b, 0x102D) → IAT[1] 0x3050
;   xor ecx, ecx           33 C9                 (2b, 0x1033)
;   call [rip+0x201D]      FF 15 1D 20 00 00     (6b, 0x1035) → IAT[2] 0x3058
;   int3 × 5 padding                             (5b, 0x103B)
g_entryCode:
    db      048h, 083h, 0ECh, 038h
    db      0B9h, 0F5h, 0FFh, 0FFh, 0FFh
    db      0FFh, 015h, 039h, 020h, 000h, 000h
    db      048h, 089h, 0C1h
    db      048h, 08Dh, 015h, 0E7h, 00Fh, 000h, 000h
    db      041h, 0B8h, 019h, 000h, 000h, 000h
    db      04Ch, 08Dh, 04Ch, 024h, 028h
    db      048h, 0C7h, 044h, 024h, 020h, 000h, 000h, 000h, 000h
    db      0FFh, 015h, 01Dh, 020h, 000h, 000h
    db      033h, 0C9h
    db      0FFh, 015h, 01Dh, 020h, 000h, 000h
    db      0CCh, 0CCh, 0CCh, 0CCh, 0CCh

szPEGenerated           db      "PE32+ generated in memory.", 0
szPEFailed              db      "PE generation failed.", 0
szPESaved               db      "output.exe saved successfully.", 0
szPESaveFailed          db      "Failed to save output.exe.", 0
szDefaultPEPath         dw      'o','u','t','p','u','t','.','e','x','e',0

align 8
PUBLIC WritePEFile
PUBLIC g_peSize
PUBLIC g_peBuffer
g_peSize                dq      0
g_peBuffer              db      8192 DUP(0)

; ── Phase 4B: Keyword table for ScoreCandidate Heuristic 5 ──
; Each entry: BYTE len, then `len` WCHARs (no null-terminator needed)
; Terminated by a zero-length entry (len=0)
align 2
g_kw_tbl:
; MASM keywords
    db  3
    dw  'm','o','v'
    db  4
    dw  'c','a','l','l'
    db  4
    dw  'p','u','s','h'
    db  3
    dw  'p','o','p'
    db  3
    dw  'j','m','p'
    db  3
    dw  'c','m','p'
    db  3
    dw  'r','e','t'
    db  3
    dw  'l','e','a'
    db  3
    dw  's','u','b'
    db  3
    dw  'a','d','d'
    db  3
    dw  'x','o','r'
    db  4
    dw  'p','r','o','c'
    db  4
    dw  'e','n','d','p'
    db  6
    dw  'e','x','t','e','r','n'
    db  6
    dw  'p','u','b','l','i','c'
    db  6
    dw  'i','n','v','o','k','e'
; C/Python keywords
    db  2
    dw  'i','f'
    db  3
    dw  'f','o','r'
    db  5
    dw  'w','h','i','l','e'
    db  6
    dw  'r','e','t','u','r','n'
    db  3
    dw  'd','e','f'
    db  4
    dw  'e','l','s','e'
    db  4
    dw  'v','o','i','d'
    db  3
    dw  'i','n','t'
    db  8
    dw  'f','u','n','c','t','i','o','n'
    db  5
    dw  'c','l','a','s','s'
    db  6
    dw  's','t','r','u','c','t'
    db  0                              ; sentinel

.data?

g_taskPromptBuf db 2048 dup (?)
align 8
g_textBuf     dw TEXT_BUF_SIZE dup(?)
g_numBuf      dw 16 dup(?)

.const
szClass        dw 'R','a','w','r','X','D','_','M','a','i','n',0
szGenerateFeature dw 'G','e','n','e','r','a','t','e',' ','F','e','a','t','u','r','e',' ','(','A','I',')',0
szTaskPromptTitle dw 'T','a','s','k',' ','I','n','p','u','t',0
szTaskPromptMessage dw 'D','e','s','c','r','i','b','e',' ','t','h','e',' ','f','e','a','t','u','r','e',' ','y','o','u',' ','w','a','n','t',' ','t','o',' ','b','u','i','l','d',':',0
szTitle        dw 'R','a','w','r','X','D',' ','A','g','e','n','t','i','c',' ','I','D','E',0
szFontName     dw 'C','o','n','s','o','l','a','s',0

; QTG bridge ANSI strings (UIBridge_GenerateFeature expects const char*)
szQTGDefaultPrompt db 'Generate feature from current context',0
szQTGDefaultFile   db 'main.asm',0
szQTGSuccess   dw 'F','e','a','t','u','r','e',' ','g','e','n','e','r','a','t','e','d',' ','s','u','c','c','e','s','s','f','u','l','l','y','!',0
szQTGFailed    dw 'F','e','a','t','u','r','e',' ','g','e','n','e','r','a','t','i','o','n',' ','f','a','i','l','e','d','.',0

; Context menu strings
szCut          dw 'C','u','t',9,'C','t','r','l','+','X',0
szCopy         dw 'C','o','p','y',9,'C','t','r','l','+','C',0
szPaste        dw 'P','a','s','t','e',9,'C','t','r','l','+','V',0
szSelectAll    dw 'S','e','l','e','c','t',' ','A','l','l',9,'C','t','r','l','+','A',0
szDelete       dw 'D','e','l','e','t','e',0
szGeneratePE   dw 'G','e','n','e','r','a','t','e',' ','P','E',0
szSavePE       dw 'S','a','v','e',' ','P','E',' ','t','o',' ','D','i','s','k',0

; Welcome text seeded into the editor on WM_CREATE
szWelcome      dw ';',' ','R','a','w','r','X','D',' ','A','g','e','n','t','i','c'
               dw ' ','I','D','E',' ',2014h,' ','S','o','v','e','r','e','i','g','n'
               dw ' ','K','e','r','n','e','l',0Ah
               dw ';',' ','T','y','p','e',' ','h','e','r','e','.',' '
               dw 'A','l','l',' ','e','d','i','t','o','r',' ','k','e','y','s'
               dw ' ','a','r','e',' ','l','i','v','e','.',0Ah
               dw 0Ah
               dw 0

.code

WndProc PROTO

UIMainLoop PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 0F0h
    .allocstack 0F0h
    .endprolog
    ; Self-initialize g_hInstance (no CRT, /ENTRY:UIMainLoop)
    xor     ecx, ecx
    call    GetModuleHandleW
    mov     g_hInstance, rax
    wndClass equ rbp-50h
    mov     dword ptr [wndClass], 80
    mov     dword ptr [wndClass+4], CS_HREDRAW or CS_VREDRAW
    lea     rax, WndProc
    mov     qword ptr [wndClass+8], rax
    xor     eax, eax
    mov     dword ptr [wndClass+10h], eax
    mov     dword ptr [wndClass+14h], eax
    mov     rax, g_hInstance
    mov     qword ptr [wndClass+18h], rax
    xor     eax, eax
    mov     qword ptr [wndClass+20h], rax
    mov     qword ptr [wndClass+28h], rax
    mov     qword ptr [wndClass+30h], COLOR_WINDOW + 1
    mov     qword ptr [wndClass+38h], rax
    lea     rcx, szClass
    mov     qword ptr [wndClass+40h], rcx
    xor     eax, eax
    mov     qword ptr [wndClass+48h], rax
    lea     rcx, [wndClass]
    call    RegisterClassExW
    test    ax, ax
    jz      @exit
    xor     ecx, ecx
    lea     rdx, szClass
    lea     r8, szTitle
    mov     r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE or WS_VSCROLL
    mov     dword ptr [rsp+20h], CW_USEDEFAULT
    mov     dword ptr [rsp+28h], CW_USEDEFAULT
    mov     dword ptr [rsp+30h], 960
    mov     dword ptr [rsp+38h], 600
    xor     eax, eax
    mov     qword ptr [rsp+40h], rax
    mov     qword ptr [rsp+48h], rax
    mov     rax, g_hInstance
    mov     qword ptr [rsp+50h], rax
    mov     qword ptr [rsp+58h], 0
    call    CreateWindowExW
    mov     hMainWnd, rax
    test    rax, rax
    jz      @exit
    msgBuf equ rbp-90h
@msg_loop:
    lea     rcx, [msgBuf]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    GetMessageW
    test    eax, eax
    jz      @exit
    lea     rcx, [msgBuf]
    call    TranslateMessage
    lea     rcx, [msgBuf]
    call    DispatchMessageW
    jmp     @msg_loop
@exit:
    mov     rcx, hEditorFont
    test    rcx, rcx
    jz      @no_font
    call    DeleteObject
@no_font:
    xor     eax, eax
    lea     rsp, [rbp]
    pop     rbp
    ret
UIMainLoop ENDP

WndProc PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 148h
    .allocstack 148h
    .endprolog
    mov     qword ptr [rbp-8], rcx
    mov     dword ptr [rbp-0Ch], edx
    mov     qword ptr [rbp-18h], r8
    mov     qword ptr [rbp-20h], r9
    cmp     edx, WM_CREATE
    je      @wm_create
    cmp     edx, WM_DESTROY
    je      @wm_destroy
    cmp     edx, WM_PAINT
    je      @wm_paint
    cmp     edx, WM_CHAR
    je      @wm_char
    cmp     edx, WM_KEYDOWN
    je      @wm_keydown
    cmp     edx, WM_SIZE
    je      @wm_size
    cmp     edx, WM_SETFOCUS
    je      @wm_setfocus
    cmp     edx, WM_KILLFOCUS
    je      @wm_killfocus
    cmp     edx, WM_ERASEBKGND
    je      @wm_erasebkgnd
    cmp     edx, WM_VSCROLL
    je      @wm_vscroll
    cmp     edx, WM_MOUSEWHEEL
    je      @wm_mousewheel
    cmp     edx, WM_RBUTTONUP
    je      @wm_rbuttonup
    cmp     edx, WM_COMMAND
    je      @wm_command
    cmp     edx, WM_LBUTTONDOWN
    je      @wm_lbuttondown
    cmp     edx, WM_SETCURSOR
    je      @wm_setcursor
    cmp     edx, WM_USER_SUGGESTION_REQ
    je      @wm_user_suggestion_req
    cmp     edx, WM_TIMER
    je      @wm_timer
    mov     rcx, qword ptr [rbp-8]
    mov     edx, dword ptr [rbp-0Ch]
    mov     r8, qword ptr [rbp-18h]
    mov     r9, qword ptr [rbp-20h]
    call    DefWindowProcW
    jmp     @wp_ret

@wm_create:
    mov     rax, qword ptr [rbp-8]
    mov     hMainWnd, rax
    lea     rdi, [rbp-0C0h]
    xor     eax, eax
    mov     ecx, 24
    rep     stosd
    lea     rdi, [rbp-0C0h]
    mov     dword ptr [rdi], 0FFFFFFF2h
    mov     dword ptr [rdi+10h], FW_NORMAL
    mov     byte  ptr [rdi+17h], DEFAULT_CHARSET
    mov     byte  ptr [rdi+1Ah], CLEARTYPE_QUALITY
    mov     byte  ptr [rdi+1Bh], FIXED_PITCH or FF_MODERN
    lea     rsi, szFontName
    lea     rdi, [rbp-0C0h+1Ch]
    mov     ecx, 9
    rep     movsw
    lea     rcx, [rbp-0C0h]
    call    CreateFontIndirectW
    mov     hEditorFont, rax
    mov     g_hFont, rax                 ; Also store as g_hFont for paint

    ; ── Create italic ghost font ──────────────────────────────
    lea     rdi, [rbp-0C0h]
    xor     eax, eax
    mov     ecx, 24
    rep     stosd
    lea     rdi, [rbp-0C0h]
    mov     dword ptr [rdi], 0FFFFFFF2h       ; lfHeight = -14 (same size)
    mov     dword ptr [rdi+10h], FW_NORMAL    ; lfWeight
    mov     byte  ptr [rdi+14h], 1            ; lfItalic = TRUE
    mov     byte  ptr [rdi+17h], DEFAULT_CHARSET
    mov     byte  ptr [rdi+1Ah], CLEARTYPE_QUALITY
    mov     byte  ptr [rdi+1Bh], FIXED_PITCH or FF_MODERN
    lea     rsi, szFontName
    lea     rdi, [rbp-0C0h+1Ch]
    mov     ecx, 9
    rep     movsw
    lea     rcx, [rbp-0C0h]
    call    CreateFontIndirectW
    mov     g_hGhostFont, rax                 ; Store italic ghost font

    mov     g_totalChars, 0
    mov     g_lineCount, 1
    mov     g_cursorLine, 0
    mov     g_cursorCol, 0
    mov     g_scrollY, 0

    ; ── Seed the editor with welcome text ──────────────────────
    ;    So the user sees a live surface, not a blank void.
    lea     rdi, g_textBuf
    lea     rsi, szWelcome
    xor     ecx, ecx
@@seed_copy:
    mov     ax, word ptr [rsi + rcx*2]
    mov     word ptr [rdi + rcx*2], ax
    test    ax, ax
    jz      @@seed_done
    inc     ecx
    jmp     @@seed_copy
@@seed_done:
    mov     g_totalChars, ecx
    call    RebuildLineTable

    call    UpdateScrollBar

    ; Set focus to self so keyboard input works immediately
    mov     rcx, hMainWnd
    call    SetFocus
    jmp     @ret_zero

@wm_setfocus:
    mov     g_hasFocus, 1
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 2
    mov     r9d, LINE_HEIGHT
    call    CreateCaret
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    call    ShowCaret
    jmp     @ret_zero

@wm_killfocus:
    mov     g_hasFocus, 0
    mov     rcx, qword ptr [rbp-8]
    call    DestroyCaret
    jmp     @ret_zero

@wm_size:
    mov     eax, dword ptr [rbp-20h]
    movzx   ecx, ax
    mov     g_clientW, ecx
    shr     eax, 16
    mov     g_clientH, eax
    call    UpdateScrollBar
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@wm_erasebkgnd:
    ; Suppress default erase — WM_PAINT handles full background fill
    mov     eax, 1
    jmp     @wp_ret

@wm_char:
    mov     eax, dword ptr [rbp-18h]
    ; ── Ctrl+key intercepts ──
    cmp     eax, 1                     ; Ctrl+A = Select All
    je      @ctrl_a
    cmp     eax, 3                     ; Ctrl+C = Copy
    je      @ctrl_c
    cmp     eax, 13h                   ; Ctrl+S = Save File
    je      @ctrl_s
    cmp     eax, 16h                   ; Ctrl+V = Paste
    je      @ctrl_v
    cmp     eax, 18h                   ; Ctrl+X = Cut
    je      @ctrl_x
    cmp     eax, 1Ah                   ; Ctrl+Z = Undo
    je      @ctrl_z
    cmp     eax, 8
    je      @char_bs
    cmp     eax, 0Dh
    je      @char_enter
    cmp     eax, 9
    je      @char_tab
    cmp     eax, 20h
    jb      @ret_zero
    ; ── If selection active, delete it first, then insert ──
    call    DeleteSelection
    ; mov     ebx, eax ; REMOVED REDUNDANT/WRONG MIX
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ret_zero
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    lea     rdi, g_textBuf
    mov     ecx, g_totalChars
    mov     ax, word ptr [rbp-18h] ; Restore char to AX for shift/write
@@ins_shift:
    cmp     ecx, esi
    jle     @@ins_write
    mov     dx, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], dx
    dec     ecx
    jmp     @@ins_shift
@@ins_write:
    mov     word ptr [rdi + rsi*2], ax
    ; Record undo for single char insert
    xor     ecx, ecx                    ; type = 0 (insert)
    mov     edx, esi                    ; charOffset
    mov     r8d, 1                      ; charCount = 1
    xor     r9d, r9d                    ; no deleted text
    call    UndoPush
    inc     g_totalChars
    mov     ecx, g_totalChars
    mov     word ptr [rdi + rcx*2], 0
    inc     g_cursorCol
    
    ; ── Abort pending inference on new keystroke ──
    cmp     byte ptr [g_inferencePending], 0
    je      @char_no_abort
    call    Bridge_AbortInference
@char_no_abort:

    ; ── Debounced ghost text: reset idle timer (350ms) ──
    ; SetTimer(hwnd, TIMER_GHOST_DEBOUNCE, GHOST_DEBOUNCE_MS, NULL)
    mov     rcx, qword ptr [rbp-8]       ; hwnd
    mov     edx, TIMER_GHOST_DEBOUNCE    ; nIDEvent
    mov     r8d, GHOST_DEBOUNCE_MS       ; uElapse
    xor     r9d, r9d                     ; lpTimerFunc = NULL
    call    SetTimer
    mov     g_ghostTimerActive, 1

    ; Clear any existing ghost (will refresh with new suggestion)
    mov     byte ptr [g_ghostActive], 0

    call    RebuildLineTable
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_tab:
    ; Check if ghost text is active
    cmp     byte ptr [g_ghostActive], 0
    je      @char_tab_normal
    
    ; Accept ghost: Call bridge to notify (1=accepted)
    mov     ecx, 1
    call    Bridge_SubmitCompletion
    
    ; Insert ghost text characters into buffer
    lea     rsi, [g_ghostTextBuffer]
    ; We need to find the total length used during parsing
    ; For now, we use a simple loop until we hit the total number of chars
    ; but we must handle g_ghostLineCount correctly.
    
    xor     ebx, ebx                     ; ebx = current ghost line index
@ghost_line_loop:
    cmp     ebx, [g_ghostLineCount]
    jge     @ghost_finished
    
    lea     rax, [g_ghostLineOffsets]
    mov     r10d, dword ptr [rax + rbx*4] ; Char offset in buffer
    lea     rax, [g_ghostLineLengths]
    mov     r11d, dword ptr [rax + rbx*4] ; Char count for this line
    
    test    r11d, r11d
    jz      @ghost_next_ln

    lea     rsi, [g_ghostTextBuffer]
    lea     rsi, [rsi + r10*2]
    mov     ecx, r11d
    
@ghost_insert_loop:
    push    rcx
    push    rbx
    lodsw                           ; AX = WCHAR
    movzx   ebx, ax                 ; Character to insert
    
    ; --- Standard Insert Logic ---
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ghost_abort_ins
    
    mov     ecx, g_cursorLine
    lea     rdx, g_lineOff
    mov     edx, dword ptr [rdx + rcx*4]
    add     edx, g_cursorCol
    lea     rdi, g_textBuf
    mov     dword ptr [rbp-0D0h], edx  ; Store insertion index for shift loop
    
    mov     ecx, g_totalChars
    movzx   rcx, cx         ; Clean 16->64 expansion
@@ghost_sh:
    mov     edx, dword ptr [rbp-0D0h] ; Re-fetch insertion index
    movzx   rdx, dx         ; Clean 16->64 expansion
    cmp     rcx, rdx
    jle     @@ghost_wr
    mov     r8w, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], r8w
    dec     rcx
    jmp     @@ghost_sh
@@ghost_wr:
    mov     word ptr [rdi + rdx*2], bx
    inc     g_totalChars
    
    ; Update cursor
    cmp     bx, 0Ah
    jne     @ghost_no_ent
    inc     g_cursorLine
    mov     g_cursorCol, 0
    call    RebuildLineTable        ; Rebuild offsets for each newline
    jmp     @ghost_pop_cont
@ghost_no_ent:
    inc     g_cursorCol
    
@ghost_pop_cont:
    pop     rbx
    pop     rcx
    dec     ecx
    jnz     @ghost_insert_loop

@ghost_next_ln:
    ; Between ghost lines, re-insert the 0x0A that Bridge_OnSuggestionReady
    ; stripped during parsing. Uses identical shift-right logic as WM_CHAR @char_enter.
    inc     ebx
    cmp     ebx, [g_ghostLineCount]
    jge     @ghost_finished

    ; --- Insert 0x0A newline between ghost lines ---
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ghost_finished              ; Buffer full — bail

    ; Compute insertion index = lineOff[cursorLine] + cursorCol
    mov     eax, g_cursorLine
    lea     rdx, g_lineOff
    mov     eax, dword ptr [rdx + rax*4]
    add     eax, g_cursorCol             ; eax = absolute char index
    lea     rdi, g_textBuf

    ; Shift-right: move chars [eax .. totalChars-1] up by 1
    mov     ecx, g_totalChars
@@gnl_shift:
    cmp     ecx, eax
    jle     @@gnl_write
    mov     r8w, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], r8w
    dec     ecx
    jmp     @@gnl_shift
@@gnl_write:
    mov     word ptr [rdi + rax*2], 0Ah  ; Write newline
    inc     g_totalChars
    inc     g_cursorLine
    mov     g_cursorCol, 0
    call    RebuildLineTable             ; Rebuild line offset table
    jmp     @ghost_line_loop

@ghost_finished:
    ; Clear ghost state
    mov     byte ptr [g_ghostActive], 0
    mov     [g_ghostLineCount], 0
    
    call    RebuildLineTable
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@ghost_abort_ins:
    pop     rbx
    pop     rcx
    jmp     @ghost_finished

@char_tab_normal:
    ; ── Insert 4 spaces at cursor position ──
    call    DeleteSelection
    mov     ecx, g_totalChars
    add     ecx, 4                         ; need room for 4 chars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ret_zero                      ; buffer full

    ; Compute insertion index: lineOff[cursorLine] + cursorCol
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol               ; esi = insertion offset

    lea     rdi, g_textBuf
    mov     ecx, g_totalChars

    ; Shift everything right by 4 WCHARs
@@tab_shift:
    cmp     ecx, esi
    jl      @@tab_write
    mov     ax, word ptr [rdi + rcx*2]
    mov     word ptr [rdi + rcx*2 + 8], ax ; +8 = 4 chars * 2 bytes
    dec     ecx
    jmp     @@tab_shift

@@tab_write:
    ; Write 4 space WCHARs (0x0020) at insertion point
    mov     word ptr [rdi + rsi*2],     20h
    mov     word ptr [rdi + rsi*2 + 2], 20h
    mov     word ptr [rdi + rsi*2 + 4], 20h
    mov     word ptr [rdi + rsi*2 + 6], 20h

    ; Record undo for tab (4-space insert)
    xor     ecx, ecx                    ; type = 0 (insert)
    mov     edx, esi                    ; charOffset
    mov     r8d, 4                      ; charCount = 4
    xor     r9d, r9d                    ; no deleted text
    call    UndoPush
    add     g_totalChars, 4
    mov     ecx, g_totalChars
    mov     word ptr [rdi + rcx*2], 0      ; null-terminate

    add     g_cursorCol, 4

    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]         ; hwnd
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_esc:
    ; Check if ghost text is active
    cmp     byte ptr [g_ghostActive], 0
    je      @ret_zero
    
    ; Clear via bridge first
    call    Bridge_ClearSuggestion
    
    ; Reject ghost: Call bridge to notify (0=rejected)
    xor     ecx, ecx
    call    Bridge_SubmitCompletion
    
    ; Clear ghost state
    mov     byte ptr [g_ghostActive], 0
    
    ; Trigger repaint
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_enter:
    call    DeleteSelection
    mov     ecx, g_totalChars
    cmp     ecx, TEXT_BUF_SIZE - 2
    jge     @ret_zero
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    lea     rdi, g_textBuf
    mov     ecx, g_totalChars
@@ent_shift:
    cmp     ecx, esi
    jle     @@ent_write
    mov     ax, word ptr [rdi + rcx*2 - 2]
    mov     word ptr [rdi + rcx*2], ax
    dec     ecx
    jmp     @@ent_shift
@@ent_write:
    mov     word ptr [rdi + rsi*2], 0Ah
    ; Record undo for enter/newline insert
    xor     ecx, ecx                    ; type = 0 (insert)
    mov     edx, esi                    ; charOffset
    mov     r8d, 1                      ; charCount = 1
    xor     r9d, r9d                    ; no deleted text
    call    UndoPush
    inc     g_totalChars
    mov     ecx, g_totalChars
    mov     word ptr [rdi + rcx*2], 0
    inc     g_cursorLine
    mov     g_cursorCol, 0
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@char_bs:
    ; If selection exists, delete it instead of single char
    cmp     g_selStart, -1
    je      @bs_nosel
    call    DeleteSelection
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero
@bs_nosel:
    mov     eax, g_cursorCol
    or      eax, g_cursorLine
    jz      @ret_zero
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    dec     esi
    js      @bs_prev
    lea     rdi, g_textBuf
    cmp     word ptr [rdi + rsi*2], 0Ah
    jne     @bs_normal
@bs_prev:
    mov     ecx, g_cursorLine
    test    ecx, ecx
    jz      @ret_zero
    dec     ecx
    mov     g_cursorLine, ecx
    lea     rax, g_lineOff
    mov     edx, dword ptr [rax + rcx*4]
    mov     ebx, dword ptr [rax + rcx*4 + 4]
    sub     ebx, edx
    dec     ebx
    js      @bs_col0
    mov     g_cursorCol, ebx
    jmp     @bs_recalc
@bs_col0:
    mov     g_cursorCol, 0
@bs_recalc:
    mov     ecx, g_cursorLine
    lea     rax, g_lineOff
    mov     esi, dword ptr [rax + rcx*4]
    add     esi, g_cursorCol
    jmp     @bs_shift
@bs_normal:
    dec     g_cursorCol
@bs_shift:
    lea     rdi, g_textBuf
    ; Record undo for backspace delete
    lea     r9, [rdi + rsi*2]           ; ptr to char being deleted
    mov     ecx, 1                      ; type = 1 (delete)
    mov     edx, esi                    ; charOffset
    mov     r8d, 1                      ; charCount = 1
    call    UndoPush
    ; rdi/rsi preserved by UndoPush
    mov     ecx, esi
    inc     ecx
@@bs_loop:
    cmp     ecx, g_totalChars
    jge     @bs_done
    mov     ax, word ptr [rdi + rcx*2]
    mov     word ptr [rdi + rcx*2 - 2], ax
    inc     ecx
    jmp     @@bs_loop
@bs_done:
    dec     g_totalChars
    mov     ecx, g_totalChars
    lea     rdi, g_textBuf
    mov     word ptr [rdi + rcx*2], 0
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; (Removed dead @char_tab_secondary — unreachable duplicate)
@ctrl_a:
    ; Select All — same as IDM_SELECTALL
    mov     g_selStart, 0
    mov     eax, g_totalChars
    mov     g_selEnd, eax
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@ctrl_c:
    ; Copy — delegate to WM_COMMAND copy logic
    mov     dword ptr [rbp-18h], IDM_COPY
    jmp     @cmd_copy

@ctrl_x:
    ; Cut — delegate to WM_COMMAND cut logic
    mov     dword ptr [rbp-18h], IDM_CUT
    jmp     @cmd_cut

@ctrl_v:
    ; Paste — delegate to WM_COMMAND paste logic
    jmp     @cmd_paste

; ── Ctrl+Z: Undo last edit ────────────────────────────────────
@ctrl_z:
    ; Check if undo ring has entries
    mov     eax, g_undoCount
    test    eax, eax
    jz      @ret_zero                    ; Nothing to undo

    ; Decrement head (circular)
    mov     ecx, g_undoHead
    test    ecx, ecx
    jnz     @cz_dec
    mov     ecx, UNDO_MAX_ENTRIES        ; wrap around
@cz_dec:
    dec     ecx
    mov     g_undoHead, ecx
    dec     g_undoCount

    ; Load undo entry: g_undoRing[ecx]
    ; Each entry = 24 bytes (UNDO_ENTRY)
    imul    edx, ecx, 24
    lea     rsi, g_undoRing
    add     rsi, rdx

    mov     eax, dword ptr [rsi]         ; undoType
    mov     edi, dword ptr [rsi+4]       ; charOffset
    mov     r8d, dword ptr [rsi+8]       ; charCount

    ; Restore cursor position
    mov     edx, dword ptr [rsi+12]      ; cursorLine
    mov     g_cursorLine, edx
    mov     edx, dword ptr [rsi+16]      ; cursorCol
    mov     g_cursorCol, edx

    cmp     eax, 0                       ; 0 = was an INSERT → undo by deleting
    je      @cz_undo_insert
    cmp     eax, 1                       ; 1 = was a DELETE → undo by re-inserting
    je      @cz_undo_delete
    jmp     @cz_done                     ; Unknown type

@cz_undo_insert:
    ; The last edit inserted r8d chars at offset edi → remove them
    lea     r10, g_textBuf
    mov     eax, g_totalChars
    ; Shift left: move [edi+r8d..totalChars) to [edi..)
    mov     edx, edi
    add     edx, r8d                     ; edx = source start
    mov     ecx, edx                     ; copy from source
@cz_ui_loop:
    cmp     ecx, eax
    jge     @cz_ui_done
    mov     r9w, word ptr [r10 + rcx*2]
    mov     word ptr [r10 + rdi*2], r9w
    inc     ecx
    inc     edi
    jmp     @cz_ui_loop
@cz_ui_done:
    sub     g_totalChars, r8d
    mov     eax, g_totalChars
    mov     word ptr [r10 + rax*2], 0
    jmp     @cz_done

@cz_undo_delete:
    ; The last edit deleted r8d chars from offset edi → re-insert from undo data
    ; Undo data is at g_undoData[head * UNDO_BUF_SIZE]
    mov     eax, g_undoHead              ; We already decremented, so current head
    ; Actually we stored at the original head before decrement...
    ; The data was stored at slot = current ecx (which is now g_undoHead)
    mov     eax, g_undoHead
    imul    eax, UNDO_BUF_SIZE * 2       ; byte offset into g_undoData
    lea     rsi, g_undoData
    add     rsi, rax                     ; rsi = saved deleted text

    ; Shift buffer right by r8d chars at position edi
    lea     r10, g_textBuf
    mov     eax, g_totalChars
    dec     eax
@cz_ud_shift:
    cmp     eax, edi
    jl      @cz_ud_copy
    mov     r9w, word ptr [r10 + rax*2]
    lea     ecx, [eax + r8d]
    mov     word ptr [r10 + rcx*2], r9w
    dec     eax
    jmp     @cz_ud_shift
@cz_ud_copy:
    ; Copy saved text back
    xor     ecx, ecx
@cz_ud_cp_loop:
    cmp     ecx, r8d
    jge     @cz_ud_cp_done
    mov     r9w, word ptr [rsi + rcx*2]
    lea     edx, [edi + ecx]
    mov     word ptr [r10 + rdx*2], r9w
    inc     ecx
    jmp     @cz_ud_cp_loop
@cz_ud_cp_done:
    add     g_totalChars, r8d
    mov     eax, g_totalChars
    mov     word ptr [r10 + rax*2], 0

@cz_done:
    call    RebuildLineTable
    call    UpdateScrollBar
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── Ctrl+S: Save file to disk ──────────────────────────────────
@ctrl_s:
    ; If we have a file path, save directly; otherwise show Save dialog
    cmp     g_filePathSet, 0
    jne     @cs_save_direct

    ; Setup OPENFILENAMEW struct
    lea     rdi, g_ofn
    ; Zero it first
    xor     eax, eax
    mov     ecx, 152
@@cs_zero:
    mov     byte ptr [rdi + rcx - 1], 0
    dec     ecx
    jnz     @@cs_zero

    ; Fill fields (x64 OPENFILENAMEW layout)
    mov     dword ptr [rdi], 152                  ; lStructSize
    mov     rax, hMainWnd
    mov     qword ptr [rdi+8], rax                ; hwndOwner
    lea     rax, g_ofnFilter
    mov     qword ptr [rdi+16], rax               ; lpstrFilter
    lea     rax, g_filePath
    mov     qword ptr [rdi+48], rax               ; lpstrFile
    mov     dword ptr [rdi+56], 260               ; nMaxFile
    mov     dword ptr [rdi+60], 2h                ; Flags = OFN_OVERWRITEPROMPT

    mov     rcx, rdi
    call    GetSaveFileNameW
    test    eax, eax
    jz      @ret_zero                             ; User cancelled
    mov     g_filePathSet, 1

@cs_save_direct:
    ; CreateFileW(g_filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, NORMAL, NULL)
    lea     rcx, g_filePath
    mov     edx, 40000000h                        ; GENERIC_WRITE
    xor     r8d, r8d                              ; no sharing
    xor     r9d, r9d                              ; no security
    sub     rsp, 30h
    mov     qword ptr [rsp+20h], CREATE_ALWAYS
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+30h], 0
    call    CreateFileW
    add     rsp, 30h
    cmp     rax, -1
    je      @cs_fail
    mov     rdi, rax                              ; rdi = hFile

    ; WriteFile(hFile, g_textBuf, totalChars*2, &written, NULL)
    mov     rcx, rdi
    lea     rdx, g_textBuf
    mov     r8d, g_totalChars
    shl     r8d, 1                                ; bytes = chars * 2
    lea     r9, [rbp-0B8h]                        ; &bytesWritten
    sub     rsp, 28h
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    add     rsp, 28h

    ; Close file
    mov     rcx, rdi
    call    CloseHandle
    jmp     @ret_zero

@cs_fail:
    lea     rcx, [szPESaveFailed]                 ; Reuse "Failed to save" string
    lea     rdx, [szTitle]
    mov     r8d, 10h                              ; MB_ICONERROR
    xor     r9d, r9d
    call    MessageBoxA
    jmp     @ret_zero

@wm_keydown:
    mov     eax, dword ptr [rbp-18h]

    ; ── Ctrl+Space: immediate ghost text trigger ──
    cmp     eax, VK_SPACE
    jne     @kd_not_ctrl_space
    push    rax
    mov     ecx, VK_CONTROL
    call    GetKeyState
    test    ax, 8000h
    pop     rax
    jz      @kd_not_ctrl_space
    ; Ctrl+Space pressed — fire suggestion immediately
    ; Kill any pending debounce timer
    cmp     g_ghostTimerActive, 0
    je      @kd_ctrl_space_fire
    mov     rcx, qword ptr [rbp-8]
    mov     edx, TIMER_GHOST_DEBOUNCE
    call    KillTimer
    mov     g_ghostTimerActive, 0
@kd_ctrl_space_fire:
    ; Post immediate suggestion request
    mov     rcx, qword ptr [rbp-8]
    mov     edx, WM_USER_SUGGESTION_REQ
    xor     r8d, r8d
    xor     r9d, r9d
    call    PostMessageW
    jmp     @ret_zero
@kd_not_ctrl_space:

    ; ── Check Shift state for selection extension ──
    push    rax
    mov     ecx, 10h                   ; VK_SHIFT
    call    GetKeyState
    test    ax, 8000h                  ; High bit = pressed
    pop     rax
    jz      @kd_no_shift

    ; Shift is held — start or extend selection
    ; If no selection active, anchor at current cursor pos
    mov     ecx, g_selStart
    cmp     ecx, -1
    jne     @kd_shift_check_key
    ; Start selection: set anchor at cursor offset
    mov     ecx, g_cursorLine
    lea     rdx, g_lineOff
    mov     ecx, dword ptr [rdx + rcx*4]
    add     ecx, g_cursorCol
    mov     g_selStart, ecx
    mov     g_selEnd, ecx

@kd_shift_check_key:
    ; Handle shifted nav keys — move cursor, then update selEnd
    cmp     eax, VK_LEFT
    je      @sk_left
    cmp     eax, VK_RIGHT
    je      @sk_right
    cmp     eax, VK_UP
    je      @sk_up
    cmp     eax, VK_DOWN
    je      @sk_down
    cmp     eax, VK_HOME
    je      @sk_home
    cmp     eax, VK_END
    je      @sk_end
    jmp     @kd_no_shift               ; Not a nav key

@sk_left:
    mov     eax, g_cursorCol
    test    eax, eax
    jz      @sk_left_prev
    dec     g_cursorCol
    jmp     @sk_update_sel
@sk_left_prev:
    mov     eax, g_cursorLine
    test    eax, eax
    jz      @sk_update_sel
    dec     g_cursorLine
    call    GetCurLineLen
    mov     g_cursorCol, eax
    jmp     @sk_update_sel

@sk_right:
    call    GetCurLineLen
    mov     ecx, g_cursorCol
    cmp     ecx, eax
    jl      @sk_right_same
    mov     ecx, g_cursorLine
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @sk_update_sel
    mov     g_cursorLine, ecx
    mov     g_cursorCol, 0
    jmp     @sk_update_sel
@sk_right_same:
    inc     g_cursorCol
    jmp     @sk_update_sel

@sk_up:
    mov     eax, g_cursorLine
    test    eax, eax
    jz      @sk_update_sel
    dec     g_cursorLine
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @sk_update_sel
    mov     g_cursorCol, eax
    jmp     @sk_update_sel

@sk_down:
    mov     eax, g_cursorLine
    inc     eax
    cmp     eax, g_lineCount
    jge     @sk_update_sel
    mov     g_cursorLine, eax
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @sk_update_sel
    mov     g_cursorCol, eax
    jmp     @sk_update_sel

@sk_home:
    mov     g_cursorCol, 0
    jmp     @sk_update_sel

@sk_end:
    call    GetCurLineLen
    mov     g_cursorCol, eax
    jmp     @sk_update_sel

@sk_update_sel:
    ; Update selEnd to new cursor offset
    mov     ecx, g_cursorLine
    lea     rdx, g_lineOff
    mov     ecx, dword ptr [rdx + rcx*4]
    add     ecx, g_cursorCol
    mov     g_selEnd, ecx
    ; If selStart == selEnd, clear selection
    mov     edx, g_selStart
    cmp     ecx, edx
    jne     @sk_repaint
    mov     g_selStart, -1
    mov     g_selEnd, -1
@sk_repaint:
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@kd_no_shift:
    ; Non-shifted nav keys clear any active selection
    cmp     eax, VK_LEFT
    jb      @kd_check_key
    cmp     eax, VK_DOWN
    ja      @kd_check_key
    ; Arrow key without Shift — clear selection
    mov     g_selStart, -1
    mov     g_selEnd, -1

@kd_check_key:
    cmp     eax, VK_TAB
    je      @char_tab
    cmp     eax, VK_ESCAPE
    je      @char_esc
    cmp     eax, VK_LEFT
    je      @k_left
    cmp     eax, VK_RIGHT
    je      @k_right
    cmp     eax, VK_UP
    je      @k_up
    cmp     eax, VK_DOWN
    je      @k_down
    cmp     eax, VK_HOME
    je      @k_home
    cmp     eax, VK_END
    je      @k_end
    cmp     eax, VK_DELETE
    je      @k_del
    cmp     eax, VK_PRIOR
    je      @k_pgup
    cmp     eax, VK_NEXT
    je      @k_pgdn
    jmp     @ret_zero

@k_left:
    mov     eax, g_cursorCol
    test    eax, eax
    jz      @kl_prev
    dec     g_cursorCol
    jmp     @k_move
@kl_prev:
    mov     eax, g_cursorLine
    test    eax, eax
    jz      @ret_zero
    dec     g_cursorLine
    call    GetCurLineLen
    mov     g_cursorCol, eax
    jmp     @k_move

@k_right:
    call    GetCurLineLen
    mov     ecx, g_cursorCol
    cmp     ecx, eax
    jl      @kr_same
    mov     ecx, g_cursorLine
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @ret_zero
    mov     g_cursorLine, ecx
    mov     g_cursorCol, 0
    jmp     @k_move
@kr_same:
    inc     g_cursorCol
    jmp     @k_move

@k_up:
    mov     eax, g_cursorLine
    test    eax, eax
    jz      @ret_zero
    dec     g_cursorLine
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_down:
    mov     eax, g_cursorLine
    inc     eax
    cmp     eax, g_lineCount
    jge     @ret_zero
    mov     g_cursorLine, eax
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_home:
    mov     g_cursorCol, 0
    jmp     @k_move

@k_end:
    call    GetCurLineLen
    mov     g_cursorCol, eax
    jmp     @k_move

@k_del:
    ; If selection active, delete selection instead of single char
    mov     eax, g_selStart
    cmp     eax, -1
    je      @k_del_single
    call    DeleteSelection
    call    RebuildLineTable
    call    UpdateScrollBar
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero
@k_del_single:
    mov     ecx, g_cursorLine
    lea     rsi, g_lineOff
    mov     esi, dword ptr [rsi + rcx*4]
    add     esi, g_cursorCol
    cmp     esi, g_totalChars
    jge     @ret_zero
    lea     rdi, g_textBuf
    ; Record undo for delete key
    lea     r9, [rdi + rsi*2]           ; ptr to char being deleted
    mov     ecx, 1                      ; type = 1 (delete)
    mov     edx, esi                    ; charOffset
    mov     r8d, 1                      ; charCount = 1
    call    UndoPush
    ; rdi/rsi preserved by UndoPush
    mov     ecx, esi
    inc     ecx
@@del_loop:
    cmp     ecx, g_totalChars
    jge     @del_done
    mov     ax, word ptr [rdi + rcx*2]
    mov     word ptr [rdi + rcx*2 - 2], ax
    inc     ecx
    jmp     @@del_loop
@del_done:
    dec     g_totalChars
    mov     ecx, g_totalChars
    lea     rdi, g_textBuf
    mov     word ptr [rdi + rcx*2], 0
    call    RebuildLineTable
    call    UpdateScrollBar
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @del_paint
    mov     g_cursorCol, eax
@del_paint:
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@k_pgup:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    sub     g_cursorLine, eax
    jns     @pgu_ok
    mov     g_cursorLine, 0
@pgu_ok:
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_pgdn:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    add     eax, g_cursorLine
    mov     ecx, g_lineCount
    dec     ecx
    cmp     eax, ecx
    jle     @pgd_ok
    mov     eax, ecx
@pgd_ok:
    test    eax, eax
    jns     @pgd_set
    xor     eax, eax
@pgd_set:
    mov     g_cursorLine, eax
    call    GetCurLineLen
    cmp     g_cursorCol, eax
    jle     @k_move
    mov     g_cursorCol, eax
    jmp     @k_move

@k_move:
    call    EnsureCursorVisible
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@wm_vscroll:
    mov     eax, dword ptr [rbp-18h]
    and     eax, 0FFFFh
    cmp     eax, SB_LINEUP
    je      @vs_up
    cmp     eax, SB_LINEDOWN
    je      @vs_dn
    cmp     eax, SB_PAGEUP
    je      @vs_pu
    cmp     eax, SB_PAGEDOWN
    je      @vs_pd
    cmp     eax, SB_THUMBTRACK
    je      @vs_thumb
    jmp     @ret_zero
@vs_up:
    mov     eax, g_scrollY
    test    eax, eax
    jz      @ret_zero
    dec     g_scrollY
    jmp     @vs_repaint
@vs_dn:
    mov     eax, g_scrollY
    inc     eax
    cmp     eax, g_lineCount
    jge     @ret_zero
    mov     g_scrollY, eax
    jmp     @vs_repaint
@vs_pu:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    sub     g_scrollY, eax
    jns     @vs_repaint
    mov     g_scrollY, 0
    jmp     @vs_repaint
@vs_pd:
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    add     eax, g_scrollY
    mov     ecx, g_lineCount
    cmp     eax, ecx
    jl      @vs_pd_ok
    lea     eax, [ecx-1]
@vs_pd_ok:
    test    eax, eax
    jns     @vs_pd_s
    xor     eax, eax
@vs_pd_s:
    mov     g_scrollY, eax
    jmp     @vs_repaint
@vs_thumb:
    mov     eax, dword ptr [rbp-18h]
    shr     eax, 16
    mov     g_scrollY, eax
@vs_repaint:
    call    UpdateScrollBar
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@wm_mousewheel:
    mov     eax, dword ptr [rbp-18h]
    shr     eax, 16
    movsx   eax, ax
    cdq
    mov     ecx, 120
    idiv    ecx
    imul    eax, 3
    neg     eax
    add     eax, g_scrollY
    test    eax, eax
    jns     @mw_max
    xor     eax, eax
@mw_max:
    mov     ecx, g_lineCount
    dec     ecx
    test    ecx, ecx
    jns     @mw_clamp
    xor     ecx, ecx
@mw_clamp:
    cmp     eax, ecx
    jle     @mw_set
    mov     eax, ecx
@mw_set:
    mov     g_scrollY, eax
    call    UpdateScrollBar
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── WM_TIMER: debounced ghost text trigger ───────────────────────
@wm_timer:
    ; wParam = timer ID
    mov     eax, dword ptr [rbp-18h]
    cmp     eax, TIMER_GHOST_DEBOUNCE
    jne     @ret_zero

    ; Kill the timer (one-shot behavior)
    mov     rcx, qword ptr [rbp-8]       ; hwnd
    mov     edx, TIMER_GHOST_DEBOUNCE    ; nIDEvent
    call    KillTimer
    mov     g_ghostTimerActive, 0

    ; Fire suggestion request (same as WM_USER_SUGGESTION_REQ)
    mov     rcx, qword ptr [rbp-8]
    mov     edx, WM_USER_SUGGESTION_REQ
    xor     r8d, r8d
    xor     r9d, r9d
    call    PostMessageW
    jmp     @ret_zero

@wm_user_suggestion_req:
    ; Prevent duplicate requests
    cmp     byte ptr [g_inferencePending], 0
    jne     @ret_zero
    
    ; Mark pending
    mov     byte ptr [g_inferencePending], 1
    
    ; Prepare context for bridge
    ; RCX = current text buffer, RDX=row, R8=col
    lea     rcx, g_textBuf
    mov     edx, [g_cursorLine]
    mov     r8d, [g_cursorCol]
    
    ; Call bridge (non-blocking)
    call    Bridge_RequestSuggestion
    ; If bridge returned < 0, clear pending flag so future requests aren't blocked
    test    eax, eax
    jns     @ret_zero
    mov     byte ptr [g_inferencePending], 0
    jmp     @ret_zero

; ── WM_LBUTTONDOWN: click-to-position caret ─────────────────────
@wm_lbuttondown:
    ; lParam is in [rbp-20h]: low word = X, high word = Y
    mov     rax, qword ptr [rbp-20h]
    movzx   ecx, ax              ; ecx = X (client)
    shr     rax, 16
    movzx   edx, ax              ; edx = Y (client)

    ; --- convert Y to line ---
    xor     eax, eax
    mov     eax, edx
    cdq
    mov     esi, LINE_HEIGHT
    idiv    esi                  ; eax = Y / LINE_HEIGHT
    add     eax, g_scrollY       ; eax = line index (0-based)
    ; clamp to [0, g_lineCount-1]
    test    eax, eax
    jns     @lb_miny
    xor     eax, eax
@lb_miny:
    mov     edi, g_lineCount
    dec     edi
    test    edi, edi
    jns     @lb_maxy
    xor     edi, edi
@lb_maxy:
    cmp     eax, edi
    jle     @lb_sety
    mov     eax, edi
@lb_sety:
    mov     g_cursorLine, eax

    ; --- convert X to column ---
    sub     ecx, GUTTER_WIDTH
    sub     ecx, 4               ; left padding
    test    ecx, ecx
    jns     @lb_colok
    xor     ecx, ecx
@lb_colok:
    cdq
    xor     edx, edx
    mov     eax, ecx
    mov     esi, CHAR_WIDTH
    div     esi                  ; eax = col
    ; clamp to line length
    push    rax
    call    GetCurLineLen         ; returns eax = length of current line
    mov     edi, eax
    pop     rax
    cmp     eax, edi
    jle     @lb_setx
    mov     eax, edi
@lb_setx:
    mov     g_cursorCol, eax

    ; clear selection
    mov     g_selStart, -1
    mov     g_selEnd, -1

    ; set keyboard focus to this window
    mov     rcx, qword ptr [rbp-8]
    call    SetFocus

    ; update caret + repaint
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── WM_RBUTTONUP: show context menu ─────────────────────────────
@wm_rbuttonup:
    ; Get cursor screen pos
    lea     rcx, [rbp-0D0h]  ; POINT struct
    call    GetCursorPos
    ; Create popup menu
    call    CreatePopupMenu
    mov     rbx, rax          ; hMenu

    ; Append items: AppendMenuW(hMenu, MF_STRING, id, text)
    mov     rcx, rbx
    xor     edx, edx          ; MF_STRING = 0
    mov     r8d, IDM_CUT
    lea     r9, szCut
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_COPY
    lea     r9, szCopy
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_PASTE
    lea     r9, szPaste
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_SELECTALL
    lea     r9, szSelectAll
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_DELETE
    lea     r9, szDelete
    call    AppendMenuW

    ; ── Separator + PE actions ──
    mov     rcx, rbx
    mov     edx, MF_SEPARATOR
    xor     r8d, r8d
    xor     r9d, r9d
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_GENERATE_PE
    lea     r9, szGeneratePE
    call    AppendMenuW

    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, IDM_SAVE_PE
    lea     r9, szSavePE
    call    AppendMenuW

    ; TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RETURNCMD, x, y, 0, hWnd, 0)
    sub     rsp, 16                ; two extra stack slots for params 6-7
    mov     qword ptr [rsp+40], 0   ; lpRect = NULL
    mov     rax, qword ptr [rbp-8]
    mov     qword ptr [rsp+32], rax ; hWnd
    mov     rcx, rbx
    mov     edx, TPM_LEFTALIGN or TPM_RETURNCMD
    mov     r8d, dword ptr [rbp-0D0h]   ; x
    mov     r9d, dword ptr [rbp-0CCh]   ; y
    call    TrackPopupMenu
    add     rsp, 16
    mov     esi, eax          ; save command ID

    ; Destroy menu
    mov     rcx, rbx
    call    DestroyMenu

    ; If a command was chosen, send WM_COMMAND
    test    esi, esi
    jz      @ret_zero
    mov     rcx, qword ptr [rbp-8]
    mov     edx, WM_COMMAND
    movzx   r8d, si
    xor     r9d, r9d
    call    DefWindowProcW
    jmp     @ret_zero

; ── WM_COMMAND: context menu actions ─────────────────────────────
@wm_command:
    mov     eax, dword ptr [rbp-18h]    ; wParam
    and     eax, 0FFFFh                 ; low word = command ID
    cmp     eax, IDM_GENERATE_PE
    je      @cmd_gen_pe
    cmp     eax, IDM_SAVE_PE
    je      @cmd_save_pe
    cmp     eax, IDM_GENERATE_FEATURE
    je      @cmd_gen_feature
    cmp     eax, IDM_PASTE
    je      @cmd_paste
    cmp     eax, IDM_COPY
    je      @cmd_copy
    cmp     eax, IDM_CUT
    je      @cmd_cut
    cmp     eax, IDM_SELECTALL
    je      @cmd_selall
    cmp     eax, IDM_DELETE
    je      @cmd_delete
    jmp     @ret_zero

@cmd_gen_feature:
    ; --- Generate Feature Handler (Phase E: wired to QTG bridge) ---
    ; 1. Show prompt dialog so user knows feature generation is starting
    ; 2. Call UIBridge_GenerateFeature(prompt, file) via C++ bridge
    ; MessageBoxW(hWnd, lpText, lpCaption, uType)
    xor     rcx, rcx                    ; hWnd=0
    lea     rdx, [szTaskPromptMessage]  ; lpText
    lea     r8,  [szTaskPromptTitle]    ; lpCaption
    mov     r9d, 1                      ; MB_OKCANCEL
    call    MessageBoxW
    cmp     eax, 2                      ; IDCANCEL
    je      @ret_zero

    ; Call the real QTG orchestrator bridge
    ; UIBridge_GenerateFeature(const char* prompt, const char* file)
    lea     rcx, [szQTGDefaultPrompt]
    lea     rdx, [szQTGDefaultFile]
    call    UIBridge_GenerateFeature
    ; RAX = session ID pointer or NULL on failure
    test    rax, rax
    jz      @qtg_fail
    
    ; Success notification
    xor     rcx, rcx
    lea     rdx, [szQTGSuccess]
    lea     r8,  [szTitle]
    mov     r9d, 40h                    ; MB_ICONINFORMATION
    call    MessageBoxW
    jmp     @ret_zero

@qtg_fail:
    xor     rcx, rcx
    lea     rdx, [szQTGFailed]
    lea     r8,  [szTitle]
    mov     r9d, 10h                    ; MB_ICONERROR
    call    MessageBoxW
    jmp     @ret_zero

@cmd_gen_pe:
    call    WritePEFile
    test    rax, rax
    jz      @cmd_gen_fail
    lea     rcx, [szPEGenerated]
    lea     rdx, [szTitle]
    mov     r8d, 0                      ; MB_OK
    call    MessageBoxA
    jmp     @ret_zero
@cmd_gen_fail:
    lea     rcx, [szPEFailed]
    lea     rdx, [szTitle]
    mov     r8d, 10h                    ; MB_ICONERROR
    call    MessageBoxA
    jmp     @ret_zero

@cmd_save_pe:
    call    SavePEToDisk
    test    rax, rax
    jz      @cmd_save_fail
    lea     rcx, [szPESaved]
    lea     rdx, [szTitle]
    mov     r8d, 0                      ; MB_OK
    call    MessageBoxA
    jmp     @ret_zero
@cmd_save_fail:
    lea     rcx, [szPESaveFailed]
    lea     rdx, [szTitle]
    mov     r8d, 10h                    ; MB_ICONERROR
    call    MessageBoxA
    jmp     @ret_zero

@cmd_paste:
    ; --- paste from clipboard ---
    mov     rcx, qword ptr [rbp-8]
    call    OpenClipboard
    test    eax, eax
    jz      @ret_zero
    mov     ecx, CF_UNICODETEXT
    call    GetClipboardData
    test    rax, rax
    jz      @cmd_pclose
    mov     rcx, rax
    call    GlobalLock
    test    rax, rax
    jz      @cmd_pclose
    mov     rsi, rax            ; rsi = pwszClipboard

    ; Count clip chars
    xor     ecx, ecx
@cmd_plen:
    cmp     word ptr [rsi + rcx*2], 0
    je      @cmd_pins
    inc     ecx
    jmp     @cmd_plen
@cmd_pins:
    ; ecx = clip length, insert at cursor position
    mov     edi, ecx            ; save clip len
    ; compute insertion offset
    mov     eax, g_cursorLine
    lea     rdx, g_lineOff
    mov     eax, dword ptr [rdx + rax*4]
    add     eax, g_cursorCol    ; eax = char offset
    ; shift buffer right by edi chars
    mov     r8d, g_totalChars
    sub     r8d, eax            ; r8d = chars to move
    lea     r10, g_textBuf
    lea     r11d, [r8d + edi]   ; not used, just for clarity
    ; memmove tail right
    mov     ecx, r8d
    test    ecx, ecx
    jle     @cmd_pcopy
    lea     rdx, [r10 + rax*2]           ; src
    lea     r11, [rdx + rdi*2]           ; dst (shifted right)
    ; copy backwards
    dec     ecx
@cmd_pmov:
    mov     r9w, word ptr [rdx + rcx*2]
    mov     word ptr [r11 + rcx*2], r9w
    dec     ecx
    jns     @cmd_pmov
@cmd_pcopy:
    ; copy clipboard text into gap
    lea     rdx, g_textBuf
    lea     rdx, [rdx + rax*2]
    xor     ecx, ecx
@cmd_pcloop:
    cmp     ecx, edi
    jge     @cmd_pupd
    mov     r9w, word ptr [rsi + rcx*2]
    mov     word ptr [rdx + rcx*2], r9w
    inc     ecx
    jmp     @cmd_pcloop
@cmd_pupd:
    ; Record undo for paste (type=insert)
    xor     ecx, ecx                    ; type = 0 (insert)
    mov     edx, eax                    ; charOffset = insertion offset
    mov     r8d, edi                    ; charCount = paste length
    xor     r9d, r9d                    ; no deleted text
    call    UndoPush
    ; edi preserved by UndoPush (lower 32 of rdi)
    add     g_totalChars, edi
    ; Advance cursor past pasted text
    ; Walk pasted chars to count newlines and find final column
    ; eax still = insertion offset, edi = paste length
    ; Re-compute: scan pasted text for newlines
    push    rdi                  ; save paste len
    lea     r10, g_textBuf
    ; eax was clobbered by copy loop — recompute from cursor
    mov     eax, g_cursorLine
    lea     rdx, g_lineOff
    mov     eax, dword ptr [rdx + rax*4]
    add     eax, g_cursorCol     ; eax = original insertion offset
    lea     r10, [r10 + rax*2]   ; r10 = &g_textBuf[insertionOffset]
    xor     ecx, ecx             ; newline count
    xor     r8d, r8d             ; chars since last newline
    xor     edx, edx             ; loop counter
@cmd_p_scan:
    cmp     edx, edi
    jge     @cmd_p_scan_done
    cmp     word ptr [r10 + rdx*2], 0Ah
    jne     @cmd_p_scan_nonnl
    inc     ecx
    xor     r8d, r8d
    jmp     @cmd_p_scan_next
@cmd_p_scan_nonnl:
    inc     r8d
@cmd_p_scan_next:
    inc     edx
    jmp     @cmd_p_scan
@cmd_p_scan_done:
    add     g_cursorLine, ecx
    test    ecx, ecx
    jz      @cmd_p_no_nl
    mov     g_cursorCol, r8d     ; col = chars after last newline
    jmp     @cmd_p_adv_done
@cmd_p_no_nl:
    add     g_cursorCol, edi     ; no newlines: advance col by paste len
@cmd_p_adv_done:
    pop     rdi
    ; unlock + close
    mov     rcx, rsi
    call    GlobalUnlock
@cmd_pclose:
    call    CloseClipboard
    ; rebuild + repaint
    call    RebuildLineTable
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@cmd_copy:
@cmd_cut:
    ; --- copy/cut selected text ---
    mov     eax, g_selStart
    cmp     eax, -1
    je      @ret_zero
    mov     edi, g_selEnd
    cmp     eax, edi
    jle     @cmd_cc_ok
    xchg    eax, edi
@cmd_cc_ok:
    ; eax = start, edi = end
    mov     esi, edi
    sub     esi, eax            ; esi = selection length
    test    esi, esi
    jle     @ret_zero
    push    rax                 ; save start offset

    ; Allocate global mem
    mov     ecx, 42h            ; GMEM_MOVEABLE | GMEM_ZEROINIT
    lea     edx, [esi*2 + 2]   ; bytes = (len+1)*2
    call    GlobalAlloc
    test    rax, rax
    jz      @cmd_cc_fail
    mov     rbx, rax            ; hMem
    mov     rcx, rax
    call    GlobalLock
    test    rax, rax
    jz      @cmd_cc_fail
    mov     rdi, rax            ; pDst
    pop     rax                 ; start offset
    lea     r10, g_textBuf
    lea     r10, [r10 + rax*2]       ; r10 = &textBuf[start]
    xor     ecx, ecx
@cmd_ccloop:
    cmp     ecx, esi
    jge     @cmd_ccend
    mov     r9w, word ptr [r10 + rcx*2]
    mov     word ptr [rdi + rcx*2], r9w
    inc     ecx
    jmp     @cmd_ccloop
@cmd_ccend:
    mov     word ptr [rdi + rcx*2], 0
    mov     rcx, rbx
    call    GlobalUnlock

    ; Set clipboard
    mov     rcx, qword ptr [rbp-8]
    call    OpenClipboard
    test    eax, eax
    jz      @ret_zero
    call    EmptyClipboard
    mov     ecx, CF_UNICODETEXT
    mov     rdx, rbx
    call    SetClipboardData
    call    CloseClipboard

    ; If cut (IDM_CUT), check original command
    mov     eax, dword ptr [rbp-18h]
    and     eax, 0FFFFh
    cmp     eax, IDM_CUT
    jne     @ret_zero
    ; Delete selected text
    mov     eax, g_selStart
    mov     edi, g_selEnd
    cmp     eax, edi
    jle     @cmd_cutok
    xchg    eax, edi
@cmd_cutok:
    mov     ecx, edi
    sub     ecx, eax            ; ecx = chars to delete
    ; shift buffer left
    lea     r10, g_textBuf
    mov     edx, g_totalChars
    sub     edx, edi            ; edx = trailing chars
    test    edx, edx
    jle     @cmd_cutdone
    lea     r11, [r10 + rdi*2]   ; r11 = &textBuf[end]
    lea     r10, [r10 + rax*2]   ; r10 = &textBuf[start]
    xor     esi, esi
@cmd_cutmov:
    cmp     esi, edx
    jge     @cmd_cutdone
    mov     r9w, word ptr [r11 + rsi*2]
    mov     word ptr [r10 + rsi*2], r9w
    inc     esi
    jmp     @cmd_cutmov
@cmd_cutdone:
    sub     g_totalChars, ecx
    mov     g_selStart, -1
    mov     g_selEnd, -1
    call    RebuildLineTable
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero
@cmd_cc_fail:
    pop     rax
    jmp     @ret_zero

@cmd_selall:
    ; Select entire buffer
    mov     g_selStart, 0
    mov     eax, g_totalChars
    mov     g_selEnd, eax
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

@cmd_delete:
    ; Delete selection
    mov     eax, g_selStart
    cmp     eax, -1
    je      @ret_zero
    mov     edi, g_selEnd
    cmp     eax, edi
    jle     @cmd_delok
    xchg    eax, edi
@cmd_delok:
    mov     ecx, edi
    sub     ecx, eax
    test    ecx, ecx
    jle     @ret_zero
    lea     r10, g_textBuf
    mov     edx, g_totalChars
    sub     edx, edi
    test    edx, edx
    jle     @cmd_deldone
    lea     r11, [r10 + rdi*2]   ; r11 = &textBuf[end]
    lea     r10, [r10 + rax*2]   ; r10 = &textBuf[start]
    xor     esi, esi
@cmd_delmov:
    cmp     esi, edx
    jge     @cmd_deldone
    mov     r9w, word ptr [r11 + rsi*2]
    mov     word ptr [r10 + rsi*2], r9w
    inc     esi
    jmp     @cmd_delmov
@cmd_deldone:
    sub     g_totalChars, ecx
    mov     g_selStart, -1
    mov     g_selEnd, -1
    call    RebuildLineTable
    call    PositionCaret
    mov     rcx, qword ptr [rbp-8]
    xor     edx, edx
    mov     r8d, 1
    call    InvalidateRect
    jmp     @ret_zero

; ── WM_SETCURSOR: set I-beam cursor in client area ──────────────
@wm_setcursor:
    ; Only change cursor if in client area (LOWORD(lParam) == HTCLIENT == 1)
    mov     eax, dword ptr [rbp-20h]
    and     eax, 0FFFFh
    cmp     eax, 1               ; HTCLIENT
    jne     @wm_setcursor_def
    ; Load I-beam cursor
    xor     ecx, ecx            ; hInstance = NULL (system cursor)
    mov     edx, 32513           ; IDC_IBEAM
    call    LoadCursorW
    test    rax, rax
    jz      @ret_zero
    mov     rcx, rax
    call    SetCursor
    ; Return TRUE to prevent DefWindowProc from resetting cursor
    mov     eax, 1
    jmp     @wp_ret
@wm_setcursor_def:
    mov     rcx, qword ptr [rbp-8]
    mov     edx, dword ptr [rbp-0Ch]
    mov     r8, qword ptr [rbp-18h]
    mov     r9, qword ptr [rbp-20h]
    call    DefWindowProcW
    jmp     @wp_ret

;=============================================================================
;============================================================================  
; WM_PAINT - Native Rendering + Multi-line Ghost Text Pass
;============================================================================  

@wm_paint:
    mov     rcx, qword ptr [rbp-8]
    lea     rdx, [rbp-60h]
    call    BeginPaint
    mov     rbx, rax                     ; rbx = hdc

    ; ── Fill entire client rect with dark background ──
    mov     rcx, BG_COLOR
    call    CreateSolidBrush
    test    rax, rax
    jz      @p_skip_fill
    mov     qword ptr [rbp-0B0h], rax    ; save brush handle
    ; Build RECT {0, 0, clientW, clientH} on stack
    sub     rsp, 20h
    mov     dword ptr [rsp],    0        ; left
    mov     dword ptr [rsp+4],  0        ; top
    mov     eax, g_clientW
    mov     dword ptr [rsp+8],  eax      ; right
    mov     eax, g_clientH
    mov     dword ptr [rsp+0Ch], eax     ; bottom
    ; FillRect(hdc, &rect, hBrush)
    mov     rcx, rbx
    lea     rdx, [rsp]
    mov     r8, qword ptr [rbp-0B0h]
    call    FillRect
    add     rsp, 20h
    ; Delete the brush
    mov     rcx, qword ptr [rbp-0B0h]
    call    DeleteObject
@p_skip_fill:

    ; Set text background to match
    mov     rcx, rbx
    mov     edx, BG_COLOR
    call    SetBkColor

    ; Set default text color (light grey on dark)
    mov     rcx, rbx
    mov     edx, 00CCCCCCh               ; light grey text
    call    SetTextColor

    ; Select font
    mov     rcx, rbx
    mov     rdx, g_hFont
    call    SelectObject

    ; Calculate visible range
    mov     esi, g_scrollY               ; esi = current line
    mov     eax, g_clientH
    cdq
    mov     ecx, LINE_HEIGHT
    idiv    ecx
    mov     edi, eax                     ; edi = visible lines
    inc     edi

@p_line:
    test    edi, edi
    jle     @p_paint_ghost               ; Go to ghost pass after main text

    ; Check if line is within total range
    cmp     esi, g_lineCount
    jge     @p_paint_ghost

    ; Get line length and offset
    lea     rax, g_lineOff
    mov     edx, dword ptr [rax + rsi*4] ; edx = start offset
    mov     ecx, esi
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @p_last
    mov     eax, dword ptr [rax + rcx*4]
    sub     eax, edx
    dec     eax                          ; Subtract newline
    jmp     @p_draw
@p_last:
    mov     eax, g_totalChars
    sub     eax, edx
@p_draw:
    mov     dword ptr [rbp-0F0h], eax    ; Save line char count

    ; ── Gutter line number ──
    mov     eax, esi
    inc     eax                          ; 1-based line number
    call    FormatLineNum

    ; Set gutter color
    mov     rcx, rbx
    mov     edx, GUTTER_TEXT_COLOR
    call    SetTextColor
    mov     dword ptr [rbp-0F4h], eax    ; save old text color

    ; Draw gutter number: TextOutW(hdc, 2, Y, g_numBuf, 5)
    sub     rsp, 30h
    mov     dword ptr [rsp+20h], 5       ; 5 digits max
    mov     rcx, rbx                     ; hdc
    mov     edx, 2                       ; X = 2px margin
    mov     r8d, esi
    sub     r8d, g_scrollY
    imul    r8d, LINE_HEIGHT             ; Y
    lea     r9, g_numBuf
    call    TextOutW
    add     rsp, 30h

    ; Restore text color
    mov     rcx, rbx
    mov     edx, dword ptr [rbp-0F4h]
    call    SetTextColor

    ; ── Selection highlight check ──
    mov     eax, dword ptr [rbp-0F0h]    ; Restore line char count
    test    eax, eax
    jle     @p_next_line

    ; Check if this line overlaps selection
    mov     ecx, g_selStart
    cmp     ecx, -1
    je      @p_no_sel                    ; No selection active

    ; Compute line's char range: [lineStart, lineStart+lineLen)
    lea     r10, g_lineOff
    mov     r10d, dword ptr [r10 + rsi*4] ; r10d = lineStart
    mov     r11d, r10d
    add     r11d, eax                    ; r11d = lineEnd

    ; Normalise selection range
    mov     ecx, g_selStart
    mov     edx, g_selEnd
    cmp     ecx, edx
    jle     @p_sel_norm
    xchg    ecx, edx
@p_sel_norm:
    ; ecx = selMin, edx = selMax
    ; Check overlap: selMin < lineEnd && selMax > lineStart
    cmp     ecx, r11d
    jge     @p_no_sel
    cmp     edx, r10d
    jle     @p_no_sel

    ; Selection overlaps — set highlight colors
    push    rax                          ; save char count
    mov     rcx, rbx
    mov     edx, SEL_BK_COLOR
    call    SetBkColor
    mov     dword ptr [rbp-0F8h], eax    ; save old bk color
    mov     rcx, rbx
    mov     edx, OPAQUE
    call    SetBkMode
    mov     dword ptr [rbp-0FCh], eax    ; save old bk mode
    mov     rcx, rbx
    mov     edx, SEL_TEXT_COLOR
    call    SetTextColor
    mov     dword ptr [rbp-100h], eax    ; save old text color
    pop     rax                          ; restore char count
    jmp     @p_do_textout

@p_no_sel:
    ; No selection — normal draw

@p_do_textout:
    ; TextOutW(hdc, x, y, lpString, count)
    ; sub rsp, 30h for 16-byte alignment (rsp was already 16-aligned)
    sub     rsp, 30h
    mov     dword ptr [rsp+20h], eax     ; 5th param: count

    mov     rcx, rbx                     ; hdc
    mov     edx, GUTTER_WIDTH + 4        ; X
    mov     r8d, esi
    sub     r8d, g_scrollY
    imul    r8d, LINE_HEIGHT             ; Y

    lea     r9, g_lineOff
    mov     r11d, dword ptr [r9 + rsi*4]
    lea     r9, g_textBuf
    lea     r9, [r9 + r11*2]             ; lpString

    call    TextOutW
    add     rsp, 30h

    ; Restore GDI state if selection was highlighted
    mov     ecx, g_selStart
    cmp     ecx, -1
    je      @p_next_line

    ; Only restore if we actually set selection colors
    ; (recheck overlap for this line)
    lea     r10, g_lineOff
    mov     r10d, dword ptr [r10 + rsi*4]
    mov     r11d, r10d
    add     r11d, dword ptr [rbp-0F0h]
    mov     ecx, g_selStart
    mov     edx, g_selEnd
    cmp     ecx, edx
    jle     @p_restore_norm
    xchg    ecx, edx
@p_restore_norm:
    cmp     ecx, r11d
    jge     @p_next_line
    cmp     edx, r10d
    jle     @p_next_line

    ; Restore bg color
    mov     rcx, rbx
    mov     edx, dword ptr [rbp-0F8h]
    call    SetBkColor
    ; Restore bk mode
    mov     rcx, rbx
    mov     edx, dword ptr [rbp-0FCh]
    call    SetBkMode
    ; Restore text color
    mov     rcx, rbx
    mov     edx, dword ptr [rbp-100h]
    call    SetTextColor

@p_next_line:
    inc     esi
    dec     edi
    jmp     @p_line

@p_paint_ghost:
    ; ================================================================
    ; MULTI-LINE GHOST TEXT RENDER PASS
    ; Renders ALL visible ghost lines in a single pass after main text.
    ; Uses italic font + grey color for visual distinction.
    ; Saves/restores GDI state via local variables (not push/pop) to
    ; maintain 16-byte stack alignment for all Win32 calls.
    ; ================================================================
    cmp     byte ptr [g_ghostActive], 0
    je      @p_done                      ; No ghost → skip to EndPaint

    ; Select italic ghost font
    mov     rcx, rbx                     ; hdc
    mov     rdx, g_hGhostFont
    test    rdx, rdx
    jnz     @p_ghost_has_font
    mov     rdx, g_hFont                 ; Fallback: use normal font
@p_ghost_has_font:
    call    SelectObject
    mov     qword ptr [rbp-0D0h], rax    ; Save old font handle

    ; Set ghost text color (grey)
    mov     rcx, rbx
    mov     edx, GHOST_TEXT_COLOR
    call    SetTextColor
    mov     dword ptr [rbp-0D8h], eax    ; Save old text color

    ; Set transparent background
    mov     rcx, rbx
    mov     edx, 1                       ; TRANSPARENT = 1
    call    SetBkMode
    mov     dword ptr [rbp-0DCh], eax    ; Save old bk mode

    ; Loop through all ghost lines
    xor     r12d, r12d                   ; r12d = ghost line index

@p_ghost_line_loop:
    cmp     r12d, [g_ghostLineCount]
    jge     @p_ghost_restore

    ; Calculate screen line = g_ghostCursorRow + r12d
    mov     eax, [g_ghostCursorRow]
    add     eax, r12d                    ; eax = absolute line index

    ; Bounds check: is this line on screen?
    mov     ecx, g_scrollY
    cmp     eax, ecx
    jl      @p_ghost_next_line           ; Above viewport
    mov     dword ptr [rbp-0E0h], eax    ; Save line index
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    mov     ecx, eax                     ; ecx = max visible rows
    mov     eax, dword ptr [rbp-0E0h]    ; Restore line index
    mov     edx, eax
    sub     edx, g_scrollY               ; edx = screen row
    cmp     edx, ecx
    jge     @p_ghost_next_line           ; Below viewport

    ; Y position
    imul    edx, LINE_HEIGHT             ; edx = Y in pixels
    mov     r13d, edx                    ; r13d = Y

    ; X position: first ghost line appends after caret, rest start at gutter
    mov     r14d, GUTTER_WIDTH + 4       ; Default X = gutter
    test    r12d, r12d
    jnz     @p_ghost_use_x
    add     r14d, g_caretX               ; First line: after cursor
@p_ghost_use_x:

    ; Get ghost line offset and length
    lea     r8, [g_ghostLineOffsets]
    mov     r15d, dword ptr [r8 + r12*4] ; r15d = char offset in buffer
    lea     r8, [g_ghostLineLengths]
    mov     eax, dword ptr [r8 + r12*4]  ; eax = length in chars

    ; Skip empty lines
    test    eax, eax
    jz      @p_ghost_next_line

    ; TextOutW(hdc, X, Y, &g_ghostTextBuffer[offset], length)
    mov     rcx, rbx                     ; HDC
    mov     edx, r14d                    ; X
    mov     r8d, r13d                    ; Y
    lea     r9, [g_ghostTextBuffer]
    movzx   r10d, r15w
    lea     r9, [r9 + r10*2]             ; String ptr at offset

    sub     rsp, 30h
    mov     dword ptr [rsp+20h], eax     ; Count
    call    TextOutW
    add     rsp, 30h

@p_ghost_next_line:
    inc     r12d
    jmp     @p_ghost_line_loop

@p_ghost_restore:
    ; Restore background mode
    mov     edx, dword ptr [rbp-0DCh]    ; Old bk mode
    mov     rcx, rbx
    call    SetBkMode

    ; Restore text color
    mov     edx, dword ptr [rbp-0D8h]    ; Old text color
    mov     rcx, rbx
    call    SetTextColor

    ; Restore original font
    mov     rdx, qword ptr [rbp-0D0h]   ; Old font handle
    mov     rcx, rbx
    call    SelectObject

    jmp     @p_done                      ; Fall through to EndPaint
@p_done:
    mov     rcx, qword ptr [rbp-8]
    lea     rdx, [rbp-60h]
    call    EndPaint
    jmp     @ret_zero

@wm_destroy:
    xor     ecx, ecx
    call    PostQuitMessage
    jmp     @ret_zero

@ret_zero:
    xor     eax, eax
@wp_ret:
    lea     rsp, [rbp]
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
WndProc ENDP

RebuildLineTable PROC
    lea     rdi, g_textBuf
    lea     rdx, g_lineOff
    xor     ecx, ecx
    mov     dword ptr [rdx], 0
    inc     ecx
    xor     eax, eax
@rlt_loop:
    cmp     eax, g_totalChars
    jge     @rlt_done
    cmp     word ptr [rdi + rax*2], 0Ah
    jne     @rlt_next
    lea     r8d, [eax+1]
    cmp     ecx, MAX_LINES
    jge     @rlt_done
    mov     dword ptr [rdx + rcx*4], r8d
    inc     ecx
@rlt_next:
    inc     eax
    jmp     @rlt_loop
@rlt_done:
    mov     g_lineCount, ecx
    ret
RebuildLineTable ENDP

GetCurLineLen PROC
    mov     ecx, g_cursorLine
    lea     rax, g_lineOff
    mov     edx, dword ptr [rax + rcx*4]
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @gcl_last
    mov     eax, dword ptr [rax + rcx*4]
    sub     eax, edx
    dec     eax
    ret
@gcl_last:
    mov     eax, g_totalChars
    sub     eax, edx
    ret
GetCurLineLen ENDP

PositionCaret PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    mov     eax, g_cursorCol
    imul    eax, CHAR_WIDTH
    mov     g_caretX, eax ; Update globally for Ghost Text
    add     eax, GUTTER_WIDTH + 4
    mov     ecx, eax
    mov     edx, g_cursorLine
    sub     edx, g_scrollY
    imul    edx, LINE_HEIGHT
    call    SetCaretPos
    add     rsp, 28h
    ret
PositionCaret ENDP

EnsureCursorVisible PROC
    mov     eax, g_clientH
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    test    eax, eax
    jz      @ecv_done
    mov     ecx, eax
    mov     eax, g_cursorLine
    cmp     eax, g_scrollY
    jl      @ecv_up
    mov     edx, g_scrollY
    add     edx, ecx
    dec     edx
    cmp     eax, edx
    jg      @ecv_dn
    jmp     @ecv_done
@ecv_up:
    mov     g_scrollY, eax
    jmp     @ecv_done
@ecv_dn:
    mov     edx, eax
    sub     edx, ecx
    inc     edx
    test    edx, edx
    jns     @ecv_set
    xor     edx, edx
@ecv_set:
    mov     g_scrollY, edx
@ecv_done:
    ret
EnsureCursorVisible ENDP

UpdateScrollBar PROC FRAME
    sub     rsp, 48h
    .allocstack 48h
    .endprolog
    lea     rax, [rsp+20h]
    mov     dword ptr [rax], 28
    mov     dword ptr [rax+4], SIF_ALL
    mov     dword ptr [rax+8], 0
    mov     ecx, g_lineCount
    mov     dword ptr [rax+0Ch], ecx
    mov     ecx, g_clientH
    push    rax
    mov     eax, ecx
    xor     edx, edx
    mov     ecx, LINE_HEIGHT
    div     ecx
    mov     ecx, eax
    pop     rax
    mov     dword ptr [rax+10h], ecx
    mov     ecx, g_scrollY
    mov     dword ptr [rax+14h], ecx
    mov     dword ptr [rax+18h], 0
    mov     rcx, hMainWnd
    mov     edx, SB_VERT
    lea     r8, [rsp+20h]
    mov     r9d, 1
    call    SetScrollInfo
    add     rsp, 48h
    ret
UpdateScrollBar ENDP

FormatLineNum PROC
    lea     rdi, g_numBuf
    mov     word ptr [rdi],    20h
    mov     word ptr [rdi+2],  20h
    mov     word ptr [rdi+4],  20h
    mov     word ptr [rdi+6],  20h
    mov     word ptr [rdi+8],  20h
    mov     word ptr [rdi+10], 0
    lea     rdi, [rdi+8]
    mov     ecx, 0
@fln_loop:
    xor     edx, edx
    mov     r8d, 10
    div     r8d
    add     edx, 30h
    mov     word ptr [rdi], dx
    sub     rdi, 2
    inc     ecx
    test    eax, eax
    jnz     @fln_loop
    ret
FormatLineNum ENDP

;=============================================================================
; Bridge_OnSuggestionReady (called from C++ via function pointer)
; extern "C" void Bridge_OnSuggestionReady(const WCHAR* text, int len);
;=============================================================================

Bridge_OnSuggestionReady PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog
    ; RCX = text ptr, RDX = total len
    
    ; Empty callback = inference failed/offline; just clear pending flag
    test    edx, edx
    jg      @bosr_has_text
    mov     byte ptr [g_inferencePending], 0
    add     rsp, 28h
    ret
@bosr_has_text:
    
    ; Validate length
    cmp     edx, MAX_GHOST_LEN
    jle     @F
    mov     edx, MAX_GHOST_LEN
@@:
    push    rdx                          ; Save total length
    
    ; Copy to buffer
    lea     rdi, [g_ghostTextBuffer]
    mov     rsi, rcx
    mov     ecx, edx
    rep     movsw
    
    ; Parse lines into offsets
    pop     rdx                          ; Restore total length
    lea     rsi, [g_ghostTextBuffer]
    lea     rdi, [g_ghostLineOffsets]
    lea     r8,  [g_ghostLineLengths]
    xor     eax, eax                     ; Current char offset
    xor     ecx, ecx                     ; Current line count
    mov     dword ptr [rdi], 0           ; First line offset is 0
    
@parse_loop:
    cmp     eax, edx
    jge     @parse_done
    cmp     ecx, MAX_GHOST_LINES
    jge     @parse_done
    
    mov     r9w, word ptr [rsi + rax*2]
    cmp     r9w, 0Ah                     ; Newline?
    jne     @parse_next
    
    ; End of line found
    ; Length = current_offset - start_offset
    mov     r10d, eax
    sub     r10d, dword ptr [rdi + rcx*4]
    mov     dword ptr [r8 + rcx*4], r10d
    
    inc     ecx
    lea     r11d, [eax + 1]
    mov     dword ptr [rdi + rcx*4], r11d
    
@parse_next:
    inc     eax
    jmp     @parse_loop
    
@parse_done:
    ; Handle last line length if no trailing newline
    mov     r10d, eax
    sub     r10d, dword ptr [rdi + rcx*4]
    mov     dword ptr [r8 + rcx*4], r10d
    inc     ecx
    mov     [g_ghostLineCount], ecx
    
    ; Activate ghost
    mov     byte ptr [g_ghostActive], 1
    
    ; Store position where ghost appears
    mov     eax, [g_cursorLine]
    mov     [g_ghostCursorRow], eax
    mov     eax, [g_cursorCol]
    mov     [g_ghostCursorCol], eax
    
    ; Clear pending flag
    mov     byte ptr [g_inferencePending], 0
    
    ; Trigger repaint to show ghost
    mov     rcx, hMainWnd                ; Use global HWND
    xor     edx, edx                     ; LPRECT = NULL (whole client)
    mov     r8d, 1                       ; bErase = TRUE
    call    InvalidateRect
    
    add     rsp, 28h
    ret
Bridge_OnSuggestionReady ENDP

PUBLIC Bridge_OnSuggestionReady

;=============================================================================
; Phase 4B: ScoreCandidates — Neural Pruning Scorer (pure register math)
; Scores a single candidate against the current editor context.
; Input:  RCX = ptr to candidate WCHAR buffer
;         EDX = candidate length (chars)
;         R8  = ptr to context (g_textBuf at cursor)
;         R9D = context length (chars before cursor)
; Output: EAX = score (0-255, higher = better)
;
; Scoring heuristics (no model weights — pure pattern matching):
;   1. Prefix continuity:  Does candidate continue the last partial token?
;   2. Bracket balance:    Does candidate maintain (){}[] balance?
;   3. Line length sanity: Penalize extremely long single lines
;   4. Repetition penalty: Penalize repeated trigrams within the candidate
;   5. Keyword bonus:      Reward language keywords (if/for/while/return/def)
;=============================================================================

ScoreCandidate PROC FRAME
    push    rbp
    .pushreg rbp
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    mov     rbp, rsp
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    ; Save args
    mov     [rbp-8],  rcx          ; candidate ptr
    mov     [rbp-0Ch], edx         ; candidate len
    mov     [rbp-18h], r8          ; context ptr
    mov     [rbp-1Ch], r9d         ; context len

    xor     r12d, r12d             ; r12d = total score (accumulator)

    ; ── Heuristic 1: Prefix continuity (0-80 points) ──
    ; Check if first 1-4 chars of candidate match last 1-4 chars of context
    mov     esi, r9d
    test    esi, esi
    jz      @sc_h2                 ; No context = skip
    mov     edi, edx
    test    edi, edi
    jz      @sc_h2

    ; Compare last char of context with first char of candidate
    mov     rax, r8                ; context ptr
    dec     esi
    movzx   eax, word ptr [rax + rsi*2]    ; Last context char
    mov     rcx, [rbp-8]
    movzx   ecx, word ptr [rcx]            ; First candidate char

    ; If candidate starts with space/newline after non-space context = OK (+30)
    cmp     cl, 20h                ; space?
    je      @sc_space_bonus
    cmp     cl, 0Ah                ; newline?
    je      @sc_space_bonus
    ; If both are alphanumeric, likely mid-token continuation (+60)
    cmp     al, 30h
    jb      @sc_h2
    cmp     al, 7Ah
    ja      @sc_h2
    cmp     cl, 30h
    jb      @sc_h2
    cmp     cl, 7Ah
    ja      @sc_h2
    add     r12d, 60               ; Mid-token continuation bonus
    jmp     @sc_h2
@sc_space_bonus:
    add     r12d, 30               ; Natural word boundary

    ; ── Heuristic 2: Bracket balance (0-50 points, penalty if imbalanced) ──
@sc_h2:
    mov     rcx, [rbp-8]           ; candidate ptr
    mov     edx, [rbp-0Ch]         ; candidate len
    xor     esi, esi               ; paren count
    xor     edi, edi               ; brace count
    xor     r8d, r8d               ; bracket count
    xor     eax, eax               ; loop index
@sc_bracket_loop:
    cmp     eax, edx
    jge     @sc_bracket_done
    movzx   r9d, word ptr [rcx + rax*2]
    cmp     r9d, 28h               ; '('
    jne     @sc_br1
    inc     esi
    jmp     @sc_br_next
@sc_br1:
    cmp     r9d, 29h               ; ')'
    jne     @sc_br2
    dec     esi
    jmp     @sc_br_next
@sc_br2:
    cmp     r9d, 7Bh               ; '{'
    jne     @sc_br3
    inc     edi
    jmp     @sc_br_next
@sc_br3:
    cmp     r9d, 7Dh               ; '}'
    jne     @sc_br4
    dec     edi
    jmp     @sc_br_next
@sc_br4:
    cmp     r9d, 5Bh               ; '['
    jne     @sc_br5
    inc     r8d
    jmp     @sc_br_next
@sc_br5:
    cmp     r9d, 5Dh               ; ']'
    jne     @sc_br_next
    dec     r8d
@sc_br_next:
    inc     eax
    jmp     @sc_bracket_loop
@sc_bracket_done:
    ; Perfect balance = +50, each imbalance = -10
    add     r12d, 50
    ; Absolute values of imbalances
    mov     eax, esi
    test    eax, eax
    jns     @sc_abs_p
    neg     eax
@sc_abs_p:
    imul    eax, 10
    sub     r12d, eax
    mov     eax, edi
    test    eax, eax
    jns     @sc_abs_b
    neg     eax
@sc_abs_b:
    imul    eax, 10
    sub     r12d, eax
    mov     eax, r8d
    test    eax, eax
    jns     @sc_abs_k
    neg     eax
@sc_abs_k:
    imul    eax, 10
    sub     r12d, eax

    ; ── Heuristic 3: Line length sanity (0-40, penalty for >120 chars) ──
    mov     rcx, [rbp-8]
    mov     edx, [rbp-0Ch]
    xor     esi, esi               ; current line length
    xor     edi, edi               ; max line length
    xor     eax, eax
@sc_linelen_loop:
    cmp     eax, edx
    jge     @sc_linelen_done
    movzx   r9d, word ptr [rcx + rax*2]
    cmp     r9d, 0Ah
    jne     @sc_ll_inc
    cmp     esi, edi
    jle     @sc_ll_reset
    mov     edi, esi
@sc_ll_reset:
    xor     esi, esi
    jmp     @sc_ll_next
@sc_ll_inc:
    inc     esi
@sc_ll_next:
    inc     eax
    jmp     @sc_linelen_loop
@sc_linelen_done:
    cmp     esi, edi               ; Final line check
    jle     @sc_ll_score
    mov     edi, esi
@sc_ll_score:
    add     r12d, 40
    cmp     edi, 120
    jle     @sc_h4
    sub     edi, 120
    shr     edi, 2                 ; penalty = (len-120)/4, capped
    cmp     edi, 40
    jle     @sc_ll_pen
    mov     edi, 40
@sc_ll_pen:
    sub     r12d, edi

    ; ── Heuristic 4: Repetition penalty (0-40, -5 per repeated trigram) ──
@sc_h4:
    add     r12d, 40
    ; Simple check: compare chars [0..2] with chars [3..5], etc.
    mov     rcx, [rbp-8]
    mov     edx, [rbp-0Ch]
    cmp     edx, 6
    jl      @sc_h5                 ; Too short for trigram check
    xor     eax, eax               ; repeat count
    mov     esi, 0                 ; i = 0
@sc_tri_loop:
    lea     edi, [esi+3]
    cmp     edi, edx
    jg      @sc_tri_done
    ; Compare trigram at [esi] with trigram at [esi+3]
    movzx   r8d,  word ptr [rcx + rsi*2]
    movzx   r9d,  word ptr [rcx + rdi*2]
    cmp     r8d, r9d
    jne     @sc_tri_next
    movzx   r8d,  word ptr [rcx + rsi*2 + 2]
    movzx   r9d,  word ptr [rcx + rdi*2 + 2]
    cmp     r8d, r9d
    jne     @sc_tri_next
    movzx   r8d,  word ptr [rcx + rsi*2 + 4]
    movzx   r9d,  word ptr [rcx + rdi*2 + 4]
    cmp     r8d, r9d
    jne     @sc_tri_next
    inc     eax                    ; Trigram repeated
@sc_tri_next:
    add     esi, 3
    jmp     @sc_tri_loop
@sc_tri_done:
    imul    eax, 5
    cmp     eax, 40
    jle     @sc_tri_apply
    mov     eax, 40
@sc_tri_apply:
    sub     r12d, eax

    ; ── Heuristic 5: Keyword bonus (0-50 points) ──
    ; Scan candidate for any keyword from g_kw_tbl.
    ; Each match: +25 (capped at +50 total).
    ; Case-insensitive: OR 0x20 to force lowercase for A-Z range.
@sc_h5:
    mov     rcx, [rbp-8]              ; candidate ptr
    mov     edx, [rbp-0Ch]            ; candidate len
    cmp     edx, 2
    jl      @sc_clamp                 ; too short for any keyword
    xor     r13d, r13d                ; keyword bonus accumulator

    lea     r14, g_kw_tbl             ; r14 = keyword table pointer
@sc_kw_outer:
    movzx   eax, byte ptr [r14]       ; keyword length
    test    eax, eax
    jz      @sc_kw_apply              ; sentinel = done
    mov     dword ptr [rbp-28h], eax  ; save kwLen
    lea     r15, [r14 + 1]            ; r15 = keyword WCHAR data (skip len byte)

    ; Slide candidate looking for this keyword
    ; Valid start positions: 0 .. (candidateLen - kwLen)
    mov     esi, edx                  ; candidateLen
    sub     esi, eax                  ; last valid start = len - kwLen
    js      @sc_kw_advance            ; keyword longer than candidate
    xor     edi, edi                  ; candidate scan index
@sc_kw_scan:
    cmp     edi, esi
    jg      @sc_kw_advance
    ; Check if position edi starts at a word boundary
    ; (index 0, or prev char is space/newline/semicolon/tab/comma)
    test    edi, edi
    jz      @sc_kw_check
    movzx   eax, word ptr [rcx + rdi*2 - 2]
    cmp     eax, 20h                  ; space
    je      @sc_kw_check
    cmp     eax, 0Ah                  ; newline
    je      @sc_kw_check
    cmp     eax, 09h                  ; tab
    je      @sc_kw_check
    cmp     eax, 3Bh                  ; semicolon
    je      @sc_kw_check
    cmp     eax, 2Ch                  ; comma
    je      @sc_kw_check
    jmp     @sc_kw_scan_next

@sc_kw_check:
    ; Compare kwLen WCHARs case-insensitively
    mov     eax, dword ptr [rbp-28h]  ; kwLen
    xor     r8d, r8d                  ; comparison index
@sc_kw_cmp:
    cmp     r8d, eax
    jge     @sc_kw_match              ; all chars matched!
    movzx   r9d, word ptr [rcx + rdi*2]  ; candidate char
    add     edi, 1                    ; advance for next
    ; To-lower: if 41h..5Ah, OR 20h
    cmp     r9d, 41h
    jb      @sc_kw_cmp_raw
    cmp     r9d, 5Ah
    ja      @sc_kw_cmp_raw
    or      r9d, 20h
@sc_kw_cmp_raw:
    movzx   r10d, word ptr [r15 + r8*2]  ; keyword char (already lowercase)
    cmp     r9d, r10d
    jne     @sc_kw_nomatch
    inc     r8d
    jmp     @sc_kw_cmp

@sc_kw_nomatch:
    ; Restore edi to start+1 for next scan position
    sub     edi, r8d                  ; back to start
    jmp     @sc_kw_scan_next

@sc_kw_match:
    ; Keyword found! +25 bonus (restore edi properly)
    sub     edi, eax                  ; back to start position
    add     r13d, 25
    cmp     r13d, 50
    jge     @sc_kw_apply              ; capped
    jmp     @sc_kw_advance            ; only count each keyword once

@sc_kw_scan_next:
    inc     edi
    jmp     @sc_kw_scan

@sc_kw_advance:
    ; Move to next keyword entry: skip len byte + kwLen*2 WCHARs
    movzx   eax, byte ptr [r14]
    lea     r14, [r14 + 1]            ; skip length byte
    movzx   eax, byte ptr [r14 - 1]
    shl     eax, 1                    ; kwLen * 2 = bytes of WCHAR data
    add     r14, rax                  ; skip WCHAR data
    mov     rcx, [rbp-8]             ; reload candidate ptr (may have been clobbered)
    mov     edx, [rbp-0Ch]           ; reload candidate len
    jmp     @sc_kw_outer

@sc_kw_apply:
    add     r12d, r13d                ; add keyword bonus to total score

    ; ── Clamp to [0, 255] ──
@sc_clamp:
    test    r12d, r12d
    jns     @sc_clamp_hi
    xor     r12d, r12d
@sc_clamp_hi:
    cmp     r12d, 255
    jle     @sc_done
    mov     r12d, 255

@sc_done:
    mov     eax, r12d
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    ret
ScoreCandidate ENDP

PUBLIC ScoreCandidate

;=============================================================================
; PruneCandidates — Score all candidates, pick best, reject below threshold
; Input:  (uses globals g_candidates, g_candidateCount, etc.)
;         RCX = context ptr (g_textBuf near cursor)
;         EDX = context len
; Output: EAX = index of best candidate, or -1 if all pruned
;=============================================================================

PruneCandidates PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    mov     [rbp-8],  rcx          ; context ptr
    mov     [rbp-0Ch], edx         ; context len

    mov     r13d, [g_candidateCount]
    test    r13d, r13d
    jz      @pc_none

    xor     r14d, r14d             ; r14d = best score
    mov     r15d, -1               ; r15d = best index
    xor     ebx, ebx               ; ebx = loop index

@pc_loop:
    cmp     ebx, r13d
    jge     @pc_select

    ; Calculate candidate buffer address: g_candidates + ebx * MAX_CANDIDATE_LEN * 2
    mov     eax, ebx
    imul    eax, MAX_CANDIDATE_LEN * 2
    lea     rcx, [g_candidates]
    add     rcx, rax               ; RCX = candidate[ebx] ptr

    lea     rax, [g_candidateLens]
    mov     edx, dword ptr [rax + rbx*4]   ; EDX = candidate len

    ; Call ScoreCandidate(candidate_ptr, len, context_ptr, context_len)
    mov     r8, [rbp-8]            ; context ptr
    mov     r9d, [rbp-0Ch]         ; context len

    ; Save caller-save regs
    push    rbx
    push    r13
    push    r14
    push    r15
    sub     rsp, 20h
    call    ScoreCandidate
    add     rsp, 20h
    pop     r15
    pop     r14
    pop     r13
    pop     rbx

    ; Store score
    lea     rcx, [g_candidateScores]
    mov     dword ptr [rcx + rbx*4], eax

    ; Track best
    cmp     eax, r14d
    jle     @pc_next
    mov     r14d, eax
    mov     r15d, ebx

@pc_next:
    inc     ebx
    jmp     @pc_loop

@pc_select:
    ; Check if best score meets threshold
    cmp     r14d, SCORE_THRESHOLD
    jl      @pc_none

    mov     [g_bestCandidate], r15d
    mov     eax, r15d
    jmp     @pc_ret

@pc_none:
    mov     eax, -1

@pc_ret:
    add     rsp, 40h
    pop     rbp
    ret
PruneCandidates ENDP

PUBLIC PruneCandidates

CreateEditorPane PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    ; The sovereign editor uses hMainWnd as the editor surface directly.
    ; No child control is created — GDI rendering is done in WndProc/WM_PAINT.
    ; Initialize editor state: clear buffer, set cursor to 0,0, rebuild line table.
    xor     eax, eax
    mov     g_totalChars, eax
    mov     g_cursorLine, eax
    mov     g_cursorCol, eax
    mov     g_scrollY, eax
    mov     g_selStart, -1
    mov     g_selEnd, -1
    lea     rdi, g_textBuf
    mov     word ptr [rdi], 0            ; null-terminate empty buffer
    call    RebuildLineTable

    mov     rax, hMainWnd                ; return the editor HWND
    add     rsp, 28h
    ret
CreateEditorPane ENDP

; ── DeleteSelection: removes selected text, updates cursor ───────
; Returns: eax = number of chars deleted (0 if no selection)
; Side effects: updates g_totalChars, g_cursorLine, g_cursorCol,
;               clears g_selStart/g_selEnd, calls RebuildLineTable
DeleteSelection PROC
    mov     eax, g_selStart
    cmp     eax, -1
    je      @ds_none
    mov     edi, g_selEnd
    ; normalize: eax = min, edi = max
    cmp     eax, edi
    jle     @ds_ordered
    xchg    eax, edi
@ds_ordered:
    mov     ecx, edi
    sub     ecx, eax            ; ecx = chars to delete
    test    ecx, ecx
    jle     @ds_none
    push    rcx                 ; save delete count

    ; shift buffer left: move [edi..totalChars) to [eax..)
    lea     r10, g_textBuf
    mov     edx, g_totalChars
    sub     edx, edi            ; edx = trailing chars
    test    edx, edx
    jle     @ds_shifted
    lea     r11, [r10 + rdi*2]  ; src = &textBuf[end]
    lea     r10, [r10 + rax*2]  ; dst = &textBuf[start]
    xor     esi, esi
@ds_movloop:
    cmp     esi, edx
    jge     @ds_shifted
    mov     r9w, word ptr [r11 + rsi*2]
    mov     word ptr [r10 + rsi*2], r9w
    inc     esi
    jmp     @ds_movloop
@ds_shifted:
    pop     rcx
    sub     g_totalChars, ecx
    ; null-terminate
    mov     edx, g_totalChars
    lea     r10, g_textBuf
    mov     word ptr [r10 + rdx*2], 0

    ; clear selection
    mov     g_selStart, -1
    mov     g_selEnd, -1

    ; update cursor position to deletion start
    ; Find which line and column 'eax' (start offset) corresponds to
    push    rax                 ; save start offset
    call    RebuildLineTable
    pop     rax                 ; restore start offset

    ; Find cursor line: scan g_lineOff to find which line contains offset eax
    lea     rdx, g_lineOff
    xor     ecx, ecx            ; line index
@ds_findline:
    inc     ecx
    cmp     ecx, g_lineCount
    jge     @ds_lastline
    cmp     eax, dword ptr [rdx + rcx*4]
    jge     @ds_findline
    dec     ecx
    mov     g_cursorLine, ecx
    mov     ecx, dword ptr [rdx + rcx*4]
    sub     eax, ecx
    mov     g_cursorCol, eax
    jmp     @ds_ret
@ds_lastline:
    dec     ecx
    mov     g_cursorLine, ecx
    mov     ecx, dword ptr [rdx + rcx*4]
    sub     eax, ecx
    mov     g_cursorCol, eax
@ds_ret:
    mov     eax, 1              ; return nonzero = selection was deleted
    ret
@ds_none:
    xor     eax, eax            ; return 0 = no selection
    ret
DeleteSelection ENDP

;=============================================================================
; UndoPush — Record an edit in the undo ring
; Input: ECX = undoType (0=insert, 1=delete)
;        EDX = charOffset
;        R8D = charCount
;        R9  = ptr to deleted text (only for type=1, up to UNDO_BUF_SIZE chars)
; Saves current cursorLine/cursorCol automatically.
;=============================================================================
UndoPush PROC
    push    rdi
    push    rsi

    ; Get current slot index
    mov     eax, g_undoHead
    ; Compute entry address: g_undoRing + eax * 24
    imul    edi, eax, 24
    lea     rsi, g_undoRing
    add     rsi, rdi

    ; Fill entry
    mov     dword ptr [rsi],    ecx           ; undoType
    mov     dword ptr [rsi+4],  edx           ; charOffset
    mov     dword ptr [rsi+8],  r8d           ; charCount
    mov     edi, g_cursorLine
    mov     dword ptr [rsi+12], edi           ; cursorLine
    mov     edi, g_cursorCol
    mov     dword ptr [rsi+16], edi           ; cursorCol

    ; If type=1 (delete), copy deleted text to g_undoData[slot]
    cmp     ecx, 1
    jne     @up_advance
    test    r9, r9
    jz      @up_advance
    ; Clamp to UNDO_BUF_SIZE
    mov     edi, r8d
    cmp     edi, UNDO_BUF_SIZE
    jle     @up_copy
    mov     edi, UNDO_BUF_SIZE
@up_copy:
    ; dest = g_undoData[head * UNDO_BUF_SIZE * 2]
    mov     eax, g_undoHead
    imul    eax, UNDO_BUF_SIZE * 2
    lea     rsi, g_undoData
    add     rsi, rax
    xor     ecx, ecx
@up_cp_loop:
    cmp     ecx, edi
    jge     @up_advance
    mov     ax, word ptr [r9 + rcx*2]
    mov     word ptr [rsi + rcx*2], ax
    inc     ecx
    jmp     @up_cp_loop

@up_advance:
    ; Advance head (circular)
    mov     eax, g_undoHead
    inc     eax
    cmp     eax, UNDO_MAX_ENTRIES
    jl      @up_set
    xor     eax, eax
@up_set:
    mov     g_undoHead, eax
    ; Increment count (cap at UNDO_MAX_ENTRIES)
    mov     eax, g_undoCount
    cmp     eax, UNDO_MAX_ENTRIES
    jge     @up_done
    inc     g_undoCount
@up_done:
    pop     rsi
    pop     rdi
    ret
UndoPush ENDP

;-----------------------------------------------------------------------------
; WritePEFile — Generates complete PE32+ in memory using shared buffer
;-----------------------------------------------------------------------------
WritePEFile PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    ; Copy DOS header
    lea     rsi, [g_dosHeader]
    lea     rdi, [g_peBuffer]
    mov     rcx, 64
    rep     movsb
    
    ; Copy NT headers (signature + file header + optional header + data directory)
    lea     rsi, [g_ntHeaders]
    lea     rdi, [g_peBuffer + 64]
    mov     rcx, 264
    rep     movsb
    
    ; Copy section headers
    lea     rsi, [g_sectionHeaders]
    lea     rdi, [g_peBuffer + 64 + 264]
    mov     rcx, 120
    rep     movsb
    
    ; Copy .text at offset 0x200 (64-byte multi-import payload)
    lea     rsi, [g_entryCode]
    lea     rdi, [g_peBuffer + 200h]
    mov     rcx, 64
    rep     movsb
    
    ; Copy .data payload at offset 0x400 (stdout message)
    lea     rsi, [g_dataPayload]
    lea     rdi, [g_peBuffer + 400h]
    mov     rcx, 32
    rep     movsb
    
    ; Copy .idata at offset 0x600
    lea     rsi, [g_importTable]
    lea     rdi, [g_peBuffer + 600h]
    mov     rcx, 256
    rep     movsb
    
    mov     g_peSize, 00000A00h
    mov     rax, 1
    
    lea     rsp, [rbp]
    pop     rbp
    ret
WritePEFile ENDP

; SavePEToDisk moved to pe_writer.asm — no longer duplicated here
; (duplicate proc removed to resolve LNK2005)

END

; --- Ghost Text Token Callback ---
; RCX = token string (ptr), RDX = length
UI_OnTokenStream:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; 1. Append token to ghost buffer (Simplified)
    ; 2. Invalidate text area
    ; 3. Redraw ghost text overlay
    
    ; FOR DEMO: 
    ; Just signal that we are streaming
    
    add     rsp, 32
    pop     rbp
    ret