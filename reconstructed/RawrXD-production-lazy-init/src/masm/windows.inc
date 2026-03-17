IFNDEF WINDOWS_INC
WINDOWS_INC EQU 1

; =============================================================================
; MASM Windows Include File - Win64 API Definitions
; Zero-dependency, comprehensive Win32 API definitions for MASM64
; =============================================================================

; ------------------------------------------------------------------
; Basic Windows Types (safe typedefs, avoid redefining built-ins)
; ------------------------------------------------------------------
; INT intentionally omitted to avoid clash with instruction name
UINT        TYPEDEF DWORD
ULONG       TYPEDEF DWORD
LONG_PTR    TYPEDEF SQWORD
ULONG_PTR   TYPEDEF QWORD
DWORD_PTR   TYPEDEF QWORD
UINT_PTR    TYPEDEF QWORD
WPARAM      TYPEDEF UINT_PTR
LPARAM      TYPEDEF LONG_PTR
LRESULT     TYPEDEF LONG_PTR
COLORREF    TYPEDEF DWORD
SIZE_T      TYPEDEF QWORD
SSIZE_T     TYPEDEF SQWORD

; Handle Types
HANDLE      TYPEDEF QWORD
HWND        TYPEDEF QWORD
HINSTANCE   TYPEDEF QWORD
HMODULE     TYPEDEF QWORD
HFILE       TYPEDEF QWORD
HCURSOR     TYPEDEF QWORD
HICON       TYPEDEF QWORD
HBRUSH      TYPEDEF QWORD
HDC         TYPEDEF QWORD
HMENU       TYPEDEF QWORD
HKEY        TYPEDEF QWORD
HCRYSTAL    TYPEDEF QWORD

; Pointer Types
PVOID       TYPEDEF QWORD
LPVOID      TYPEDEF QWORD
PBYTE       TYPEDEF QWORD
LPBYTE      TYPEDEF QWORD
PWORD       TYPEDEF QWORD
LPWORD      TYPEDEF QWORD
PDWORD      TYPEDEF QWORD
LPDWORD     TYPEDEF QWORD
PCSTR       TYPEDEF QWORD
LPSTR       TYPEDEF QWORD
PSTR        TYPEDEF QWORD
PCWSTR      TYPEDEF QWORD
LPWSTR      TYPEDEF QWORD
PWSTR       TYPEDEF QWORD
LPCSTR      TYPEDEF QWORD
LPCWSTR     TYPEDEF QWORD

; Boolean Types
BOOL        TYPEDEF SDWORD
BOOLEAN     TYPEDEF BYTE

; Constants
MAX_PATH    EQU 260
IFNDEF WM_RENDERFORMAT
WM_RENDERFORMAT EQU 0305h
ENDIF
EM_SETSEL       EQU 00B1h

; ------------------------------------------------------------------
; Constants
; ------------------------------------------------------------------
NULL        EQU 0
TRUE        EQU 1
FALSE       EQU 0

; Standard Values
INVALID_HANDLE_VALUE EQU -1
INVALID_FILE_SIZE    EQU -1
INVALID_SET_FILE_POINTER EQU -1

; ------------------------------------------------------------------
; String Functions (MASM Built-in)
; ------------------------------------------------------------------
lstrlenA PROTO :QWORD
lstrcmpA PROTO :QWORD, :QWORD
lstrcpynA PROTO :QWORD, :QWORD, :SDWORD
lstrcpyA PROTO :QWORD, :QWORD
lstrcatA PROTO :QWORD, :QWORD
lstrcmpiA PROTO :QWORD, :QWORD
CharLowerA PROTO :QWORD

; ------------------------------------------------------------------
; File I/O Functions
; ------------------------------------------------------------------
CreateFileA PROTO :QWORD, :DWORD, :DWORD, :QWORD, :DWORD, :DWORD, :QWORD
ReadFile PROTO :QWORD, :QWORD, :DWORD, :QWORD, :QWORD
WriteFile PROTO :QWORD, :QWORD, :DWORD, :QWORD, :QWORD
CloseHandle PROTO :QWORD
DeleteFileA PROTO :QWORD
GetFileSize PROTO :QWORD, :QWORD
GetFileSizeEx PROTO :QWORD, :QWORD
SetFilePointer PROTO :QWORD, :SDWORD, :QWORD, :DWORD
FindFirstFileA PROTO :QWORD, :QWORD
FindNextFileA PROTO :QWORD, :QWORD
FindClose PROTO :QWORD

; ------------------------------------------------------------------
; Memory Management Functions
; ------------------------------------------------------------------
GlobalAlloc PROTO :DWORD, :QWORD
GlobalFree PROTO :QWORD
GlobalLock PROTO :QWORD
GlobalUnlock PROTO :QWORD
GetProcessHeap PROTO
HeapAlloc PROTO :QWORD, :DWORD, :QWORD
HeapFree PROTO :QWORD, :DWORD, :QWORD
GetSystemInfo PROTO :QWORD
VirtualAlloc PROTO :QWORD, :QWORD, :DWORD, :DWORD
VirtualFree PROTO :QWORD, :QWORD, :DWORD
VirtualProtect PROTO :QWORD, :QWORD, :DWORD, :QWORD

; ------------------------------------------------------------------
; File Mapping Functions
; ------------------------------------------------------------------
CreateFileMappingA PROTO :QWORD, :QWORD, :DWORD, :DWORD, :QWORD, :QWORD
MapViewOfFile PROTO :QWORD, :DWORD, :DWORD, :QWORD, :QWORD
UnmapViewOfFile PROTO :QWORD
OpenFileMappingA PROTO :DWORD, :QWORD, :QWORD

; ------------------------------------------------------------------
; Directory Functions
; ------------------------------------------------------------------
CreateDirectoryA PROTO :QWORD, :QWORD
RemoveDirectoryA PROTO :QWORD
GetCurrentDirectoryA PROTO :DWORD, :QWORD
SetCurrentDirectoryA PROTO :QWORD
GetTempPathA PROTO :DWORD, :QWORD

; ------------------------------------------------------------------
; Process Functions
; ------------------------------------------------------------------
CreateProcessA PROTO :QWORD, :QWORD, :QWORD, :QWORD, :DWORD, :DWORD, :QWORD, :QWORD
GetExitCodeProcess PROTO :QWORD, :QWORD
WaitForSingleObject PROTO :QWORD, :DWORD
TerminateProcess PROTO :QWORD, :DWORD
OpenProcess PROTO :DWORD, :QWORD, :DWORD
GetCurrentProcess PROTO
GetCurrentProcessId PROTO
GetCurrentThreadId PROTO
Sleep PROTO :DWORD

; ------------------------------------------------------------------
; Thread Functions
; ------------------------------------------------------------------
CreateThread PROTO :QWORD, :QWORD, :QWORD, :QWORD, :DWORD, :QWORD
ResumeThread PROTO :QWORD
SuspendThread PROTO :QWORD
ExitThread PROTO :DWORD
SetThreadPriority PROTO :QWORD, :SDWORD
GetThreadPriority PROTO :QWORD
GetCurrentThread PROTO

; ------------------------------------------------------------------
; Synchronization Functions
; ------------------------------------------------------------------
CreateMutexA PROTO :QWORD, :QWORD, :QWORD
ReleaseMutex PROTO :QWORD
CreateEventA PROTO :QWORD, :QWORD, :QWORD, :QWORD
SetEvent PROTO :QWORD
ResetEvent PROTO :QWORD

