; ==============================================================================
; RawrXD_Sovereign_Monolith.asm
; Zero-Dependency Monolithic x64 Professional IDE
; No includes. No CRT. No static imports. No structural definitions in code.
; All API resolved via PEB walk at runtime.
; Build: ml64.exe /c RawrXD_Sovereign_Monolith.asm
;        link.exe /ENTRY:Start /SUBSYSTEM:WINDOWS /NODEFAULTLIB RawrXD_Sovereign_Monolith.obj
; ==============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE

; ==============================================================================
; .data - All string constants, API name tables, keyword tables
; ==============================================================================
.data
align 8

; --- DLL Name Strings (UTF-16LE for PEB walk) ---
szKernel32      DB 'K',0,'E',0,'R',0,'N',0,'E',0,'L',0,'3',0,'2',0,'.',0,'D',0,'L',0,'L',0,0,0
szUser32        DB 'U',0,'S',0,'E',0,'R',0,'3',0,'2',0,'.',0,'D',0,'L',0,'L',0,0,0
szGdi32         DB 'G',0,'D',0,'I',0,'3',0,'2',0,'.',0,'D',0,'L',0,'L',0,0,0

; --- Kernel32 API Names ---
aLoadLibraryA       DB 'LoadLibraryA',0
aGetProcAddress     DB 'GetProcAddress',0
aVirtualAlloc       DB 'VirtualAlloc',0
aVirtualFree        DB 'VirtualFree',0
aCreateFileA        DB 'CreateFileA',0
aReadFile           DB 'ReadFile',0
aWriteFile          DB 'WriteFile',0
aCloseHandle        DB 'CloseHandle',0
aGetFileSize        DB 'GetFileSize',0
aSetFilePointer     DB 'SetFilePointer',0
aSetEndOfFile       DB 'SetEndOfFile',0
aExitProcess        DB 'ExitProcess',0
aGetModuleHandleA   DB 'GetModuleHandleA',0
aGetCommandLineA    DB 'GetCommandLineA',0
aMultiByteToWideChar DB 'MultiByteToWideChar',0

; --- Kernel32 Console/Thread/Process/Sync/Error/Timing API Names ---
aGetStdHandle       DB 'GetStdHandle',0
aWriteConsoleA      DB 'WriteConsoleA',0
aReadConsoleA       DB 'ReadConsoleA',0
aAllocConsole       DB 'AllocConsole',0
aFreeConsole        DB 'FreeConsole',0
aCreateThread       DB 'CreateThread',0
aResumeThread       DB 'ResumeThread',0
aSuspendThread      DB 'SuspendThread',0
aTerminateThread    DB 'TerminateThread',0
aWaitForSingleObject DB 'WaitForSingleObject',0
aWaitForMultipleObjects DB 'WaitForMultipleObjects',0
aGetExitCodeThread  DB 'GetExitCodeThread',0
aCreateMutexA       DB 'CreateMutexA',0
aReleaseMutex       DB 'ReleaseMutex',0
aCreateSemaphoreA   DB 'CreateSemaphoreA',0
aReleaseSemaphore   DB 'ReleaseSemaphore',0
aCreateEventA       DB 'CreateEventA',0
aSetEvent            DB 'SetEvent',0
aResetEvent          DB 'ResetEvent',0
aInitializeCriticalSection DB 'InitializeCriticalSection',0
aEnterCriticalSection      DB 'EnterCriticalSection',0
aLeaveCriticalSection      DB 'LeaveCriticalSection',0
aDeleteCriticalSection     DB 'DeleteCriticalSection',0
aSleep               DB 'Sleep',0
aGetTickCount64      DB 'GetTickCount64',0
aQueryPerformanceCounter   DB 'QueryPerformanceCounter',0
aQueryPerformanceFrequency DB 'QueryPerformanceFrequency',0
aSetLastError        DB 'SetLastError',0
aGetLastError        DB 'GetLastError',0
aOpenProcess         DB 'OpenProcess',0
aTerminateProcess    DB 'TerminateProcess',0
aGetCurrentProcessId DB 'GetCurrentProcessId',0
aGetCurrentThreadId  DB 'GetCurrentThreadId',0
aReadProcessMemory   DB 'ReadProcessMemory',0
aWriteProcessMemory  DB 'WriteProcessMemory',0
aFlushFileBuffers    DB 'FlushFileBuffers',0
aGetFileSizeEx       DB 'GetFileSizeEx',0
aHeapCreate          DB 'HeapCreate',0
aHeapAlloc           DB 'HeapAlloc',0
aHeapFree            DB 'HeapFree',0
aHeapDestroy         DB 'HeapDestroy',0

; --- User32 API Names ---
aRegisterClassExA   DB 'RegisterClassExA',0
aCreateWindowExA    DB 'CreateWindowExA',0
aShowWindow         DB 'ShowWindow',0
aUpdateWindow       DB 'UpdateWindow',0
aGetMessageA        DB 'GetMessageA',0
aTranslateMessage   DB 'TranslateMessage',0
aDispatchMessageA   DB 'DispatchMessageA',0
aDefWindowProcA     DB 'DefWindowProcA',0
aPostQuitMessage    DB 'PostQuitMessage',0
aLoadCursorA        DB 'LoadCursorA',0
aLoadIconA          DB 'LoadIconA',0
aBeginPaint         DB 'BeginPaint',0
aEndPaint           DB 'EndPaint',0
aGetClientRect      DB 'GetClientRect',0
aInvalidateRect     DB 'InvalidateRect',0
aGetDC              DB 'GetDC',0
aReleaseDC          DB 'ReleaseDC',0
aSetScrollInfo      DB 'SetScrollInfo',0
aGetScrollInfo      DB 'GetScrollInfo',0
aSetTimer           DB 'SetTimer',0
aKillTimer          DB 'KillTimer',0
aMessageBoxA        DB 'MessageBoxA',0
aSetFocus           DB 'SetFocus',0
aGetKeyState        DB 'GetKeyState',0
aSetCapture         DB 'SetCapture',0
aReleaseCapture     DB 'ReleaseCapture',0
aCreateCaret        DB 'CreateCaret',0
aSetCaretPos        DB 'SetCaretPos',0
aShowCaret          DB 'ShowCaret',0
aHideCaret          DB 'HideCaret',0
aDestroyCaret       DB 'DestroyCaret',0
aFillRect           DB 'FillRect',0
aGetSysColor        DB 'GetSysColor',0
aCreateSolidBrush   DB 'CreateSolidBrush',0
aMessageBoxW        DB 'MessageBoxW',0
aGetWindowTextA     DB 'GetWindowTextA',0
aSetWindowTextA     DB 'SetWindowTextA',0
aDrawTextA          DB 'DrawTextA',0
aGetWindowLongPtrA  DB 'GetWindowLongPtrA',0
aSetWindowLongPtrA  DB 'SetWindowLongPtrA',0
aGetSystemMetrics   DB 'GetSystemMetrics',0
aSendMessageA       DB 'SendMessageA',0
aEnableWindow       DB 'EnableWindow',0
aIsWindowVisible    DB 'IsWindowVisible',0
aMoveWindow         DB 'MoveWindow',0
aDestroyWindow      DB 'DestroyWindow',0
aSetWindowPos       DB 'SetWindowPos',0
aGetForegroundWindow DB 'GetForegroundWindow',0
aSetForegroundWindow DB 'SetForegroundWindow',0

; --- GDI32 API Names ---
aCreateFontIndirectA    DB 'CreateFontIndirectA',0
aSelectObject           DB 'SelectObject',0
aDeleteObject           DB 'DeleteObject',0
aSetTextColor           DB 'SetTextColor',0
aSetBkColor             DB 'SetBkColor',0
aSetBkMode              DB 'SetBkMode',0
aTextOutA               DB 'TextOutA',0
aBitBlt                 DB 'BitBlt',0
aCreateCompatibleDC     DB 'CreateCompatibleDC',0
aCreateCompatibleBitmap DB 'CreateCompatibleBitmap',0
aDeleteDC               DB 'DeleteDC',0
aGetTextMetricsA        DB 'GetTextMetricsA',0
aGetTextExtentPoint32A  DB 'GetTextExtentPoint32A',0
aMoveToEx               DB 'MoveToEx',0
aLineTo                 DB 'LineTo',0
aCreatePen              DB 'CreatePen',0
aPatBlt                 DB 'PatBlt',0

; --- Window Class / Title ---
szClassName     DB 'RawrXD_Sovereign',0
szTitle         DB 'RawrXD Sovereign IDE',0
szFontName      DB 'Consolas',0
szUntitled      DB '[Untitled]',0
szModified      DB ' [Modified]',0

; --- Lexer Keyword Tables (C/ASM combined, sorted by hash) ---
; Format: length byte, token type byte, string bytes
kwTable:
    DB 2, 42, 'if',0,0,0,0,0,0       ; TK_KEYWORD=21h, TK_INSTRUCTION=42
    DB 3, 42, 'mov',0,0,0,0,0
    DB 3, 42, 'ret',0,0,0,0,0
    DB 3, 42, 'jmp',0,0,0,0,0
    DB 3, 42, 'jnz',0,0,0,0,0
    DB 3, 42, 'jne',0,0,0,0,0
    DB 3, 42, 'jnb',0,0,0,0,0
    DB 3, 42, 'cmp',0,0,0,0,0
    DB 3, 42, 'add',0,0,0,0,0
    DB 3, 42, 'sub',0,0,0,0,0
    DB 3, 42, 'xor',0,0,0,0,0
    DB 3, 42, 'and',0,0,0,0,0
    DB 3, 42, 'lea',0,0,0,0,0
    DB 3, 42, 'shl',0,0,0,0,0
    DB 3, 42, 'shr',0,0,0,0,0
    DB 3, 42, 'inc',0,0,0,0,0
    DB 3, 42, 'dec',0,0,0,0,0
    DB 3, 42, 'rep',0,0,0,0,0
    DB 3, 42, 'nop',0,0,0,0,0
    DB 3, 22, 'int',0,0,0,0,0       ; TK_TYPE=22
    DB 4, 21, 'void',0,0,0,0        ; TK_KEYWORD=21
    DB 4, 21, 'else',0,0,0,0
    DB 4, 21, 'case',0,0,0,0
    DB 4, 42, 'call',0,0,0,0
    DB 4, 42, 'push',0,0,0,0
    DB 4, 42, 'test',0,0,0,0
    DB 4, 42, 'imul',0,0,0,0
    DB 4, 41, 'PROC',0,0,0,0        ; TK_DIRECTIVE=41
    DB 4, 41, 'ENDP',0,0,0,0
    DB 5, 21, 'break',0,0,0
    DB 5, 21, 'while',0,0,0
    DB 5, 23, 'const',0,0,0         ; TK_MODIFIER=23
    DB 6, 21, 'return',0,0
    DB 6, 23, 'static',0,0
    DB 6, 22, 'struct',0,0
    DB 8, 21, 'continue',0
    DB 0, 0, 0,0,0,0,0,0,0          ; Sentinel

; --- Register table ---
regTable:
    DB 3, 40, 'rax',0,0,0,0,0       ; TK_REGISTER=40
    DB 3, 40, 'rbx',0,0,0,0,0
    DB 3, 40, 'rcx',0,0,0,0,0
    DB 3, 40, 'rdx',0,0,0,0,0
    DB 3, 40, 'rsi',0,0,0,0,0
    DB 3, 40, 'rdi',0,0,0,0,0
    DB 3, 40, 'rsp',0,0,0,0,0
    DB 3, 40, 'rbp',0,0,0,0,0
    DB 3, 40, 'r8d',0,0,0,0,0
    DB 3, 40, 'r9d',0,0,0,0,0
    DB 3, 40, 'eax',0,0,0,0,0
    DB 3, 40, 'ebx',0,0,0,0,0
    DB 3, 40, 'ecx',0,0,0,0,0
    DB 3, 40, 'edx',0,0,0,0,0
    DB 2, 40, 'r8',0,0,0,0,0,0
    DB 2, 40, 'r9',0,0,0,0,0,0
    DB 0, 0, 0,0,0,0,0,0,0

; --- Syntax coloring palette (COLORREF) ---
colBackground   DD 001E1E1Eh       ; Dark background
colText         DD 000D4D4D4h       ; Default text
colKeyword      DD 000569CD6h       ; Blue
colType         DD 0004EC9B0h       ; Green
colComment      DD 0006A9955h       ; Green-gray
colString       DD 000CE9178h       ; Orange
colNumber       DD 000B5CEA8h       ; Light green
colOperator     DD 000D4D4D4h       ; White
colRegister     DD 0009CDCFEh       ; Light blue
colDirective    DD 000C586C0h       ; Purple
colMacro        DD 000DCDCAAh       ; Yellow
colLineNum      DD 000858585h       ; Gray
colCursorLine   DD 000282828h       ; Slightly lighter than bg
colSelection    DD 000264F78h       ; Blue selection
colCaret        DD 000FFFFFFh       ; White caret

; --- DOS Header Template (64 bytes - PE foundation) ---
align 8
DosHeaderTemplate:
    DW  05A4Dh          ; e_magic: 'MZ'
    DW  00090h          ; e_cblp
    DW  00003h          ; e_cp
    DW  00000h          ; e_crlc
    DW  00004h          ; e_cparhdr
    DW  00000h          ; e_minalloc
    DW  0FFFFh          ; e_maxalloc
    DW  00000h          ; e_ss
    DW  000B8h          ; e_sp
    DW  00000h          ; e_csum
    DW  00000h          ; e_ip
    DW  00000h          ; e_cs
    DW  00040h          ; e_lfarlc
    DW  00000h          ; e_ovno
    DW  0, 0, 0, 0      ; e_res[4]
    DW  00000h          ; e_oemid
    DW  00000h          ; e_oeminfo
    DW  0, 0, 0, 0, 0, 0, 0, 0, 0, 0  ; e_res2[10]
    DD  00000080h       ; e_lfanew -> PE header at 128

; --- DOS Stub (14 bytes code + message + terminator) ---
DosStubCode:
    DB  0Eh, 1Fh, 0BAh, 0Eh, 00h, 0B4h, 09h, 0CDh
    DB  21h, 0B8h, 01h, 4Ch, 0CDh, 21h
    DB  'This program requires x64 Windows.', 0Dh, 0Ah, 24h

; ==============================================================================
; .data? - All runtime state, function pointers, editor buffers
; ==============================================================================
.data?
align 16

; --- DLL Handles ---
hKernel32               DQ ?
hUser32                 DQ ?
hGdi32                  DQ ?

; --- Kernel32 Function Pointers ---
fnLoadLibraryA          DQ ?
fnGetProcAddress        DQ ?
fnVirtualAlloc          DQ ?
fnVirtualFree           DQ ?
fnCreateFileA           DQ ?
fnReadFile              DQ ?
fnWriteFile             DQ ?
fnCloseHandle           DQ ?
fnGetFileSize           DQ ?
fnSetFilePointer        DQ ?
fnSetEndOfFile          DQ ?
fnExitProcess           DQ ?
fnGetModuleHandleA      DQ ?
fnGetCommandLineA       DQ ?

; --- Kernel32 Console Function Pointers ---
fnGetStdHandle          DQ ?
fnWriteConsoleA         DQ ?
fnReadConsoleA          DQ ?
fnAllocConsole          DQ ?
fnFreeConsole           DQ ?

; --- Kernel32 Thread Function Pointers ---
fnCreateThread          DQ ?
fnResumeThread          DQ ?
fnSuspendThread         DQ ?
fnTerminateThread       DQ ?
fnWaitForSingleObject   DQ ?
fnWaitForMultipleObjects DQ ?
fnGetExitCodeThread     DQ ?

