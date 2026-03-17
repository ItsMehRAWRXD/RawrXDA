; ╔══════════════════════════════════════════════════════════════════════════════╗
; ║ nvme_oracle_service.asm - Pure MASM x64 NVMe Thermal Sidecar Service        ║
; ║                                                                              ║
; ║ Purpose: Windows SYSTEM service providing real NVMe temperature telemetry   ║
; ║          via IOCTL_STORAGE_QUERY_PROPERTY, published to Local\ MMF          ║
; ║                                                                              ║
; ║ Features:                                                                    ║
; ║   - Runs as NT AUTHORITY\SYSTEM via SCM                                     ║
; ║   - Direct IOCTL_STORAGE_QUERY_PROPERTY (no WMI, no PowerShell)            ║
; ║   - Real Kelvin→Celsius conversion (temp-2731)/10                          ║
; ║   - Local\ namespace MMF (no SeCreateGlobalPrivilege needed)               ║
; ║   - ABI-correct x64 calling convention with shadow space                   ║
; ║   - Deterministic memory layout matching C++ harness                        ║
; ║   - Debug output via OutputDebugStringA                                     ║
; ║                                                                              ║
; ║ Build:                                                                       ║
; ║   ml64 /c nvme_oracle_service.asm                                           ║
; ║   link /subsystem:windows /entry:ServiceEntry nvme_oracle_service.obj ^     ║
; ║        kernel32.lib advapi32.lib                                            ║
; ║                                                                              ║
; ║ Install:                                                                     ║
; ║   sc create SovereignNVMeOracle binPath= "C:\path\nvme_oracle_service.exe" ^║
; ║      start= auto obj= "NT AUTHORITY\SYSTEM"                                 ║
; ║   sc start SovereignNVMeOracle                                              ║
; ║                                                                              ║
; ║ Author: RawrXD IDE Team                                                      ║
; ║ Version: 2.0.0                                                               ║
; ║ Target: x86-64 / AMD64 / Intel EM64T                                        ║
; ╚══════════════════════════════════════════════════════════════════════════════╝

option casemap:none
option frame:auto

; ═══════════════════════════════════════════════════════════════════════════════
; Windows API Constants
; ═══════════════════════════════════════════════════════════════════════════════

; Service Control Manager Constants
SERVICE_WIN32_OWN_PROCESS     EQU 10h
SERVICE_ACCEPT_STOP           EQU 01h
SERVICE_ACCEPT_SHUTDOWN       EQU 04h
SERVICE_CONTROL_STOP          EQU 01h
SERVICE_CONTROL_SHUTDOWN      EQU 05h
SERVICE_RUNNING               EQU 04h
SERVICE_STOPPED               EQU 01h
SERVICE_STOP_PENDING          EQU 03h
SERVICE_START_PENDING         EQU 02h
NO_ERROR                      EQU 0

; Memory Mapping Constants
PAGE_READWRITE                EQU 04h
FILE_MAP_ALL_ACCESS           EQU 0F001Fh
FILE_MAP_READ                 EQU 04h
MMF_SIZE                      EQU 256
INVALID_HANDLE_VALUE          EQU -1

; File I/O Constants
GENERIC_READ                  EQU 80000000h
GENERIC_WRITE                 EQU 40000000h
FILE_SHARE_READ               EQU 01h
FILE_SHARE_WRITE              EQU 02h
OPEN_EXISTING                 EQU 03h

; IOCTL Constants
IOCTL_STORAGE_QUERY_PROPERTY  EQU 002D1400h
StorageDeviceTemperatureProperty EQU 0Bh
PropertyStandardQuery         EQU 00h