; ------------------------------------------------------------------
; Window Functions
; ------------------------------------------------------------------
RegisterClassExA PROTO :QWORD
CreateWindowExA PROTO :DWORD, :QWORD, :QWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :QWORD, :QWORD, :QWORD, :QWORD
DefWindowProcA PROTO :QWORD, :UINT, :WPARAM, :LPARAM
GetMessageA PROTO :QWORD, :QWORD, :WPARAM, :WPARAM
TranslateMessage PROTO :QWORD
DispatchMessageA PROTO :QWORD
PostMessageA PROTO :QWORD, :UINT, :WPARAM, :LPARAM
SendMessageA PROTO :QWORD, :UINT, :WPARAM, :LPARAM
ShowWindow PROTO :QWORD, :SDWORD
UpdateWindow PROTO :QWORD
GetWindowTextA PROTO :QWORD, :QWORD, :SDWORD
SetWindowTextA PROTO :QWORD, :QWORD
GetWindowRect PROTO :QWORD, :QWORD
GetClientRect PROTO :QWORD, :QWORD
InvalidateRect PROTO :QWORD, :QWORD, :QWORD
ValidateRect PROTO :QWORD, :QWORD
GetDC PROTO :QWORD
ReleaseDC PROTO :QWORD, :QWORD
BeginPaint PROTO :QWORD, :QWORD
EndPaint PROTO :QWORD, :QWORD
GetMessagePos PROTO
GetWindowLongA PROTO :QWORD, :SDWORD
SetWindowLongA PROTO :QWORD, :SDWORD, :LONG_PTR
CallWindowProcA PROTO :QWORD, :QWORD, :UINT, :WPARAM, :LPARAM
IsWindow PROTO :QWORD
IsDialogMessageA PROTO :QWORD, :QWORD

; ------------------------------------------------------------------
; Clipboard Functions
; ------------------------------------------------------------------
OpenClipboard PROTO :QWORD
GetClipboardData PROTO :UINT
SetClipboardData PROTO :UINT, :QWORD
EmptyClipboard PROTO
CloseClipboard PROTO

; ------------------------------------------------------------------
; Dialog Functions
; ------------------------------------------------------------------
DialogBoxParamA PROTO :QWORD, :QWORD, :QWORD, :QWORD, :LONG_PTR
GetDlgItem PROTO :QWORD, :SDWORD
GetDlgItemTextA PROTO :QWORD, :SDWORD, :QWORD, :SDWORD
SetDlgItemTextA PROTO :QWORD, :SDWORD, :QWORD
CheckDlgButton PROTO :QWORD, :SDWORD, :DWORD
IsDlgButtonChecked PROTO :QWORD, :SDWORD
MessageBoxA PROTO :QWORD, :QWORD, :QWORD, :UINT

; ------------------------------------------------------------------
; System Functions
; ------------------------------------------------------------------
GetSystemDirectoryA PROTO :QWORD, :DWORD
GetWindowsDirectoryA PROTO :QWORD, :DWORD
GetComputerNameA PROTO :QWORD, :QWORD
GetUserNameA PROTO :QWORD, :QWORD
GetVersion PROTO
GetTickCount PROTO
GetTickCount64 PROTO
GetLogicalDrives PROTO
GetDriveTypeA PROTO :QWORD

; ------------------------------------------------------------------
; Common Dialog Functions
; ------------------------------------------------------------------
GetOpenFileNameA PROTO :QWORD
GetSaveFileNameA PROTO :QWORD

; ------------------------------------------------------------------
; Registry Functions
; ------------------------------------------------------------------
RegOpenKeyExA PROTO :QWORD, :QWORD, :DWORD, :DWORD, :QWORD
RegCloseKey PROTO :QWORD
RegQueryValueExA PROTO :QWORD, :QWORD, :QWORD, :QWORD, :QWORD, :QWORD
RegSetValueExA PROTO :QWORD, :QWORD, :DWORD, :DWORD, :QWORD, :DWORD
RegCreateKeyExA PROTO :QWORD, :QWORD, :QWORD, :QWORD, :DWORD, :DWORD, :QWORD, :QWORD, :QWORD

; ------------------------------------------------------------------
; Shell Functions
; ------------------------------------------------------------------
ShellExecuteA PROTO :QWORD, :QWORD, :QWORD, :QWORD, :QWORD, :SDWORD
SHGetFolderPathA PROTO :QWORD, :SDWORD, :QWORD, :DWORD, :QWORD
DragAcceptFiles PROTO :QWORD, :QWORD
DragQueryFileA PROTO :QWORD, :UINT, :QWORD, :UINT
DragQueryPoint PROTO :QWORD, :QWORD
DragFinish PROTO :QWORD

; ------------------------------------------------------------------
; Console Functions
; ------------------------------------------------------------------
AllocConsole PROTO
FreeConsole PROTO
GetStdHandle PROTO :DWORD
SetStdHandle PROTO :DWORD, :QWORD
WriteConsoleA PROTO :QWORD, :QWORD, :DWORD, :QWORD, :QWORD
ReadConsoleA PROTO :QWORD, :QWORD, :DWORD, :QWORD, :QWORD
ReadConsoleInputA PROTO :QWORD, :QWORD, :DWORD, :QWORD
PeekConsoleInputA PROTO :QWORD, :QWORD, :DWORD, :QWORD
WriteConsoleInputA PROTO :QWORD, :QWORD, :DWORD, :QWORD
FlushConsoleInputBuffer PROTO :QWORD

; ------------------------------------------------------------------
; Drawing Functions
; ------------------------------------------------------------------
BeginPaint PROTO :QWORD, :QWORD
EndPaint PROTO :QWORD, :QWORD
MoveToEx PROTO :QWORD, :SDWORD, :SDWORD, :QWORD
LineTo PROTO :QWORD, :SDWORD, :SDWORD
Rectangle PROTO :QWORD, :SDWORD, :SDWORD, :SDWORD, :SDWORD
Ellipse PROTO :QWORD, :SDWORD, :SDWORD, :SDWORD, :SDWORD
CreateSolidBrush PROTO :COLORREF
GetStockObject PROTO :SDWORD
SelectObject PROTO :QWORD, :QWORD
DeleteObject PROTO :QWORD
CreateFontA PROTO :SDWORD, :SDWORD, :SDWORD, :SDWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :QWORD
TextOutA PROTO :QWORD, :SDWORD, :SDWORD, :QWORD, :SDWORD
DrawTextA PROTO :QWORD, :QWORD, :SDWORD, :QWORD, :UINT
SetTextColor PROTO :QWORD, :COLORREF
SetBkColor PROTO :QWORD, :COLORREF
SetBkMode PROTO :QWORD, :SDWORD
LoadIconA PROTO :QWORD, :QWORD
LoadCursorA PROTO :QWORD, :QWORD
DestroyIcon PROTO :QWORD
DestroyCursor PROTO :QWORD

; ------------------------------------------------------------------
; Time Functions
; ------------------------------------------------------------------
GetLocalTime PROTO :QWORD
GetSystemTime PROTO :QWORD
FileTimeToSystemTime PROTO :QWORD, :QWORD
SystemTimeToFileTime PROTO :QWORD, :QWORD
GetFileTime PROTO :QWORD, :QWORD, :QWORD, :QWORD
SetFileTime PROTO :QWORD, :QWORD, :QWORD, :QWORD

; ------------------------------------------------------------------
; Module Functions
; ------------------------------------------------------------------
GetModuleHandleA PROTO :QWORD
GetModuleFileNameA PROTO :QWORD, :QWORD, :DWORD
LoadLibraryA PROTO :QWORD
FreeLibrary PROTO :QWORD
GetProcAddress PROTO :QWORD, :QWORD

; ------------------------------------------------------------------
; Environment Functions
; ------------------------------------------------------------------
GetEnvironmentVariableA PROTO :QWORD, :QWORD, :DWORD
SetEnvironmentVariableA PROTO :QWORD, :QWORD
ExpandEnvironmentStringsA PROTO :QWORD, :QWORD, :DWORD

; ------------------------------------------------------------------
; Error Functions
; ------------------------------------------------------------------
GetLastError PROTO
SetLastError PROTO :DWORD
FormatMessageA PROTO :DWORD, :QWORD, :DWORD, :DWORD, :QWORD, :DWORD, :QWORD
GetLastErrorMessage PROTO :QWORD, :DWORD, :QWORD

; ------------------------------------------------------------------
; Debug Functions
; ------------------------------------------------------------------
OutputDebugStringA PROTO :QWORD
DebugBreak PROTO
IsDebuggerPresent PROTO
CheckRemoteDebuggerPresent PROTO :QWORD, :QWORD

