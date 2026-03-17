; ============================================================================
; Sovereign NVMe Oracle - Pure MASM x64 Windows Service
; Creates Local\SOVEREIGN_NVME_TEMPS, polls drives, publishes real temps
; ============================================================================
option casemap:none
option frame:auto

; ----------------------------
; Constants
; ----------------------------
INVALID_HANDLE_VALUE            EQU -1
PAGE_READWRITE                  EQU 04h
FILE_MAP_ALL_ACCESS             EQU 0F001Fh
GENERIC_READ                    EQU 80000000h
GENERIC_WRITE                   EQU 40000000h
FILE_SHARE_READ                 EQU 1
FILE_SHARE_WRITE                EQU 2
OPEN_EXISTING                   EQU 3

; IOCTL / Storage properties
IOCTL_STORAGE_QUERY_PROPERTY    EQU 002D1400h
StorageDeviceTemperatureProperty EQU 0Bh
PropertyStandardQuery           EQU 0

; Service constants
SERVICE_WIN32_OWN_PROCESS       EQU 10h
SERVICE_ACCEPT_STOP             EQU 01h
SERVICE_CONTROL_STOP            EQU 01h
SERVICE_RUNNING                 EQU 04h
SERVICE_STOPPED                 EQU 01h

; MMF layout
OFF_SIGNATURE                   EQU 0
OFF_VERSION                     EQU 4
OFF_COUNT                       EQU 8
OFF_RESERVED                    EQU 12
OFF_TEMPS                       EQU 16
OFF_WEAR                        EQU 80
OFF_TIMESTAMP                   EQU 144
MMF_TOTAL_SIZE                  EQU 152

SIGNATURE_SOVE                  EQU 45564F53h ; "SOVE"
VERSION_VALUE                   EQU 1
MAX_DRIVES                      EQU 5

; ----------------------------
; External APIs
; ----------------------------
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC
EXTERN ExitProcess:PROC
EXTERN OutputDebugStringA:PROC
EXTERN GetLastError:PROC
EXTERN StartServiceCtrlDispatcherA:PROC
EXTERN RegisterServiceCtrlHandlerA:PROC
EXTERN SetServiceStatus:PROC

; ----------------------------
; Packed structs for IOCTL
; ----------------------------
STORAGE_PROPERTY_QUERY STRUCT 1
    PropertyId  dd ?
    QueryType   dd ?
    Additional  dd ?
    Padding     dd ?
STORAGE_PROPERTY_QUERY ENDS

STORAGE_TEMPERATURE_INFO STRUCT 1
    Index       dd ?
    Temperature dd ?    ; 0.1 Kelvin (signed short promoted to dword here)
    OverThresh  dd ?
    UnderThresh dd ?
STORAGE_TEMPERATURE_INFO ENDS

STORAGE_TEMPERATURE_DATA_DESCRIPTOR STRUCT 1
    Version     dd ?
    Size        dd ?
    TempCount   dd ?
    Reserved    dd ?
    TempInfo    STORAGE_TEMPERATURE_INFO <>
STORAGE_TEMPERATURE_DATA_DESCRIPTOR ENDS

; ----------------------------
; Service structures
; ----------------------------
SERVICE_STATUS STRUCT 1
    dwServiceType              dd ?
    dwCurrentState             dd ?
    dwControlsAccepted         dd ?
    dwWin32ExitCode            dd ?
    dwServiceSpecificExitCode  dd ?
    dwCheckPoint               dd ?
    dwWaitHint                 dd ?
SERVICE_STATUS ENDS

; ----------------------------
; Data
; ----------------------------
.data
szServiceName   db "SovereignNVMeOracle",0
szMMFName       db "Local\SOVEREIGN_NVME_TEMPS",0
szDrivePath     db "\\.\PhysicalDrive0",0

; Drives and timing
driveIds        dd 0,1,2,4,5
driveCount      dd MAX_DRIVES
pollIntervalMs  dd 500

; Service state
hStatus         dq 0
svcStatus       SERVICE_STATUS <>
running         dd 1

; MMF state
hMMF            dq 0
pView           dq 0

; IOCTL working buffers
query           STORAGE_PROPERTY_QUERY <>
tempDesc        STORAGE_TEMPERATURE_DATA_DESCRIPTOR <>
bytesReturned   dq 0

; Debug strings
szDbgStart      db "Oracle: Service starting",0
szDbgMMFOK      db "Oracle: MMF ready",0

; ----------------------------
; Code
; ----------------------------
.code

DebugLog PROC FRAME
    ; rcx = message
    jmp OutputDebugStringA
DebugLog ENDP

ServiceCtrl PROC FRAME
    cmp ecx, SERVICE_CONTROL_STOP
    jne @F
    mov running, 0
@@:
    mov rcx, hStatus
    lea rdx, svcStatus
    jmp SetServiceStatus
ServiceCtrl ENDP