; --- Kernel32 Sync Function Pointers ---
fnCreateMutexA          DQ ?
fnReleaseMutex          DQ ?
fnCreateSemaphoreA      DQ ?
fnReleaseSemaphore      DQ ?
fnCreateEventA          DQ ?
fnSetEvent              DQ ?
fnResetEvent            DQ ?
fnInitializeCriticalSection DQ ?
fnEnterCriticalSection  DQ ?
fnLeaveCriticalSection  DQ ?
fnDeleteCriticalSection DQ ?

; --- Kernel32 Timing Function Pointers ---
fnSleep                 DQ ?
fnGetTickCount64        DQ ?
fnQueryPerformanceCounter   DQ ?
fnQueryPerformanceFrequency DQ ?

; --- Kernel32 Error Function Pointers ---
fnSetLastError          DQ ?
fnGetLastError          DQ ?

; --- Kernel32 Process Function Pointers ---
fnOpenProcess           DQ ?
fnTerminateProcess      DQ ?
fnGetCurrentProcessId   DQ ?
fnGetCurrentThreadId    DQ ?
fnReadProcessMemory     DQ ?
fnWriteProcessMemory    DQ ?
fnFlushFileBuffers      DQ ?
fnGetFileSizeEx         DQ ?

; --- Kernel32 Heap Function Pointers ---
fnHeapCreate            DQ ?
fnHeapAlloc             DQ ?
fnHeapFree              DQ ?
fnHeapDestroy           DQ ?

; --- User32 Function Pointers ---
fnRegisterClassExA      DQ ?
fnCreateWindowExA       DQ ?
fnShowWindow            DQ ?
fnUpdateWindow          DQ ?
fnGetMessageA           DQ ?
fnTranslateMessage      DQ ?
fnDispatchMessageA      DQ ?
fnDefWindowProcA        DQ ?
fnPostQuitMessage       DQ ?
fnLoadCursorA           DQ ?
fnLoadIconA             DQ ?
fnBeginPaint            DQ ?
fnEndPaint              DQ ?
fnGetClientRect         DQ ?
fnInvalidateRect        DQ ?
fnGetDC                 DQ ?
fnReleaseDC             DQ ?
fnSetScrollInfo         DQ ?
fnGetScrollInfo         DQ ?
fnSetTimer              DQ ?
fnKillTimer             DQ ?
fnMessageBoxA           DQ ?
fnSetFocus              DQ ?
fnGetKeyState           DQ ?
fnSetCapture            DQ ?
fnReleaseCapture        DQ ?
fnCreateCaret           DQ ?
fnSetCaretPos           DQ ?
fnShowCaret             DQ ?
fnHideCaret             DQ ?
fnDestroyCaret          DQ ?
fnFillRect              DQ ?
fnGetSysColor           DQ ?
fnCreateSolidBrush      DQ ?

; --- User32 Extended Function Pointers ---
fnMessageBoxW           DQ ?
fnGetWindowTextA        DQ ?
fnSetWindowTextA        DQ ?
fnDrawTextA             DQ ?
fnGetWindowLongPtrA     DQ ?
fnSetWindowLongPtrA     DQ ?
fnGetSystemMetrics      DQ ?
fnSendMessageA          DQ ?
fnEnableWindow          DQ ?
fnIsWindowVisible       DQ ?
fnMoveWindow            DQ ?
fnDestroyWindow         DQ ?
fnSetWindowPos          DQ ?
fnGetForegroundWindow   DQ ?
fnSetForegroundWindow   DQ ?

; --- GDI32 Function Pointers ---
fnCreateFontIndirectA   DQ ?
fnSelectObject          DQ ?
fnDeleteObject          DQ ?
fnSetTextColor          DQ ?
fnSetBkColor            DQ ?
fnSetBkMode             DQ ?
fnTextOutA              DQ ?
fnBitBlt                DQ ?
fnCreateCompatibleDC    DQ ?
fnCreateCompatibleBitmap DQ ?
fnDeleteDC              DQ ?
fnGetTextMetricsA       DQ ?
fnGetTextExtentPoint32A DQ ?
fnMoveToEx              DQ ?
fnLineTo                DQ ?
fnCreatePen             DQ ?
fnPatBlt                DQ ?

; --- Window State ---
hInstance               DQ ?
hWndMain                DQ ?
hFont                   DQ ?
hFontBold               DQ ?
hMemDC                  DQ ?
hMemBitmap              DQ ?
hOldBitmap              DQ ?
hBgBrush                DQ ?
hCurLineBrush           DQ ?
hSelBrush               DQ ?
hCaretPen               DQ ?

clientWidth             DD ?
clientHeight            DD ?
charWidth               DD ?
charHeight              DD ?
hasFocus                DD ?

; --- Gap Buffer (Editor Core) ---
; Offsets: +0=BasePtr, +8=Capacity, +16=GapStart, +24=GapEnd,
;          +32=TextLength, +40=LineCount, +48=Modified
pBuffer                 DQ ?
bufferCapacity          DQ ?
gapStart                DQ ?
gapEnd                  DQ ?
textLength              DQ ?
lineCount               DQ ?
modified                DD ?
                        DD ?        ; pad

; --- Line Offset Cache ---
lineOffsets             DQ ?        ; ptr to QWORD array
lineOffsetCap           DQ ?
linesDirty              DD ?
                        DD ?        ; pad

; --- Viewport State ---
; +0=topLine, +8=topCol, +16=curLine, +24=curCol, +32=curOffset
; +40=selAnchor, +48=selEnd, +56=viewH, +60=viewW
topLine                 DQ ?
topCol                  DQ ?
curLine                 DQ ?
curCol                  DQ ?
curOffset               DQ ?
selAnchor               DQ ?
selEnd                  DQ ?
viewLines               DD ?
viewCols                DD ?

; --- Undo Ring ---
; Each entry: 32 bytes header + up to 1024 bytes inline data
undoArena               DQ ?
undoWritePos            DQ ?
undoReadPos             DQ ?
undoEntryCount          DQ ?
undoSequence            DD ?
undoInProgress          DD ?

; --- Search State ---
searchNeedle            DQ ?
searchNeedleLen         DQ ?
searchLastMatch         DQ ?
searchMatchCount        DQ ?
searchBadChar           DB 256 DUP (?)
searchCaseMode          DD ?
                        DD ?        ; pad

; --- Clipboard Ring ---
clipArena               DQ ?
clipHead                DD ?
clipTail                DD ?
clipCurrent             DD ?
clipCount               DD ?
clipSequence            DQ ?

; --- Lexer Token Arena ---
tokenArena              DQ ?
tokenCount              DQ ?
tokenCapacity           DQ ?

; --- Scratch line buffer for rendering ---
lineBuf                 DB 4096 DUP (?)

; --- LOGFONT area (60 bytes) ---
logfontBuf              DB 64 DUP (?)

; --- PAINTSTRUCT area (72 bytes) ---
psBuf                   DB 80 DUP (?)

; --- SCROLLINFO area (28 bytes) ---
siBuf                   DB 32 DUP (?)

; --- MSG area (48 bytes) ---
msgBuf                  DB 56 DUP (?)

; --- WNDCLASSEX area (80 bytes) ---
wcBuf                   DB 88 DUP (?)

; --- RECT area (16 bytes) ---
rcBuf                   DB 16 DUP (?)

; --- TEXTMETRIC area (60 bytes) ---
tmBuf                   DB 64 DUP (?)

; --- File path buffer ---
filePathBuf             DB 520 DUP (?)

; --- Status bar text ---
statusBuf               DB 256 DUP (?)
numBuf                  DB 32 DUP (?)

; --- Console Handles ---
hStdIn                  DQ ?
hStdOut                 DQ ?
hStdErr                 DQ ?
consoleAttached         DD ?
                        DD ?        ; pad

; --- Thread Pool (16 thread handles max) ---
threadHandles           DQ 16 DUP (?)
threadCount             DD ?
                        DD ?        ; pad

; --- Linked List Arena (generic node pool: 64KB) ---
; Node layout: +0=pNext(8), +8=pPrev(8), +16=dataSize(8), +24..=data
llArena                 DQ ?
llFreeHead              DQ ?
llNodeSize              DQ ?
llNodeCount             DQ ?

; --- Stack Arena (generic LIFO: 4096 entries x 8 bytes) ---
stackArena              DQ ?
stackTop                DQ ?
stackCapacity           DQ ?

; --- Queue Arena (generic FIFO ring: 4096 entries x 8 bytes) ---
queueArena              DQ ?
queueHead               DQ ?
queueTail               DQ ?
queueCapacity           DQ ?
queueCount              DQ ?

; --- Critical Section scratch area (40 bytes) ---
critSecBuf              DB 48 DUP (?)

; --- Performance counter scratch ---
perfCounterBuf          DQ ?
perfFrequencyBuf        DQ ?

; --- Heap handle ---
hPrivateHeap            DQ ?

; --- Console I/O scratch ---
consoleBuf              DB 1024 DUP (?)
consoleWritten          DD ?
consoleRead             DD ?

; ==============================================================================
; .code
; ==============================================================================
.code

; ==============================================================================
; ENTRY POINT
; ==============================================================================
align 16
Start PROC
    sub     rsp, 40
    call    InitImports
    test    eax, eax
    jnz     @@fatal
    call    EditorInit
    call    WinMain
@@fatal:
    mov     rcx, rax
    call    qword ptr [fnExitProcess]
    int     3
Start ENDP

; ==============================================================================
; PEB WALK - Get loaded module base by UTF-16LE name comparison
; RCX = pointer to UTF-16LE DLL name
; Returns RAX = module base, 0 if not found
; ==============================================================================
align 16
GetModuleBase PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    mov     r12, rcx
    mov     rax, gs:[60h]
    mov     rax, [rax+18h]
    lea     rsi, [rax+20h]
    mov     rbx, [rsi]
@@walk:
    cmp     rbx, rsi
    je      @@notfound
    lea     rdi, [rbx-10h]
    movzx   r13d, word ptr [rdi+48h]
    shr     r13d, 1
    mov     r8, [rdi+50h]
    mov     r9, r12
    mov     ecx, r13d
@@cmpch:
    test    ecx, ecx
    jz      @@found
    dec     ecx
    movzx   eax, word ptr [r8+rcx*2]
    movzx   edx, word ptr [r9+rcx*2]
    or      eax, 20h
    or      edx, 20h
    cmp     eax, edx
    jne     @@next
    jmp     @@cmpch
@@found:
    mov     rax, [rdi+20h]
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@@next:
    mov     rbx, [rbx]
    jmp     @@walk
@@notfound:
    xor     rax, rax
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GetModuleBase ENDP

; ==============================================================================
; EXPORT TABLE WALK - Find function by ASCII name in PE export directory
; RCX = module base, RDX = ASCII function name
; Returns RAX = function address, 0 if not found
; ==============================================================================
align 16
GetExportProc PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rbx, rcx
    mov     r15, rdx
    mov     eax, [rbx+3Ch]
    lea     rax, [rbx+rax+88h]
    mov     eax, [rax]
    test    eax, eax
    jz      @@noexp
    add     rax, rbx
    mov     r12, rax
    mov     eax, [r12+20h]
    add     rax, rbx
    mov     r13, rax
    mov     eax, [r12+24h]
    add     rax, rbx
    mov     rsi, rax
    mov     eax, [r12+1Ch]
    add     rax, rbx
    mov     rdi, rax
    mov     ecx, [r12+18h]
    xor     r14d, r14d
@@exploop:
    cmp     r14d, ecx
    jae     @@noexp
    mov     eax, [r13+r14*4]
    add     rax, rbx
    mov     r8, r15
    mov     r9, rax
@@expcmp:
    mov     r10b, [r9]
    mov     r11b, [r8]
    cmp     r10b, r11b
    jne     @@expnext
    test    r10b, r10b
    jz      @@expfound
    inc     r9
    inc     r8
    jmp     @@expcmp
@@expnext:
    inc     r14d
    jmp     @@exploop
@@expfound:
    movzx   eax, word ptr [rsi+r14*2]
    mov     eax, [rdi+rax*4]
    add     rax, rbx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@@noexp:
    xor     rax, rax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GetExportProc ENDP

; ==============================================================================
; ResolveOne - Resolve single API: RCX=hDll, RDX=name, R8=&storage
; ==============================================================================
align 16
ResolveOne PROC
    push    r12
    mov     r12, r8
    sub     rsp, 32
    call    qword ptr [fnGetProcAddress]
    add     rsp, 32
    mov     [r12], rax
    pop     r12
    ret
ResolveOne ENDP