; ------------------------------------------------------------------
; Utility Functions
; ------------------------------------------------------------------
GetCommandLineA PROTO
GetCommandLineW PROTO
CommandLineToArgvW PROTO :QWORD, :QWORD
ExpandEnvironmentStringsA PROTO :QWORD, :QWORD, :DWORD
PathFindExtensionA PROTO :QWORD
PathFindFileNameA PROTO :QWORD
PathRemoveExtensionA PROTO :QWORD
PathAppendA PROTO :QWORD, :QWORD
PathIsRelativeA PROTO :QWORD
PathIsDirectoryA PROTO :QWORD

; ------------------------------------------------------------------
; String Functions (Extended)
; ------------------------------------------------------------------
strcat_s PROTO :QWORD, :QWORD, :QWORD
strcpy_s PROTO :QWORD, :QWORD, :QWORD
wcscat_s PROTO :QWORD, :QWORD, :QWORD
wcscpy_s PROTO :QWORD, :QWORD, :QWORD
_wtoi PROTO :QWORD
_wtof PROTO :QWORD
MultiByteToWideChar PROTO :UINT, :DWORD, :QWORD, :SDWORD, :QWORD, :SDWORD
WideCharToMultiByte PROTO :UINT, :QWORD, :SDWORD, :QWORD, :QWORD, :QWORD, :QWORD, :QWORD

; ------------------------------------------------------------------
; Structures
; ------------------------------------------------------------------

; FILETIME Structure
FILETIME STRUCT
    dwLowDateTime  DWORD ?
    dwHighDateTime DWORD ?
FILETIME ENDS

; SYSTEMTIME Structure
SYSTEMTIME STRUCT
    wYear         WORD ?
    wMonth        WORD ?
    wDayOfWeek    WORD ?
    wDay          WORD ?
    wHour         WORD ?
    wMinute       WORD ?
    wSecond       WORD ?
    wMilliseconds WORD ?
SYSTEMTIME ENDS

; STARTUPINFOA Structure
STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle        QWORD ?
    dwX            DWORD ?
    dwY            DWORD ?
    dwXSize        DWORD ?
    dwYSize        DWORD ?
    dwXCountChars  DWORD ?
    dwYCountChars  DWORD ?
    dwFillAttribute DWORD ?
    dwFlags        DWORD ?
    wShowWindow    WORD ?
    cbReserved2    WORD ?
    lpReserved2    QWORD ?
    hStdInput      QWORD ?
    hStdOutput     QWORD ?
    hStdError      QWORD ?
STARTUPINFOA ENDS

; PROCESS_INFORMATION Structure
PROCESS_INFORMATION STRUCT
    hProcess   QWORD ?
    hThread    QWORD ?
    dwProcessId DWORD ?
    dwThreadId DWORD ?
PROCESS_INFORMATION ENDS

; OVERLAPPED Structure
OVERLAPPED STRUCT
    Internal     QWORD ?
    InternalHigh QWORD ?
    OffsetLow    QWORD ?
    OffsetHigh   QWORD ?
    hEvent       QWORD ?
OVERLAPPED ENDS

; WNDCLASSEXA Structure
WNDCLASSEXA STRUCT
    cbSize        DWORD ?
    style         DWORD ?
    lpfnWndProc   QWORD ?
    cbClsExtra    DWORD ?
    cbWndExtra    DWORD ?
    hInstance     QWORD ?
    hIcon         QWORD ?
    hCursor       QWORD ?
    hbrBackground QWORD ?
    lpszMenuName  QWORD ?
    lpszClassName QWORD ?
    hIconSm       QWORD ?
WNDCLASSEXA ENDS

; POINT Structure
POINT STRUCT
    x SDWORD ?
    y SDWORD ?
POINT ENDS

; MSG Structure
MSG STRUCT
    hwnd      QWORD ?
    message   UINT ?
    wParam    WPARAM ?
    lParam    LPARAM ?
    time      DWORD ?
    pt        POINT <>
MSG ENDS

; RECT Structure
RECT STRUCT
    left   SDWORD ?
    top    SDWORD ?
    right  SDWORD ?
    bottom SDWORD ?
RECT ENDS

; POINT Structure
POINT STRUCT
    x SDWORD ?
    y SDWORD ?
POINT ENDS

; PAINTSTRUCT Structure
PAINTSTRUCT STRUCT
    hdc         QWORD ?
    fErase      DWORD ?
    rcPaint     RECT <>
    fRestore    DWORD ?
    fIncUpdate  DWORD ?
    rgbReserved BYTE 32 DUP(?)
PAINTSTRUCT ENDS

; SECURITY_ATTRIBUTES Structure
SECURITY_ATTRIBUTES STRUCT
    nLength              DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle      DWORD ?
SECURITY_ATTRIBUTES ENDS

; SYSTEM_INFO Structure
SYSTEM_INFO STRUCT
    dwOemId          DWORD ?
    dwPageSize       DWORD ?
    lpMinimumAddress QWORD ?
    lpMaximumAddress QWORD ?
    dwActiveProcessorMask QWORD ?
    dwNumberOfProcessors DWORD ?
    dwProcessorType  DWORD ?
    dwAllocationGranularity DWORD ?
    dwProcessorLevel WORD ?
    dwProcessorRevision WORD ?
SYSTEM_INFO ENDS

; MEMORY_BASIC_INFORMATION Structure
MEMORY_BASIC_INFORMATION STRUCT
    BaseAddress      QWORD ?
    AllocationBase   QWORD ?
    AllocationProtect DWORD ?
    RegionSize      QWORD ?
    State           DWORD ?
    Protect         DWORD ?
    TypeValue        DWORD ?
MEMORY_BASIC_INFORMATION ENDS

; WIN32_FILE_ATTRIBUTE_DATA Structure
WIN32_FILE_ATTRIBUTE_DATA STRUCT
    dwFileAttributes     DWORD ?
    ftCreationTime      FILETIME <>
    ftLastAccessTime    FILETIME <>
    ftLastWriteTime     FILETIME <>
    nFileSizeHigh       DWORD ?
    nFileSizeLow        DWORD ?
WIN32_FILE_ATTRIBUTE_DATA ENDS

; WIN32_FIND_DATAA Structure
WIN32_FIND_DATAA STRUCT
    dwFileAttributes     DWORD ?
    ftCreationTime      FILETIME <>
    ftLastAccessTime    FILETIME <>
    ftLastWriteTime     FILETIME <>
    nFileSizeHigh       DWORD ?
    nFileSizeLow        DWORD ?
    dwReserved0         DWORD ?
    dwReserved1         DWORD ?
    cFileName           BYTE MAX_PATH DUP(?)
    cAlternateFileName BYTE 14 DUP(?)
WIN32_FIND_DATAA ENDS

; ------------------------------------------------------------------
; Message Box Constants
; ------------------------------------------------------------------
MB_OK               EQU 00000000h
MB_OKCANCEL         EQU 00000001h
MB_ABORTRETRYIGNORE EQU 00000002h
MB_YESNOCANCEL      EQU 00000003h
MB_YESNO            EQU 00000004h
MB_RETRYCANCEL      EQU 00000005h
MB_CANCELTRYCONTINUE EQU 00000006h
MB_ICONHAND         EQU 00000010h
MB_ICONQUESTION     EQU 00000020h
MB_ICONEXCLAMATION EQU 00000030h
MB_ICONASTERISK     EQU 00000040h

IDOK                EQU 1
IDCANCEL            EQU 2
IDABORT             EQU 3
IDRETRY             EQU 4
IDIGNORE            EQU 5
IDYES               EQU 6
IDNO                EQU 7
IDTRYAGAIN          EQU 10
IDCONTINUE          EQU 11

