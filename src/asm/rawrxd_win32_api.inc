; ═══════════════════════════════════════════════════════════════════════════════
; rawrxd_win32_api.inc - Comprehensive Win32 API EXTERN Declarations (x64)
; ═══════════════════════════════════════════════════════════════════════════════
; All undefined Win32 CALL targets resolved. Include this in any 64-bit .asm
; file that calls Win32 APIs without its own EXTERN block.
;
; Usage:  INCLUDE rawrxd_win32_api.inc
; ═══════════════════════════════════════════════════════════════════════════════

IFNDEF RAWRXD_WIN32_API_INC
RAWRXD_WIN32_API_INC EQU 1

; ═══════════════════════════════════════════════════════════════════════════════
; kernel32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF AddVectoredExceptionHandler:PROC
EXTERNDEF CheckRemoteDebuggerPresent:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF CreateDirectoryA:PROC
EXTERNDEF CreateDirectoryW:PROC
EXTERNDEF CreateEventA:PROC
EXTERNDEF CreateEventW:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF CreateFileW:PROC
EXTERNDEF CreateFileMappingA:PROC
EXTERNDEF CreateFileMappingW:PROC
EXTERNDEF CreateMutexA:PROC
EXTERNDEF CreateMutexW:PROC
EXTERNDEF CreateProcessA:PROC
EXTERNDEF CreateProcessW:PROC
EXTERNDEF CreateSemaphoreA:PROC
EXTERNDEF CreateSemaphoreW:PROC
EXTERNDEF CreateThread:PROC
EXTERNDEF DeleteFileA:PROC
EXTERNDEF DeleteFileW:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF FlushFileBuffers:PROC
EXTERNDEF FreeLibrary:PROC
EXTERNDEF GetCommandLineA:PROC
EXTERNDEF GetCommandLineW:PROC
EXTERNDEF GetConsoleScreenBufferInfo:PROC
EXTERNDEF GetCurrentProcess:PROC
EXTERNDEF GetCurrentProcessId:PROC
EXTERNDEF GetCurrentThread:PROC
EXTERNDEF GetCurrentThreadId:PROC
EXTERNDEF GetEnvironmentVariableA:PROC
EXTERNDEF GetEnvironmentVariableW:PROC
EXTERNDEF GetExitCodeProcess:PROC
EXTERNDEF GetFileSizeEx:PROC
EXTERNDEF GetLastError:PROC
EXTERNDEF GetModuleFileNameA:PROC
EXTERNDEF GetModuleFileNameW:PROC
EXTERNDEF GetModuleHandleA:PROC
EXTERNDEF GetModuleHandleW:PROC
EXTERNDEF GetOverlappedResult:PROC
EXTERNDEF GetProcessHeap:PROC
EXTERNDEF GetProcAddress:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF GetSystemInfo:PROC
EXTERNDEF GetSystemTimePreciseAsFileTime:PROC
EXTERNDEF GetSystemTimeAsFileTime:PROC
EXTERNDEF GetTempPathA:PROC
EXTERNDEF GetTempPathW:PROC
EXTERNDEF GetTickCount64:PROC
EXTERNDEF GlobalAlloc:PROC
EXTERNDEF GlobalFree:PROC
EXTERNDEF HeapAlloc:PROC
EXTERNDEF HeapCreate:PROC
EXTERNDEF HeapDestroy:PROC
EXTERNDEF HeapFree:PROC
EXTERNDEF IsDebuggerPresent:PROC
EXTERNDEF IsWow64Process:PROC
EXTERNDEF LoadLibraryA:PROC
EXTERNDEF LoadLibraryW:PROC
EXTERNDEF LocalAlloc:PROC
EXTERNDEF LocalFree:PROC
EXTERNDEF MapViewOfFile:PROC
EXTERNDEF MultiByteToWideChar:PROC
EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF QueryPerformanceCounter:PROC
EXTERNDEF QueryPerformanceFrequency:PROC
EXTERNDEF ReadConsoleA:PROC
EXTERNDEF ReadConsoleInputA:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF RemoveVectoredExceptionHandler:PROC
EXTERNDEF ResumeThread:PROC
EXTERNDEF RtlZeroMemory:PROC
EXTERNDEF SearchPathA:PROC
EXTERNDEF SearchPathW:PROC
EXTERNDEF SetConsoleCursorPosition:PROC
EXTERNDEF SetConsoleMode:PROC
EXTERNDEF SetErrorMode:PROC
EXTERNDEF SetEvent:PROC
EXTERNDEF SetThreadAffinityMask:PROC
EXTERNDEF SetUnhandledExceptionFilter:PROC
EXTERNDEF Sleep:PROC
EXTERNDEF SwitchToThread:PROC
EXTERNDEF TerminateProcess:PROC
EXTERNDEF TlsAlloc:PROC
EXTERNDEF TlsFree:PROC
EXTERNDEF TlsGetValue:PROC
EXTERNDEF TlsSetValue:PROC
EXTERNDEF UnmapViewOfFile:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF VirtualProtect:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF WaitForMultipleObjects:PROC
EXTERNDEF WideCharToMultiByte:PROC
EXTERNDEF WriteConsoleA:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF lstrcmpiA:PROC
EXTERNDEF lstrcpynA:PROC
EXTERNDEF lstrlenA:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; user32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF AppendMenuA:PROC
EXTERNDEF BeginPaint:PROC
EXTERNDEF CreateMenu:PROC
EXTERNDEF CreatePopupMenu:PROC
EXTERNDEF CreateWindowExA:PROC
EXTERNDEF CreateWindowExW:PROC
EXTERNDEF DefWindowProcA:PROC
EXTERNDEF DefWindowProcW:PROC
EXTERNDEF DestroyWindow:PROC
EXTERNDEF DispatchMessageA:PROC
EXTERNDEF EndPaint:PROC
EXTERNDEF GetClientRect:PROC
EXTERNDEF GetMessageA:PROC
EXTERNDEF GetSysColor:PROC
EXTERNDEF GetSystemMetrics:PROC
EXTERNDEF InvalidateRect:PROC
EXTERNDEF LoadCursorA:PROC
EXTERNDEF LoadIconA:PROC
EXTERNDEF MessageBoxA:PROC
EXTERNDEF MessageBoxW:PROC
EXTERNDEF MoveWindow:PROC
EXTERNDEF PeekMessageA:PROC
EXTERNDEF PostQuitMessage:PROC
EXTERNDEF RegisterClassA:PROC
EXTERNDEF RegisterClassExA:PROC
EXTERNDEF RegisterClassExW:PROC
EXTERNDEF SendMessageA:PROC
EXTERNDEF SendMessageW:PROC
EXTERNDEF SetLayeredWindowAttributes:PROC
EXTERNDEF SetMenu:PROC
EXTERNDEF SetTimer:PROC
EXTERNDEF SetWindowTextA:PROC
EXTERNDEF ShowWindow:PROC
EXTERNDEF TranslateMessage:PROC
EXTERNDEF UpdateWindow:PROC
EXTERNDEF wsprintfA:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; gdi32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF DeleteObject:PROC
EXTERNDEF GetTextMetricsA:PROC
EXTERNDEF PatBlt:PROC
EXTERNDEF SetBkMode:PROC
EXTERNDEF SelectObject:PROC
EXTERNDEF CreateFontIndirectA:PROC
EXTERNDEF TextOutA:PROC
EXTERNDEF SetTextColor:PROC
EXTERNDEF SetBkColor:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; advapi32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF RegCloseKey:PROC
EXTERNDEF RegCreateKeyExA:PROC
EXTERNDEF RegCreateKeyExW:PROC
EXTERNDEF RegDeleteKeyA:PROC
EXTERNDEF RegDeleteKeyW:PROC
EXTERNDEF RegOpenKeyExA:PROC
EXTERNDEF RegOpenKeyExW:PROC
EXTERNDEF RegQueryValueExA:PROC
EXTERNDEF RegQueryValueExW:PROC
EXTERNDEF RegSetValueExA:PROC
EXTERNDEF RegSetValueExW:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; ole32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CoInitializeEx:PROC
EXTERNDEF CoUninitialize:PROC
EXTERNDEF CoCreateInstance:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; shell32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CommandLineToArgvW:PROC
EXTERNDEF ShellExecuteA:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; shlwapi.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF PathCombineA:PROC
EXTERNDEF PathCombineW:PROC
EXTERNDEF PathFileExistsA:PROC
EXTERNDEF PathFileExistsW:PROC
EXTERNDEF PathRemoveFileSpecA:PROC
EXTERNDEF PathRemoveFileSpecW:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; comctl32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF InitCommonControlsEx:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; comdlg32.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF GetOpenFileNameA:PROC
EXTERNDEF GetSaveFileNameA:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; wininet.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF HttpAddRequestHeadersW:PROC
EXTERNDEF HttpOpenRequestA:PROC
EXTERNDEF HttpOpenRequestW:PROC
EXTERNDEF HttpQueryInfoA:PROC
EXTERNDEF HttpQueryInfoW:PROC
EXTERNDEF HttpSendRequestA:PROC
EXTERNDEF HttpSendRequestW:PROC
EXTERNDEF InternetCloseHandle:PROC
EXTERNDEF InternetConnectA:PROC
EXTERNDEF InternetConnectW:PROC
EXTERNDEF InternetOpenA:PROC
EXTERNDEF InternetOpenUrlA:PROC
EXTERNDEF InternetOpenW:PROC
EXTERNDEF InternetQueryDataAvailable:PROC
EXTERNDEF InternetReadFile:PROC
EXTERNDEF InternetSetOptionW:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; winhttp.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF WinHttpCloseHandle:PROC
EXTERNDEF WinHttpConnect:PROC
EXTERNDEF WinHttpOpen:PROC
EXTERNDEF WinHttpOpenRequest:PROC
EXTERNDEF WinHttpReceiveResponse:PROC
EXTERNDEF WinHttpSendRequest:PROC
EXTERNDEF WinHttpQueryDataAvailable:PROC
EXTERNDEF WinHttpReadData:PROC
EXTERNDEF WinHttpSetTimeouts:PROC
EXTERNDEF WinHttpAddRequestHeaders:PROC
EXTERNDEF WinHttpSetOption:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; httpapi.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF HttpAddUrl:PROC
EXTERNDEF HttpCreateHttpHandle:PROC
EXTERNDEF HttpInitialize:PROC
EXTERNDEF HttpReceiveHttpRequest:PROC
EXTERNDEF HttpReceiveRequestEntityBody:PROC
EXTERNDEF HttpRemoveUrl:PROC
EXTERNDEF HttpSendHttpResponse:PROC
EXTERNDEF HttpTerminate:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; ws2_32.dll (Winsock2)
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF WSAStartup:PROC
EXTERNDEF WSACleanup:PROC
EXTERNDEF WSAGetLastError:PROC
EXTERNDEF socket:PROC
EXTERNDEF bind:PROC
EXTERNDEF listen:PROC
EXTERNDEF accept:PROC
EXTERNDEF connect:PROC
EXTERNDEF send:PROC
EXTERNDEF recv:PROC
EXTERNDEF closesocket:PROC
EXTERNDEF select:PROC
EXTERNDEF getaddrinfo:PROC
EXTERNDEF freeaddrinfo:PROC
EXTERNDEF htons:PROC
EXTERNDEF ntohs:PROC
EXTERNDEF inet_addr:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; d2d1.dll / d3d11.dll / dxgi.dll / dwrite.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF D2D1CreateFactory:PROC
EXTERNDEF D3D11CreateDevice:PROC
EXTERNDEF CreateDXGIFactory1:PROC
EXTERNDEF DWriteCreateFactory:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; dbghelp.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF MiniDumpWriteDump:PROC
EXTERNDEF SymInitialize:PROC
EXTERNDEF StackWalk64:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; psapi.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF EnumProcessModules:PROC
EXTERNDEF GetModuleBaseNameA:PROC
EXTERNDEF GetModuleInformation:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; ntdll.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF NtAllocateVirtualMemory:PROC
EXTERNDEF NtCreateThreadEx:PROC
EXTERNDEF NtQueryInformationProcess:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; WebView2Loader.dll
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CreateCoreWebView2EnvironmentWithOptions:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; IOCP (kernel32)
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CreateIoCompletionPort:PROC
EXTERNDEF GetQueuedCompletionStatus:PROC
EXTERNDEF PostQueuedCompletionStatus:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Common Constants
; ═══════════════════════════════════════════════════════════════════════════════
IFNDEF TRUE
TRUE                    EQU 1
ENDIF
IFNDEF FALSE
FALSE                   EQU 0
ENDIF
IFNDEF NULL
NULL                    EQU 0
ENDIF
IFNDEF INFINITE
INFINITE                EQU 0FFFFFFFFh
ENDIF
IFNDEF STD_OUTPUT_HANDLE
STD_OUTPUT_HANDLE       EQU -11
ENDIF
IFNDEF STD_INPUT_HANDLE
STD_INPUT_HANDLE        EQU -10
ENDIF
IFNDEF STD_ERROR_HANDLE
STD_ERROR_HANDLE        EQU -12
ENDIF
IFNDEF INVALID_HANDLE_VALUE
INVALID_HANDLE_VALUE    EQU -1
ENDIF
IFNDEF HEAP_ZERO_MEMORY
HEAP_ZERO_MEMORY        EQU 00000008h
ENDIF
IFNDEF MEM_COMMIT
MEM_COMMIT              EQU 00001000h
ENDIF
IFNDEF MEM_RESERVE
MEM_RESERVE             EQU 00002000h
ENDIF
IFNDEF MEM_RELEASE
MEM_RELEASE             EQU 00008000h
ENDIF
IFNDEF PAGE_READWRITE
PAGE_READWRITE          EQU 04h
ENDIF
IFNDEF PAGE_EXECUTE_READWRITE
PAGE_EXECUTE_READWRITE  EQU 40h
ENDIF
IFNDEF WAIT_OBJECT_0
WAIT_OBJECT_0           EQU 0
ENDIF
IFNDEF GENERIC_READ
GENERIC_READ            EQU 80000000h
ENDIF
IFNDEF GENERIC_WRITE
GENERIC_WRITE           EQU 40000000h
ENDIF
IFNDEF OPEN_EXISTING
OPEN_EXISTING           EQU 3
ENDIF
IFNDEF CREATE_ALWAYS
CREATE_ALWAYS           EQU 2
ENDIF
IFNDEF FILE_ATTRIBUTE_NORMAL
FILE_ATTRIBUTE_NORMAL   EQU 80h
ENDIF
IFNDEF FILE_MAP_READ
FILE_MAP_READ           EQU 4
ENDIF
IFNDEF LMEM_FIXED
LMEM_FIXED              EQU 0
ENDIF

ENDIF ; RAWRXD_WIN32_API_INC