; ==============================================================================
; IMPORT RESOLUTION - PEB walk + GetProcAddress chain
; ==============================================================================
align 16
InitImports PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48

    lea     rcx, szKernel32
    call    GetModuleBase
    test    rax, rax
    jz      @@fail
    mov     hKernel32, rax

    mov     rcx, rax
    lea     rdx, aGetProcAddress
    call    GetExportProc
    test    rax, rax
    jz      @@fail
    mov     fnGetProcAddress, rax

    mov     rcx, hKernel32
    lea     rdx, aLoadLibraryA
    call    GetExportProc
    mov     fnLoadLibraryA, rax

    mov     rbx, hKernel32
    lea     rsi, aVirtualAlloc
    lea     rdi, fnVirtualAlloc
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8, rdi
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aVirtualFree
    lea     r8, fnVirtualFree
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aCreateFileA
    lea     r8, fnCreateFileA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aReadFile
    lea     r8, fnReadFile
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aWriteFile
    lea     r8, fnWriteFile
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aCloseHandle
    lea     r8, fnCloseHandle
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetFileSize
    lea     r8, fnGetFileSize
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aSetFilePointer
    lea     r8, fnSetFilePointer
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aSetEndOfFile
    lea     r8, fnSetEndOfFile
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aExitProcess
    lea     r8, fnExitProcess
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetModuleHandleA
    lea     r8, fnGetModuleHandleA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetCommandLineA
    lea     r8, fnGetCommandLineA
    call    ResolveOne

    ; --- Kernel32: Console APIs ---
    mov     rcx, rbx
    lea     rdx, aGetStdHandle
    lea     r8, fnGetStdHandle
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aWriteConsoleA
    lea     r8, fnWriteConsoleA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aReadConsoleA
    lea     r8, fnReadConsoleA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aAllocConsole
    lea     r8, fnAllocConsole
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aFreeConsole
    lea     r8, fnFreeConsole
    call    ResolveOne

    ; --- Kernel32: Thread APIs ---
    mov     rcx, rbx
    lea     rdx, aCreateThread
    lea     r8, fnCreateThread
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aResumeThread
    lea     r8, fnResumeThread
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aSuspendThread
    lea     r8, fnSuspendThread
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aTerminateThread
    lea     r8, fnTerminateThread
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aWaitForSingleObject
    lea     r8, fnWaitForSingleObject
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aWaitForMultipleObjects
    lea     r8, fnWaitForMultipleObjects
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetExitCodeThread
    lea     r8, fnGetExitCodeThread
    call    ResolveOne

    ; --- Kernel32: Synchronization APIs ---
    mov     rcx, rbx
    lea     rdx, aCreateMutexA
    lea     r8, fnCreateMutexA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aReleaseMutex
    lea     r8, fnReleaseMutex
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aCreateSemaphoreA
    lea     r8, fnCreateSemaphoreA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aReleaseSemaphore
    lea     r8, fnReleaseSemaphore
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aCreateEventA
    lea     r8, fnCreateEventA
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aSetEvent
    lea     r8, fnSetEvent
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aResetEvent
    lea     r8, fnResetEvent
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aInitializeCriticalSection
    lea     r8, fnInitializeCriticalSection
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aEnterCriticalSection
    lea     r8, fnEnterCriticalSection
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aLeaveCriticalSection
    lea     r8, fnLeaveCriticalSection
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aDeleteCriticalSection
    lea     r8, fnDeleteCriticalSection
    call    ResolveOne

    ; --- Kernel32: Timing APIs ---
    mov     rcx, rbx
    lea     rdx, aSleep
    lea     r8, fnSleep
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetTickCount64
    lea     r8, fnGetTickCount64
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aQueryPerformanceCounter
    lea     r8, fnQueryPerformanceCounter
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aQueryPerformanceFrequency
    lea     r8, fnQueryPerformanceFrequency
    call    ResolveOne

    ; --- Kernel32: Error APIs ---
    mov     rcx, rbx
    lea     rdx, aSetLastError
    lea     r8, fnSetLastError
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetLastError
    lea     r8, fnGetLastError
    call    ResolveOne

    ; --- Kernel32: Process APIs ---
    mov     rcx, rbx
    lea     rdx, aOpenProcess
    lea     r8, fnOpenProcess
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aTerminateProcess
    lea     r8, fnTerminateProcess
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetCurrentProcessId
    lea     r8, fnGetCurrentProcessId
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetCurrentThreadId
    lea     r8, fnGetCurrentThreadId
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aReadProcessMemory
    lea     r8, fnReadProcessMemory
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aWriteProcessMemory
    lea     r8, fnWriteProcessMemory
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aFlushFileBuffers
    lea     r8, fnFlushFileBuffers
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aGetFileSizeEx
    lea     r8, fnGetFileSizeEx
    call    ResolveOne

    ; --- Kernel32: Heap APIs ---
    mov     rcx, rbx
    lea     rdx, aHeapCreate
    lea     r8, fnHeapCreate
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aHeapAlloc
    lea     r8, fnHeapAlloc
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aHeapFree
    lea     r8, fnHeapFree
    call    ResolveOne

    mov     rcx, rbx
    lea     rdx, aHeapDestroy
    lea     r8, fnHeapDestroy
    call    ResolveOne

    ; --- Load user32.dll ---
    lea     rcx, [szUser32+1]
    and     cl, 0
    lea     rcx, aRegisterClassExA
    sub     rcx, (aRegisterClassExA - szUser32)
    lea     rcx, szUser32
    ; Convert UTF-16 to ANSI for LoadLibraryA
    sub     rsp, 8
    lea     rcx, statusBuf
    mov     byte ptr [rcx+0], 'u'
    mov     byte ptr [rcx+1], 's'
    mov     byte ptr [rcx+2], 'e'
    mov     byte ptr [rcx+3], 'r'
    mov     byte ptr [rcx+4], '3'
    mov     byte ptr [rcx+5], '2'
    mov     byte ptr [rcx+6], '.'
    mov     byte ptr [rcx+7], 'd'
    mov     byte ptr [rcx+8], 'l'
    mov     byte ptr [rcx+9], 'l'
    mov     byte ptr [rcx+10], 0
    call    qword ptr [fnLoadLibraryA]
    add     rsp, 8
    test    rax, rax
    jz      @@fail
    mov     hUser32, rax
    mov     rsi, rax

    mov     rcx, rsi
    lea     rdx, aRegisterClassExA
    lea     r8, fnRegisterClassExA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aCreateWindowExA
    lea     r8, fnCreateWindowExA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aShowWindow
    lea     r8, fnShowWindow
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aUpdateWindow
    lea     r8, fnUpdateWindow
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetMessageA
    lea     r8, fnGetMessageA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aTranslateMessage
    lea     r8, fnTranslateMessage
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aDispatchMessageA
    lea     r8, fnDispatchMessageA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aDefWindowProcA
    lea     r8, fnDefWindowProcA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aPostQuitMessage
    lea     r8, fnPostQuitMessage
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aLoadCursorA
    lea     r8, fnLoadCursorA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aLoadIconA
    lea     r8, fnLoadIconA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aBeginPaint
    lea     r8, fnBeginPaint
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aEndPaint
    lea     r8, fnEndPaint
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetClientRect
    lea     r8, fnGetClientRect
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aInvalidateRect
    lea     r8, fnInvalidateRect
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetDC
    lea     r8, fnGetDC
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aReleaseDC
    lea     r8, fnReleaseDC
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetScrollInfo
    lea     r8, fnSetScrollInfo
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetScrollInfo
    lea     r8, fnGetScrollInfo
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetTimer
    lea     r8, fnSetTimer
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aKillTimer
    lea     r8, fnKillTimer
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aMessageBoxA
    lea     r8, fnMessageBoxA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetFocus
    lea     r8, fnSetFocus
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetKeyState
    lea     r8, fnGetKeyState
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aCreateCaret
    lea     r8, fnCreateCaret
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetCaretPos
    lea     r8, fnSetCaretPos
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aShowCaret
    lea     r8, fnShowCaret
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aHideCaret
    lea     r8, fnHideCaret
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aDestroyCaret
    lea     r8, fnDestroyCaret
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aFillRect
    lea     r8, fnFillRect
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aCreateSolidBrush
    lea     r8, fnCreateSolidBrush
    call    ResolveOne

    ; --- User32: Extended APIs ---
    mov     rcx, rsi
    lea     rdx, aMessageBoxW
    lea     r8, fnMessageBoxW
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetWindowTextA
    lea     r8, fnGetWindowTextA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetWindowTextA
    lea     r8, fnSetWindowTextA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aDrawTextA
    lea     r8, fnDrawTextA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetWindowLongPtrA
    lea     r8, fnGetWindowLongPtrA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetWindowLongPtrA
    lea     r8, fnSetWindowLongPtrA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetSystemMetrics
    lea     r8, fnGetSystemMetrics
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSendMessageA
    lea     r8, fnSendMessageA
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aEnableWindow
    lea     r8, fnEnableWindow
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aIsWindowVisible
    lea     r8, fnIsWindowVisible
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aMoveWindow
    lea     r8, fnMoveWindow
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aDestroyWindow
    lea     r8, fnDestroyWindow
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetWindowPos
    lea     r8, fnSetWindowPos
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aGetForegroundWindow
    lea     r8, fnGetForegroundWindow
    call    ResolveOne

    mov     rcx, rsi
    lea     rdx, aSetForegroundWindow
    lea     r8, fnSetForegroundWindow
    call    ResolveOne

    ; --- Load gdi32.dll ---
    lea     rcx, statusBuf
    mov     byte ptr [rcx+0], 'g'
    mov     byte ptr [rcx+1], 'd'
    mov     byte ptr [rcx+2], 'i'
    mov     byte ptr [rcx+3], '3'
    mov     byte ptr [rcx+4], '2'
    mov     byte ptr [rcx+5], '.'
    mov     byte ptr [rcx+6], 'd'
    mov     byte ptr [rcx+7], 'l'
    mov     byte ptr [rcx+8], 'l'
    mov     byte ptr [rcx+9], 0
    sub     rsp, 8
    call    qword ptr [fnLoadLibraryA]
    add     rsp, 8
    test    rax, rax
    jz      @@fail
    mov     hGdi32, rax
    mov     rdi, rax

    mov     rcx, rdi
    lea     rdx, aCreateFontIndirectA
    lea     r8, fnCreateFontIndirectA
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aSelectObject
    lea     r8, fnSelectObject
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aDeleteObject
    lea     r8, fnDeleteObject
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aSetTextColor
    lea     r8, fnSetTextColor
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aSetBkColor
    lea     r8, fnSetBkColor
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aSetBkMode
    lea     r8, fnSetBkMode
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aTextOutA
    lea     r8, fnTextOutA
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aBitBlt
    lea     r8, fnBitBlt
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aCreateCompatibleDC
    lea     r8, fnCreateCompatibleDC
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aCreateCompatibleBitmap
    lea     r8, fnCreateCompatibleBitmap
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aDeleteDC
    lea     r8, fnDeleteDC
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aGetTextMetricsA
    lea     r8, fnGetTextMetricsA
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aMoveToEx
    lea     r8, fnMoveToEx
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aLineTo
    lea     r8, fnLineTo
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aCreatePen
    lea     r8, fnCreatePen
    call    ResolveOne

    mov     rcx, rdi
    lea     rdx, aPatBlt
    lea     r8, fnPatBlt
    call    ResolveOne

    xor     eax, eax
    jmp     @@done
@@fail:
    mov     eax, 1
@@done:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
InitImports ENDP

; ==============================================================================
; MEMORY HELPERS
; ==============================================================================
align 16
MemAlloc PROC
    sub     rsp, 40
    xor     ecx, ecx
    mov     rdx, r8
    mov     r8d, 3000h
    mov     r9d, 4
    call    qword ptr [fnVirtualAlloc]
    add     rsp, 40
    ret
MemAlloc ENDP

align 16
MemFree PROC
    sub     rsp, 40
    xor     edx, edx
    mov     r8d, 8000h
    call    qword ptr [fnVirtualFree]
    add     rsp, 40
    ret
MemFree ENDP

align 16
MemZero PROC
    push    rdi
    mov     rdi, rcx
    mov     rcx, rdx
    xor     eax, eax
    rep     stosb
    pop     rdi
    ret
MemZero ENDP

align 16
MemCopy PROC
    push    rsi
    push    rdi
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rcx, r8
    rep     movsb
    pop     rdi
    pop     rsi
    ret
MemCopy ENDP

align 16
StrLenA PROC
    xor     rax, rax
    mov     rdx, rcx
@@sl:
    cmp     byte ptr [rdx+rax], 0
    je      @@sld
    inc     rax
    jmp     @@sl
@@sld:
    ret
StrLenA ENDP

align 16
IntToStr PROC
    push    rbx
    push    rdi
    mov     rdi, rcx
    mov     rax, rdx
    mov     rbx, 10
    xor     ecx, ecx
    test    rax, rax
    jnz     @@i2s_loop
    mov     byte ptr [rdi], '0'
    mov     byte ptr [rdi+1], 0
    mov     rax, 1
    pop     rdi
    pop     rbx
    ret
@@i2s_loop:
    xor     rdx, rdx
    div     rbx
    add     dl, '0'
    push    rdx
    inc     ecx
    test    rax, rax
    jnz     @@i2s_loop
    xor     eax, eax
@@i2s_pop:
    pop     rdx
    mov     [rdi+rax], dl
    inc     rax
    dec     ecx
    jnz     @@i2s_pop
    mov     byte ptr [rdi+rax], 0
    pop     rdi
    pop     rbx
    ret
IntToStr ENDP

; ==============================================================================
; GAP BUFFER - Core text engine
; ==============================================================================
align 16
GapBuffer_Init PROC
    push    rbx
    sub     rsp, 48
    mov     rbx, rcx
    mov     r8, rbx
    call    MemAlloc
    test    rax, rax
    jz      @@gbfail
    mov     pBuffer, rax
    mov     bufferCapacity, rbx
    mov     gapStart, 0
    mov     gapEnd, rbx
    mov     textLength, 0
    mov     lineCount, 1
    mov     modified, 0
    mov     selAnchor, -1
    mov     selEnd, -1
    mov     r8, rbx
    mov     rcx, rax
    mov     rdx, rbx
    call    MemZero
    xor     eax, eax
    jmp     @@gbdone
@@gbfail:
    mov     eax, 1
@@gbdone:
    add     rsp, 48
    pop     rbx
    ret
GapBuffer_Init ENDP

align 16
GapBuffer_GapSize PROC
    mov     rax, gapEnd
    sub     rax, gapStart
    ret
GapBuffer_GapSize ENDP

align 16
GapBuffer_ContentLen PROC
    mov     rax, bufferCapacity
    mov     rcx, gapEnd
    sub     rcx, gapStart
    sub     rax, rcx
    ret
GapBuffer_ContentLen ENDP

align 16
GapBuffer_MoveGap PROC
    push    rsi
    push    rdi
    push    rbx
    mov     rbx, rcx
    cmp     rbx, gapStart
    je      @@mgdone
    ja      @@mgright
    mov     rcx, gapStart
    sub     rcx, rbx
    mov     rsi, pBuffer
    add     rsi, rbx
    mov     rdi, pBuffer
    add     rdi, gapEnd
    sub     rdi, rcx
    push    rcx
    rep     movsb
    pop     rcx
    sub     gapStart, rcx
    sub     gapEnd, rcx
    jmp     @@mgdone
@@mgright:
    mov     rcx, rbx
    sub     rcx, gapStart
    mov     rsi, pBuffer
    add     rsi, gapEnd
    mov     rdi, pBuffer
    add     rdi, gapStart
    push    rcx
    rep     movsb
    pop     rcx
    add     gapStart, rcx
    add     gapEnd, rcx
@@mgdone:
    pop     rbx
    pop     rdi
    pop     rsi
    ret
GapBuffer_MoveGap ENDP

align 16
GapBuffer_EnsureGap PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48
    mov     rbx, rcx
    call    GapBuffer_GapSize
    cmp     rax, rbx
    jae     @@egdone
    mov     rdi, bufferCapacity
    add     rdi, rbx
    add     rdi, 4096
    mov     r8, rdi
    call    MemAlloc
    test    rax, rax
    jz      @@egfail
    mov     rsi, rax
    mov     rcx, rsi
    mov     rdx, pBuffer
    mov     r8, gapStart
    call    MemCopy
    mov     rcx, rsi
    add     rcx, rdi
    mov     rax, bufferCapacity
    sub     rax, gapEnd
    sub     rcx, rax
    mov     rdx, pBuffer
    add     rdx, gapEnd
    mov     r8, rax
    call    MemCopy
    mov     rcx, pBuffer
    call    MemFree
    mov     pBuffer, rsi
    mov     rax, rdi
    sub     rax, bufferCapacity
    mov     rcx, gapEnd
    sub     rcx, gapStart
    add     rcx, rax
    add     gapEnd, rax
    mov     bufferCapacity, rdi
@@egdone:
    xor     eax, eax
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@@egfail:
    mov     eax, 1
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GapBuffer_EnsureGap ENDP

align 16
GapBuffer_InsertChar PROC
    push    rbx
    sub     rsp, 32
    movzx   ebx, cl
    mov     rcx, 1
    call    GapBuffer_EnsureGap
    test    eax, eax
    jnz     @@icfail
    mov     rcx, curOffset
    cmp     rcx, gapStart
    je      @@icwrite
    call    GapBuffer_MoveGap
@@icwrite:
    mov     rax, pBuffer
    mov     rcx, gapStart
    mov     [rax+rcx], bl
    inc     gapStart
    inc     curOffset
    mov     modified, 1
    mov     linesDirty, 1
    cmp     bl, 0Ah
    jne     @@icnolf
    inc     lineCount
    inc     curLine
    mov     curCol, 0
    jmp     @@icdone
@@icnolf:
    inc     curCol
@@icdone:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
@@icfail:
    mov     eax, 1
    add     rsp, 32
    pop     rbx
    ret
GapBuffer_InsertChar ENDP

align 16
GapBuffer_Backspace PROC
    sub     rsp, 32
    mov     rax, gapStart
    test    rax, rax
    jz      @@bsdone
    mov     rcx, curOffset
    cmp     rcx, gapStart
    je      @@bsdel
    call    GapBuffer_MoveGap