; ------------------------------------------------------------------
; Window Message Constants
; ------------------------------------------------------------------
WM_NULL                         EQU 0000h
WM_CREATE                       EQU 0001h
WM_DESTROY                      EQU 0002h
WM_MOVE                         EQU 0003h
WM_SIZE                         EQU 0005h
WM_ACTIVATE                     EQU 0006h
WM_SETFOCUS                     EQU 0007h
WM_KILLFOCUS                    EQU 0008h
WM_ENABLE                       EQU 000Ah
WM_SETREDRAW                    EQU 000Bh
WM_SETTEXT                      EQU 000Ch
WM_GETTEXT                      EQU 000Dh
WM_GETTEXTLENGTH                EQU 000Eh
WM_PAINT                        EQU 000Fh
WM_CLOSE                        EQU 0010h
WM_QUERYENDSESSION              EQU 0011h
WM_QUIT                         EQU 0012h
WM_QUERYOPEN                    EQU 0013h
WM_ERASEBKGND                   EQU 0014h
WM_SYSCOLORCHANGE               EQU 0015h
WM_ENDSESSION                   EQU 0016h
WM_SHOWWINDOW                   EQU 0018h
WM_WININICHANGE                 EQU 001Ah
WM_DEVMODECHANGE                EQU 001Bh
WM_ACTIVATEAPP                  EQU 001Ch
WM_FONTCHANGE                   EQU 001Dh
WM_TIMECHANGE                   EQU 001Eh
WM_CANCELMODE                   EQU 001Fh
WM_SETCURSOR                    EQU 0020h
WM_MOUSEACTIVATE                EQU 0021h
WM_CHILDACTIVATE                EQU 0022h
WM_QUEUESYNC                    EQU 0023h
WM_GETMINMAXINFO                EQU 0024h
WM_PAINTICON                    EQU 0026h
WM_ICONERASEBKGND               EQU 0027h
WM_NEXTDLGCTL                   EQU 0028h
WM_SPOOLERSTATUS                EQU 002Ah
WM_DRAWITEM                     EQU 002Bh
WM_MEASUREITEM                  EQU 002Ch
WM_DELETEITEM                   EQU 002Dh
WM_VKEYTOITEM                   EQU 002Eh
WM_CHARTOITEM                   EQU 002Fh
WM_SETFONT                      EQU 0030h
WM_GETFONT                      EQU 0031h
WM_SETHOTKEY                    EQU 0032h
WM_GETHOTKEY                    EQU 0033h
WM_QUERYDRAGICON                EQU 003Bh
WM_COMPAREITEM                  EQU 003Dh
WM_GETOBJECT                    EQU 003Dh
WM_COMPACTING                    EQU 0041h
WM_WINDOWPOSCHANGING            EQU 0046h
WM_WINDOWPOSCHANGED             EQU 0047h
WM_POWER                        EQU 0048h
WM_COPYDATA                     EQU 004Ah
WM_CANCELJOURNAL                EQU 004Bh
WM_NOTIFY                       EQU 004Eh
WM_INPUTLANGCHANGEREQUEST       EQU 0050h
WM_INPUTLANGCHANGE              EQU 0051h
WM_TCARD                        EQU 0052h
WM_HELP                         EQU 0053h
WM_USERCHANGED                  EQU 0054h
WM_NOTIFYFORMAT                 EQU 0055h
WM_CONTEXTMENU                  EQU 007Bh
WM_STYLECHANGING                EQU 007Ch
WM_STYLECHANGED                 EQU 007Dh
WM_DISPLAYCHANGE                EQU 007Eh
WM_GETICON                      EQU 007Fh
WM_SETICON                      EQU 0080h
WM_NCCREATE                     EQU 0081h
WM_NCDESTROY                    EQU 0082h
WM_NCCALCSIZE                   EQU 0083h
WM_NCHITTEST                    EQU 0084h
WM_NCPAINT                      EQU 0085h
WM_NCACTIVATE                   EQU 0086h
WM_GETDLGCODE                   EQU 0087h
WM_SYNCPAINT                    EQU 0088h
WM_NCMOUSEMOVE                  EQU 00A0h
WM_NCLBUTTONDOWN               EQU 00A1h
WM_NCLBUTTONUP                 EQU 00A2h
WM_NCLBUTTONDBLCLK             EQU 00A3h
WM_NCRBUTTONDOWN               EQU 00A4h
WM_NCRBUTTONUP                 EQU 00A5h
WM_NCRBUTTONDBLCLK             EQU 00A6h
WM_NCMBUTTONDOWN               EQU 00A7h
WM_NCMBUTTONUP                 EQU 00A8h
WM_NCMBUTTONDBLCLK             EQU 00A9h
WM_KEYDOWN                     EQU 0100h
WM_KEYUP                       EQU 0101h
WM_CHAR                        EQU 0102h
WM_DEADCHAR                    EQU 0103h
WM_SYSKEYDOWN                  EQU 0104h
WM_SYSKEYUP                    EQU 0105h
WM_SYSCHAR                     EQU 0106h
WM_SYSDEADCHAR                 EQU 0107h
WM_KEYLAST                     EQU 0108h
WM_IME_STARTCOMPOSITION        EQU 010Dh
WM_IME_ENDCOMPOSITION          EQU 010Eh
WM_IME_COMPOSITION             EQU 010Fh
WM_INITDIALOG                  EQU 0110h
WM_COMMAND                     EQU 0111h
WM_SYSCOMMAND                  EQU 0112h
WM_TIMER                       EQU 0113h
WM_HSCROLL                     EQU 0114h
WM_VSCROLL                     EQU 0115h
WM_INITMENU                    EQU 0116h
WM_INITMENUPOPUP               EQU 0117h
WM_MENUSELECT                  EQU 011Fh
WM_MENUCHAR                    EQU 0120h
WM_ENTERIDLE                   EQU 0121h
WM_MENURBUTTONUP               EQU 0122h
WM_MENUDRAG                    EQU 0123h
WM_MENUGETOBJECT               EQU 0124h
WM_UNINITMENUPOPUP             EQU 0125h
WM_MENUCOMMAND                 EQU 0126h
WM_CHANGEUISTATE               EQU 0127h
WM_UPDATEUISTATE               EQU 0128h
WM_QUERYUISTATE                EQU 0129h
WM_CTLCOLORMSGBOX              EQU 0132h
WM_CTLCOLOREDIT                EQU 0133h
WM_CTLCOLORLISTBOX             EQU 0134h
WM_CTLCOLORBTN                 EQU 0135h
WM_CTLCOLORDLG                 EQU 0136h
WM_CTLCOLORSCROLLBAR           EQU 0137h
WM_CTLCOLORSTATIC              EQU 0138h
WM_MOUSEMOVE                   EQU 0200h
WM_LBUTTONDOWN                 EQU 0201h
WM_LBUTTONUP                   EQU 0202h
WM_LBUTTONDBLCLK               EQU 0203h
WM_RBUTTONDOWN                 EQU 0204h
WM_RBUTTONUP                   EQU 0205h
WM_RBUTTONDBLCLK               EQU 0206h
WM_MBUTTONDOWN                 EQU 0207h
WM_MBUTTONUP                   EQU 0208h
WM_MBUTTONDBLCLK               EQU 0209h
WM_MOUSEWHEEL                  EQU 020Ah
WM_XBUTTONDOWN                 EQU 020Bh
WM_XBUTTONUP                   EQU 020Ch
WM_XBUTTONDBLCLK               EQU 020Dh
WM_MOUSEHWHEEL                 EQU 020Eh
WM_PARENTNOTIFY                EQU 0210h
WM_ENTERMENULOOP               EQU 0211h
WM_EXITMENULOOP                EQU 0212h
WM_NEXTMENU                    EQU 0213h
WM_SIZING                      EQU 0214h
WM_CAPTURECHANGED             EQU 0215h
WM_MOVING                      EQU 0216h
WM_POWERBROADCAST              EQU 0218h
WM_DEVICECHANGE                EQU 0219h
WM_MDICREATE                   EQU 0220h
WM_MDIDESTROY                  EQU 0221h
WM_MDIACTIVATE                 EQU 0222h
WM_MDIRESTORE                  EQU 0223h
WM_MDINEXT                     EQU 0224h
WM_MDIMAXIMIZE                 EQU 0225h
WM_MDITILE                     EQU 0226h
WM_MDICASCADE                  EQU 0227h
WM_MDIICONARRANGE              EQU 0228h
WM_MDIGETACTIVE                EQU 0229h
WM_MDISETMENU                  EQU 0230h
WM_ENTERSIZEMOVE               EQU 0231h
WM_EXITSIZEMOVE                EQU 0232h
WM_DROPFILES                   EQU 0233h
WM_MDIREFRESHMENU              EQU 0234h
WM_IME_SETCONTEXT              EQU 0281h
WM_IME_NOTIFY                  EQU 0282h
WM_IME_CONTROL                 EQU 0283h
WM_IME_COMPOSITIONFULL         EQU 0284h
WM_IME_SELECT                  EQU 0285h
WM_IME_CHAR                    EQU 0286h
WM_IME_KEYDOWN                 EQU 0288h
WM_IME_KEYUP                   EQU 0289h
WM_MOUSEHOVER                  EQU 02A1h
WM_MOUSELEAVE                  EQU 02A3h
WM_NCMOUSEHOVER                EQU 02A0h
WM_NCMOUSELEAVE                EQU 02A2h
WM_WTSSESSION_CHANGE           EQU 02B1h
WM_TABLET_FIRST                EQU 02C0h
WM_TABLET_LAST                 EQU 02DFh
WM_CUT                         EQU 0300h
WM_COPY                        EQU 0301h
WM_PASTE                       EQU 0302h
WM_CLEAR                       EQU 0303h
WM_UNDO                        EQU 0304h
WM_RENDERFORMAT                EQU 0305h
WM_RENDERALLFORMATS            EQU 0306h
WM_DESTROYCLIPBOARD            EQU 0307h
WM_DRAWCLIPBOARD               EQU 0308h
WM_PAINTCLIPBOARD              EQU 0309h
WM_VSCROLLCLIPBOARD            EQU 030Ah
WM_SIZECLIPBOARD               EQU 030Bh
WM_ASKCBFORMATNAME             EQU 030Ch
WM_CHANGECBCHAIN               EQU 030Dh
WM_HSCROLLCLIPBOARD            EQU 030Eh
WM_QUERYNEWPALETTE             EQU 030Fh
WM_PALETTEISCHANGING           EQU 0310h
WM_PALETTECHANGED              EQU 0311h
WM_HOTKEY                      EQU 0312h
WM_PRINT                       EQU 0317h
WM_PRINTCLIENT                 EQU 0318h
WM_APPCOMMAND                  EQU 0319h
WM_THEMECHANGED                EQU 031Ah
WM_CLIPBOARDUPDATE             EQU 031Dh
WM_DWMCOMPOSITIONCHANGED       EQU 031Eh
WM_DWMNCRENDERINGCHANGED       EQU 031Fh
WM_DWMCOLORIZATIONCOLORCHANGED EQU 0320h
WM_DWMWINDOWMAXIMIZEDCHANGE    EQU 0321h
WM_GETTITLEBARINFOEX           EQU 033Fh
WM_HANDHELDFIRST               EQU 0358h
WM_HANDHELDLAST                EQU 035Fh
WM_AFXFIRST                    EQU 0360h
WM_AFXLAST                     EQU 037Fh
WM_PENWINFIRST                 EQU 0380h
WM_PENWINLAST                  EQU 038Fh
WM_APP                         EQU 8000h

