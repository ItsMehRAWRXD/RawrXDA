;===============================================================================
; WINDOWS STRUCTURES - For Agentic Tools MASM Implementation
;===============================================================================

; WIN32_FIND_DATAA Structure
WIN32_FIND_DATAA STRUCT
    dwFileAttributes    DWORD       ?
    ftCreationTime      QWORD       ?
    ftLastAccessTime    QWORD       ?
    ftLastWriteTime     QWORD       ?
    nFileSizeHigh       DWORD       ?
    nFileSizeLow        DWORD       ?
    dwReserved0         DWORD       ?
    dwReserved1         DWORD       ?
    cFileName           BYTE        260 dup(?)
    cAlternateFileName  BYTE        14 dup(?)
WIN32_FIND_DATAA ENDS

; STARTUPINFOA Structure
STARTUPINFOA STRUCT
    cb                  DWORD       ?
    lpReserved          DWORD       ?
    lpDesktop           DWORD       ?
    lpTitle             DWORD       ?
    dwX                 DWORD       ?
    dwY                 DWORD       ?
    dwXSize             DWORD       ?
    dwYSize             DWORD       ?
    dwXCountChars       DWORD       ?
    dwYCountChars       DWORD       ?
    dwFillAttribute     DWORD       ?
    dwFlags             DWORD       ?
    wShowWindow         WORD        ?
    cbReserved2         WORD        ?
    lpReserved2         DWORD       ?
    hStdInput           DWORD       ?
    hStdOutput          DWORD       ?
    hStdError           DWORD       ?
STARTUPINFOA ENDS

; PROCESS_INFORMATION Structure
PROCESS_INFORMATION STRUCT
    hProcess            DWORD       ?
    hThread             DWORD       ?
    dwProcessId         DWORD       ?
    dwThreadId          DWORD       ?
PROCESS_INFORMATION ENDS

; SECURITY_ATTRIBUTES Structure
SECURITY_ATTRIBUTES STRUCT
    nLength             DWORD       ?
    lpSecurityDescriptor DWORD      ?
    bInheritHandle      DWORD       ?
SECURITY_ATTRIBUTES ENDS

; FILETIME Structure
FILETIME STRUCT
    dwLowDateTime       DWORD       ?
    dwHighDateTime      DWORD       ?
FILETIME ENDS

; SYSTEMTIME Structure
SYSTEMTIME STRUCT
    wYear               WORD        ?
    wMonth              WORD        ?
    wDayOfWeek          WORD        ?
    wDay                WORD        ?
    wHour               WORD        ?
    wMinute             WORD        ?
    wSecond             WORD        ?
    wMilliseconds       WORD        ?
SYSTEMTIME ENDS

;===============================================================================
; WINDOWS API FUNCTION MACROS
;===============================================================================

CreateFileA             equ     CreateFileA@28
ReadFile                equ     ReadFile@20
WriteFile               equ     WriteFile@20
CloseHandle             equ     CloseHandle@4
GetFileSize             equ     GetFileSize@8
GetLastError            equ     GetLastError@0
FindFirstFileA          equ     FindFirstFileA@8
FindNextFileA           equ     FindNextFileA@8
FindClose               equ     FindClose@4
GetCurrentDirectoryA    equ     GetCurrentDirectoryA@8
SetCurrentDirectoryA    equ     SetCurrentDirectoryA@4
GlobalAlloc             equ     GlobalAlloc@8
GlobalFree              equ     GlobalFree@4
lstrlenA                equ     lstrlenA@4
lstrcpyA                equ     lstrcpyA@8
lstrcatA                equ     lstrcatA@8
lstrcmpA                equ     lstrcmpA@8
wsprintfA               equ     wsprintfA@16
MessageBoxA             equ     MessageBoxA@16
ExitProcess             equ     ExitProcess@4

;===============================================================================
; CONSTANTS
;===============================================================================

; File Attributes
FILE_ATTRIBUTE_READONLY     equ 1
FILE_ATTRIBUTE_HIDDEN       equ 2
FILE_ATTRIBUTE_SYSTEM       equ 4
FILE_ATTRIBUTE_DIRECTORY    equ 16
FILE_ATTRIBUTE_ARCHIVE      equ 32
FILE_ATTRIBUTE_NORMAL       equ 128

; File Creation Disposition
CREATE_NEW              equ 1
CREATE_ALWAYS           equ 2
OPEN_EXISTING           equ 3
OPEN_ALWAYS             equ 4
TRUNCATE_EXISTING       equ 5

; File Share Mode
FILE_SHARE_READ         equ 1
FILE_SHARE_WRITE        equ 2
FILE_SHARE_DELETE       equ 4