@@bsdel:
    dec     gapStart
    dec     curOffset
    mov     rax, pBuffer
    mov     rcx, gapStart
    movzx   edx, byte ptr [rax+rcx]
    mov     modified, 1
    mov     linesDirty, 1
    cmp     dl, 0Ah
    jne     @@bsnolf
    dec     lineCount
    cmp     curLine, 0
    je      @@bsnolf
    dec     curLine
@@bsnolf:
@@bsdone:
    add     rsp, 32
    ret
GapBuffer_Backspace ENDP

align 16
GapBuffer_Delete PROC
    sub     rsp, 32
    mov     rax, gapEnd
    cmp     rax, bufferCapacity
    jae     @@deldone
    mov     rcx, curOffset
    cmp     rcx, gapStart
    je      @@deldel
    call    GapBuffer_MoveGap
@@deldel:
    mov     rax, pBuffer
    mov     rcx, gapEnd
    movzx   edx, byte ptr [rax+rcx]
    inc     gapEnd
    mov     modified, 1
    mov     linesDirty, 1
    cmp     dl, 0Ah
    jne     @@deldone
    dec     lineCount
@@deldone:
    add     rsp, 32
    ret
GapBuffer_Delete ENDP

align 16
GapBuffer_GetChar PROC
    mov     rax, rcx
    cmp     rax, gapStart
    jb      @@gcbefore
    mov     rdx, gapEnd
    sub     rdx, gapStart
    add     rax, rdx
@@gcbefore:
    mov     rdx, pBuffer
    movzx   eax, byte ptr [rdx+rax]
    ret
GapBuffer_GetChar ENDP

; ==============================================================================
; LINE INDEX - O(n) build, O(1) lookup
; ==============================================================================
align 16
LineIndex_Build PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 48
    cmp     linesDirty, 0
    je      @@libdone
    cmp     lineOffsets, 0
    jne     @@liexists
    mov     r8, 80000h
    call    MemAlloc
    test    rax, rax
    jz      @@libdone
    mov     lineOffsets, rax
    mov     lineOffsetCap, 10000h
@@liexists:
    mov     rdi, lineOffsets
    mov     qword ptr [rdi], 0
    mov     rsi, 1
    call    GapBuffer_ContentLen
    mov     r12, rax
    xor     rbx, rbx
@@liscan:
    cmp     rbx, r12
    jae     @@lidone
    mov     rcx, rbx
    call    GapBuffer_GetChar
    cmp     al, 0Ah
    jne     @@linolf
    lea     rcx, [rbx+1]
    mov     [rdi+rsi*8], rcx
    inc     rsi
    cmp     rsi, lineOffsetCap
    jae     @@lidone
@@linolf:
    inc     rbx
    jmp     @@liscan
@@lidone:
    mov     lineCount, rsi
    mov     linesDirty, 0
@@libdone:
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
LineIndex_Build ENDP

align 16
LineIndex_GetOffset PROC
    cmp     rcx, lineCount
    jae     @@liinv
    mov     rax, lineOffsets
    mov     rax, [rax+rcx*8]
    ret
@@liinv:
    xor     eax, eax
    ret
LineIndex_GetOffset ENDP

; ==============================================================================
; UNDO MANAGER - Circular ring buffer
; ==============================================================================
align 16
Undo_Init PROC
    sub     rsp, 32
    mov     r8, 100000h
    call    MemAlloc
    mov     undoArena, rax
    mov     undoWritePos, 0
    mov     undoReadPos, 0
    mov     undoEntryCount, 0
    mov     undoSequence, 1
    mov     undoInProgress, 0
    add     rsp, 32
    ret
Undo_Init ENDP

align 16
Undo_PushInsert PROC
    cmp     undoInProgress, 1
    je      @@upiret
    mov     rax, undoArena
    test    rax, rax
    jz      @@upiret
    mov     rdx, undoWritePos
    imul    rdx, 32
    add     rax, rdx
    mov     dword ptr [rax+0], 1
    mov     [rax+8], rcx
    mov     [rax+16], rdx
    mov     edx, undoSequence
    mov     [rax+4], edx
    inc     undoWritePos
    cmp     undoWritePos, 4096
    jb      @@upinw
    mov     undoWritePos, 0
@@upinw:
    cmp     undoEntryCount, 4096
    jae     @@upiret
    inc     undoEntryCount
@@upiret:
    ret
Undo_PushInsert ENDP

align 16
Undo_PushDelete PROC
    cmp     undoInProgress, 1
    je      @@updret
    mov     rax, undoArena
    test    rax, rax
    jz      @@updret
    mov     rdx, undoWritePos
    imul    rdx, 32
    add     rax, rdx
    mov     dword ptr [rax+0], 2
    mov     [rax+8], rcx
    mov     [rax+16], rdx
    mov     edx, undoSequence
    mov     [rax+4], edx
    inc     undoWritePos
    cmp     undoWritePos, 4096
    jb      @@updnw
    mov     undoWritePos, 0
@@updnw:
    cmp     undoEntryCount, 4096
    jae     @@updret
    inc     undoEntryCount
@@updret:
    ret
Undo_PushDelete ENDP

; ==============================================================================
; SEARCH ENGINE - Boyer-Moore-Horspool
; ==============================================================================
align 16
Search_BuildSkip PROC
    push    rdi
    push    rsi
    mov     rsi, rcx
    mov     rdi, rdx
    lea     rcx, searchBadChar
    mov     rax, rdi
    mov     rdx, 256
@@sbfill:
    mov     [rcx], al
    inc     rcx
    dec     rdx
    jnz     @@sbfill
    lea     rcx, searchBadChar
    xor     rdx, rdx
    mov     r8, rdi
    dec     r8
@@sbloop:
    cmp     rdx, r8
    jae     @@sbdone
    movzx   eax, byte ptr [rsi+rdx]
    mov     r9, r8
    sub     r9, rdx
    mov     byte ptr [rcx+rax], r9b
    inc     rdx
    jmp     @@sbloop
@@sbdone:
    pop     rsi
    pop     rdi
    ret
Search_BuildSkip ENDP

align 16
Search_Next PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12, rcx
    mov     r13, rdx
    mov     r14, r8
    mov     r15, r9
    test    r15, r15
    jz      @@snfail
    cmp     r15, r13
    ja      @@snfail
    mov     rbx, [rsp+64]
    cmp     rbx, -1
    jne     @@snresume
    xor     rbx, rbx
    jmp     @@snbounds
@@snresume:
    inc     rbx
@@snbounds:
    mov     rax, r13
    sub     rax, r15
@@snloop:
    cmp     rbx, rax
    ja      @@snfail
    xor     rdi, rdi
@@sncmp:
    cmp     rdi, r15
    jae     @@snhit
    lea     r8, [r12+rbx]
    movzx   ecx, byte ptr [r8+rdi]
    movzx   edx, byte ptr [r14+rdi]
    cmp     cl, dl
    jne     @@snskip
    inc     rdi
    jmp     @@sncmp
@@snskip:
    mov     rcx, r15
    dec     rcx
    add     rcx, rbx
    movzx   edx, byte ptr [r12+rcx]
    lea     rsi, searchBadChar
    movzx   edx, byte ptr [rsi+rdx]
    add     rbx, rdx
    test    rdx, rdx
    jnz     @@snloop
    inc     rbx
    jmp     @@snloop
@@snhit:
    mov     rax, rbx
    jmp     @@sndone
@@snfail:
    mov     rax, -1
@@sndone:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Search_Next ENDP

; ==============================================================================
; CLIPBOARD RING - 32 entries, 64KB each max
; ==============================================================================
align 16
Clip_Init PROC
    sub     rsp, 32
    mov     r8, 200000h
    call    MemAlloc
    mov     clipArena, rax
    mov     clipHead, 0
    mov     clipTail, 0
    mov     clipCurrent, 0
    mov     clipCount, 0
    mov     clipSequence, 0
    add     rsp, 32
    ret
Clip_Init ENDP

align 16
Clip_Push PROC
    push    rbx
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, rdx
    mov     rax, clipArena
    test    rax, rax
    jz      @@cpdone
    mov     ebx, clipTail
    imul    ebx, 10008h
    add     rax, rbx
    mov     [rax], rdi
    mov     rcx, rax
    add     rcx, 8
    mov     rdx, rsi
    mov     r8, rdi
    call    MemCopy
    inc     clipTail
    cmp     clipTail, 32
    jb      @@cpnw
    mov     clipTail, 0
@@cpnw:
    cmp     clipCount, 32
    jae     @@cpdone
    inc     clipCount
@@cpdone:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Clip_Push ENDP

; ==============================================================================
; VIEWPORT / CURSOR MANAGEMENT
; ==============================================================================
align 16
View_ScrollToCursor PROC
    mov     rax, curLine
    cmp     rax, topLine
    jae     @@vschkdn
    mov     topLine, rax
    ret
@@vschkdn:
    mov     rdx, topLine
    mov     ecx, viewLines
    dec     ecx
    add     rdx, rcx
    cmp     rax, rdx
    jbe     @@vsdone
    sub     rax, rcx
    mov     topLine, rax
@@vsdone:
    ret
View_ScrollToCursor ENDP

align 16
View_CursorLeft PROC
    cmp     curOffset, 0
    je      @@cldone
    dec     curOffset
    cmp     curCol, 0
    je      @@clprevline
    dec     curCol
    jmp     @@cldone
@@clprevline:
    cmp     curLine, 0
    je      @@cldone
    dec     curLine
    mov     curCol, 0
@@cldone:
    call    View_ScrollToCursor
    ret
View_CursorLeft ENDP

align 16
View_CursorRight PROC
    push    rbx
    call    GapBuffer_ContentLen
    mov     rbx, rax
    cmp     curOffset, rbx
    jae     @@crdone
    mov     rcx, curOffset
    call    GapBuffer_GetChar
    inc     curOffset
    cmp     al, 0Ah
    jne     @@crnolf
    inc     curLine
    mov     curCol, 0
    jmp     @@crupd
@@crnolf:
    inc     curCol
@@crupd:
    call    View_ScrollToCursor
@@crdone:
    pop     rbx
    ret
View_CursorRight ENDP

align 16
View_CursorUp PROC
    cmp     curLine, 0
    je      @@cudone
    dec     curLine
    call    LineIndex_Build
    mov     rcx, curLine
    call    LineIndex_GetOffset
    add     rax, curCol
    mov     curOffset, rax
    call    View_ScrollToCursor
@@cudone:
    ret
View_CursorUp ENDP

align 16
View_CursorDown PROC
    mov     rax, curLine
    inc     rax
    cmp     rax, lineCount
    jae     @@cddone
    mov     curLine, rax
    call    LineIndex_Build
    mov     rcx, curLine
    call    LineIndex_GetOffset
    add     rax, curCol
    mov     curOffset, rax
    call    View_ScrollToCursor
@@cddone:
    ret
View_CursorDown ENDP

align 16
View_Home PROC
    call    LineIndex_Build
    mov     rcx, curLine
    call    LineIndex_GetOffset
    mov     curOffset, rax
    mov     curCol, 0
    ret
View_Home ENDP

align 16
View_End PROC
    push    rbx
    call    LineIndex_Build
    mov     rcx, curLine
    inc     rcx
    cmp     rcx, lineCount
    jae     @@veeof
    call    LineIndex_GetOffset
    dec     rax
    mov     curOffset, rax
    jmp     @@vecalccol
@@veeof:
    call    GapBuffer_ContentLen
    mov     curOffset, rax
@@vecalccol:
    mov     rcx, curLine
    call    LineIndex_GetOffset
    mov     rbx, curOffset
    sub     rbx, rax
    mov     curCol, rbx
    pop     rbx
    ret
View_End ENDP

; ==============================================================================
; LEXER - Incremental syntax tokenizer (returns token type for a byte)
; ==============================================================================
align 16
Lexer_ClassifyChar PROC
    movzx   eax, cl
    cmp     al, 0Ah
    je      @@lcnl
    cmp     al, 0Dh
    je      @@lcnl
    cmp     al, 20h
    je      @@lcws
    cmp     al, 09h
    je      @@lcws
    cmp     al, ';'
    je      @@lccom
    cmp     al, '/'
    je      @@lcslash
    cmp     al, '"'
    je      @@lcstr
    cmp     al, 27h
    je      @@lcstr
    cmp     al, '#'
    je      @@lcmacro
    cmp     al, '0'
    jb      @@lcop
    cmp     al, '9'
    jbe     @@lcnum
    cmp     al, '_'
    je      @@lcid
    cmp     al, 'A'
    jb      @@lcop
    cmp     al, 'Z'
    jbe     @@lcid
    cmp     al, 'a'
    jb      @@lcop
    cmp     al, 'z'
    jbe     @@lcid
    jmp     @@lcop
@@lcnl:  mov eax, 2
    ret
@@lcws:  mov eax, 3
    ret
@@lccom: mov eax, 5
    ret
@@lcslash: mov eax, 50
    ret
@@lcstr: mov eax, 11
    ret
@@lcmacro: mov eax, 14
    ret
@@lcnum: mov eax, 10
    ret
@@lcid:  mov eax, 20
    ret
@@lcop:  mov eax, 30
    ret
Lexer_ClassifyChar ENDP

; ==============================================================================
; FILE I/O
; ==============================================================================
align 16
File_Load PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    push    rbx
    push    rsi
    push    rdi
    mov     rbx, rcx

    sub     rsp, 56
    mov     rcx, rbx
    mov     edx, 80000000h
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+32], 3
    mov     qword ptr [rsp+40], 80h
    mov     qword ptr [rsp+48], 0
    call    qword ptr [fnCreateFileA]
    add     rsp, 56
    cmp     rax, -1
    je      @@flfail
    mov     rsi, rax

    sub     rsp, 32
    mov     rcx, rsi
    xor     edx, edx
    call    qword ptr [fnGetFileSize]
    add     rsp, 32
    mov     rdi, rax
    test    edi, edi
    jz      @@flclose

    add     rdi, 4096
    mov     rcx, rdi
    call    GapBuffer_Init

    sub     rsp, 40
    mov     rcx, rsi
    mov     rdx, pBuffer
    mov     r8d, edi
    sub     r8d, 4096
    lea     r9, [rbp-8]
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnReadFile]
    add     rsp, 40

    mov     eax, [rbp-8]
    mov     gapStart, rax
    mov     curOffset, 0
    mov     curLine, 0
    mov     curCol, 0
    mov     linesDirty, 1
    call    LineIndex_Build

@@flclose:
    sub     rsp, 32
    mov     rcx, rsi
    call    qword ptr [fnCloseHandle]
    add     rsp, 32
    mov     rcx, rbx
    lea     rdx, filePathBuf
    call    StrCopyA
    mov     modified, 0
    xor     eax, eax
    jmp     @@flexit

@@flfail:
    mov     eax, 1
@@flexit:
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
File_Load ENDP

align 16
File_Save PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    push    rbx
    push    rsi

    lea     rcx, filePathBuf
    cmp     byte ptr [rcx], 0
    je      @@fsfail

    sub     rsp, 56
    mov     rcx, rcx
    mov     edx, 40000000h
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+32], 2
    mov     qword ptr [rsp+40], 80h
    mov     qword ptr [rsp+48], 0
    lea     rcx, filePathBuf
    call    qword ptr [fnCreateFileA]
    add     rsp, 56
    cmp     rax, -1
    je      @@fsfail
    mov     rbx, rax

    mov     rax, gapStart
    test    rax, rax
    jz      @@fsafter
    sub     rsp, 40
    mov     rcx, rbx
    mov     rdx, pBuffer
    mov     r8, gapStart
    lea     r9, [rbp-8]
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnWriteFile]
    add     rsp, 40