; ------------------------------------------------------------------
; File Attributes
; ------------------------------------------------------------------
FILE_ATTRIBUTE_READONLY             EQU 00000001h
FILE_ATTRIBUTE_HIDDEN               EQU 00000002h
FILE_ATTRIBUTE_SYSTEM               EQU 00000004h
FILE_ATTRIBUTE_DIRECTORY            EQU 00000010h
FILE_ATTRIBUTE_ARCHIVE             EQU 00000020h
FILE_ATTRIBUTE_DEVICE              EQU 00000040h
FILE_ATTRIBUTE_NORMAL              EQU 00000080h
FILE_ATTRIBUTE_TEMPORARY           EQU 00000100h
FILE_ATTRIBUTE_SPARSE_FILE         EQU 00000200h
FILE_ATTRIBUTE_REPARSE_POINT       EQU 00000400h
FILE_ATTRIBUTE_COMPRESSED          EQU 00000800h
FILE_ATTRIBUTE_OFFLINE             EQU 00001000h
FILE_ATTRIBUTE_NOT_CONTENT_INDEXED EQU 00002000h
FILE_ATTRIBUTE_ENCRYPTED           EQU 00004000h

; ------------------------------------------------------------------
; File Flags
; ------------------------------------------------------------------
FILE_FLAG_WRITE_THROUGH        EQU 80000000h
FILE_FLAG_OVERLAPPED          EQU 40000000h
FILE_FLAG_NO_BUFFERING        EQU 20000000h
FILE_FLAG_RANDOM_ACCESS        EQU 10000000h
FILE_FLAG_SEQUENTIAL_SCAN     EQU 08000000h
FILE_FLAG_DELETE_ON_CLOSE     EQU 04000000h
FILE_FLAG_BACKUP_SEMANTICS    EQU 02000000h
FILE_FLAG_POSIX_SEMANTICS     EQU 01000000h
FILE_FLAG_SESSION_AWARE       EQU 00800000h
FILE_FLAG_OPEN_REPARSE_POINT  EQU 00200000h
FILE_FLAG_OPEN_NO_RECALL      EQU 00100000h
FILE_FLAG_FIRST_PIPE_INSTANCE EQU 00080000h

; ------------------------------------------------------------------
; Standard Handles
; ------------------------------------------------------------------
STD_INPUT_HANDLE  EQU -10
STD_OUTPUT_HANDLE EQU -11
STD_ERROR_HANDLE  EQU -12

; ------------------------------------------------------------------
; Window Styles
; ------------------------------------------------------------------
WS_OVERLAPPED       EQU 00000000h
WS_POPUP           EQU 80000000h
WS_CHILD           EQU 40000000h
WS_MINIMIZE        EQU 20000000h
WS_VISIBLE         EQU 10000000h
WS_DISABLED        EQU 08000000h
WS_CLIPSIBLINGS    EQU 04000000h
WS_CLIPCHILDREN    EQU 02000000h
WS_MAXIMIZE        EQU 01000000h
WS_CAPTION         EQU 00C00000h
WS_BORDER          EQU 00800000h
WS_DLGFRAME        EQU 00400000h
WS_VSCROLL         EQU 00200000h
WS_HSCROLL         EQU 00100000h
WS_SYSMENU         EQU 00080000h
WS_THICKFRAME      EQU 00040000h
WS_GROUP           EQU 00020000h
WS_TABSTOP         EQU 00010000h
WS_MINIMIZEBOX     EQU 00020000h
WS_MAXIMIZEBOX     EQU 00010000h

WS_EX_DLGMODALFRAME    EQU 00000001h
WS_EX_NOPARENTNOTIFY   EQU 00000004h
WS_EX_TOPMOST          EQU 00000008h
WS_EX_ACCEPTFILES      EQU 00000010h
WS_EX_TRANSPARENT      EQU 00000020h
WS_EX_MDICHILD         EQU 00000040h
WS_EX_TOOLWINDOW       EQU 00000080h
WS_EX_WINDOWEDGE       EQU 00000100h
WS_EX_CLIENTEDGE       EQU 00000200h
WS_EX_CONTEXTHELP      EQU 00000400h
WS_EX_RIGHT           EQU 00001000h
WS_EX_LEFT            EQU 00000000h
WS_EX_RTLREADING      EQU 00002000h
WS_EX_LTRREADING      EQU 00000000h
WS_EX_LEFTSCROLLBAR   EQU 00004000h
WS_EX_RIGHTSCROLLBAR  EQU 00000000h
WS_EX_CONTROLPARENT   EQU 00010000h
WS_EX_STATICEDGE      EQU 00020000h
WS_EX_APPWINDOW       EQU 00040000h
WS_EX_LAYERED         EQU 00080000h
WS_EX_NOINHERITLAYOUT EQU 00100000h
WS_EX_NOREDIRECTIONBITMAP EQU 00200000h
WS_EX_LAYOUTRTL       EQU 00400000h
WS_EX_COMPOSITED      EQU 00800000h
WS_EX_NOACTIVATE      EQU 08000000h