; ═══════════════════════════════════════════════════════════════════════════════
; MMF Layout Offsets (MUST MATCH C++ SharedLayout struct)
; ═══════════════════════════════════════════════════════════════════════════════
; Offset  Size   Field
; 0x00    u32    Signature  "SOVE" = 0x45564F53
; 0x04    u32    Version    (1)
; 0x08    u32    DriveCount
; 0x0C    u32    Reserved
; 0x10    i32[16] Temps     (64 bytes)
; 0x50    i32[16] Wear      (64 bytes)
; 0x90    u64    Timestamp  (ms since boot)
; ═══════════════════════════════════════════════════════════════════════════════

OFF_SIGNATURE                 EQU 0
OFF_VERSION                   EQU 4
OFF_COUNT                     EQU 8
OFF_RESERVED                  EQU 12
OFF_TEMPS                     EQU 16       ; 0x10
OFF_WEAR                      EQU 80       ; 0x50
OFF_TIMESTAMP                 EQU 144      ; 0x90

; Magic Values
SIGNATURE_SOVE                EQU 45564F53h  ; "SOVE" little-endian
VERSION_VALUE                 EQU 1
MAX_DRIVES                    EQU 5
TEMP_INVALID                  EQU -1

; Polling Configuration
POLL_INTERVAL_MS              EQU 500

; ═══════════════════════════════════════════════════════════════════════════════
; Structures (Packed for IOCTL compatibility)
; ═══════════════════════════════════════════════════════════════════════════════

; SERVICE_STATUS structure (24 bytes)
SERVICE_STATUS STRUCT
    dwServiceType             dd ?
    dwCurrentState            dd ?
    dwControlsAccepted        dd ?
    dwWin32ExitCode           dd ?
    dwServiceSpecificExitCode dd ?
    dwCheckPoint              dd ?
    dwWaitHint                dd ?
SERVICE_STATUS ENDS

; SERVICE_TABLE_ENTRY structure (16 bytes on x64)
SERVICE_TABLE_ENTRY STRUCT
    lpServiceName             dq ?
    lpServiceProc             dq ?
SERVICE_TABLE_ENTRY ENDS

; STORAGE_PROPERTY_QUERY structure (12 bytes, pad to 16)
STORAGE_PROPERTY_QUERY STRUCT 1
    PropertyId                dd ?
    QueryType                 dd ?
    AdditionalParameters      db 4 dup(?)
STORAGE_PROPERTY_QUERY ENDS

; STORAGE_TEMPERATURE_INFO structure (16 bytes)
STORAGE_TEMPERATURE_INFO STRUCT 1
    Index                     dw ?
    Temperature               dw ?      ; In 0.1 Kelvin units!
    OverThreshold             dw ?
    UnderThreshold            dw ?
    Reserved1                 dw ?
    Reserved2                 dw ?
    Reserved3                 dw ?
    Reserved4                 dw ?
STORAGE_TEMPERATURE_INFO ENDS

; STORAGE_TEMPERATURE_DATA_DESCRIPTOR structure
STORAGE_TEMPERATURE_DATA_DESCRIPTOR STRUCT 1
    Version                   dd ?
    Size                      dd ?
    CriticalTemperature       dw ?
    WarningTemperature        dw ?
    InfoCount                 dw ?
    Reserved0                 dw ?
    Reserved1                 dd ?
    TemperatureInfo           STORAGE_TEMPERATURE_INFO <>
STORAGE_TEMPERATURE_DATA_DESCRIPTOR ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; External API Declarations
; ═══════════════════════════════════════════════════════════════════════════════

EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC
EXTERN ExitProcess:PROC
EXTERN StartServiceCtrlDispatcherA:PROC
EXTERN RegisterServiceCtrlHandlerA:PROC
EXTERN SetServiceStatus:PROC
EXTERN OutputDebugStringA:PROC
EXTERN GetLastError:PROC
EXTERN wsprintfA:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Data Section
; ═══════════════════════════════════════════════════════════════════════════════