@@fsafter:
    mov     rax, bufferCapacity
    sub     rax, gapEnd
    test    rax, rax
    jz      @@fsclose
    sub     rsp, 40
    mov     rcx, rbx
    mov     rdx, pBuffer
    add     rdx, gapEnd
    mov     r8, bufferCapacity
    sub     r8, gapEnd
    lea     r9, [rbp-8]
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnWriteFile]
    add     rsp, 40

@@fsclose:
    sub     rsp, 32
    mov     rcx, rbx
    call    qword ptr [fnCloseHandle]
    add     rsp, 32
    mov     modified, 0
    xor     eax, eax
    jmp     @@fsexit

@@fsfail:
    mov     eax, 1
@@fsexit:
    pop     rsi
    pop     rbx
    leave
    ret
File_Save ENDP

align 16
StrCopyA PROC
    push    rsi
    push    rdi
    mov     rdi, rdx
    mov     rsi, rcx
@@sca:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    test    al, al
    jnz     @@sca
    pop     rdi
    pop     rsi
    ret
StrCopyA ENDP

; ==============================================================================
; EDITOR INITIALIZATION
; ==============================================================================
align 16
EditorInit PROC
    sub     rsp, 48

    mov     rcx, 10000h
    call    GapBuffer_Init

    call    Undo_Init
    call    Clip_Init

    mov     topLine, 0
    mov     topCol, 0
    mov     curLine, 0
    mov     curCol, 0
    mov     curOffset, 0
    mov     selAnchor, -1
    mov     selEnd, -1
    mov     viewLines, 40
    mov     viewCols, 120

    mov     linesDirty, 1
    call    LineIndex_Build

    add     rsp, 48
    ret
EditorInit ENDP

; ==============================================================================
; RENDERING - Extract line, classify tokens, draw with GDI
; ==============================================================================
align 16
ExtractLine PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    mov     rdi, rcx
    mov     r12, rdx
    call    LineIndex_Build
    mov     rcx, r12
    call    LineIndex_GetOffset
    mov     rsi, rax
    xor     ebx, ebx
@@elloop:
    cmp     ebx, 4090
    jae     @@eldone
    mov     rcx, rsi
    add     rcx, rbx
    call    GapBuffer_ContentLen
    mov     rdx, rsi
    add     rdx, rbx
    cmp     rdx, rax
    jae     @@eldone
    mov     rcx, rdx
    call    GapBuffer_GetChar
    cmp     al, 0Ah
    je      @@eldone
    cmp     al, 0Dh
    je      @@eldone
    cmp     al, 09h
    jne     @@elstore
    mov     ecx, ebx
    and     ecx, 3
    mov     edx, 4
    sub     edx, ecx
@@eltab:
    cmp     ebx, 4090
    jae     @@eldone
    mov     byte ptr [rdi+rbx], 20h
    inc     ebx
    dec     edx
    jnz     @@eltab
    jmp     @@elloop
@@elstore:
    mov     [rdi+rbx], al
    inc     ebx
    jmp     @@elloop
@@eldone:
    mov     byte ptr [rdi+rbx], 0
    mov     eax, ebx
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ExtractLine ENDP

align 16
RenderLine PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 80
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r12, rcx
    mov     r13d, edx
    mov     r14d, r8d
    mov     r15d, r9d

    cmp     r14, curLine
    jne     @@rlnohl
    mov     ecx, colCursorLine
    sub     rsp, 32
    mov     rcx, r12
    mov     edx, 0
    mov     r8d, r15d
    mov     r9d, clientWidth
    push    r9
    mov     eax, charHeight
    push    rax
    push    r8
    push    0
    mov     rax, fnPatBlt
    test    rax, rax
    jz      @@rlnohl2
    pop     rcx
    pop     rdx
    pop     r8
    pop     r9
    jmp     @@rlnohl
@@rlnohl2:
    add     rsp, 32
@@rlnohl:

    lea     rcx, lineBuf
    mov     rdx, r14
    call    ExtractLine
    mov     ebx, eax

    sub     rsp, 32
    mov     rcx, r12
    mov     edx, 1
    call    qword ptr [fnSetBkMode]
    add     rsp, 32

    lea     rcx, numBuf
    mov     rdx, r14
    inc     rdx
    call    IntToStr
    mov     rsi, rax

    sub     rsp, 32
    mov     rcx, r12
    mov     edx, colLineNum
    call    qword ptr [fnSetTextColor]
    add     rsp, 32

    sub     rsp, 48
    mov     rcx, r12
    mov     edx, 4
    mov     r8d, r15d
    lea     r9, numBuf
    mov     dword ptr [rsp+32], esi
    call    qword ptr [fnTextOutA]
    add     rsp, 48

    test    ebx, ebx
    jz      @@rldone

    sub     rsp, 32
    mov     rcx, r12
    mov     edx, colText
    call    qword ptr [fnSetTextColor]
    add     rsp, 32

    mov     eax, charWidth
    imul    eax, 6
    add     eax, 8

    sub     rsp, 48
    mov     rcx, r12
    mov     edx, eax
    mov     r8d, r15d
    lea     r9, lineBuf
    mov     dword ptr [rsp+32], ebx
    call    qword ptr [fnTextOutA]
    add     rsp, 48

@@rldone:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
RenderLine ENDP

align 16
EditorRender PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 96
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     r12, rcx

    sub     rsp, 32
    mov     rcx, r12
    mov     rdx, hFont
    call    qword ptr [fnSelectObject]
    add     rsp, 32
    mov     r13, rax

    sub     rsp, 32
    mov     rcx, r12
    mov     edx, colBackground
    call    qword ptr [fnSetBkColor]
    add     rsp, 32

    sub     rsp, 32
    mov     rcx, r12
    mov     edx, 2
    call    qword ptr [fnSetBkMode]
    add     rsp, 32

    sub     rsp, 48
    mov     rcx, r12
    xor     edx, edx
    xor     r8d, r8d
    mov     r9d, clientWidth
    mov     dword ptr [rsp+32], 0
    mov     eax, clientHeight
    mov     dword ptr [rsp+36], eax
    mov     dword ptr [rsp+40], 42h
    call    qword ptr [fnPatBlt]
    add     rsp, 48

    call    LineIndex_Build

    mov     rbx, topLine
    xor     esi, esi
@@erloop:
    mov     eax, viewLines
    cmp     esi, eax
    jae     @@erdone
    mov     rax, rbx
    add     rax, rsi
    cmp     rax, lineCount
    jae     @@erdone

    mov     eax, esi
    imul    eax, charHeight
    mov     ecx, eax

    push    rsi
    push    rbx
    mov     rcx, r12
    mov     edx, esi
    mov     r8, rbx
    add     r8, rsi
    mov     r9d, eax
    call    RenderLine
    pop     rbx
    pop     rsi

    inc     esi
    jmp     @@erloop

@@erdone:
    sub     rsp, 32
    mov     rcx, r12
    mov     rdx, r13
    call    qword ptr [fnSelectObject]
    add     rsp, 32

    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    leave
    ret
EditorRender ENDP

; ==============================================================================
; WINDOW PROCEDURE
; ==============================================================================
align 16
WndProc PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 160
    mov     [rbp+10h], rcx
    mov     [rbp+18h], edx
    mov     [rbp+20h], r8
    mov     [rbp+28h], r9

    cmp     edx, 01h
    je      @@wc
    cmp     edx, 05h
    je      @@ws
    cmp     edx, 0Fh
    je      @@wp
    cmp     edx, 100h
    je      @@wkd
    cmp     edx, 102h
    je      @@wch
    cmp     edx, 115h
    je      @@wvs
    cmp     edx, 07h
    je      @@wsf
    cmp     edx, 08h
    je      @@wkf
    cmp     edx, 02h
    je      @@wd
    jmp     @@wdef

@@wc:
    mov     rcx, [rbp+10h]
    mov     hWndMain, rcx

    sub     rsp, 32
    mov     rcx, hWndMain
    lea     rdx, tmBuf
    call    qword ptr [fnGetDC]
    add     rsp, 32
    mov     [rbp-8], rax

    lea     rcx, logfontBuf
    lea     rdx, logfontBuf
    mov     r8, 64
    call    MemZero
    lea     rcx, logfontBuf
    mov     dword ptr [rcx], -16
    mov     dword ptr [rcx+16], 400
    mov     byte ptr [rcx+23], 1
    mov     byte ptr [rcx+27], 31h
    lea     rdi, [rcx+28]
    lea     rsi, szFontName
@@wcfn:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    test    al, al
    jnz     @@wcfn

    sub     rsp, 32
    lea     rcx, logfontBuf
    call    qword ptr [fnCreateFontIndirectA]
    add     rsp, 32
    mov     hFont, rax

    sub     rsp, 32
    mov     rcx, [rbp-8]
    mov     rdx, hFont
    call    qword ptr [fnSelectObject]
    add     rsp, 32

    sub     rsp, 32
    mov     rcx, [rbp-8]
    lea     rdx, tmBuf
    call    qword ptr [fnGetTextMetricsA]
    add     rsp, 32
    lea     rcx, tmBuf
    mov     eax, [rcx]
    mov     charHeight, eax
    mov     eax, [rcx+20]
    mov     charWidth, eax

    sub     rsp, 32
    mov     rcx, hWndMain
    mov     rdx, [rbp-8]
    call    qword ptr [fnReleaseDC]
    add     rsp, 32

    lea     rcx, siBuf
    mov     dword ptr [rcx], 28
    mov     dword ptr [rcx+4], 7
    mov     dword ptr [rcx+8], 0
    mov     dword ptr [rcx+12], 10000
    mov     dword ptr [rcx+16], 40
    mov     dword ptr [rcx+20], 0

    sub     rsp, 40
    mov     rcx, hWndMain
    mov     edx, 1
    lea     r8, siBuf
    xor     r9d, r9d
    call    qword ptr [fnSetScrollInfo]
    add     rsp, 40

    sub     rsp, 32
    mov     ecx, colBackground
    call    qword ptr [fnCreateSolidBrush]
    add     rsp, 32
    mov     hBgBrush, rax

    jmp     @@wret0

@@ws:
    mov     eax, [rbp+28h]
    and     eax, 0FFFFh
    mov     clientWidth, eax
    mov     eax, [rbp+28h]
    shr     eax, 16
    mov     clientHeight, eax
    mov     ecx, clientHeight
    xor     edx, edx
    mov     eax, charHeight
    test    eax, eax
    jz      @@wsnovl
    xchg    eax, ecx
    xor     edx, edx
    div     ecx
    mov     viewLines, eax
@@wsnovl:
    sub     rsp, 32
    mov     rcx, hWndMain
    xor     edx, edx
    xor     r8d, r8d
    mov     r9d, 1
    call    qword ptr [fnInvalidateRect]
    add     rsp, 32
    jmp     @@wret0

@@wp:
    lea     rdx, psBuf
    sub     rsp, 32
    mov     rcx, [rbp+10h]
    call    qword ptr [fnBeginPaint]
    add     rsp, 32
    mov     [rbp-16], rax

    mov     rcx, rax
    call    EditorRender

    lea     rdx, psBuf
    sub     rsp, 32
    mov     rcx, [rbp+10h]
    call    qword ptr [fnEndPaint]
    add     rsp, 32
    jmp     @@wret0

@@wsf:
    mov     hasFocus, 1
    sub     rsp, 32
    mov     rcx, [rbp+10h]
    xor     edx, edx
    mov     r8d, charWidth
    mov     r9d, charHeight
    call    qword ptr [fnCreateCaret]
    add     rsp, 32
    sub     rsp, 32
    mov     rcx, [rbp+10h]
    call    qword ptr [fnShowCaret]
    add     rsp, 32
    jmp     @@wret0

@@wkf:
    mov     hasFocus, 0
    sub     rsp, 32
    mov     rcx, [rbp+10h]
    call    qword ptr [fnHideCaret]
    add     rsp, 32
    sub     rsp, 32
    call    qword ptr [fnDestroyCaret]
    add     rsp, 32
    jmp     @@wret0

@@wkd:
    mov     rax, [rbp+20h]
    cmp     eax, 25h
    je      @@kl
    cmp     eax, 27h
    je      @@kr
    cmp     eax, 26h
    je      @@ku
    cmp     eax, 28h
    je      @@kdown
    cmp     eax, 24h
    je      @@khome
    cmp     eax, 23h
    je      @@kend
    cmp     eax, 2Eh
    je      @@kdel
    cmp     eax, 21h
    je      @@kpgup
    cmp     eax, 22h
    je      @@kpgdn
    cmp     eax, 53h
    je      @@ksave
    jmp     @@wret0

@@kl:
    call    View_CursorLeft
    jmp     @@kinv
@@kr:
    call    View_CursorRight
    jmp     @@kinv
@@ku:
    call    View_CursorUp
    jmp     @@kinv
@@kdown:
    call    View_CursorDown
    jmp     @@kinv
@@khome:
    call    View_Home
    jmp     @@kinv
@@kend:
    call    View_End
    jmp     @@kinv
@@kdel:
    call    GapBuffer_Delete
    jmp     @@kinv
@@kpgup:
    mov     eax, viewLines
    mov     rcx, topLine
    sub     rcx, rax
    mov     topLine, rcx
    js      @@kpgufix
    jmp     @@kinv
@@kpgufix:
    mov     topLine, 0
    jmp     @@kinv
@@kpgdn:
    mov     eax, viewLines
    mov     rcx, topLine
    add     rcx, rax
    mov     topLine, rcx
    jmp     @@kinv
@@ksave:
    sub     rsp, 32
    mov     ecx, 11h
    call    qword ptr [fnGetKeyState]
    add     rsp, 32
    test    ax, 8000h
    jz      @@wret0
    call    File_Save
    jmp     @@kinv

@@kinv:
    sub     rsp, 32
    mov     rcx, hWndMain
    xor     edx, edx
    xor     r8d, r8d
    mov     r9d, 1
    call    qword ptr [fnInvalidateRect]
    add     rsp, 32

    mov     rax, curCol
    sub     rax, topCol
    mov     ecx, charWidth
    imul    eax, ecx
    add     eax, charWidth
    imul    eax, 6
    add     eax, 8
    mov     edx, eax
    mov     rax, curLine
    sub     rax, topLine
    mov     ecx, charHeight
    imul    eax, ecx
    mov     r8d, eax
    sub     rsp, 32
    mov     ecx, edx
    mov     edx, r8d
    call    qword ptr [fnSetCaretPos]
    add     rsp, 32
    jmp     @@wret0

@@wch:
    movzx   eax, byte ptr [rbp+20h]
    cmp     al, 08h
    je      @@chbs
    cmp     al, 0Dh
    je      @@chenter
    cmp     al, 1Ah
    je      @@wret0
    cmp     al, 13h
    je      @@chsave
    cmp     al, 20h
    jb      @@wret0
    mov     cl, al
    call    GapBuffer_InsertChar
    jmp     @@kinv

@@chbs:
    call    GapBuffer_Backspace
    cmp     curCol, 0
    jne     @@kinv
    jmp     @@kinv

@@chenter:
    mov     cl, 0Ah
    call    GapBuffer_InsertChar
    jmp     @@kinv

@@chsave:
    sub     rsp, 32
    mov     ecx, 11h
    call    qword ptr [fnGetKeyState]
    add     rsp, 32
    test    ax, 8000h
    jz      @@wret0
    call    File_Save
    jmp     @@kinv

