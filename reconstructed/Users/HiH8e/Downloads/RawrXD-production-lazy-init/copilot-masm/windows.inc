; Minimal windows.inc stub for assembly samples
; NOTE: This is a small stub to allow assembling the example modules in this repo.
; For production builds, replace with a full SDK-provided include set (MASM/UASM headers).

; Simple INVOKE macro - expands to a call (arguments must be placed in registers by caller)
INVOKE MACRO proc:REQ
    call proc
ENDM

; Common constants
WM_USER          EQU 0400h
EM_SETSEL        EQU 00B1h
EM_REPLACESEL    EQU 00C2h
EM_SETCHARFORMAT EQU 0444h
SCF_SELECTION    EQU 0x0001

WS_POPUP         EQU 80000000h
WS_BORDER        EQU 00800000h
WS_VISIBLE       EQU 10000000h
WS_CHILD         EQU 40000000h
WS_VSCROLL       EQU 00200000h
WS_CAPTION       EQU 00C00000h
WS_SYSMENU       EQU 00080000h

WS_EX_CLIENTEDGE EQU 00000200h
WS_EX_TOOLWINDOW EQU 00000080h

ES_MULTILINE     EQU 0004h
ES_READONLY      EQU 0800h
ES_AUTOHSCROLL   EQU 0080h

LBS_NOTIFY       EQU 0001h
BS_PUSHBUTTON    EQU 0h

MB_OK            EQU 0h

; Font flags
DEFAULT_CHARSET       EQU 1
OUT_DEFAULT_PRECIS    EQU 0
CLIP_DEFAULT_PRECIS   EQU 0
DEFAULT_QUALITY       EQU 0
FIXED_PITCH           EQU 10h
FF_MODERN             EQU 30h
FW_NORMAL             EQU 400

HEAP_ZERO_MEMORY      EQU 00000008h

; IDC placeholders
IDC_CMDK_SEARCH       EQU 1001
IDC_CMDK_LIST         EQU 1002
IDC_DIFF_OLD          EQU 2001
IDC_DIFF_NEW          EQU 2002
IDC_DIFF_ACCEPT       EQU 2003
IDC_DIFF_REJECT       EQU 2004
IDC_CMDK_SEARCH       EQU 1001
IDC_CMDK_LIST         EQU 1002
IDC_THINKING_BOX      EQU 3001

; EXTERN declarations for WinAPI & our helpers (minimal set used by the samples)
EXTERN WinHttpOpen:PROC
EXTERN WinHttpSetTimeouts:PROC
EXTERN WinHttpConnect:PROC
EXTERN WinHttpOpenRequest:PROC
EXTERN WinHttpAddRequestHeaders:PROC
EXTERN WinHttpSendRequest:PROC
EXTERN WinHttpReadData:PROC
EXTERN WinHttpCloseHandle:PROC

EXTERN BCryptOpenAlgorithmProvider:PROC
EXTERN BCryptCloseAlgorithmProvider:PROC
EXTERN BCryptImportKeyPair:PROC
EXTERN BCryptCreateHash:PROC
EXTERN BCryptHashData:PROC
EXTERN BCryptFinishHash:PROC
EXTERN BCryptVerifySignature:PROC

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN RtlCopyMemory:PROC

EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC
EXTERN wsprintfA:PROC
EXTERN lstrcmpA:PROC

EXTERN CreateWindowExA:PROC
EXTERN CreateFontA:PROC
EXTERN SendMessageA:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN GetWindowTextLengthA:PROC
EXTERN GetDlgItem:PROC
EXTERN SetFocus:PROC
EXTERN DestroyWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN GetClientRect:PROC
EXTERN DrawTypingDots:PROC
EXTERN PostMessageA:PROC
EXTERN MessageBoxA:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSizeEx:PROC
EXTERN GetDesktopWindow:PROC
EXTERN SetWindowPos:PROC
EXTERN GetWindowRect:PROC
EXTERN GetClientRect:PROC
;
; EXTERN declarations for helper modules expected in later batches
EXTERN JsonExtractString:PROC
EXTERN JsonExtractObject:PROC
EXTERN JsonParseArray:PROC
EXTERN JsonExtractBool:PROC
EXTERN Vector_Push:PROC
EXTERN ToolRegistry_Find:PROC
EXTERN Diff_ComputeLines:PROC
EXTERN DrawTypingDots:PROC
EXTERN String_FuzzyMatch:PROC
EXTERN GetSelectedText:PROC
EXTERN GetCurrentFileContent:PROC
EXTERN Plan_GetStep:PROC

; Common macros defaults
PROTOMACROPROTO MACRO
ENDM

; Basic RECT struct
RECT STRUCT
    left DWORD ?
    top DWORD ?
    right DWORD ?
    bottom DWORD ?
RECT ENDS

; End of stub