; File Flags
FILE_FLAG_WRITE_THROUGH     equ 80000000h
FILE_FLAG_OVERLAPPED        equ 40000000h
FILE_FLAG_NO_BUFFERING      equ 20000000h
FILE_FLAG_RANDOM_ACCESS     equ 10000000h
FILE_FLAG_SEQUENTIAL_SCAN   equ 8000000h
FILE_FLAG_DELETE_ON_CLOSE   equ 4000000h
FILE_FLAG_BACKUP_SEMANTICS  equ 2000000h
FILE_FLAG_POSIX_SEMANTICS   equ 1000000h

; Generic Access Rights
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
GENERIC_EXECUTE         equ 20000000h
GENERIC_ALL             equ 10000000h

; Wait Results
WAIT_OBJECT_0           equ 0
WAIT_ABANDONED          equ 80h
WAIT_TIMEOUT            equ 102h
WAIT_FAILED             equ -1

; Invalid Handle
INVALID_HANDLE_VALUE    equ -1

; Memory Allocation Flags
GMEM_FIXED              equ 0
GMEM_MOVEABLE           equ 2
GMEM_ZEROINIT           equ 40h
GPTR                    equ 40h

; Boolean
TRUE                    equ 1
FALSE                   equ 0

; Path Length
MAX_PATH                equ 260

; Error Codes
ERROR_SUCCESS           equ 0
ERROR_INVALID_FUNCTION  equ 1
ERROR_FILE_NOT_FOUND    equ 2
ERROR_PATH_NOT_FOUND    equ 3
ERROR_ACCESS_DENIED     equ 5
ERROR_INVALID_HANDLE    equ 6
ERROR_NOT_ENOUGH_MEMORY equ 8
ERROR_INVALID_DATA      equ 13
ERROR_NO_MORE_FILES     equ 18
ERROR_SHARING_VIOLATION equ 32
ERROR_LOCK_VIOLATION    equ 33
ERROR_ALREADY_EXISTS    equ 183

; MessageBox Constants
MB_OK                       equ 0
MB_OKCANCEL                 equ 1
MB_ABORTRETRYIGNORE         equ 2
MB_YESNOCANCEL              equ 3
MB_YESNO                    equ 4
MB_RETRYCANCEL              equ 5
MB_CANCELTRYCONTINUE        equ 6
MB_ICONHAND                 equ 10h
MB_ICONQUESTION             equ 20h
MB_ICONEXCLAMATION          equ 30h
MB_ICONASTERISK             equ 40h
MB_ICONWARNING              equ 30h
MB_ICONERROR                equ 10h
MB_ICONINFORMATION          equ 40h
MB_DEFBUTTON1               equ 0
MB_DEFBUTTON2               equ 100h
MB_DEFBUTTON3               equ 200h
MB_DEFBUTTON4               equ 300h
MB_APPLMODAL                equ 0
MB_SYSTEMMODAL              equ 1000h
MB_TASKMODAL                equ 2000h
MB_TOPMOST                  equ 40000h

; Process Creation Flags
CREATE_DEFAULT_ERROR_MODE   equ 4000000h
CREATE_NEW_CONSOLE          equ 10h
CREATE_NEW_PROCESS_GROUP    equ 200h
CREATE_NO_WINDOW            equ 8000000h
CREATE_SUSPENDED            equ 4
DETACHED_PROCESS            equ 8
NORMAL_PRIORITY_CLASS       equ 20h
IDLE_PRIORITY_CLASS         equ 40h
HIGH_PRIORITY_CLASS         equ 80h
REALTIME_PRIORITY_CLASS     equ 100h

; Standard Handles
STD_INPUT_HANDLE            equ -10
STD_OUTPUT_HANDLE           equ -11
STD_ERROR_HANDLE            equ -12

; Startup Info Flags
STARTF_USESHOWWINDOW        equ 1
STARTF_USESIZE              equ 2
STARTF_USEPOSITION          equ 4
STARTF_USECOUNTCHARS        equ 8
STARTF_USEFILLATTRIBUTE     equ 10h
STARTF_RUNFULLSCREEN        equ 20h
STARTF_FORCEONFEEDBACK      equ 40h
STARTF_FORCEOFFFEEDBACK     equ 80h
STARTF_USESTDHANDLES        equ 100h

; Show Window Commands
SW_HIDE                     equ 0
SW_SHOWNORMAL               equ 1
SW_SHOWMINIMIZED            equ 2
SW_SHOWMAXIMIZED            equ 3
SW_SHOWNOACTIVATE           equ 4
SW_SHOW                     equ 5
SW_MINIMIZE                 equ 6
SW_SHOWMINNOACTIVE          equ 7
SW_SHOWNA                   equ 8
SW_RESTORE                  equ 9
SW_SHOWDEFAULT              equ 10

END