@@wvs:
    lea     rcx, siBuf
    mov     dword ptr [rcx], 28
    mov     dword ptr [rcx+4], 17h
    sub     rsp, 40
    mov     rcx, [rbp+10h]
    mov     edx, 1
    lea     r8, siBuf
    call    qword ptr [fnGetScrollInfo]
    add     rsp, 40
    mov     eax, [rbp+20h]
    and     eax, 0FFFFh
    cmp     eax, 0
    je      @@vsup
    cmp     eax, 1
    je      @@vsdn
    cmp     eax, 5
    je      @@vstr
    cmp     eax, 2
    je      @@vspgu
    cmp     eax, 3
    je      @@vspgd
    jmp     @@vsdone
@@vsup:
    cmp     topLine, 0
    je      @@vsdone
    dec     topLine
    jmp     @@vsdone
@@vsdn:
    inc     topLine
    jmp     @@vsdone
@@vstr:
    lea     rcx, siBuf
    mov     eax, [rcx+24]
    mov     topLine, rax
    jmp     @@vsdone
@@vspgu:
    mov     eax, viewLines
    mov     rcx, topLine
    sub     rcx, rax
    mov     topLine, rcx
    js      @@vsfix
    jmp     @@vsdone
@@vsfix:
    mov     topLine, 0
    jmp     @@vsdone
@@vspgd:
    mov     eax, viewLines
    mov     rcx, topLine
    add     rcx, rax
    mov     topLine, rcx
@@vsdone:
    lea     rcx, siBuf
    mov     rax, topLine
    mov     [rcx+20], eax
    mov     dword ptr [rcx+4], 4
    sub     rsp, 40
    mov     rcx, [rbp+10h]
    mov     edx, 1
    lea     r8, siBuf
    xor     r9d, r9d
    call    qword ptr [fnSetScrollInfo]
    add     rsp, 40
    jmp     @@kinv

@@wd:
    sub     rsp, 32
    xor     ecx, ecx
    call    qword ptr [fnPostQuitMessage]
    add     rsp, 32
    jmp     @@wret0

@@wdef:
    sub     rsp, 32
    mov     rcx, [rbp+10h]
    mov     edx, [rbp+18h]
    mov     r8, [rbp+20h]
    mov     r9, [rbp+28h]
    call    qword ptr [fnDefWindowProcA]
    add     rsp, 32
    leave
    ret

@@wret0:
    xor     eax, eax
    leave
    ret
WndProc ENDP

; ==============================================================================
; WINMAIN - Window creation and message loop
; ==============================================================================
align 16
WinMain PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 200

    sub     rsp, 32
    xor     ecx, ecx
    call    qword ptr [fnGetModuleHandleA]
    add     rsp, 32
    mov     hInstance, rax

    lea     rcx, wcBuf
    mov     rdx, 88
    call    MemZero

    lea     rcx, wcBuf
    mov     dword ptr [rcx+0], 80
    mov     dword ptr [rcx+4], 23h
    lea     rax, WndProc
    mov     [rcx+8], rax
    mov     dword ptr [rcx+16], 0
    mov     dword ptr [rcx+20], 0
    mov     rax, hInstance
    mov     [rcx+24], rax

    push    rcx
    sub     rsp, 32
    xor     ecx, ecx
    mov     edx, 7F00h
    call    qword ptr [fnLoadIconA]
    add     rsp, 32
    pop     rcx
    mov     [rcx+32], rax
    mov     [rcx+72], rax

    push    rcx
    sub     rsp, 32
    xor     ecx, ecx
    mov     edx, 7F00h
    call    qword ptr [fnLoadCursorA]
    add     rsp, 32
    pop     rcx
    mov     [rcx+40], rax

    mov     qword ptr [rcx+48], 6
    mov     qword ptr [rcx+56], 0
    lea     rax, szClassName
    mov     [rcx+64], rax

    sub     rsp, 32
    lea     rcx, wcBuf
    call    qword ptr [fnRegisterClassExA]
    add     rsp, 32

    sub     rsp, 104
    xor     ecx, ecx
    lea     rdx, szClassName
    lea     r8, szTitle
    mov     r9d, 10CF0000h
    or      r9d, 200000h
    mov     dword ptr [rsp+32], 80000000h
    mov     dword ptr [rsp+40], 80000000h
    mov     dword ptr [rsp+48], 1400
    mov     dword ptr [rsp+56], 800
    mov     qword ptr [rsp+64], 0
    mov     qword ptr [rsp+72], 0
    mov     rax, hInstance
    mov     [rsp+80], rax
    mov     qword ptr [rsp+88], 0
    call    qword ptr [fnCreateWindowExA]
    add     rsp, 104
    mov     hWndMain, rax

    sub     rsp, 32
    mov     rcx, hWndMain
    mov     edx, 0Ah
    call    qword ptr [fnShowWindow]
    add     rsp, 32

    sub     rsp, 32
    mov     rcx, hWndMain
    call    qword ptr [fnUpdateWindow]
    add     rsp, 32

    sub     rsp, 32
    call    qword ptr [fnGetCommandLineA]
    add     rsp, 32
    test    rax, rax
    jz      @@wmloop
    mov     rcx, rax
    call    StrLenA
    test    rax, rax
    jz      @@wmloop
    mov     rcx, [fnGetCommandLineA]
    sub     rsp, 32
    call    qword ptr [fnGetCommandLineA]
    add     rsp, 32

@@wmloop:
    sub     rsp, 32
    lea     rcx, msgBuf
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    qword ptr [fnGetMessageA]
    add     rsp, 32
    test    eax, eax
    jz      @@wmexit

    sub     rsp, 32
    lea     rcx, msgBuf
    call    qword ptr [fnTranslateMessage]
    add     rsp, 32

    sub     rsp, 32
    lea     rcx, msgBuf
    call    qword ptr [fnDispatchMessageA]
    add     rsp, 32
    jmp     @@wmloop

@@wmexit:
    lea     rcx, msgBuf
    mov     eax, [rcx+8]
    add     rsp, 200
    pop     rdi
    pop     rsi
    pop     rbx
    ret
WinMain ENDP

; ==============================================================================
; DOS HEADER EMITTER - Write valid MZ header to buffer
; RCX = dest buffer (64+ bytes), EDX = e_lfanew value
; Returns RAX = 64 (bytes written)
; ==============================================================================
align 16
Emit_DosHeader PROC
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     esi, edx
    lea     rcx, DosHeaderTemplate
    mov     rdx, rdi
    mov     r8, 60
    push    rsi
    call    MemCopy
    pop     rsi
    mov     [rdi+60], esi
    mov     rax, 64
    pop     rsi
    pop     rdi
    ret
Emit_DosHeader ENDP

; ==============================================================================
; DOS STUB EMITTER - Write stub after DOS header
; RCX = dest buffer (at offset 64), Returns RAX = bytes written
; ==============================================================================
align 16
Emit_DosStub PROC
    push    rdi
    push    rsi
    mov     rdi, rcx
    lea     rsi, DosStubCode
    xor     ecx, ecx
@@edsloop:
    mov     al, [rsi+rcx]
    mov     [rdi+rcx], al
    inc     ecx
    cmp     al, 24h
    jne     @@edsloop
    mov     rax, rcx
    pop     rsi
    pop     rdi
    ret
Emit_DosStub ENDP

; ==============================================================================
; PE SIGNATURE EMITTER - Write "PE\0\0" at e_lfanew offset
; RCX = buffer base, EDX = e_lfanew offset
; ==============================================================================
align 16
Emit_PeSignature PROC
    mov     dword ptr [rcx+rdx], 00004550h
    mov     rax, 4
    ret
Emit_PeSignature ENDP

; ==============================================================================
; COFF FILE HEADER EMITTER (20 bytes at e_lfanew+4)
; RCX = dest, DX = machine, R8W = sections, R9D = characteristics
; ==============================================================================
align 16
Emit_FileHeader PROC
    mov     [rcx], dx
    mov     [rcx+2], r8w
    mov     dword ptr [rcx+4], 0
    mov     dword ptr [rcx+8], 0
    mov     dword ptr [rcx+12], 0
    mov     word ptr [rcx+16], 240
    mov     [rcx+18], r9w
    mov     rax, 20
    ret
Emit_FileHeader ENDP

; ==============================================================================
; VALIDATE DOS HEADER
; RCX = buffer, Returns RAX = 0 valid, 1+ error
; ==============================================================================
align 16
Validate_DosHeader PROC
    cmp     word ptr [rcx], 5A4Dh
    jne     @@vdbad1
    mov     eax, [rcx+60]
    test    eax, 7
    jnz     @@vdbad2
    cmp     eax, 64
    jb      @@vdbad3
    xor     eax, eax
    ret
@@vdbad1:
    mov     eax, 1
    ret
@@vdbad2:
    mov     eax, 2
    ret
@@vdbad3:
    mov     eax, 3
    ret
Validate_DosHeader ENDP

; ==============================================================================
; STRING MANIPULATION - StrCompA, StrCompIA, StrCatA, StrChrA, StrRChrA,
;                       StrToUpper, StrToLower, StrRevA, StrTrimA, StrDupA
; ==============================================================================

; StrCompA - Compare two ANSI strings
; RCX = strA, RDX = strB
; Returns: RAX = 0 equal, <0 if A<B, >0 if A>B
align 16
StrCompA PROC
@@sc:
    movzx   eax, byte ptr [rcx]
    movzx   r8d, byte ptr [rdx]
    sub     eax, r8d
    jnz     @@scdone
    cmp     byte ptr [rcx], 0
    je      @@scdone
    inc     rcx
    inc     rdx
    jmp     @@sc
@@scdone:
    ret
StrCompA ENDP

; StrCompIA - Case-insensitive compare
; RCX = strA, RDX = strB
; Returns: RAX = 0 equal
align 16
StrCompIA PROC
@@sci:
    movzx   eax, byte ptr [rcx]
    movzx   r8d, byte ptr [rdx]
    cmp     al, 'A'
    jb      @@scinoup1
    cmp     al, 'Z'
    ja      @@scinoup1
    or      al, 20h
@@scinoup1:
    cmp     r8b, 'A'
    jb      @@scinoup2
    cmp     r8b, 'Z'
    ja      @@scinoup2
    or      r8b, 20h
@@scinoup2:
    sub     eax, r8d
    jnz     @@scidone
    cmp     byte ptr [rcx], 0
    je      @@scidone
    inc     rcx
    inc     rdx
    jmp     @@sci
@@scidone:
    ret
StrCompIA ENDP

; StrCatA - Concatenate strB onto strA
; RCX = dest (strA, must have space), RDX = src (strB)
; Returns: RAX = dest
align 16
StrCatA PROC
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rax, rdi
@@scatfind:
    cmp     byte ptr [rdi], 0
    je      @@scatcopy
    inc     rdi
    jmp     @@scatfind
@@scatcopy:
    mov     cl, [rsi]
    mov     [rdi], cl
    inc     rsi
    inc     rdi
    test    cl, cl
    jnz     @@scatcopy
    pop     rsi
    pop     rdi
    ret
StrCatA ENDP

; StrChrA - Find first occurrence of char in string
; RCX = str, DL = char
; Returns: RAX = pointer to char, 0 if not found
align 16
StrChrA PROC
@@schr:
    mov     al, [rcx]
    cmp     al, dl
    je      @@schrhit
    test    al, al
    jz      @@schrmiss
    inc     rcx
    jmp     @@schr
@@schrhit:
    mov     rax, rcx
    ret
@@schrmiss:
    xor     eax, eax
    ret
StrChrA ENDP

; StrRChrA - Find last occurrence of char in string
; RCX = str, DL = char
; Returns: RAX = pointer to last char, 0 if not found
align 16
StrRChrA PROC
    xor     rax, rax
@@srchr:
    mov     r8b, [rcx]
    test    r8b, r8b
    jz      @@srchdone
    cmp     r8b, dl
    jne     @@srchnxt
    mov     rax, rcx
@@srchnxt:
    inc     rcx
    jmp     @@srchr
@@srchdone:
    ret
StrRChrA ENDP

; StrToUpper - In-place uppercase
; RCX = str
; Returns: RAX = str
align 16
StrToUpper PROC
    mov     rax, rcx
@@stu:
    mov     dl, [rcx]
    test    dl, dl
    jz      @@studone
    cmp     dl, 'a'
    jb      @@stunxt
    cmp     dl, 'z'
    ja      @@stunxt
    and     dl, 0DFh
    mov     [rcx], dl
@@stunxt:
    inc     rcx
    jmp     @@stu
@@studone:
    ret
StrToUpper ENDP

; StrToLower - In-place lowercase
; RCX = str
; Returns: RAX = str
align 16
StrToLower PROC
    mov     rax, rcx
@@stl:
    mov     dl, [rcx]
    test    dl, dl
    jz      @@stldone
    cmp     dl, 'A'
    jb      @@stlnxt
    cmp     dl, 'Z'
    ja      @@stlnxt
    or      dl, 20h
    mov     [rcx], dl
@@stlnxt:
    inc     rcx
    jmp     @@stl
@@stldone:
    ret
StrToLower ENDP

; StrRevA - In-place reverse
; RCX = str
; Returns: RAX = str
align 16
StrRevA PROC
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     rax, rcx
    push    rax
    call    StrLenA
    mov     rsi, rax
    pop     rax
    test    rsi, rsi
    jz      @@srvdone
    lea     rdx, [rdi+rsi-1]
@@srvswap:
    cmp     rdi, rdx
    jae     @@srvdone
    mov     cl, [rdi]
    mov     r8b, [rdx]
    mov     [rdi], r8b
    mov     [rdx], cl
    inc     rdi
    dec     rdx
    jmp     @@srvswap
@@srvdone:
    pop     rsi
    pop     rdi
    ret
StrRevA ENDP

; StrTrimA - Trim leading/trailing whitespace in-place
; RCX = str
; Returns: RAX = str (adjusted ptr to first non-ws)
align 16
StrTrimA PROC
    push    rdi
    push    rsi
    mov     rdi, rcx
@@sttlead:
    mov     al, [rdi]
    cmp     al, 20h
    je      @@sttskip
    cmp     al, 09h
    je      @@sttskip
    cmp     al, 0Dh
    je      @@sttskip
    cmp     al, 0Ah
    je      @@sttskip
    jmp     @@stttrail
@@sttskip:
    inc     rdi
    jmp     @@sttlead
@@stttrail:
    mov     rax, rdi
    push    rax
    call    StrLenA
    pop     rax
    test    rax, rax
    jz      @@stttdone
    lea     rsi, [rdi+rax-1]
@@stttrim:
    cmp     rsi, rdi
    jb      @@stttdone
    mov     cl, [rsi]
    cmp     cl, 20h
    je      @@stttz
    cmp     cl, 09h
    je      @@stttz
    cmp     cl, 0Dh
    je      @@stttz
    cmp     cl, 0Ah
    je      @@stttz
    jmp     @@stttdone
@@stttz:
    mov     byte ptr [rsi], 0
    dec     rsi
    jmp     @@stttrim
@@stttdone:
    mov     rax, rdi
    pop     rsi
    pop     rdi
    ret
StrTrimA ENDP

; StrDupA - Allocate and copy string (uses VirtualAlloc)
; RCX = source string
; Returns: RAX = new string ptr, 0 on failure
align 16
StrDupA PROC
    push    rbx
    push    rsi
    sub     rsp, 32
    mov     rsi, rcx
    call    StrLenA
    inc     rax
    mov     rbx, rax
    mov     r8, rax
    call    MemAlloc
    test    rax, rax
    jz      @@sddone
    mov     rcx, rax
    mov     rdx, rsi
    mov     r8, rbx
    push    rax
    call    MemCopy
    pop     rax
@@sddone:
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
StrDupA ENDP

