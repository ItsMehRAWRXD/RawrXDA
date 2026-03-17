; Replacement for windows.inc for this specific project
; Definitions needed for http_bridge.asm

; -------------------------------------------------------------------------
; Constants
; -------------------------------------------------------------------------
AF_INET         EQU 2
SOCK_STREAM     EQU 1
IPPROTO_TCP     EQU 6
INVALID_SOCKET  EQU -1

; -------------------------------------------------------------------------
; Structure Definitions
; -------------------------------------------------------------------------

; sockaddr_in (size 16)
sockaddr_in STRUCT
    sin_family  WORD    ?
    sin_port    WORD    ?
    sin_addr    DWORD   ?
    sin_zero    BYTE    8 dup(?)
sockaddr_in ENDS

; WSADATA (size ~400+, just using enough space)
WSADATA STRUCT
    wVersion        WORD    ?
    wHighVersion    WORD    ?
    szDescription   BYTE    257 dup(?)
    szSystemStatus  BYTE    129 dup(?)
    iMaxSockets     WORD    ?
    iMaxUdpDg       WORD    ?
    lpVendorInfo    QWORD   ?
WSADATA ENDS

; -------------------------------------------------------------------------
; API Prototypes
; -------------------------------------------------------------------------
EXTERN WSAStartup: PROC
EXTERN socket: PROC
EXTERN bind: PROC
EXTERN listen: PROC
EXTERN accept: PROC
EXTERN recv: PROC
EXTERN send: PROC
EXTERN closesocket: PROC
EXTERN LoadLibraryA: PROC
EXTERN GetProcAddress: PROC
EXTERN ExitProcess: PROC
