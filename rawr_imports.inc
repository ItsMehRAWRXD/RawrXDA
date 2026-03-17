; =========================
; FILE: rawr_imports.inc
; =========================
option casemap:none

; --- process / debug ---
extrn   ExitProcess              : proc
extrn   GetLastError             : proc
extrn   OutputDebugStringW       : proc
extrn   GetCommandLineW          : proc
extrn   GetModuleHandleW         : proc

; --- console ---
extrn   GetStdHandle             : proc

; --- file i/o ---
extrn   CreateFileW              : proc
extrn   ReadFile                 : proc
extrn   WriteFile                : proc
extrn   CloseHandle              : proc
extrn   GetFileSizeEx            : proc
extrn   FlushFileBuffers         : proc

; --- heap ---
extrn   HeapCreate               : proc
extrn   HeapAlloc                : proc
extrn   HeapFree                 : proc
extrn   HeapDestroy              : proc
extrn   GetProcessHeap           : proc

; --- virtual memory ---
extrn   VirtualAlloc             : proc
extrn   VirtualFree              : proc
extrn   GetSystemInfo            : proc

; --- high-res time ---
extrn   QueryPerformanceCounter  : proc
extrn   QueryPerformanceFrequency: proc

; --- named pipes ---
extrn   CreateNamedPipeW         : proc
extrn   ConnectNamedPipe         : proc
extrn   DisconnectNamedPipe      : proc

; --- constants ---
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
FILE_SHARE_READ         equ 00000001h
FILE_SHARE_WRITE        equ 00000002h
OPEN_EXISTING           equ 3
CREATE_ALWAYS           equ 2
OPEN_ALWAYS             equ 4
FILE_ATTRIBUTE_NORMAL   equ 00000080h

INVALID_HANDLE_VALUE    equ -1

STD_ERROR_HANDLE        equ -12
STD_OUTPUT_HANDLE       equ -11

HEAP_NO_SERIALIZE       equ 00000001h
HEAP_ZERO_MEMORY        equ 00000008h

MEM_COMMIT              equ 00001000h
MEM_RESERVE             equ 00002000h
MEM_RELEASE             equ 00008000h
PAGE_READWRITE          equ 00000004h

PIPE_ACCESS_DUPLEX      equ 00000003h
PIPE_TYPE_BYTE          equ 00000000h
PIPE_READMODE_BYTE      equ 00000000h
PIPE_WAIT               equ 00000000h
PIPE_UNLIMITED_INSTANCES equ 255