; ------------------------------------------------------------------
; Show Window Commands
; ------------------------------------------------------------------
SW_HIDE           EQU 0
SW_SHOWNORMAL     EQU 1
SW_NORMAL         EQU 1
SW_SHOWMINIMIZED  EQU 2
SW_SHOWMAXIMIZED  EQU 3
SW_MAXIMIZE       EQU 3
SW_SHOWNOACTIVATE EQU 4
SW_SHOW           EQU 5
SW_MINIMIZE       EQU 6
SW_SHOWMINNOACTIVE EQU 7
SW_SHOWNA         EQU 8
SW_RESTORE        EQU 9
SW_SHOWDEFAULT    EQU 10
SW_FORCEMINIMIZE  EQU 11
SW_MAX            EQU 11

; ------------------------------------------------------------------
; Virtual Key Codes
; ------------------------------------------------------------------
VK_LBUTTON        EQU 01h
VK_RBUTTON        EQU 02h
VK_CANCEL         EQU 03h
VK_MBUTTON        EQU 04h
VK_BACK           EQU 08h
VK_TAB            EQU 09h
VK_CLEAR          EQU 0Ch
VK_RETURN         EQU 0Dh
VK_SHIFT          EQU 10h
VK_CONTROL        EQU 11h
VK_MENU           EQU 12h
VK_PAUSE          EQU 13h
VK_CAPITAL        EQU 14h
VK_KANA           EQU 15h
VK_HANGUL         EQU 15h
VK_JUNJA          EQU 17h
VK_FINAL          EQU 18h
VK_HANJA          EQU 19h
VK_KANJI          EQU 19h
VK_ESCAPE         EQU 1Bh
VK_CONVERT        EQU 1Ch
VK_NONCONVERT     EQU 1Dh
VK_ACCEPT         EQU 1Eh
VK_MODECHANGE     EQU 1Fh
VK_SPACE          EQU 20h
VK_PRIOR          EQU 21h
VK_NEXT           EQU 22h
VK_END            EQU 23h
VK_HOME           EQU 24h
VK_LEFT           EQU 25h
VK_UP             EQU 26h
VK_RIGHT          EQU 27h
VK_DOWN           EQU 28h
VK_SELECT         EQU 29h
VK_PRINT          EQU 2Ah
VK_EXECUTE        EQU 2Bh
VK_SNAPSHOT       EQU 2Ch
VK_INSERT         EQU 2Dh
VK_DELETE         EQU 2Eh
VK_HELP           EQU 2Fh
VK_LWIN           EQU 5Bh
VK_RWIN           EQU 5Ch
VK_APPS           EQU 5Dh
VK_SLEEP          EQU 5Fh
VK_NUMPAD0        EQU 60h
VK_NUMPAD1        EQU 61h
VK_NUMPAD2        EQU 62h
VK_NUMPAD3        EQU 63h
VK_NUMPAD4        EQU 64h
VK_NUMPAD5        EQU 65h
VK_NUMPAD6        EQU 66h
VK_NUMPAD7        EQU 67h
VK_NUMPAD8        EQU 68h
VK_NUMPAD9        EQU 69h
VK_MULTIPLY       EQU 6Ah
VK_ADD            EQU 6Bh
VK_SEPARATOR      EQU 6Ch
VK_SUBTRACT       EQU 6Dh
VK_DECIMAL        EQU 6Eh
VK_DIVIDE         EQU 6Fh
VK_F1             EQU 70h
VK_F2             EQU 71h
VK_F3             EQU 72h
VK_F4             EQU 73h
VK_F5             EQU 74h
VK_F6             EQU 75h
VK_F7             EQU 76h
VK_F8             EQU 77h
VK_F9             EQU 78h
VK_F10            EQU 79h
VK_F11            EQU 7Ah
VK_F12            EQU 7Bh
VK_F13            EQU 7Ch
VK_F14            EQU 7Dh
VK_F15            EQU 7Eh
VK_F16            EQU 7Fh
VK_F17            EQU 80h
VK_F18            EQU 81h
VK_F19            EQU 82h
VK_F20            EQU 83h
VK_F21            EQU 84h
VK_F22            EQU 85h
VK_F23            EQU 86h
VK_F24            EQU 87h
VK_NUMLOCK        EQU 90h
VK_SCROLL         EQU 91h
VK_OEM_NEC_EQUAL  EQU 92h
VK_OEM_FJ_JISHO   EQU 92h
VK_OEM_FJ_MASSHOU EQU 93h
VK_OEM_FJ_TOUROKU EQU 94h
VK_OEM_FJ_LOYA    EQU 95h
VK_OEM_FJ_ROYA    EQU 96h
VK_LSHIFT          EQU 0A0h
VK_RSHIFT          EQU 0A1h
VK_LCONTROL        EQU 0A2h
VK_RCONTROL        EQU 0A3h
VK_LMENU           EQU 0A4h
VK_RMENU           EQU 0A5h
VK_BROWSER_BACK        EQU 0A6h
VK_BROWSER_FORWARD    EQU 0A7h
VK_BROWSER_REFRESH    EQU 0A8h
VK_BROWSER_STOP       EQU 0A9h
VK_BROWSER_SEARCH     EQU 0AAh
VK_BROWSER_FAVORITES  EQU 0ABh
VK_BROWSER_HOME       EQU 0ACh
VK_VOLUME_MUTE        EQU 0ADh
VK_VOLUME_DOWN        EQU 0AEh
VK_VOLUME_UP          EQU 0AFh
VK_MEDIA_NEXT_TRACK   EQU 0B0h
VK_MEDIA_PREV_TRACK   EQU 0B1h
VK_MEDIA_PLAY_PAUSE   EQU 0B2h
VK_LAUNCH_MAIL        EQU 0B4h
VK_LAUNCH_MEDIA_SELECT EQU 0B5h
VK_LAUNCH_APP1        EQU 0B6h
VK_LAUNCH_APP2        EQU 0B7h
VK_OEM_1              EQU 0BAh
VK_OEM_PLUS           EQU 0BBh
VK_OEM_COMMA          EQU 0BCh
VK_OEM_MINUS          EQU 0BDh
VK_OEM_PERIOD         EQU 0BEh
VK_OEM_2              EQU 0BFh
VK_OEM_3              EQU 0C0h
VK_OEM_4              EQU 0DBh
VK_OEM_5              EQU 0DCh
VK_OEM_6              EQU 0DDh
VK_OEM_7              EQU 0DEh
VK_OEM_8              EQU 0DFh
VK_OEM_102            EQU 0E2h
VK_OEM_BACKTAB        EQU 0F5h
VK_ATTN               EQU 0F6h
VK_CRSEL              EQU 0F7h
VK_EXSEL              EQU 0F8h
VK_EREOF              EQU 0F9h
VK_PLAY               EQU 0FAh
VK_ZOOM               EQU 0FBh
VK_NONAME             EQU 0FCh
VK_PA1                EQU 0FDh
VK_OEM_CLEAR          EQU 0FEh

; ------------------------------------------------------------------
; Mouse Input Constants
; ------------------------------------------------------------------
MOUSEEVENTF_MOVE         EQU 0001h
MOUSEEVENTF_LEFTDOWN     EQU 0002h
MOUSEEVENTF_LEFTUP       EQU 0004h
MOUSEEVENTF_RIGHTDOWN    EQU 0008h
MOUSEEVENTF_RIGHTUP      EQU 0010h
MOUSEEVENTF_MIDDLEDOWN   EQU 0020h
MOUSEEVENTF_MIDDLEUP     EQU 0040h
MOUSEEVENTF_XDOWN        EQU 0080h
MOUSEEVENTF_XUP          EQU 0100h
MOUSEEVENTF_WHEEL        EQU 0800h
MOUSEEVENTF_HWHEEL       EQU 01000h
MOUSEEVENTF_MOVE_NOCOALESCE EQU 2000h
MOUSEEVENTF_VIRTUAL_DESK EQU 4000h
MOUSEEVENTF_ABSOLUTE     EQU 8000h

