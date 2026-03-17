IFNDEF WINDOWS_INC
WINDOWS_INC EQU 1

; Types
HWND        typedef QWORD
HINSTANCE   typedef QWORD
HMODULE     typedef QWORD
WPARAM      typedef QWORD
LPARAM      typedef QWORD
UINT        typedef DWORD
HANDLE      typedef QWORD
LPVOID      typedef QWORD
LPCSTR      typedef QWORD

; File operation constants
INVALID_HANDLE_VALUE EQU -1
GENERIC_READ         EQU 80000000h
GENERIC_WRITE        EQU 40000000h
FILE_SHARE_READ      EQU 00000001h
FILE_SHARE_WRITE     EQU 00000002h
OPEN_EXISTING        EQU 3
CREATE_ALWAYS        EQU 2
FILE_ATTRIBUTE_NORMAL EQU 80h

; Common constants
WM_CREATE        EQU 0001h
WM_DESTROY       EQU 0002h
WM_SIZE          EQU 0005h
WM_SETTEXT       EQU 000Ch
WM_GETTEXT       EQU 000Dh
WM_GETTEXTLENGTH EQU 000Eh
WM_COMMAND       EQU 0111h
WM_USER          EQU 0400h
EM_SETSEL        EQU 00B1h
EM_LINESCROLL    EQU 00B6h
EM_REPLACESEL    EQU 00C2h
EM_SETCHARFORMAT EQU 0444h
EM_GETTEXTRANGE  EQU 044Bh
EM_FINDTEXT      EQU 0438h
SCF_SELECTION    EQU 0001h
SCF_ALL          EQU 0004h

FINDTEXTA STRUCT
    cpMin           DWORD ?
    cpMax           DWORD ?
    lpstrText       QWORD ?
FINDTEXTA ENDS

CFM_COLOR        EQU 40000000h
CFE_AUTOCOLOR    EQU 40000000h

CHARFORMAT2 STRUCT
    cbSize          DWORD ?
    dwMask          DWORD ?
    dwEffects       DWORD ?
    yHeight         DWORD ?
    yOffset         DWORD ?
    crTextColor     DWORD ?
    bCharSet        BYTE ?
    bPitchAndFamily BYTE ?
    szFaceName      BYTE 32 DUP (?)
    wWeight         WORD ?
    sSpacing        WORD ?
    crBackColor     DWORD ?
    lcid            DWORD ?
    dwReserved      DWORD ?
    sStyle          WORD ?
    wKerning        WORD ?
    bUnderlineType  BYTE ?
    bAnimation      BYTE ?
    bRevAuthor      BYTE ?
    bReserved1      BYTE ?
CHARFORMAT2 ENDS

TEXTRANGE STRUCT
    cpMin           DWORD ?
    cpMax           DWORD ?
    lpstrText       QWORD ?
TEXTRANGE ENDS

WS_OVERLAPPEDWINDOW EQU 00CF0000h
WS_POPUP         EQU 80000000h
WS_BORDER        EQU 00800000h
WS_VISIBLE       EQU 10000000h
WS_CHILD         EQU 40000000h
WS_VSCROLL       EQU 00200000h
WS_CAPTION       EQU 00C00000h
WS_SYSMENU       EQU 00080000h

WS_EX_CLIENTEDGE EQU 00000200h
WS_EX_TOOLWINDOW EQU 00000080h

SW_SHOW          EQU 5
PM_REMOVE        EQU 1
WM_INITDIALOG    EQU 0110h
WM_CLOSE         EQU 0010h
WS_CLIPSIBLINGS  EQU 04000000h
TCIF_TEXT        EQU 0001h
TCIF_IMAGE       EQU 0002h
TCIF_PARAM       EQU 0008h
TCIF_RTLREADING  EQU 0004h
TCIF_STATE       EQU 0010h

LVCF_FMT         EQU 0001h
LVCF_WIDTH       EQU 0002h
LVCF_TEXT        EQU 0004h
LVCF_SUBITEM     EQU 0008h

LVIF_TEXT        EQU 0001h
LVIF_IMAGE       EQU 0002h
LVIF_PARAM       EQU 0004h
LVIF_STATE       EQU 0008h

LVS_REPORT       EQU 0001h
LVNI_SELECTED    EQU 0002h
LVIS_SELECTED    EQU 0002h

TCITEM STRUCT
    dwMask              DWORD   ?
    dwState             DWORD   ?
    dwStateMask         DWORD   ?
    pszText             QWORD   ?
    cchTextMax          DWORD   ?
    iImage              DWORD   ?
    lParam              QWORD   ?