InitMMF PROC FRAME
    sub rsp,40h
    mov rcx, INVALID_HANDLE_VALUE
    xor rdx, rdx
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    mov dword ptr [rsp+20h], MMF_TOTAL_SIZE
    lea rax, szMMFName
    mov qword ptr [rsp+28h], rax
    call CreateFileMappingA
    test rax, rax
    jz _fail
    mov hMMF, rax
    mov rcx, rax
    mov edx, FILE_MAP_ALL_ACCESS
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+20h], MMF_TOTAL_SIZE
    call MapViewOfFile
    test rax, rax
    jz _fail
    mov pView, rax
    ; Init header
    mov rbx, rax
    mov dword ptr [rbx+OFF_SIGNATURE], SIGNATURE_SOVE
    mov dword ptr [rbx+OFF_VERSION], VERSION_VALUE
    mov eax, driveCount
    mov dword ptr [rbx+OFF_COUNT], eax
    mov dword ptr [rbx+OFF_RESERVED], 0
    ; Clear temps/wear to -1
    mov ecx, MAX_DRIVES
    lea rdi, [rbx+OFF_TEMPS]
    mov eax, -1
    rep stosd
    mov ecx, MAX_DRIVES
    lea rdi, [rbx+OFF_WEAR]
    mov eax, -1
    rep stosd
    lea rcx, szDbgMMFOK
    call DebugLog
    add rsp,40h
    mov rax,1
    ret
_fail:
    add rsp,40h
    xor rax, rax
    ret
InitMMF ENDP

QueryTemp PROC FRAME
    ; ecx = driveId
    push rbx
    sub rsp,40h
    ; Patch path digit at index 17
    mov al, '0'
    add al, cl
    mov [szDrivePath+17], al
    ; Open drive
    lea rcx, szDrivePath
    mov edx, GENERIC_READ or GENERIC_WRITE
    mov r8d, FILE_SHARE_READ or FILE_SHARE_WRITE
    xor r9d, r9d
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je _fail
    mov rbx, rax
    ; Setup query
    mov query.PropertyId, StorageDeviceTemperatureProperty
    mov query.QueryType, PropertyStandardQuery
    mov query.Additional, 0
    ; DeviceIoControl
    mov rcx, rbx
    mov edx, IOCTL_STORAGE_QUERY_PROPERTY
    lea r8, query
    mov r9d, sizeof STORAGE_PROPERTY_QUERY
    lea rax, tempDesc
    mov qword ptr [rsp+20h], rax
    mov qword ptr [rsp+28h], sizeof STORAGE_TEMPERATURE_DATA_DESCRIPTOR
    lea rax, bytesReturned
    mov qword ptr [rsp+30h], rax
    mov qword ptr [rsp+38h], 0
    call DeviceIoControl
    test eax, eax
    jz _close_fail
    ; Convert (0.1 Kelvin) -> Celsius: (val - 2731)/10
    mov eax, tempDesc.TempInfo.Temperature
    sub eax, 2731
    cdq
    mov ecx, 10
    idiv ecx
    mov edx, eax
    mov rcx, rbx
    call CloseHandle
    mov eax, edx
    add rsp,40h
    pop rbx
    ret
_close_fail:
    mov rcx, rbx
    call CloseHandle
_fail:
    mov eax, -1
    add rsp,40h
    pop rbx
    ret
QueryTemp ENDP

UpdateDrives PROC FRAME
    push rbx
    push r12
    sub rsp,20h
    mov rbx, pView
    xor r12d, r12d
_loop:
    cmp r12d, MAX_DRIVES
    jge _done
    ; drive id
    mov ecx, driveIds[r12*4]
    call QueryTemp
    mov [rbx+OFF_TEMPS+r12*4], eax
    ; wear placeholder
    mov dword ptr [rbx+OFF_WEAR+r12*4], -1
    inc r12d
    jmp _loop
_done:
    call GetTickCount64
    mov [rbx+OFF_TIMESTAMP], rax
    add rsp,20h
    pop r12
    pop rbx
    ret
UpdateDrives ENDP

ServiceMain PROC FRAME
    sub rsp,40h
    ; Register handler
    lea rcx, szServiceName
    lea rdx, ServiceCtrl
    call RegisterServiceCtrlHandlerA
    mov hStatus, rax
    ; Running status
    mov svcStatus.dwServiceType, SERVICE_WIN32_OWN_PROCESS
    mov svcStatus.dwCurrentState, SERVICE_RUNNING
    mov svcStatus.dwControlsAccepted, SERVICE_ACCEPT_STOP
    mov svcStatus.dwWin32ExitCode, 0
    mov rcx, hStatus
    lea rdx, svcStatus
    call SetServiceStatus
    lea rcx, szDbgStart
    call DebugLog
    ; Init MMF
    call InitMMF
    test rax, rax
    jz _exit
_main:
    cmp running, 0
    je _shutdown
    call UpdateDrives
    mov ecx, pollIntervalMs
    call Sleep
    jmp _main
_shutdown:
    mov rcx, pView
    test rcx, rcx
    jz @F
    call UnmapViewOfFile
@@:
    mov rcx, hMMF
    test rcx, rcx
    jz @F
    call CloseHandle
@@:
    mov svcStatus.dwCurrentState, SERVICE_STOPPED
    mov rcx, hStatus
    lea rdx, svcStatus
    call SetServiceStatus
_exit:
    xor ecx, ecx
    call ExitProcess
ServiceMain ENDP

ServiceEntry PROC FRAME
    sub rsp, 40h
    ; Build service table on stack: {name, ServiceMain}, {NULL, NULL}
    sub rsp, 32
    xor rax, rax
    mov [rsp+16], rax
    mov [rsp+24], rax
    lea rax, ServiceMain
    mov [rsp+8], rax
    lea rax, szServiceName
    mov [rsp], rax
    mov rcx, rsp
    call StartServiceCtrlDispatcherA
    add rsp, 72
    ret
ServiceEntry ENDP

END