; ------------------------------------------------------------------
; Extended Window Styles
; ------------------------------------------------------------------
IFNDEF WS_EX_DLGMODALFRAME
WS_EX_DLGMODALFRAME    EQU 00000001h
ENDIF
IFNDEF WS_EX_NOPARENTNOTIFY
WS_EX_NOPARENTNOTIFY   EQU 00000002h
ENDIF
IFNDEF WS_EX_TOPMOST
WS_EX_TOPMOST          EQU 00000008h
ENDIF
IFNDEF WS_EX_ACCEPTFILES
WS_EX_ACCEPTFILES      EQU 00000010h
ENDIF
IFNDEF WS_EX_TRANSPARENT
WS_EX_TRANSPARENT      EQU 00000020h
ENDIF
IFNDEF WS_EX_MDICHILD
WS_EX_MDICHILD         EQU 00000040h
ENDIF
IFNDEF WS_EX_TOOLWINDOW
WS_EX_TOOLWINDOW       EQU 00000080h
ENDIF
IFNDEF WS_EX_WINDOWEDGE
WS_EX_WINDOWEDGE       EQU 00000100h
ENDIF
IFNDEF WS_EX_CLIENTEDGE
WS_EX_CLIENTEDGE       EQU 00000200h
ENDIF
IFNDEF WS_EX_CONTEXTHELP
WS_EX_CONTEXTHELP      EQU 00000400h
ENDIF
IFNDEF WS_EX_RIGHT
WS_EX_RIGHT           EQU 00001000h
ENDIF
IFNDEF WS_EX_LEFT
WS_EX_LEFT            EQU 00000000h
ENDIF
IFNDEF WS_EX_RTLREADING
WS_EX_RTLREADING      EQU 00002000h
ENDIF
IFNDEF WS_EX_LTRREADING
WS_EX_LTRREADING      EQU 00000000h
ENDIF
IFNDEF WS_EX_LEFTSCROLLBAR
WS_EX_LEFTSCROLLBAR   EQU 00004000h
ENDIF
IFNDEF WS_EX_RIGHTSCROLLBAR
WS_EX_RIGHTSCROLLBAR  EQU 00000000h
ENDIF
IFNDEF WS_EX_CONTROLPARENT
WS_EX_CONTROLPARENT   EQU 00010000h
ENDIF
IFNDEF WS_EX_STATICEDGE
WS_EX_STATICEDGE      EQU 00020000h
ENDIF
IFNDEF WS_EX_APPWINDOW
WS_EX_APPWINDOW       EQU 00040000h
ENDIF
IFNDEF WS_EX_LAYERED
WS_EX_LAYERED         EQU 00080000h
ENDIF
IFNDEF WS_EX_NOINHERITLAYOUT
WS_EX_NOINHERITLAYOUT EQU 00100000h
ENDIF
WS_EX_NOREDIRECTIONBITMAP EQU 00200000h
WS_EX_LAYOUTRTL       EQU 00400000h
WS_EX_COMPOSITED      EQU 00800000h
WS_EX_NOACTIVATE      EQU 08000000h

; ------------------------------------------------------------------
; Console Constants
; ------------------------------------------------------------------
CONSOLE_TEXTMODE_BUFFER EQU 1

; ------------------------------------------------------------------
; Color Values
; ------------------------------------------------------------------
COLOR_SCROLLBAR       EQU 0
COLOR_BACKGROUND      EQU 1
COLOR_ACTIVECAPTION  EQU 2
COLOR_INACTIVECAPTION EQU 3
COLOR_MENU           EQU 4
COLOR_WINDOW         EQU 5
COLOR_WINDOWFRAME    EQU 6
COLOR_MENUTEXT       EQU 7
COLOR_WINDOWTEXT     EQU 8
COLOR_CAPTIONTEXT    EQU 9
COLOR_ACTIVEBORDER   EQU 10
COLOR_INACTIVEBORDER EQU 11
COLOR_APPWORKSPACE   EQU 12
COLOR_HIGHLIGHT      EQU 13
COLOR_HIGHLIGHTTEXT  EQU 14
COLOR_BTNFACE        EQU 15
COLOR_BTNSHADOW      EQU 16
COLOR_GRAYTEXT       EQU 17
COLOR_BTNTEXT        EQU 18
COLOR_INACTIVECAPTIONTEXT EQU 19
COLOR_BTNHIGHLIGHT   EQU 20
COLOR_3DDKSHADOW     EQU 21
COLOR_3DLIGHT        EQU 22
COLOR_INFOTEXT       EQU 23
COLOR_INFOBK         EQU 24
COLOR_DESKTOP        EQU 25
IFNDEF COLOR_3DFACE
COLOR_3DFACE         EQU 26
ENDIF
COLOR_3DSHADOW       EQU 27
IFNDEF COLOR_3DHIGHLIGHT
COLOR_3DHIGHLIGHT    EQU 28
ENDIF
IFNDEF COLOR_3DHILIGHT
COLOR_3DHILIGHT      EQU 28
ENDIF
COLOR_BTNHILIGHT     EQU 20
COLOR_GRADIENTACTIVECAPTION EQU 27
COLOR_GRADIENTINACTIVECAPTION EQU 28
COLOR_MENUHILIGHT    EQU 29
COLOR_MENUBAR        EQU 30

; ------------------------------------------------------------------
; Font Families
; ------------------------------------------------------------------
FF_DONTCARE EQU 0
FF_ROMAN    EQU 16
FF_SWISS    EQU 32
FF_MODERN   EQU 48
FF_SCRIPT   EQU 64
FF_DECORATIVE EQU 80

; ------------------------------------------------------------------
; Font Weights
; ------------------------------------------------------------------
FW_DONTCARE EQU 0
FW_THIN     EQU 100
FW_EXTRALIGHT EQU 200
FW_ULTRALIGHT EQU 200
FW_LIGHT    EQU 300
FW_NORMAL   EQU 400
FW_REGULAR  EQU 400
FW_MEDIUM   EQU 500
FW_SEMIBOLD EQU 600
FW_DEMIBOLD EQU 600
FW_BOLD     EQU 700
FW_EXTRABOLD EQU 800
FW_ULTRABOLD EQU 800
FW_HEAVY    EQU 900
FW_BLACK    EQU 900

; ------------------------------------------------------------------
; Text Alignment Flags
; ------------------------------------------------------------------
TA_NOUPDATECP EQU 0
TA_UPDATECP   EQU 1
TA_LEFT       EQU 0
TA_RIGHT      EQU 2
TA_CENTER     EQU 6
TA_TOP        EQU 0
TA_BOTTOM     EQU 8
TA_BASELINE   EQU 24

; ------------------------------------------------------------------
; Background Modes
; ------------------------------------------------------------------
TRANSPARENT   EQU 1
OPAQUE        EQU 2

; ------------------------------------------------------------------
; Raster Operations
; ------------------------------------------------------------------
R2_BLACK            EQU 1
R2_NOTMERGEPEN      EQU 2
R2_MASKNOTPEN       EQU 3
R2_NOTCOPYPEN       EQU 4
R2_MASKPENNOT       EQU 5
R2_NOT              EQU 6
R2_XORPEN           EQU 7
R2_NOTMASKPEN       EQU 8
R2_MASKPEN          EQU 9
R2_NOTXORPEN        EQU 10
R2_NOP              EQU 11
R2_MERGENOTPEN      EQU 12
R2_COPYPEN          EQU 13
R2_MERGEPEN         EQU 14
R2_WHITE            EQU 15

; ------------------------------------------------------------------
; Pen Styles
; ------------------------------------------------------------------
PS_SOLID       EQU 0
PS_DASH        EQU 1
PS_DOT         EQU 2
PS_DASHDOT     EQU 3
PS_DASHDOTDOT  EQU 4
PS_NULL        EQU 5
PS_INSIDEFRAME EQU 6
PS_USERSTYLE   EQU 7
PS_ALTERNATE   EQU 8

