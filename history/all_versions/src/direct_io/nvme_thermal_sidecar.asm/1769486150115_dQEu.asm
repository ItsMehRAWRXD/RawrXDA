; src/direct_io/nvme_thermal_sidecar.asm
; ════════════════════════════════════════════════════════════════════════════════
; SovereignControlBlock NVMe Thermal Sidecar (Admin/SYSTEM writer)
; Creates Global\SOVEREIGN_NVME_TEMPS shared mapping and updates temps
; ════════════════════════════════════════════════════════════════════════════════
;
; Build: ml64.exe /c nvme_thermal_sidecar.asm
; Link:  link.exe /subsystem:console /entry:SidecarMain nvme_thermal_sidecar.obj kernel32.lib advapi32.lib
;
; Shared layout (little-endian):
;   0x00: uint32 signature  ("SOVE")
;   0x04: uint32 version    (1)
;   0x08: uint32 count
;   0x0C: uint32 reserved
;   0x10: int32  temps[MAX_DRIVES]
;   0x50: int32  wear[MAX_DRIVES]
;   0x90: uint64 timestamp_ms
;
; ════════════════════════════════════════════════════════════════════════════════

; ════════════════════════════════════════════════════════════════════════════════
; Constants
; ════════════════════════════════════════════════════════════════════════════════
INVALID_HANDLE_VALUE        EQU -1
PAGE_READWRITE              EQU 04h
FILE_MAP_WRITE              EQU 0002h
SDDL_REVISION_1              EQU 1

MAX_DRIVES                  EQU 16
SIGNATURE_SOVE              EQU 45564F53h
VERSION_VALUE               EQU 1

OFF_SIGNATURE               EQU 0
OFF_VERSION                 EQU 4
OFF_COUNT                   EQU 8
OFF_RESERVED                EQU 12
OFF_TEMPS                   EQU 16
OFF_WEAR                    EQU (OFF_TEMPS + (MAX_DRIVES * 4))
OFF_TIMESTAMP               EQU (OFF_WEAR + (MAX_DRIVES * 4))
MAP_SIZE                    EQU (OFF_TIMESTAMP + 8)

; ════════════════════════════════════════════════════════════════════════════════
; External Windows API / helpers
; ════════════════════════════════════════════════════════════════════════════════
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC
EXTERN ConvertStringSecurityDescriptorToSecurityDescriptorA:PROC
EXTERN LocalFree:PROC
EXTERN ExitProcess:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CreateDirectoryA:PROC
EXTERN lstrlenA:PROC

EXTERN NVMe_GetTemperature:PROC
EXTERN NVMe_GetWearLevel:PROC

; ════════════════════════════════════════════════════════════════════════════════
; RIP-safe helpers
; ════════════════════════════════════════════════════════════════════════════════
LOAD_GPTR MACRO reg, sym
    lea reg, sym
ENDM

; ════════════════════════════════════════════════════════════════════════════════
; Data
; ════════════════════════════════════════════════════════════════════════════════
.data
ALIGN 16

mapName             DB "Global\\SOVEREIGN_NVME_TEMPS", 0
mapNameLocal        DB "Local\\SOVEREIGN_NVME_TEMPS", 0
mapNameBare         DB "SOVEREIGN_NVME_TEMPS", 0
debugDir            DB "C:\\ProgramData\\Sovereign", 0
debugLogPath        DB "C:\\ProgramData\\Sovereign\\sidecar_debug.log", 0
dbgPhase1           DB "Phase1: PrepareSecurity", 13, 10, 0
dbgPhase2           DB "Phase2: CreateFileMapping", 13, 10, 0
dbgPhase3           DB "Phase3: MapViewOfFile", 13, 10, 0
dbgPhase4           DB "Phase4: Entering OracleLoop", 13, 10, 0
dbgRetry            DB "PhaseRetry: init retry", 13, 10, 0
sddlString          DB "D:(A;;GA;;;WD)", 0

pSD                 DQ 0

sa_nLength          DD 24
                    DD 0 ; Padding for x64 alignment
sa_lpSecurityDesc   DQ 0
sa_bInheritHandle   DD 1
                    DD 0 ; Padding for structure size align

hMapFile            DQ 0
pView               DQ 0

sidecarDriveIds     DD 0, 1, 2, 4, 5
sidecarDriveCount   DD 5
sidecarSleepMs      DD 500

; ════════════════════════════════════════════════════════════════════════════════
; PrepareSecurity
; Converts SDDL to SECURITY_DESCRIPTOR and updates SECURITY_ATTRIBUTES
; Returns: EAX=1 on success, 0 on failure
; ════════════════════════════════════════════════════════════════════════════════
.code
LogDebug PROC
    push rbp
    mov rbp, rsp
    sub rsp, 60h

    mov r10, rcx

    sub rsp, 20h
    LOAD_GPTR rcx, debugDir
    xor rdx, rdx
    call CreateDirectoryA
    add rsp, 20h

    sub rsp, 40h
    LOAD_GPTR rcx, debugLogPath
    mov edx, 00000004h
    mov r8d, 00000003h
    xor r9d, r9d
    mov DWORD PTR [rsp+20h], 4
    mov DWORD PTR [rsp+28h], 00000080h
    mov QWORD PTR [rsp+30h], 0
    call CreateFileA
    add rsp, 40h

    cmp rax, INVALID_HANDLE_VALUE
    je _log_done

    mov r11, rax
    mov rcx, r10
    call lstrlenA
    mov r8d, eax

    sub rsp, 20h
    mov rcx, r11
    mov rdx, r10
    mov QWORD PTR [rsp+20h], 0
    mov QWORD PTR [rsp+28h], 0
    call WriteFile
    add rsp, 20h

    mov rcx, r11
    call CloseHandle