.DATA
    ALIGN 8
    
    ; Service identification
    szServiceName     db "SovereignNVMeOracle",0
    
    ; MMF name (Local namespace - no SeCreateGlobalPrivilege required)
    szMMFName         db "Local\SOVEREIGN_NVME_TEMPS",0
    
    ; Drive path template (index 17 is the digit position)
    szDrivePath       db "\\.\PhysicalDrive0",0
    
    ; Debug message templates
    szDbgStarting     db "[NVMeOracle] Service starting...",13,10,0
    szDbgMMFCreated   db "[NVMeOracle] MMF created successfully",13,10,0
    szDbgMMFFailed    db "[NVMeOracle] FATAL: MMF creation failed",13,10,0
    szDbgMapFailed    db "[NVMeOracle] FATAL: MapViewOfFile failed",13,10,0
    szDbgLoopStart    db "[NVMeOracle] Entering main loop",13,10,0
    szDbgStopping     db "[NVMeOracle] Service stopping...",13,10,0
    szDbgDriveTemp    db "[NVMeOracle] Drive %d = %dC",13,10,0
    szDbgDriveFail    db "[NVMeOracle] Drive %d: IOCTL failed",13,10,0
    
    ; Drive ID array (physical drives to poll)
    ALIGN 4
    driveIds          dd 0, 1, 2, 4, 5
    
    ; Globals
    ALIGN 8
    g_hMMF            dq 0          ; MMF handle
    g_pView           dq 0          ; Mapped view pointer
    g_hStatus         dq 0          ; Service status handle
    g_running         dd 1          ; Service running flag
    
    ; Service status structure
    ALIGN 4
    g_ServiceStatus   SERVICE_STATUS <>
    
    ; IOCTL buffers
    ALIGN 8
    qry               STORAGE_PROPERTY_QUERY <>
    tempDesc          STORAGE_TEMPERATURE_DATA_DESCRIPTOR <>
    bytesReturned     dq 0
    
    ; Debug format buffer
    ALIGN 8
    dbgBuffer         db 256 dup(0)

; ═══════════════════════════════════════════════════════════════════════════════
; Code Section
; ═══════════════════════════════════════════════════════════════════════════════

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; DebugPrint - Output debug string (preserves all registers)
; RCX = pointer to null-terminated string
; ═══════════════════════════════════════════════════════════════════════════════
DebugPrint PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    ; rcx already has the string pointer
    call    OutputDebugStringA
    
    add     rsp, 32
    pop     rsi
    pop     rdi
    pop     rbx
    ret
DebugPrint ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; DebugPrintFormat - Formatted debug output
; RCX = format string, RDX = arg1, R8 = arg2
; ═══════════════════════════════════════════════════════════════════════════════
DebugPrintFormat PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog
    
    ; Save args
    mov     rbx, rcx          ; format
    mov     rdi, rdx          ; arg1
    
    ; wsprintfA(dbgBuffer, format, arg1, arg2)
    lea     rcx, dbgBuffer
    mov     rdx, rbx
    mov     r8, rdi
    ; r9 already has arg2
    call    wsprintfA
    
    ; OutputDebugStringA(dbgBuffer)
    lea     rcx, dbgBuffer
    call    OutputDebugStringA
    
    add     rsp, 48
    pop     rdi
    pop     rbx
    ret
DebugPrintFormat ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ServiceHandler - SCM Control Handler
; RCX = dwControl
; Returns: void
; ═══════════════════════════════════════════════════════════════════════════════
ServiceHandler PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog
    
    ; Check for STOP or SHUTDOWN
    cmp     ecx, SERVICE_CONTROL_STOP
    je      @HandleStop
    cmp     ecx, SERVICE_CONTROL_SHUTDOWN
    je      @HandleStop
    jmp     @UpdateStatus
    
@HandleStop:
    ; Signal service to stop
    mov     g_running, 0
    
    ; Debug output
    lea     rcx, szDbgStopping
    call    DebugPrint
    
    ; Update status to STOP_PENDING
    mov     g_ServiceStatus.dwCurrentState, SERVICE_STOP_PENDING
    mov     g_ServiceStatus.dwCheckPoint, 1
    mov     g_ServiceStatus.dwWaitHint, 3000
    