; StrNCompA - Compare up to N bytes
; RCX = strA, RDX = strB, R8 = max count
; Returns: RAX = 0 equal, diff otherwise
align 16
StrNCompA PROC
    test    r8, r8
    jz      @@snceq
@@snc:
    movzx   eax, byte ptr [rcx]
    movzx   r9d, byte ptr [rdx]
    sub     eax, r9d
    jnz     @@sncdone
    cmp     byte ptr [rcx], 0
    je      @@sncdone
    inc     rcx
    inc     rdx
    dec     r8
    jnz     @@snc
@@snceq:
    xor     eax, eax
@@sncdone:
    ret
StrNCompA ENDP

; ==============================================================================
; ERROR HANDLING - SetLastError, GetLastError wrappers + error dispatch
; ==============================================================================

; ErrSet - Set thread error code
; ECX = error code
align 16
ErrSet PROC
    sub     rsp, 32
    call    qword ptr [fnSetLastError]
    add     rsp, 32
    ret
ErrSet ENDP

; ErrGet - Get thread error code
; Returns: EAX = error code
align 16
ErrGet PROC
    sub     rsp, 32
    call    qword ptr [fnGetLastError]
    add     rsp, 32
    ret
ErrGet ENDP

; ErrCheck - Call GetLastError, return nonzero if error
; Returns: RAX = last error (0 = no error)
align 16
ErrCheck PROC
    sub     rsp, 32
    call    qword ptr [fnGetLastError]
    add     rsp, 32
    ret
ErrCheck ENDP

; ErrFatal - Display error via MessageBoxA and ExitProcess
; RCX = error message string, EDX = exit code
align 16
ErrFatal PROC
    push    rbx
    sub     rsp, 48
    mov     rbx, rdx
    xor     r9d, r9d
    mov     r8, rcx
    xor     edx, edx
    xor     ecx, ecx
    or      r9d, 10h
    call    qword ptr [fnMessageBoxA]
    mov     ecx, ebx
    call    qword ptr [fnExitProcess]
    int     3
ErrFatal ENDP

; ==============================================================================
; CONSOLE I/O - Attach/detach, read/write
; ==============================================================================

; Console_Attach - Allocate a console and get handles
; Returns: EAX = 0 success, 1 fail
align 16
Console_Attach PROC
    sub     rsp, 48
    call    qword ptr [fnAllocConsole]
    test    eax, eax
    jz      @@cafail

    mov     ecx, 0FFFFFFF5h
    call    qword ptr [fnGetStdHandle]
    mov     hStdIn, rax

    mov     ecx, 0FFFFFFF4h
    call    qword ptr [fnGetStdHandle]
    mov     hStdOut, rax

    mov     ecx, 0FFFFFFF4h
    sub     ecx, 1
    call    qword ptr [fnGetStdHandle]
    mov     hStdErr, rax

    mov     consoleAttached, 1
    xor     eax, eax
    add     rsp, 48
    ret
@@cafail:
    mov     eax, 1
    add     rsp, 48
    ret
Console_Attach ENDP

; Console_Detach - Free the console
align 16
Console_Detach PROC
    sub     rsp, 32
    call    qword ptr [fnFreeConsole]
    mov     consoleAttached, 0
    add     rsp, 32
    ret
Console_Detach ENDP

; Console_WriteA - Write ANSI string to stdout
; RCX = string ptr, RDX = length (0 = auto-strlen)
; Returns: EAX = bytes written
align 16
Console_WriteA PROC
    push    rbx
    sub     rsp, 48
    mov     rbx, rcx
    test    rdx, rdx
    jnz     @@cwhaslen
    call    StrLenA
    mov     rdx, rax
    mov     rcx, rbx
@@cwhaslen:
    mov     r8, rdx
    mov     rdx, rcx
    mov     rcx, hStdOut
    lea     r9, consoleWritten
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnWriteConsoleA]
    mov     eax, consoleWritten
    add     rsp, 48
    pop     rbx
    ret
Console_WriteA ENDP

; Console_ReadA - Read from stdin into buffer
; RCX = dest buffer, EDX = max chars
; Returns: EAX = chars read
align 16
Console_ReadA PROC
    push    rbx
    sub     rsp, 48
    mov     rbx, rcx
    mov     r8d, edx
    mov     rdx, rbx
    mov     rcx, hStdIn
    lea     r9, consoleRead
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnReadConsoleA]
    mov     eax, consoleRead
    add     rsp, 48
    pop     rbx
    ret
Console_ReadA ENDP

; Console_WriteLn - Write string + CRLF
; RCX = string ptr
align 16
Console_WriteLn PROC
    push    rbx
    sub     rsp, 48
    mov     rbx, rcx
    xor     edx, edx
    call    Console_WriteA
    lea     rcx, consoleBuf
    mov     byte ptr [rcx], 0Dh
    mov     byte ptr [rcx+1], 0Ah
    mov     rdx, 2
    call    Console_WriteA
    add     rsp, 48
    pop     rbx
    ret
Console_WriteLn ENDP

; ==============================================================================
; THREAD MANAGEMENT - Create, wait, suspend, resume, terminate
; ==============================================================================

; Thread_Create - Launch a new thread
; RCX = thread proc (LPTHREAD_START_ROUTINE), RDX = param
; Returns: RAX = thread handle, 0 on failure
align 16
Thread_Create PROC
    push    rbx
    sub     rsp, 56
    mov     r8, rcx
    mov     r9, rdx
    xor     ecx, ecx
    xor     edx, edx
    mov     qword ptr [rsp+32], 0
    mov     qword ptr [rsp+40], 0
    call    qword ptr [fnCreateThread]
    test    rax, rax
    jz      @@tcdone
    mov     rbx, rax
    mov     ecx, threadCount
    cmp     ecx, 16
    jae     @@tcfull
    lea     rdx, threadHandles
    mov     [rdx+rcx*8], rax
    inc     threadCount
@@tcfull:
    mov     rax, rbx
@@tcdone:
    add     rsp, 56
    pop     rbx
    ret
Thread_Create ENDP

; Thread_Wait - Wait for thread to complete
; RCX = thread handle, EDX = timeout ms (0FFFFFFFFh = INFINITE)
; Returns: EAX = wait result
align 16
Thread_Wait PROC
    sub     rsp, 32
    call    qword ptr [fnWaitForSingleObject]
    add     rsp, 32
    ret
Thread_Wait ENDP

; Thread_WaitAll - Wait for all tracked threads
; ECX = timeout ms
; Returns: EAX = wait result
align 16
Thread_WaitAll PROC
    push    rbx
    sub     rsp, 48
    mov     ebx, ecx
    mov     ecx, threadCount
    test    ecx, ecx
    jz      @@twadone
    lea     rdx, threadHandles
    mov     r8d, 1
    mov     r9d, ebx
    call    qword ptr [fnWaitForMultipleObjects]
    jmp     @@twaout
@@twadone:
    xor     eax, eax
@@twaout:
    add     rsp, 48
    pop     rbx
    ret
Thread_WaitAll ENDP

; Thread_Suspend - Suspend a thread
; RCX = thread handle
; Returns: EAX = previous suspend count
align 16
Thread_Suspend PROC
    sub     rsp, 32
    call    qword ptr [fnSuspendThread]
    add     rsp, 32
    ret
Thread_Suspend ENDP

; Thread_Resume - Resume a suspended thread
; RCX = thread handle
; Returns: EAX = previous suspend count
align 16
Thread_Resume PROC
    sub     rsp, 32
    call    qword ptr [fnResumeThread]
    add     rsp, 32
    ret
Thread_Resume ENDP

; Thread_Terminate - Force-terminate a thread
; RCX = handle, EDX = exit code
align 16
Thread_Terminate PROC
    sub     rsp, 32
    call    qword ptr [fnTerminateThread]
    add     rsp, 32
    ret
Thread_Terminate ENDP

; Thread_GetExitCode - Get thread exit code
; RCX = thread handle, RDX = ptr to DWORD result
; Returns: EAX = nonzero on success
align 16
Thread_GetExitCode PROC
    sub     rsp, 32
    call    qword ptr [fnGetExitCodeThread]
    add     rsp, 32
    ret
Thread_GetExitCode ENDP

; Thread_CloseAll - Close all tracked thread handles
align 16
Thread_CloseAll PROC
    push    rbx
    push    rsi
    sub     rsp, 32
    xor     esi, esi
    mov     ebx, threadCount
@@tcclose:
    cmp     esi, ebx
    jae     @@tcadone
    lea     rax, threadHandles
    mov     rcx, [rax+rsi*8]
    call    qword ptr [fnCloseHandle]
    inc     esi
    jmp     @@tcclose
@@tcadone:
    mov     threadCount, 0
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
Thread_CloseAll ENDP

; ==============================================================================
; SYNCHRONIZATION PRIMITIVES - Mutex, Semaphore, Event, CriticalSection
; ==============================================================================

; Sync_CreateMutex - Create unnamed mutex
; ECX = initial owner (0=no, 1=yes)
; Returns: RAX = handle
align 16
Sync_CreateMutex PROC
    sub     rsp, 32
    mov     edx, ecx
    xor     ecx, ecx
    xor     r8d, r8d
    call    qword ptr [fnCreateMutexA]
    add     rsp, 32
    ret
Sync_CreateMutex ENDP

; Sync_ReleaseMutex - Release owned mutex
; RCX = handle
align 16
Sync_ReleaseMutex PROC
    sub     rsp, 32
    call    qword ptr [fnReleaseMutex]
    add     rsp, 32
    ret
Sync_ReleaseMutex ENDP

; Sync_CreateSemaphore - Create semaphore
; ECX = initial count, EDX = max count
; Returns: RAX = handle
align 16
Sync_CreateSemaphore PROC
    sub     rsp, 40
    mov     r8d, edx
    mov     edx, ecx
    xor     ecx, ecx
    xor     r9d, r9d
    call    qword ptr [fnCreateSemaphoreA]
    add     rsp, 40
    ret
Sync_CreateSemaphore ENDP

; Sync_ReleaseSemaphore - Release semaphore
; RCX = handle, EDX = release count
; Returns: EAX = nonzero on success
align 16
Sync_ReleaseSemaphore PROC
    sub     rsp, 32
    xor     r8d, r8d
    call    qword ptr [fnReleaseSemaphore]
    add     rsp, 32
    ret
Sync_ReleaseSemaphore ENDP

; Sync_CreateEvent - Create auto-reset event
; ECX = manual reset (0=auto,1=manual), EDX = initial state
; Returns: RAX = handle
align 16
Sync_CreateEvent PROC
    sub     rsp, 40
    mov     r8d, edx
    mov     edx, ecx
    xor     ecx, ecx
    xor     r9d, r9d
    call    qword ptr [fnCreateEventA]
    add     rsp, 40
    ret
Sync_CreateEvent ENDP

; Sync_Signal - Set event to signaled
; RCX = handle
align 16
Sync_Signal PROC
    sub     rsp, 32
    call    qword ptr [fnSetEvent]
    add     rsp, 32
    ret
Sync_Signal ENDP

; Sync_Reset - Reset event to non-signaled
; RCX = handle
align 16
Sync_Reset PROC
    sub     rsp, 32
    call    qword ptr [fnResetEvent]
    add     rsp, 32
    ret
Sync_Reset ENDP

; Sync_Wait - Wait on any waitable handle with timeout
; RCX = handle, EDX = timeout ms
; Returns: EAX = wait result (0=WAIT_OBJECT_0, 102h=TIMEOUT, etc)
align 16
Sync_Wait PROC
    sub     rsp, 32
    call    qword ptr [fnWaitForSingleObject]
    add     rsp, 32
    ret
Sync_Wait ENDP

; CritSec_Init - Initialize critical section at address
; RCX = ptr to 40-byte critical section buffer
align 16
CritSec_Init PROC
    sub     rsp, 32
    call    qword ptr [fnInitializeCriticalSection]
    add     rsp, 32
    ret
CritSec_Init ENDP

; CritSec_Enter - Enter critical section
; RCX = ptr to critical section
align 16
CritSec_Enter PROC
    sub     rsp, 32
    call    qword ptr [fnEnterCriticalSection]
    add     rsp, 32
    ret
CritSec_Enter ENDP

; CritSec_Leave - Leave critical section
; RCX = ptr to critical section
align 16
CritSec_Leave PROC
    sub     rsp, 32
    call    qword ptr [fnLeaveCriticalSection]
    add     rsp, 32
    ret
CritSec_Leave ENDP

; CritSec_Delete - Destroy critical section
; RCX = ptr to critical section
align 16
CritSec_Delete PROC
    sub     rsp, 32
    call    qword ptr [fnDeleteCriticalSection]
    add     rsp, 32
    ret
CritSec_Delete ENDP

; ==============================================================================
; PROCESS MANAGEMENT - Open, read/write memory, terminate, IDs
; ==============================================================================

; Process_GetCurrentPid - Get current process ID
; Returns: EAX = PID
align 16
Process_GetCurrentPid PROC
    sub     rsp, 32
    call    qword ptr [fnGetCurrentProcessId]
    add     rsp, 32
    ret
Process_GetCurrentPid ENDP

; Process_GetCurrentTid - Get current thread ID
; Returns: EAX = TID
align 16
Process_GetCurrentTid PROC
    sub     rsp, 32
    call    qword ptr [fnGetCurrentThreadId]
    add     rsp, 32
    ret
Process_GetCurrentTid ENDP

; Process_Open - Open process by PID
; ECX = desired access, EDX = inherit handles (0/1), R8D = PID
; Returns: RAX = process handle, 0 on failure
align 16
Process_Open PROC
    sub     rsp, 32
    call    qword ptr [fnOpenProcess]
    add     rsp, 32
    ret
Process_Open ENDP

; Process_Terminate - Terminate process
; RCX = handle, EDX = exit code
align 16
Process_Terminate PROC
    sub     rsp, 32
    call    qword ptr [fnTerminateProcess]
    add     rsp, 32
    ret
Process_Terminate ENDP

; Process_ReadMemory - Read another process memory
; RCX = hProcess, RDX = base address, R8 = buffer, R9 = size
; [rsp+32] = ptr to bytes read (or 0)
; Returns: EAX = nonzero on success
align 16
Process_ReadMemory PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnReadProcessMemory]
    leave
    ret
Process_ReadMemory ENDP

; Process_WriteMemory - Write another process memory
; RCX = hProcess, RDX = base address, R8 = buffer, R9 = size
; Returns: EAX = nonzero on success
align 16
Process_WriteMemory PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    mov     qword ptr [rsp+32], 0
    call    qword ptr [fnWriteProcessMemory]
    leave
    ret
Process_WriteMemory ENDP

; ==============================================================================
; TIMING - Sleep, tick count, high-resolution timer
; ==============================================================================

; Timer_Sleep - Sleep for N milliseconds
; ECX = milliseconds
align 16
Timer_Sleep PROC
    sub     rsp, 32
    call    qword ptr [fnSleep]
    add     rsp, 32
    ret
Timer_Sleep ENDP

; Timer_GetTick64 - Get ms since boot
; Returns: RAX = tick count (64-bit)
align 16
Timer_GetTick64 PROC
    sub     rsp, 32
    call    qword ptr [fnGetTickCount64]
    add     rsp, 32
    ret
Timer_GetTick64 ENDP

; Timer_QueryCounter - Read high-res counter
; Returns: RAX = counter value
align 16
Timer_QueryCounter PROC
    sub     rsp, 32
    lea     rcx, perfCounterBuf
    call    qword ptr [fnQueryPerformanceCounter]
    mov     rax, perfCounterBuf
    add     rsp, 32
    ret