TCITEM ENDS

LVCOLUMN STRUCT
    dwMask              DWORD   ?
    fmt                 DWORD   ?
    cx_                 DWORD   ?
    pszText             QWORD   ?
    cchTextMax          DWORD   ?
    iSubItem            DWORD   ?
    iImage              DWORD   ?
    iOrder              DWORD   ?
    _padding            DWORD   ?
LVCOLUMN ENDS

LVITEM STRUCT
    dwMask              DWORD   ?
    iItem               DWORD   ?
    iSubItem            DWORD   ?
    state               DWORD   ?
    stateMask           DWORD   ?
    pszText             QWORD   ?
    cchTextMax          DWORD   ?
    iImage              DWORD   ?
    lParam              QWORD   ?
    iIndent             DWORD   ?
    _padding            DWORD   ?
LVITEM ENDS

MB_OK            EQU 0h
MB_OKCANCEL      EQU 1h
MB_ICONERROR     EQU 10h
IDCANCEL         EQU 2
IDOK             EQU 1

; Menu flags
MFT_STRING       EQU 0h
MF_STRING        EQU 0h
MF_POPUP         EQU 00000010h

; Window Class Styles
CS_VREDRAW       EQU 0001h
CS_HREDRAW       EQU 0002h

; Colors
COLOR_BTNFACE    EQU 15

; Cursors and Icons
IDC_ARROW        EQU 32512
IDI_APPLICATION  EQU 32512

; ShowWindow commands
SW_SHOWNORMAL    EQU 1

; CreateWindow constants
CW_USEDEFAULT    EQU 80000000h

; Font flags
DEFAULT_CHARSET       EQU 1
OUT_DEFAULT_PRECIS    EQU 0
CLIP_DEFAULT_PRECIS   EQU 0
DEFAULT_QUALITY       EQU 0
FIXED_PITCH           EQU 10h
FF_MODERN             EQU 30h
FW_NORMAL             EQU 400

HEAP_ZERO_MEMORY      EQU 00000008h
HANDLE_FLAG_INHERIT   EQU 00000001h
STARTF_USESTDHANDLES  EQU 00000100h
STARTF_USESHOWWINDOW  EQU 00000001h
SW_HIDE               EQU 0
CREATE_NEW_CONSOLE    EQU 00000010h
CREATE_NO_WINDOW      EQU 08000000h
TRUE                  EQU 1
FALSE                 EQU 0
NULL            EQU 0
; Registry constants
HKEY_CURRENT_USER   EQU 80000001h
KEY_READ            EQU 20019h
KEY_WRITE           EQU 20006h
REG_SZ              EQU 1
REG_DWORD           EQU 4


; IDC placeholders
IDC_CMDK_SEARCH       EQU 1001
IDC_CMDK_LIST         EQU 1002

; Memory-map flags
PAGE_READONLY       EQU 02h
FILE_MAP_READ       EQU 04h

; File search
MAX_PATH            EQU 260

WIN32_FIND_DATA STRUCT
    dwFileAttributes    DWORD   ?
    ftCreationTime      QWORD   ?
    ftLastAccessTime    QWORD   ?
    ftLastWriteTime     QWORD   ?
    nFileSizeHigh       DWORD   ?
    nFileSizeLow        DWORD   ?
    dwReserved0         DWORD   ?
    dwReserved1         DWORD   ?
    cFileName           BYTE    260 DUP (?)
    cAlternateFileName  BYTE    14 DUP (?)
WIN32_FIND_DATA ENDS

SECURITY_ATTRIBUTES STRUCT
    nLength             DWORD   ?
    lpSecurityDescriptor QWORD  ?
    bInheritHandle      DWORD   ?
SECURITY_ATTRIBUTES ENDS

STARTUPINFO STRUCT
    cb                  DWORD   ?
    _pad_cb             DWORD   ?
    lpReserved          QWORD   ?
    lpDesktop           QWORD   ?
    lpTitle             QWORD   ?
    dwX                 DWORD   ?
    dwY                 DWORD   ?
    dwXSize             DWORD   ?
    dwYSize             DWORD   ?
    dwXCountChars       DWORD   ?
    dwYCountChars       DWORD   ?
    dwFillAttribute     DWORD   ?
    dwFlags             DWORD   ?
    wShowWindow         WORD    ?
    cbReserved2         WORD    ?
    lpReserved2         QWORD   ?
    hStdInput           QWORD   ?
    hStdOutput          QWORD   ?
    hStdError           QWORD   ?
STARTUPINFO ENDS