@UpdateStatus:
    ; SetServiceStatus(g_hStatus, &g_ServiceStatus)
    mov     rcx, g_hStatus
    lea     rdx, g_ServiceStatus
    call    SetServiceStatus
    
    add     rsp, 40
    ret
ServiceHandler ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; InitMMF - Create and map the shared memory region
; Returns: RAX = 1 on success, 0 on failure
; ═══════════════════════════════════════════════════════════════════════════════
InitMMF PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 56
    .allocstack 56
    .endprolog
    
    ; CreateFileMappingA(
    ;   INVALID_HANDLE_VALUE,  // rcx - pagefile-backed
    ;   NULL,                  // rdx - default security
    ;   PAGE_READWRITE,        // r8  - protection
    ;   0,                     // r9  - size high
    ;   [rsp+32] MMF_SIZE,     // size low
    ;   [rsp+40] szMMFName     // name
    ; )
    mov     rcx, INVALID_HANDLE_VALUE
    xor     rdx, rdx                    ; NULL security
    mov     r8d, PAGE_READWRITE
    xor     r9d, r9d                    ; Size high = 0
    mov     dword ptr [rsp+32], MMF_SIZE
    lea     rax, szMMFName
    mov     qword ptr [rsp+40], rax
    call    CreateFileMappingA
    
    test    rax, rax
    jz      @MMFFailed
    mov     g_hMMF, rax
    mov     rbx, rax                    ; Save handle
    
    ; Debug: MMF created
    lea     rcx, szDbgMMFCreated
    call    DebugPrint
    
    ; MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MMF_SIZE)
    mov     rcx, rbx
    mov     edx, FILE_MAP_ALL_ACCESS
    xor     r8d, r8d                    ; Offset high
    xor     r9d, r9d                    ; Offset low
    mov     qword ptr [rsp+32], MMF_SIZE
    call    MapViewOfFile
    
    test    rax, rax
    jz      @MapFailed
    mov     g_pView, rax
    mov     rdi, rax                    ; Save view pointer
    
    ; Initialize MMF header
    mov     dword ptr [rdi+OFF_SIGNATURE], SIGNATURE_SOVE
    mov     dword ptr [rdi+OFF_VERSION], VERSION_VALUE
    mov     dword ptr [rdi+OFF_COUNT], MAX_DRIVES
    mov     dword ptr [rdi+OFF_RESERVED], 0
    
    ; Initialize temps and wear to -1 (invalid)
    mov     ecx, 16                     ; 16 drives max
    lea     rbx, [rdi+OFF_TEMPS]
@InitTemps:
    mov     dword ptr [rbx], TEMP_INVALID
    add     rbx, 4
    dec     ecx
    jnz     @InitTemps
    
    mov     ecx, 16
    lea     rbx, [rdi+OFF_WEAR]
@InitWear:
    mov     dword ptr [rbx], TEMP_INVALID
    add     rbx, 4
    dec     ecx
    jnz     @InitWear
    
    ; Initial timestamp
    call    GetTickCount64
    mov     qword ptr [rdi+OFF_TIMESTAMP], rax
    
    ; Success
    mov     rax, 1
    jmp     @InitDone
    
@MapFailed:
    lea     rcx, szDbgMapFailed
    call    DebugPrint
    ; Close the MMF handle since mapping failed
    mov     rcx, g_hMMF
    call    CloseHandle
    mov     g_hMMF, 0
    xor     rax, rax
    jmp     @InitDone
    
@MMFFailed:
    lea     rcx, szDbgMMFFailed
    call    DebugPrint
    xor     rax, rax
    
@InitDone:
    add     rsp, 56
    pop     rdi
    pop     rbx
    ret