Timer_QueryCounter ENDP

; Timer_QueryFrequency - Read counter frequency
; Returns: RAX = frequency (counts per second)
align 16
Timer_QueryFrequency PROC
    sub     rsp, 32
    lea     rcx, perfFrequencyBuf
    call    qword ptr [fnQueryPerformanceFrequency]
    mov     rax, perfFrequencyBuf
    add     rsp, 32
    ret
Timer_QueryFrequency ENDP

; Timer_ElapsedUs - Calculate microseconds between two counter values
; RCX = start counter, RDX = end counter
; Returns: RAX = microseconds elapsed
align 16
Timer_ElapsedUs PROC
    push    rbx
    sub     rsp, 32
    sub     rdx, rcx
    mov     rbx, rdx
    call    Timer_QueryFrequency
    test    rax, rax
    jz      @@teuzero
    imul    rbx, 1000000
    xor     edx, edx
    mov     rcx, rax
    mov     rax, rbx
    div     rcx
    add     rsp, 32
    pop     rbx
    ret
@@teuzero:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
Timer_ElapsedUs ENDP

; ==============================================================================
; HEAP MANAGEMENT - Private heap for fine-grained alloc/free
; ==============================================================================

; Heap_Init - Create a private heap
; Returns: RAX = heap handle
align 16
Heap_Init PROC
    sub     rsp, 32
    xor     ecx, ecx
    xor     edx, edx
    xor     r8d, r8d
    call    qword ptr [fnHeapCreate]
    mov     hPrivateHeap, rax
    add     rsp, 32
    ret
Heap_Init ENDP

; Heap_Alloc - Allocate from private heap
; RCX = size in bytes
; Returns: RAX = pointer, 0 on failure
align 16
Heap_Alloc PROC
    sub     rsp, 32
    mov     r8, rcx
    mov     rcx, hPrivateHeap
    mov     edx, 8
    call    qword ptr [fnHeapAlloc]
    add     rsp, 32
    ret
Heap_Alloc ENDP

; Heap_Free - Free heap block
; RCX = pointer to free
; Returns: EAX = nonzero on success
align 16
Heap_Free PROC
    sub     rsp, 32
    mov     r8, rcx
    mov     rcx, hPrivateHeap
    xor     edx, edx
    call    qword ptr [fnHeapFree]
    add     rsp, 32
    ret
Heap_Free ENDP

; Heap_Destroy - Destroy private heap
align 16
Heap_Destroy PROC
    sub     rsp, 32
    mov     rcx, hPrivateHeap
    call    qword ptr [fnHeapDestroy]
    mov     hPrivateHeap, 0
    add     rsp, 32
    ret
Heap_Destroy ENDP

; ==============================================================================
; DATA STRUCTURES - Linked List (doubly-linked), Stack (LIFO), Queue (FIFO ring)
; ==============================================================================

; --- LINKED LIST ---
; Node layout: [pNext:8][pPrev:8][dataSize:8][data:N]
; Uses private heap for allocation

; LL_Init - Initialize linked list state
align 16
LL_Init PROC
    mov     llFreeHead, 0
    mov     llNodeCount, 0
    mov     llNodeSize, 0
    ret
LL_Init ENDP

; LL_AllocNode - Allocate a list node
; RCX = data size in bytes
; Returns: RAX = node ptr (or 0), data area at [RAX+24]
align 16
LL_AllocNode PROC
    push    rbx
    sub     rsp, 32
    mov     rbx, rcx
    add     rcx, 24
    call    Heap_Alloc
    test    rax, rax
    jz      @@lladone
    mov     qword ptr [rax], 0
    mov     qword ptr [rax+8], 0
    mov     [rax+16], rbx
    inc     llNodeCount
@@lladone:
    add     rsp, 32
    pop     rbx
    ret
LL_AllocNode ENDP

; LL_FreeNode - Free a list node
; RCX = node ptr
align 16
LL_FreeNode PROC
    sub     rsp, 32
    test    rcx, rcx
    jz      @@llfdone
    call    Heap_Free
    cmp     llNodeCount, 0
    je      @@llfdone
    dec     llNodeCount
@@llfdone:
    add     rsp, 32
    ret
LL_FreeNode ENDP

; LL_InsertAfter - Insert newNode after refNode
; RCX = refNode, RDX = newNode
align 16
LL_InsertAfter PROC
    test    rcx, rcx
    jz      @@lliadone
    test    rdx, rdx
    jz      @@lliadone
    mov     rax, [rcx]
    mov     [rdx], rax
    mov     [rdx+8], rcx
    mov     [rcx], rdx
    test    rax, rax
    jz      @@lliadone
    mov     [rax+8], rdx
@@lliadone:
    ret
LL_InsertAfter ENDP

; LL_InsertBefore - Insert newNode before refNode
; RCX = refNode, RDX = newNode
align 16
LL_InsertBefore PROC
    test    rcx, rcx
    jz      @@llibdone
    test    rdx, rdx
    jz      @@llibdone
    mov     rax, [rcx+8]
    mov     [rdx+8], rax
    mov     [rdx], rcx
    mov     [rcx+8], rdx
    test    rax, rax
    jz      @@llibdone
    mov     [rax], rdx
@@llibdone:
    ret
LL_InsertBefore ENDP

; LL_Remove - Unlink node from list
; RCX = node to remove
; Returns: RAX = node (caller must free)
align 16
LL_Remove PROC
    test    rcx, rcx
    jz      @@llrdone
    mov     rax, [rcx]
    mov     rdx, [rcx+8]
    test    rdx, rdx
    jz      @@llrnoprev
    mov     [rdx], rax
@@llrnoprev:
    test    rax, rax
    jz      @@llrnonext
    mov     [rax+8], rdx
@@llrnonext:
    mov     qword ptr [rcx], 0
    mov     qword ptr [rcx+8], 0
    mov     rax, rcx
@@llrdone:
    ret
LL_Remove ENDP

; LL_FindByIndex - Walk list from head and find Nth node
; RCX = head node, RDX = index (0-based)
; Returns: RAX = node ptr, 0 if out of range
align 16
LL_FindByIndex PROC
    test    rcx, rcx
    jz      @@llfnull
    test    rdx, rdx
    jz      @@llfhit
@@llfwalk:
    mov     rcx, [rcx]
    test    rcx, rcx
    jz      @@llfnull
    dec     rdx
    jnz     @@llfwalk
@@llfhit:
    mov     rax, rcx
    ret
@@llfnull:
    xor     eax, eax
    ret
LL_FindByIndex ENDP

; --- STACK (LIFO) ---
; Array-based, 4096 QWORD entries max

; Stack_Init - Allocate stack arena
; Returns: EAX = 0 success
align 16
Stack_Init PROC
    sub     rsp, 32
    mov     r8, 8000h
    call    MemAlloc
    test    rax, rax
    jz      @@sfail
    mov     stackArena, rax
    mov     stackTop, 0
    mov     stackCapacity, 1000h
    xor     eax, eax
    add     rsp, 32
    ret
@@sfail:
    mov     eax, 1
    add     rsp, 32
    ret
Stack_Init ENDP

; Stack_Push - Push QWORD value
; RCX = value
; Returns: EAX = 0 success, 1 overflow
align 16
Stack_Push PROC
    mov     rax, stackTop
    cmp     rax, stackCapacity
    jae     @@spov
    mov     rdx, stackArena
    mov     [rdx+rax*8], rcx
    inc     stackTop
    xor     eax, eax
    ret
@@spov:
    mov     eax, 1
    ret
Stack_Push ENDP

; Stack_Pop - Pop QWORD value
; Returns: RAX = value, RDX = 0 success / 1 underflow
align 16
Stack_Pop PROC
    mov     rax, stackTop
    test    rax, rax
    jz      @@spuf
    dec     rax
    mov     stackTop, rax
    mov     rdx, stackArena
    mov     rax, [rdx+rax*8]
    xor     edx, edx
    ret
@@spuf:
    xor     eax, eax
    mov     edx, 1
    ret
Stack_Pop ENDP

; Stack_Peek - Read top without popping
; Returns: RAX = value, RDX = 0 success / 1 empty
align 16
Stack_Peek PROC
    mov     rax, stackTop
    test    rax, rax
    jz      @@skuf
    dec     rax
    mov     rdx, stackArena
    mov     rax, [rdx+rax*8]
    xor     edx, edx
    ret
@@skuf:
    xor     eax, eax
    mov     edx, 1
    ret
Stack_Peek ENDP

; Stack_IsEmpty - Check if stack is empty
; Returns: EAX = 1 if empty, 0 if not
align 16
Stack_IsEmpty PROC
    cmp     stackTop, 0
    sete    al
    movzx   eax, al
    ret
Stack_IsEmpty ENDP

; Stack_Count - Return number of entries
; Returns: RAX = count
align 16
Stack_Count PROC
    mov     rax, stackTop
    ret
Stack_Count ENDP

; Stack_Clear - Reset stack to empty
align 16
Stack_Clear PROC
    mov     stackTop, 0
    ret
Stack_Clear ENDP

; --- QUEUE (FIFO Ring Buffer) ---
; Array-based, 4096 QWORD entries max

; Queue_Init - Allocate queue arena
; Returns: EAX = 0 success
align 16
Queue_Init PROC
    sub     rsp, 32
    mov     r8, 8000h
    call    MemAlloc
    test    rax, rax
    jz      @@qifail
    mov     queueArena, rax
    mov     queueHead, 0
    mov     queueTail, 0
    mov     queueCapacity, 1000h
    mov     queueCount, 0
    xor     eax, eax
    add     rsp, 32
    ret
@@qifail:
    mov     eax, 1
    add     rsp, 32
    ret
Queue_Init ENDP

; Queue_Enqueue - Add QWORD to tail
; RCX = value
; Returns: EAX = 0 success, 1 full
align 16
Queue_Enqueue PROC
    mov     rax, queueCount
    cmp     rax, queueCapacity
    jae     @@qefull
    mov     rdx, queueArena
    mov     rax, queueTail
    mov     [rdx+rax*8], rcx
    inc     rax
    cmp     rax, queueCapacity
    jb      @@qenwrap
    xor     eax, eax
@@qenwrap:
    mov     queueTail, rax
    inc     queueCount
    xor     eax, eax
    ret
@@qefull:
    mov     eax, 1
    ret
Queue_Enqueue ENDP

; Queue_Dequeue - Remove QWORD from head
; Returns: RAX = value, RDX = 0 success / 1 empty
align 16
Queue_Dequeue PROC
    cmp     queueCount, 0
    je      @@qdempty
    mov     rdx, queueArena
    mov     rax, queueHead
    mov     rax, [rdx+rax*8]
    push    rax
    mov     rax, queueHead
    inc     rax
    cmp     rax, queueCapacity
    jb      @@qdnwrap
    xor     eax, eax
@@qdnwrap:
    mov     queueHead, rax
    dec     queueCount
    pop     rax
    xor     edx, edx
    ret
@@qdempty:
    xor     eax, eax
    mov     edx, 1
    ret
Queue_Dequeue ENDP

; Queue_Peek - Read head without dequeue
; Returns: RAX = value, RDX = 0 success / 1 empty
align 16
Queue_Peek PROC
    cmp     queueCount, 0
    je      @@qpempty
    mov     rdx, queueArena
    mov     rax, queueHead
    mov     rax, [rdx+rax*8]
    xor     edx, edx
    ret
@@qpempty:
    xor     eax, eax
    mov     edx, 1
    ret
Queue_Peek ENDP

; Queue_IsEmpty - Returns EAX = 1 if empty
align 16
Queue_IsEmpty PROC
    cmp     queueCount, 0
    sete    al
    movzx   eax, al
    ret
Queue_IsEmpty ENDP

; Queue_Count - Return item count
; Returns: RAX = count
align 16
Queue_Count PROC
    mov     rax, queueCount
    ret
Queue_Count ENDP

; Queue_Clear - Reset queue
align 16
Queue_Clear PROC
    mov     queueHead, 0
    mov     queueTail, 0
    mov     queueCount, 0
    ret
Queue_Clear ENDP

; ==============================================================================
; FNV-1a 64-BIT HASH
; RCX = data ptr, RDX = length
; Returns: RAX = 64-bit hash
; ==============================================================================
align 16
Hash_FNV1a_64 PROC
    mov     rax, 0CBF29CE484222325h
    mov     r8, 00100000001B3h
    test    rdx, rdx
    jz      @@hfdone
@@hfloop:
    movzx   r9d, byte ptr [rcx]
    xor     rax, r9
    imul    rax, r8
    inc     rcx
    dec     rdx
    jnz     @@hfloop
@@hfdone:
    ret
Hash_FNV1a_64 ENDP

; ==============================================================================
; OPTIONAL HEADER 64 EMITTER (240 bytes at e_lfanew+24)
; RCX = dest buffer
; RDX = ImageBase (default 140000000h)
; R8  = SizeOfImage
; R9  = SizeOfHeaders (aligned)
; Returns: RAX = 240 (bytes written)
; ==============================================================================
align 16
Emit_OptionalHeader64 PROC
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     rcx, rdi
    mov     rdx, 240
    call    MemZero
    mov     word ptr [rdi+0], 020Bh
    mov     byte ptr [rdi+2], 14
    mov     byte ptr [rdi+3], 0
    mov     dword ptr [rdi+16], 1000h
    mov     dword ptr [rdi+20], 200h
    mov     dword ptr [rdi+24], 200h
    mov     qword ptr [rdi+24], rdx
    mov     dword ptr [rdi+32], 1000h
    mov     dword ptr [rdi+56], r8d
    mov     dword ptr [rdi+60], r9d
    mov     word ptr [rdi+64], 6
    mov     word ptr [rdi+66], 0
    mov     word ptr [rdi+68], 6
    mov     word ptr [rdi+70], 0
    mov     dword ptr [rdi+72], 0
    mov     dword ptr [rdi+80], 100000h
    mov     dword ptr [rdi+84], 1000h
    mov     dword ptr [rdi+88], 100000h
    mov     dword ptr [rdi+92], 1000h
    mov     word ptr [rdi+108], 2
    mov     dword ptr [rdi+116], 16
    mov     rax, 240
    pop     rsi
    pop     rdi
    ret
Emit_OptionalHeader64 ENDP

; ==============================================================================
; SECTION HEADER EMITTER (40 bytes per section)
; RCX = dest buffer
; RDX = section name ptr (8 bytes max)
; R8D = VirtualSize
; R9D = VirtualAddress
; [rsp+32] = SizeOfRawData
; [rsp+40] = PointerToRawData
; [rsp+48] = Characteristics
; Returns: RAX = 40
; ==============================================================================
align 16
Emit_SectionHeader PROC
    push    rbp
    mov     rbp, rsp
    push    rdi
    push    rsi
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rcx, rdi
    mov     rdx, 40
    call    MemZero
    ; Copy name (up to 8 bytes)
    xor     ecx, ecx
@@eshname:
    cmp     ecx, 8
    jae     @@eshnext
    mov     al, [rsi+rcx]
    mov     [rdi+rcx], al
    test    al, al
    jz      @@eshnext
    inc     ecx
    jmp     @@eshname
@@eshnext:
    mov     [rdi+8], r8d
    mov     [rdi+12], r9d
    mov     eax, [rbp+48]
    mov     [rdi+16], eax
    mov     eax, [rbp+56]
    mov     [rdi+20], eax
    mov     eax, [rbp+64]
    mov     [rdi+36], eax
    mov     rax, 40
    pop     rsi
    pop     rdi
    pop     rbp
    ret
Emit_SectionHeader ENDP

END
