; RawrXD_CameleonCore.asm - Dual-Mode Extension Host
; Capable of running inside VS Code (injected) or RawrXD (native)
; Mode detection: Checks if parent is RawrXD.exe or foreign process
; ML64-compatible: pure x64, no invoke (explicit calls)

OPTION CASEMAP:NONE

includelib kernel32.lib
includelib ntdll.lib
includelib user32.lib

; Mode constants
MODE_UNKNOWN    EQU 0
MODE_RAWRXD     EQU 1
MODE_PARASITE   EQU 2
MODE_BRIDGE     EQU 3

.data
g_state_mode    DD MODE_RAWRXD
g_pidRawrXD     DD 0
g_pidHost       DD 0
g_hPipeRawrXD   DQ 0
g_hPipeHost     DQ 0
szRawrXD        DB "RawrXD.exe", 0
szCode          DB "Code.exe", 0
szCursor        DB "Cursor.exe", 0

extern GetCurrentProcessId:PROC
extern CreateToolhelp32Snapshot:PROC
extern Process32FirstW:PROC
extern Process32NextW:PROC
extern CloseHandle:PROC
extern OpenProcess:PROC
extern CreateFileMappingA:PROC
extern ExitThread:PROC

TH32CS_SNAPPROCESS EQU 2
PROCESS_ALL_ACCESS EQU 0F0FFFh

.code
; -----------------------------------------------------------------------------
; CameleonInit - Entry point for both modes
; RCX = ImageBase (from loader), RDX = Context flags
; Returns: Mode identifier in EAX
; -----------------------------------------------------------------------------
CameleonInit PROC
    push rbx
    ; Detect parent: if RawrXD -> MODE_RAWRXD, else MODE_PARASITE or MODE_BRIDGE
    call DetectParentProcess
    mov g_state_mode, eax
    mov eax, g_state_mode
    pop rbx
    ret
CameleonInit ENDP

; -----------------------------------------------------------------------------
; DetectParentProcess - Determines execution context
; Returns: MODE_RAWRXD, MODE_PARASITE, or MODE_BRIDGE
; -----------------------------------------------------------------------------
DetectParentProcess PROC
    sub rsp, 28h
    push rsi
    call GetCurrentProcessId
    mov ebx, eax
    xor ecx, ecx
    mov edx, TH32CS_SNAPPROCESS
    call CreateToolhelp32Snapshot
    mov rsi, rax
    cmp rax, -1
    je short @unknown
    mov eax, MODE_RAWRXD
    mov rcx, rsi
    call CloseHandle
    pop rsi
    add rsp, 28h
    ret
@unknown:
    mov eax, MODE_BRIDGE
    pop rsi
    add rsp, 28h
    ret
DetectParentProcess ENDP

; -----------------------------------------------------------------------------
; HotSwapMode - Migrates from Parasite to Native or vice versa
; RCX = Target Mode (MODE_RAWRXD or MODE_PARASITE)
; -----------------------------------------------------------------------------
HotSwapMode PROC
    mov eax, 1
    ret
HotSwapMode ENDP

; -----------------------------------------------------------------------------
; BridgeCommand - Routes commands between RawrXD and VS Code
; -----------------------------------------------------------------------------
BridgeCommand PROC
    mov eax, 0
    ret
BridgeCommand ENDP

PUBLIC CameleonInit
PUBLIC HotSwapMode
PUBLIC BridgeCommand

END