_log_done:
    add rsp, 60h
    pop rbp
    ret
LogDebug ENDP

PrepareSecurity PROC
    sub rsp, 28h

    LOAD_GPTR rcx, sddlString
    mov edx, SDDL_REVISION_1
    LOAD_GPTR r8, pSD
    xor r9d, r9d
    call ConvertStringSecurityDescriptorToSecurityDescriptorA

    test eax, eax
    jz _prep_fail

    LOAD_GPTR rax, pSD
    mov rdx, QWORD PTR [rax]
    LOAD_GPTR rcx, sa_lpSecurityDesc
    mov QWORD PTR [rcx], rdx

    mov eax, 1
    jmp _prep_done

_prep_fail:
    xor eax, eax

_prep_done:
    add rsp, 28h
    ret
PrepareSecurity ENDP

; ════════════════════════════════════════════════════════════════════════════════
; SidecarMain (entrypoint)
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC SidecarMain
SidecarMain PROC
    push rbx
    push r12
    push r13
    sub rsp, 20h

_init_retry:
    LOAD_GPTR rcx, dbgPhase1
    call LogDebug
    call PrepareSecurity
    xor rbx, rbx
    test eax, eax
    jz _create_map
    LOAD_GPTR rbx, sa_nLength

_create_map:
    LOAD_GPTR rcx, dbgPhase2
    call LogDebug
    ; CreateFileMappingA(INVALID_HANDLE_VALUE, &sa or NULL, PAGE_READWRITE, 0, MAP_SIZE, mapName)
    sub rsp, 40h
    mov rcx, INVALID_HANDLE_VALUE
    mov rdx, rbx
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    mov DWORD PTR [rsp+20h], MAP_SIZE
    LOAD_GPTR rax, mapName
    mov QWORD PTR [rsp+28h], rax
    call CreateFileMappingA
    add rsp, 40h

    test rax, rax
    jnz _map_ready

    sub rsp, 40h
    mov rcx, INVALID_HANDLE_VALUE
    mov rdx, rbx
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    mov DWORD PTR [rsp+20h], MAP_SIZE
    LOAD_GPTR rax, mapNameLocal
    mov QWORD PTR [rsp+28h], rax
    call CreateFileMappingA
    add rsp, 40h

    test rax, rax
    jnz _map_ready

    sub rsp, 40h
    mov rcx, INVALID_HANDLE_VALUE
    xor rdx, rdx
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    mov DWORD PTR [rsp+20h], MAP_SIZE
    LOAD_GPTR rax, mapNameBare
    mov QWORD PTR [rsp+28h], rax
    call CreateFileMappingA
    add rsp, 40h

    test rax, rax
    jz _retry_wait

_map_ready:
    LOAD_GPTR rcx, dbgPhase3
    call LogDebug

    LOAD_GPTR rcx, hMapFile
    mov QWORD PTR [rcx], rax

    ; MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, MAP_SIZE)
    sub rsp, 30h
    mov rcx, rax
    mov edx, FILE_MAP_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov QWORD PTR [rsp+20h], MAP_SIZE
    call MapViewOfFile
    add rsp, 30h

    test rax, rax
    jz _retry_wait

    LOAD_GPTR rcx, pView
    mov QWORD PTR [rcx], rax

_main_loop:
    LOAD_GPTR rbx, pView
    mov rbx, QWORD PTR [rbx]

    LOAD_GPTR rcx, dbgPhase4
    call LogDebug

    mov DWORD PTR [rbx + OFF_SIGNATURE], SIGNATURE_SOVE
    mov DWORD PTR [rbx + OFF_VERSION], VERSION_VALUE

    LOAD_GPTR rax, sidecarDriveCount
    mov r12d, DWORD PTR [rax]
    mov DWORD PTR [rbx + OFF_COUNT], r12d
    mov DWORD PTR [rbx + OFF_RESERVED], 0

    xor r13d, r13d

_drive_loop:
    cmp r13d, r12d
    jge _write_timestamp

    LOAD_GPTR rdx, sidecarDriveIds
    mov ecx, DWORD PTR [rdx + r13*4]
    call NVMe_GetTemperature

    mov DWORD PTR [rbx + OFF_TEMPS + r13*4], eax
    LOAD_GPTR rdx, sidecarDriveIds
    mov ecx, DWORD PTR [rdx + r13*4]
    call NVMe_GetWearLevel
    mov DWORD PTR [rbx + OFF_WEAR + r13*4], eax

    inc r13d
    jmp _drive_loop

_write_timestamp:
    call GetTickCount64
    mov QWORD PTR [rbx + OFF_TIMESTAMP], rax

    LOAD_GPTR rcx, sidecarSleepMs
    mov ecx, DWORD PTR [rcx]
    call Sleep

    jmp _main_loop

_retry_wait:
    LOAD_GPTR rcx, dbgRetry
    call LogDebug
    mov ecx, 1000
    call Sleep
    jmp _init_retry

_fatal_exit:
    LOAD_GPTR rax, pView
    mov rax, QWORD PTR [rax]
    test rax, rax
    jz _close_map
    mov rcx, rax
    call UnmapViewOfFile

_close_map:
    LOAD_GPTR rax, hMapFile
    mov rax, QWORD PTR [rax]
    test rax, rax
    jz _free_sd
    mov rcx, rax
    call CloseHandle

_free_sd:
    LOAD_GPTR rax, pSD
    mov rax, QWORD PTR [rax]
    test rax, rax
    jz _exit
    mov rcx, rax
    call LocalFree

_exit:
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    xor ecx, ecx
    call ExitProcess
    ret
SidecarMain ENDP

END