; ------------------------------------------------------------------
; Brush Styles
; ------------------------------------------------------------------
BS_SOLID       EQU 0
BS_NULL        EQU 1
BS_HATCHED     EQU 2
BS_PATTERN     EQU 3
BS_INDEXED     EQU 4
BS_DIBPATTERN  EQU 5
BS_DIBPATTERNPT EQU 6
BS_PATTERN8X8  EQU 7
BS_DIBPATTERN8X8 EQU 8

; ------------------------------------------------------------------
; Hatch Styles
; ------------------------------------------------------------------
HS_HORIZONTAL   EQU 0
HS_VERTICAL     EQU 1
HS_FDIAGONAL    EQU 2
HS_BDIAGONAL    EQU 3
HS_CROSS        EQU 4
HS_DIAGCROSS    EQU 5

; ------------------------------------------------------------------
; Stock Object Constants
; ------------------------------------------------------------------
WHITE_BRUSH         EQU 0
LTGRAY_BRUSH        EQU 1
GRAY_BRUSH          EQU 2
DKGRAY_BRUSH        EQU 3
BLACK_BRUSH         EQU 4
NULL_BRUSH          EQU 5
HOLLOW_BRUSH        EQU 5
WHITE_PEN           EQU 6
BLACK_PEN           EQU 7
NULL_PEN            EQU 8
OEM_FIXED_FONT      EQU 10
ANSI_FIXED_FONT     EQU 11
ANSI_VAR_FONT       EQU 12
SYSTEM_FONT         EQU 13
DEVICE_DEFAULT_FONT EQU 14
DEFAULT_PALETTE     EQU 15
SYSTEM_FIXED_FONT   EQU 16
DEFAULT_GUI_FONT    EQU 17

; ------------------------------------------------------------------
; Clipboard Formats
; ------------------------------------------------------------------
CF_TEXT         EQU 1
CF_BITMAP       EQU 2
CF_METAFILEPICT EQU 3
CF_SYLK         EQU 4
CF_DIF          EQU 5
CF_TIFF         EQU 6
CF_OEMTEXT      EQU 7
CF_DIB          EQU 8
CF_PALETTE      EQU 9
CF_PENDATA      EQU 10
CF_RIFF         EQU 11
CF_WAVE         EQU 12
CF_UNICODETEXT  EQU 13
CF_ENHMETAFILE  EQU 14
CF_HDROP        EQU 15
CF_LOCALE       EQU 16
CF_DIBV5        EQU 17

; ------------------------------------------------------------------
; Error Codes (Win32)
; ------------------------------------------------------------------
ERROR_SUCCESS                EQU 0
ERROR_INVALID_FUNCTION       EQU 1
ERROR_FILE_NOT_FOUND         EQU 2
ERROR_PATH_NOT_FOUND         EQU 3
ERROR_TOO_MANY_OPEN_FILES    EQU 4
ERROR_ACCESS_DENIED          EQU 5
ERROR_INVALID_HANDLE         EQU 6
ERROR_ARENA_TRASHED          EQU 7
ERROR_NOT_ENOUGH_MEMORY      EQU 8
ERROR_INVALID_BLOCK          EQU 9
ERROR_BAD_ENVIRONMENT        EQU 10
ERROR_BAD_FORMAT             EQU 11
ERROR_INVALID_ACCESS         EQU 12
ERROR_INVALID_DATA           EQU 13
ERROR_OUTOFMEMORY            EQU 14
ERROR_INVALID_DRIVE          EQU 15
ERROR_CURRENT_DIRECTORY      EQU 16
ERROR_NOT_SAME_DEVICE        EQU 17
ERROR_NO_MORE_FILES          EQU 18
ERROR_WRITE_PROTECT          EQU 19
ERROR_BAD_UNIT               EQU 20
ERROR_NOT_READY              EQU 21
ERROR_BAD_COMMAND            EQU 22
ERROR_CRC                    EQU 23
ERROR_BAD_LENGTH             EQU 24
ERROR_SEEK                   EQU 25
ERROR_NOT_DOS_DISK           EQU 26
ERROR_SECTOR_NOT_FOUND       EQU 27
ERROR_OUT_OF_PAPER           EQU 28
ERROR_WRITE_FAULT            EQU 29
ERROR_READ_FAULT             EQU 30
ERROR_GEN_FAILURE            EQU 31
ERROR_SHARING_VIOLATION      EQU 32
ERROR_LOCK_VIOLATION         EQU 33
ERROR_WRONG_DISK            EQU 34
ERROR_SHARING_BUFFER_EXCEEDED EQU 36
ERROR_HANDLE_EOF             EQU 38
ERROR_HANDLE_DISK_FULL       EQU 39
ERROR_NOT_SUPPORTED          EQU 50
ERROR_REM_NOT_LIST           EQU 51
ERROR_DUP_NAME               EQU 52
ERROR_BAD_NETPATH            EQU 53
ERROR_NETWORK_BUSY           EQU 54
ERROR_DEV_NOT_EXIST          EQU 55
ERROR_TOO_MANY_CMDS          EQU 56
ERROR_ADAP_HDW_ERR           EQU 57
ERROR_BAD_NET_RESP           EQU 58
ERROR_UNEXP_NET_ERR          EQU 59
ERROR_BAD_REM_ADAP           EQU 60
ERROR_PRINTQ_FULL            EQU 61
ERROR_NO_SPOOL_SPACE         EQU 62
ERROR_PRINT_CANCELLED        EQU 63
ERROR_NETNAME_DELETED        EQU 64
ERROR_NETWORK_ACCESS_DENIED  EQU 65
ERROR_BAD_DEV_TYPE           EQU 66
ERROR_BAD_NET_NAME           EQU 67
ERROR_TOO_MANY_NAMES         EQU 68
ERROR_TOO_MANY_SESS         EQU 69
ERROR_SHARING_PAUSED        EQU 70
ERROR_REQUEST_NOT_ACCEPTED   EQU 71
ERROR_REDIR_PAUSED          EQU 72
ERROR_FILE_EXISTS           EQU 80
ERROR_CANNOT_MAKE           EQU 82
ERROR_FAIL_I24              EQU 83
ERROR_OUT_OF_STRUCTURES     EQU 84
ERROR_ALREADY_ASSIGNED      EQU 85
ERROR_INVALID_PASSWORD      EQU 86
ERROR_INVALID_PARAMETER    EQU 87
ERROR_NET_WRITE_FAULT      EQU 88
ERROR_NO_PROC_SLOTS        EQU 89
ERROR_TOO_MANY_SEMAPHORES  EQU 100
ERROR_EXCL_SEM_ALREADY_OWNED EQU 101
ERROR_SEM_IS_SET           EQU 102
ERROR_TOO_MANY_SEM_REQUESTS EQU 103
ERROR_INVALID_AT_INTERRUPT_TIME EQU 104
ERROR_SEM_OWNER_DIED       EQU 105
ERROR_SEM_USER_LIMIT       EQU 106
ERROR_SIGNAL_PENDING       EQU 107
ERROR_TOO_MANY_THREADS     EQU 108
ERROR_SEMAPHORE_TIMEOUT    EQU 121
ERROR_INSUFFICIENT_BUFFER  EQU 122
ERROR_INVALID_LEVEL        EQU 123
ERROR_NO_BEARINGS          EQU 124
ERROR_BROKEN_PIPE          EQU 109
ERROR_NO_DATA              EQU 110
ERROR_PIPE_NOT_CONNECTED   EQU 116
ERROR_BAD_PIPE             EQU 230
ERROR_PIPE_BUSY            EQU 231
ERROR_NO_MEMORY            EQU 8
ERROR_BUFFER_OVERFLOW      EQU 111
ERROR_NO_MORE_ITEMS        EQU 259
ERROR_CANNOT_COPY          EQU 266
ERROR_DIRECTORY           EQU 267
ERROR_DLL_INIT_FAILED     EQU 677
ERROR_BAD_USERNAME        EQU 2202
ERROR_NOT_CONNECTED       EQU 2250
ERROR_OPEN_FAILED         EQU 110
ERROR_BUFFER_OVERFLOW     EQU 111
ERROR_INSUFFICIENT_BUFFER EQU 122
ERROR_META_EXPANSION_TOO_LONG EQU 408

ENDIF