InitMMF ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; QueryDriveTemp - Query temperature for a specific drive via IOCTL
; ECX = drive index (0-9)
; Returns: EAX = temperature in Celsius, or -1 on failure
; ═══════════════════════════════════════════════════════════════════════════════
QueryDriveTemp PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 64
    .allocstack 64
    .endprolog
    
    mov     esi, ecx                    ; Save drive ID
    
    ; Build drive path: \\.\PhysicalDriveN
    ; The digit is at offset 17 in szDrivePath
    mov     al, '0'
    add     al, cl                      ; Convert to ASCII digit
    lea     rbx, szDrivePath
    mov     [rbx+17], al
    
    ; CreateFileA(
    ;   szDrivePath,                    // rcx
    ;   GENERIC_READ | GENERIC_WRITE,   // rdx (0xC0000000)
    ;   FILE_SHARE_READ | FILE_SHARE_WRITE, // r8 (3)
    ;   NULL,                           // r9
    ;   [rsp+32] OPEN_EXISTING,         // 3
    ;   [rsp+40] 0,                     // flags
    ;   [rsp+48] NULL                   // template
    ; )
    lea     rcx, szDrivePath
    mov     edx, GENERIC_READ OR GENERIC_WRITE
    mov     r8d, FILE_SHARE_READ OR FILE_SHARE_WRITE
    xor     r9d, r9d                    ; NULL security
    mov     dword ptr [rsp+32], OPEN_EXISTING
    mov     dword ptr [rsp+40], 0
    mov     qword ptr [rsp+48], 0
    call    CreateFileA
    
    cmp     rax, INVALID_HANDLE_VALUE
    je      @QueryFailed
    mov     rdi, rax                    ; Save device handle
    
    ; Prepare STORAGE_PROPERTY_QUERY
    lea     rbx, qry
    mov     dword ptr [rbx], StorageDeviceTemperatureProperty  ; PropertyId
    mov     dword ptr [rbx+4], PropertyStandardQuery           ; QueryType
    mov     dword ptr [rbx+8], 0                               ; Additional
    
    ; DeviceIoControl(
    ;   hDevice,                        // rcx
    ;   IOCTL_STORAGE_QUERY_PROPERTY,   // rdx
    ;   &qry,                           // r8
    ;   sizeof(qry),                    // r9
    ;   [rsp+32] &tempDesc,
    ;   [rsp+40] sizeof(tempDesc),
    ;   [rsp+48] &bytesReturned,
    ;   [rsp+56] NULL (overlapped)
    ; )
    mov     rcx, rdi
    mov     edx, IOCTL_STORAGE_QUERY_PROPERTY
    lea     r8, qry
    mov     r9d, 16                     ; sizeof STORAGE_PROPERTY_QUERY (padded)
    lea     rax, tempDesc
    mov     qword ptr [rsp+32], rax
    mov     qword ptr [rsp+40], sizeof STORAGE_TEMPERATURE_DATA_DESCRIPTOR + 64
    lea     rax, bytesReturned
    mov     qword ptr [rsp+48], rax
    mov     qword ptr [rsp+56], 0
    call    DeviceIoControl
    
    ; Close handle first (we'll check result after)
    push    rax                         ; Save IOCTL result
    mov     rcx, rdi
    call    CloseHandle
    pop     rax
    
    ; Check IOCTL result
    test    eax, eax
    jz      @QueryFailed
    
    ; Check bytesReturned >= minimum size
    mov     rax, bytesReturned
    cmp     rax, 16                     ; At least header + one info entry
    jl      @QueryFailed
    
    ; Check InfoCount > 0
    lea     rbx, tempDesc
    movzx   eax, word ptr [rbx+12]      ; InfoCount
    test    eax, eax
    jz      @QueryFailed
    
    ; Get temperature from first info entry
    ; Temperature is at TemperatureInfo.Temperature (offset 18 in tempDesc)
    ; It's a 16-bit signed value in 0.1 Kelvin units
    movsx   eax, word ptr [rbx+18]      ; Temperature in 0.1K
    
    ; Convert from 0.1 Kelvin to Celsius: (temp - 2731) / 10
    ; 2731 = 273.15 * 10 (0°C in 0.1K units)
    sub     eax, 2731
    
    ; Check for negative (invalid reading)
    test    eax, eax
    js      @QueryFailed
    
    ; Divide by 10
    cdq
    mov     ecx, 10
    idiv    ecx
    ; EAX now contains temperature in Celsius
    
    ; Debug output: Drive N = tempC
    mov     edx, esi                    ; Drive ID
    mov     r8d, eax                    ; Temperature
    push    rax                         ; Save temp
    lea     rcx, szDbgDriveTemp
    call    DebugPrintFormat
    pop     rax                         ; Restore temp
    
    jmp     @QueryDone
    
@QueryFailed:
    ; Debug output: Drive N failed
    mov     edx, esi
    xor     r8d, r8d
    lea     rcx, szDbgDriveFail
    call    DebugPrintFormat
    mov     eax, TEMP_INVALID
    
@QueryDone:
    add     rsp, 64
    pop     rdi
    pop     rsi
    pop     rbx
    ret
QueryDriveTemp ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; UpdateDrives - Poll all configured drives and update MMF
; ═══════════════════════════════════════════════════════════════════════════════
UpdateDrives PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog
    
    mov     rdi, g_pView                ; MMF view pointer
    xor     r12d, r12d                  ; Drive index counter
    
@DriveLoop:
    cmp     r12d, MAX_DRIVES
    jge     @DriveLoopDone
    
    ; Get drive ID from array
    lea     rbx, driveIds
    mov     ecx, dword ptr [rbx + r12*4]
    
    ; Query temperature for this drive
    call    QueryDriveTemp
    
    ; Store temperature in MMF
    mov     dword ptr [rdi + OFF_TEMPS + r12*4], eax
    
    ; Store wear level as -1 (not implemented yet)
    mov     dword ptr [rdi + OFF_WEAR + r12*4], TEMP_INVALID
    
    inc     r12d
    jmp     @DriveLoop
    
@DriveLoopDone:
    ; Update timestamp
    call    GetTickCount64
    mov     qword ptr [rdi + OFF_TIMESTAMP], rax
    
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
UpdateDrives ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; CleanupMMF - Unmap and close the MMF
; ═══════════════════════════════════════════════════════════════════════════════
CleanupMMF PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog
    
    ; Unmap view if mapped
    mov     rcx, g_pView
    test    rcx, rcx
    jz      @NoView
    call    UnmapViewOfFile
    mov     g_pView, 0
    
@NoView:
    ; Close MMF handle if open
    mov     rcx, g_hMMF
    test    rcx, rcx
    jz      @NoHandle
    call    CloseHandle
    mov     g_hMMF, 0
    
@NoHandle:
    add     rsp, 40
    ret
CleanupMMF ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ServiceMain - Main service entry point (called by SCM dispatcher)
; RCX = argc, RDX = argv (ignored)
; ═══════════════════════════════════════════════════════════════════════════════
ServiceMain PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    
    ; Debug output: Starting
    lea     rcx, szDbgStarting
    call    DebugPrint
    
    ; Register service control handler
    lea     rcx, szServiceName
    lea     rdx, ServiceHandler
    call    RegisterServiceCtrlHandlerA
    
    test    rax, rax
    jz      @ServiceExit
    mov     g_hStatus, rax
    
    ; Initialize service status
    mov     g_ServiceStatus.dwServiceType, SERVICE_WIN32_OWN_PROCESS
    mov     g_ServiceStatus.dwCurrentState, SERVICE_START_PENDING
    mov     g_ServiceStatus.dwControlsAccepted, 0
    mov     g_ServiceStatus.dwWin32ExitCode, NO_ERROR
    mov     g_ServiceStatus.dwServiceSpecificExitCode, 0
    mov     g_ServiceStatus.dwCheckPoint, 1
    mov     g_ServiceStatus.dwWaitHint, 3000
    
    ; Report START_PENDING
    mov     rcx, g_hStatus
    lea     rdx, g_ServiceStatus
    call    SetServiceStatus
    
    ; Initialize MMF
    call    InitMMF
    test    rax, rax
    jz      @ServiceFailed
    
    ; Report RUNNING
    mov     g_ServiceStatus.dwCurrentState, SERVICE_RUNNING
    mov     g_ServiceStatus.dwControlsAccepted, SERVICE_ACCEPT_STOP OR SERVICE_ACCEPT_SHUTDOWN
    mov     g_ServiceStatus.dwCheckPoint, 0
    mov     g_ServiceStatus.dwWaitHint, 0
    
    mov     rcx, g_hStatus
    lea     rdx, g_ServiceStatus
    call    SetServiceStatus
    
    ; Debug output: Loop start
    lea     rcx, szDbgLoopStart
    call    DebugPrint
    
    ; ════════════════════════════════════════════════════════════════════════
    ; Main Service Loop
    ; ════════════════════════════════════════════════════════════════════════
@MainLoop:
    ; Check if we should stop
    cmp     g_running, 0
    je      @StopService
    
    ; Update all drive temperatures
    call    UpdateDrives
    
    ; Sleep for poll interval
    mov     ecx, POLL_INTERVAL_MS
    call    Sleep
    
    jmp     @MainLoop
    
    ; ════════════════════════════════════════════════════════════════════════
    ; Service Shutdown
    ; ════════════════════════════════════════════════════════════════════════
@StopService:
    ; Cleanup MMF
    call    CleanupMMF
    
    ; Report STOPPED
    mov     g_ServiceStatus.dwCurrentState, SERVICE_STOPPED
    mov     g_ServiceStatus.dwControlsAccepted, 0
    mov     g_ServiceStatus.dwCheckPoint, 0
    mov     g_ServiceStatus.dwWaitHint, 0
    
    mov     rcx, g_hStatus
    lea     rdx, g_ServiceStatus
    call    SetServiceStatus
    jmp     @ServiceExit
    
@ServiceFailed:
    ; Report failure
    mov     g_ServiceStatus.dwCurrentState, SERVICE_STOPPED
    mov     g_ServiceStatus.dwWin32ExitCode, 1
    
    mov     rcx, g_hStatus
    lea     rdx, g_ServiceStatus
    call    SetServiceStatus
    
@ServiceExit:
    add     rsp, 48
    pop     rbx
    ret
ServiceMain ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ServiceEntry - Process entry point
; Connects to SCM and dispatches to ServiceMain
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC ServiceEntry
ServiceEntry PROC FRAME
    sub     rsp, 56
    .allocstack 56
    .endprolog
    
    ; Build SERVICE_TABLE_ENTRY array on stack
    ; Entry 0: { szServiceName, ServiceMain }
    ; Entry 1: { NULL, NULL } - terminator
    
    lea     rax, szServiceName
    mov     qword ptr [rsp+32], rax     ; lpServiceName
    lea     rax, ServiceMain
    mov     qword ptr [rsp+40], rax     ; lpServiceProc
    mov     qword ptr [rsp+48], 0       ; NULL terminator name
    mov     qword ptr [rsp+56], 0       ; NULL terminator proc
    
    ; StartServiceCtrlDispatcherA(&serviceTable)
    lea     rcx, [rsp+32]
    call    StartServiceCtrlDispatcherA
    
    ; Exit process
    xor     ecx, ecx
    call    ExitProcess
    
    add     rsp, 56
    ret
ServiceEntry ENDP

END
