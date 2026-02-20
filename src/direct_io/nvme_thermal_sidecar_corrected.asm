; src/direct_io/nvme_thermal_sidecar_corrected.asm
; ════════════════════════════════════════════════════════════════════════════════
; SovereignControlBlock NVMe Thermal Sidecar (Admin/SYSTEM writer)
; Creates Global\SOVEREIGN_NVME_TEMPS shared mapping and updates temps in a loop
; ════════════════════════════════════════════════════════════════════════════════
;
; Build: ml64.exe /c nvme_thermal_sidecar_corrected.asm
; Link:  link.exe /subsystem:console /entry:SidecarMain nvme_thermal_sidecar_corrected.obj kernel32.lib advapi32.lib
;
; Shared layout (little-endian):
;   0x00: uint32 signature  ("SOVE" = 0x45564F53)
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
SDDL_REVISION_1             EQU 1
MEM_RELEASE                 EQU 8000h

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
; External Windows API
; ════════════════════════════════════════════════════════════════════════════════

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetTickCount64:PROC
EXTERN Sleep:PROC
EXTERN ConvertStringSecurityDescriptorToSecurityDescriptorA:PROC
EXTERN LocalFree:PROC
EXTERN ExitProcess:PROC

EXTERN NVMe_GetTemperature:PROC

; ════════════════════════════════════════════════════════════════════════════════
; Data
; ════════════════════════════════════════════════════════════════════════════════
.data
ALIGN 16

mapName             DB "Global\\SOVEREIGN_NVME_TEMPS", 0
sddlString          DB "D:(A;;GA;;;WD)", 0

pSD                 DQ 0

sa_nLength          DD 24
sa_lpSecurityDesc   DQ 0
sa_bInheritHandle   DD 1
sa_padding          DD 0

sidecarDriveIds     DD 0, 1, 2, 4, 5
sidecarDriveCount   DD 5
sidecarSleepMs      DD 500

.code

; ════════════════════════════════════════════════════════════════════════════════
; PrepareSecurity
; Converts SDDL to SECURITY_DESCRIPTOR and updates SECURITY_ATTRIBUTES
; Returns: EAX=1 on success, 0 on failure
; ════════════════════════════════════════════════════════════════════════════════
PrepareSecurity PROC
    sub rsp, 28h

    lea rcx, sddlString
    mov edx, SDDL_REVISION_1
    lea r8, pSD
    xor r9d, r9d
    call ConvertStringSecurityDescriptorToSecurityDescriptorA

    test eax, eax
    jz _prep_fail

    lea rax, pSD
    mov rdx, QWORD PTR [rax]
    lea rcx, sa_lpSecurityDesc
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
    push rbp
    mov rbp, rsp
    sub rsp, 28h

    push rbx
    push r12
    push r13
    push r14

    call PrepareSecurity
    test eax, eax
    jz _fatal_exit

    ; CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, MAP_SIZE, mapName)
    sub rsp, 38h
    mov rcx, INVALID_HANDLE_VALUE
    lea rdx, sa_nLength
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    mov DWORD PTR [rsp+20h], MAP_SIZE
    lea rax, mapName
    mov QWORD PTR [rsp+28h], rax
    call CreateFileMappingA
    add rsp, 38h

    test rax, rax
    jz _fatal_exit

    mov rbx, rax  ; hMapFile

    ; MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, MAP_SIZE)
    sub rsp, 28h
    mov rcx, rbx
    mov edx, FILE_MAP_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov QWORD PTR [rsp+20h], MAP_SIZE
    call MapViewOfFile
    add rsp, 28h

    test rax, rax
    jz _fatal_exit

    mov r14, rax  ; pView

_main_loop:
    mov DWORD PTR [r14 + OFF_SIGNATURE], SIGNATURE_SOVE
    mov DWORD PTR [r14 + OFF_VERSION], VERSION_VALUE

    mov r12d, sidecarDriveCount
    mov DWORD PTR [r14 + OFF_COUNT], r12d
    mov DWORD PTR [r14 + OFF_RESERVED], 0

    xor r13d, r13d

_drive_loop:
    cmp r13d, r12d
    jge _write_timestamp

    lea rcx, sidecarDriveIds
    mov ecx, DWORD PTR [rcx + r13*4]
    call NVMe_GetTemperature

    mov DWORD PTR [r14 + OFF_TEMPS + r13*4], eax
    mov DWORD PTR [r14 + OFF_WEAR + r13*4], 0FFFFFFFFh

    inc r13d
    jmp _drive_loop

_write_timestamp:
    call GetTickCount64
    mov QWORD PTR [r14 + OFF_TIMESTAMP], rax

    mov ecx, sidecarSleepMs
    call Sleep

    jmp _main_loop

_fatal_exit:
    test r14, r14
    jz _close_map
    mov rcx, r14
    call UnmapViewOfFile

_close_map:
    test rbx, rbx
    jz _free_sd
    mov rcx, rbx
    call CloseHandle

_free_sd:
    lea rax, pSD
    mov rax, QWORD PTR [rax]
    test rax, rax
    jz _exit
    mov rcx, rax
    call LocalFree

_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 28h
    pop rbp
    xor ecx, ecx
    call ExitProcess
    ret
SidecarMain ENDP

END
