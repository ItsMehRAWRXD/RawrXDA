; nvme_service.asm - Service Host
; Assemble: ml64 /c nvme_service.asm
; Link: link nvme_service.obj kernel32.lib advapi32.lib /subsystem:console /entry:main

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ─── PUBLIC Exports ──────────────────────────────────────────────────────────
PUBLIC ServiceMain
PUBLIC Handler

includelib kernel32.lib
includelib advapi32.lib

; Windows Constants
SERVICE_WIN32_OWN_PROCESS EQU 10h
SERVICE_ACCEPT_STOP       EQU 1
SERVICE_RUNNING           EQU 4
SERVICE_STOPPED           EQU 1
SERVICE_CONTROL_STOP      EQU 1
INVALID_HANDLE_VALUE      EQU -1

EXTERN RegisterServiceCtrlHandlerA:PROC
EXTERN SetServiceStatus:PROC
EXTERN StartServiceCtrlDispatcherA:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN GetTickCount64:PROC
EXTERN CloseHandle:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN Sleep:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN FreeLibrary:PROC

.data
ServiceName     db "SovereignNVMeOracle", 0
MMFName         db "Local\\SOVEREIGN_NVME_TEMPS", 0
DLLName         db "nvme_query.dll", 0
QuerySymbol     db "QueryNVMeTemp", 0

gStatus         dd SERVICE_WIN32_OWN_PROCESS ; dwServiceType
                dd SERVICE_RUNNING           ; dwCurrentState
                dd SERVICE_ACCEPT_STOP       ; dwControlsAccepted
                dd 0                         ; dwWin32ExitCode
                dd 0                         ; dwServiceSpecificExitCode
                dd 0                         ; dwCheckPoint
                dd 0                         ; dwWaitHint

gStatusHandle   dq 0
gRunning        dq 1
gMMFHandle      dq 0
gMMFPtr         dq 0
queryTempPtr    dq 0

ServiceTable    dq ServiceName, ServiceMain
                dq 0, 0

.code

Handler PROC
    ; RCX contains Control Code
    cmp ecx, SERVICE_CONTROL_STOP
    jne Handler_Ignore
    
    mov [gRunning], 0
    
Handler_Ignore:
    ret
Handler ENDP

InitMMF PROC FRAME
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    
    ; CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 256, MMFName)
    mov rcx, INVALID_HANDLE_VALUE
    xor rdx, rdx            ; NULL
    mov r8d, 04h            ; PAGE_READWRITE
    xor r9d, r9d            ; dwMaximumSizeHigh
    mov qword ptr [rsp+20h], 256 ; dwMaximumSizeLow
    lea rax, MMFName
    mov [rsp+28h], rax      ; lpName
    call CreateFileMappingA
    
    test rax, rax
    jz MMF_Failed
    mov [gMMFHandle], rax

    ; MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0)
    mov rcx, rax            ; hFileMappingObject
    mov edx, 0F001Fh        ; FILE_MAP_ALL_ACCESS
    xor r8d, r8d            ; dwFileOffsetHigh
    xor r9d, r9d            ; dwFileOffsetLow
    mov qword ptr [rsp+20h], 0 ; dwNumberOfBytesToMap (0=whole)
    call MapViewOfFile
    test rax, rax
    jz MMF_Failed
    mov [gMMFPtr], rax

    ; Write header
    mov rdi, rax
    mov dword ptr [rdi], 0534F5645h      ; 'SOVE'
    mov dword ptr [rdi+4], 1             ; version
    mov dword ptr [rdi+8], 5             ; drive count

    ; Clear values
    mov rcx, 16
    xor rax, rax
MMF_Zero:
    mov dword ptr [rdi+rcx], -1
    add rcx, 4
    cmp rcx, 128
    jb MMF_Zero
    
    add rsp, 48h
    mov eax, 1
    ret
MMF_Failed:
    xor eax, eax
    add rsp, 48h
    ret
InitMMF ENDP

ServiceMain PROC FRAME
    sub rsp, 48h
    .allocstack 48h
    .endprolog

    lea rcx, ServiceName
    lea rdx, Handler
    call RegisterServiceCtrlHandlerA
    test rax, rax
    jz Service_Done
    mov [gStatusHandle], rax
    
    ; Load DLL
    lea rcx, DLLName
    call LoadLibraryA
    test rax, rax
    jz Service_ReportFail
    mov rbx, rax    ; Save HMODULE

    lea rcx, QuerySymbol
    mov rcx, rbx
    lea rdx, QuerySymbol
    call GetProcAddress
    mov [queryTempPtr], rax
    test rax, rax
    jz Service_ReportFail

    call InitMMF
    test eax, eax
    jz Service_ReportFail

    ; Report RUNNING
    mov rcx, [gStatusHandle]
    lea rdx, gStatus
    mov dword ptr [rdx+4], SERVICE_RUNNING
    call SetServiceStatus

Service_Loop:
    mov rax, [gRunning]
    test rax, rax
    jz Service_Stop

    ; Loop over 5 drives
    xor r15, r15
Service_Next:
    mov rcx, r15            ; Drive ID
    mov rax, [queryTempPtr]
    call rax
    
    mov rdx, [gMMFPtr]
    mov [rdx + 16 + r15*4], eax

    inc r15
    cmp r15, 5
    jl Service_Next

    ; Write timestamp
    call GetTickCount64
    mov rdx, [gMMFPtr]
    mov [rdx + 144], rax

    mov ecx, 1000
    call Sleep
    jmp Service_Loop

Service_Stop:
    mov rcx, [gMMFPtr]
    call UnmapViewOfFile
    mov rcx, [gMMFHandle]
    call CloseHandle
    mov rcx, rbx
    call FreeLibrary

Service_ReportStop:
    mov rcx, [gStatusHandle]
    lea rdx, gStatus
    mov dword ptr [rdx+4], SERVICE_STOPPED
    call SetServiceStatus
    jmp Service_Done

Service_ReportFail:
    mov rcx, [gStatusHandle]
    lea rdx, gStatus
    mov dword ptr [rdx+4], SERVICE_STOPPED
    call SetServiceStatus

Service_Done:
    add rsp, 48h
    ret
ServiceMain ENDP

main PROC FRAME
    sub rsp, 58h    ; Align stack for 16-byte boundary (8 ret + 88 = 96)
    .allocstack 58h
    .endprolog
    
    lea rcx, ServiceTable
    call StartServiceCtrlDispatcherA
    
    add rsp, 58h
    ret
main ENDP
END
