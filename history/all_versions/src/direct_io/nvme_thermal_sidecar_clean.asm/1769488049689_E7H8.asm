; ============================================================================
; Sovereign NVMe Thermal Sidecar (MASM x64, Clean Fixed Version)
; Creates Local\SOVEREIGN_NVME_TEMPS MMF and publishes telemetry
; ============================================================================
; This is the cleaned up version based on the working C++ reference.
; Removes all the problematic SDDL/security descriptor code.
; Uses Local\ namespace which works without elevation.
; ============================================================================

option casemap:none

; ----------------------------
; Constants
; ----------------------------
INVALID_HANDLE_VALUE    EQU -1
PAGE_READWRITE          EQU 04h
FILE_MAP_WRITE          EQU 0002h
GENERIC_READ            EQU 80000000h
FILE_SHARE_READ         EQU 1
FILE_SHARE_WRITE        EQU 2
OPEN_EXISTING           EQU 3

IOCTL_STORAGE_QUERY_PROPERTY      EQU 002D1400h
StorageDeviceTemperatureProperty  EQU 14h
StorageDeviceWearLevelingProperty EQU 0Eh
PropertyStandardQuery             EQU 0

TEMPINFO_OFFSET         EQU 10h
TEMP_VALUE_OFFSET       EQU 2
WEAR_VALUE_OFFSET       EQU 8

OFF_SIGNATURE           EQU 0
OFF_VERSION             EQU 4
OFF_COUNT               EQU 8
OFF_RESERVED            EQU 12
OFF_TEMPS               EQU 16      ; int32[16]
OFF_WEAR                EQU 80      ; int32[16]
OFF_TIMESTAMP           EQU 144     ; uint64
MMF_SIZE                EQU 152     ; Total size matching C++ struct

SIGNATURE_SOVE          EQU 45564F53h   ; "SOVE" in little-endian
VERSION_VALUE           EQU 1
MAX_DRIVES              EQU 5

; ----------------------------
; External APIs
; ----------------------------
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC
EXTERN ExitProcess:PROC
EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC

; ----------------------------
; Data
; ----------------------------
.data
ALIGN 8

; MMF name - using Local\ namespace (works without elevation)
mapName         DB "Local\SOVEREIGN_NVME_TEMPS", 0

; Drive configuration
driveIds        DD 0, 1, 2, 4, 5
driveCount      DD MAX_DRIVES
sleepMs         DD 500

; Runtime handles
hMapFile        DQ 0
pView           DQ 0

; IOCTL query + buffers
queryPropertyId DD 0
queryQueryType  DD 0
queryPadding    DQ 0

tempBuf         DB 512 DUP(0)
wearBuf         DB 64 DUP(0)
bytesReturned   DQ 0

; Dynamic drive path: index 17 holds drive number
drivePath       DB "\\.\PhysicalDrive0", 0

; ----------------------------
; Code
; ----------------------------
.code

; ============================================================================
; SidecarMain - Main entry point
; Creates MMF, maps view, and loops forever updating thermal data
; ============================================================================
PUBLIC SidecarMain
SidecarMain PROC
    ; Preserve non-volatile registers
    push rbx
    push r12
    push r13
    sub rsp, 40h                ; Shadow space + alignment

    ; ========================================
    ; CreateFileMappingA
    ; ========================================
    ; HANDLE CreateFileMappingA(
    ;   HANDLE hFile,                    // rcx = INVALID_HANDLE_VALUE
    ;   LPSECURITY_ATTRIBUTES lpAttr,    // rdx = NULL
    ;   DWORD flProtect,                 // r8  = PAGE_READWRITE
    ;   DWORD dwMaximumSizeHigh,         // r9  = 0
    ;   DWORD dwMaximumSizeLow,          // [rsp+20h] = MMF_SIZE
    ;   LPCSTR lpName                    // [rsp+28h] = &mapName
    ; )
    mov rcx, INVALID_HANDLE_VALUE
    xor rdx, rdx                        ; NULL security (default)
    mov r8d, PAGE_READWRITE
    xor r9d, r9d                        ; Size high = 0
    mov DWORD PTR [rsp+20h], MMF_SIZE
    lea rax, mapName
    mov QWORD PTR [rsp+28h], rax
    call CreateFileMappingA

    test rax, rax
    jz _fatal_exit                      ; Failed to create mapping

    lea rcx, hMapFile
    mov QWORD PTR [rcx], rax
    mov rbx, rax                        ; Save handle for MapViewOfFile

    ; ========================================
    ; MapViewOfFile
    ; ========================================
    ; LPVOID MapViewOfFile(
    ;   HANDLE hFileMappingObject,       // rcx = hMapFile
    ;   DWORD dwDesiredAccess,           // rdx = FILE_MAP_WRITE
    ;   DWORD dwFileOffsetHigh,          // r8  = 0
    ;   DWORD dwFileOffsetLow,           // r9  = 0
    ;   SIZE_T dwNumberOfBytesToMap      // [rsp+20h] = MMF_SIZE
    ; )
    mov rcx, rbx
    mov edx, FILE_MAP_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov QWORD PTR [rsp+20h], MMF_SIZE
    call MapViewOfFile

    test rax, rax
    jz _fatal_exit                      ; Failed to map view

    lea rcx, pView
    mov QWORD PTR [rcx], rax

    ; ========================================
    ; Main Loop - Poll drives forever
    ; ========================================
_main_loop:
    ; Get view pointer
    lea rax, pView
    mov rbx, QWORD PTR [rax]

    ; Write header
    mov DWORD PTR [rbx + OFF_SIGNATURE], SIGNATURE_SOVE
    mov DWORD PTR [rbx + OFF_VERSION], VERSION_VALUE
    lea rax, driveCount
    mov r12d, DWORD PTR [rax]
    mov DWORD PTR [rbx + OFF_COUNT], r12d
    mov DWORD PTR [rbx + OFF_RESERVED], 0

    ; Poll each drive
    xor r13d, r13d                      ; Drive index

_drive_loop:
    cmp r13d, r12d
    jge _write_timestamp

    ; Get temperature
    lea rax, driveIds
    mov ecx, DWORD PTR [rax + r13*4]    ; driveId
    call NVMe_GetTemperature
    mov DWORD PTR [rbx + OFF_TEMPS + r13*4], eax

    ; Get wear level
    lea rax, driveIds
    mov ecx, DWORD PTR [rax + r13*4]    ; driveId
    call NVMe_GetWearLevel
    mov DWORD PTR [rbx + OFF_WEAR + r13*4], eax

    inc r13d
    jmp _drive_loop

_write_timestamp:
    ; Update timestamp
    call GetTickCount64
    mov QWORD PTR [rbx + OFF_TIMESTAMP], rax

    ; Sleep before next poll
    lea rax, sleepMs
    mov ecx, DWORD PTR [rax]
    call Sleep

    jmp _main_loop

    ; ========================================
    ; Fatal Exit (MMF creation failed)
    ; ========================================
_fatal_exit:
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    mov ecx, 1                          ; Exit code 1
    call ExitProcess
    ret

SidecarMain ENDP

END
