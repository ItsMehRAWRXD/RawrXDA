; nvme_query.asm - Silicon Query Engine (Corrected Structure Offsets)
option casemap:none

; Standard Windows Definitions
GENERIC_READ             EQU 80000000h
GENERIC_WRITE            EQU 40000000h
FILE_SHARE_READ          EQU 00000001h
FILE_SHARE_WRITE         EQU 00000002h
OPEN_EXISTING            EQU 3
IOCTL_STORAGE_QUERY_PROPERTY EQU 002D1400h
INVALID_HANDLE_VALUE     EQU -1

; STORAGE_TEMPERATURE_DATA_DESCRIPTOR layout (Windows 10/11):
; Offset 0x00: DWORD Version
; Offset 0x04: DWORD Size
; Offset 0x08: SHORT CriticalTemperature (Celsius)
; Offset 0x0A: SHORT WarningTemperature (Celsius)
; Offset 0x0C: WORD  InfoCount
; Offset 0x0E: BYTE  Reserved0[2]
; Offset 0x10: STORAGE_TEMPERATURE_INFO[0]
;   +0x00: WORD  Index
;   +0x02: SHORT Temperature (Celsius!)
;   +0x04: SHORT OverThreshold
;   +0x06: SHORT UnderThreshold
;   +0x08: BOOLEAN OverThresholdChangable
;   +0x09: BOOLEAN UnderThresholdChangable
;   +0x0A: BOOLEAN EventGenerated
;   +0x0B: BYTE Reserved0
;   +0x0C: DWORD Reserved1

EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC
EXTERN CloseHandle:PROC

PUBLIC QueryNVMeTemp

.DATA
    ALIGN 8
    qry      dd 0Bh     ; PropertyId = StorageDeviceTemperatureProperty (11)
             dd 0       ; QueryType = PropertyStandardQuery (0)
             dd 0, 0    ; Additional padding
    tempBuf  db 512 dup(0)
    bytesRet dq 0
    szDrive  db "\\.\PhysicalDrive0",0

.CODE
QueryNVMeTemp PROC FRAME
    ; RCX = Drive ID (0-9)
    ; Returns: Temperature in Celsius, or -1 on failure
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push rdi
    .pushreg rdi
    sub rsp, 60h
    .allocstack 60h
    .endprolog
    
    ; Validate drive ID (0-9 only)
    cmp ecx, 9
    ja _fail
    
    ; Build drive path string
    mov r12d, ecx
    add r12b, '0'
    lea rax, szDrive
    mov byte ptr [rax+17], r12b

    ; CreateFileA
    lea rcx, szDrive
    mov edx, GENERIC_READ OR GENERIC_WRITE
    mov r8d, FILE_SHARE_READ OR FILE_SHARE_WRITE
    xor r9d, r9d
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je _fail
    mov rbx, rax

    ; Clear output buffer
    lea rdi, tempBuf
    xor eax, eax
    mov ecx, 128
    rep stosd

    ; DeviceIoControl
    mov rcx, rbx
    mov edx, IOCTL_STORAGE_QUERY_PROPERTY
    lea r8, qry
    mov r9d, 16
    lea rax, tempBuf
    mov qword ptr [rsp+20h], rax
    mov qword ptr [rsp+28h], 512
    lea rax, bytesRet
    mov qword ptr [rsp+30h], rax
    mov qword ptr [rsp+38h], 0
    call DeviceIoControl
    
    test eax, eax
    jz _close_fail

    ; Check we got enough data
    mov rax, [bytesRet]
    cmp rax, 20
    jb _close_fail

    ; Parse STORAGE_TEMPERATURE_DATA_DESCRIPTOR
    lea rdi, tempBuf
    
    ; Check InfoCount > 0 (at offset 0x0C, WORD)
    movzx eax, word ptr [rdi+0Ch]
    test eax, eax
    jz _close_fail

    ; Read Temperature from STORAGE_TEMPERATURE_INFO[0].Temperature
    ; Offset 0x10 = start of first info entry
    ; +0x02 = Temperature field (SHORT, signed, Celsius)
    movsx eax, word ptr [rdi+12h]    ; 0x10 + 0x02 = 0x12 (18 decimal)
    
    ; Validate temperature is reasonable (-50 to 150 C)
    cmp eax, -50
    jl _close_fail
    cmp eax, 150
    jg _close_fail
    
    ; Save valid temperature
    mov r12d, eax

    ; Close handle
    mov rcx, rbx
    call CloseHandle
    
    ; Return temperature
    mov eax, r12d
    jmp _exit

_close_fail:
    mov rcx, rbx
    call CloseHandle
_fail:
    mov eax, -1
_exit:
    add rsp, 60h
    pop rdi
    pop r12
    pop rbx
    ret
QueryNVMeTemp ENDP
END
    mov r12d, eax   ; Save result

    mov rcx, rbx
    call CloseHandle
    mov eax, r12d
    jmp _exit

_close_fail:
    mov rcx, rbx
    call CloseHandle
_fail:
    mov eax, -1
_exit:
    add rsp, 48h
    pop r12
    pop rbx
    ret
QueryNVMeTemp ENDP
END