PROCESS_INFORMATION STRUCT
    hProcess            QWORD   ?
    hThread             QWORD   ?
    dwProcessId         DWORD   ?
    dwThreadId          DWORD   ?
PROCESS_INFORMATION ENDS

RECT STRUCT
    left                DWORD   ?
    top                 DWORD   ?
    right               DWORD   ?
    bottom              DWORD   ?
RECT ENDS

WNDCLASSEX STRUCT
    cbSize              DWORD   ?
    style               DWORD   ?
    lpfnWndProc         QWORD   ?
    cbClsExtra          DWORD   ?
    cbWndExtra          DWORD   ?
    hInstance           QWORD   ?
    hIcon               QWORD   ?
    hCursor             QWORD   ?
    hbrBackground       QWORD   ?
    lpszMenuName        QWORD   ?
    lpszClassName       QWORD   ?
    hIconSm             QWORD   ?
WNDCLASSEX ENDS

MSG STRUCT
    hwnd                QWORD   ?
    message             DWORD   ?
    _padding            DWORD   ?
    wParam              QWORD   ?
    lParam              QWORD   ?
    time                DWORD   ?
    pt                  QWORD   ? ; POINT STRUCT is 2 DWORD ?S
    _padding2           DWORD   ?
MSG ENDS

; Prototypes
EXTERN GetModuleHandleA:PROC
EXTERN GetCommandLineA:PROC
EXTERN ExitProcess:PROC
EXTERN CreateFileA:PROC
EXTERN GetFileSize:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetFileSizeEx:PROC
EXTERN MessageBoxA:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrcmpA:PROC
EXTERN CreateWindowExA:PROC
EXTERN CreateFontA:PROC
EXTERN SendMessageA:PROC
EXTERN SetWindowTextA:PROC
EXTERN GetWindowTextA:PROC
EXTERN GetWindowTextLengthA:PROC
EXTERN OutputDebugStringA:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetDlgItem:PROC
EXTERN SetFocus:PROC
EXTERN DestroyWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN PostQuitMessage:PROC
EXTERN DefWindowProcA:PROC
EXTERN RegisterClassExA:PROC
EXTERN GetClassInfoExA:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN LoadCursorA:PROC
EXTERN LoadIconA:PROC
EXTERN GetClientRect:PROC
EXTERN MoveWindow:PROC
EXTERN CreateMenu:PROC
EXTERN CreatePopupMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN SetMenu:PROC
EXTERN GetProcAddress:PROC
EXTERN GetLastError:PROC
EXTERN GetTickCount:PROC
EXTERN FreeLibrary:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN RtlZeroMemory:PROC
EXTERN VirtualProtect:PROC
EXTERN GetWindowLongPtrA:PROC
EXTERN GetClassNameA:PROC
EXTERN GetParent:PROC
EXTERN ScreenToClient:PROC
EXTERN EnumChildWindows:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN PostMessageA:PROC
EXTERN GetDesktopWindow:PROC
EXTERN SetWindowPos:PROC
EXTERN GetWindowRect:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN DuplicateHandle:PROC
EXTERN GetCurrentProcess:PROC
EXTERN CreateProcessA:PROC
EXTERN CreateDIBSection:PROC
EXTERN SelectObject:PROC
EXTERN BitBlt:PROC
EXTERN DwmExtendFrameIntoClientArea:PROC
EXTERN BeginBufferedPaint:PROC
EXTERN EndBufferedPaint:PROC
EXTERN D2D1CreateFactory:PROC
EXTERN DWriteCreateFactory:PROC
EXTERN EnableWindow:PROC

; Button Styles
BS_PUSHBUTTON    EQU 00000000h
BS_DEFPUSHBUTTON EQU 00000001h
BS_CHECKBOX      EQU 00000002h
BS_AUTOCHECKBOX  EQU 00000003h
BS_RADIOBUTTON   EQU 00000004h
BS_3STATE        EQU 00000005h
BS_AUTO3STATE    EQU 00000006h
BS_GROUPBOX      EQU 00000007h
BS_USERBUTTON    EQU 00000008h
BS_AUTORADIOBUTTON EQU 00000009h

; Edit Control Styles
ES_MULTILINE     EQU 0004h
ES_AUTOVSCROLL   EQU 0040h
ES_AUTOHSCROLL   EQU 0080h
ES_NOHIDESEL     EQU 0100h
ES_READONLY      EQU 0800h
ES_WANTRETURN    EQU 1000h

; Clipboard Messages
WM_CUT           EQU 0300h
WM_COPY          EQU 0301h
WM_PASTE         EQU 0302h
WM_CLEAR         EQU 0303h
WM_UNDO          EQU 0304h

ENDIF






